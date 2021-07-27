/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/psp/common.h: Global declarations specific to the PSP.
 */

#ifndef SIL_SYSDEP_PSP_COMMON_H
#define SIL_SYSDEP_PSP_COMMON_H

EXTERN_C_BEGIN

struct SysTexture;

/*************************************************************************/
/******************** PSP-specific global definitions ********************/
/*************************************************************************/

/* Fix incorrect definitions of SIZE_MIN and SIZE_MAX in the stdint.h
 * header from the unofficial PSP SDK. */

#undef SIZE_MIN
#undef SIZE_MAX
#define SIZE_MIN  0
#define SIZE_MAX  0xFFFFFFFFU

/*-----------------------------------------------------------------------*/

/* Icon size used for save files.  (The image data passed to
 * userdata_save_savefile() must be of exactly this size.) */

#define PSP_SAVE_IMAGE_WIDTH  144
#define PSP_SAVE_IMAGE_HEIGHT  80

/*-----------------------------------------------------------------------*/

/* Define malloc() and operator new/delete replacements which use our
 * custom allocation functions (with debugging support, if appropriate). */

#ifndef SIL_MEMORY_FORBID_MALLOC

extern void *malloc(size_t size);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);
extern void free(void *ptr);

#ifdef DEBUG

extern void *debug_malloc(size_t size, const char *file, int line);
extern void *debug_calloc(size_t nmemb, size_t size, const char *file, int line);
extern void *debug_realloc(void *ptr, size_t size, const char *file, int line);
extern void debug_free(void *ptr, const char *file, int line);
/* Redefine malloc(), etc. to point to the debug functions. */
# define malloc(size)        debug_malloc(size, __FILE__, __LINE__)
# define calloc(nmemb,size)  debug_calloc(nmemb, size, __FILE__, __LINE__)
# define realloc(ptr,size)   debug_realloc(ptr, size, __FILE__, __LINE__)
# define free(ptr)           debug_free(ptr, __FILE__, __LINE__)

#ifdef __cplusplus
EXTERN_C_END

/* First include some system headers that use weird new() syntax, at least
 * in GCC. */
#include <algorithm>
#include <string>
#include <vector>

/* Define the debugging versions of the operators. */
inline void *operator new(size_t size, const char *file, int line) {
    return debug_malloc(size, file, line);
}
inline void *operator new[](size_t size, const char *file, int line) {
    return debug_malloc(size, file, line);
}
inline void operator delete(void *ptr, const char *file, int line) {
    return debug_free(ptr, file, line);
}
inline void operator delete[](void *ptr, const char *file, int line) {
    return debug_free(ptr, file, line);
}

/* Define a macro that converts ordinary "new" into a call to the debugging
 * version.  For some reason, this does _not_ work with "delete". */
#define new new(__FILE__,__LINE__)
//#define delete delete(__FILE__,__LINE__)

EXTERN_C_BEGIN
#endif  // __cplusplus

#endif  // DEBUG

#endif  // !SIL_MEMORY_FORBID_MALLOC

/*-----------------------------------------------------------------------*/

/* Force atof() to single precision. */
#undef atof
#define atof(s)  strtof((s), NULL)

/*************************************************************************/
/************************ PSP-specific functions *************************/
/*************************************************************************/

/**
 * psp_movie_open_direct:  Open the given movie file and prepare it for
 * playback via direct rendering to the display.  The movie will be
 * drawn unscaled and centered within the display.
 *
 * If the direct_audio flag is true, movie audio will be played by sending
 * it directly to the hardware.  This can reduce CPU load, but it also
 * risks losing synchronization with any other audio being played at the
 * same time.  Setting direct_audio to false will use the software mixer
 * for movie audio playback.
 *
 * The returned movie ID can be used with most movie_*() functions, with
 * the following exceptions:
 *
 * - The behavior of calling movie_get_texture() is undefined.
 *
 * - movie_next_frame() and movie_update() (if it advances to the next
 *   frame) will draw directly to the display; the behavior of calling
 *   movie_draw() is undefined.  Note that if movie_update() does _not_
 *   advance to the next frame, it will not redraw the current frame, so
 *   the caller should take care not to clear the display between frames.
 *
 * [Parameters]
 *     path: Pathname (or resource name) of movie file to open.
 *     direct_audio: True to output movie audio directly to the hardware;
 *         false to send movie audio through the software mixer.
 * [Return value]
 *     Movie ID ( on success, false on error.
 */
extern int psp_movie_open_direct(const char *path, int direct_audio);

/**
 * psp_texture_get_pixel_data:  Return a pointer to the pixel data for a
 * texture.  If the texture is mipmapped, this returns a pointer to the
 * base image.
 *
 * The caller may safely write through the returned pointer as long as it
 * takes care to remain within the bounds of the pixel buffer.
 *
 * [Parameters]
 *     texture: Texture to operate on (the SysTexture pointer for a texture
 *         ID can be retrieved with texture_lock_raw()).
 * [Return value]
 *     Pointer to texture's pixel data.
 */
void *psp_texture_get_pixel_data(struct SysTexture *texture);

/**
 * psp_texture_get_palette:  Return a pointer to the palette data for an
 * 8-bit-per-pixel texture.  The data pointed to by the return value must
 * not be modified.  NULL is returned for non-8-bit-per-pixel textures.
 *
 * [Parameters]
 *     texture: Texture to operate on (the SysTexture pointer for a texture
 *         ID can be retrieved with texture_lock_raw()).
 * [Return value]
 *     Pointer to palette data (in RGBA8888 format), or NULL if the texture
 *     does not have a palette.
 */
const void *psp_texture_get_palette(struct SysTexture *texture);

/**
 * psp_texture_set_palette:  Set the palette for an 8-bit-per-pixel texture
 * (alpha, luminance, and paletted textures are all valid).  Behavior is
 * undefined if the texture is not an 8-bit-per-pixel texture.
 *
 * Palette data is not copied; if palette is not NULL, the data it points
 * to must be 64-byte aligned and must remain valid until the texture is
 * destroyed or the palette is changed again.
 *
 * [Parameters]
 *     texture: Texture to modify (the SysTexture pointer for a texture ID
 *         can be retrieved with texture_lock_raw()).
 *     palette: Color palette to apply (in RGBA8888 format), or NULL to
 *         restore the texture's original palette.
 */
extern void psp_texture_set_palette(struct SysTexture *texture,
                                    const void *palette);

/**
 * psp_userdata_set_low_priority_mode:  Enable or disable the use of low
 * thread priorities for the PSP save data utility.  Enabling low-priority
 * mode will give smoother performance, but client code must take care not
 * to monopolize the CPU while the save data utility is running.
 *
 * [Parameters]
 *     on: True to enable low-priority mode, false to disable.
 */
extern void psp_userdata_set_low_priority_mode(int on);

/**
 * psp_userdata_set_stats_file_info:  Set the title and descriptive text
 * to be stored with the PSP save data file containing statistics
 * information.
 *
 * [Parameters]
 *     title: Title text to associate with the file.
 *     desc: Descriptive text to associate with the file.
 */
extern void psp_userdata_set_stats_file_info(const char *title, const char *desc);

/**
 * psp_vram_alloc:  Allocate a block of video EDRAM from the spare pool.
 *
 * [Parameters]
 *     size: Size of memory to allocate, in bytes.
 *     align: Required alignment, in bytes (must be a power of 2), or 0
 *         for the default alignment (64 bytes).
 * [Return value]
 *     Pointer to allocated memory, or NULL on error.
 */
extern void *psp_vram_alloc(uint32_t size, uint32_t align);

/**
 * psp_vram_free:  Free a block of video EDRAM previously allocated from
 * the spare pool with psp_vram_alloc().  Does nothing if ptr == NULL.
 *
 * [Parameters]
 *     ptr: Pointer to memory block to free.
 */
extern void psp_vram_free(void *ptr);


#ifdef DEBUG

/**
 * psp_debug_display_memory_map:  Display a map of memory usage on the top
 * of the screen, along with current usage statistics.  This display is
 * intended to be used in place of the memory meter of the standard debug
 * interface.
 *
 * This function is only available when building with DEBUG defined.
 */
extern void psp_debug_display_memory_map(void);

/**
 * psp_debug_display_ge_info:  Display rendering and buffer usage
 * statistics for the graphics subsystem.
 *
 * This function is only available when building with DEBUG defined.
 */
extern void psp_debug_display_ge_info(void);

/**
 * psp_debug_display_log:  Display the most recent debug messages as an
 * overlay on the screen.
 *
 * This function is only available when building with DEBUG defined.
 */
extern void psp_debug_display_log(void);

/**
 * psp_debug_dump_log:  Write the most recent debug messages to the given
 * file.
 *
 * This function is only available when building with DEBUG defined.
 */
extern void psp_debug_dump_log(const char *path);

#endif  // DEBUG

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_PSP_COMMON_H
