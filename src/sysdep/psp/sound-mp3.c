/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/sound-mp3.c: MP3 decoding module for the PSP, making use
 * of the PSP's Media Engine.
 */

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-mp3.h"
#include "src/sysdep/psp/thread.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Maximum MP3 frame length (in samples) and corresponding PCM data size
 * (in bytes).  Actual frame length depends on the MP3 version and layer. */
#define MP3_FRAME_LEN             1152
#define MP3_FRAME_PCMSIZE_MONO    (MP3_FRAME_LEN*2)
#define MP3_FRAME_PCMSIZE_STEREO  (MP3_FRAME_LEN*4)

/* Number of PCM buffers to use for storing decoded PCM data.  One buffer
 * is used per frame, regardless of frame length. */
#define NUM_PCM_BUFFERS    4

/* Maximum size of an encoded MP3 frame (in bytes). */
#define MP3_FRAME_MAXDATA  2020  // 2016 + padding (Version 1 Layer 1)

/* Number of samples to skip at the beginning of a decoded stream.  MP3
 * encoding inserts dummy samples at the beginning of the stream to set
 * up the decode state; these samples are ignored when decoding or
 * calculating loop positions.  If an extension header is present, the
 * value given by the header is used instead. */
#define MP3_INITIAL_SKIP   529

/* Size (in bytes) of an extension ("Xing") header, including the frame
 * header. */
#define XING_HEADER_SIZE   194

/*-----------------------------------------------------------------------*/

/* MPEG audio bitrate and frequency tables. */

static const uint16_t mpeg_kbitrate[2][3][15] = {
    {  // MPEG Version 1
        {32, 64, 96,128,160,192,224,256,288,320,352,384,416,448},  // Layer 1
        {32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384},  // Layer 2
        {32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320},  // Layer 3
    },
    {  // MPEG Version 2, 2.5
        {32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256},  // Layer 1
        { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160},  // Layer 2
        { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160},  // Layer 3
    },
};

static const uint16_t mpeg_pcmlen[2][3] = {
    /* Layer1  Layer2  Layer3 */
    {    384,   1152,   1152   },  // Version 1
    {    384,   1152,    576   },  // Version 2/2.5
};

static const uint16_t mpeg_freq[2][3] = {
    {44100, 48000, 32000},  // Version 1
    {22050, 24000, 16000},  // Version 2/2.5
};

/*-----------------------------------------------------------------------*/

/* sceAudiocodec control buffer (must be 64-byte aligned). */

typedef struct MP3ControlBuffer {
    uint32_t unknown00[3];
    void *EDRAM_ptr;
    uint32_t EDRAM_size;
    uint32_t unknown14;
    const void *src;    // Input MP3 data buffer
    uint32_t src_size;  // Input MP3 frame size (in bytes)
    void *dest;         // Output PCM buffer
    uint32_t dest_size; // Output PCM buffer size (in bytes)
    uint32_t unknown28; // Unknown (store frame size when decoding)
    uint32_t unknown2C[53];
} MP3ControlBuffer;


/* Private data structure (must be 64-byte aligned). */

struct SoundDecodePrivate {

    /* sceAudiocodec control buffer. */
    ALIGNED(64) MP3ControlBuffer mp3_control_buffer;

    /* Buffers for storing decoded PCM data.  Note that each buffer is
     * individually 64-byte aligned, since MP3_FRAME_PCMSIZE_STEREO is a
     * multiple of 64. */
    ALIGNED(64) uint8_t pcm_buffer[NUM_PCM_BUFFERS][MP3_FRAME_PCMSIZE_STEREO];
    /* PCM buffer usage flags (true = corresponding buffer contains PCM
     * data.  Set only by the decoding thread, cleared only by the main
     * thread. */
    uint8_t pcm_buffer_ok[NUM_PCM_BUFFERS];
    /* Stream position (in samples) of the first sample in each buffer. */
    int pcm_buffer_pos[NUM_PCM_BUFFERS];
    /* Number of valid samples in each buffer. */
    int pcm_buffer_len[NUM_PCM_BUFFERS];
    /* Index of the next buffer from which to extract PCM data.  Used only
     * by the main thread. */
    int next_pcm_buffer;
    /* Index of the first sample within next_pcm_buffer to extract on the
     * next get_pcm() call. */
    int next_pcm_offset;
    /* Stream sample index of the next sample to be returned from get_pcm(). */
    int next_pcm_pos;

    /* Handle for the decoding thread. */
    SysThreadID decode_thread;
    /* Flag used to terminate decoding.  Set by the main thread to request
     * termination; the decoding thread terminates as soon as it detects
     * this flag set. */
    uint8_t thread_stop;

    /* Number of samples per MP3 frame for this stream. */
    int frame_len;
    /* Number of samples to skip at the beginning of the stream (see notes
     * at MP3_INITIAL_SKIP above). */
    int initial_skip;
    /* Length (in samples) of the entire stream, excluding initial_skip, or
     * 0 if unknown. */
    int file_len;

    /* Flag indicating whether the stream position corresponding to the
     * beginning of the loop has been found. */
    uint8_t loop_found;
    /* Stream position (in bytes) from which to begin decoding when
     * restarting from the beginning of the loop. */
    int loop_file_pos;
    /* Stream sample index corresponding to loop_file_pos. */
    int loop_decode_pos;
    /* Byte positions of the last 12 frames (used to find the seek position
     * for looping; see comments in decode_thread()). */
    int frame_pos[12];

    /* Stream read position (in bytes).  Used only by the decoding thread. */
    int file_pos;
    /* Stream sample index corresponding to file_pos.  Used only by the
     * decoding thread. */
    int decode_pos;
    /* Number of samples to discard.  Used only by the decoding thread. */
    int discard;

    /* Pointer for garbage list. */
    SoundDecodePrivate *delete_next;

#ifdef DEBUG
    /* At least when running under PSPlink, calling DLOG() anywhere in
     * the decode thread seems to cause the kernel to kill the thread.
     * (Possibly a kernel race condition in sceIoWrite()?)  To get around
     * this, we write log messages into this buffer and pull them out in
     * the get_pcm() method. */
    char logbuf[1024];
#endif

};  // SoundDecodePrivate

/*-----------------------------------------------------------------------*/

/* Garbage list for SoundDecodePrivate structures.  We can't free private
 * data in the close() method because the decode thread will generally not
 * terminate immediately, so we link the data into this list and free it
 * separately once the thread has terminated. */
static SoundDecodePrivate *private_delete_list;

/*-----------------------------------------------------------------------*/

/* Decoder method declarations (other than open(), which is declared in
 * the header).  See ../../sound/decode.h for method details. */

static int psp_decode_mp3_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret);
static void psp_decode_mp3_close(SoundDecodeHandle *this);


/* Local routine declarations. */

/**
 * init_mp3_control_buffer:  Initialize an sceAudiocodec MP3 control buffer.
 *
 * [Parameters]
 *     mp3ctrl: MP3 control buffer.
 * [Return value]
 *     True on success, false on error.
 */
static int init_mp3_control_buffer(MP3ControlBuffer *mp3ctrl);

/**
 * start_decode_thread:  Start the decoding thread for an MP3 decoder.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int start_decode_thread(SoundDecodeHandle *this);

/**
 * decode_thread:  Thread routine for performing MP3 decoding.
 *
 * [Parameters]
 *     this: SoundDecodeHandle object.
 * [Return value]
 *     0
 */
static int decode_thread(void *this_);

/**
 * parse_xing_header:  Parse an MP3 extension header (Xing header) and
 * update the decoder's private data structure.  The header is assumed to
 * begin with a valid MP3 frame header as reported by mp3_frame_size().
 *
 * [Parameters]
 *     this: SoundDecodeHandle object.
 *     data: Header data (at least XING_HEADER_SIZE bytes).
 */
static void parse_xing_header(SoundDecodeHandle *this, const uint8_t *data);

/**
 * mp3_frame_size:  Return the frame size in bytes for the given MP3 frame
 * header.
 *
 * [Parameters]
 *     frame_header: MP3 frame header, interpreted as a 32-bit big-endian
 *         integer.
 * [Return value]
 *     Frame size including header, in bytes, or zero if the frame header
 *     is invalid.
 */
static CONST_FUNCTION int mp3_frame_size(const uint32_t frame_header);

/**
 * mp3_frame_pcmlen:  Return the PCM frame length in samples for the given
 * MP3 frame header.  The header is assumed to be valid as reported by
 * mp3_frame_size().
 *
 * [Parameters]
 *     frame_header: MP3 frame header, interpreted as a 32-bit big-endian
 *         integer.
 * [Return value]
 *     Length of decoded PCM data, in samples.
 */
static CONST_FUNCTION int mp3_frame_pcmlen(const uint32_t frame_header);

/**
 * mp3_frame_freq:  Return the sampling frequency for the given MP3 frame
 * header.  The header is assumed to be valid as reported by mp3_frame_size().
 *
 * [Parameters]
 *     frame_header: MP3 frame header, interpreted as a 32-bit big-endian
 *         integer.
 * [Return value]
 *     Sampling frequency, in Hz.
 */
static CONST_FUNCTION int mp3_frame_freq(const uint32_t frame_header);

/**
 * mp3_frame_bitrate:  Return the data rate for the given MP3 frame header.
 * The header is assumed to be valid as reported by mp3_frame_size().
 *
 * [Parameters]
 *     frame_header: MP3 frame header, interpreted as a 32-bit big-endian
 *         integer.
 * [Return value]
 *     Data rate, in bits per second.
 */
static CONST_FUNCTION int mp3_frame_bitrate(const uint32_t frame_header);

/**
 * mp3_frame_channels:  Return the number of channels for the given MP3
 * frame header.  The header is assumed to be valid as reported by
 * mp3_frame_size().
 *
 * This function is not currently used, but is included for completeness.
 *
 * [Parameters]
 *     frame_header: MP3 frame header, interpreted as a 32-bit big-endian
 *         integer.
 * [Return value]
 *     Number of channels (1 or 2).
 */
UNUSED static CONST_FUNCTION int mp3_frame_channels(
    const uint32_t frame_header);

/*************************************************************************/
/******************** Decoder method implementations *********************/
/*************************************************************************/

int psp_decode_mp3_open(SoundDecodeHandle *this)
{
    this->get_pcm = psp_decode_mp3_get_pcm;
    this->close   = psp_decode_mp3_close;

    this->private = mem_alloc(sizeof(*this->private), 64,
                              MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (!this->private) {
        DLOG("Out of memory");
        goto error_return;
    }
    if (!init_mp3_control_buffer(&this->private->mp3_control_buffer)) {
        goto error_free_private;
    }

    /* Parse the MP3 header of the first frame in the stream. */

    const uint8_t *data;
    if (decode_get_data(this, 0, 4, &data) != 4) {
        DLOG("Short file");
        goto error_free_EDRAM;
    }
    const uint32_t header = data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
    if (header>>21 != 0x7FF) {
        DLOG("MP3 frame not found");
        goto error_free_EDRAM;
    }
    if (!mp3_frame_size(header)) {
        DLOG("Invalid MP3 frame header: %02X %02X %02X %02X",
             data[0], data[1], data[2], data[3]);
        goto error_free_EDRAM;
    }
    this->native_freq = mp3_frame_freq(header);
    this->bitrate = mp3_frame_bitrate(header);
    this->stereo = 1;  // sceAudiocodec gives stereo PCM even for mono streams.
    this->loop_start = 0;
    this->loop_length = 0;
    this->private->frame_len = mp3_frame_pcmlen(header);
    this->private->initial_skip = MP3_INITIAL_SKIP;
    this->private->file_len = 0;  // Not yet known.
    if (decode_get_data(this, 0, XING_HEADER_SIZE, &data) == XING_HEADER_SIZE) {
        parse_xing_header(this, data);
    }

    /* The PSP's MP3 decoder seems to have an extra frame of decoding delay. */
    this->private->initial_skip += mp3_frame_pcmlen(header);

    /* Start up the decoding thread.  Since we always decode at least one
     * frame before reaching the first sample to output, the caller has
     * one frame's leeway to set this->loop_start, and in the current
     * implementation loop_start is always set (if appropriate) immediately
     * after creating the decoder, so we'll never detect the wrong loop
     * start point.  Even if we did, as long as loop_start is set before
     * the end of the loop, we'll still loop back to the right place (we
     * just end up discarding more data than we would otherwise). */
    if (!start_decode_thread(this)) {
        goto error_free_EDRAM;
    }

    this->private->next_pcm_buffer = 0;
    this->private->next_pcm_offset = 0;
    this->private->next_pcm_pos = 0;
    return 1;

  error_free_EDRAM:
    sceAudiocodecReleaseEDRAM((void *)&this->private->mp3_control_buffer);
  error_free_private:
    mem_free(this->private);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int psp_decode_mp3_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret)
{
    SoundDecodePrivate *private = this->private;
    const unsigned int sample_size = this->stereo ? 4 : 2;

    /* Our loop handling logic doesn't work if the loop start and end
     * points are in the same MP3 frame. */
    if (this->internal->loop
     && this->loop_length > 0
     && this->loop_length <= this->private->frame_len) {
        DLOG("Loop too short, must be at least %d samples",
             this->private->frame_len + 1);
        this->internal->loop = 0;
    }

    *loop_offset_ret = 0;
    int copied = 0;
    while (copied < pcm_len) {

        /* If the next PCM buffer hasn't been decoded yet, wait for the
         * decoder thread to give it to us. */
        while (!private->pcm_buffer_ok[private->next_pcm_buffer]) {
            if (!private->decode_thread
             || !sys_thread_is_running(private->decode_thread)) {
                break;
            }
            sceKernelDelayThread(100);
        }
        BARRIER();
        /* If we dropped out because the thread is no longer running, check
         * once more for a PCM buffer before quitting -- the thread may
         * have decoded the last frame of the stream and exited while we
         * were checking its status. */
        if (!private->pcm_buffer_ok[private->next_pcm_buffer]) {
            goto stopped;
        }

        /* Update the loop offset if we looped back. */
        const int this_pos = (private->pcm_buffer_pos[private->next_pcm_buffer]
                              + private->next_pcm_offset);
        *loop_offset_ret += private->next_pcm_pos - this_pos;
        private->next_pcm_pos = this_pos;

        /* Copy the decoded PCM data into the caller's buffer. */
        const int pcm_buffer_len =
            private->pcm_buffer_len[private->next_pcm_buffer];
        const int to_copy =
            min(pcm_len - copied, pcm_buffer_len - private->next_pcm_offset);
        memcpy((uint8_t *)pcm_buffer + copied * sample_size,
               private->pcm_buffer[private->next_pcm_buffer]
                   + (private->next_pcm_offset * sample_size),
               to_copy * sample_size);
        copied += to_copy;
        private->next_pcm_offset += to_copy;
        private->next_pcm_pos += to_copy;

        /* If we used up the entire decode buffer, free it and move on to
         * the next one. */
        if (private->next_pcm_offset >= pcm_buffer_len) {
            private->pcm_buffer_ok[private->next_pcm_buffer] = 0;
            BARRIER();
            private->next_pcm_buffer =
                (private->next_pcm_buffer + 1) % NUM_PCM_BUFFERS;
            private->next_pcm_offset = 0;
        }

    }  // while (copied < pcm_len)
  stopped:

#ifdef DEBUG
    if (*private->logbuf) {
        DLOG("Decode thread log:\n%s", private->logbuf);
        *private->logbuf = 0;
    }
#endif

    return copied;
}

/*-----------------------------------------------------------------------*/

static void psp_decode_mp3_close(SoundDecodeHandle *this)
{
    this->private->thread_stop = 1;
    this->private->delete_next = private_delete_list;
    private_delete_list = this->private;
}

/*************************************************************************/
/************************* PSP-internal routines *************************/
/*************************************************************************/

void psp_clean_mp3_garbage(int wait)
{
    SoundDecodePrivate *private, **prev_ptr;

    prev_ptr = &private_delete_list;
    while ((private = *prev_ptr) != NULL) {
        if (!private->decode_thread
         || !sys_thread_is_running(private->decode_thread)
         || (wait && private->thread_stop)) {
            if (private->decode_thread) {
                int result_unused;
                sys_thread_wait(private->decode_thread, &result_unused);
            }
            *prev_ptr = private->delete_next;
            sceAudiocodecReleaseEDRAM((void *)&private->mp3_control_buffer);
            mem_free(private);
        } else {
            prev_ptr = &private->delete_next;
        }
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static int init_mp3_control_buffer(MP3ControlBuffer *mp3ctrl)
{
    int res;

    res = sceAudiocodecCheckNeedMem((void *)mp3ctrl, PSP_CODEC_MP3);
    if (res < 0) {
        DLOG("sceAudiocodecCheckNeedMem(): %s", psp_strerror(res));
        return 0;
    }
    res = sceAudiocodecGetEDRAM((void *)mp3ctrl, PSP_CODEC_MP3);
    if (res < 0) {
        DLOG("sceAudiocodecGetEDRAM(): %s", psp_strerror(res));
        return 0;
    }
    res = sceAudiocodecInit((void *)mp3ctrl, PSP_CODEC_MP3);
    if (res < 0) {
        DLOG("sceAudiocodecInit(): %s", psp_strerror(res));
        sceAudiocodecReleaseEDRAM((void *)mp3ctrl);
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static int start_decode_thread(SoundDecodeHandle *this)
{
    static unsigned int threadnum;
    char namebuf[28];
    strformat(namebuf, sizeof(namebuf), "MP3DecodeThread_%u", threadnum++);
    this->private->file_pos = 0;
    this->private->decode_pos = -(this->private->initial_skip);
    this->private->discard = this->private->initial_skip;
    for (unsigned int i = 0; i < NUM_PCM_BUFFERS; i++) {
        this->private->pcm_buffer_ok[i] = 0;
    }
    this->private->decode_thread =
        psp_thread_create_named(namebuf, THREADPRI_MAIN - THREADPRI_SOUND,
                                4096, decode_thread, this);
    if (!this->private->decode_thread) {
        DLOG("psp_thread_create_named(%s) failed", namebuf);
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

#ifdef DEBUG
/* See the comment on SoundDecodePrivate.logbuf for why we need this macro. */
# define DECODE_LOG(format,...) \
    strformat(private->logbuf + strlen(private->logbuf), \
              sizeof(private->logbuf) - strlen(private->logbuf), \
              "   [decode_thread:%d] " format "\n", __LINE__ , ## __VA_ARGS__)
#else
# define DECODE_LOG(...)  ((void)0)  // Avoid "empty if body" warnings.
#endif

static int decode_thread(void *this_)
{
    SoundDecodeHandle *this = (SoundDecodeHandle *)this_;
    SoundDecodePrivate *private = this->private;
    MP3ControlBuffer *mp3ctrl = &private->mp3_control_buffer;
    const unsigned int sample_size = this->stereo ? 4 : 2;

    /* Index of the next PCM buffer to write to. */
    unsigned int target_pcm_buffer = 0;
    /* Safety valve to avoid infinite loops. */
    int allow_loop = 1;

    while (!private->thread_stop) {

        /* Wait for the next buffer to become available (or for the main
         * thread to tell us to stop). */
        while (private->pcm_buffer_ok[target_pcm_buffer]) {
            sceKernelDelayThread(1000);
            if (private->thread_stop) {
                goto stop_thread;
            }
        }
        BARRIER();

        /* Read in the next frame. */
        const uint8_t *data;
        int datalen = decode_get_data(this, private->file_pos,
                                      MP3_FRAME_MAXDATA, &data);
        if (datalen < 4) {
            if (datalen != 0) {
                DECODE_LOG("Short frame header at end of stream (0x%X)",
                           private->file_pos);
            }
            goto eof;
        }
        const uint32_t frame_header =
            data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
        const int frame_size = mp3_frame_size(frame_header);
        if (UNLIKELY(!frame_size)) {
            DECODE_LOG("Invalid frame header (%02X %02X %02X %02X) at 0x%X,"
                       " terminating stream", data[0], data[1], data[2],
                       data[3], private->file_pos);
            goto eof;
        }
        if (UNLIKELY(datalen < frame_size)) {
            DECODE_LOG("Short frame at end of stream (0x%X)",
                       private->file_pos);
            goto eof;
        }
        const int pcm_size = private->frame_len * sample_size;

        /* If we've reached the frame containing the loop start point,
         * record the byte offset in the stream from which we should
         * restart decoding on loop.  An MP3 frame can refer to up to 511
         * bytes of preceding data, so we look back to the positions of
         * previous frames to find the most recent frame which starts at
         * least 511 bytes earlier than the current one.  Sometimes the
         * decoder will fail on the very first frame, so we start decoding
         * on the first frame _before_ the frame containing the 511th
         * previous byte.  MP3 frames can be as small as 48 bytes, so we
         * store position data for the last ceil(511/48)+1 = 12 frames to
         * ensure that we can always find the right frame. */
        if (!private->loop_found) {
            if (private->decode_pos + private->frame_len
                > this->loop_start)
            {
                ASSERT(private->decode_pos <= this->loop_start);
                private->loop_found = 1;
                private->loop_decode_pos = private->decode_pos;
                if (private->file_pos > 0) {
                    private->loop_decode_pos -= private->frame_len;
                    unsigned int i;
                    for (i = lenof(private->frame_pos) - 1; i > 0; i--) {
                        if (private->frame_pos[i] + 511 <= private->file_pos) {
                            if (private->frame_pos[i] > 0) {
                                ASSERT(i > 0, break);
                                private->loop_decode_pos -= private->frame_len;
                                i--;
                            }
                            break;
                        }
                        if (private->frame_pos[i] == 0) {
                            break; // Can't go back past the top of the stream!
                        }
                        private->loop_decode_pos -= private->frame_len;
                    }
                    private->loop_file_pos = private->frame_pos[i];
                } else {
                    private->loop_file_pos = 0;
                }
            } else {
                unsigned int i;
                for (i = 0; i < lenof(private->frame_pos) - 1; i++) {
                    private->frame_pos[i] = private->frame_pos[i+1];
                }
                private->frame_pos[i] = private->file_pos;
            }
        }

        /* Decode the frame. */
        mp3ctrl->src       = data;
        mp3ctrl->src_size  = frame_size;
        mp3ctrl->dest      = private->pcm_buffer[target_pcm_buffer];
        mp3ctrl->dest_size = pcm_size;
        mp3ctrl->unknown28 = frame_size;
        int res = sceAudiocodecDecode((void *)mp3ctrl, PSP_CODEC_MP3);
        if (UNLIKELY(res < 0)) {
            DECODE_LOG("MP3 decode failed at 0x%X (decode_pos %d): %s",
                       private->file_pos, private->decode_pos,
                       psp_strerror(res));
            mem_clear(private->pcm_buffer, pcm_size);
        }
        int pcm_len = private->frame_len;
        if (private->discard > 0) {
            if (private->discard >= private->frame_len) {
                private->discard -= private->frame_len;
                private->decode_pos += private->frame_len;
                pcm_len = 0;
            } else {
                private->decode_pos += private->discard;
                pcm_len -= private->discard;
                memmove(private->pcm_buffer[target_pcm_buffer],
                        private->pcm_buffer[target_pcm_buffer]
                            + (private->discard * sample_size),
                        pcm_len * sample_size);
                private->discard = 0;
            }
        }

        /* Update position counters, ensuring that we don't go past the
         * end of the file (or loop, if appropriate). */
        int decode_limit = private->file_len;
        if (this->internal->loop && this->loop_length > 0) {
            const int loopend = this->loop_start + this->loop_length;
            if (private->decode_pos < loopend) {
                if (decode_limit == 0 || loopend < decode_limit) {
                    decode_limit = loopend;
                }
            }
        }
        if (decode_limit > 0 && pcm_len > decode_limit - private->decode_pos) {
            pcm_len = decode_limit - private->decode_pos;
        }
        private->pcm_buffer_pos[target_pcm_buffer] = private->decode_pos;
        private->pcm_buffer_len[target_pcm_buffer] = pcm_len;
        private->file_pos += frame_size;
        private->decode_pos += pcm_len;

        /* If we didn't store any samples, immediately proceed to the next
         * frame. */
        if (pcm_len <= 0) {
            continue;
        }

        /* We did store some samples, so we can safely loop now. */
        allow_loop = 1;

        /* Return this buffer to the main thread and advance to the next
         * buffer. */
        BARRIER();
        private->pcm_buffer_ok[target_pcm_buffer] = 1;
        BARRIER();
        target_pcm_buffer = (target_pcm_buffer + 1) % NUM_PCM_BUFFERS;

        /* Check for end-of-stream or end-of-loop. */
        if (decode_limit > 0 && private->decode_pos >= decode_limit) {
          eof:;
            const int loopend = this->loop_start + this->loop_length;
            if (this->internal->loop
                && (this->loop_length == 0
                    || private->decode_pos - private->frame_len < loopend))
            {
                if (UNLIKELY(!private->loop_found)) {
                    DECODE_LOG("WARNING: Failed to find loop start %d",
                               this->loop_start);
                    break;
                }
                if (UNLIKELY(private->file_pos == private->loop_file_pos)) {
                    DECODE_LOG("Failed to read any bytes from stream,"
                               " aborting loop");
                    break;
                }
                if (UNLIKELY(!allow_loop)) {
                    DECODE_LOG("Failed to return any samples after loop"
                               " start %d, aborting loop", this->loop_start);
                    break;
                }
                private->file_pos = private->loop_file_pos;
                private->decode_pos = private->loop_decode_pos;
                private->discard = this->loop_start - private->loop_decode_pos;
                if (UNLIKELY(private->discard < 0)) {
                    DECODE_LOG("Loop start was moved backward (now %d, decode"
                               " restart at %d); restarting from beginning of"
                               " stream and clearing restart info for next"
                               " loop", this->loop_start,
                               private->loop_decode_pos);
                    private->file_pos = 0;
                    private->decode_pos = -(private->initial_skip);
                    private->discard = this->loop_start + private->initial_skip;
                } else if (UNLIKELY(private->discard > 12*private->frame_len)) {
                    DECODE_LOG("Loop start was moved forward (now %d, decode"
                               " restart at %d); clearing restart info for"
                               " next loop", this->loop_start,
                               private->loop_decode_pos);
                    private->loop_found = 0;
                }
                allow_loop = 0;
            } else {
                break;
            }
        }

    }  // while (!private->thread_stop)

  stop_thread:
    return 0;
}

#undef DECODE_LOG

/*-----------------------------------------------------------------------*/

static void parse_xing_header(SoundDecodeHandle *this, const uint8_t *data)
{
    const uint32_t frame_header =
        data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
    const int mpeg_version_index = frame_header>>19 &  3;
    const int mpeg_layer_index   = frame_header>>17 &  3;
    const int mode_index         = frame_header>> 6 &  3;
    ASSERT(mpeg_version_index != 1);
    ASSERT(mpeg_layer_index != 0);

    int xing_offset;
    if (mpeg_version_index == 3) {  // Version 1
        xing_offset = (mode_index == 3) ? 4+17 : 4+32;
    } else {
        xing_offset = (mode_index == 3) ? 4+9 : 4+17;
    }
    data += xing_offset;

    if (memcmp(data, "Xing", 4) != 0 && memcmp(data, "Info", 4) != 0) {
        return;
    }
    data += 4;

    const uint32_t xing_flags =
        data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
    data += 4;

    int num_frames, data_size;
    if (xing_flags & 0x1) {  // Flag: number of frames present
        num_frames = data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
        data += 4;
    } else {
        DLOG("Xing header missing frame count, can't compute file length");
        return;
    }
    if (xing_flags & 0x2) {  // Flag: encoded data size present
        data_size = data[0]<<24 | data[1]<<16 | data[2]<<8 | data[3];
        data += 4;
    } else {
        data_size = 0;
    }
    if (xing_flags & 0x4) {  // Flag: seek index (100 bytes) present
        data += 100;
    }
    if (xing_flags & 0x8) {  // Flag: VBR scaling data present
        data += 4;
    }
    data += 21;

    const int encoder_delay   = data[0]<<4 | data[1]>>4;
    const int encoder_padding = (data[1] & 0x0F) << 8 | data[2];
    if (encoder_padding >= 529) {
        this->private->initial_skip = encoder_delay + 529;
    } else {
        DLOG("Final padding too short (%d), decode may be corrupt",
             encoder_padding);
        this->private->initial_skip = encoder_delay + encoder_padding;
    }
    const int samples_per_frame = mp3_frame_pcmlen(frame_header);
    this->private->file_len = num_frames * samples_per_frame
                              - (encoder_delay + encoder_padding);
    if (data_size > 0) {
        /* Bits per second = bits / duration
         *                 = bits / (frames * samples/frame / samples/sec)
         *                 = (bits * samples/sec) / (frames * samples/frame) */
        const int64_t bits = (int64_t)data_size * 8;
        const int64_t numerator = bits * this->native_freq;
        const int64_t denominator = this->private->file_len;
        this->bitrate = (int)((numerator + denominator/2) / denominator);
    }
}

/*-----------------------------------------------------------------------*/

static CONST_FUNCTION int mp3_frame_size(const uint32_t frame_header)
{
    const int mpeg_version_index = frame_header>>19 &  3;
    const int mpeg_layer_index   = frame_header>>17 &  3;
    const int bitrate_index      = frame_header>>12 & 15;
    const int freq_index         = frame_header>>10 &  3;
    const int padding            = frame_header>> 9 &  1;
    if (UNLIKELY(mpeg_version_index == 1)) {
        return 0;
    }
    if (UNLIKELY(mpeg_layer_index == 0)) {
        return 0;
    }
    if (UNLIKELY(freq_index == 3)) {
        return 0;
    }
    if (UNLIKELY(bitrate_index == 0) || UNLIKELY(bitrate_index == 15)) {
        return 0;
    }

    const int version_index = mpeg_version_index==3 ? 0 : 1;
    const int layer_index = 3 - mpeg_layer_index;
    const int kbitrate =
        mpeg_kbitrate[version_index][layer_index][bitrate_index-1];
    const int pcmlen = mp3_frame_pcmlen(frame_header);
    const int freq = mp3_frame_freq(frame_header);
    ASSERT(freq > 0, return 0);
    return ((pcmlen/8) * (1000*kbitrate) / freq)
         + (padding ? (mpeg_layer_index==3 ? 4 : 1) : 0);
}

/*-----------------------------------------------------------------------*/

static CONST_FUNCTION int mp3_frame_pcmlen(const uint32_t frame_header)
{
    const int mpeg_version_index = frame_header>>19 &  3;
    const int mpeg_layer_index   = frame_header>>17 &  3;

    const int version_index = mpeg_version_index==3 ? 0 : 1;
    const int layer_index = 3 - mpeg_layer_index;
    return mpeg_pcmlen[version_index][layer_index];
}

/*-----------------------------------------------------------------------*/

static CONST_FUNCTION int mp3_frame_freq(const uint32_t frame_header)
{
    const int mpeg_version_index = frame_header>>19 &  3;
    const int freq_index         = frame_header>>10 &  3;

    const int version_index = mpeg_version_index==3 ? 0 : 1;
    int freq = mpeg_freq[version_index][freq_index];
    if (mpeg_version_index == 0) {  // Version 2.5
        freq /= 2;
    }
    return freq;
}

/*-----------------------------------------------------------------------*/

static CONST_FUNCTION int mp3_frame_bitrate(const uint32_t frame_header)
{
    const int mpeg_version_index = frame_header>>19 &  3;
    const int mpeg_layer_index   = frame_header>>17 &  3;
    const int bitrate_index      = frame_header>>12 & 15;

    const int version_index = mpeg_version_index==3 ? 0 : 1;
    const int layer_index = 3 - mpeg_layer_index;
    const int kbitrate =
        mpeg_kbitrate[version_index][layer_index][bitrate_index-1];
    return kbitrate * 1000;
}

/*-----------------------------------------------------------------------*/

static CONST_FUNCTION int mp3_frame_channels(const uint32_t frame_header)  // NOTREACHED
{
    return (frame_header>>6 & 3) == 3 ? 1 : 2;  // NOTREACHED
}

/*************************************************************************/
/*************************************************************************/
