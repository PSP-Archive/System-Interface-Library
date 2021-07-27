/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/internal.h: Declarations used internally by
 * Windows-specific code.
 */

#ifndef SIL_SRC_SYSDEP_WINDOWS_INTERNAL_H
#define SIL_SRC_SYSDEP_WINDOWS_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/* Windows version constants, for use with WINVER and the windows_version*
 * functions. */

#define WINDOWS_VERSION_2000   0x0500
#define WINDOWS_VERSION_XP     0x0501
#define WINDOWS_VERSION_VISTA  0x0600
#define WINDOWS_VERSION_7      0x0601
#define WINDOWS_VERSION_8      0x0602
#define WINDOWS_VERSION_8_1    0x0603
#define WINDOWS_VERSION_10     0x0A00

/*-----------------------------------------------------------------------*/

/* System headers.  Note that we need to do a bit of magic with #defines
 * because windef.h tries to define "near" and "far" to nothing. */

#undef near
#undef far

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <hidusage.h>
#include <hidpi.h>
#include <hidsdi.h>

/* Fix Windows stomping on our macros. */
#undef ASSERT
#ifdef DEBUG
# define ASSERT(condition,...)                                          \
    if (UNLIKELY(!(condition))) {                                       \
        do_DLOG(__FILE__, __LINE__, __FUNCTION__,                       \
                "\n\n*** ALERT *** ASSERTION FAILED:\n%s\n\n",          \
                #condition);                                            \
        if (0) {__VA_ARGS__;}                                           \
        abort();                                                        \
    } else (void)0
#else
# define ASSERT(condition,...)          \
    if (UNLIKELY(!(condition))) {       \
        __VA_ARGS__;                    \
    } else (void)0
#endif

#if WINVER < WINDOWS_VERSION_VISTA
# define WM_MOUSEHWHEEL  0x20E
#endif

#if WINVER < WINDOWS_VERSION_8
# define SM_DIGITIZER  94
# define NID_INTEGRATED_TOUCH  0x01
# define WM_POINTERUPDATE  0x0245
# define WM_POINTERDOWN  0x0246
# define WM_POINTERUP  0x0247
# define WM_POINTERCAPTURECHANGED  0x024C
# define GET_POINTERID_WPARAM(wParam)  LOWORD((wParam))
typedef enum {
    PT_POINTER = 1,
    PT_TOUCH = 2,
    PT_PEN = 3,
    PT_MOUSE = 4,
    PT_TOUCHPAD = 5,
} POINTER_INPUT_TYPE;
extern BOOL WINAPI GetPointerType(UINT32 pointerId,
                                  POINTER_INPUT_TYPE *pointerType);
#endif

#if WINVER >= WINDOWS_VERSION_8_1
# include <ShellScalingApi.h>
#else
typedef enum {
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2,
} PROCESS_DPI_AWARENESS;
#endif

#include "src/sysdep/windows/utf8-wrappers.h"

#undef near
#undef far
#define near near_
#define far far_

/* Also get rid of these.  Will we ever be free from win16 crap? */
#undef NEAR
#undef FAR

/*-----------------------------------------------------------------------*/

/* If tests are enabled, rename functions which we wrap so our code calls
 * the wrapped functions.  (Windows linkers won't allow us to simply
 * override the library functions with our own, as we can do on Linux.) */

#ifdef SIL_INCLUDE_TESTS

#define CloseHandle _wrap_CloseHandle
#define CreateFileW _wrap_CreateFileW
#define FreeLibrary _wrap_FreeLibrary
#define GetCommandLineW _wrap_GetCommandLineW
#define GetModuleHandleA _wrap_GetModuleHandleA
#define GetRawInputData _wrap_GetRawInputData
#define GetRawInputDeviceInfoW _wrap_GetRawInputDeviceInfoW
#define GetRawInputDeviceList _wrap_GetRawInputDeviceList
#define LoadLibraryW _wrap_LoadLibraryW
#define RegisterRawInputDevices _wrap_RegisterRawInputDevices
#define WriteFile _wrap_WriteFile

extern BOOL WINAPI CloseHandle(HANDLE hObject);
extern HANDLE WINAPI CreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
extern BOOL WINAPI FreeLibrary(HMODULE hModule);
extern LPWSTR WINAPI GetCommandLineW(void);
extern HMODULE WINAPI GetModuleHandleA(LPCSTR lpModuleName);
extern UINT WINAPI GetRawInputData(
    HRAWINPUT hRawInput, UINT uiCommand, LPVOID pData, PUINT pcbSize,
    UINT cbSizeHeader);
extern UINT WINAPI GetRawInputDeviceInfoW(
    HANDLE hDevice, UINT uiCommand, LPVOID pData, PUINT pcbSize);
extern UINT WINAPI GetRawInputDeviceList(
    PRAWINPUTDEVICELIST pRawInputDeviceList, PUINT puiNumDevices, UINT cbSize);
extern HMODULE WINAPI LoadLibraryW(LPCWSTR lpFileName);
extern BOOL WINAPI RegisterRawInputDevices(
    PCRAWINPUTDEVICE pRawInputDevices, UINT uiNumDevices, UINT cbSize);
extern BOOL WINAPI WriteFile(
    HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
    LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);

#endif

/*-----------------------------------------------------------------------*/

/* Library-internal declarations. */


/******** condvar.c ********/

#ifdef SIL_INCLUDE_TESTS
/**
 * TEST_windows_condvar_disable_native:  When set to true (nonzero), the
 * sys_condvar_*() functions will use emulated condition variables even if
 * the system supports native condition variables.
 */
extern uint8_t TEST_windows_condvar_disable_native;
#endif


/******** graphics.c ********/

/*
 * Wrappers for wgl*() functions (see definitions in graphics.c for
 * rationale).  We have to redeclare these to override the dllimport
 * declarations in <wingdi.h>.
 */

/* Suppress dllimport warnings, since we deliberately redeclare these. */
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wattributes"
#elif defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable:4273)
#endif

extern HGLRC WINAPI wglCreateContext(HDC dc);
extern BOOL WINAPI wglDeleteContext(HGLRC context);
extern HGLRC WINAPI wglGetCurrentContext(void);
extern PROC WINAPI wglGetProcAddress(const char *name);
extern BOOL WINAPI wglMakeCurrent(HDC dc, HGLRC context);

#if defined(__GNUC__)
# pragma GCC diagnostic pop
#elif defined(_MSC_VER)
# pragma warning(pop)
#endif

/**
 * windows_update_window:  Perform periodic processing for the currently
 * open window, if any.
 */
extern void windows_update_window(void);

/**
 * windows_close_window:  Close the currently open window, if any.
 * Intended primarily for use by test routines.
 */
extern void windows_close_window(void);

/**
 * windows_flush_message_queue:  Wait for all pending window messages to
 * be processed by the window procedure.  Intended primarily for use by
 * test routines.
 */
extern void windows_flush_message_queue(void);

/**
 * windows_reset_video_mode:  If the video mode of the display device has
 * been changed, reset it to the original mode.
 */
extern void windows_reset_video_mode(void);

/**
 * windows_set_mouse_grab:  Set whether to confine the mouse to the window
 * while a window is open.
 *
 * [Parameters]
 *     grab: True to grab the mouse, false to not grab.
 */
extern void windows_set_mouse_grab(int grab);

/**
 * windows_show_mouse_pointer:  Override or restore the shown/hidden state
 * of the mouse pointer.
 *
 * [Parameters]
 *     override: 1 to show the mouse pointer, 0 to hide the pointer, -1 to
 *         restore the state selected by sys_graphics_show_mouse_pointer().
 */
extern void windows_show_mouse_pointer(int override);

/**
 * windows_vsync_interval:  Return the current vertical sync interval as
 * set with the "vsync" display attribute.
 */
extern int windows_vsync_interval(void);

/**
 * windows_wgl_context:  Return the GL context for the current window.
 *
 * [Return value]
 *     GL context for the current window, or NULL if no window is open.
 */
extern HGLRC windows_wgl_context(void);

/**
 * windows_window:  Return the handle for the current window.
 *
 * [Return value]
 *     Handle for the current window, or NULL if no window is open.
 */
extern HWND windows_window(void);

/**
 * windows_window_title:  Return the window title currently set via
 * graphics_set_window_title().
 *
 * [Return value]
 *     Window title, or NULL if no title has been set.
 */
extern const char *windows_window_title(void);

#ifdef SIL_INCLUDE_TESTS
/**
 * TEST_windows_force_direct3d:  If true, graphics_set_display_mode() will
 * always attempt to create a Direct3D context regardless of the backend
 * mode selected with the backend_name display attribute.
 */
extern uint8_t TEST_windows_force_direct3d;
#endif


/******** input.c ********/

/**
 * windows_init_input_message_lock:  Initialize the internal lock used by
 * input message handling.  Must be called exactly once at program startup,
 * before any windows are opened.
 *
 * This function is only needed because Windows lacks the equivalent of a
 * static initializer like PTHREAD_MUTEX_INITIALIZER.
 */
extern void windows_init_input_message_lock(void);

/**
 * windows_handle_input_message:  Handle window messages relating to input
 * events.
 *
 * [Parameters]
 *     hwnd: Window on which the message was received (always the currently
 *         open window).
 *     uMsg: Message type.
 *     wParam, lParam: Message parameters.
 * [Return value]
 *     True if the message was consumed, false if not.
 */
extern int windows_handle_input_message(HWND hwnd, UINT uMsg,
                                        WPARAM wParam, LPARAM lParam);


/******** main.c ********/

/**
 * windows_executable_dir:  Return the pathname of the directory containing
 * the executable file used to start the program, or "." if the directory
 * is unknown.  The path is returned in Windows format, using backslashes
 * rather than slashes to separate path components.
 */
extern const char *windows_executable_dir(void);

#ifdef SIL_INCLUDE_TESTS
/**
 * TEST_windows_no_main_abort:  If set to true (nonzero), WinMain() will
 * not abort when sil_main() indicates init failure.  Used in tests.
 */
extern uint8_t TEST_windows_no_main_abort;
#endif


/******** misc.c ********/

/**
 * windows_set_error:  Record the given SYSERR_* error code as the error to
 * be returned from sys_last_error().  If code == 0, the code is instead
 * set based on the Windows error code specified by windows_code; if
 * windows_code is also zero, the current Windows error code (as returned
 * by GetLastError()) is used.
 *
 * [Parameters]
 *     code: SYSERR_* error code, or zero to use a Windows error code.
 *     windows_code: Windows error code, or zero to use the system error code.
 */
extern void windows_set_error(int code, DWORD windows_code);


/******** sound.c ********/

/**
 * AudioDriver:  Structure encapsulating driver interface routines for a
 * specific audio driver.
 */
typedef struct AudioDriver AudioDriver;
struct AudioDriver {
    /**
     * open:  Open an audio device.
     *
     * [Parameters]
     *     device_name: Name of audio device to open, or the empty string
     *         to open the default device.  Guaranteed non-NULL.
     * [Return value]
     *     Sampling rate for audio playback, or 0 on error.
     */
    int (*open)(const char *device_name);

    /**
     * close:  Close the currently open audio device.
     *
     * The audio device must have been successfully opened before calling
     * this function.
     */
    void (*close)(void);

    /**
     * get_latency:  Return the current audio output latency.
     *
     * The audio device must have been successfully opened before calling
     * this function.
     *
     * [Return value]
     *     Output latency, in seconds.
     */
    float (*get_latency)(void);

    /**
     * set_latency:  Set the audio output latency.
     *
     * This function is only called from the audio playback thread.
     *
     * [Parameters]
     *     latency: Desired output latency, in seconds.
     */
    void (*set_latency)(float latency);

    /**
     * get_buffer:  Return the next output buffer into which mixed audio
     * can be stored.  If the next buffer is not yet available, block
     * until it becomes available or until the specified timeout elapses.
     *
     * Data will be written to the buffer in interleaved 16-bit integer
     * PCM format.
     *
     * This function is only called from the audio playback thread.
     *
     * [Parameters]
     *     timeout: Maximum length of time to wait for a buffer to become
     *         available, in seconds.
     *     buffer_ret: Pointer to variable to receive the buffer pointer.
     *     size_ret: Pointer to variable to receive the buffer size in frames.
     * [Return value]
     *     1 on success, 0 on timeout, or -1 on failure.
     */
    int (*get_buffer)(float timeout, int16_t **buffer_ret, int *size_ret);

    /**
     * submit_buffer:  Submit the buffer most recently returned by
     * get_buffer() to the system.
     *
     * This function is only called from the audio playback thread, and will
     * only and always be called after a successful call to get_buffer().
     */
    void (*submit_buffer)(void);
};


/******** sound-wasapi.c ********/

/**
 * windows_wasapi_init:  Check that WASAPI is available and initialize
 * basic objects.
 *
 * This function may safely be called multiple times.
 *
 * [Return value]
 *     True on success, false on error.
 */
extern int windows_wasapi_init(void);

/**
 * windows_wasapi_driver:  Audio driver using the Windows Audio Session API
 * (WASAPI).
 */
extern AudioDriver windows_wasapi_driver;


/******** sound-winmm.c ********/

/**
 * windows_winmm_driver:  Audio driver using the WinMM (waveOut) API.
 */
extern AudioDriver windows_winmm_driver;


/******** util.c ********/

/**
 * strcmp_16:  Compare two 16-bit-character strings.  The return value is
 * as for strcmp().
 */
extern int strcmp_16(const uint16_t *a, const uint16_t *b);

/**
 * strdup_16:  Return a copy of the given 16-bit-character string, or NULL
 * if out of memory.
 */
extern uint16_t *strdup_16(const uint16_t *s);

/**
 * strdup_16to8:  Translate the given UTF-16 string into UTF-8 and return
 * it in a buffer allocated with mem_alloc().  Any invalid surrogates (a
 * lead surrogate not followed by a trail surrogate or vice versa) are
 * replaced with U+FFFD.
 *
 * [Parameters]
 *     s16: UTF-16 string to convert (must be non-NULL).
 * [Return value]
 *     Converted string, or NULL if out of memory.
 */
extern char *strdup_16to8(const uint16_t *s16);

/**
 * convert_16to8:  Translate the given UTF-16 string into UTF-8 in the same
 * manner as strdup_16to8(), and store it in the given buffer.
 *
 * This function does not check for buffer overflow; the caller is
 * responsible for ensuring that the buffer is large enough to hold the
 * result of conversion.
 *
 * [Parameters]
 *     s16: UTF-16 string to convert (must be non-NULL).
 *     buffer: Buffer to hold output string (must have at least 3 bytes of
 *         space for every 16-byte element in the source string, plus one
 *         byte for the trailing null).
 */
extern void convert_16to8(const uint16_t *s16, char *buffer);

/**
 * strdup_8to16:  Translate the given UTF-8 string into UTF-16 and return
 * it in a buffer allocated with mem_alloc().  Invalid UTF-8 sequences are
 * ignored; codepoints which cannot be encoded in UTF-16 (U+D800 through
 * U+DFFF, U+110000 and above) are replaced with U+FFFD.
 *
 * [Parameters]
 *     s8: UTF-8 string to convert (must be non-NULL).
 * [Return value]
 *     Converted string, or NULL if out of memory.
 */
extern uint16_t *strdup_8to16(const char *s8);

/**
 * timeout_to_ms:  Convert the given timeout to a value suitable for
 * passing to WaitForSingleObject() or similar wait functions.  Helper
 * function for sys_condvar_wait(), sys_mutex_lock(), and sys_semaphore_wait().
 *
 * [Parameters]
 *     timeout: Timeout, in seconds, or a negative value for no timeout.
 * [Return value]
 *     Value to pass as the dwMilliseconds parameter to WaitForSingleObject().
 */
extern DWORD timeout_to_ms(float timeout);

/**
 * windows_getenv:  Retrieve an environment variable's value.  The returned
 * buffer should be freed with mem_free() when no longer needed.
 *
 * [Parameters]
 *     name: Name of environment variable to retrieve.
 * [Return value]
 *     Value of environment variable, or NULL if the environment variable
 *     is not found (or memory allocation failed).
 */
extern char *windows_getenv(const char *name);

/**
 * windows_strerror:  Return a text string describing the error associated
 * with the given Windows error code.  The string is stored in a static
 * buffer which will be overwritten on each call to the function.
 *
 * [Parameters]
 *     code: Windows error code.
 * [Return value]
 *     Error description (including the error code at the beginning).
 */
extern const char *windows_strerror(DWORD code);

/**
 * windows_version:  Return the version of Windows under which the program
 * is running, as a WINVER-style 16-bit value (see the WINDOWS_VERSION_*
 * constants).
 *
 * Due to limitations of the Windows API, this function currently will not
 * return any version number greater than WINDOWS_VERSION_8_1, even if the
 * actual Windows environment is newer.
 *
 * [Return value]
 *     Windows version.
 */
extern CONST_FUNCTION int windows_version(void);

/**
 * windows_version_is_at_least:  Return whether the version of Windows on
 * which the program is running is at least the given version.
 *
 * [Parameters]
 *     version: Windows version (a WINDOWS_VERSION_* constant).
 * [Return value]
 *     True if the runtime Windows version is equal to or later than the
 *     given version, false if not.
 */
extern CONST_FUNCTION int windows_version_is_at_least(int version);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_WINDOWS_INTERNAL_H
