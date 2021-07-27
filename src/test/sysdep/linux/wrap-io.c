/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/wrap-io.c: Wrappers for I/O system calls allowing
 * replacement of those calls by override functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/sysdep/linux/internal.h"
#include "src/test/base.h"
#include "src/test/sysdep/linux/wrap-io.h"

#include <dlfcn.h>
#define RTLD_DEFAULT  NULL
#define RTLD_NEXT     ((void *)(intptr_t)-1)

#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

/*************************************************************************/
/*************************************************************************/

/*
 * The DEFINE_FUNCTIONS macro defines the system call wrapper, which
 * overlays the original function defined in libc, and the trampoline
 * function, which is used by overrides to call the original function.
 *
 * Originally, these were defined using GCC's __builtin_apply() and
 * __builtin_return() primitives, which allow implementation of the
 * functions using a single macro regardless of the number of arguments
 * taken by the function.  However, these functions are not supported by
 * (what is currently) the other major open-source compiler, Clang, so
 * using them prevents running the Linux-specific tests when building
 * with Clang.
 *
 * Furthermore, on 32-bit x86, GCC through at least version 7.2.0 has a
 * bug (see: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=83292) which
 * triggers an x87 stack exception on the next instruction after a
 * __builtin_apply() call, due to mixing of x87 and MMX instructions.
 * Normally we build with -mfpmath=sse, which would in theory avoid the
 * effects of the bug, but GCC still uses the fild instruction to load a
 * 64-bit integer into a floating-point variable, such as is done by
 * frandom() and frandom_r().  Without a workaround, that will cause the
 * next 64-bit load to turn into a NaN.
 *
 * Because of these issues, and also because it seems (from comments on
 * the bug referenced above) that even GCC developers may be leaning
 * toward deprecating the __builtin_apply() functionality, we use
 * handwritten assembly on known architectures, with __builtin_apply()
 * left in as a fallback for other architectures and to document the
 * behavior of the assembly at a higher level.
 */

#if defined(SIL_ARCH_X86_32)

#define DEFINE_FUNCTIONS(rettype, name, args) \
    rettype (*override_##name) args;            \
    __asm__(                                    \
        "    .section .rodata.str1.1,\"aMS\",@progbits,1\n" \
        "0:  .string \"" #name "\"\n"           \
        "    .text\n"                           \
        "    .globl " #name "\n"                \
        #name ":\n"                             \
        "    mov override_" #name ", %eax\n"    \
        "    test %eax, %eax\n"                 \
        "    jnz 1f\n"                          \
        "    .globl trampoline_" #name "\n"     \
        "trampoline_" #name ":\n"               \
        "    lea 0b, %eax\n"                    \
        "    push %eax\n"                       \
        "    push $-1\n"                        \
        "    call dlsym\n"                      \
        "    add $8, %esp\n"                    \
        "1:  jmp *%eax\n"                       \
    );

#elif defined(SIL_ARCH_X86_64)

#define DEFINE_FUNCTIONS(rettype, name, args) \
    rettype (*override_##name) args;            \
    __asm__(                                    \
        "    .section .rodata.str1.1,\"aMS\",@progbits,1\n" \
        "0:  .string \"" #name "\"\n"           \
        "    .text\n"                           \
        "    .globl " #name "\n"                \
        #name ":\n"                             \
        "    mov override_" #name "(%rip), %rax\n" \
        "    test %rax, %rax\n"                 \
        "    jnz 1f\n"                          \
        "    .globl trampoline_" #name "\n"     \
        "trampoline_" #name ":\n"               \
        "    push %rdi\n"                       \
        "    push %rsi\n"                       \
        "    push %rdx\n"                       \
        "    push %rcx\n"                       \
        "    push %r8\n"                        \
        "    push %r9\n"                        \
        "    mov $-1, %rdi\n"                   \
        "    lea 0b(%rip), %rsi\n"              \
        "    call dlsym@PLT\n"                  \
        "    pop %r9\n"                         \
        "    pop %r8\n"                         \
        "    pop %rcx\n"                        \
        "    pop %rdx\n"                        \
        "    pop %rsi\n"                        \
        "    pop %rdi\n"                        \
        "1:  jmp *%rax\n"                       \
    );

#else  // unknown architecture

/* Needed since most parameters are only referenced by __builtin_apply_args. */
#pragma GCC diagnostic ignored "-Wunused-parameter"

#define MAX_ARG_SIZE  (8 * sizeof(void *))
#define DEFINE_WRAPPER(rettype, name, args)                             \
    rettype name args {                                                 \
        if (LIKELY(!override_##name)) {                                 \
            __builtin_return(__builtin_apply((void *)trampoline_##name, \
                                             __builtin_apply_args(),    \
                                             MAX_ARG_SIZE));            \
        } else {                                                        \
            __builtin_return(__builtin_apply((void *)override_##name,   \
                                             __builtin_apply_args(),    \
                                             MAX_ARG_SIZE));            \
        }                                                               \
    }
#define DEFINE_TRAMPOLINE(rettype, name, args)                    \
    rettype trampoline_##name args {                              \
        __typeof__(name) *wrapped_func = dlsym(RTLD_NEXT, #name); \
        ASSERT(wrapped_func != name);                             \
        ASSERT(wrapped_func != NULL);                             \
        __builtin_return(__builtin_apply((void *)wrapped_func,    \
                                         __builtin_apply_args(),  \
                                         MAX_ARG_SIZE));          \
    }
#define DEFINE_FUNCTIONS(rettype, name, args) \
    rettype (*override_##name) args; \
    DEFINE_WRAPPER(rettype, name, args) \
    DEFINE_TRAMPOLINE(rettype, name, args)

#endif  // SIL_ARCH_*

/*-----------------------------------------------------------------------*/

DEFINE_FUNCTIONS(int, open, (const char *pathname, int flags, ...))
DEFINE_FUNCTIONS(int, close, (int fd))
DEFINE_FUNCTIONS(ssize_t, read, (int fd, void *buf, size_t count))
DEFINE_FUNCTIONS(ssize_t, write, (int fd, const void *buf, size_t count))
DEFINE_FUNCTIONS(int, fdatasync, (int fd))
DEFINE_FUNCTIONS(int, utime,
                 (const char *filename, const struct utimbuf *times))
DEFINE_FUNCTIONS(int, fcntl, (int fd, int cmd, ...))
DEFINE_FUNCTIONS(int, ioctl, (int fd, unsigned long request, ...))
DEFINE_FUNCTIONS(ssize_t, readlink,
                 (const char *pathname, char *buf, size_t bufsiz))
DEFINE_FUNCTIONS(int, mkdir, (const char *pathname, mode_t mode))
DEFINE_FUNCTIONS(int, chdir, (const char *path))
DEFINE_FUNCTIONS(DIR *, opendir, (const char *pathname))
DEFINE_FUNCTIONS(struct dirent *, readdir, (DIR *d))
DEFINE_FUNCTIONS(int, closedir, (DIR *d))
DEFINE_FUNCTIONS(int, inotify_init, (void))
DEFINE_FUNCTIONS(int, inotify_add_watch,
                 (int fd, const char *pathname, uint32_t mask))

/*-----------------------------------------------------------------------*/

/* glibc wrapper for read() on a fixed-size buffer.  This might not be
 * declared (such as when compiling at -O0), so we declare it ourselves
 * to avoid compiler warnings. */
extern ssize_t __read_chk(int fd, void *buf, size_t count, size_t buflen);
ssize_t __read_chk(int fd, void *buf, size_t count, size_t buflen)
{
    if (LIKELY(!override_read)) {
        __typeof__(__read_chk) *wrapped_func = dlsym(RTLD_NEXT, "__read_chk");
        ASSERT(wrapped_func != __read_chk);
        ASSERT(wrapped_func != NULL);
        return (*wrapped_func)(fd, buf, count, buflen);
    } else {
        return read(fd, buf, count);
    }
}

/*************************************************************************/
/********************** Exported utility functions ***********************/
/*************************************************************************/

void clear_io_wrapper_variables(void)
{
    override_open = NULL;
    override_close = NULL;
    override_read = NULL;
    override_write = NULL;
    override_fdatasync = NULL;
    override_utime = NULL;
    override_fcntl = NULL;
    override_ioctl = NULL;
    override_readlink = NULL;
    override_mkdir = NULL;
    override_chdir = NULL;
    override_opendir = NULL;
    override_readdir = NULL;
    override_closedir = NULL;
    override_inotify_init = NULL;
    override_inotify_add_watch = NULL;
}

/*************************************************************************/
/*************************************************************************/
