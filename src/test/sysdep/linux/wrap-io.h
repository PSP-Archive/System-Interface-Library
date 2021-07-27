/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/linux/wrap-io.h: Control interface for the I/O function
 * wrappers.
 */

#ifndef SIL_SRC_TEST_SYSDEP_LINUX_WRAP_IO_H
#define SIL_SRC_TEST_SYSDEP_LINUX_WRAP_IO_H

#include <dirent.h>
#include <sys/types.h>
#include <utime.h>

/*************************************************************************/
/*************************************************************************/

/* Pointers to override routines for each call.  If NULL, the original
 * function is executed. */
extern int (*override_open)(const char *pathname, int flags, ...);
extern int (*override_close)(int fd);
extern ssize_t (*override_read)(int fd, void *buf, size_t count);
extern ssize_t (*override_write)(int fd, const void *buf, size_t count);
extern int (*override_fdatasync)(int fd);
extern int (*override_utime)(const char *filename, const struct utimbuf *times);
extern int (*override_fcntl)(int fd, int cmd, ...);
extern int (*override_ioctl)(int fd, unsigned long request, ...);
extern ssize_t (*override_readlink)(const char *pathname, char *buf, size_t bufsiz);
extern int (*override_mkdir)(const char *pathname, mode_t mode);
extern int (*override_chdir)(const char *path);
extern DIR *(*override_opendir)(const char *pathname);
extern struct dirent *(*override_readdir)(DIR *d);
extern int (*override_closedir)(DIR *d);
extern int (*override_inotify_init)(void);
extern int (*override_inotify_add_watch)(int fd, const char *pathname, uint32_t mask);

/* Trampolines which jump to the relevant system call, intended for used by
 * override functions which want to perform the original behavior for a
 * particular call. */
extern int trampoline_open(const char *pathname, int flags, ...);
extern int trampoline_close(int fd);
extern ssize_t trampoline_read(int fd, void *buf, size_t count);
extern ssize_t trampoline_write(int fd, const void *buf, size_t count);
extern int trampoline_fdatasync(int fd);
extern int trampoline_utime(const char *filename, const struct utimbuf *times);
extern int trampoline_fcntl(int fd, int cmd, ...);
extern int trampoline_ioctl(int fd, unsigned long request, ...);
extern ssize_t trampoline_readlink(const char *pathname, char *buf, size_t bufsiz);
extern int trampoline_mkdir(const char *pathname, mode_t mode);
extern int trampoline_chdir(const char *path);
extern DIR *trampoline_opendir(const char *pathname);
extern struct dirent *trampoline_readdir(DIR *d);
extern int trampoline_closedir(DIR *d);
extern int trampoline_inotify_init(void);
extern int trampoline_inotify_add_watch(int fd, const char *pathname, uint32_t mask);

/*-----------------------------------------------------------------------*/

/**
 * clear_io_wrapper_variables:  Reset all I/O function overrides to NULL.
 */
extern void clear_io_wrapper_variables(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_SYSDEP_LINUX_WRAP_IO_H
