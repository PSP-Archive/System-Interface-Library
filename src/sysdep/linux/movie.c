/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/movie.c: Movie playback support for Linux.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/sysdep/opengl/opengl.h"
#include "src/sysdep/posix/files.h"
#include "src/texture.h"
#include "src/time.h"
#include "src/utility/yuv2rgb.h"

#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG

/* On 32-bit x86, the alignment of int64_t in (at least) GCC-compiled code
 * varies depending on whether SSE instructions are enabled.  We build SIL
 * programs with SSE instructions enabled by default, but distributions
 * will typically build libraries with SSE disabled, so we enforce 4-byte
 * alignment when reading the FFmpeg headers to try and avoid structure
 * layout mismatches. */
# if defined(__GNUC__) && defined(SIL_ARCH_X86_32)
#  pragma pack(4)
# endif

# include <libavcodec/avcodec.h>
# include <libavformat/avformat.h>
# include <libavresample/avresample.h>
# include <libavutil/opt.h>
# if LIBAVCODEC_VERSION_MAJOR >= 55
#  include <libavutil/frame.h>
#  define avcodec_alloc_frame av_frame_alloc
#  define avcodec_free_frame av_frame_free
# endif
# if LIBAVCODEC_VERSION_MAJOR < 57
#  define codecpar codec
#  define AV_PIX_FMT_YUV420P PIX_FMT_YUV420P
#  define av_packet_unref av_free_packet
# endif

# if defined(__GNUC__) && defined(SIL_ARCH_X86_32)
#  pragma pack()
# endif

# ifdef DEBUG
#  include <ctype.h>  // For isspace().
# endif

#endif  // SIL_PLATFORM_LINUX_USE_FFMPEG

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Data for an opened movie. */
struct SysMovieHandle {
    SysFile *fh;                // File handle to read from
    int64_t base_offset;        // Base file offset of movie data
    int64_t end_offset;         // Offset of last byte of movie data + 1
    int64_t video, audio;       // Current file offsets for video and audio
    int width, height;          // Size of the output image
    double framerate;           // Movie framerate
    int stereo;                 // Flag: is audio stream in stereo?
    int audio_rate;             // Audio playback rate
    float volume;               // Current volume
    int sound_channel;          // Sound channel for output
    int texture;                // Output texture
    uint8_t playing;            // Is the movie currently playing?
    uint8_t smooth_uv;          // Should U/V planes be linearly interpolated?
    /* For audio processing: */
    uint8_t *chunk_buf;         // Decoded audio buffer
    int32_t chunk_size;         // Decoded audio length (in samples)
    int32_t chunk_pos;          // Current position in audio (in samples),
                                //    negative = insert silence at beginning

#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG
    uint8_t is_ffmpeg;          // Are we using the FFmpeg decoder?
    int video_stream;           // Video stream index
    int audio_stream;           // Audio stream index
    AVIOContext *v_reader;      // AVIO handle for video
    AVFormatContext *v_demuxer; // libavformat handle for video
    AVCodecContext *v_decoder;  // libavcodec handle for video
    AVIOContext *a_reader;      // AVIO handle for audio
    AVFormatContext *a_demuxer; // libavformat handle for audio
    AVCodecContext *a_decoder;  // libavcodec handle for audio
    AVAudioResampleContext *a_conv; // Audio format conversion handle
    void *audio_inbuf;          // Raw audio input data buffer
    int32_t audio_insize;       // Size of audio_inbuf (in bytes)
    int32_t audio_inpos;        // Offset of first unconsumed audio byte
#endif
};

/*-----------------------------------------------------------------------*/

/* Local routine declarations. */

/**
 * get_frame:  Read the next frame of the movie into movie->texture.
 *
 * [Parameters]
 *     movie: Movie handle.
 * [Return value]
 *     True if a frame was successfully read, false otherwise.
 */
static int get_frame(SysMovieHandle *movie);

/*----------------------------------*/

#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG

/**
 * ffmpeg_init_decoder:  Prepare to play a movie using the FFmpeg libraries.
 *
 * [Parameters]
 *     movie: Movie handle.
 * [Return value]
 *     True if the movie format is supported and playback initialization
 *     succeeded, false otherwise.
 */
static int ffmpeg_init_decoder(SysMovieHandle *movie);

/**
 * ffmpeg_close_decoder:  Free playback resources used by the FFmpeg libraries.
 *
 * [Parameters]
 *     movie: Movie handle.
 */
static void ffmpeg_close_decoder(SysMovieHandle *movie);

/**
 * ffmpeg_read_video_frame:  Read and decode a video frame from the movie
 * stream, storing the image data in imagebuf (an RGBA buffer of size
 * movie->width * movie->height).
 *
 * [Parameters]
 *     movie: Movie handle.
 *     imagebuf: Buffer in which to store image.
 * [Return value]
 *     True if a frame was decoded, false if not.
 */
static int ffmpeg_read_video_frame(SysMovieHandle *movie, void *imagebuf);

/**
 * ffmpeg_read_audio_samples:  Read and decode audio samples from the movie
 * stream, updating the chunk_buf and chunk_size fields on success.  If
 * there are any audio samples remaining in the stream, at least one sample
 * will always be returned.
 *
 * [Parameters]
 *     movie: Movie handle.
 */
static void ffmpeg_read_audio_samples(SysMovieHandle *movie);

/**
 * ffmpeg_read_packet_v, ffmpeg_read_packet_a:  Fill the given buffer with
 * data from the video or audio stream.  Stream read callbacks for libavformat.
 *
 * [Parameters]
 *     opaque: Movie handle (SysMovieHandle *).
 *     buf: Buffer into which to read data.
 *     buf_size: Size of read buffer (maximum number of bytes to read).
 * [Return value]
 *     Number of bytes read.
 */
static int ffmpeg_read_packet_v(void *opaque, uint8_t *buf, int buf_size);
static int ffmpeg_read_packet_a(void *opaque, uint8_t *buf, int buf_size);

/**
 * ffmpeg_read_packet:  Common implementation for ffmpeg_read_packet_[va]().
 *
 * [Parameters]
 *     movie: Movie handle.
 *     offset_ptr: Pointer to file offset variable to use.
 *     buf: Buffer into which to read data.
 *     buf_size: Size of read buffer (maximum number of bytes to read).
 * [Return value]
 *     Number of bytes read.
 */
static int ffmpeg_read_packet(SysMovieHandle *movie, int64_t *offset_ptr,
                              uint8_t *buf, int buf_size);

/**
 * ffmpeg_seek_v, ffmpeg_seek_a:  Seek to the given byte position in the
 * video or audio stream.  Stream seek callbacks for libavformat.
 *
 * [Parameters]
 *     opaque: Movie handle (SysMovieHandle *).
 *     offset: Target offset, in bytes.
 *     whence: Seek method (stdio SEEK_*).
 * [Return value]
 *     New file position.
 */
static int64_t ffmpeg_seek_v(void *opaque, int64_t offset, int whence);
static int64_t ffmpeg_seek_a(void *opaque, int64_t offset, int whence);

/**
 * ffmpeg_seek:  Common implementation for ffmpeg_seek_[va]().
 *
 * [Parameters]
 *     opaque: Movie handle (SysMovieHandle *).
 *     offset_ptr: Pointer to file offset variable to use.
 *     offset: Target offset, in bytes.
 *     whence: Seek method (stdio SEEK_*).
 * [Return value]
 *     New file position.
 */
static int64_t ffmpeg_seek(SysMovieHandle *movie, int64_t *offset_ptr,
                           int64_t offset, int whence);

/**
 * ffmpeg_log:  Log callback for FFmpeg.
 *
 * [Parameters]
 *     ptr: Pointer to a struct whose first field is a pointer to an
 *         AVClass struct.
 *     level: Importance level (lower number = more important, see AV_LOG_*).
 *     fmt: Message format string.
 *     args: Message format arguments.
 */
static void ffmpeg_log(void *ptr, int level, const char *fmt, va_list args);

#endif  // SIL_PLATFORM_LINUX_USE_FFMPEG

/*----------------------------------*/

/**
 * movie_sound_open:  open() implementation for the movie sound decoder.
 *
 * [Return value]
 *     True on success, false on error.
 */
static int movie_sound_open(SoundDecodeHandle *this);

/**
 * movie_sound_get_pcm:  get_pcm() implementation for the movie sound decoder.
 *
 * [Parameters]
 *     pcm_buffer: Buffer into which to store PCM (signed 16-bit) data.
 *     pcm_len: Number of samples to retrieve.
 *     loop_offset_ret: Pointer to variable to receive the number of
 *         samples skipped backward due to looping.
 * [Return value]
 *     Number of samples stored in pcm_buffer.
 */
static int movie_sound_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret);

/**
 * movie_sound_close:  close() implementation for the movie sound decoder.
 */
static void movie_sound_close(SoundDecodeHandle *this);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

SysMovieHandle *sys_movie_open(SysFile *fh, int64_t offset, int length,
                               int smooth_uv)
{
    SysMovieHandle *movie;

    /* First allocate and initialize the movie handle structure. */
    movie = mem_alloc(sizeof(*movie), 0, MEM_ALLOC_TEMP | MEM_ALLOC_CLEAR);
    if (UNLIKELY(!movie)) {
        DLOG("alloc(SysMovieHandle) failed");
        goto error_return;
    }
    movie->fh = fh;
    movie->base_offset = offset;
    movie->end_offset = offset + length;
    movie->video = movie->audio = movie->base_offset;
    movie->volume = 1.0f;
    movie->smooth_uv = smooth_uv;

    /* Reserve a sound channel for audio output. */
    movie->sound_channel = sound_reserve_channel();
    if (!movie->sound_channel) {
        DLOG("sound_reserve_channel() failed");
        goto error_free_movie;
    }

    /* Parse the movie data to find the video codec and parameters. */
#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG
    if (ffmpeg_init_decoder(movie)) {
        movie->is_ffmpeg = 1;
    } else
#endif
    {
        DLOG("Movie format not supported");
        goto error_free_sound_channel;
    }

    /* Create a texture to hold the YUV->RGB colorspace conversion result. */
    movie->texture = texture_create(movie->width, movie->height, 0, 0);
    if (UNLIKELY(!movie->texture)) {
        DLOG("No memory for %dx%d texture", movie->width, movie->height);
        goto error_free_decoder;
    }

    /* All done. */
    DLOG("WARNING: decoding movie using deprecated FFmpeg support");
    return movie;

  error_free_decoder:
    sys_movie_close(movie);
    return NULL;

  error_free_sound_channel:
    sound_free_channel(movie->sound_channel);
  error_free_movie:
    mem_free(movie);
  error_return:
    sys_file_close(fh);
    return NULL;
}

/*-----------------------------------------------------------------------*/

void sys_movie_close(SysMovieHandle *movie)
{
    sys_movie_stop(movie);
    mem_free(movie->chunk_buf);
    texture_destroy(movie->texture);
#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG
    if (movie->is_ffmpeg) {
        ffmpeg_close_decoder(movie);
    }
#endif
    sound_free_channel(movie->sound_channel);
    sys_file_close(movie->fh);
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
    return movie->framerate;
}

/*-----------------------------------------------------------------------*/

void sys_movie_set_volume(SysMovieHandle *movie, float volume)
{
    movie->volume = volume;
    sound_adjust_volume(movie->sound_channel, volume, 0);
}

/*-----------------------------------------------------------------------*/

int sys_movie_play(SysMovieHandle *movie)
{
    SoundDecodeHandle *decoder =
        sound_decode_open_custom(movie_sound_open, movie, 1);
    if (UNLIKELY(!decoder)) {
        DLOG("Failed to create decoder");
        return 0;
    }

    /* This can't fail since we already allocated a sound channel. */
    ASSERT(sound_play_decoder(decoder, movie->sound_channel,
                              movie->volume, 0));

    movie->playing = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

void sys_movie_stop(SysMovieHandle *movie)
{
    sound_cut(movie->sound_channel);
    movie->playing = 0;
}

/*-----------------------------------------------------------------------*/

int sys_movie_get_texture(
    SysMovieHandle *movie,
    float *left_ret, float *right_ret, float *top_ret, float *bottom_ret)
{
    if (movie->texture) {
        *left_ret = 0;
        *right_ret = 1;
        *top_ret = 0;
        *bottom_ret = 1;
    }
    return movie->texture;
}

/*-----------------------------------------------------------------------*/

int sys_movie_draw_frame(SysMovieHandle *movie)
{
    /* The core code will never call us after the movie stops. */
    ASSERT(movie->playing, return 0);

    if (!get_frame(movie)) {
        movie->playing = 0;
        return 0;
    }

    return 1;
}

/*************************************************************************/
/************************* Common local routines *************************/
/*************************************************************************/

static int get_frame(SysMovieHandle *movie)
{
    PRECOND(movie != NULL, return 0);

    void *imagebuf = texture_lock_writeonly(movie->texture);
    if (UNLIKELY(!imagebuf)) {
        DLOG("Failed to lock video texture for write");
        return 0;
    }

    int got_picture = 0;
#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG
    if (movie->is_ffmpeg) {
        got_picture = ffmpeg_read_video_frame(movie, imagebuf);
    }
#endif

    texture_unlock(movie->texture);
    return got_picture;
}

/*************************************************************************/
/*************************** FFmpeg interface ****************************/
/*************************************************************************/

#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG

/*-----------------------------------------------------------------------*/

static int ffmpeg_init_decoder(SysMovieHandle *movie)
{
    /* Initialize the FFmpeg library. */
    av_register_all();
    avcodec_register_all();
    av_log_set_callback(ffmpeg_log);

    /* Set up an AVIOContext for reading from the file. */
    const int buffer_size = 4096;
    void *avio_buffer = av_malloc(buffer_size);
    if (!avio_buffer) {
        DLOG("No memory for video read buffer");
        goto error_return;
    }
    movie->v_reader = avio_alloc_context(
        avio_buffer, buffer_size, 0, movie,
        ffmpeg_read_packet_v, NULL, ffmpeg_seek_v);
    if (UNLIKELY(!movie->v_reader)) {
        DLOG("Failed to allocate video I/O context");
        av_free(avio_buffer);
        goto error_return;
    }

    /* Parse the movie data using libavformat. */
    movie->v_demuxer = avformat_alloc_context();
    if (UNLIKELY(!movie->v_demuxer)) {
        DLOG("Failed to allocate video demuxer context");
        goto error_free_v_reader;
    }
    movie->v_demuxer->pb = movie->v_reader;
    movie->v_demuxer->probesize = ubound(
        movie->v_demuxer->probesize, movie->end_offset - movie->base_offset);
    int error;
    if ((error = avformat_open_input(&movie->v_demuxer, NULL, NULL,
                                     NULL)) < 0) {
        DLOG("Failed to parse file (1): %d", error);
        /* avformat_open_input() frees a user-allocated AVFormatContext on
         * error, so don't try to free it twice. */
        goto error_free_v_reader;
    }
    if ((error = avformat_find_stream_info(movie->v_demuxer, NULL)) < 0) {
        DLOG("Failed to parse file (2): %d", error);
        goto error_free_v_demuxer;
    }

    /* Find the video stream and retrieve its parameters. */
    AVCodec *video_codec;
    movie->video_stream = av_find_best_stream(
        movie->v_demuxer, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if (movie->video_stream < 0) {
        if (movie->video_stream == AVERROR_STREAM_NOT_FOUND) {
            DLOG("No video stream found in file");
        } else if (movie->video_stream == AVERROR_DECODER_NOT_FOUND) {
            DLOG("No codec found for video stream");
        } else {
            DLOG("Failed to find video stream: %d", movie->video_stream);
        }
        goto error_free_v_demuxer;
    }
    AVStream *video_stream = movie->v_demuxer->streams[movie->video_stream];
    if (video_stream->
#if LIBAVCODEC_VERSION_MAJOR >= 57
                      codecpar->format
#else
                      codec->pix_fmt
#endif
                                       != AV_PIX_FMT_YUV420P) {
        DLOG("Unsupported pixel format: %d", video_stream->
#if LIBAVCODEC_VERSION_MAJOR >= 57
                                                           codecpar->format
#else
                                                           codec->pix_fmt
#endif
                                                                           );
        goto error_free_v_demuxer;
    }
    movie->framerate = av_q2d(video_stream->r_frame_rate);
    movie->width = video_stream->codecpar->width;
    movie->height = video_stream->codecpar->height;

    /* Find the audio stream and retrieve its parameters. */
    AVCodec *audio_codec;
    movie->audio_stream = av_find_best_stream(
        movie->v_demuxer, AVMEDIA_TYPE_AUDIO, -1, movie->video_stream,
        &audio_codec, 0);
    AVStream *audio_stream;
    if (movie->audio_stream < 0) {
        /* It's okay to not have an audio stream.  We still love you. */
        audio_stream = NULL;
        movie->a_conv = NULL;
    } else {
        audio_stream = movie->v_demuxer->streams[movie->audio_stream];
        movie->audio_rate = audio_stream->codecpar->sample_rate;
        if (audio_stream->codecpar->channels == 1) {
            movie->stereo = 0;
        } else if (audio_stream->codecpar->channels == 2) {
            movie->stereo = 1;
        } else {
            DLOG("Unsupported number of audio channels: %d",
                 audio_stream->codecpar->channels);
            goto error_free_v_demuxer;
        }
        if (audio_stream->
#if LIBAVCODEC_VERSION_MAJOR >= 57
                          codecpar->format
#else
                          codec->sample_fmt
#endif
                                           == AV_SAMPLE_FMT_S16) {
            movie->a_conv = NULL;
        } else {
            movie->a_conv = avresample_alloc_context();
            if (UNLIKELY(!movie->a_conv)) {
                DLOG("Failed to create audio conversion context");
                goto error_free_v_demuxer;
            }
            av_opt_set_int(
                movie->a_conv, "in_channel_layout",
                movie->stereo ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO, 0);
            av_opt_set_int(
                movie->a_conv, "in_sample_fmt",
                audio_stream->
#if LIBAVCODEC_VERSION_MAJOR >= 57
                              codecpar->format
#else
                              codec->sample_fmt
#endif
                                              , 0);
            av_opt_set_int(
                movie->a_conv, "out_channel_layout",
                movie->stereo ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO, 0);
            av_opt_set_int(
                movie->a_conv, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
            error = avresample_open(movie->a_conv);
            if (error < 0) {
                DLOG("Failed to initialize audio conversion context for"
                     " format %d: %d", audio_stream->
#if LIBAVCODEC_VERSION_MAJOR >= 57
                                                     codecpar->format
#else
                                                     codec->sample_fmt
#endif
                                                                     , error);
                goto error_free_a_conv;
            }
        }
    }

    /* Set up the FFmpeg decoder. */
    movie->v_decoder = avcodec_alloc_context3(video_codec);
    if (!movie->v_decoder) {
        DLOG("Failed to allocate video decoder context");
        goto error_free_a_conv;
    }
    movie->v_decoder->flags = 0;
    movie->v_decoder->flags2 = 0;
    movie->v_decoder->extradata = video_stream->codecpar->extradata;
    movie->v_decoder->extradata_size = video_stream->codecpar->extradata_size;
    movie->v_decoder->draw_horiz_band = NULL;
    movie->v_decoder->slice_count = 0;
    movie->v_decoder->slice_offset = NULL;
    movie->v_decoder->slice_flags = 0;
    movie->v_decoder->skip_top = 0;
    movie->v_decoder->skip_bottom = 0;
    movie->v_decoder->workaround_bugs = FF_BUG_AUTODETECT;
    movie->v_decoder->strict_std_compliance = FF_COMPLIANCE_NORMAL;
    movie->v_decoder->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
    movie->v_decoder->debug = 0;
    movie->v_decoder->debug_mv = 0;
    movie->v_decoder->err_recognition = 0;
    movie->v_decoder->hwaccel_context = NULL;
    movie->v_decoder->bits_per_coded_sample =
        video_stream->codecpar->bits_per_coded_sample;
    movie->v_decoder->lowres = 0;
    movie->v_decoder->thread_count = 0;
    movie->v_decoder->thread_type = FF_THREAD_SLICE;
    movie->v_decoder->skip_loop_filter = AVDISCARD_DEFAULT;
    movie->v_decoder->skip_idct = AVDISCARD_DEFAULT;
    movie->v_decoder->skip_frame = AVDISCARD_DEFAULT;
    if (avcodec_open2(movie->v_decoder, video_codec, NULL) < 0) {
        DLOG("Failed to initialize video decoder");
        goto error_free_v_decoder;
    }

    /* If there's an audio stream, create a separate demuxer for it so the
     * av_read_frame() calls don't interfere with each other. */
    movie->a_reader = NULL;
    movie->a_demuxer = NULL;
    movie->a_decoder = NULL;
    if (movie->audio_stream >= 0) {
        avio_buffer = av_malloc(buffer_size);
        if (!avio_buffer) {
            DLOG("No memory for audio read buffer");
            goto error_free_v_decoder;
        }
        movie->a_reader = avio_alloc_context(
            avio_buffer, buffer_size, 0, movie,
            ffmpeg_read_packet_a, NULL, ffmpeg_seek_a);
        if (UNLIKELY(!movie->a_reader)) {
            DLOG("Failed to allocate audio I/O context");
            av_free(avio_buffer);
            goto error_free_v_decoder;
        }
        movie->a_demuxer = avformat_alloc_context();
        if (UNLIKELY(!movie->a_demuxer)) {
            DLOG("Failed to allocate audio demuxer context");
            goto error_free_a_reader;
        }
        movie->a_demuxer->pb = movie->a_reader;
        if ((error = avformat_open_input(&movie->a_demuxer, NULL, NULL,
                                         NULL)) < 0) {
            DLOG("Failed to parse file for audio demuxer: %d", error);
            goto error_free_a_reader;
        }
        movie->a_decoder = avcodec_alloc_context3(audio_codec);
        if (!movie->a_decoder) {
            DLOG("Failed to allocate audio decoder context");
            goto error_free_a_demuxer;
        }
        movie->a_decoder->flags = 0;
        movie->a_decoder->flags2 = 0;
        movie->a_decoder->extradata = audio_stream->codecpar->extradata;
        movie->a_decoder->extradata_size =
            audio_stream->codecpar->extradata_size;
        movie->a_decoder->channels = movie->stereo ? 2 : 1;
        movie->a_decoder->workaround_bugs = FF_BUG_AUTODETECT;
        movie->a_decoder->strict_std_compliance = FF_COMPLIANCE_NORMAL;
        movie->a_decoder->error_concealment = 0;
        movie->a_decoder->debug = 0;
        movie->a_decoder->err_recognition = 0;
        movie->a_decoder->hwaccel_context = NULL;
        movie->a_decoder->idct_algo = FF_IDCT_AUTO;
        if (avcodec_open2(movie->a_decoder, audio_codec, NULL) < 0) {
            DLOG("Failed to initialize audio decoder");
            goto error_free_a_decoder;
        }
    }

    return 1;

  error_free_a_decoder:
    if (movie->a_decoder) {
        avcodec_close(movie->a_decoder);
        av_free(movie->a_decoder);
    }
  error_free_a_demuxer:
    if (movie->a_demuxer) {
        avformat_close_input(&movie->a_demuxer);
    }
  error_free_a_reader:
    av_free(movie->a_reader->buffer);
    av_free(movie->a_reader);
  error_free_v_decoder:
    avcodec_close(movie->v_decoder);
    av_free(movie->v_decoder);
  error_free_a_conv:
    if (movie->a_conv) {
        avresample_free(&movie->a_conv);
    }
  error_free_v_demuxer:
    avformat_close_input(&movie->v_demuxer);
  error_free_v_reader:
    av_free(movie->v_reader->buffer);
    av_free(movie->v_reader);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

static void ffmpeg_close_decoder(SysMovieHandle *movie)
{
    avcodec_close(movie->v_decoder);
    av_free(movie->v_decoder);
    avformat_close_input(&movie->v_demuxer);
    av_free(movie->v_reader->buffer);
    av_free(movie->v_reader);
    if (movie->a_decoder) {
        avcodec_close(movie->a_decoder);
        av_free(movie->a_decoder);
        avformat_close_input(&movie->a_demuxer);
        av_free(movie->a_reader->buffer);
        av_free(movie->a_reader);
    }
    if (movie->a_conv) {
        avresample_free(&movie->a_conv);
    }
}

/*-----------------------------------------------------------------------*/

static int ffmpeg_read_video_frame(SysMovieHandle *movie, void *imagebuf)
{
    int retval = 0;

    AVFrame *frame = avcodec_alloc_frame();
    int got_frame = 0;
    do {
        AVPacket packet;
        mem_clear(&packet, sizeof(packet));
        for (;;) {
            if (av_read_frame(movie->v_demuxer, &packet) < 0) {
                goto out;
            }
            if (packet.stream_index == movie->video_stream) {
                break;
            }
            av_packet_unref(&packet);
        }
AV_NOWARN_DEPRECATED(
        const int res = avcodec_decode_video2(movie->v_decoder, frame,
                                              &got_frame, &packet);
)
        av_packet_unref(&packet);
        if (res < 0) {
            DLOG("avcodec_decode_video() failed: %d", res);
            goto out;
        }
    } while (!got_frame);

    yuv2rgb((const uint8_t **)frame->data, frame->linesize,
            imagebuf, movie->width, movie->width, movie->height,
            movie->smooth_uv);
    retval = 1;

  out:
    avcodec_free_frame(&frame);
    return retval;
}

/*-----------------------------------------------------------------------*/

static void ffmpeg_read_audio_samples(SysMovieHandle *movie)
{
    if (!movie->a_demuxer) {
        return;  // No audio.
    }

    AVFrame *frame = avcodec_alloc_frame();
    int got_frame = 0;
    do {
        AVPacket packet;
        mem_clear(&packet, sizeof(packet));
        for (;;) {
            if (av_read_frame(movie->a_demuxer, &packet) < 0) {
                goto out;
            }
            if (packet.stream_index == movie->audio_stream) {
                break;
            }
            av_packet_unref(&packet);
        }
AV_NOWARN_DEPRECATED(
        const int res = avcodec_decode_audio4(movie->a_decoder, frame,
                                              &got_frame, &packet);
)
        av_packet_unref(&packet);
        if (res < 0) {
            DLOG("avcodec_decode_audio() failed: %d", res);
            goto out;
        }
    } while (!got_frame);

    movie->chunk_buf = mem_realloc(
        movie->chunk_buf, frame->nb_samples * (movie->stereo ? 4 : 2),
        MEM_ALLOC_TEMP);
    if (movie->a_conv) {
        movie->chunk_size = avresample_convert(
            movie->a_conv, &movie->chunk_buf, 0, frame->nb_samples,
            frame->data, frame->linesize[0], frame->nb_samples);
    } else {
        memcpy(movie->chunk_buf, frame->data[0],
               frame->nb_samples * (movie->stereo ? 4 : 2));
        movie->chunk_size = frame->nb_samples;
    }

  out:
    avcodec_free_frame(&frame);
}

/*-----------------------------------------------------------------------*/

static int ffmpeg_read_packet_v(void *opaque, uint8_t *buf, int buf_size)
{
    SysMovieHandle *movie = (SysMovieHandle *)opaque;
    return ffmpeg_read_packet(movie, &movie->video, buf, buf_size);
}

static int ffmpeg_read_packet_a(void *opaque, uint8_t *buf, int buf_size)
{
    SysMovieHandle *movie = (SysMovieHandle *)opaque;
    return ffmpeg_read_packet(movie, &movie->audio, buf, buf_size);
}

static int ffmpeg_read_packet(SysMovieHandle *movie, int64_t *offset_ptr,
                              uint8_t *buf, int buf_size)
{
    sys_file_seek(movie->fh, *offset_ptr, FILE_SEEK_SET);
    const int to_read = ubound(buf_size, movie->end_offset - *offset_ptr);
    const int nread = sys_file_read(movie->fh, buf, to_read);
    if (UNLIKELY(nread < to_read)) {
        DLOG("Read error: %s", sys_last_errstr());
    }
    *offset_ptr += nread;
    return nread;
}

/*-----------------------------------------------------------------------*/

static int64_t ffmpeg_seek_v(void *opaque, int64_t offset, int whence)
{
    SysMovieHandle *movie = (SysMovieHandle *)opaque;
    return ffmpeg_seek(movie, &movie->video, offset, whence);
}

static int64_t ffmpeg_seek_a(void *opaque, int64_t offset, int whence)
{
    SysMovieHandle *movie = (SysMovieHandle *)opaque;
    return ffmpeg_seek(movie, &movie->audio, offset, whence);
}

static int64_t ffmpeg_seek(SysMovieHandle *movie, int64_t *offset_ptr,
                           int64_t offset, int whence)
{
    switch (whence) {
        case 0: *offset_ptr = movie->base_offset + offset; break;
        case 1: *offset_ptr = *offset_ptr        + offset; break;
        case 2: *offset_ptr = movie->end_offset  + offset; break;
        case AVSEEK_SIZE: return movie->end_offset - movie->base_offset;
        default: DLOG("Unknown seek type %d", whence); return -1;
    }
    if (*offset_ptr < movie->base_offset) {
        *offset_ptr = movie->base_offset;
    } else if (*offset_ptr > movie->end_offset) {
        *offset_ptr = movie->end_offset;
    }
    return *offset_ptr - movie->base_offset;
}

/*-----------------------------------------------------------------------*/

static void ffmpeg_log(DEBUG_USED void *ptr, DEBUG_USED int level,
                       DEBUG_USED const char *fmt, DEBUG_USED va_list args)
{
#ifdef DEBUG
    char buf[1000];

    const char *level_str;
    if (level >= AV_LOG_VERBOSE) {
        return;  // Ignore.
    } else if (level >= AV_LOG_INFO) {
        level_str = "info";
    } else if (level >= AV_LOG_WARNING) {
        level_str = "warning";
    } else if (level >= AV_LOG_ERROR) {
        level_str = "error";
    } else if (level >= AV_LOG_FATAL) {
        level_str = "fatal";
    } else {
        level_str = "PANIC";
    }
    strformat(buf, sizeof(buf), "ffmpeg %s: ", level_str);

    AVClass *class = ptr ? *(AVClass **)ptr : NULL;
    if (class) {
        if (class->parent_log_context_offset) {
            /* Cast through void * to avoid a cast-align warning. */
            AVClass **parent = *(AVClass ***)(void *)(
                (char *)ptr + class->parent_log_context_offset);
            if (parent && *parent) {
                strformat(buf + strlen(buf), sizeof(buf) - strlen(buf),
                          "[%s @ %p] ", (*parent)->item_name(parent), parent);
            }
        }
        strformat(buf + strlen(buf), sizeof(buf) - strlen(buf),
                  "[%s @ %p] ", class->item_name(ptr), ptr);
    }

    strformat(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s", fmt);
    char *s = buf + strlen(buf);
    while (s > buf && isspace(s[-1])) {
        *--s = 0;
    }

    vdo_DLOG(NULL, 0, NULL, buf, args);
#endif
}

/*-----------------------------------------------------------------------*/

#endif  // SIL_PLATFORM_LINUX_USE_FFMPEG

/*************************************************************************/
/************************* Other local routines **************************/
/*************************************************************************/

static int movie_sound_open(SoundDecodeHandle *this)
{
    SysMovieHandle *movie = (SysMovieHandle *)this->custom_data;
    PRECOND(movie != NULL, return 0);

    this->get_pcm     = movie_sound_get_pcm;
    this->close       = movie_sound_close;
    this->stereo      = movie->stereo;
    this->native_freq = movie->audio_rate;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int movie_sound_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret)
{
    SysMovieHandle *movie = (SysMovieHandle *)this->custom_data;
    PRECOND(movie != NULL, return 0);

    *loop_offset_ret = 0;  // We don't loop.

    int pos = 0;
    while (pos < pcm_len) {
        if (movie->chunk_pos < 0) {
            int toclear = pcm_len - pos;
            if (toclear > -movie->chunk_pos) {
                toclear = -movie->chunk_pos;
            }
            mem_clear(&pcm_buffer[pos*2], toclear * (movie->stereo ? 4 : 2));
            pos += toclear;
            movie->chunk_pos += toclear;
        }
        int tocopy = pcm_len - pos;
        if (tocopy > movie->chunk_size - movie->chunk_pos) {
            tocopy = lbound(movie->chunk_size - movie->chunk_pos, 0);
        }
        int i;
        for (i = 0; i < tocopy; i++, movie->chunk_pos++, pos++) {
            if (movie->stereo) {
                pcm_buffer[pos*2+0] = movie->chunk_buf[movie->chunk_pos*4+0]
                                    | movie->chunk_buf[movie->chunk_pos*4+1]<<8;
                pcm_buffer[pos*2+1] = movie->chunk_buf[movie->chunk_pos*4+2]
                                    | movie->chunk_buf[movie->chunk_pos*4+3]<<8;
            } else {
                pcm_buffer[pos] = movie->chunk_buf[movie->chunk_pos*2+0]
                                | movie->chunk_buf[movie->chunk_pos*2+1]<<8;
            }
        }
        if (movie->chunk_pos >= movie->chunk_size) {
            /* Do proper subtraction instead of just setting to 0 so we
             * preserve the number of samples to skip. */
            movie->chunk_pos -= movie->chunk_size;
            mem_free(movie->chunk_buf);
            movie->chunk_buf = NULL;
            movie->chunk_size = 0;
#ifdef SIL_PLATFORM_LINUX_USE_FFMPEG
            if (movie->is_ffmpeg) {
                ffmpeg_read_audio_samples(movie);
            }
#endif
            if (!movie->chunk_size) {
                break;  // No more data.
            }
        }
    }

    return pos;
}

/*-----------------------------------------------------------------------*/

static void movie_sound_close(UNUSED SoundDecodeHandle *this)
{
    /* Nothing to do. */
}

/*************************************************************************/
/*************************************************************************/
