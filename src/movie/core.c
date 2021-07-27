/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/movie/core.c: Core movie playback routines.
 */

#include "src/base.h"
#include "src/graphics.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/movie.h"
#include "src/movie/webm.h"
#include "src/resource.h"
#include "src/sound.h"
#include "src/sound/decode.h"
#include "src/sysdep.h"
#include "src/texture.h"
#include "src/time.h"
#include "src/utility/id-array.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Movie handle structure. */
typedef struct MovieHandle MovieHandle;
struct MovieHandle {
    /* Exactly one of these pointers will be non-NULL. */
    WebMDecodeHandle *webm_handle;
    SysMovieHandle *sys_handle;

    uint8_t playing;    // Is the movie currently playing?
    int current_frame;  // Currently rendered frame number (0 = first frame).
    double start_time;  // time_now() timestamp for the first frame.
    double framerate;   // Video frame rate (cached for convenience).
    int texture;        // Texture for video image, 0 for direct render.
    /* Texture coordinate bounds of the image.  Undefined if texture == 0. */
    float tex_left, tex_right, tex_top, tex_bottom;

    /* The remainder of these fields are only used if sys_handle is NULL. */

    int width, height;  // Video frame size (cached for convenience).
    int channels, audio_rate; // Audio parameters (cached for convenience).
    float volume;       // Current volume.
    int sound_channel;  // Sound channel for output.
};

/*-----------------------------------------------------------------------*/

/* Array of allocated movie IDs. */
static IDArray movies = ID_ARRAY_INITIALIZER(10);

/**
 * VALIDATE_MOVIE:  Validate the movie ID passed to a movie playback
 * routine, and store the corresponding pointer in the variable "movie".
 * If the movie ID is invalid, the "error_return" statement is executed;
 * this may consist of multiple statements, but must include a "return" to
 * exit the function.
 */
#define VALIDATE_MOVIE(id,movie,error_return) \
    ID_ARRAY_VALIDATE(&movies, (id), MovieHandle *, movie, \
                      DLOG("Movie ID %d is invalid", _id); error_return)

/*-----------------------------------------------------------------------*/

/* Sound decoder method declarations. */

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

int movie_open(const char *path, int add_extension, int smooth_chroma)
{
    if (!path) {
        DLOG("path == NULL");
        return 0;
    }

    char *new_path = NULL;
    if (add_extension) {
        const char *extension;
#if defined(SIL_MOVIE_INCLUDE_WEBM)
        extension = ".webm";
#elif defined(SIL_PLATFORM_PSP)
        extension = ".str";
#else
        extension = ".webm";
#endif
        const int new_size = (int)strlen(path) + (int)strlen(extension) + 1;
        new_path = mem_alloc(new_size, 1, MEM_ALLOC_TEMP);
        if (UNLIKELY(!new_path)) {
            DLOG("No memory to add extension to path");
            goto error_return;
        }
        ASSERT(strformat_check(new_path, new_size, "%s%s", path, extension),
               return 0);
        path = new_path;
    }

    MovieHandle *movie = mem_alloc(sizeof(*movie), 0, 0);
    if (!movie) {
        DLOG("No memory for movie handle");
        goto error_free_new_path;
    }

    const int id = id_array_register(&movies, movie);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store movie %p in array", movie);
        goto error_free_movie;
    }

    int64_t offset;
    int size;
    SysFile *fh = resource_internal_open_file(path, &offset, &size);
    if (!fh) {
        DLOG("Failed to open movie file %s", path);
        goto error_free_id;
    }

    movie->webm_handle = movie_webm_open(fh, offset, size, smooth_chroma);
    if (movie->webm_handle) {
        movie->framerate = movie_webm_framerate(movie->webm_handle);
        movie->width = movie_webm_width(movie->webm_handle);
        movie->height = movie_webm_height(movie->webm_handle);
        movie->channels = movie_webm_audio_channels(movie->webm_handle);
        movie->audio_rate = movie_webm_audio_rate(movie->webm_handle);
        movie->volume = 1.0f;
        movie->sound_channel = sound_reserve_channel();
        if (!movie->sound_channel) {
            DLOG("sound_reserve_channel() failed");
            goto error_close_webm_handle;
        }
        movie->texture = texture_create(movie->width, movie->height,
                                        MEM_ALLOC_CLEAR, 0);
        if (!movie->texture) {
            DLOG("Failed to create %dx%d texture for video",
                 movie->width, movie->height);
            goto error_free_sound_channel;
        }
        movie->tex_left = 0;
        movie->tex_right = 1;
        movie->tex_top = 0;
        movie->tex_bottom = 1;
    } else {
        movie->sys_handle = sys_movie_open(fh, offset, size, smooth_chroma);
        if (!movie->sys_handle) {
            DLOG("Failed to prepare movie %s for playback", path);
            goto error_free_id;  // The file will have already been closed.
        }
        movie->framerate = sys_movie_framerate(movie->sys_handle);
        movie->width = sys_movie_width(movie->sys_handle);
        movie->height = sys_movie_height(movie->sys_handle);
        movie->texture = sys_movie_get_texture(
            movie->sys_handle, &movie->tex_left, &movie->tex_right,
            &movie->tex_top, &movie->tex_bottom);
    }

    movie->playing = 0;
    movie->current_frame = -1;
    mem_free(new_path);
    return id;

  error_free_sound_channel:
    sound_free_channel(movie->sound_channel);
  error_close_webm_handle:
    movie_webm_close(movie->webm_handle);
  error_free_id:
    id_array_release(&movies, id);
  error_free_movie:
    mem_free(movie);
  error_free_new_path:
    mem_free(new_path);
  error_return:
    return 0;
}

/*-----------------------------------------------------------------------*/

void movie_close(int movie_id)
{
    if (movie_id) {
        MovieHandle *movie;
        VALIDATE_MOVIE(movie_id, movie, return);

        if (movie->playing) {
            movie_stop(movie_id);
        }
        if (movie->webm_handle) {
            texture_destroy(movie->texture);
            sound_free_channel(movie->sound_channel);
            movie_webm_close(movie->webm_handle);
        } else {
            sys_movie_close(movie->sys_handle);
        }
        mem_free(movie);
        id_array_release(&movies, movie_id);
    }
}

/*-----------------------------------------------------------------------*/

double movie_framerate(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return 0);

    if (movie->webm_handle) {
        return movie_webm_framerate(movie->webm_handle);
    } else {
        return sys_movie_framerate(movie->sys_handle);
    }
}

/*-----------------------------------------------------------------------*/

void movie_set_volume(int movie_id, float volume)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return);
    if (UNLIKELY(volume < 0)) {
        DLOG("Invalid volume: %g", volume);
        return;
    }

    if (movie->webm_handle) {
        movie->volume = volume;
        sound_adjust_volume(movie->sound_channel, volume, 0);
    } else {
        sys_movie_set_volume(movie->sys_handle, volume);
    }
}

/*-----------------------------------------------------------------------*/

int movie_play(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return 0);

    if (movie->playing) {
        return 1;
    }

    if (movie->webm_handle) {
        SoundDecodeHandle *decoder =
            sound_decode_open_custom(movie_sound_open, movie, 1);
        if (UNLIKELY(!decoder)) {
            DLOG("Failed to create decoder");
            return 0;
        }
        if (UNLIKELY(!sound_play_decoder(decoder, movie->sound_channel,
                                         movie->volume, 0))) {
            DLOG("Failed to start sound");
            sound_decode_close(decoder);
            return 0;
        }
    } else {
        if (!sys_movie_play(movie->sys_handle)) {
            return 0;
        }
    }

    if (movie->framerate) {
        movie->start_time = (time_now()
                             - ((movie->current_frame+1) / movie->framerate)
                             - graphics_frame_period() / 2);
    }
    movie->playing = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

void movie_stop(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return);

    if (!movie->playing) {
        return;
    }

    if (movie->webm_handle) {
        sound_cut(movie->sound_channel);
    } else {
        sys_movie_stop(movie->sys_handle);
    }
    movie->playing = 0;
}

/*-----------------------------------------------------------------------*/

int movie_is_playing(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return 0);
    return movie->playing;
}

/*-----------------------------------------------------------------------*/

int movie_get_texture(
    int movie_id,
    float *left_ret, float *right_ret, float *top_ret, float *bottom_ret)
{
    if (UNLIKELY(!left_ret) || UNLIKELY(!right_ret)
     || UNLIKELY(!top_ret) || UNLIKELY(!bottom_ret)) {
        DLOG("Invalid parameters: %d %p %p %p %p",
             movie_id, left_ret, right_ret, top_ret, bottom_ret);
        return 0;
    }
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return 0);

    *left_ret   = movie->tex_left;
    *right_ret  = movie->tex_right;
    *top_ret    = movie->tex_top;
    *bottom_ret = movie->tex_bottom;
    return movie->texture;
}

/*-----------------------------------------------------------------------*/

int movie_next_frame(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return 0);

    if (!movie->playing) {
        return 0;
    }

    if (movie->webm_handle) {
        void *imagebuf = texture_lock_writeonly(movie->texture);
        if (!imagebuf) {
            DLOG("Failed to lock video texture for update");
            movie_stop(movie_id);
            return 0;
        }
        const int got_frame =
            movie_webm_get_video(movie->webm_handle, imagebuf);
        texture_unlock(movie->texture);
        if (!got_frame) {
            movie_stop(movie_id);
            return 0;
        }
    } else {
        if (!sys_movie_draw_frame(movie->sys_handle)) {
            movie_stop(movie_id);
            return 0;
        }
        /* Update the texture coordinates in case they changed.  Note that
         * the implementation isn't allowed to change the texture ID. */
        ASSERT(sys_movie_get_texture(
                   movie->sys_handle, &movie->tex_left, &movie->tex_right,
                   &movie->tex_top, &movie->tex_bottom)
               == movie->texture);
    }

    movie->current_frame++;
    return 1;
}

/*-----------------------------------------------------------------------*/

int movie_update(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return 0);

    if (!movie->playing) {
        return 0;
    }

    if (!movie->framerate) {
        return movie_next_frame(movie_id);
    }

    const double rel_time = time_now() - movie->start_time;
    const int target_frame = ifloorf(rel_time * movie->framerate);
    while (movie->current_frame < target_frame) {
        if (!movie_next_frame(movie_id)) {
            return 0;
        }
    }
    return 1;
}

/*-----------------------------------------------------------------------*/

void movie_draw(int movie_id)
{
    MovieHandle *movie;
    VALIDATE_MOVIE(movie_id, movie, return);

    graphics_enable_alpha_test(0);
    graphics_set_blend(GRAPHICS_BLEND_ADD,
                       GRAPHICS_BLEND_ONE, GRAPHICS_BLEND_ZERO);
    graphics_enable_depth_test(0);
    graphics_enable_depth_write(0);
    graphics_set_face_cull(GRAPHICS_FACE_CULL_NONE);
    graphics_set_fixed_color(&(Vector4f){1, 1, 1, 1});
    graphics_enable_fog(0);
    texture_set_repeat(movie->texture, 0, 0);
    texture_apply(0, movie->texture);

    graphics_begin_primitive(GRAPHICS_PRIMITIVE_QUADS);
    graphics_add_vertex(
        &(Vector3f){0, 0, 0},
        &(Vector2f){movie->tex_left, movie->tex_top}, NULL);
    graphics_add_vertex(
        &(Vector3f){movie->width, 0, 0},
        &(Vector2f){movie->tex_right, movie->tex_top}, NULL);
    graphics_add_vertex(
        &(Vector3f){movie->width, movie->height, 0},
        &(Vector2f){movie->tex_right, movie->tex_bottom}, NULL);
    graphics_add_vertex(
        &(Vector3f){0, movie->height, 0},
        &(Vector2f){movie->tex_left, movie->tex_bottom}, NULL);
    graphics_end_and_draw_primitive();
    texture_apply(0, 0);
}

/*************************************************************************/
/********************** Internal interface routines **********************/
/*************************************************************************/

int movie_import(SysMovieHandle *sysmovie)
{
    PRECOND(sysmovie != NULL, return 0);

    MovieHandle *movie = mem_alloc(sizeof(*movie), 0, 0);
    if (!movie) {
        DLOG("No memory for movie handle");
        return 0;
    }
    movie->webm_handle = NULL;
    movie->sys_handle = sysmovie;
    movie->framerate = sys_movie_framerate(movie->sys_handle);
    movie->width = sys_movie_width(movie->sys_handle);
    movie->height = sys_movie_height(movie->sys_handle);
    movie->texture = sys_movie_get_texture(
            movie->sys_handle, &movie->tex_left, &movie->tex_right,
            &movie->tex_top, &movie->tex_bottom);
    movie->playing = 0;
    movie->current_frame = -1;

    const int id = id_array_register(&movies, movie);
    if (UNLIKELY(!id)) {
        DLOG("Failed to store movie %p in array", movie);
        mem_free(movie);
        return 0;
    }

    return id;
}

/*************************************************************************/
/********************* Sound decoder implementation **********************/
/*************************************************************************/

static int movie_sound_open(SoundDecodeHandle *this)
{
    MovieHandle *movie = (MovieHandle *)this->custom_data;
    PRECOND(movie != NULL, return 0);

    this->get_pcm     = movie_sound_get_pcm;
    this->close       = movie_sound_close;
    this->stereo      = (movie->channels == 2);
    this->native_freq = movie->audio_rate;
    return 1;
}

/*-----------------------------------------------------------------------*/

static int movie_sound_get_pcm(
    SoundDecodeHandle *this, int16_t *pcm_buffer, int pcm_len,
    int *loop_offset_ret)
{
    MovieHandle *movie = (MovieHandle *)this->custom_data;
    PRECOND(movie != NULL, return 0);

    *loop_offset_ret = 0;  // We don't loop.
    return movie_webm_get_audio(movie->webm_handle, pcm_buffer, pcm_len);
}

/*-----------------------------------------------------------------------*/

static void movie_sound_close(UNUSED SoundDecodeHandle *this)
{
    /* Nothing to do. */
}

/*************************************************************************/
/*************************************************************************/
