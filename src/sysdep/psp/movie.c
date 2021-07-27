/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/movie.c: Movie playback support for the PSP.
 */

/*
 * This implementation of movie playback functionality allows playback of
 * H.264-encoded video and linear PCM audio encapsulated in a custom-format
 * stream file (see str-file.h and tools/streamux.c for details).
 *
 * To ensure smooth playback, the implementation creates one thread for
 * reading data and another for audio playback, leaving only video decoding
 * and rendering to the main thread.  The data reader thread reads one
 * frame at a time from the file, copying each frame into a ring buffer.
 * The main thread pulls each frame out of the ring buffer, displaying the
 * video image and copying the audio data into a separate ring buffer for
 * access by the playback thread, which pulls and plays audio data one
 * hardware buffer at a time.
 *
 * For both ring buffers, exactly one thread stores to the buffer and
 * exactly one thread reads from it, allowing a lock-free implementation.
 * Specifically:
 *    - The writer thread writes a data unit to the ring buffer slot
 *      selected by the current write index (call it i), waits until the
 *      current read index is not equal to i+1, then updates the current
 *      write index to i+1.
 *    - The reader thread waits until the current write index is not equal
 *      to the current read index (call it j), reads and processes the data
 *      in ring buffer slot j, then updates the current read index to j+1.
 * Since both pointers always advance in the same direction (and since PSP
 * code runs on a single CPU and two threads can never run simultaneously),
 * correct behavior is guaranteed without the use of locks.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/movie.h"
#include "src/resource.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/sysdep/psp/internal.h"
#include "src/sysdep/psp/sound-low.h"
#include "src/sysdep/psp/str-file.h"
#include "src/sysdep/psp/texture.h"
#include "src/sysdep/psp/thread.h"
#include "src/texture.h"
#include "src/time.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Video buffer size (in bytes).  Video frames must be no larger than this. */
#define VIDEO_BUFSIZE   0x18000

/* Audio buffer size (in samples), large enough for one frame's audio at
 * 15fps with a little extra for safety. */
#define SOUND_BUFLEN   3042  // ((int)(44100/(15/1.001)) + 100)
/* The same, in bytes. */
#define SOUND_BUFSIZE  (SOUND_BUFLEN*4)
/* Number of audio buffers to use (must be at least 3). */
#define SOUND_NUMBUFS   8
/* Hardware buffer size (in samples). */
#define SOUND_HW_BUFSIZE 512

/* Minimum number of file read buffers.  The code will attempt to allocate
 * as many buffers as possible, but will require at least this many buffers
 * for successful initialization. */
#define MIN_FILE_BUFFERS    30
/* Amount of free memory to leave after allocating file read buffers (in
 * bytes). */
#define FILEBUF_SPARE_MEMORY  (1024*1024)

/*-----------------------------------------------------------------------*/

/* Movie handle structure. */

struct SysMovieHandle {
    /* Basic data. */
    SysFile *fp;                // Movie filehandle
    int width, height;          // Frame size (in pixels)
    int32_t fps_num, fps_den;   // Frame rate (frames/second)
    int num_frames;             // Total frame count
    int frame;                  // Index of next frame to render
    int readframe;              // Index of next frame to read
    STRFrameIndex *frame_list;  // List of file offsets/sizes for each frame

    /* Reader thread handle. */
    SceUID read_thread;
    /* File read buffers.  Each frame's video and audio data (concatenated)
     * is written into a single buffer. */
    struct {
        uint8_t align_pad[64 - sizeof(STRFrameHeader)];
        union {
            uint8_t buf[sizeof(STRFrameHeader) + VIDEO_BUFSIZE + SOUND_BUFSIZE];
            struct {
                STRFrameHeader header;
                uint8_t data[0];
            };
        };
    } *filebuf;
    /* Number of file read buffers. */
    int filebuf_num;
    /* Index of next buffer to render (only written by the main thread). */
    int nextplay;
    /* Index of next buffer into which to read a frame (only written by
     * the reader thread).  nextread==nextplay indicates that no data is
     * available. */
    int nextread;
    /* End-of-stream flag (only set by the reader thread). */
    uint8_t eos;
    /* Flag set by the main thread to stop the reader thread. */
    uint8_t stop;

    /* Video rendering data. */
    int texture;                // Texture for video image, 0 if direct-render
    SceMpeg mpeg;               // MPEG decoding context
    void *mpeg_data;            // MPEG context data buffer
    SceMpegRingbuffer mpeg_ringbuffer;
    void *mpeg_es;
    struct SceMpegLLI ALIGNED(64) mpeg_lli[MAX_DMABLOCKS];
    SceMpegAu mpeg_au;
    /* See note in sys_movie_draw_frame() for why we need this flag. */
    uint8_t drew_last_frame;    // Did we draw the last frame yet?

    /* Audio playback data. */
    uint8_t direct_audio;       // Use direct hardware playback for audio?
    int sound_channel;          // Hardware/software sound channel for output
    float volume;               // Volume (0.0 ... 1.0)
    uint8_t ALIGNED(64) hwbuf[2][SOUND_HW_BUFSIZE*4]; // Hardware audio buffers
    int next_hwbuf;             // Next hardware buffer to play (0 or 1)
    struct {
        uint8_t ALIGNED(64) data[SOUND_BUFSIZE];
        int32_t valid;          // Number of valid samples in buffer
    } sound_buf[SOUND_NUMBUFS];
    int sound_playofs;          // Playback sample position in current buffer
    int sound_nextplay;         // Next buffer to play (after the current one)
    int sound_nextwrite;        // Next bfufer into which to store data
    uint8_t sound_exit;         // Thread stop flag (set by the main thread)
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * open_movie:  Prepare a movie stream for playback.  Implements
 * sys_movie_open() and psp_movie_open_direct().
 *
 * [Parameters]
 *     fh: As for sys_movie_open().
 *     offset: As for sys_movie_open().
 *     length: As for sys_movie_open().
 *     direct_render: True for psp_movie_open_direct(), false for
 *         sys_movie_open().
 *     direct_audio: True to use direct hardware output for audio.
 * [Return value]
 *     SysMovieHandle instance for the movie, or NULL on error.
 */
static SysMovieHandle *open_movie(SysFile *fh, int64_t offset, int length,
                                  int direct_render, int direct_audio);

/**
 * read_one_frame:  Read video and audio data for a single frame and store
 * it in movie->filebuf[buf].
 *
 * [Parameters]
 *     movie: Movie handle.
 *     frame: Frame number to read.
 *     buf: File read buffer index.
 *     deadline: Deadline for completing the read operation, in seconds.
 * [Return value]
 *     True on success, false on error (including end-of-stream).
 */
static int read_one_frame(SysMovieHandle *movie, int frame, int buf,
                          float deadline);

/**
 * movie_read_thread:  Thread routine for reading data from the movie file.
 *
 * [Parameters]
 *     args: Argument size (always 4).
 *     argp: Argument pointer (always a pointer to the movie handle).
 * [Return value]
 *     0
 */
static int movie_read_thread(SceSize args, void *argp);

/**
 * movie_hw_sound_callback:  Sound channel callback routine for movie audio
 * playback through a hardware sound channel.
 *
 * [Parameters]
 *     blocksize: Number of samples to generate.
 *     volume_ret: Pointer to variable to receive volume (0...PSP_VOLUME_MAX).
 *     userdata: Movie handle.
 * [Return value]
 *     Pointer to data buffer containing blocksize samples (NULL for silence).
 */
static const void *movie_hw_sound_callback(int blocksize, int *volume_ret,
                                           void *userdata);

/**
 * movie_sw_sound_open:  open() implementation for the software-mixer-based
 * movie sound decoder.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int movie_sw_sound_open(SoundDecodeHandle *this);

/**
 * movie_sw_sound_get_pcm:  get_pcm() implementation for the software-mixer-
 * based movie sound decoder.
 *
 * [Parameters]
 *     pcm_buffer: Buffer into which to store PCM (signed 16-bit) data.
 *     pcm_len: Number of samples to retrieve.
 *     loop_offset_ret: Pointer to variable to receive the number of
 *         samples skipped backward due to looping.
 * [Return value]
 *     Number of samples stored in pcm_buffer.
 */
static int movie_sw_sound_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret);

/**
 * movie_sw_sound_close:  close() implementation for the software-mixer-based
 * movie sound decoder.
 */
static void movie_sw_sound_close(SoundDecodeHandle *this);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysMovieHandle *sys_movie_open(SysFile *fh, int64_t offset, int length,
                               UNUSED int smooth_chroma)
{
    return open_movie(fh, offset, length, 0, 0);
}

/*-----------------------------------------------------------------------*/

void sys_movie_close(SysMovieHandle *movie)
{
    sys_movie_stop(movie);
    if (movie->read_thread >= 0) {
        movie->stop = 1;
        while (!psp_delete_thread_if_stopped(movie->read_thread, NULL)) {
            sceKernelDelayThread(1000);
        }
    }

    sceMpegFreeAvcEsBuf(&movie->mpeg, movie->mpeg_es);
    sceMpegDelete(&movie->mpeg);
    sceMpegRingbufferDestruct(&movie->mpeg_ringbuffer);
    mem_free(movie->mpeg_data);
    sceMpegFinish();

    texture_destroy(movie->texture);
    sys_file_close(movie->fp);
    mem_free(movie->frame_list);
    mem_free(movie->filebuf);
    mem_free(movie);
}

/*-----------------------------------------------------------------------*/

int sys_movie_width(SysMovieHandle *movie)
{
    return movie->width;
}

/*-----------------------------------------------------------------------*/

int sys_movie_height(SysMovieHandle *movie)
{
    return movie->height;
}

/*-----------------------------------------------------------------------*/

double sys_movie_framerate(SysMovieHandle *movie)
{
    /* Perform the division in single precision to reduce code size. */
    return (double)((float)movie->fps_num / (float)movie->fps_den);
}

/*-----------------------------------------------------------------------*/

void sys_movie_set_volume(SysMovieHandle *movie, float volume)
{
    movie->volume = volume;
}

/*-----------------------------------------------------------------------*/

int sys_movie_play(SysMovieHandle *movie)
{
    if (movie->direct_audio) {
        movie->sound_channel = psp_sound_start_channel(
            SOUND_HW_BUFSIZE, movie_hw_sound_callback, movie, 0x1000);
        if (UNLIKELY(movie->sound_channel < 0)) {
            DLOG("Movie %p: failed to start hardware sound channel", movie);
            return 0;
        }
    } else {
        SoundDecodeHandle *decoder =
            sound_decode_open_custom(movie_sw_sound_open, movie, 1);
        if (UNLIKELY(!decoder)) {
            DLOG("Failed to create decoder");
            return 0;
        }
        movie->sound_channel = sound_play_decoder(decoder, 0, movie->volume, 0);
        if (UNLIKELY(!movie->sound_channel)) {
            DLOG("Failed to start sound");
            sound_decode_close(decoder);
            return 0;
        }
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_movie_stop(SysMovieHandle *movie)
{
    if (movie->sound_channel >= 0) {
        if (movie->direct_audio) {
            psp_sound_stop_channel(movie->sound_channel);
        } else {
            sound_cut(movie->sound_channel);
        }
        movie->sound_channel = -1;
    }
}

/*-----------------------------------------------------------------------*/

int sys_movie_get_texture(
    SysMovieHandle *movie,
    float *left_ret, float *right_ret, float *top_ret, float *bottom_ret)
{
    if (movie->texture) {
        *left_ret = 0;
        *right_ret = (float)movie->width / texture_width(movie->texture);
        *top_ret = 0;
        *bottom_ret = (float)movie->height / texture_height(movie->texture);
    }
    return movie->texture;
}

/*-----------------------------------------------------------------------*/

int sys_movie_draw_frame(SysMovieHandle *movie)
{
    int res;
    const int is_first_frame = (movie->frame < 0);
    int drawing_last_frame = 0;

    /* Wait for the next frame if it hasn't been read in yet. */
    while (movie->nextplay == movie->nextread) {
        if (movie->eos) {
            /* Reached the end of the stream.  There's still one frame
             * buffered by the sceMpeg library (see note below), so we
             * don't return "end of stream" to the caller until that frame
             * has been drawn. */
            if (movie->drew_last_frame) {
                return 0;
            } else {
                drawing_last_frame = 1;
                break;
            }
        }
        sceKernelDelayThread(1000);
    }

    if (movie->direct_audio && is_first_frame) {
        /* When displaying the first frame, we feed one frame of silence
         * to the audio hardware to ensure A/V sync (and avoid stuttering).
         * Since audio is not playing yet, we don't need to check whether
         * there's room in the audio ring buffer. */
        int i;
        for (i = 0; i < 1; i++) {
            movie->sound_buf[movie->sound_nextwrite].valid =
                (44100*movie->fps_den) / movie->fps_num;
            mem_clear(movie->sound_buf[movie->sound_nextwrite].data,
                      4 * movie->sound_buf[movie->sound_nextwrite].valid);
            movie->sound_nextwrite =
                (movie->sound_nextwrite + 1) % lenof(movie->sound_buf);
        }
    }

    int32_t video_bytes = movie->filebuf[movie->nextplay].header.video_size;
    int32_t audio_bytes = movie->filebuf[movie->nextplay].header.audio_size;
    uint8_t *video_buf  = movie->filebuf[movie->nextplay].data;
    uint8_t *audio_buf  = movie->filebuf[movie->nextplay].data
                        + movie->filebuf[movie->nextplay].header.video_size
                        + movie->filebuf[movie->nextplay].header.video_padding;

    /* Copy the video data to Media Engine memory for decoding. */
    int32_t left = video_bytes;
    int block = 0;
    uint8_t *src  = video_buf;
    uint8_t *dest = (uint8_t *)0x4A000;  // Address within the ME memory space.
    while (left > 0 && block < MAX_DMABLOCKS) {
        movie->mpeg_lli[block].src  = src;
        movie->mpeg_lli[block].dest = dest;
        movie->mpeg_lli[block].size = ubound(left, MAX_DMASIZE);
        movie->mpeg_lli[block].next = NULL;
        if (block > 0) {
            movie->mpeg_lli[block-1].next = &movie->mpeg_lli[block];
        }
        src  += movie->mpeg_lli[block].size;
        dest += movie->mpeg_lli[block].size;
        left -= movie->mpeg_lli[block].size;
        block++;
    }
    ASSERT(left == 0);
    sceKernelDcacheWritebackInvalidateAll();
    res = sceMpegbase_BEA18F91(movie->mpeg_lli);
    if (UNLIKELY(res < 0)) {
        DLOG("Frame %d (0x%X): sceMpegbase_BEA18F91(): %s", movie->frame,
             is_first_frame ? 0 : movie->frame_list[movie->frame].offset,
             psp_strerror(res));
        return 0;
    }

    /* Decode the video data to the texture buffer, or directly to the
     * rendering buffer if using direct rendering.  Note that the sceMpeg
     * library seems to "lag" one frame behind: for the first AU,
     * sceMpegAvcDecode() returns false in got_picture and doesn't draw
     * anything, and for subsequent AUs, sceMpegAvcDecode() draws the image
     * from the previous frame. */
    int got_picture = 0;
    void *outptr;
    int stride;
    SysTexture *systex;
    if (movie->texture) {
        systex = texture_lock_raw(movie->texture);
        if (UNLIKELY(!systex)) {
            DLOG("Failed to lock movie texture");
            return 0;
        }
        outptr = systex->pixels[0];
        stride = systex->stride;
    } else {
        systex = NULL;  // Avoid a compiler warning.
        const int x = (psp_framebuffer_width() - movie->width) / 2;
        const int y = (psp_framebuffer_height() - movie->height) / 2;
        psp_sync_framebuffer(x, y, movie->width, movie->height);
        outptr = psp_fb_pixel_address(x, y);
        stride = psp_framebuffer_stride();
    }
    if (drawing_last_frame) {
        res = sceMpegAvcDecodeStop(&movie->mpeg, stride, &outptr,
                                   &got_picture);
    } else {
        movie->mpeg_au.iAuSize = video_bytes;
        res = sceMpegAvcDecode(&movie->mpeg, &movie->mpeg_au, stride, &outptr,
                               &got_picture);
    }
    if (movie->texture) {
        /* The decoder clears the alpha byte to 0, so we need to fix it.
         * We cast through uintptr_t to avoid a cast-align warning. */
        ASSERT(systex->stride % 4 == 0);
        uint32_t *pixels = (uint32_t *)(uintptr_t)systex->pixels[0];
        uint32_t *pixels_top = pixels + (systex->stride * systex->height);
        for (; pixels < pixels_top; pixels += 4) {
            pixels[0] |= 255<<24;
            pixels[1] |= 255<<24;
            pixels[2] |= 255<<24;
            pixels[3] |= 255<<24;
        }
        texture_unlock(movie->texture);
    }
    if (UNLIKELY(res != 0)) {
        DLOG("Frame %d (0x%X): sceMpegAvcDecode%s(): %s", movie->frame,
             is_first_frame ? 0 : movie->frame_list[movie->frame].offset,
             drawing_last_frame ? "Stop" : "", psp_strerror(res));
        return 0;
    }

    /* Copy the audio data to the audio ring buffer. */
    if (!drawing_last_frame) {
        const int nextnext =
            (movie->sound_nextwrite+1) % lenof(movie->sound_buf);
        if (nextnext == movie->sound_nextplay) {
            DLOG("Frame %d (0x%X): audio buffer overrun!", movie->frame,
                 is_first_frame ? 0 : movie->frame_list[movie->frame].offset);
        } else if (audio_bytes > (int)sizeof(movie->sound_buf[0].data)) {
            DLOG("Frame %d (0x%X): too much audio data! (%d, bufsize=%d)",
                 movie->frame,
                 is_first_frame ? 0 : movie->frame_list[movie->frame].offset,
                 video_bytes, sizeof(movie->sound_buf[0].data));
        } else {
            memcpy(movie->sound_buf[movie->sound_nextwrite].data,
                   audio_buf, audio_bytes);
            movie->sound_buf[movie->sound_nextwrite].valid = audio_bytes/4;
            movie->sound_nextwrite = nextnext;
        }
    }

    /* Advance to the next frame. */
    if (drawing_last_frame) {
        movie->drew_last_frame = 1;
    } else {
        movie->frame++;
        movie->nextplay = (movie->nextplay + 1) % movie->filebuf_num;
    }

    /* If this was the first frame, do it all over again because we didn't
     * get an output frame. */
    if (is_first_frame) {
        return sys_movie_draw_frame(movie);
    }

    return 1;
}

/*************************************************************************/
/**************** PSP-specific global interface routines *****************/
/*************************************************************************/

int psp_movie_open_direct(const char *path, int direct_audio)
{
    int64_t offset;
    int size;
    SysFile *fh = resource_internal_open_file(path, &offset, &size);
    if (!fh) {
        DLOG("Failed to open movie file %s", path);
        return 0;
    }

    SysMovieHandle *movie = open_movie(fh, offset, size, 1, direct_audio);
    if (!movie) {
        DLOG("Failed to initialize movie handle for %s", path);
        return 0;
    }

    const int id = movie_import(movie);
    if (!id) {
        DLOG("Failed to register movie handle for %s", path);
        sys_movie_close(movie);
        return 0;
    }

    return id;
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static SysMovieHandle *open_movie(SysFile *fh, int64_t offset, int length,
                                  int direct_render, int direct_audio)
{
    SysMovieHandle *movie;
    int res;

    /* Create the movie handle. */
    movie = mem_alloc(sizeof(*movie), 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!movie)) {
        DLOG("alloc(SysMovieHandle) failed");
        goto error_return;
    }
    movie->filebuf_num =
        min(mem_contig(0), mem_avail(0) - FILEBUF_SPARE_MEMORY)
        / sizeof(*movie->filebuf);
    if (UNLIKELY(movie->filebuf_num < MIN_FILE_BUFFERS)) {
        DLOG("Not enough memory for >=%d file buffers (mem_avail=%lld,"
             " mem_contig=%lld)", MIN_FILE_BUFFERS, mem_avail(0),
             mem_contig(0));
        goto error_free_movie;
    }
    while (!(movie->filebuf = mem_alloc(
                 sizeof(*movie->filebuf) * movie->filebuf_num, 0, 0))) {
        movie->filebuf_num--;
        if (movie->filebuf_num < MIN_FILE_BUFFERS) {
            DLOG("Failed to allocate >=%d file buffers!", MIN_FILE_BUFFERS);
            goto error_free_movie;
        }
    }
    movie->fp            = fh;
    movie->frame         = -1;  // The first video decode call returns no data.
    movie->read_thread   = -1;
    movie->direct_audio  = direct_audio;
    movie->sound_channel = -1;
    movie->volume        = 1.0f;

    /* Initialize the MPEG decoding library. */
    res = sceMpegInit();
    if (UNLIKELY(res < 0)) {
        DLOG("sceMpegInit(): %s", psp_strerror(res));
        goto error_free_filebuf;
    }

    /* Read and parse the file header. */
    STRFileHeader header;
    sys_file_seek(movie->fp, offset, FILE_SEEK_SET);
    if (UNLIKELY((int)sizeof(header) > length)
     || UNLIKELY(sys_file_read(movie->fp, &header, sizeof(header))
                 != sizeof(header))) {
        DLOG("Failed to read header");
        goto error_close_mpeg_library;
    }
    if (UNLIKELY(memcmp(header.magic, "STR\0", 4) != 0)) {
        DLOG("Bad header magic: %.4s", header.magic);
        goto error_close_mpeg_library;
    }
    if (UNLIKELY(header.header_size != sizeof(header))) {
        DLOG("Bad header size: %d", header.header_size);
        goto error_close_mpeg_library;
    }
    if (UNLIKELY(header.max_video_size > VIDEO_BUFSIZE)) {
        DLOG("Max video frame size %u too large (must be <= %u)",
             header.max_video_size, VIDEO_BUFSIZE);
        goto error_close_mpeg_library;
    }
    if (UNLIKELY(header.max_audio_size > SOUND_BUFSIZE)) {
        DLOG("Max audio frame size %u too large (must be <= %u)",
             header.max_audio_size, SOUND_BUFSIZE);
        goto error_close_mpeg_library;
    }
    if (UNLIKELY(!header.fps_num || !header.fps_den)) {
        DLOG("Frame rate numerator or denominator is zero");
        goto error_close_mpeg_library;
    }
    movie->num_frames = header.num_frames;
    movie->width      = header.width;
    movie->height     = header.height;
    movie->fps_num    = header.fps_num;
    movie->fps_den    = header.fps_den;
    const int frame_list_size = sizeof(*movie->frame_list) * movie->num_frames;
    movie->frame_list = mem_alloc(frame_list_size, 0, MEM_ALLOC_TEMP);
    if (UNLIKELY(!movie->frame_list)) {
        DLOG("No memory for frame list (%d frames)", movie->num_frames);
        goto error_close_mpeg_library;
    }
    if (UNLIKELY((int)(sizeof(header) + frame_list_size) > length)
     || UNLIKELY(sys_file_read(movie->fp, movie->frame_list, frame_list_size)
                 != frame_list_size)) {
        DLOG("Failed to read frame list (%d frames)", movie->num_frames);
        goto error_free_frame_list;
    }

    /* Create a texture if direct rendering was not requested. */
    if (direct_render) {
        movie->texture = 0;
    } else {
        const int tex_width = align_up(movie->width, 16);
        const int tex_height = movie->height;
        SysTexture *systex = psp_create_vram_texture(tex_width, tex_height);
        if (UNLIKELY(!systex)) {
            DLOG("Failed to create texture for rendering");
            goto error_free_frame_list;
        }
        mem_clear(systex->pixels[0], systex->stride * systex->height * 4);
        movie->texture = texture_import(systex, MEM_ALLOC_TEMP);
        if (UNLIKELY(!movie->texture)) {
            DLOG("Failed to register texture for rendering");
            sys_texture_destroy(systex);
            goto error_free_frame_list;
        }
    }

    /* Prepare an MPEG decoding context. */
    int size = sceMpegQueryMemSize(0);
    if (UNLIKELY(size < 0)) {
        DLOG("sceMpegQueryMemSize(0): %s", psp_strerror(size));
        goto error_destroy_texture;
    } else if (UNLIKELY(size == 0)) {
        DLOG("sceMpegQueryMemSize(0) returned 0 -- library not loaded?");
        goto error_destroy_texture;
    }
    movie->mpeg_data = mem_alloc(size, 64, MEM_ALLOC_TEMP);
    if (UNLIKELY(!movie->mpeg_data)) {
        DLOG("No memory for MPEG library data buffer (%d bytes)", size);
        goto error_destroy_texture;
    }
    res = sceMpegRingbufferConstruct(&movie->mpeg_ringbuffer, 0, 0, 0, 0, 0);
    if (UNLIKELY(res != 0)) {
        DLOG("sceMpegRingbufferConstruct(): %s", psp_strerror(res));
        goto error_free_mpeg_data;
    }
    res = sceMpegCreate(&movie->mpeg, movie->mpeg_data, size,
                        &movie->mpeg_ringbuffer, 512, 0, 0);
    if (UNLIKELY(res != 0)) {
        DLOG("sceMpegCreate(): %s", psp_strerror(res));
        goto error_destroy_mpeg_ringbuffer;
    }
    movie->mpeg_es = sceMpegMallocAvcEsBuf(&movie->mpeg);
    if (UNLIKELY(!movie->mpeg_es)) {
        DLOG("sceMpegMallocAvcEsBuf() failed");
        goto error_delete_mpeg_handle;
    }
    sceMpegInitAu(&movie->mpeg, movie->mpeg_es, &movie->mpeg_au);

    /* Start the data reader thread. */
    SysMovieHandle *movie_param = movie;
    movie->read_thread = psp_start_thread(
        "MovieReadThread", movie_read_thread, THREADPRI_FILEIO,
        0x1000, sizeof(movie_param), &movie_param);
    if (UNLIKELY(movie->read_thread < 0)) {
        goto error_free_avc_es_buf;
    }

    /* Success! */
    return movie;

  error_free_avc_es_buf:
    sceMpegFreeAvcEsBuf(&movie->mpeg, movie->mpeg_es);
  error_delete_mpeg_handle:
    sceMpegDelete(&movie->mpeg);
  error_destroy_mpeg_ringbuffer:
    sceMpegRingbufferDestruct(&movie->mpeg_ringbuffer);
  error_free_mpeg_data:
    mem_free(movie->mpeg_data);
  error_destroy_texture:
    texture_destroy(movie->texture);
  error_free_frame_list:
    mem_free(movie->frame_list);
  error_close_mpeg_library:
    sceMpegFinish();
  error_free_filebuf:
    mem_free(movie->filebuf);
  error_free_movie:
    mem_free(movie);
  error_return:
    sys_file_close(fh);
    return NULL;
}

/*-----------------------------------------------------------------------*/

static int read_one_frame(SysMovieHandle *movie, int frame, int buf,
                          float deadline)
{
    PRECOND(movie != NULL, return 0);

    const int32_t offset = movie->frame_list[frame].offset;
    const int32_t size   = movie->frame_list[frame].size;
    uint8_t * const dest = movie->filebuf[buf].buf;
    if (UNLIKELY(size > (int)sizeof(movie->filebuf[buf].buf))) {
        DLOG("Frame %d (0x%X): frame too large (%d > %d)", frame, offset,
             size, sizeof(movie->filebuf[buf].buf));
        return 0;
    }
    int request = sys_file_read_async(movie->fp, dest, size, offset, deadline);
    if (UNLIKELY(!request)) {
        DLOG("Frame %d (0x%X): async read failed", frame, offset);
        return 0;
    }
    int32_t result = sys_file_wait_async(request);
    if (UNLIKELY(result != size)) {
        DLOG("Frame %d (0x%X): read frame failed", movie->readframe, offset);
        return 0;
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

static int movie_read_thread(UNUSED SceSize args, void *argp)
{
    SysMovieHandle *movie = *((SysMovieHandle **)argp);
    PRECOND(movie != NULL, return 0);
    PRECOND(movie->fps_num != 0, return 0);
    const float spf = (float)movie->fps_den / (float)movie->fps_num;

    while (!movie->stop && movie->readframe < movie->num_frames) {
        /* Determine the deadline for completing the read operation. */
        const int32_t frames_ahead = movie->readframe - movie->frame;
        const float sec_ahead = (float)frames_ahead * spf;
        /* Read the frame data into the current buffer. */
        if (!read_one_frame(movie, movie->readframe, movie->nextread,
                            sec_ahead)) {
            break;
        }
        movie->readframe++;
        /* Wait until the next read buffer is available for writing. */
        int nextbuf = (movie->nextread + 1) % movie->filebuf_num;
        while (!movie->stop && nextbuf == movie->nextplay) {
            sceKernelDelayThread(1000);
        }
        /* Update the current buffer index and proceed to the next frame. */
        movie->nextread = nextbuf;
    }  // while (movie->readframe < movie->num_frames)

    movie->eos = 1;
    return 0;
}

/*-----------------------------------------------------------------------*/

static const void *movie_hw_sound_callback(
    UNUSED int blocksize, int *volume_ret, void *userdata)
{
    PRECOND(volume_ret != NULL, return NULL);
    PRECOND(userdata != NULL, return NULL);
    SysMovieHandle *movie = (SysMovieHandle *)userdata;

    /* First make sure we have data to play back. */
    if (UNLIKELY(movie->sound_nextwrite == movie->sound_nextplay)) {
        return NULL;
    }

    /* Copy audio data to the hardware output buffer.  We may need to copy
     * from multiple input buffers, so we loop until the output buffer is
     * full. */
    uint8_t *hwbuf = movie->hwbuf[movie->next_hwbuf];
    int copied = 0;
    while (copied < SOUND_HW_BUFSIZE) {
        const int wanted = SOUND_HW_BUFSIZE - copied;
        if (movie->sound_nextplay == movie->sound_nextwrite) {
            DLOG("BUFFER UNDERRUN! clear last %d samples", wanted);
            mem_clear(hwbuf + copied*4, wanted*4);
            copied += wanted;
            continue;
        }
        int nextplay = movie->sound_nextplay;
        int playofs = movie->sound_playofs;
        const int avail = movie->sound_buf[nextplay].valid - playofs;
        const int tocopy = min(wanted, avail);
        ASSERT(tocopy > 0, break);
        memcpy(hwbuf + copied*4,
               movie->sound_buf[nextplay].data + playofs*4, tocopy*4);
        copied += tocopy;
        playofs += tocopy;
        if (playofs < movie->sound_buf[nextplay].valid) {
            movie->sound_playofs = playofs;
        } else {  // We used up the entire input buffer.
            movie->sound_nextplay = (nextplay+1) % lenof(movie->sound_buf);
            movie->sound_playofs = 0;
        }
    }  // while (copied < SOUND_HW_BUFSIZE)

    movie->next_hwbuf = (movie->next_hwbuf + 1) % lenof(movie->hwbuf);
    /* Divide by 2 to match the behavior of the software mixer. */
    *volume_ret = iroundf((bound(movie->volume,0,1) * PSP_VOLUME_MAX) / 2);
    return hwbuf;
}

/*-----------------------------------------------------------------------*/

static int movie_sw_sound_open(SoundDecodeHandle *this)
{
    SysMovieHandle *movie = (SysMovieHandle *)this->custom_data;
    PRECOND(movie != NULL, return 0);

    this->get_pcm     = movie_sw_sound_get_pcm;
    this->close       = movie_sw_sound_close;
    this->stereo      = 1;
    this->native_freq = 44100;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int movie_sw_sound_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret)
{
    SysMovieHandle *movie = (SysMovieHandle *)this->custom_data;
    PRECOND(movie != NULL, return 0);

    *loop_offset_ret = 0;  // We don't loop.

    int copied = 0;
    while (copied < pcm_len) {
        const int wanted = pcm_len - copied;
        if (movie->sound_nextplay == movie->sound_nextwrite) {
            DLOG("BUFFER UNDERRUN! clear last %d samples", wanted);
            mem_clear(pcm_buffer + copied*2, wanted*4);
            copied += wanted;
            continue;
        }
        int nextplay = movie->sound_nextplay;
        int playofs = movie->sound_playofs;
        const int avail = movie->sound_buf[nextplay].valid - playofs;
        const int tocopy = min(wanted, avail);
        ASSERT(tocopy > 0, break);
        memcpy(pcm_buffer + copied*2,
               movie->sound_buf[nextplay].data + playofs*4, tocopy*4);
        copied += tocopy;
        playofs += tocopy;
        if (playofs < movie->sound_buf[nextplay].valid) {
            movie->sound_playofs = playofs;
        } else {  // We used up the entire input buffer.
            movie->sound_nextplay = (nextplay+1) % lenof(movie->sound_buf);
            movie->sound_playofs = 0;
        }
    }

    return copied;
}

/*-----------------------------------------------------------------------*/

static void movie_sw_sound_close(UNUSED SoundDecodeHandle *this)
{
    /* Nothing to do. */
}

/*************************************************************************/
/*************************************************************************/
