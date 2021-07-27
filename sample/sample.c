/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * sample/sample.c: Sample client source file for SIL.
 */

/*
 * This file implements a simple client of the SIL library which serves
 * two purposes: it demonstrates how a program can use the SIL build system
 * to build an executable, and it allows the internal SIL tests to be run
 * with a "-test" command-line parameter.
 *
 * See the Makefile in the build/ subdirectory of this directory for
 * details on how to use the SIL build system.
 */

#include <SIL/base.h>
#include <SIL/test.h>
#include <SIL/utility/misc.h>

/* When testing, include all other headers to check for public header bugs. */
#ifdef SIL_INCLUDE_TESTS
# include <SIL/condvar.h>
# include <SIL/debug.h>
# include <SIL/endian.h>
# include <SIL/font.h>
# include <SIL/framebuffer.h>
# include <SIL/graphics.h>
# include <SIL/input.h>
# include <SIL/keycodes.h>
# include <SIL/math.h>
# include <SIL/memory.h>
# include <SIL/movie.h>
# include <SIL/mutex.h>
# include <SIL/random.h>
# include <SIL/resource.h>
# include <SIL/resource/package.h>
# include <SIL/semaphore.h>
# include <SIL/shader.h>
# include <SIL/sound.h>
# include <SIL/sound/decode.h>
# ifdef SIL_PLATFORM_IOS
#  include <SIL/sysdep/ios/gamekit.h>
# endif
# ifdef SIL_PLATFORM_PSP
#  include <SIL/sysdep/psp/lalloc.h>
# endif
# include <SIL/texture.h>
# include <SIL/thread.h>
# include <SIL/time.h>
# include <SIL/userdata.h>
# include <SIL/utility/compress.h>
# include <SIL/utility/log.h>
# include <SIL/utility/png.h>
# include <SIL/utility/strformat.h>
# include <SIL/utility/utf8.h>
# include <SIL/workqueue.h>
#endif  // SIL_INCLUDE_TESTS

#if defined(SIL_PLATFORM_WINDOWS) && defined(DUMP_D3D_SHADERS)
# include <windows.h>
extern char *d3d_compile_default_shaders(void);
#endif

int sil_main(int argc, const char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "-test") == 0
                     || strncmp(argv[1], "-test=", 6) == 0)) {
        const char *tests_to_run = (strlen(argv[1]) > 6) ? &argv[1][6] : "";
        if (!run_internal_tests(tests_to_run)) {
            return EXIT_FAILURE;
        }
    }

#if defined(SIL_PLATFORM_WINDOWS) && defined(DUMP_D3D_SHADERS)
    if (argc > 1 && strncmp(argv[1], "-dump-d3d-shaders=", 18) == 0) {
        const char *path = argv[1] + 18;
        console_printf("Compiling default shaders and dumping to %s...\n",
                       path);
        char *code = d3d_compile_default_shaders();
        const int codelen = strlen(code);
        HANDLE f = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              0, NULL);
        if (f == INVALID_HANDLE_VALUE) {
            console_printf("Failed to open %s: %08X\n", path, GetLastError());
            mem_free(code);
            return EXIT_FAILURE;
        }
        const BOOL write_ok = WriteFile(f, code, codelen, (DWORD[1]){0}, NULL);
        CloseHandle(f);
        mem_free(code);
        if (!write_ok) {
            console_printf("Failed to write to %s: %08X\n",
                           path, GetLastError());
            return EXIT_FAILURE;
        } else {
            console_printf("Shader dump complete.\n");
            return EXIT_SUCCESS;
        }
    }
#endif

    DLOG("%s logs: Hello, world!", *argv[0] ? argv[0] : "No Name");
    console_printf("%s says: Hello, world!\n", *argv[0] ? argv[0] : "No Name");
    return EXIT_SUCCESS;
}
