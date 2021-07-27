/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/main.c: Program entry point for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/main.h"
#include "src/math/fpu.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/utility/misc.h"

/*************************************************************************/
/*************************** Test control data ***************************/
/*************************************************************************/

#ifndef SIL_INCLUDE_TESTS
static const
#endif
    uint8_t TEST_windows_no_main_abort;

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Original exception handler (will normally be NULL). */
static LPTOP_LEVEL_EXCEPTION_FILTER original_exception_handler;

/* Pathname of executable's directory, or "." if unknown. */
static char executable_dir[4096];

/*************************************************************************/
/*************************** Exception handler ***************************/
/*************************************************************************/

/**
 * exception_handler:  Top-level exception handler.  Resets the current
 * video mode if necessary, then proceeds with default exception handling.
 */
static LONG WINAPI exception_handler(LPEXCEPTION_POINTERS ExceptionInfo)
{
    windows_reset_video_mode();
    if (original_exception_handler) {
        return (*original_exception_handler)(ExceptionInfo);
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
}

/*************************************************************************/
/************************** Program entry point **************************/
/*************************************************************************/

/* This __declspec is required when building with older versions of MinGW
 * binutils to force an export section to be generated, which in turn is
 * required to enable ASLR (adress space layout randomization).  See:
 * https://stackoverflow.com/questions/24283918 */
__declspec(dllexport)
int WINAPI WinMain(UNUSED HINSTANCE hInstance, UNUSED HINSTANCE hPrevInstance,
                   UNUSED LPSTR lpCmdLine, UNUSED int nCmdShow)
{
#if defined(SIL_ARCH_X86_64)
    DLOG("Executable architecture: x86 64-bit");
#elif defined(SIL_ARCH_X86_32)
    DLOG("Executable architecture: x86 32-bit");
#elif defined(SIL_ARCH_ARM_32)
    DLOG("Executable architecture: ARM 32-bit");
#else
    DLOG("Executable architecture: unknown");
#endif
    DLOG("Windows version: 0x%04X", windows_version());

    /* Install an exception handler so we can (attempt to) recover from
     * fullscreen mode on fatal exceptions. */
    original_exception_handler =
        SetUnhandledExceptionFilter(exception_handler);
    /* MSDN recommends setting SEM_FAILCRITICALERRORS, and we set
     * SEM_NOOPENFILEERRORBOX because we handle failure of the relevant
     * functions ourselves. */
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

    /* Mark the program as DPI-aware (Vista and later) so we don't get
     * randomly scaled by the OS.  Normally this is taken care of by the
     * application manifest, but we check just in case the executable was
     * started in some nonstandard way that skips manifest processing. */
    int set_dpi_aware = 0;
    HMODULE shcore = LoadLibrary("shcore.dll");
    if (shcore) {
        HRESULT (STDAPICALLTYPE *p_GetProcessDpiAwareness)(HANDLE, PROCESS_DPI_AWARENESS *) =
            (void *)GetProcAddress(shcore, "GetProcessDpiAwareness");
        HRESULT (STDAPICALLTYPE *p_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS) =
            (void *)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (p_GetProcessDpiAwareness && p_SetProcessDpiAwareness) {
            PROCESS_DPI_AWARENESS current = PROCESS_DPI_UNAWARE;
            (*p_GetProcessDpiAwareness)(NULL, &current);
            if (current == PROCESS_PER_MONITOR_DPI_AWARE) {
                set_dpi_aware = 1;
            } else {
                const HRESULT result =
                    (*p_SetProcessDpiAwareness)(PROCESS_PER_MONITOR_DPI_AWARE);
                if (result == S_OK) {
                    set_dpi_aware = 1;
                } else {
                    DLOG("SetProcessDpiAwareness() failed: %s",
                         windows_strerror((DWORD)(uintptr_t)result));
                }
            }
        }
        FreeLibrary(shcore);
    }
    if (!set_dpi_aware) {
        BOOL (WINAPI *p_IsProcessDPIAware)(void) =
            (void *)GetProcAddress(GetModuleHandle("user32.dll"),
                                   "IsProcessDPIAware");
        BOOL (WINAPI *p_SetProcessDPIAware)(void) =
            (void *)GetProcAddress(GetModuleHandle("user32.dll"),
                                   "SetProcessDPIAware");
        if (p_IsProcessDPIAware && p_SetProcessDPIAware) {
            if (!(*p_IsProcessDPIAware)()) {
                const BOOL result = (*p_SetProcessDPIAware)();
                if (!result) {
                    DLOG("SetProcessDPIAware() failed: %s",
                         windows_strerror(GetLastError()));
                }
            }
        }
    }

    /* Set the low-frequency timer to the highest possible frequency for
     * more precise time_delay() behavior. */
    {
        TIMECAPS timecaps;
        int error;
        if ((error = timeGetDevCaps(&timecaps, sizeof(timecaps)))
            == TIMERR_NOERROR)
        {
            const int period = lbound(timecaps.wPeriodMin, 1);
            if ((error = timeBeginPeriod(period)) == TIMERR_NOERROR) {
                DLOG("Timer resolution: %d ms", period);
            } else {
                DLOG("timeBeginPeriod(%d) failed: error %d", period, error);
            }
        } else {
            DLOG("timeGetDevCaps() failed: error %d", error);
        }
    }

    /* Handle other Windows-specific early initialization. */
    windows_init_input_message_lock();

    /* Look up the executable's containing directory. */
    memcpy(executable_dir, ".", 2);  // Default value.
    char exe_path[4096];
    STATIC_ASSERT(sizeof(exe_path) <= sizeof(executable_dir),
                  "exe_path too small");
    const int exe_pathlen = GetModuleFileName(NULL, exe_path, sizeof(exe_path));
    if (exe_pathlen == 0) {
        DLOG("GetModuleFileName(NULL) failed: %s",
             windows_strerror(GetLastError()));
    } else if (exe_pathlen >= (int)sizeof(exe_path)) {
        DLOG("GetModuleFileName(NULL) indicated a buffer overflow");
    } else {
        int i;
        for (i = exe_pathlen-1; i >= 0; i--) {
            if (exe_path[i] == '\\') {
                break;
            }
        }
        if (i > 0) {
            memcpy(executable_dir, exe_path, i);
            executable_dir[i] = '\0';
        }
    }

    fpu_configure();

    const uint16_t *cmdline_utf16 = GetCommandLineW();
    ASSERT(cmdline_utf16 != NULL, cmdline_utf16 = (const uint16_t[]){0});
    int argc;
    uint16_t **argv_utf16 = CommandLineToArgvW(cmdline_utf16, &argc);
    ASSERT(argc >= 0, argc = 0);
    ASSERT(argv_utf16 != NULL, argc = 0);

    char *argv_dummy[2];
    char **argv = mem_alloc((argc+1) * sizeof(*argv), 0, 0);
    if (LIKELY(argv)) {
        argv[argc] = NULL;
        for (int i = 0; i < argc; i++) {
            argv[i] = strdup_16to8(argv_utf16[i]);
            if (UNLIKELY(!argv[i])) {
                while (--i >= 0) {
                    mem_free(argv[i]);
                }
                mem_free(argv);
                argv = NULL;
                break;
            }
        }
    }
    if (UNLIKELY(!argv)) {
        argc = 1;
        argv = argv_dummy;
        argv[0] = (char *)"SIL";
        argv[1] = NULL;
    }

    const int exitcode = sil__main(argc, (const char **)argv);

    if (argv != argv_dummy) {
        for (int i = 0; i < argc; i++) {
            mem_free(argv[i]);
        }
        mem_free(argv);
    }

    if (exitcode == 2 && !TEST_windows_no_main_abort) {
        DLOG("Aborting due to init failure");
        /* This exception should never occur in SIL programs, so we use it
         * as a user-visible signal for init failure. */
        RaiseException(EXCEPTION_NONCONTINUABLE_EXCEPTION,
                       EXCEPTION_NONCONTINUABLE, 0, NULL);
    }

    return exitcode;
}

/*************************************************************************/
/****************** Windows-internal exported routines *******************/
/*************************************************************************/

const char *windows_executable_dir(void)
{
    return executable_dir;
}

/*************************************************************************/
/*************************************************************************/
