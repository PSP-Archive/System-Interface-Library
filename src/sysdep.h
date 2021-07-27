/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep.h: Interface to system-specific implementation routines.
 */

/*
 * This header declares the interface through which the platform-
 * independent routines exported by SIL call the actual implementations
 * specific to each platform.
 *
 * These routines are intended only for use by SIL itself; there is
 * normally no need to call them from outside.
 */

#ifndef SIL_SRC_SYSDEP_H
#define SIL_SRC_SYSDEP_H

struct DateTime;
struct GraphicsDisplayModeEntry;
struct InputEvent;
struct Matrix4f;
struct ThreadAttributes;
struct Vector2f;
struct Vector3f;
struct Vector4f;
enum FramebufferColorType;
enum GraphicsError;
enum GraphicsPrimitiveType;
enum ShaderAttribute;
enum ShaderType;
enum TextureFormat;

/* This is declared below in a typedef, but we also need it in some
 * functions declared earlier. */
struct SysTexture;

/*************************************************************************/
/********************** Macros for testing (part 1) **********************/
/*************************************************************************/

/*
 * For testing, we define separate versions of the sys_*() functions which
 * simulate the requisite functionality in a way which is reproducible and
 * which allows us to verify the results of each operation.  Since we
 * naturally cannot include two copies of an identically-named function
 * (much less expect the caller to know which one to call!), and since we
 * want to minimize the additional clutter in real sysdep implementations,
 * we write stubs here for every sysdep function which call either the real
 * or the test implementation depending on whether testing mode is enabled.
 * The actual stub functions and redirecting macros are defined at the
 * bottom of this file.
 *
 * In order for this functionality to work, any file which defines a
 * sys_*() function or which calls a specific implementation of such a
 * function must define either IN_SYSDEP (for real implementations) or
 * IN_SYSDEP_TEST (for test implementations).
 *
 * Note that we do not stub out the thread and synchronization functions,
 * since their effects are directly testable (and since otherwise we'd
 * have to write a full-fledged scheduler and handle thread scheduling and
 * atomic operations ourselves).  Similarly, we don't stub out the file
 * and directory reading functions, instead relying on existing data (the
 * testdata directory at the top of the source tree) for testing.
 */

#if defined(SIL_INCLUDE_TESTS) && defined(IN_SYSDEP_TEST)
// Through the end of this section.

#define sys_input_init                  TEST_sys_input_init
#define sys_input_cleanup               TEST_sys_input_cleanup
#define sys_input_update                TEST_sys_input_update
#define sys_input_info                  TEST_sys_input_info
#define sys_input_grab                  TEST_sys_input_grab
#define sys_input_is_quit_requested     TEST_sys_input_is_quit_requested
#define sys_input_is_suspend_requested  TEST_sys_input_is_suspend_requested
#define sys_input_acknowledge_suspend_request TEST_sys_input_acknowledge_suspend_request
#define sys_input_enable_unfocused_joystick TEST_sys_input_enable_unfocused_joystick
#define sys_input_joystick_copy_name    TEST_sys_input_joystick_copy_name
#define sys_input_joystick_button_mapping TEST_sys_input_joystick_button_mapping
#define sys_input_joystick_rumble       TEST_sys_input_joystick_rumble
#define sys_input_mouse_set_position    TEST_sys_input_mouse_set_position
#define sys_input_text_set_state        TEST_sys_input_text_set_state

#define sys_sound_init                  TEST_sys_sound_init
#define sys_sound_playback_rate         TEST_sys_sound_playback_rate
#define sys_sound_set_latency           TEST_sys_sound_set_latency
#define sys_sound_enable_headphone_disconnect_check TEST_sys_sound_enable_headphone_disconnect_check
#define sys_sound_check_headphone_disconnect TEST_sys_sound_check_headphone_disconnect
#define sys_sound_acknowledge_headphone_disconnect TEST_sys_sound_acknowledge_headphone_disconnect
#define sys_sound_cleanup               TEST_sys_sound_cleanup

#define sys_time_init                   TEST_sys_time_init
#define sys_time_unit                   TEST_sys_time_unit
#define sys_time_now                    TEST_sys_time_now
#define sys_time_delay                  TEST_sys_time_delay
#define sys_time_get_utc                TEST_sys_time_get_utc

#define sys_userdata_init               TEST_sys_userdata_init
#define sys_userdata_cleanup            TEST_sys_userdata_cleanup
#define sys_userdata_get_data_path      TEST_sys_userdata_get_data_path
#define sys_userdata_perform            TEST_sys_userdata_perform

#define sys_console_vprintf             TEST_sys_console_vprintf
#define sys_display_error               TEST_sys_display_error
#define sys_get_language                TEST_sys_get_language
#define sys_open_file                   TEST_sys_open_file
#define sys_open_url                    TEST_sys_open_url
#define sys_reset_idle_timer            TEST_sys_reset_idle_timer

#define sys_debug_get_memory_stats      TEST_sys_debug_get_memory_stats

#endif  // SIL_INCLUDE_TESTS && IN_SYSDEP_TEST

/*************************************************************************/
/******************* Generic file and directory access *******************/
/*************************************************************************/

/**
 * SysFile:  Opaque file handle type, analogous to the FILE type defined in
 * the stdio library.
 */
typedef struct SysFile SysFile;

/**
 * SysDir: Opaque directory handle type, analogous to the DIR type defined
 * by dirent.h on POSIX systems.
 */
typedef struct SysDir SysDir;

/**
 * MAX_ASYNC_READS:  Maximum number of simultaneous asynchronous reads that
 * are supported by all systems (i.e., minimum number of simultaneous reads
 * that all systems must support).  Attempting to perform an asynchronous
 * read when this number of reads are outstanding may fail, depending on
 * the system.
 */
#define MAX_ASYNC_READS  100

/*--------------------------- File operations ---------------------------*/

/**
 * sys_file_init:  Initialize the file/directory access functionality.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_file_init(void);

/**
 * sys_file_cleanup:  Shut down the file/directory access functionality.
 * The behavior of this function is undefined if any files or directories
 * are open when the function is called.
 */
extern void sys_file_cleanup(void);

/**
 * sys_file_open:  Open the given file from the host filesystem.  Pathname
 * components are delimited with "/" characters and are treated
 * case-insensitively.  The meaning of multiple "/" characters in sequence
 * is system-dependent.
 *
 * If the pathname is not absolute, it is taken as relative to the current
 * working directory of the process on systems which support such a
 * concept; on other systems, the behavior is undefined.  Even on systems
 * which do support a current directory, no guarantees are made about what
 * the current directory is; in particular, it may bear no relation to the
 * location of the program's resource files.
 *
 * If the native filesystem is case-sensitive and there are multiple files
 * which match the given pathname, one of the matching files will be
 * opened, but it is undefined which file is chosen -- _except_ that exact
 * matches for individual pathname components are preferred over inexact
 * matches, with earlier components in the pathname taking precedence over
 * later ones.
 *
 * For example, when requesting the path "abc/def/ghi" on a case-sensitive
 * filesystem, if the following three files exist in the filesystem:
 *     1) abc/Def/Ghi
 *     2) abc/DEF/GHI
 *     3) ABC/def/ghi
 * then either file 1 or file 2 will be chosen at random.  While file 3 has
 * more exact component matches ("def" and "ghi"), it loses to files 1 and
 * 2 on the first component and thus cannot be selected.
 *
 * If the pathname refers to a directory, the result is undefined; if the
 * function returns successfully for such a pathname, all operations on the
 * returned handle except sys_file_close() are likewise undefined.
 *
 * [Parameters]
 *     path: Pathname of file to open.
 * [Return value]
 *     File handle, or NULL on error.
 */
extern SysFile *sys_file_open(const char *path);

/**
 * sys_file_dup:  Duplicate the given file handle.
 *
 * [Parameters]
 *     fh: File handle.
 * [Return value]
 *     Duplicated file handle, or NULL on error.
 */
extern SysFile *sys_file_dup(SysFile *fh);

/**
 * sys_file_close:  Close the given file.  If there are any outstanding
 * asynchronous read requests, they are automatically aborted as though
 * sys_file_abort_async() had been called on each such outstanding request.
 * Attempting to close a file while other threads are performing
 * synchronous reads on the file results in undefined behavior.
 *
 * This function does nothing if fh == NULL.
 *
 * [Parameters]
 *     fh: File handle.
 */
extern void sys_file_close(SysFile *fh);

/**
 * sys_file_size:  Return the size in bytes of the given file.  This
 * function does not fail if passed a valid, open file handle.
 *
 * [Parameters]
 *     fh: File handle.
 * [Return value]
 *     Size of file, in bytes.
 */
extern int64_t sys_file_size(SysFile *fh);

/**
 * sys_file_seek:  Set the position for synchronous file reads.  This
 * function does not fail if passed a valid, open file handle and a valid
 * FILE_SEEK_* constant for "how".
 *
 * [Parameters]
 *     fh: File handle.
 *     pos: New synchronous read position, in bytes.
 *     how: Method for setting position (FILE_SEEK_*).
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_file_seek(SysFile *fh, int64_t pos, int how);
enum {
    FILE_SEEK_SET = 0,  // Set the read position to the given value.
    FILE_SEEK_CUR,      // Add the given value to the current read position.
    FILE_SEEK_END,      // Set the position to the given value + the file size.
};

/**
 * sys_file_tell:  Return the current position for synchronous file reads.
 * This function does not fail if passed a valid, open file handle.
 *
 * [Parameters]
 *     fh: File handle.
 * [Return value]
 *     Current synchronous read position, in bytes.
 */
extern int64_t sys_file_tell(SysFile *fh);

/**
 * sys_file_read:  Read data synchronously from the given file, starting
 * at the current synchronous read position.  If any asynchronous reads
 * are in progress on the file, this function may wait until those reads
 * have completed.
 *
 * [Parameters]
 *     fh: File handle.
 *     buf: Read buffer.
 *     len: Number of bytes to read.
 * [Return value]
 *     Number of bytes read, or a negative value on error.  End-of-file is
 *     is not considered an error.
 */
extern int32_t sys_file_read(SysFile *fh, void *buf, int32_t len);

/**
 * sys_file_read_at:  Read data synchronously from the given file, starting
 * at the given position.  If any asynchronous reads are in progress on the
 * file, this function may wait until those reads have completed.
 *
 * This function does not affect the current synchronous read position used
 * by sys_file_seek() and sys_file_read().
 *
 * [Parameters]
 *     fh: File handle.
 *     buf: Read buffer.
 *     len: Number of bytes to read.
 *     filepos: Starting position for the read, in bytes from the beginning
 *         of the file.
 * [Return value]
 *     Number of bytes read, or a negative value on error.  End-of-file is
 *     not considered an error.
 */
extern int32_t sys_file_read_at(SysFile *fh, void *buf, int32_t len, int64_t filepos);

/**
 * sys_file_read_async:  Start an asynchronous read from the given file,
 * and return immediately.  The read buffer may not be accessed until
 * completion of the read has been confirmed by calling sys_file_wait_async().
 *
 * The read request ID returned by this function should be considered
 * thread-local, and must not be used from other threads.
 *
 * The caller can use the deadline parameter to request that the data be
 * read in within that time limit.  The function will make an effort to
 * have the read complete within that time limit, though this is not
 * guaranteed.  A negative value indicates that the caller does not care
 * how long it takes to read in the data; such requests are processed in
 * FIFO order.
 *
 * Asynchronous reads do not affect the current synchronous read position.
 *
 * [Parameters]
 *     fh: File handle.
 *     buf: Read buffer.
 *     len: Number of bytes to read.
 *     filepos: Starting position for the read, in bytes from the beginning
 *         of the file.
 *     deadline: Time by which the read should complete, in seconds from
 *         the call to this function, or a negative value for no deadline.
 * [Return value]
 *     Read request ID (nonzero), or zero on error.
 */
extern int sys_file_read_async(SysFile *fh, void *buf, int32_t len,
                               int64_t filepos, float deadline);

/**
 * sys_file_poll_async:  Check whether the given asynchronous read has
 * has completed.
 *
 * [Parameters]
 *     request: Read request ID.
 * [Return value]
 *     True if the read has completed (whether successfully or not) or if
 *     the request ID is invalid; false if the read is in progress.
 */
extern int sys_file_poll_async(int request);

/**
 * sys_file_wait_async:  Wait (if necessary) for the given asynchronous
 * read to complete, and return the result of the read.  The result is the
 * same as would have been returned by sys_file_read() with the same
 * parameters from the same read position.
 *
 * This function will never block for a given read request if
 * sys_file_poll_async() returns true for that request.
 *
 * [Parameters]
 *     request: Read request ID.
 * [Return value]
 *     Number of bytes read, or a negative value on error.  End-of-file is
 *     not considered an error.
 */
extern int32_t sys_file_wait_async(int request);

/**
 * sys_file_abort_async:  Abort the given asynchronous read.  Even if this
 * function succeeds, the buffer must not be freed or reused until the
 * request's completion has been confirmed with sys_file_wait_async().
 * After calling this function, the request's result will always be an
 * error, but the state of the buffer is undefined.
 *
 * Some systems may not be able to immediately abort an asynchronous read
 * request, so a successful return from this function does _not_ guarantee
 * that a subsequent sys_file_wait_async() call on the same request will
 * return immediately.
 *
 * [Parameters]
 *     request: Read request ID.
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_file_abort_async(int request);

/*------------------------ Directory operations -------------------------*/

/**
 * sys_dir_open:  Open the given directory from the filesystem.  Pathname
 * components are delimited with "/" characters and are treated
 * case-insensitively, following the same rules as for sys_file_open().
 *
 * [Parameters]
 *     path: Pathname of directory to open.
 * [Return value]
 *     Directory handle, or NULL on error.
 */
extern SysDir *sys_dir_open(const char *path);

/**
 * sys_dir_read:  Read the next file entry from the given directory and
 * return its filename (not including the directory pathname).  Entries
 * are not guaranteed to be returned in any particular order, but each
 * distinct file or subdirectory in the directory is guaranteed to be
 * returned only once.  Entries which are neither files nor subdirectories
 * (such as POSIX named pipes) are _not_ returned.
 *
 * Note that the returned string is stored in a static buffer which will
 * be overwritten by the next call to sys_dir_read() for this handle and
 * is only valid until sys_dir_close() is called for the handle.
 *
 * [Parameters]
 *     dir: Directory handle.
 *     is_subdir_ret: Pointer to variable to receive true (nonzero) if the
 *         returned entry is a subdirectory, false (zero) if it is a file.
 *         The value stored is undefined on NULL return.
 * [Return value]
 *     Fiilename of next file, or NULL if all file entries have been read.
 */
extern const char *sys_dir_read(SysDir *dir, int *is_subdir_ret);

/**
 * sys_dir_close:  Close the given directory.  This function does nothing
 * if passed a NULL value.
 *
 * [Parameters]
 *     dir: Directory handle.
 */
extern void sys_dir_close(SysDir *dir);

/*************************************************************************/
/************************* Framebuffer handling **************************/
/*************************************************************************/

/**
 * SysFramebuffer:  Opaque type for system-specific framebuffer data.
 */
typedef struct SysFramebuffer SysFramebuffer;

/*-----------------------------------------------------------------------*/

/**
 * sys_framebuffer_supported:  Return whether the system supports
 * offscreen framebuffers.  Implements the high-level function
 * framebuffer_supported().
 *
 * [Return value]
 *     True if offscreen framebuffers are supported, false if not.
 */
extern int sys_framebuffer_supported(void);

/**
 * sys_framebuffer_create:  Create a new framebuffer of the given size.
 * If the system does not support framebuffers, this function will always
 * return NULL.
 *
 * [Parameters]
 *     width, height: Framebuffer size, in pixels (guaranteed >=1).
 *     color_type: Color format (FBCOLOR_*).
 *     depth_bits: Minimum number of depth buffer bits per pixel
 *         (guaranteed >=0).
 *     stencil_bits: Minimum number of stencil buffer bits per pixel
 *         (guaranteed >=0).
 * [Return value]
 *     New framebuffer, or NULL on error.
 */
extern SysFramebuffer *sys_framebuffer_create(
    int width, int height, enum FramebufferColorType color_type,
    int depth_bits, int stencil_bits);

/**
 * sys_framebuffer_destroy:  Destroy the given framebuffer.
 *
 * [Parameters]
 *     framebuffer: Framebuffer to destroy (guaranteed non-NULL).
 */
extern void sys_framebuffer_destroy(SysFramebuffer *framebuffer);

/**
 * sys_framebuffer_bind:  Bind the given framebuffer to the current
 * rendering target.  If framebuffer == NULL, cancel any framebuffer
 * binding so that the display is bound to the rendering target.
 *
 * [Parameters]
 *     framebuffer: Framebuffer to bind, or NULL to cancel binding.
 */
extern void sys_framebuffer_bind(SysFramebuffer *framebuffer);

/**
 * sys_framebuffer_get_texture:  Return a texture (SysTexture *) which
 * refers to the framebuffer's data.  The texture remains owned by the
 * framebuffer.
 *
 * This function always succeeds for a valid framebuffer.  (As a corollary,
 * implementations must return failure from sys_framebuffer_create() if a
 * texture cannot be created for the framebuffer.)
 *
 * [Parameters]
 *     framebuffer: Framebuffer to return texture for (guaranteed non-NULL).
 * [Return value]
 *     Texture for framebuffer (guaranteed non-NULL).
 */
extern struct SysTexture *sys_framebuffer_get_texture(
    SysFramebuffer *framebuffer);

/**
 * sys_framebuffer_set_antialias:  Set the given framebuffer's antialiasing
 * flag.
 *
 * [Parameters]
 *     framebuffer: Framebuffer to modify.
 *     on: True to enable antialiasing, false to disable.
 */
extern void sys_framebuffer_set_antialias(SysFramebuffer *framebuffer, int on);

/**
 * sys_framebuffer_discard_data:  Discard all pixel data in the given
 * framebuffer.  Implements the high-level function framebuffer_discard_data().
 *
 * [Parameters]
 *     framebuffer: Framebuffer whose contents may be discarded (guaranteed
 *         non-NULL).
 */
extern void sys_framebuffer_discard_data(SysFramebuffer *framebuffer);

/*************************************************************************/
/***************** Graphics and rendering functionality ******************/
/*************************************************************************/

/*
 * Many of these functions are simply implementation functions for the
 * high-level graphics interface in graphics.h; however, these functions
 * may assume that their parameters have been checked for validity, unless
 * otherwise specified in the function documentation.
 */

/*-------------- Graphics-related constants and structures --------------*/

/**
 * SysGraphicsInfo:  Structure describing the system's graphics capabilities.
 */
typedef struct SysGraphicsInfo SysGraphicsInfo;
struct SysGraphicsInfo {
    /* True if a windowed mode is supported, as for
     * graphics_has_windowed_mode(). */
    uint8_t has_windowed_mode;

    /* Number of display devices available (must be positive). */
    int num_devices;

    /* Number and array of supported display sizes.  At least one valid
     * size must be returned from sys_graphics_init().  The pointer is
     * declared const to enforce read-onlyness to the caller, but the
     * implementation is free to cast it away if needed. */
    int num_modes;
    const struct GraphicsDisplayModeEntry *modes;
};

/**
 * SysGraphicsParam:  Constants for standard rendering parameters, used
 * with sys_graphics_{set,get}_*_param().
 */
enum SysGraphicsParam {
    /* Coordinate transformation matrices. */
    SYS_GRAPHICS_PARAM_PROJECTION_MATRIX = 1,
    SYS_GRAPHICS_PARAM_VIEW_MATRIX,
    SYS_GRAPHICS_PARAM_MODEL_MATRIX,

    /* Rendering parameters. */
    SYS_GRAPHICS_PARAM_ALPHA_TEST = 101,      // boolean
    SYS_GRAPHICS_PARAM_ALPHA_TEST_COMPARISON, // int (GRAPHICS_COMPARISON_*)
    SYS_GRAPHICS_PARAM_ALPHA_REFERENCE,       // float
    SYS_GRAPHICS_PARAM_BLEND_COLOR,           // Vector4f
    SYS_GRAPHICS_PARAM_CLIP,                  // boolean
    SYS_GRAPHICS_PARAM_COLOR_WRITE,           // boolean[4] (bit0=R, bit1=G, bit2=B, bit3=A)
    SYS_GRAPHICS_PARAM_DEPTH_TEST,            // boolean test enabled?
    SYS_GRAPHICS_PARAM_DEPTH_TEST_COMPARISON, // int (GRAPHICS_COMPARISON_*)
    SYS_GRAPHICS_PARAM_DEPTH_WRITE,           // boolean
    SYS_GRAPHICS_PARAM_FACE_CULL,             // boolean
    SYS_GRAPHICS_PARAM_FACE_CULL_CW,          // boolean (true=CW, false=CCW)
    SYS_GRAPHICS_PARAM_FIXED_COLOR,           // Vector4f
    SYS_GRAPHICS_PARAM_FOG,                   // boolean
    SYS_GRAPHICS_PARAM_FOG_START,             // float
    SYS_GRAPHICS_PARAM_FOG_END,               // float
    SYS_GRAPHICS_PARAM_FOG_COLOR,             // Vector4f
    SYS_GRAPHICS_PARAM_POINT_SIZE,            // float
    SYS_GRAPHICS_PARAM_STENCIL_TEST,          // boolean
    /* These three values are always set as a group, STENCIL_MASK last. */
    SYS_GRAPHICS_PARAM_STENCIL_COMPARISON,    // int (GRAPHICS_COMPARISON_*)
    SYS_GRAPHICS_PARAM_STENCIL_REFERENCE,     // unsigned int
    SYS_GRAPHICS_PARAM_STENCIL_MASK,          // unsigned int
    /* These three values are always set as a group, STENCIL_OP_DPASS last. */
    SYS_GRAPHICS_PARAM_STENCIL_OP_SFAIL,      // int (GRAPHICS_STENCIL_*)
    SYS_GRAPHICS_PARAM_STENCIL_OP_DFAIL,      // int (GRAPHICS_STENCIL_*)
    SYS_GRAPHICS_PARAM_STENCIL_OP_DPASS,      // int (GRAPHICS_STENCIL_*)

    /* Texture mapping parameters. */
    SYS_GRAPHICS_PARAM_TEXTURE_OFFSET = 201,  // Vector2f
};
typedef enum SysGraphicsParam SysGraphicsParam;

/**
 * SysPrimitive:  Opaque structure representing a graphics primitive and
 * associated vertex data, used with the sys_graphics_*_primitive() functions.
 */
typedef struct SysPrimitive SysPrimitive;

/*---------------------- Basic graphics operations ----------------------*/

/**
 * sys_graphics_init:  Initialize the graphics/rendering functionality,
 * and return a structure describing the system's graphics capabilities.
 * The returned structure must remain valid until sys_graphics_cleanup()
 * is called.
 *
 * [Return value]
 *     Pointer to graphics capability structure, or NULL on error.
 */
extern const SysGraphicsInfo *sys_graphics_init(void);

/**
 * sys_graphics_cleanup:  Shut down the graphics/rendering functionality,
 * close the display (if open), and free all related resources.
 */
extern void sys_graphics_cleanup(void);

/**
 * sys_graphics_device_width, sys_graphics_device_height:  Return the
 * current width or height of the display device.  These implement the
 * high-level functions graphics_device_width() and graphics_device_height().
 *
 * [Return value]
 *     Display device width or height, in pixels.
 */
extern int sys_graphics_device_width(void);
extern int sys_graphics_device_height(void);

/**
 * sys_graphics_set_display_attr:  Set a system-specific display attribute.
 * Implements the high-level function graphics_set_display_mode().
 *
 * [Parameters]
 *     name: Attribute name (guaranteed non-NULL).
 *     args: Attribute value(s).
 * [Return value]
 *     True if the display attribute was successfully set, false if the
 *     attribute is unknown or an error occurred.
 */
extern int sys_graphics_set_display_attr(const char *name, va_list args);

/**
 * sys_graphics_set_display_mode:  Set a new display mode.  Implements
 * the high-level function graphics_set_display_mode().
 *
 * [Parameters]
 *     width, height: Display size, in pixels.
 * [Return value]
 *     GRAPHICS_ERROR_SUCCESS if the display mode was successfully set,
 *     otherwise an appropriate error code.
 */
extern enum GraphicsError sys_graphics_set_display_mode(int width, int height);

/**
 * sys_graphics_renderer_info:  Return a string describing the low-level
 * rendering backend.  Implements the high-level function
 * graphics_renderer_info().
 *
 * [Return value]
 *     Renderer information string.
 */
extern const char *sys_graphics_renderer_info(void);

/**
 * sys_graphics_display_is_window:  Return whether the current display
 * mode is a windowed mode.  Implements the high-level function
 * graphics_display_is_window().
 *
 * [Return value]
 *     True if the display mode is a windowed mode, false if the display
 *     mode is a full-screen mode or if no display mode is set.
 */
extern int sys_graphics_display_is_window(void);

/**
 * sys_graphics_set_window_title:  Set the title to be displayed in the
 * window title bar or any equivalent system facility.  Implements the
 * high-level function graphics_set_window_title().
 *
 * [Parameters]
 *     title: Title string (guaranteed non-NULL).
 */
extern void sys_graphics_set_window_title(const char *title);

/**
 * sys_graphics_set_window_icon:  Set the icon to be associated with the
 * window.  Implements the high-level function graphics_set_window_icon().
 *
 * [Parameters]
 *     texture: Texture to use as icon image (guaranteed non-NULL and
 *         unlocked).
 */
extern void sys_graphics_set_window_icon(struct SysTexture *texture);

/**
 * sys_graphics_set_window_resize_limits:  Set constraints on window
 * resize operations.  Implements the high-level function
 * graphics_set_window_resize_limits().
 *
 * For each pair of arguments (width/height or x/y), the caller guarantees
 * that either both arguments are positive or both arguments are zero.
 *
 * [Parameters]
 *     min_width, min_height: Minimum allowable size for the window.
 *     max_width, max_height: Maximum allowable size for the window.
 *     min_aspect_x, min_aspect_y: Minimum allowable aspect ratio for the
 *         window.
 *     max_aspect_x, max_aspect_y: Maximum allowable aspect ratio for the
 *         window.
 */
extern void sys_graphics_set_window_resize_limits(
    int min_width, int min_height, int max_width, int max_height,
    int min_aspect_x, int min_aspect_y, int max_aspect_x, int max_aspect_y);

/**
 * sys_graphics_show_mouse_pointer:  Set whether to display a system-level
 * visible mouse pointer on top of the display.  Implements the high-level
 * function graphics_show_mouse_pointer().
 *
 * [Parameters]
 *     on: True to display the pointer, false to hide it.
 */
extern void sys_graphics_show_mouse_pointer(int on);

/**
 * sys_graphics_get_mouse_pointer_state:  Return whether the system-level
 * mouse pointer display is enabled.  Implements the high-level function
 * graphics_get_mouse_pointer_state().
 *
 * [Return value]
 *     True if the pointer is displayed, false if not.
 */
extern int sys_graphics_get_mouse_pointer_state(void);

/**
 * sys_graphics_get_frame_period:  Return the nominal frame period of the
 * display in seconds, based on the current display mode and attributes.
 * The frame period is defined as the mean time between successive calls
 * to sys_graphics_finish_frame() under the assumptions that (1) the
 * intervening calls to sys_graphics_start_frame() block until the display
 * hardware is ready to accept a new frame and (2) all other graphics
 * calls complete immediately.
 *
 * If the frame period is unknown, or if the display does not have a fixed
 * frame period (for example, because vertical sync has been disabled), the
 * function returns a period of zero (numerator == 0, denominator != 0).
 *
 * The behavior of this function is undefined if called before a display
 * mode has been successfully set (including after a failed call to
 * sys_graphics_set_display_mode()).
 *
 * [Parameters]
 *     numerator_ret: Pointer to variable to receive the numerator of the
 *         frame period (guaranteed non-NULL).
 *     denominator_ret: Pointer to variable to receive the denominator of
 *         the frame period (guaranteed non-NULL).
 */
extern void sys_graphics_get_frame_period(int *numerator_ret,
                                          int *denominator_ret);

/**
 * sys_graphics_has_focus:  Return whether this program has the input
 * focus.  Implements the high-level function graphics_has_focus().
 *
 * [Return value]
 *     True if this program has the input focus, false if not.
 */
extern int sys_graphics_has_focus(void);

/**
 * sys_graphics_start_frame:  Prepare for rendering a new frame, and return
 * the current width and height of the display.  (These values will be
 * treated as constant until the next call to sys_graphics_finish_frame().)
 *
 * This function may freely change the framebuffer binding; the high-level
 * function graphics_start_frame() will call sys_framebuffer_bind(NULL)
 * after this function returns.
 *
 * [Parameters]
 *     width_ret, height_ret: Pointers to variables to receive the current
 *         display size, in pixels (guaranteed non-NULL).
 */
extern void sys_graphics_start_frame(int *width_ret, int *height_ret);

/**
 * sys_graphics_finish_frame:  Finish rendering the current frame, and
 * perform any operations necessary to present the rendered framebuffer
 * to the user (such as flipping buffers for a double-buffered display).
 * This function should _not_ wait for background hardware rendering to
 * complete or wait for the next vertical blank unless required by the
 * system.
 *
 * This function may freely change the framebuffer binding.
 */
extern void sys_graphics_finish_frame(void);

/**
 * sys_graphics_sync:  Wait for any background rendering operations to
 * complete, and optionally flush any resources which are pending deletion.
 *
 * [Parameters]
 *     flush: True to release resources pending deletion.
 */
extern void sys_graphics_sync(int flush);

/**
 * sys_graphics_clear:  Clear the display.  Implements the high-level
 * functions graphics_clear(), graphics_clear_color(), and
 * graphics_clear_depth().
 *
 * [Parameters]
 *     color: Clear color, or NULL to not clear the color buffer.
 *     depth: Pointer to depth value to store, or NULL to not clear the
 *         depth and stencil buffers.
 *     stencil: Stencil value to store.  Ignored if depth is NULL.
 */
extern void sys_graphics_clear(const struct Vector4f *color,
                               const float *depth, unsigned int stencil);

/**
 * sys_graphics_read_pixels:  Read pixels from the display.  Implements
 * the high-level function graphics_read_pixels().
 *
 * [Parameters]
 *     x, y: Pixel coordinates of lower-left corner of region to read
 *         (0,0 is the bottom-left corner of the display); both are
 *         guaranteed to be nonnegative.
 *     w, h: Size of region to read, in pixels; both are guaranteed to
 *         be positive.
 *     stride: Line stride of buffer, in pixels; guaranteed to be >= w.
 *     buffer: Buffer into which pixel data will be stored; guaranteed to
 *         be non-NULL and have room for at least w*h*4 bytes.
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_graphics_read_pixels(int x, int y, int w, int h, int stride,
                                    void *buffer);

/*---------------------- Render state manipulation ----------------------*/

/*
 * Note that the high-level graphics routines do not attempt to check for
 * redundant state changes (i.e., state change operations that set a state
 * parameter to its current value or that are subsequently changed again
 * without being used in a draw operation), so sysdep implementations
 * should add such checks if they would be significantly less expensive
 * than just performing the redundant change.
 */

/**
 * sys_graphics_set_viewport:  Set the render viewport position and size.
 * Implements the high-level function graphics_set_viewport().
 *
 * [Parameters]
 *     left: Leftmost X coordinate of the viewport, in pixels
 *         (guaranteed >=0).
 *     bottom: Bottommost Y coordinate of the viewport, in pixels
 *         (guaranteed >=0).
 *     width: Width of the viewport, in pixels (guaranteed >=1).
 *     height: Height of the viewport, in pixels (guaranteed >=1).
 */
extern void sys_graphics_set_viewport(int left, int bottom,
                                      int width, int height);

/**
 * sys_graphics_set_clip_region:  Set the render clipping region and size.
 * Implements the high-level function graphics_set_clip_region(); note
 * that the case of width==0 && height==0 (reset clipping region) is
 * translated to proper values by graphics_set_clip_region().
 *
 * [Parameters]
 *     left: Leftmost X coordinate of the clipping region, in pixels
 *         (guaranteed >=0).
 *     bottom: Bottommost Y coordinate of the clipping region, in pixels
 *         (guaranteed >=0).
 *     width: Width of the clipping region, in pixels (guaranteed >=0).
 *     height: Height of the clipping region, in pixels (guaranteed >=0).
 */
extern void sys_graphics_set_clip_region(int left, int bottom,
                                         int width, int height);

/**
 * sys_graphics_set_depth_range:  Set the depth range mapping.
 * Implements the high-level function graphics_set_depth_range(); the
 * caller guarantees that both parameters are within [-1,+1].
 *
 * [Parameters]
 *     near: Window coordinate of the near clipping plane.
 *     far: Window coordinate of the far clipping plane.
 */
extern void sys_graphics_set_depth_range(float near, float far);

/**
 * sys_graphics_set_blend:  Set the pixel blending operation and pixel
 * factors.  The caller guarantees that the parameters are valid enumerants
 * of the respective groups.
 *
 * [Parameters]
 *     operation: Blending operation (GRAPHICS_BLEND_ADD, etc.).
 *     src_factor: Source (new) pixel blending factor (GRAPHICS_BLEND_*).
 *     dest_factor: Destination (old) pixel blending factor (GRAPHICS_BLEND_*).
 * [Return value]
 *     True on success, false if the requested parameters cannot be set.
 */
extern int sys_graphics_set_blend(int operation, int src_factor, int dest_factor);

/**
 * sys_graphics_set_blend_alpha:  Set the pixel blending factors for the
 * alpha channel.  The caller guarantees that enable is either 0 or 1 and
 * the factor parameters are valid GRAPHICS_BLEND_* enumerants.
 *
 * [Parameters]
 *     enable: True to enable independent alpha blending, false to disable.
 *     src_factor: Source blending factor for alpha (GRAPHICS_BLEND_*).
 *     dest_factor: Destination blending factor for alpha (GRAPHICS_BLEND_*).
 * [Return value]
 *     True on success, false if the requested parameters cannot be set.
 */
extern int sys_graphics_set_blend_alpha(int enable,
                                        int src_factor, int dest_factor);

/**
 * sys_graphics_set_*_param:  Set a rendering parameter.  The caller will
 * ensure the value and parameter types match (booleans are passed as ints).
 *
 * [Parameters]
 *     id: Parameter ID.
 *     value: Value to set.  For booleans, a nonzero value is true and
 *         zero is false.
 */
extern void sys_graphics_set_int_param(SysGraphicsParam id, int value);
extern void sys_graphics_set_float_param(SysGraphicsParam id, float value);
extern void sys_graphics_set_vec2_param(SysGraphicsParam id,
                                        const struct Vector2f *value);
extern void sys_graphics_set_vec4_param(SysGraphicsParam id,
                                        const struct Vector4f *value);
extern void sys_graphics_set_matrix_param(SysGraphicsParam id,
                                          const struct Matrix4f *value);

/**
 * sys_graphics_get_matrix_param:  Retrieve the value of a matrix-type
 * rendering parameter.
 *
 * [Parameters]
 *     id: Parameter ID.
 *     value_ret: Pointer to variable into which to store parameter value.
 */
extern void sys_graphics_get_matrix_param(SysGraphicsParam id,
                                          struct Matrix4f *value_ret);

/**
 * sys_graphics_max_point_size:  Return the maximum point size supported
 * for point primitives.  Implements the high-level function
 * graphics_max_point_size().
 *
 * [Return value]
 *     Maximum point size, in pixels (must be at least 1).
 */
extern float sys_graphics_max_point_size(void);

/*------------------ Primitive creation and rendering -------------------*/

/**
 * sys_graphics_create_primitive:  Create a new primitive given the
 * primitive type and vertex list.  If immediate == 0, the vertex data
 * buffer will be copied, so it may be reused or destroyed after this
 * function returns.
 *
 * The "format" parameter describes the format of the vertex data, and
 * points to an array of uint32_t values generated by the
 * GRAPHICS_VERTEX_FORMAT() macro, terminated by a value of zero.  The
 * caller guarantees that at least a vertex position will be included.
 *
 * Optionally, a list of indices can be provided in index_data.  The size
 * of each index is specified by index_size as 1, 2, or 4, representing
 * unsigned 8-bit, 16-bit, or 32-bit integers respectively.
 *
 * If the "immediate" parameter is true, then the function may assume that
 * the primitive will be drawn and destroyed immediately without any other
 * intervening primitive calls.  In particular, the function may choose to
 * render the primitive immediately and ignore the following
 * sys_graphics_draw_primitive() call.  The passed-in buffers may also be
 * reused, so the caller must ensure they remain valid until
 * sys_graphics_draw_primitive() returns.
 *
 * [Parameters]
 *     type: Primitive type (GRAPHICS_PRIMITIVE_*, guaranteed valid).
 *     data: Vertex data pointer (guaranteed non-NULL).
 *     format: Vertex data format description (guaranteed non-NULL).
 *     size: Size of a single vertex, in bytes (guaranteed >0).
 *     count: Number of vertices (guaranteed >0).
 *     index_data: Index data pointer, or NULL if indices are not used.
 *     index_size: Size of an index value, in bytes (guaranteed valid if
 *         index_data != NULL).
 *     index_count: Number of indices (guaranteed >0 if index_data != NULL).
 *     immediate: Optimization hint: if true, this primitive will be drawn
 *         and destroyed immediately.
 * [Return value]
 *     New primitive, or NULL on error.
 */
extern SysPrimitive *sys_graphics_create_primitive(
    enum GraphicsPrimitiveType type, const void *data, const uint32_t *format,
    int size, int count, const void *index_data, int index_size,
    int index_count, int immediate);

/**
 * sys_graphics_draw_primitive:  Draw all or part of a previously-created
 * primitive.
 *
 * [Parameters]
 *     primitive: Primitive to draw (guaranteed non-NULL).
 *     start: Zero-based index of first vertex to draw (guaranteed
 *         nonnegative).
 *     count: Number of vertices to draw, or a negative value to draw all
 *         remaining vertices in the primitive.
 */
extern void sys_graphics_draw_primitive(SysPrimitive *primitive, int start,
                                        int count);

/**
 * sys_graphics_destroy_primitive:  Destroy a primitive.
 *
 * [Parameters]
 *     primitive: Primitive to destroy (guaranteed non-NULL).
 */
extern void sys_graphics_destroy_primitive(SysPrimitive *primitive);

/*---------------------- Shader generator control -----------------------*/

/**
 * sys_graphics_set_shader_generator:  Set the callback functions and
 * associated parameters to be used when generating shaders for primitive
 * rendering, and clears all existing shaders and custom uniforms/attributes.
 * Implements the high-level function graphics_set_shader_generator().
 *
 * The function pointer parameters to this function are declared as type
 * "void *" to avoid a dependency on the graphics.h header.  The
 * implementation must take care to call them using the proper signature.
 *
 * [Parameters]
 *     vertex_source_callback: Source code generation function to call
 *         for vertex shaders, or NULL to use the default generator.
 *     fragment_source_callback: Source code generation function to call
 *         for fragment shaders (guaranteed non-NULL if
 *         vertex_source_callback != NULL).
 *     key_callback: Key generation function to call (guaranteed non-NULL
 *         if vertex_source_callback != NULL).
 *     hash_table_size: Initial size of the shader hash table (guaranteed
 *         nonnegative, and guaranteed positive if dynamic_resize is true).
 *     dynamic_resize: True to allow dynamic resizing of the hash table,
 *         false to flush old shaders when the table is full.
 * [Return value]
 *     True on success, false on error or if the system does not support
 *     shaders.
 */
extern int sys_graphics_set_shader_generator(
    void *vertex_source_callback, void *fragment_source_callback,
    void *key_callback, int hash_table_size, int dynamic_resize);

/**
 * sys_graphics_add_shader_uniform:  Define a custom uniform to be used
 * with a shader generator.  Implements the high-level function
 * graphics_add_shader_uniform().
 *
 * [Parameters]
 *     name: Uniform name (guaranteed non-NULL and non-empty).
 * [Return value]
 *     Uniform identifier (nonzero), or zero on error.
 */
extern int sys_graphics_add_shader_uniform(const char *name);

/**
 * sys_graphics_add_shader_attribute:  Define a custom vertex attribute to
 * be used with a shader generator.  Implements the high-level function
 * graphics_add_shader_attribute().
 *
 * The returned attribute identifier must be in the inclusive range [1,4095].
 *
 * [Parameters]
 *     name: Attribute name (guaranteed non-NULL and non-empty).
 *     size: Number of values in attribute (1-4, guaranteed valid).
 * [Return value]
 *     Attribute identifier, or zero on error.
 */
extern int sys_graphics_add_shader_attribute(const char *name, int size);

/**
 * sys_graphics_set_shader_uniform_*:  Set the value of a custom uniform
 * for shaders created with a custom shader generator.  These implement the
 * corresponding high-level graphics_set_shader_uniform_*() functions.
 *
 * [Parameters]
 *     uniform: Uniform identifier (guaranteed nonzero).
 *     value: Value to set (pointer arguments are guaranteed non-NULL).
 */
extern void sys_graphics_set_shader_uniform_int(int uniform, int value);
extern void sys_graphics_set_shader_uniform_float(int uniform, float value);
extern void sys_graphics_set_shader_uniform_vec2(int uniform,
                                                 const struct Vector2f *value);
extern void sys_graphics_set_shader_uniform_vec3(int uniform,
                                                 const struct Vector3f *value);
extern void sys_graphics_set_shader_uniform_vec4(int uniform,
                                                 const struct Vector4f *value);
extern void sys_graphics_set_shader_uniform_mat4(int uniform,
                                                 const struct Matrix4f *value);

/*---------------------- Shader object management -----------------------*/

/**
 * SysShader:  Opaque type for system-specific shader data.
 */
typedef struct SysShader SysShader;

/**
 * SysShaderPipeline:  Opaque type for system-specific shader pipeline data.
 */
typedef struct SysShaderPipeline SysShaderPipeline;

/**
 * sys_graphics_enable_shader_objects:  Enable the use of shader objects
 * for primitive rendering.
 *
 * Note that shader objects are assumed to be disabled after
 * sys_graphics_init() is successfully called.
 *
 * [Return value]
 *     True on success, false on error or if the system does not support
 *     shader objects.
 */
extern int sys_graphics_enable_shader_objects(void);

/**
 * sys_graphics_disable_shader_objects:  Disable the use of shader objects
 * for primitive rendering, restoring the standard rendering pipeline.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_graphics_disable_shader_objects(void);

/**
 * sys_shader_background_compilation_supported:  Return whether the
 * system and the current display mode support compiling shader objects
 * on alternate threads.  Implements the high-level function
 * shader_background_compilation_supported().
 *
 * [Return value]
 *     True if background shader compilation is supported, false if not.
 */
extern PURE_FUNCTION int sys_shader_background_compilation_supported(void);

/**
 * sys_shader_enable_get_binary:  Set whether shader_get_binary() should
 * be supported for subsequently created shaders (if the system supports
 * it in the first place).  Implements the high-level function
 * shader_enable_get_binary().
 *
 * [Parameters]
 *     enable: True to enable shader_get_binary() on subsequent shaders,
 *         false for system-default behavior.
 */
extern void sys_shader_enable_get_binary(int enable);

/**
 * sys_shader_max_attributes:  Return the maximum number of vertex
 * attributes that can be used in a single shader.  Implements the
 * high-level function shader_max_attributes().
 *
 * [Return value]
 *     Maximum number of vertex attributes in a shader.
 */
extern PURE_FUNCTION int sys_shader_max_attributes(void);

/**
 * sys_shader_set_attribute:  Define a vertex shader attribute binding for
 * shader compilation.  Implements the high-level function
 * shader_set_attribute().
 *
 * The value passed to the index parameter is guaranteed to be nonnegative
 * and less than the value returned by sys_shader_max_attributes().  Note
 * that currently SIL has an internal limit of 256 attributes, so the index
 * will always be less than 256 even if sys_shader_max_attributes() returns
 * a greater value.
 *
 * This function is responsible for checking whether the requested name is
 * already bound to a different index and returning an error in that case.
 *
 * [Parameters]
 *     index: Attribute index (guaranteed to be nonnegative and less than
 *         the value returned by sys_shader_max_attributes()).
 *     name: Attribute name, or NULL to clear any existing binding.
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_shader_set_attribute(int index, const char *name);

/**
 * sys_shader_bind_standard_attribute:  Bind a standard vertex attribute to
 * a vertex shader attribute index.  Implements the high-level function
 * shader_bind_standard_attribute().
 *
 * No guarantees are made about the value of index; the implementation
 * should treat out-of-range values like negative values and set the
 * attribute to an unbound state.
 *
 * [Parameters]
 *     attribute: Standard attribute to bind (SHADER_ATTRIBUTE_*,
 *         guaranteed valid).
 *     index: Vertex shader attribute index to bind to, or negative to
 *         cancel any existing binding.
 */
extern void sys_shader_bind_standard_attribute(enum ShaderAttribute attribute,
                                               int index);

/**
 * sys_shader_clear_attributes:  Clear all vertex shader attributes.
 * Implements the high-level function shader_clear_attributes().
 */
extern void sys_shader_clear_attributes(void);

/**
 * sys_shader_create:  Create a new shader object from the given data.
 * If is_binary is true, data is a binary representation of the compiled
 * shader program, and size is its size in bytes; otherwise, data is a
 * string containing the shader source code, and size is its length (not
 * including the terminating null character) in bytes.  Implements the
 * high-level functions shader_create_from_source() and
 * shader_create_from_binary().
 *
 * [Parameters]
 *     type: Type of shader to create (SHADER_TYPE_*).
 *     data: Buffer containing shader source code or compiled shader
 *         program data (guaranteed non-NULL).
 *     size: Size of data, in bytes (guaranteed positive).
 *     is_binary: True if data is compiled shader program data; false if
 *         data is shader source code.
 * [Return value]
 *     New shader object, or NULL on error.
 */
extern SysShader *sys_shader_create(enum ShaderType type, const void *data,
                                    int size, int is_binary);

/**
 * sys_shader_destroy:  Destroy the given shader object.  Implements the
 * high-level function shader_destroy().
 *
 * [Parameters]
 *     shader: Shader object to destroy (guaranteed non-NULL).
 */
extern void sys_shader_destroy(SysShader *shader);

/**
 * sys_shader_get_binary:  Return compiled shader program data for the
 * given shader object which could be passed to sys_shader_create() to
 * recreate the same shader (assuming no change in system state).
 * Implements the high-level function shader_get_binary().
 *
 * [Parameters]
 *     shader: Shader object for which to retrieve compiled program data
 *         (guaranteed non-NULL).
 *     size_ret: Pointer to variable to receive the size of the data, in
 *         bytes (guaranteed non-NULL).
 * [Return value]
 *     Binary data representing the compiled shader program, or NULL on error.
 */
extern void *sys_shader_get_binary(SysShader *shader, int *size_ret);

/**
 * sys_shader_compile:  Compile the given shader source code and return the
 * compiled program data, without creating a persistent shader object.
 * Implements the high-level function shader_compile_to_binary().
 *
 * If sys_shader_background_compilation_supported() returns true, this
 * function might be called from any thread created with
 * sys_thread_create(), and the implementation must take care of any
 * necessary synchronization.
 *
 * [Parameters]
 *     type: Type of shader to create (SHADER_TYPE_*).
 *     source: String containing shader source code.
 *     length: Length of source, in bytes (guaranteed positive).
 *     size_ret: Pointer to variable to receive the size of the data, in bytes
 *         (guaranteed non-NULL).
 * [Return value]
 *     Binary data representing the compiled shader program, or NULL on error.
 */
extern void *sys_shader_compile(enum ShaderType type, const char *source,
                                int length, int *size_ret);

/**
 * sys_shader_get_uniform_id:  Return a value identifying the given uniform
 * in the given shader.  Implements the high-level function
 * shader_get_uniform_id.
 *
 * [Parameters]
 *     shader: Shader object.
 *     name: Name of uniform (guaranteed non-NULL and non-empty).
 * [Return value]
 *     Uniform ID to be used with subsequent sys_shader_uniform_*() calls,
 *         or zero if the uniform is not found.
 */
extern int sys_shader_get_uniform_id(SysShader *shader, const char *name);

/**
 * sys_shader_set_uniform_*:  Set the value of a shader uniform.  The data
 * type is assumed to match the type used in the shader.
 *
 * [Parameters]
 *     shader: Shader object.
 *     uniform: Uniform ID.  Guaranteed to be nonzero, but _not_ guaranteed
 *         to be valid for the given shader.
 *     value: Value to set (pointers are guaranteed non-NULL).
 */
extern void sys_shader_set_uniform_int(
    SysShader *shader, int uniform, int value);
extern void sys_shader_set_uniform_float(
    SysShader *shader, int uniform, float value);
extern void sys_shader_set_uniform_vec2(
    SysShader *shader, int uniform, const struct Vector2f *value);
extern void sys_shader_set_uniform_vec3(
    SysShader *shader, int uniform, const struct Vector3f *value);
extern void sys_shader_set_uniform_vec4(
    SysShader *shader, int uniform, const struct Vector4f *value);
extern void sys_shader_set_uniform_mat4(
    SysShader *shader, int uniform, const struct Matrix4f *value);

/**
 * sys_shader_pipeline_create:  Create a new shader pipeline.  Implements
 * the high-level function shader_pipeline_create().
 *
 * [Parameters]
 *     vertex_shader: Vertex shader to use in pipeline (guaranteed non-NULL).
 *     fragment_shader: Fragment shader to use in pipeline (guaranteed
 *         non-NULL).
 * [Return value]
 *     New shader pipeline object, or NULL on error.
 */
extern SysShaderPipeline *sys_shader_pipeline_create(
    SysShader *vertex_shader, SysShader *fragment_shader);

/**
 * sys_shader_pipeline_destroy:  Destroy a shader pipeline.  Implements
 * the high-level function shader_pipeline_destroy().
 *
 * [Parameters]
 *     pipeline: Shader pipeline to destroy (guaranteed non-NULL).
 */
extern void sys_shader_pipeline_destroy(SysShaderPipeline *pipeline);

/**
 * sys_shader_pipeline_apply:  Use the given shader pipeline for
 * subsequent draw operations.  Implements the high-level function
 * shader_pipeline_apply().
 *
 * [Parameters]
 *     pipeline: Ahader pipeline to apply, or NULL to remove the currently
 *         applied shader pipeline.
 */
extern void sys_shader_pipeline_apply(SysShaderPipeline *pipeline);

/*************************************************************************/
/************** Text rendering using a system-provided font **************/
/*************************************************************************/

/**
 * SysFont:  Opaque structure representing a system-provided font, used
 * with the sys_sysfont_*() functions.
 */
typedef struct SysFont SysFont;

/*-----------------------------------------------------------------------*/

/**
 * sys_sysfont_create:  Create a new reference to a system-provided font.
 *
 * The font name passed to this function can be interpreted as appropriate
 * for the system, except that the empty string should select a default
 * font (which must support at least the ASCII character set, U+0020
 * through U+007E).  If the requested font is not available, this function
 * should return NULL rather than attempting to find a "similar" font
 * (however, requests for the default font should always succeed unless
 * the system does not support fonts at all).
 *
 * The size parameter indicates the character size at which the font is
 * intended to be used, but operations on the font may use any size.
 * If the system is unable to scale the font to arbitrary sizes, render
 * operations should return an appropriate scale factor in the "scale_ret"
 * parameter to sys_sysfont_render(), which will cause the high-level code
 * to expand the rendered text to the appropriate size.
 *
 * [Parameters]
 *     name: System-dependent font name, or the empty string for the
 *         default font.
 *     size: Desired font size, in pixels.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     SysFont object for the font, or NULL on error.
 */
extern SysFont *sys_sysfont_create(const char *name, float size, int mem_flags);

/**
 * sys_sysfont_destroy:  Free resources used by a SysFont object.
 *
 * [Parameters]
 *     font: SysFont object to destroy (guaranteed non-NULL).
 */
extern void sys_sysfont_destroy(SysFont *font);

/**
 * sys_sysfont_native_size:  Return the "native" size of the given font,
 * i.e., the size at which the font is designed to be rendered.  In the
 * case of a vector or similar font which can be rendered equally well at
 * any size, the "native" size is zero.
 *
 * [Parameters]
 *     font: Font reference (guaranteed non-NULL).
 * [Return value]
 *     Native size of font, in pixels, or zero if none.
 */
extern int sys_sysfont_native_size(SysFont *font);

/**
 * sys_sysfont_get_metrics:  Return font size metrics for text drawn at
 * the given font size.  The values returned are defined as for the
 * high-level functions font_height(), font_baseline(), font_ascent(),
 * and font_descent().
 *
 * [Parameters]
 *     font: Font reference (guaranteed non-NULL).
 *     size: Font size, in pixels (guaranteed >0).
 *     height_ret: Pointer to variable to receive the line height, in pixels
 *         (guaranteed non-NULL).
 *     baseline_ret: Pointer to variable to receive the baseline offset, in
 *         pixels (guaranteed non-NULL).
 *     ascent_ret: Pointer to variable to receive the font ascent, in pixels
 *         (guaranteed non-NULL).
 *     descent_ret: Pointer to variable to receive the font descent, in pixels
 *         (guaranteed non-NULL).
 */
extern void sys_sysfont_get_metrics(
    SysFont *font, float size, float *height_ret, float *baseline_ret,
    float *ascent_ret, float *descent_ret);

/**
 * sys_sysfont_char_advance:  Return the horizontal advance for the given
 * character at the given font size.  Implements the high-level function
 * font_char_advance() for system-provided fonts.
 *
 * [Parameters]
 *     font: Font reference (guaranteed non-NULL).
 *     ch: Unicode character (codepoint).
 *     size: Font size, in pixels (guaranteed >0).
 * [Return value]
 *     Horizontal advance, in pixels.
 */
extern float sys_sysfont_char_advance(SysFont *font, int32_t ch, float size);

/**
 * sys_sysfont_text_advance:  Return the horizontal advance for the given
 * text string at the given font size.  Implements the high-level function
 * font_text_advance() for system-provided fonts.
 *
 * [Parameters]
 *     font: Font reference (guaranteed non-NULL).
 *     str: UTF-8 text string (guaranteed non-NULL).
 *     size: Font size, in pixels (guaranteed >0).
 * [Return value]
 *     Horizontal advance, in pixels.
 */
extern float sys_sysfont_text_advance(SysFont *font, const char *str, float size);

/**
 * sys_sysfont_get_text_bounds:  Return the horizontal bounds of the given
 * text string as it would be rendered at the given font size.
 *
 * [Parameters]
 *     font: Font reference (guaranteed non-NULL).
 *     str: UTF-8 text string (guaranteed non-NULL).
 *     size: Font size, in pixels (guaranteed >0).
 *     left_ret: Pointer to variable to receive the distance from the origin
 *         to the left edge of the text, in pixels (guaranteed non-NULL).
 *     right_ret: Pointer to variable to receive the distance from the origin
 *         to the right edge of the text, in pixels (guaranteed non-NULL).
 */
extern void sys_sysfont_get_text_bounds(
    SysFont *font, const char *str, float size, float *left_ret,
    float *right_ret);

/**
 * sys_sysfont_render:  Render the given text to a texture.
 *
 * [Parameters]
 *     font: Font reference (guaranteed non-NULL).
 *     str: UTF-8 text string (guaranteed non-NULL).
 *     size: Font size, in pixels (guaranteed >0).
 *     origin_x_ret: Pointer to variable to receive the texture X coordinate
 *         of the text origin, in pixels (guaranteed non-NULL).
 *     origin_y_ret: Pointer to variable to receive the texture Y coordinate
 *         of the text origin, in pixels (guaranteed non-NULL).
 *     advance_ret: Pointer to variable to receive the horizontal advance
 *         from the text origin, in pixels (guaranteed non-NULL).
 *     scale_ret: Pointer to variable to receive the scale factor at which
 *         the texture should be drawn (guaranteed non-NULL).
 * [Return value]
 *     Newly created texture, or NULL on error.
 */
extern struct SysTexture *sys_sysfont_render(
    SysFont *text, const char *str, float size, float *origin_x_ret,
    float *origin_y_ret, float *advance_ret, float *scale_ret);

/*************************************************************************/
/************************* Input device handling *************************/
/*************************************************************************/

/**
 * SysInputJoystick:  Information about a single joystick.
 */
typedef struct SysInputJoystick SysInputJoystick;
struct SysInputJoystick {
    /* Is the joystick at this index connected? */
    uint8_t connected;

    /* Does this joystick support rumble (force feedback)? */
    uint8_t can_rumble;

    /* Number of buttons on the joystick (counting from zero, so that the
     * highest valid butten number is num_buttons-1). */
    int num_buttons;

    /* Number of sticks on the joystick (counting from zero). */
    int num_sticks;
};

/**
 * SysInputInfo:  Information about available input devices on the system.
 */
typedef struct SysInputInfo SysInputInfo;
struct SysInputInfo {
    /* Is a joystick available? */
    uint8_t has_joystick;

    /* Number of joystick devices available (whether a joystick is actually
     * connected or not). */
    int num_joysticks;

    /* Array of num_joysticks SysInputJoystick structures describing the
     * individual joystick devices.  The array pointed to by this field
     * is guaranteed to remain valid until the next sys_input_info() or
     * sys_input_cleanup() call. */
    const SysInputJoystick *joysticks;

    /*------------------------------*/

    /* Is a keyboard, keypad, or similar key-based input device available? */
    uint8_t has_keyboard;

    /* Is the key-based input device a "full" keyboard suitable for text
     * entry? */
    uint8_t keyboard_is_full;

    /*------------------------------*/

    /* Is a mouse available? */
    uint8_t has_mouse;

    /*------------------------------*/

    /* Is a generic text entry interface (such as a software keyboard)
     * available?  Systems with true keyboards are _not_ required to
     * implement this interface, though it is recommended to ensure proper
     * translation of locale-specific keys and key combinations.  If a
     * keyboard is available and this field is set to zero, a default
     * translation table will be used to provide text entry services via
     * the keyboard. */
    uint8_t has_text;

    /* Does the text entry functionality use its own display interface? */
    uint8_t text_uses_custom_interface;

    /* If the text entry functionality has a custom display interface,
     * does that interface support displaying a prompt string? */
    uint8_t text_has_prompt;

    /*------------------------------*/

    /* Is a touch interface available? */
    uint8_t has_touch;
};

/*------------------------- Basic functionality -------------------------*/

/**
 * sys_input_init:  Initialize input device handling functionality.  The
 * event callback function passed in as event_callback will receive input
 * events in the format defined in input.h.
 *
 * Note that event_callback may be called on an independent thread, so the
 * callback must properly lock any data it shares with the rest of the
 * library.  On some platforms, the function will be called on the same
 * thread that handles global UI processing, so the function should not
 * perform any time-consuming operations.
 *
 * Note for implementors: The callback will fill in the following event
 * fields on its own, so the system-level callback may ignore them.
 *    - For keyboard events: the modifier field.
 *    - For touch events: the initial_x and initial_y fields.
 *
 * [Parameters]
 *     event_callback: Pointer to a callback function to receive input events.
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_input_init(void (*event_callback)(const struct InputEvent *));

/**
 * sys_input_cleanup:  Shut down input device handling functionality.
 */
extern void sys_input_cleanup(void);

/**
 * sys_input_update:  Perform any periodic processing required to update
 * the current state of all input devices.  This routine is permitted (but
 * not required) to store input device state for returning from the other
 * sys_input_*() functions; the only requirement is that the return data
 * be no older than the last call to sys_input_update().
 */
extern void sys_input_update(void);

/**
 * sys_input_info:  Return information about available input devices.
 * This information may change if input devices are added or removed while
 * the program is running.
 *
 * [Parameters]
 *     info_ret: Pointer to structure to receive input device information.
 */
extern void sys_input_info(SysInputInfo *info_ret);

/**
 * sys_input_grab:  Activate or deactivate input grabbing.  Implements the
 * high-level function input_grab().
 *
 * [Parameters]
 *     grab: True to grab input, false to ungrab input.
 */
extern void sys_input_grab(int grab);

/**
 * sys_input_is_quit_requested:  Return whether a system-specific quit
 * request has been received.  Implements the high-level function
 * input_is_quit_requested().
 *
 * [Return value]
 *     True if a quit request has been received, false if not.
 */
extern int sys_input_is_quit_requested(void);

/**
 * sys_input_is_suspend_requested:  Return whether a system-specific
 * suspend request has been received.  Implements the high-level function
 * input_is_suspend_requested().
 *
 * [Return value]
 *     True if a suspend request has been received, false if not.
 */
extern int sys_input_is_suspend_requested(void);

/**
 * sys_input_acknowledge_suspend_request:  Acknowledge a suspend request
 * from the system, blocking until the system resumes from suspend.
 * Implements the high-level function input_acknowledge_suspend_request().
 */
extern void sys_input_acknowledge_suspend_request(void);

/*-------------------------- Joystick handling --------------------------*/

/**
 * sys_input_enable_unfocused_joystick:  Set whether or not to send
 * joystick input events while the application does not have input focus.
 * Implements the high-level function input_enable_unfocused_joystick().
 *
 * Core SIL code will not call this function with a true argument during
 * initialization; system-dependent code is responsible for setting the
 * default state of accepting joystick events regardless of input focus.
 *
 * On platforms which do not have the concept of input focus, this routine
 * can be a no-op.
 *
 * [Parameters]
 *     enable: True to send joystick input events while the window does
 *         not have focus, false to suppress them.
 */
extern void sys_input_enable_unfocused_joystick(int enable);

/**
 * sys_input_joystick_copy_name:  Return the name of the given joystick,
 * if available, in a newly allocated buffer.  Implements the high-level
 * function input_joystick_copy_name().
 *
 * [Parameters]
 *     index: Joystick index (0 <= index < SysInputInfo.num_joysticks as
 *         of the last call to sys_input_info()).
 * [Return value]
 *     Name of the joystick (in a newly allocated buffer), or NULL if the
 *     joystick's name is unavailable.
 */
extern char *sys_input_joystick_copy_name(int index);

/**
 * sys_input_joystick_button_mapping:  Return the joystick button, if any,
 * which corresponds to the given button name.  Implements the high-level
 * function input_joystick_button_mapping().
 *
 * [Parameters]
 *     index: Joystick index (0 <= index < SysInputInfo.num_joysticks as
 *         of the last call to sys_input_info()).
 *     name: Button name (INPUT_JOYBUTTON_*, guaranteed to be valid).
 * [Return value]
 *     Corresponding button number, or -1 if there is no corresponding button.
 */
extern int sys_input_joystick_button_mapping(int index, int name);

/**
 * sys_input_joystick_rumble:  Send a "rumble" (force feedback) effect to
 * the given joystick.  Implements the high-level function
 * input_joystick_rumble().
 *
 * [Parameters]
 *     index: Joystick index (0 <= index < SysInputInfo.num_joysticks as
 *         of the last call to sys_input_info()).
 *     left: Left motor intensity (0 = off, 1 = full intensity).
 *     right: Right motor intensity (0 = off, 1 = full intensity).
 *     time: Duration of effect, in seconds (non-negative).
 */
extern void sys_input_joystick_rumble(int index, float left, float right,
                                      float time);

/*--------------------------- Mouse handling ----------------------------*/

/**
 * sys_input_mouse_set_position:  Set the position of the mouse pointer, if
 * possible.  Implements the high-level function input_mouse_set_position().
 *
 * [Parameters]
 *     x, y: New mouse pointer position.
 */
extern void sys_input_mouse_set_position(float x, float y);

/*------------------------- Text entry handling -------------------------*/

/**
 * sys_input_text_set_state:  Set the state of the text entry interface
 * (active or hidden).
 *
 * [Parameters]
 *     on: True to show the interface, false to hide it.
 *     text: Default text (NULL for no default).
 *     prompt: Prompt string to use when showing the interface (NULL for none).
 */
extern void sys_input_text_set_state(int on, const char *text,
                                     const char *prompt);

/*************************************************************************/
/******************************** Logging ********************************/
/*************************************************************************/

/**
 * sys_log_open:  Open a log file with the given name.  The directory in
 * which the file is created is system-dependent.  If the file already
 * exists, it is truncated to zero size.
 *
 * The returned pointer is suitable only for passing as the logfile
 * parameter to sys_log_write() or sys_log_close().
 *
 * [Parameters]
 *     name: Name of log file (guaranteed non-NULL).
 * [Return value]
 *     Log file handle, or NULL on error.
 */
extern void *sys_log_open(const char *name);

/**
 * sys_log_write:  Output a log message.
 *
 * The caller guarantees that message[len] is a newline ('\n') character.
 *
 * [Parameters]
 *     logfile: Log file handle to which to write the message, or NULL for
 *         the default log message target.
 *     message: Message to output (guaranteed non-NULL).
 *     len: Length of message, in bytes (excluding the trailing newline;
 *         guaranteed >=0).
 */
extern void sys_log_write(void *logfile, const char *message, int len);

/**
 * sys_log_close:  Close the given log file.
 *
 * [Parameters]
 *     logfile: Handle of log file to close (guaranteed non-NULL).
 */
extern void sys_log_close(void *logfile);

/*************************************************************************/
/*************************** Memory management ***************************/
/*************************************************************************/

/* These functions are only declared globally if a custom implementation is
 * in use (signalled by SIL_MEMORY_CUSTOM being defined).  Otherwise, the
 * declarations are limited to memory.c, but the declarations are kept here
 * to ensure consistency between the default implementation and custom
 * implementations. */

#if defined(SIL_MEMORY_CUSTOM) || defined(SIL__IN_MEMORY_C)

# ifndef SIL_MEMORY_CUSTOM
#  define extern  static inline  // Defined within memory.c.
# endif

/**
 * sys_mem_alloc:  Allocate and return a block of memory.
 *
 * The caller guarantees that size is nonzero, align is either zero or a
 * power of two, align is no greater than sys_max_mem_align(), and flags
 * does not contain MEM_ALLOC_CLEAR.
 *
 * [Parameters]
 *     size: Size of block to allocate, in bytes.
 *     align: Desired alignment of block, in bytes (always a power of 2),
 *         or zero for the system default.
 *     flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     Pointer to allocated block, or NULL on error.
 */
extern void *sys_mem_alloc(int64_t size, int align, int flags);

/**
 * sys_mem_free:  Free a block of memory previously allocated with
 * sys_mem_alloc() or sys_mem_realloc().
 *
 * The caller guarantees that ptr is non-NULL.
 *
 * [Parameters]
 *     ptr: Pointer to block to free.
 */
extern void sys_mem_free(void *ptr);

/**
 * sys_mem_realloc:  Resize a block of memory previously allocated with
 * sys_mem_alloc() or sys_mem_realloc().
 *
 * The caller guarantees that ptr is non-NULL, size is nonzero, align is
 * equal to the alignment used when the block was allocated, and flags
 * does not contain MEM_ALLOC_CLEAR.
 *
 * [Parameters]
 *     ptr: Pointer to block to resize.
 *     size: New size for block, in bytes.
 *     align: Desired alignment of block, in bytes (always a power of 2),
 *         or zero for the system default.
 *     flags: Memory allocation flags (MEM_ALLOC_*).
 * [Return value]
 *     Pointer to resized block, or NULL on error.
 */
extern void *sys_mem_realloc(void *ptr, int64_t size, int align, int flags);

/**
 * sys_mem_avail:  Return the amount of available memory.  Implements
 * mem_avail().
 *
 * [Return value]
 *     Total amount of memory available, in bytes.
 */
extern int64_t sys_mem_avail(int flags);

/**
 * sys_mem_contig:  Return the size of the largest single allocatable block.
 * Implements mem_contig().
 *
 * [Return value]
 *     Maximum allocatable block size, in bytes.
 */
extern int64_t sys_mem_contig(int flags);

/**
 * sys_mem_max_align:  Return the maximum alignment value that can be used
 * with sys_mem_alloc().  This value must be constant over the life of the
 * program, and must be no less than sizeof(void *).
 *
 * [Return value]
 *     Maximum supported alignment, in bytes.
 */
extern CONST_FUNCTION int sys_mem_max_align(void);

# undef extern

#endif  // SIL_MEMORY_CUSTOM || SIL__IN_MEMORY_C

/*************************************************************************/
/****************** Movie (synced video/audio) playback ******************/
/*************************************************************************/

/**
 * SysMovieHandle:  Opaque handle type for a movie object.
 */
typedef struct SysMovieHandle SysMovieHandle;

/*-----------------------------------------------------------------------*/

/**
 * sys_movie_open:  Create a movie handle for a movie resource.  The
 * file handle is owned by the movie handle on success, automatically
 * closed on error.
 *
 * [Parameters]
 *     fh: File handle from which to read data (guaranteed non-NULL).
 *     offset: Offset of movie data within file, in bytes.
 *     length: Size of movie data, in bytes.
 *     smooth_chroma: For formats with subsampled chroma data, true to
 *         linearly interpolate the U and V planes, false to use point
 *         sampling.  (This value can be passed as the smooth_uv parameter
 *         to the utility function yuv2rgb() to get the correct behavior.)
 * [Return value]
 *     New movie handle, or NULL on error.
 */
extern SysMovieHandle *sys_movie_open(struct SysFile *fh, int64_t offset,
                                      int length, int smooth_chroma);

/**
 * sys_movie_close:  Close the given movie handle, releasing all resources
 * used by the movie.
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 */
extern void sys_movie_close(SysMovieHandle *movie);

/**
 * sys_movie_width, sys_movie_height:  Return the width or height of a
 * movie's video stream.
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 * [Return value]
 *     Video width or height, in pixels.
 */
extern int sys_movie_width(SysMovieHandle *movie);
extern int sys_movie_height(SysMovieHandle *movie);

/**
 * sys_movie_framerate:  Return the frame rate for a movie's video stream.
 * Implements movie_framerate().
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 * [Return value]
 *     Video frame rate, in frames per second, or zero if unknown.
 */
extern double sys_movie_framerate(SysMovieHandle *movie);

/**
 * sys_movie_set_volume:  Set the audio playback volume for a movie.
 * Implements movie_set_volume().
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 *     volume: Audio volume (0...∞, 0 = silent, 1 = as recorded).
 */
void sys_movie_set_volume(SysMovieHandle *movie, float volume);

/**
 * sys_movie_play:  Start playback of a movie.  Implements movie_play().
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_movie_play(SysMovieHandle *movie);

/**
 * sys_movie_stop:  Stop playback of a movie.  Implements movie_stop().
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 */
extern void sys_movie_stop(SysMovieHandle *movie);

/**
 * sys_movie_get_texture:  Return the ID of a texture containing the video
 * image.  The texture ID must be constant for a given movie, but the
 * coordinates may change after each call to sys_movie_draw_frame().
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 *     left_ret: Pointer to variable to receive the U coordinate of the
 *         left edge of the image (guaranteed non-NULL).
 *     right_ret: Pointer to variable to receive the U coordinate of the
 *         right edge of the image (guaranteed non-NULL).
 *     top_ret: Pointer to variable to receive the V coordinate of the
 *         top edge of the image (guaranteed non-NULL).
 *     bottom_ret: Pointer to variable to receive the V coordinate of the
 *         bottom edge of the image (guaranteed non-NULL).
 * [Return value]
 *     Texture ID.
 */
extern int sys_movie_get_texture(
    SysMovieHandle *movie,
    float *left_ret, float *right_ret, float *top_ret, float *bottom_ret);

/**
 * sys_movie_draw_frame:  Draw the next frame of the movie to the movie's
 * texture.
 *
 * [Parameters]
 *     movie: Movie handle (guaranteed non-NULL).
 * [Return value]
 *     True if a frame was drawn, false if the end of the stream has been
 *     reached.
 */
extern int sys_movie_draw_frame(SysMovieHandle *movie);

/*************************************************************************/
/**************************** Sound playback *****************************/
/*************************************************************************/

/**
 * sys_sound_init:  Initialize the audio output functionality, and allocate
 * an array of playback channels.  The channel IDs (arbitrary unique
 * integers) to be used in calling other sys_sound_*() functions are stored
 * consecutively in the array pointed to by "channels_ret".
 *
 * This function may only be called while the audio output functionality
 * is not initialized: that is, either as the first sys_sound_*() call
 * after program startup or after a sys_sound_cleanup() call.
 *
 * [Parameters]
 *     device_name: System-dependent device name for audio output, or the
 *         empty string to request the default output device (guaranteed
 *         non-NULL).
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_sound_init(const char *device_name);

/**
 * sys_sound_playback_rate:  Return the system's sampling rate for audio
 * playback.  The mixer will generate data at this rate, resampling input
 * audio streams as necessary.
 *
 * The value returned by this function must remain constant between a call
 * to sys_sound_init() and the corresponding call to sys_sound_cleanup().
 *
 * [Return value]
 *     Playback sampling rate, in Hz.
 */
extern int sys_sound_playback_rate(void);

/**
 * sys_sound_set_latency:  Optionally set the desired output audio latency,
 * then return the current latency.  Implements the high-level functions
 * sound_get_latency() and sound_set_latency().
 *
 * [Parameters]
 *     latency: Requested audio point latency, in seconds, or zero to leave
 *         the current latency unchanged (guaranteed nonnegative).
 * [Return value]
 *     Current output audio latency, in seconds (must be positive).
 */
extern float sys_sound_set_latency(float latency);

/**
 * sys_sound_enable_headphone_disconnect_check:  Enable detection of
 * headphone disconnect events.  Implements the high-level function
 * sound_enable_headphone_disconnect_check().
 *
 * Note that system-level code is responsible for performing the muting
 * described in the sound_enable_headphone_disconnect_check() documentation;
 * the core functions simply wrap these functions.
 *
 * [Parameters]
 *     enable: True to enable headphone disconnect detection, false to disable.
 */
extern void sys_sound_enable_headphone_disconnect_check(int enable);

/**
 * sys_sound_check_headphone_disconnect:  Return true if the a headphone
 * disconnect event has been detected.  Implements the high-level function
 * sound_check_headphone_disconnect().
 *
 * If the system does not support headphone disconnect detection, this
 * function should always return false.
 *
 * [Return value]
 *     True if a headphone disconnect has been detected, false if not.
 */
extern int sys_sound_check_headphone_disconnect(void);

/**
 * sys_sound_acknowledge_headphone_disconnect:  Reset the headphone-
 * disconnect flag.  Implements the high-level function
 * sound_acknowledge_headphone_disconnect().
 */
extern void sys_sound_acknowledge_headphone_disconnect(void);

/**
 * sys_sound_cleanup:  Shut down the audio output functionality.
 */
extern void sys_sound_cleanup(void);

/*************************************************************************/
/*************************** Texture handling ****************************/
/*************************************************************************/

/**
 * SysTexture:  Opaque type for system-specific texture data.
 */
typedef struct SysTexture SysTexture;

/**
 * SysTextureLockMode (SYS_TEXTURE_LOCK_*):  Values for the lock_mode
 * parameter to sys_texture_lock().
 */
enum SysTextureLockMode {
    /* Return an array of 32-bit RGBA pixels. */
    SYS_TEXTURE_LOCK_NORMAL = 1,

    /* Return an uninitialized buffer for 32-bit RGBA pixels. */
    SYS_TEXTURE_LOCK_DISCARD,
};
typedef enum SysTextureLockMode SysTextureLockMode;

/* Invalid SysTextureLockMode value that can be used as a placeholder.
 * Defined outside the enum so we don't get warnings about it for -Wswitch. */
#define SYS_TEXTURE_LOCK_INVALID ((SysTextureLockMode)0)

/*-------------------- Texture creation and deletion --------------------*/

/**
 * sys_texture_create:  Create a new SysTexture object, and fill it with
 * the provided image data, if any.
 *
 * If mipmaps != 0, the image data provided is used only for the first
 * (full-size) level, and any subsequent mipmap levels in the data are
 * ignored.  Otherwise, if num_levels > 1, each mipmap level is assumed to
 * have a size of lbound(w/2,1) x lbound(h/2,1) where w and h are the size
 * of the preceding level; thus, for example, a 7x3 texture has two mipmap
 * levels, of size 3x1 and 1x1.
 *
 * If reuse != 0, then the memory region pointed to by data (if data != NULL)
 * must have been allocated with mem_alloc().  On success, the data buffer
 * will be managed by the SysTexture object (or freed if it can't feasibly
 * be reused); on error, this function will free the data buffer.
 *
 * If reuse == 0, the memory region pointed to by data will not be modified.
 *
 * [Parameters]
 *     width: Texture width, in pixels (guaranteed >=1).
 *     height: Texture height, in pixels (guaranteed >=1).
 *     data_format: Image data format (one of TEX_FORMAT_* from
 *         utility/tex-file.h).
 *     num_levels: Number of mipmap levels (including the base image).
 *         Zero indicates that the texture should be created empty.
 *     data: Pointer to image data.  Ignored if num_levels == 0.
 *     stride: Line size of image data for base level, in pixels.  Ignored
 *         if num_levels == 0 or if the image data format is a compressed
 *         format.
 *     level_offsets: Byte offsets into data for each image level;
 *         level_offsets[0] points to the main image data and includes the
 *         palette data for paletted textures.  Ignored if num_levels == 0.
 *     level_sizes: Size of data, in bytes, for each image level.  Ignored
 *         if num_levels == 0.
 *     mipmaps: True to enable automatic mipmap generation for this texture
 *         (if applicable to the system); false to prevent mipmap generation.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*).
 *     reuse: True to reuse the image data, or free it if reuse is not
 *         possible; false to copy the image data.
 * [Return value]
 *     New texture, or NULL on error.
 */
extern SysTexture *sys_texture_create(
    int width, int height, enum TextureFormat data_format, int num_levels,
    void *data, int stride, const int32_t *level_offsets,
    const int32_t *level_sizes, int mipmaps, int mem_flags, int reuse);

/**
 * sys_texture_destroy:  Destroy a SysTexture object.
 *
 * [Parameters]
 *     texture: Texture to destroy (guaranteed non-NULL and not locked).
 */
extern void sys_texture_destroy(SysTexture *texture);

/*-------------------- Texture information retrieval --------------------*/

/**
 * sys_texture_width, sys_texture_height:  Return the width or height of
 * the given texture.
 *
 * [Parameters]
 *     texture: Texture for which to retrieve size.
 * [Return value]
 *     Width or height of the texture, in pixels.
 */
extern int sys_texture_width(SysTexture *texture);
extern int sys_texture_height(SysTexture *texture);

/**
 * texture_has_mipmaps:  Return whether the given texture has stored
 * mipmaps in addition to the base image.
 *
 * [Parameters]
 *     texture: Texture for which to retrieve mipmap state.
 * [Return value]
 *     True if the texture has stored mipmaps, false if not.
 */
extern int sys_texture_has_mipmaps(SysTexture *texture);

/*----------------------- Pixel data manipulation -----------------------*/

/**
 * sys_texture_grab:  Create a new texture containing a portion of the
 * display or currently bound framebuffer.  Implements the high-level
 * function texture_create_from_display().
 *
 * [Parameters]
 *     x, y: Base display coordinates of region to copy, in pixels (might
 *         be negative).
 *     w, h: Size of region to copy, in pixels (guaranteed positive, and
 *         guaranteed no larger than the texture size if texture != NULL).
 *     readable: False if the texture is not required to be readable.
 *     mipmaps: Whether or not to generate mipmaps for a newly created
 *         texture (like the mipmaps parameter to sys_texture_create()).
 *         Ignored if texture != NULL.
 *     mem_flags: Memory allocation flags (MEM_ALLOC_*), used when
 *         texture == NULL.
 * [Return value]
 *     Texture containing display data, or NULL on error.
 */
extern SysTexture *sys_texture_grab(
    int x, int y, int w, int h, int readable, int mipmaps, int mem_flags);

/**
 * sys_texture_lock:  Lock a texture's image data into memory, and return
 * a pointer to all or some of the texture's pixel data.  Implements the
 * high-level texture_lock() family of functions.  The caller guarantees
 * that the texture is not already locked.
 *
 * For SYS_TEXTURE_LOCK_RAW, the position and size coordinates are ignored.
 *
 * [Parameters]
 *     texture: Texture to lock.
 *     lock_mode: Lock mode (SYS_TEXTURE_LOCK_*).
 *     x, y: Base texture coordinates of region to lock, in pixels.
 *     w, h: Size of region to lock, in pixels.
 * [Return value]
 *     Pointer to pixel data, or NULL on error.
 */
extern void *sys_texture_lock(SysTexture *texture,
                              SysTextureLockMode lock_mode,
                              int x, int y, int w, int h);

/**
 * sys_texture_unlock:  Unlock a texture locked with sys_texture_lock().
 * If update != 0, the texture is updated from the data in the pixel data
 * buffer returned by sys_texture_lock().  The caller guarantees that the
 * texture is currently locked, and that if update != 0, the entire texture
 * buffer (not a subregion) was locked by the preceding lock operation.
 *
 * [Parameters]
 *     texture: Texture to unlock.
 *     update: True to copy the pixel data into the texture; false to
 *         leave the texture data as is.
 */
extern void sys_texture_unlock(SysTexture *texture, int update);

/**
 * sys_texture_flush:  Assume that texture data stored in memory may have
 * been modified, and ensure such modifications are visible to the graphics
 * hardware.  This function is called by texture_unlock() when unlocking
 * after a texture_lock_raw() call, and will never be called while the
 * texture is locked with sys_texture_lock().
 *
 * [Parameters]
 *     texture: Texture to flush.
 */
extern void sys_texture_flush(SysTexture *texture);

/*-------------------------- Rendering control --------------------------*/

/**
 * sys_texture_set_repeat:  Set the given texture's coordinate wrap flags.
 *
 * [Parameters]
 *     texture: Texture to modify.
 *     repeat_u: True to repeat horizontally, false to clamp.
 *     repeat_v: True to repeat vertically, false to clamp.
 */
extern void sys_texture_set_repeat(SysTexture *texture, int repeat_u, int repeat_v);

/**
 * sys_texture_set_antialias:  Set the given texture's antialiasing flag.
 *
 * [Parameters]
 *     texture: Texture to modify.
 *     on: True to enable antialiasing, false to disable.
 */
extern void sys_texture_set_antialias(SysTexture *texture, int on);

/**
 * sys_texture_apply:  Set the given texture as the texture to be used for
 * subsequent rendering operations.
 *
 * [Parameters]
 *     unit: Index of texture unit to modify (guaranteed nonnegative).
 *     texture: Texture to set, or NULL to clear any previously set texture.
 */
extern void sys_texture_apply(int unit, SysTexture *texture);

/**
 * sys_texture_num_units:  Return the number of texture units available.
 * The value returned must be at least 1.
 *
 * [Return value]
 *     Number of texture units available.
 */
extern int sys_texture_num_units(void);

/*************************************************************************/
/***************** Thread management and synchronization *****************/
/*************************************************************************/

/*----------------------------- Data types ------------------------------*/

/**
 * SysThreadID:  Thread handle type.  Systems which use a numeric system
 * handle rather than a pointer may cast the numeric value to this type
 * rather than allocating a new handle (and similarly for the other types
 * below).
 */
typedef void *SysThreadID;

/**
 * SysCondVarID:  Condition variable type.
 */
typedef void *SysCondVarID;

/**
 * SysMutexID:  Mutex (mutual-exclusion primitive) type.
 */
typedef void *SysMutexID;

/**
 * SysSemaphoreID:  Semaphore type.
 */
typedef void *SysSemaphoreID;

/*--------------------------- Thread routines ---------------------------*/

/**
 * sys_thread_get_num_cores:  Return the number of processing cores
 * (logical CPUs) available for threads.  Implements the high-level
 * function thread_get_num_cores().
 *
 * [Return value]
 *     Number of processing cores available (must be at least 1).
 */
extern int sys_thread_get_num_cores(void);

/**
 * sys_thread_create:  Create and start a thread executing the given
 * function with the given attributes.  Implements the high-level
 * functions thread_create(), thread_create_with_priority(), and
 * thread_create_with_attr().
 *
 * [Parameters]
 *     attr: Thread attributes (guaranteed non-NULL, and attr->stack_size
 *         is guaranteed nonnegative; note that attr->affinity is _not_
 *         sanitized).
 *     function: Function to execute (guaranteed non-NULL).
 *     param: Arbitrary parameter to pass to the function.
 * [Return value]
 *     New thread handle, or zero on error.
 */
extern SysThreadID sys_thread_create(
    const struct ThreadAttributes *attr, int (*function)(void *), void *param);

/**
 * sys_thread_exit:  Terminate the current thread.  Implements the
 * high-level function thread_exit().
 *
 * [Parameters]
 *     exit_code: Value to return via thread_wait() as the thread's exit code.
 */
extern NORETURN void sys_thread_exit(int exit_code);

/**
 * sys_thread_get_id:  Return the ID (handle) of the current thread, or 0
 * if the current thread was not created with sys_thread_create().
 * Implements the high-level function thread_get_id().
 *
 * [Return value]
 *     Handle of the current thread.
 */
extern SysThreadID sys_thread_get_id(void);

/**
 * sys_thread_get_priority:  Return the priority of the current thread, or
 * 0 if the current thread was not created with sys_thread_create().
 * Implements the high-level function thread_get_priority().
 *
 * [Return value]
 *     Priority of the current thread.
 */
extern int sys_thread_get_priority(void);

/**
 * sys_thread_set_affinity:  Modify the set of processing cores on which
 * the current thread should run.  Implements the high-level function
 * thread_set_affinity().
 *
 * [Parameters]
 *     affinity: Affinity mask (guaranteed to only contain set bits for
 *         cores with index 0 <= n < sys_thread_get_num_cores()).
 * [Return value]
 *     True if the affinity mask was set; false on error.
 */
extern int sys_thread_set_affinity(uint64_t affinity);

/**
 * sys_thread_get_affinity:  Return the set of processing cores on which
 * the current thread will run.  Implements the high-level function
 * thread_get_affinity().
 *
 * [Return value]
 *     Current affinity mask.
 */
extern uint64_t sys_thread_get_affinity(void);

/**
 * sys_thread_is_running:  Return whether the given thread is still
 * running.  Implements the high-level function thread_is_running().
 *
 * The caller guarantees that no other thread will wait on the given thread
 * while this function is being called.
 *
 * [Parameters]
 *     thread: Handle of thread to check (guaranteed nonzero).
 * [Return value]
 *     True if the thread is still running, false if the thread has terminated.
 */
extern int sys_thread_is_running(SysThreadID thread);

/**
 * sys_thread_wait:  Wait for the given thread to terminate, and return
 * its result value.  Implements the high-level functions thread_wait() and
 * thread_wait2().
 *
 * [Parameters]
 *     thread: Handle of thread to wait for (guaranteed nonzero).
 *     result_ret: Pointer to variable to receive thread result value
 *         (guaranteed non-NULL).
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_thread_wait(SysThreadID thread, int *result_ret);

/**
 * sys_thread_yield:  Yield the CPU to another thread.  Implements the
 * high-level function thread_yield().
 */
extern void sys_thread_yield(void);

/*--------------------- Condition variable routines ---------------------*/

/**
 * sys_condvar_create:  Create a condition variable.  Implements the
 * high-level function condvar_create().
 *
 * [Return value]
 *     New condition variable, or zero on error.
 */
extern SysCondVarID sys_condvar_create(void);

/**
 * sys_condvar_destroy:  Destroy a condition variable.  Implements the
 * high-level function condvar_destroy().
 *
 * [Parameters]
 *     condvar: Condition variable to destroy (guaranteed nonzero).
 */
extern void sys_condvar_destroy(SysCondVarID condvar);

/**
 * sys_condvar_wait:  Wait on a condition variable.  Implements the
 * high-level functions condvar_wait() and condvar_wait_timeout().
 *
 * [Parameters]
 *     condvar: Condition variable on which to wait (guaranteed nonzero).
 *     mutex: Mutex associated with condition variable (guaranteed nonzero).
 *     timeout: Maximum time to wait for the signal, in seconds, or a
 *         negative value to wait forever.
 * [Return value]
 *     True if the wait operation succeeded, false if not.
 */
extern int sys_condvar_wait(SysCondVarID condvar, SysMutexID mutex,
                            float timeout);

/**
 * sys_condvar_signal:  Signal a condition variable.  Implements the
 * high-level functions condvar_signal() and condvar_broadcast().
 *
 * [Parameters]
 *     condvar: Condition variable to signal (guaranteed nonzero).
 *     broadcast: True to signal all waiting threads, false to signal only
 *         (at least) one thread.
 */
extern void sys_condvar_signal(SysCondVarID condvar, int broadcast);

/*--------------------------- Mutex routines ----------------------------*/

/**
 * sys_mutex_create:  Create a mutex.  Implements the high-level function
 * mutex_create().
 *
 * [Parameters]
 *     recursive: True if the mutex must be recursive, false to accept a
 *         non-recursive mutex.
 *     initially_locked: True to create the mutex in a locked state, false
 *         to create it in an unlocked state.
 * [Return value]
 *     New mutex, or zero on error.
 */
extern SysMutexID sys_mutex_create(int recursive, int initially_locked);

/**
 * sys_mutex_destroy:  Destroy a mutex.  Implements the high-level function
 * mutex_destroy().
 *
 * [Parameters]
 *     mutex: Mutex to destroy (guaranteed nonzero).
 */
extern void sys_mutex_destroy(SysMutexID mutex);

/**
 * sys_mutex_lock:  Lock a mutex.  Implements the high-level functions
 * mutex_lock() and mutex_lock_timeout().
 *
 * [Parameters]
 *     mutex: Mutex to lock (guaranteed nonzero).
 *     timeout: Maximum time to wait for the mutex, in seconds, or a
 *         negative value to wait forever.
 */
extern int sys_mutex_lock(SysMutexID mutex, float timeout);

/**
 * sys_mutex_unlock:  Unlock a mutex.  Implements the high-level function
 * mutex_unlock().
 *
 * [Parameters]
 *     mutex: Mutex to unlock (guaranteed nonzero).
 */
extern void sys_mutex_unlock(SysMutexID mutex);

/*------------------------- Semaphore routines --------------------------*/

/**
 * sys_semaphore_max_value:  Return the maximum value supported for
 * semaphores.  Implements the high-level function semaphore_max_value().
 *
 * [Return value]
 *     Maximum value for a semaphore.
 */
extern PURE_FUNCTION int sys_semaphore_max_value(void);

/**
 * sys_semaphore_create:  Create a semaphore.  Implements the high-level
 * function semaphore_create().
 *
 * [Parameters]
 *     initial_value: Initial value for the semaphore (guaranteed nonnegative).
 *     required_max: Required maximum value for the semaphore (guaranteed
 *         positive and no less than initial_value).
 * [Return value]
 *     New semaphore, or zero on error.
 */
extern SysSemaphoreID sys_semaphore_create(int initial_value, int required_max);

/**
 * sys_semaphore_destroy:  Destroy a semaphore.  Implements the high-level
 * function semaphore_destroy().
 *
 * [Parameters]
 *     semaphore: Semaphore to destroy (guaranteed nonzero).
 */
extern void sys_semaphore_destroy(SysSemaphoreID semaphore);

/**
 * sys_semaphore_wait:  Wait on a semaphore.  Implements the high-level
 * functions semaphore_wait() and semaphore_wait_timeout().
 *
 * [Parameters]
 *     semaphore: Semaphore on which to wait (guaranteed nonzero).
 *     timeout: Maximum time to wait for the signal, in seconds, or a
 *         negative value to wait forever.
 * [Return value]
 *     True if the wait operation succeeded, false if not.
 */
extern int sys_semaphore_wait(SysSemaphoreID semaphore, float timeout);

/**
 * sys_semaphore_signal:  Signal a semaphore.  Implements the high-level
 * function semaphore_signal().
 *
 * [Parameters]
 *     semaphore: Semaphore to signal (guaranteed nonzero).
 */
extern void sys_semaphore_signal(SysSemaphoreID semaphore);

/*************************************************************************/
/*********************** Timekeeping functionality ***********************/
/*************************************************************************/

/**
 * sys_time_init:  Initialize timekeeping functionality.  This function
 * must not fail.
 *
 * This function may be called more than once; the implementation is free
 * to reset internal counters or other data on each call (in particular,
 * the time unit and values returned from sys_time_now() may be reset by a
 * call to this function).
 */
extern void sys_time_init(void);

/**
 * sys_time_init:  Return the number of time units per second in the time
 * values returned by sys_time_now().
 *
 * [Return value]
 *     Number of time units per second.
 */
extern uint64_t sys_time_unit(void);

/**
 * sys_time_now:  Return the system time.  The meaning of the value is
 * system-dependent; the value should only be used for comparison or
 * difference calculations with other time values returned from this
 * function.
 *
 * Implementations may assume that the value returned from the first call
 * to sys_time_now() following a call to sys_time_init() will be used as
 * the epoch for values reported from time_now().  For example, the POSIX
 * implementation of these functions exports this value via the
 * sys_posix_time_epoch() function, allowing other system timestamps to be
 * converted directly to floating-point values compatible with time_now().
 *
 * [Return value]
 *     Current time, in units as returned by sys_time_unit().
 */
extern uint64_t sys_time_now(void);

/**
 * sys_time_delay:  Delay the program for the given length of time (which
 * must be less than or equal to one minute).  Depending on the system, the
 * program may actually delay for slightly longer, but will never return
 * before at least the specified amount of time has passed.
 *
 * The caller guarantees that the time value given corresponds to a length
 * of time no longer than 60 seconds.
 *
 * [Parameters]
 *     time: Length of time to wait, in units as returned by sys_time_unit().
 */
extern void sys_time_delay(int64_t time);

/**
 * sys_time_get_utc:  Return the current real-world (wall-clock) time in
 * Coordinated Universal Time (UTC), along with the current offset from
 * UTC to local time.
 *
 * This function must not fail.  If a system error prevents retrieval of
 * the current time, return an arbitrary time instead.
 *
 * [Parameters]
 *     time_ret: Pointer to variable to receive the current time
 *         (guaranteed non-NULL).
 * [Return value]
 *     Offset from UTC in minutes, computed as (local time - UTC).
 *     Must be within the range (-1440,+1440).
 */
extern int sys_time_get_utc(struct DateTime *time_ret);

/*************************************************************************/
/*************************** User data access ****************************/
/*************************************************************************/

/* Structures defined in userdata.h to which pointers are used here. */
struct UserStatInfo;
struct UserStatValue;

/**
 * SysUserDataOp:  Enumeration of user data operations for the
 * SysUserDataParams.operation field.
 */
enum SysUserDataOp {
    SYS_USERDATA_SAVE_SAVEFILE = 1,
    SYS_USERDATA_LOAD_SAVEFILE,
    SYS_USERDATA_DELETE_SAVEFILE,
    SYS_USERDATA_SCAN_SAVEFILES,
    SYS_USERDATA_SAVE_SETTINGS,
    SYS_USERDATA_LOAD_SETTINGS,
    SYS_USERDATA_SAVE_SCREENSHOT,
    SYS_USERDATA_SAVE_DATA,
    SYS_USERDATA_LOAD_DATA,
    SYS_USERDATA_DELETE_DATA,
    SYS_USERDATA_LOAD_STATS,
    SYS_USERDATA_SAVE_STATS,
    SYS_USERDATA_CLEAR_STATS,
};
typedef enum SysUserDataOp SysUserDataOp;

/**
 * SysUserDataParamsPrivate:  Opaque structure which may be used by system
 * implementations to store private data associated with a SysUserDataParams
 * parameter block.
 */
typedef struct SysUserDataParamsPrivate SysUserDataParamsPrivate;

/**
 * SysUserDataParams:  Structure holding parameters for a user data
 * operation.
 */
typedef struct SysUserDataParams SysUserDataParams;
struct SysUserDataParams {
    /* The operation to be performed (one of SYS_USERDATA_*). */
    SysUserDataOp operation;

    /* Override pathname for this operation (overrides the default
     * system-dependent pathname), or NULL if none. */
    const char *override_path;

    /* Program name or ID string (required for all operations). */
    const char *program_name;
    /* Game title (required for all operations). */
    const char *game_title;

    /* Save file number (SAVE_SAVEFILE, LOAD_SAVEFILE, SCAN_SAVEFILES,
     * DELETE_SAVEFILE only).  For SCAN_SAVEFILES, this is the number of
     * the first save to scan for. */
    int savefile_num;
    /* Data file path (SAVE_DATA, LOAD_DATA, DELETE_DATA only). */
    const char *datafile_path;
    /* File title and descriptive text (SAVE_SAVEFILE, SAVE_SETTINGS only). */
    const char *title;
    const char *desc;

    /* Pointer to the data to be saved (SAVE operations except
     * SAVE_SCREENSHOT only).  Must remain valid until the operation
     * completes. */
    const void *save_data;
    /* Size in bytes of the data to be saved (SAVE operations except
     * SAVE_SCREENSHOT only). */
    int save_size;
    /* Pointer to the RGBA pixel data to be saved as a screenshot
     * (SAVE_SAVEFILE, SAVE_SCREENSHOT only).  Must remain valid until the
     * operation completes. */
    const void *save_image;
    /* Size in pixels of the screenshot data to be saved (SAVE_SAVEFILE,
     * SAVE_SCREENSHOT only). */
    int save_image_width, save_image_height;

    /* Output field, set to a pointer to a buffer allocated with mem_alloc()
     * containing the loaded data (LOAD operations only, undefined on
     * operation failure). */
    void *load_data;
    /* Output field, set to the size in bytes of the loaded data (LOAD
     * operations only, undefined on operation failure). */
    int load_size;
    /* Output field, set to a pointer to a buffer allocated with mem_alloc()
     * containing the loaded RGBA pixel data for the save file's screenshot,
     * or NULL if no screenshot was found (LOAD_SAVEFILE only, undefined on
     * operation failure). */
    void *load_image;
    /* Output field, set to the size in pixels of the loaded screenshot
     * data (LOAD_SAVEFILE only, undefined on operation failure). */
    int load_image_width, load_image_height;

    /* Pointer to a buffer to be filled with savefile scan results
     * (SCAN_SAVEFILES only). */
    uint8_t *scan_buffer;
    /* Number of files to scan for (SCAN_SAVEFILES only).  scan_buffer must
     * have room for at least this many entries. */
    int scan_count;

    /* Pointer to an array of UserStatInfo structures describing statistics
     * to load (LOAD_STATS), save (SAVE_STATS), or clear (CLEAR_STATS).
     * The array must remain valid until the operation completes. */
    struct UserStatInfo *stat_info;

    /* Pointer to an array of UserStatValue structures to save (SAVE_STATS)
     * or in which to store loaded statistic values (LOAD_STATS).  Each
     * entry corresponds to the entry with the same index in the stat_info[]
     * array.  The array must remain valid until the operation completes. */
    double *stat_values;

    /* Pointer to an array of flags indicating which statistics' values
     * have changed since the last SAVE_STATS operation (SAVE_STATS only).
     * Each entry corresponds to the entry with the same index in the
     * stat_info[] array.  The array must remain valid until the operation
     * completes. */
    uint8_t *stat_updated;

    /* Length of the stat_* arrays (LOAD_STATS and SAVE_STATS only). */
    int stat_count;

    /* Pointer to implementation-private data (opaque to callers). */
    SysUserDataParamsPrivate *private;
};

/*-----------------------------------------------------------------------*/

/**
 * userdata_init:  Initialize the system-level user data management
 * functionality.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_userdata_init(void);

/**
 * userdata_cleanup:  Shut down the system-level user data management
 * functionality.  The caller guarantees that no user data operations are
 * in progress when this function is called.
 */
extern void sys_userdata_cleanup(void);

/**
 * sys_userdata_get_data_path:  Return a pathname suitable for accessing
 * resources stored as user data, as for the system-independent interface
 * function userdata_get_data_path().  The returned string (if any) is
 * stored in a buffer allocated with mem_alloc(), which should be freed
 * with mem_free() when no longer needed.
 *
 * [Parameters]
 *     program_name: Program name for which to generate data pathname.
 * [Return value]
 *     Newly-allocated buffer containing generated pathname; NULL on error
 *     or if not supported by this platform.
 */
extern char *sys_userdata_get_data_path(const char *program_name);

/**
 * sys_userdata_perform:  Perform the user data operation specified by the
 * passed-in parameter block.
 *
 * The platform-independent user data interface is asynchronous to allow
 * the caller to perform other actions (such as updating the display)
 * while a user data operation is in progress.  This is implemented by
 * performing each user data operation in its own thread, so this function
 * must take care to protect any shared data with mutexes or similar
 * locking mechanisms.
 *
 * [Parameters]
 *     params: Parameter block describing operation to be performed.
 * [Return value]
 *     True if the operation was successfully performed, false if an error
 *     occurred.
 */
extern int sys_userdata_perform(SysUserDataParams *params);

/*************************************************************************/
/********************** Miscellaneous functionality **********************/
/*************************************************************************/

/**
 * sys_console_vprintf:  Display a message on the console device, if any.
 * Implements the high-level utility function console_printf().
 *
 * [Parameters]
 *     format: printf()-style format string for text (guaranteed non-NULL).
 *     args: Format arguments for text.
 */
extern void sys_console_vprintf(const char *format, va_list args);

/**
 * sys_display_error:  Display an error message to the user.  Implements
 * the high-level utility function display_error().
 *
 * [Parameters]
 *     message: Error message to display (guaranteed non-NULL).  May
 *         contain printf()-style formatting tokens.
 *     args: Format arguments for error message.
 */
extern void sys_display_error(const char *message, va_list args);

/**
 * sys_get_language:  Return the system's preferred language as a 2-letter
 * ISO 639-1 language code (e.g., "en" for English), along with an optional
 * 2-letter ISO 3166 region code identifying the dialect (e.g., "UK" for
 * British English).  Implements the high-level utility function
 * get_system_language().
 *
 * [Parameters]
 *     index: Language preference index (0 = system default language).
 *     language_ret: Pointer to string buffer to receive the language code.
 *     dialect_ret: Pointer to string buffer to receive the dialect code.
 * [Return value]
 *     True on success, false if there is no language for the requested index.
 */
extern int sys_get_language(int index, char *language_ret, char *dialect_ret);

/**
 * sys_get_resource_path_prefix:  Return the resource path prefix for the
 * runtime environment.  Resource pathnames are appended directly to this
 * string, so it should end with a path separator if the prefix is intended
 * to specify a directory.  The returned string is always null-terminated,
 * even if it is truncated due to buffer overflow.
 *
 * This function is potentially called once for each resource operation on
 * a data file, so it should not perform any expensive operations.
 *
 * [Parameters]
 *     prefix_buf: Buffer into which to store the path prefix (guaranteed
 *         non-NULL).
 *     bufsize: Size of prefix_buf, in bytes (guaranteed positive).
 * [Return value]
 *     Length of the path prefix, in bytes.  If this value is >= bufsize,
 *     the returned string was truncated.
 */
extern int sys_get_resource_path_prefix(char *prefix_buf, int bufsize);

/**
 * sys_last_error:  Return a numeric code describing the error associated
 * with the previous failing system-specific function call.  Behavior is
 * undefined if not called immediately after such a failed call.
 *
 * [Return value]
 *     Error code (SYSERR_*).
 */
extern int sys_last_error(void);
enum {
    SYSERR_UNKNOWN_ERROR = 1,   // All errors except the below.
    SYSERR_INVALID_PARAMETER,   // An invalid parameter value was used.
    SYSERR_OUT_OF_MEMORY,       // Insufficient memory (or other system
                                //    resource) is available.
    SYSERR_BUFFER_OVERFLOW,     // An internal buffer size was exceeded.
    SYSERR_TRANSIENT_FAILURE,   // A transient failure occurred; retrying the
                                //    operation at a later time may succeed.
    SYSERR_FILE_NOT_FOUND,      // The file or directory could not be opened.
    SYSERR_FILE_ACCESS_DENIED,  // Access to the file or directory was denied.
    SYSERR_FILE_WRONG_TYPE,     // The file is of the wrong type (for example,
                                //    sys_file_open() on a directory).
    SYSERR_FILE_ASYNC_ABORTED,  // The asynchronous read was aborted.
    SYSERR_FILE_ASYNC_INVALID,  // The asynchronous read ID is invalid.
    SYSERR_FILE_ASYNC_FULL,     // Too many asynchronous reads are in progress.
};

/**
 * sys_last_errstr:  Return a text string describing the error associated
 * with the previous failing system-specific function call.  Behavior is
 * undefined if not called immediately after such a failed call, except
 * that the return value will always be a valid string.
 *
 * [Return value]
 *     Error description.
 */
extern const char *sys_last_errstr(void);

/**
 * sys_open_file:  Open a local file in an appropriate external program;
 * if path == NULL, just return whether opening of local files is supported
 * by the system, under the assumption that the file type is recognized.
 * Implements the high-level utility functions can_open_file() and open_file().
 *
 * [Parameters]
 *     path: Path of file to open, or NULL to check for file opening
 *         capability.
 * [Return value]
 *     False if the system does not support opening files in external
 *     applications, or if path != NULL and an error is known to have
 *     occurred; true otherwise.
 */
extern int sys_open_file(const char *path);

/**
 * sys_open_url:  Open a URL in the user's browser; if url == NULL, just
 * return whether web browsing is supported by the system.  Implements the
 * high-level utility functions can_open_url() and open_url().
 *
 * [Parameters]
 *     url: URL to open, or NULL to check for web browsing capability.
 * [Return value]
 *     False if the system does not support web browsing, or if url != NULL
 *     and an error is known to have occurred; true otherwise.
 */
extern int sys_open_url(const char *url);

/**
 * sys_random_seed:  Return a nonzero random seed derived from the runtime
 * environment or other state.  The returned value should be different
 * across multiple runs of the program.
 *
 * This function must _not_ call any of the random number generation
 * functions.
 *
 * [Return value]
 *     Nonzero random seed.
 */
extern uint64_t sys_random_seed(void);

/**
 * sys_reset_idle_timer:  Reset any system "idle timers", such as
 * screensaver activation or auto-suspend timers.  Implements the
 * high-level utility function reset_idle_timer().
 */
extern void sys_reset_idle_timer(void);

/**
 * sys_set_performance_level:  Request the system to switch to a
 * different performance level.  Implements the high-level function
 * set_performance_level().
 *
 * [Parameters]
 *     level: New performance level (PERFORMANCE_LEVEL_* or a
 *         system-dependent positive value).
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_set_performance_level(int level);

/*************************************************************************/
/************************ Debug interface helpers ************************/
/*************************************************************************/

/**
 * sys_debug_get_memory_stats:  Retrieve system memory usage statistics.
 * Values returned are in units of bytes.  For systems with swap or similar
 * secondary memory, the returned values refer only to primary memory (RAM).
 *
 * [Parameters]
 *     total_ret: Pointer to variable to receive the total amount of memory
 *         in the system.
 *     self_ret: Pointer to variable to receive the amount of memory used
 *         by the current process.
 *     avail_ret: Pointer to variable to receive amount of memory available
 *         for use in the system.
 * [Return value]
 *     True on success, false on error.
 */
extern int sys_debug_get_memory_stats(
    int64_t *total_ret, int64_t *self_ret, int64_t *avail_ret);

/*************************************************************************/
/********************** Macros for testing (part 2) **********************/
/*************************************************************************/

#if defined(SIL_INCLUDE_TESTS) && !defined(IN_SYSDEP) && !defined(IN_SYSDEP_TEST)
// Through the end of this section.

extern int is_running_tests(void);

#define DEFINE_STUB(type,name,params,args) \
extern type TEST_##name params; \
static inline type STUB_##name params { \
    return is_running_tests() ? TEST_##name args : name args; \
}

DEFINE_STUB(int, sys_input_init, (void (*event_callback)(const struct InputEvent *)), (event_callback))
DEFINE_STUB(void, sys_input_cleanup, (void), ())
DEFINE_STUB(void, sys_input_update, (void), ())
DEFINE_STUB(void, sys_input_info, (SysInputInfo *info_ret), (info_ret))
DEFINE_STUB(void, sys_input_grab, (int grab), (grab))
DEFINE_STUB(int, sys_input_is_quit_requested, (void), ())
DEFINE_STUB(int, sys_input_is_suspend_requested, (void), ())
DEFINE_STUB(void, sys_input_acknowledge_suspend_request, (void), ())
DEFINE_STUB(void, sys_input_enable_unfocused_joystick, (int enable), (enable))
DEFINE_STUB(char *, sys_input_joystick_copy_name, (int index), (index))
DEFINE_STUB(int, sys_input_joystick_button_mapping, (int index, int name), (index, name))
DEFINE_STUB(void, sys_input_joystick_rumble, (int index, float left, float right, float time), (index, left, right, time))
DEFINE_STUB(void, sys_input_mouse_set_position, (float x, float y), (x, y))
DEFINE_STUB(void, sys_input_text_set_state, (int on, const char *text, const char *prompt), (on, text, prompt))

DEFINE_STUB(int, sys_sound_init, (const char *device_name), (device_name))
DEFINE_STUB(int, sys_sound_playback_rate, (void), ())
DEFINE_STUB(float, sys_sound_set_latency, (float latency), (latency))
DEFINE_STUB(void, sys_sound_enable_headphone_disconnect_check, (int enable), (enable))
DEFINE_STUB(int, sys_sound_check_headphone_disconnect, (void), ())
DEFINE_STUB(void, sys_sound_acknowledge_headphone_disconnect, (void), ())
DEFINE_STUB(void, sys_sound_cleanup, (void), ())

DEFINE_STUB(void, sys_time_init, (void), ())
DEFINE_STUB(uint64_t, sys_time_unit, (void), ())
DEFINE_STUB(uint64_t, sys_time_now, (void), ())
DEFINE_STUB(void, sys_time_delay, (int64_t time), (time))
DEFINE_STUB(int, sys_time_get_utc, (struct DateTime *time_ret), (time_ret))

DEFINE_STUB(int, sys_userdata_init, (void), ())
DEFINE_STUB(void, sys_userdata_cleanup, (void), ())
DEFINE_STUB(char *, sys_userdata_get_data_path, (const char *program_name), (program_name))
DEFINE_STUB(int, sys_userdata_perform, (SysUserDataParams *params), (params))

DEFINE_STUB(void, sys_console_vprintf, (const char *format, va_list args), (format, args))
DEFINE_STUB(void, sys_display_error, (const char *message, va_list args), (message, args))
DEFINE_STUB(int, sys_get_language, (int index, char *language_ret, char *dialect_ret), (index, language_ret, dialect_ret))
DEFINE_STUB(int, sys_open_file, (const char *path), (path))
DEFINE_STUB(int, sys_open_url, (const char *url), (url))
DEFINE_STUB(void, sys_reset_idle_timer, (void), ())

DEFINE_STUB(int, sys_debug_get_memory_stats, (int64_t *total_ret, int64_t *self_ret, int64_t *avail_ret), (total_ret, self_ret, avail_ret))

#undef DEFINE_STUB

/*-----------------------------------------------------------------------*/

#define sys_input_init                  STUB_sys_input_init
#define sys_input_cleanup               STUB_sys_input_cleanup
#define sys_input_update                STUB_sys_input_update
#define sys_input_info                  STUB_sys_input_info
#define sys_input_grab                  STUB_sys_input_grab
#define sys_input_is_quit_requested     STUB_sys_input_is_quit_requested
#define sys_input_is_suspend_requested  STUB_sys_input_is_suspend_requested
#define sys_input_acknowledge_suspend_request STUB_sys_input_acknowledge_suspend_request
#define sys_input_enable_unfocused_joystick STUB_sys_input_enable_unfocused_joystick
#define sys_input_joystick_copy_name    STUB_sys_input_joystick_copy_name
#define sys_input_joystick_button_mapping STUB_sys_input_joystick_button_mapping
#define sys_input_joystick_rumble       STUB_sys_input_joystick_rumble
#define sys_input_mouse_set_position    STUB_sys_input_mouse_set_position
#define sys_input_text_set_state        STUB_sys_input_text_set_state

#define sys_sound_init                  STUB_sys_sound_init
#define sys_sound_playback_rate         STUB_sys_sound_playback_rate
#define sys_sound_set_latency           STUB_sys_sound_set_latency
#define sys_sound_enable_headphone_disconnect_check STUB_sys_sound_enable_headphone_disconnect_check
#define sys_sound_check_headphone_disconnect STUB_sys_sound_check_headphone_disconnect
#define sys_sound_acknowledge_headphone_disconnect STUB_sys_sound_acknowledge_headphone_disconnect
#define sys_sound_cleanup               STUB_sys_sound_cleanup

#define sys_time_init                   STUB_sys_time_init
#define sys_time_unit                   STUB_sys_time_unit
#define sys_time_now                    STUB_sys_time_now
#define sys_time_delay                  STUB_sys_time_delay
#define sys_time_get_utc                STUB_sys_time_get_utc

#define sys_userdata_init               STUB_sys_userdata_init
#define sys_userdata_cleanup            STUB_sys_userdata_cleanup
#define sys_userdata_get_data_path      STUB_sys_userdata_get_data_path
#define sys_userdata_perform            STUB_sys_userdata_perform

#define sys_console_vprintf             STUB_sys_console_vprintf
#define sys_display_error               STUB_sys_display_error
#define sys_get_language                STUB_sys_get_language
#define sys_open_file                   STUB_sys_open_file
#define sys_open_url                    STUB_sys_open_url
#define sys_reset_idle_timer            STUB_sys_reset_idle_timer

#define sys_debug_get_memory_stats      STUB_sys_debug_get_memory_stats

#endif  // SIL_INCLUDE_TESTS && !IN_SYSDEP && !IN_SYSDEP_TEST

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_H
