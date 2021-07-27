/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sound/decode-ogg.c: Audio decoder for Ogg Vorbis data.
 */

#ifdef SIL_SOUND_INCLUDE_OGG  // To the end of the file.

#include "src/base.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sound/decode-ogg.h"

#include <nogg.h>

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Private data structure. */

struct SoundDecodePrivate {
    vorbis_t *vorbis;
    int filepos;       // Read position for libnogg callbacks.
    uint8_t error;     // True if a fatal decoding error has occurred.
};

/*-----------------------------------------------------------------------*/

/* Method implementation declarations. */

static int decode_ogg_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret);
static void decode_ogg_close(SoundDecodeHandle *this);

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * get_loop_info:  Attempt to read loop information from the Ogg stream's
 * comment header.  Helper for decode_ogg_open().
 */
static void get_loop_info(SoundDecodeHandle *this);

/**
 * ogg_length:  Length callback for libnogg.
 *
 * [Parameters]
 *     opaque: Callback data (a SoundDecodeHandle pointer).
 * [Return value]
 *     Stream data length, in bytes, or -1 if the stream is not seekable.
 */
static int64_t ogg_length(void *opaque);

/**
 * ogg_tell:  Tell callback for libnogg.
 *
 * [Parameters]
 *     opaque: Callback data (a SoundDecodeHandle pointer).
 * [Return value]
 *     Current read position, in bytes, or -1 on error.
 */
static int64_t ogg_tell(void *opaque);

/**
 * ogg_seek:  Seek callback for libnogg.
 *
 * [Parameters]
 *     opaque: Callback data (a SoundDecodeHandle pointer).
 *     offset: New read position, in bytes.
 */
static void ogg_seek(void *opaque, int64_t offset);

/**
 * ogg_read:  Read callback for libnogg.
 *
 * [Parameters]
 *     opaque: Callback data (a SoundDecodeHandle pointer).
 *     buffer: Read buffer.
 *     length: Number of bytes to read.
 * [Return value]
 *     Number of bytes read.
 */
static int32_t ogg_read(void *opaque, void *buffer, int32_t length);

/**
 * ogg_malloc:  Memory allocation callback for libnogg.
 *
 * [Parameters]
 *     opaque: Callback data (a SoundDecodeHandle pointer).
 *     size: Number of bytes to allocate.
 *     align: Required alignment, or 0 for the default alignment.
 * [Return value]
 *     Pointer to the allocated memory, or NULL on error.
 */
static void *ogg_malloc(void *opaque, int32_t size, int32_t align);

/**
 * ogg_free:  Memory free callback for libnogg.
 *
 * [Parameters]
 *     opaque: Callback data (a SoundDecodeHandle pointer).
 *     ptr: Memory region to free (may be NULL).
 */
static void ogg_free(void *opaque, void *ptr);

#ifdef SIL_INCLUDE_TESTS
/* Set true to force a one-shot read error. */
static uint8_t TEST_fail_ogg_read;
#endif

/*************************************************************************/
/************************ Method implementations *************************/
/*************************************************************************/

int decode_ogg_open(SoundDecodeHandle *this)
{
    this->get_pcm = decode_ogg_get_pcm;
    this->close   = decode_ogg_close;

    this->private = mem_alloc(sizeof(*this->private), 0,
                              MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (!this->private) {
        DLOG("Out of memory");
        goto error_return;
    }

    vorbis_error_t error;
    this->private->vorbis = vorbis_open_callbacks(
        (const vorbis_callbacks_t){
            .length = ogg_length,
            .seek = ogg_seek,
            .tell = ogg_tell,
            .read = ogg_read,
            .malloc = ogg_malloc,
            .free = ogg_free},
        this, VORBIS_OPTION_READ_INT16_ONLY, &error);
    if (!this->private->vorbis) {
        DLOG("vorbis_open_callbacks() failed: %d", error);
        goto error_free_private;
    }

    const int channels = vorbis_channels(this->private->vorbis);
    if (channels == 1) {
        this->stereo = 0;
    } else if (channels == 2) {
        this->stereo = 1;
    } else {
        DLOG("Bad channel count %d", channels);
        goto error_close_vorbis;
    }
    const uint32_t rate = vorbis_rate(this->private->vorbis);
    if (rate > INT_MAX) {
        DLOG("Bad sampling rate %u", rate);
        goto error_close_vorbis;
    }
    this->native_freq = (int)rate;
    /* libnogg provides vorbis_bitrate(), but that's only an average, and
     * we need to ensure that the data is available when required for the
     * worst-case (highest-bitrate) scenario.  So we ignore vorbis_bitrate()
     * and report the bitrate of an equivalent 16-bit PCM stream instead.
     * Better safe than stuttering. */
    this->bitrate = this->native_freq * channels * 16;

    /* Look for loop comments in the header. */
    this->loop_start = 0;
    this->loop_length = 0;
    get_loop_info(this);

    /* If looping is enabled, look up the stream length to prime the seek
     * mechanism (avoids a delay on the first seek). */
    if (this->internal->loop) {
        (void) vorbis_length(this->private->vorbis);
    }

    return 1;

  error_close_vorbis:
    vorbis_close(this->private->vorbis);
  error_free_private:
    mem_free(this->private);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static int decode_ogg_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret)
{
    SoundDecodePrivate *private = this->private;
    const int channels = this->stereo ? 2 : 1;
    /* loopend is unused if loop_length == 0. */
    const int loopend = this->loop_start + this->loop_length;
    *loop_offset_ret = 0;

    if (private->error) {
        return 0;
    }

    int allow_loop = 1;  // Safety valve to avoid infinite loops.
    int copied = 0;
    while (copied < pcm_len) {

        int64_t curpos = vorbis_tell(private->vorbis);
        ASSERT(curpos >= 0, curpos = 0);
        int toread = pcm_len - copied;
        if (this->loop_length > 0 && curpos < loopend) {
            toread = min(toread, loopend - curpos);
        }

        vorbis_error_t error;
        int thisread;
        do {
            thisread = vorbis_read_int16(
                private->vorbis, pcm_buffer + (copied * channels),
                toread, &error);
            if (error == VORBIS_ERROR_DECODE_RECOVERED) {
                DLOG("WARNING: decompression error, data dropped!");
            }
        } while (thisread == 0 && error == VORBIS_ERROR_DECODE_RECOVERED);
        if (error && error != VORBIS_ERROR_STREAM_END) {
            DLOG("Decompression error: %d", error);
            private->error = 1;
            break;
        } else if (thisread == 0) {
            /* Reached the end of the audio stream. */
            if (this->internal->loop
             && (this->loop_length == 0 || curpos <= loopend)) {
                if ((int64_t)this->loop_start < curpos) {
                    /* allow_loop will only be false if vorbis_read_int16()
                     * returns EOF despite having seeked backwards on the
                     * previous loop, which should be impossible without a
                     * bug in libnogg. */
                    ASSERT(allow_loop, break);
                    if (this->loop_length > 0) {
                        DLOG("Warning: loopend %u > pcmlen %lld, looping"
                             " early", loopend, (long long)curpos);
                    }
                    if (!vorbis_seek(private->vorbis, this->loop_start)) {
                        DLOG("Seek error");
                        private->error = 1;
                        break;
                    } else {
                        *loop_offset_ret += curpos - this->loop_start;
                        curpos = this->loop_start;
                        allow_loop = 0;
                    }
                } else {
                    DLOG("Tried to loop but no data available, bailing");
                    break;
                }
            } else {
                break;
            }
        } else {
            copied += thisread;
            curpos += thisread;
        }

        const int64_t newpos = vorbis_tell(private->vorbis);
        if (UNLIKELY(newpos != curpos)) {
            DLOG("WARNING: decode position mismatch: current position is"
                 " %lld but should be %lld!  Audio data may be corrupt.",
                 (long long)newpos, (long long)curpos);
            curpos = newpos;
        }

        if (this->internal->loop && this->loop_length > 0
         && curpos == loopend)  {
            if (!vorbis_seek(private->vorbis, this->loop_start)) {
                DLOG("Seek error");
                private->error = 1;
                break;
            } else {
                *loop_offset_ret += curpos - this->loop_start;
            }
        }

    }  // while (copied < pcm_len)

    return copied;
}

/*-----------------------------------------------------------------------*/

static void decode_ogg_close(SoundDecodeHandle *this)
{
    vorbis_close(this->private->vorbis);
    mem_free(this->private);
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

#ifdef SIL_INCLUDE_TESTS

void sound_decode_ogg_test_fail_next_read(void)
{
    TEST_fail_ogg_read = 1;
}

#endif

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static void get_loop_info(SoundDecodeHandle *this)
{
    const uint8_t *header;
    /* We assume the comment header fits within the first 1000 bytes of the
     * file. */
    int limit = decode_get_data(this, 0, 1000, &header);

    /* These checks should never fail since we already successfully parsed
     * the headers when opening the file. */
    ASSERT(limit >= 58 + 27, return);
    header += 58;
    limit -= 58;

    const int num_segments = header[26];
    ASSERT(num_segments > 0, return);
    ASSERT(limit >= 27 + num_segments, return);
    int comment_size = header[27];
    for (int i = 1; header[27 + (i-1)] == 255; i++) {
        ASSERT(i < num_segments, break);
        comment_size += header[27 + i];
    }
    header += 27 + num_segments;
    limit -= 27 + num_segments;
    limit = ubound(comment_size, limit);

    ASSERT(limit >= 7, return);
    ASSERT(memcmp(header, "\3vorbis", 7) == 0, return);
    header += 7;
    limit -= 7;

    /* For simplicity, we just look for the strings LOOPSTART= and
     * LOOPLENGTH= in the comment buffer, rather than attempting to parse
     * the full structure of the header.  For further simplicity, we also
     * assume the relevant comment strings will each be less than 256
     * bytes long. */
    int start = -1, length = -1;
    for (int i = 1; i <= limit-14; i++) {
        char strbuf[256];
        if (memcmp(&header[i], "\0\0\0LOOPSTART=", 13) == 0) {
            const int len = header[i-1] - 10;
            if (len > 0) {
                memcpy(strbuf, &header[i+13], len);
                strbuf[len] = '\0';
                char *s;
                start = (int)strtol(strbuf, &s, 10);
                if (UNLIKELY(*s)) {
                    DLOG("Invalid value for LOOPSTART: %s", strbuf);
                    return;
                }
            }
        } else if (memcmp(&header[i], "\0\0\0LOOPLENGTH=", 14) == 0) {
            const int len = header[i-1] - 11;
            if (len > 0) {
                memcpy(strbuf, &header[i+14], len);
                strbuf[len] = '\0';
                char *s;
                length = (int)strtol(strbuf, &s, 10);
                if (UNLIKELY(*s)) {
                    DLOG("Invalid value for LOOPLENGTH: %s", strbuf);
                    return;
                }
            }
        }
    }

    if (start >= 0 && length >= 0) {
        this->loop_start = start;
        this->loop_length = length;
    }
}

/*************************************************************************/
/*********************** I/O routines for libnogg ************************/
/*************************************************************************/

static int64_t ogg_length(void *opaque)
{
    ASSERT(opaque != NULL, return -1);
    SoundDecodeHandle *handle = (SoundDecodeHandle *)opaque;

    return handle->internal->datalen;
}

/*-----------------------------------------------------------------------*/

static int64_t ogg_tell(void *opaque)
{
    ASSERT(opaque != NULL, return -1);
    SoundDecodeHandle *handle = (SoundDecodeHandle *)opaque;

    return handle->private->filepos;
}

/*-----------------------------------------------------------------------*/

static void ogg_seek(void *opaque, int64_t offset)
{
    ASSERT(opaque != NULL, return);
    SoundDecodeHandle *handle = (SoundDecodeHandle *)opaque;

    handle->private->filepos = bound(offset, 0, handle->internal->datalen);
}

/*-----------------------------------------------------------------------*/

static int32_t ogg_read(void *opaque, void *buffer, int32_t length)
{
    ASSERT(opaque != NULL, return 0);
    ASSERT(buffer != NULL, return 0);
    SoundDecodeHandle *handle = (SoundDecodeHandle *)opaque;

    const uint8_t *data;
    const int nread =
        decode_get_data(handle, handle->private->filepos, length, &data);
#ifdef SIL_INCLUDE_TESTS
    if (TEST_fail_ogg_read && nread > 0) {
        TEST_fail_ogg_read = 0;
        return 0;
    }
#endif
    memcpy(buffer, data, nread);
    handle->private->filepos += nread;
    return nread;
}

/*-----------------------------------------------------------------------*/

static void *ogg_malloc(UNUSED void *opaque, int32_t size, int32_t align)
{
    return mem_alloc(size, align, 0);
}

/*-----------------------------------------------------------------------*/

static void ogg_free(UNUSED void *opaque, void *ptr)
{
    mem_free(ptr);
}

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SOUND_INCLUDE_OGG
