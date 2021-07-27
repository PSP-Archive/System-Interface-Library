/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/psp/internal.h: Common header for declarations internal to
 * PSP-specific code.
 */

#ifndef SIL_SRC_SYSDEP_PSP_INTERNAL_H
#define SIL_SRC_SYSDEP_PSP_INTERNAL_H

/*************************************************************************/
/************************** PSP system headers ***************************/
/*************************************************************************/

/* Avoid defining these in the headers so we can give the compiler
 * useful hints. */
#define sceKernelExitGame    sdk_sceKernelExitGame
#define sceKernelExitThread  sdk_sceKernelExitThread

#include <pspuser.h>
#include <pspaudio.h>
#include <pspaudiocodec.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspmpeg.h>
#include <psppower.h>
#include <psputility.h>

#undef sceKernelExitGame
#undef sceKernelExitThread

/* Give the compiler some hints. */
extern NORETURN void sceKernelExitGame(void);
extern NORETURN void sceKernelExitThread(int code);

/* Declare a system call missing from pspmpeg.h. */
struct SceMpegLLI {
    void *src, *dest;
    void *next;
    int32_t size;
};
#define MAX_DMASIZE  4095
#define MAX_DMABLOCKS  99
extern int sceMpegbase_BEA18F91(struct SceMpegLLI *lli);

/* Fix a misnamed constant in psputility_savedata.h. */
#define PSP_UTILITY_SAVEDATA_DELETE  PSP_UTILITY_SAVEDATADELETE

/*************************************************************************/
/******************** Common constants and variables *********************/
/*************************************************************************/

/* PSP display size and line stride (in pixels). */
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  272
#define DISPLAY_STRIDE  512

/* Sampling rate for audio output (in Hz). */
#define SOUND_RATE  44100

/* Thread priorities (smaller values are higher priority). */
#define THREADPRI_UTILITY_LOW    36  // Save data utility threads (low-pri)
#define THREADPRI_USER_MAX       35  // Maximum allowed value for user threads
#define THREADPRI_MAIN           32  // Main thread
#define THREADPRI_USER_MIN       30  // Minimum allowed value for user threads
#define THREADPRI_UTILITY_BASE   26  // Save data utility threads
#define THREADPRI_FILEIO         25  // File I/O operations
#define THREADPRI_SOUND          20  // Sound streaming and playback
#define THREADPRI_CALLBACK_WATCH 15  // HOME/power button callback monitor

/* Suspend-requested flag.  Nonzero indicates that a system suspend
 * operation is pending. */
extern uint8_t psp_suspend;

/* Most recent system error code (used primarily by sys_file_*() functions). */
extern uint32_t psp_errno;
/* Suspend-ready semaphore.  Signalled by
 * sys_input_acknowledge_suspend_request() to allow a pending suspend
 * operation to complete. */
SceUID psp_suspend_ok_sema;

/* Resume semaphore.  Signalled when the system resumes from suspend, and
 * waited on by sys_input_acknowledge_suspend_request(). */
SceUID psp_resume_sema;

/*************************************************************************/
/****************** PSP-internal function declarations *******************/
/*************************************************************************/

/******** files.c ********/

struct SysFile;

/**
 * psp_file_open_async:  Asynchronously open a file from the filesystem.
 * The pathname is processed as for sys_file_open().  The returned request
 * ID is for use with sys_file_peek_async() and sys_file_wait_async(); the
 * result of the operation is nonzero on success, zero on error.
 *
 * If this function succeeds but the open itself fails, the returned file
 * handle will be invalidated and can no longer be used.
 *
 * [Parameters]
 *     path: Pathname of file to open.
 *     fh_ret: Pointer to variable to receive new file handle.
 * [Return value]
 *     Open request ID (nonzero), or zero on error.
 */
extern int psp_file_open_async(const char *file, struct SysFile **fh_ret);

/**
 * psp_file_pause:  Wait for all pending asynchronous operations to
 * complete, then close all open file descriptors while preserving the
 * current state of the file handles themselves.  Used when preparing to
 * suspend the system.  (The PSP's OS invalidates all open file descriptors
 * when resuming from a suspend operation, and in any case there's no
 * guarantee that the storage media hasn't been removed or changed between
 * suspend and resume time.)
 */
extern void psp_file_pause(void);

/**
 * psp_file_unpause:  Reopen the file descriptors for all open file handles.
 */
extern void psp_file_unpause(void);


/******** graphics.c ********/

struct SysFramebuffer;

/**
 * psp_is_ge_busy:  Return whether the GE is potentially busy rendering.
 * True from a call to graphics_start_frame() through the next call to
 * graphics_finish_frame() and until the GE signals completion of that
 * frame's display list.
 *
 * [Return value]
 *     True if the GE is potentially busy, false if the GE is known to be idle.
 */
extern int psp_is_ge_busy(void);

/**
 * psp_draw_buffer, psp_depth_buffer:  Return a pointer to the display
 * work buffer or depth buffer, respectively.  The buffer size and line
 * stride are given by the DISPLAY_{WIDTH,HEIGHT,STRIDE} constants.
 *
 * If a display mode with no depth buffer is in use, psp_depth_buffer()
 * returns NULL.
 *
 * [Return value]
 *     Pointer to rendering framebuffer or depth buffer.
 */
extern PURE_FUNCTION uint32_t *psp_draw_buffer(void);
extern PURE_FUNCTION uint16_t *psp_depth_buffer(void);

/**
 * psp_framebuffer_width, psp_framebuffer_height, psp_framebuffer_stride:
 * Return the width, height, or line size of the framebuffer currently
 * bound as the render target.  If no framebuffer is bound, these functions
 * return DISPLAY_WIDTH, DISPLAY_HEIGHT, and DISPLAY_STRIDE, respectively.
 *
 * [Return value]
 *     Render target width, height, or stride.
 */
extern int psp_framebuffer_width(void);
extern int psp_framebuffer_height(void);
extern int psp_framebuffer_stride(void);

/**
 * psp_fb_pixel_address:  Return a pointer to the given pixel in the
 * framebuffer currently bound as the render target (which may be the
 * display's back buffer).  Note that the coordinates are not range-checked.
 *
 * [Parameters]
 *     x, y: Coordinates of pixel.
 * [Return value]
 *     Pointer to pixel.
 */
extern void *psp_fb_pixel_address(int x, int y);

/**
 * psp_use_framebuffer:  Use the given framebuffer as the render target.
 * If framebuffer == NULL, rendering operations will go to the display.
 *
 * [Parameters]
 *     framebuffer: Framebuffer to use, or NULL to render to the display.
 */
extern void psp_use_framebuffer(struct SysFramebuffer *framebuffer);

/**
 * psp_sync_framebuffer:  Wait for rendering to the given area of the
 * current framebuffer to complete, and flush any CPU cache lines pointing
 * to that area so the framebuffer contents can be read by the CPU.
 *
 * [Parameters]
 *     x, y: Pixel coordinates of upper-left corner of area to sync.
 *     width, height: Size of area to sync, in pixels.
 */
extern void psp_sync_framebuffer(int x, int y, int width, int height);

/**
 * psp_current_framebuffer:  Return the framebuffer currently bound as
 * the render target, or NULL if no framebuffer if bound.
 *
 * [Return value]
 *     Currently bound framebuffer, or NULL if none.
 */
extern struct SysFramebuffer *psp_current_framebuffer(void);


/******** main.c ********/

/**
 * psp_executable_dir:  Return the pathname of the directory containing
 * the executable file used to start the program.
 */
extern const char *psp_executable_dir(void);


/******** malloc.c ********/

#ifdef DEBUG

/**
 * malloc_display_debuginfo:  Display debug information about malloc()
 * heaps.  Only implemented when DEBUG is defined.
 */
extern void malloc_display_debuginfo(void);

#endif // DEBUG


/******** memory.c ********/

/**
 * psp_mem_init:  Initialize system-level memory management functionality.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int psp_mem_init(void);

/**
 * psp_mem_total:  Return the total size of all memory pools.
 *
 * [Return value]
 *     Total size of memory pools, in bytes.
 */
extern int64_t psp_mem_total(void);

/**
 * psp_mem_get_pool_info:  Return the base address and size of the main
 * and temporary memory pools.  If there is no temporary pool in use,
 * *temp_size_ret will be set to zero.
 *
 * [Parameters]
 *     main_base_ret: Pointer to variable to receive the main pool base
 *         address.
 *     main_size_ret: Pointer to variable to receive the main pool size.
 *     temp_base_ret: Pointer to variable to receive the temporary pool base
 *         address.
 *     temp_size_ret: Pointer to variable to receive the temporary pool size.
 */
extern void psp_mem_get_pool_info(void **main_base_ret, uint32_t *main_size_ret,
                                  void **temp_base_ret, uint32_t *temp_size_ret);

/**
 * psp_mem_report_allocs:  Log the status (free/used state and allocation
 * size) of all memory blocks.
 */
extern void psp_mem_report_allocs(void);


/******** memory-low.c ********/

/**
 * psp_mem_alloc_pools:  Allocate memory pools for use by the top-level
 * memory allocation functions (mem_alloc() and friends).
 *
 * [Parameters]
 *     main_pool_ret: Pointer to variable to receive the main pool pointer.
 *     main_poolsize_ret: Pointer to variable to receive the main pool size.
 *     temp_pool_ret: Pointer to variable to receive the temporary pool
 *         pointer.
 *     temp_poolsize_ret: Pointer to variable to receive the temporary pool
 *         size.
 * [Return value]
 *     True on success, false on error.  Failure to allocate the temporary
 *     pool is not by itself considered an error.
 */
extern int psp_mem_alloc_pools(void **main_pool_ret, uint32_t *main_poolsize_ret,
                               void **temp_pool_ret, uint32_t *temp_poolsize_ret);


/******** misc.c ********/

/**
 * psp_strerror:  Return a string corresponding to the given system call
 * error code.  The returned string is stored in a static buffer which is
 * overwritten on each call.
 *
 * [Parameters]
 *     code: PSP system call error code.
 * [Return value]
 *     Corresponding error string.
 */
extern const char *psp_strerror(uint32_t code);


/******** texture.c ********/

struct SysTexture;

/**
 * psp_texture_init:  Initialize texture handling state.
 */
extern void psp_texture_init(void);

/**
 * psp_set_texture_state:  Send GE commands to apply the current texture
 * state.  Used to apply changed texture settings before drawing a
 * primitive; can also be called after manipulating the GE texture settings
 * directly or to restore the GE to a knwon state.  The following settings
 * are modified:
 *    - GE_STATE_TEXTURE on/off state
 *    - Color palette for indexed textures
 *    - Texture data pointers for all mipmap levels
 *    - Texture data format
 *    - Texture drawing mode
 *    - Texture coordinate scale
 *    - Texture mag/min filters
 *
 * [Parameters]
 *     force: True to forcibly update all settings, false to only update
 *         settings which have changed.
 */
extern void psp_set_texture_state(int force);

/**
 * psp_current_texture:  Return a pointer to the currently applied texture,
 * or NULL if no texture is currently applied.
 *
 * [Return value]
 *     Currently applied texture, or NULL if none.
 */
extern struct SysTexture *psp_current_texture(void);

/**
 * psp_set_texture:  Set the given texture as the current texture for
 * subsequent draw operations.
 *
 * [Parameters]
 *     texture: Texture to use for draw operations, or NULL to use no texture.
 */
extern void psp_set_texture(struct SysTexture *texture);

/**
 * psp_texture_flush_deferred_destroy_list:  Free memory used by any
 * textures which were destroyed while the GE was busy.  This function
 * should only be called while the GE is idle.
 */
extern void psp_texture_flush_deferred_destroy_list(void);

/**
 * psp_create_vram_texture:  Create and return a new 32bpp texture whose
 * pixel buffer is stored in video RAM instead of main system RAM.  The
 * texture's contents are initially undefined.
 *
 * [Parameters]
 *     width: Width of new texture, in pixels.
 *     height: Height of new texture, in pixels.
 * [Return value]
 *     Newly created texture, or NULL on error.
 */
extern struct SysTexture *psp_create_vram_texture(int width, int height);

/*************************************************************************/
/*********************** PSP-specific error codes ************************/
/*************************************************************************/

/* Error codes from sceIo*() functions.  (Apparently numbered like errno.h?) */
enum {
    PSP_EPERM   = 0x80010001,
    PSP_ENOENT  = 0x80010002,
    PSP_ESRCH   = 0x80010003,
    PSP_EINTR   = 0x80010004,
    PSP_EIO     = 0x80010005,
    PSP_ENXIO   = 0x80010006,
    PSP_E2BIG   = 0x80010007,
    PSP_ENOEXEC = 0x80010008,  // Reused to mean "async table full".
    PSP_EBADF   = 0x80010009,
    PSP_ECHILD  = 0x8001000A,
    PSP_EAGAIN  = 0x8001000B,
    PSP_ENOMEM  = 0x8001000C,
    PSP_EACCES  = 0x8001000D,
    PSP_EFAULT  = 0x8001000E,
    PSP_ENOTBLK = 0x8001000F,
    PSP_EBUSY   = 0x80010010,
    PSP_EEXIST  = 0x80010011,
    PSP_EXDEV   = 0x80010012,
    PSP_ENODEV  = 0x80010013,
    PSP_ENOTDIR = 0x80010014,
    PSP_EISDIR  = 0x80010015,
    PSP_EINVAL  = 0x80010016,
    PSP_ENFILE  = 0x80010017,
    PSP_EMFILE  = 0x80010018,
    PSP_ENOTTY  = 0x80010019,
    PSP_ETXTBSY = 0x8001001A,
    PSP_EFBIG   = 0x8001001B,
    PSP_ENOSPC  = 0x8001001C,
    PSP_ESPIPE  = 0x8001001D,
    PSP_EROFS   = 0x8001001E,
    PSP_EMLINK  = 0x8001001F,
    PSP_EPIPE   = 0x80010020,
    PSP_EDOM    = 0x80010021,
    PSP_ERANGE  = 0x80010022,
    PSP_EDEADLK = 0x80010023,
    PSP_ENAMETOOLONG = 0x80010024,
    PSP_ECANCELED    = 0x8001007D,  // Not sure if the OS uses this, but we do.
};

/* Other error codes. */
enum {
    PSP_UTILITY_BAD_ADDRESS      = 0x80110002,
    PSP_UTILITY_BAD_PARAM_SIZE   = 0x80110004,
    PSP_UTILITY_BUSY             = 0x80110005,
    PSP_SAVEDATA_LOAD_NO_CARD    = 0x80110301,
    PSP_SAVEDATA_LOAD_IO_ERROR   = 0x80110305,
    PSP_SAVEDATA_LOAD_CORRUPT    = 0x80110306,
    PSP_SAVEDATA_LOAD_NOT_FOUND  = 0x80110307,
    PSP_SAVEDATA_LOAD_BAD_PARAMS = 0x80110308,
    PSP_SAVEDATA_SAVE_NO_CARD    = 0x80110381,
    PSP_SAVEDATA_SAVE_CARD_FULL  = 0x80110383,
    PSP_SAVEDATA_SAVE_WRITE_PROT = 0x80110384,
    PSP_SAVEDATA_SAVE_IO_ERROR   = 0x80110385,
    PSP_SAVEDATA_SAVE_BAD_PARAMS = 0x80110388,
};

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_PSP_INTERNAL_H
