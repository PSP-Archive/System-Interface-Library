/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/linux/meminfo.c: System/process memory information functions
 * for Linux (and Android).
 */

#include "src/base.h"
#include "src/sysdep/linux/meminfo.h"

#include <fcntl.h>
#include <unistd.h>

/*************************************************************************/
/*************************************************************************/

int64_t linux_get_total_memory(void)
{
    int fd;
    char readbuf[1024+1];  // Should be enough to find the info we want.
    ssize_t nread;
    char *s;

    fd = open("/proc/meminfo", O_RDONLY);
    if (UNLIKELY(fd < 0)) {
        DLOG("Failed to open /proc/meminfo: %s", strerror(errno));
        return 0;
    }
    nread = read(fd, readbuf, sizeof(readbuf)-1);
    if (UNLIKELY(nread < 0)) {
        DLOG("Failed to read from /proc/meminfo: %s", strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    readbuf[nread] = '\0';

    s = readbuf;
    while (*s) {
        char *eol = s + strcspn(s, "\n");
        if (UNLIKELY(!*eol)) {
            break;  // Avoid trying to parse an incomplete line.
        }
        *eol++ = 0;
        char *tag = s;
        s = eol;
        char *value = tag + strcspn(tag, ":");
        ASSERT(*value, continue);
        *value++ = 0;
        if (strcmp(tag, "MemTotal") == 0) {
            return (int64_t)strtol(value, NULL, 10) * 1024;
        }
    }

    DLOG("Failed to find MemTotal tag in /proc/meminfo");
    return 0;
}

/*-----------------------------------------------------------------------*/

int64_t linux_get_process_size(void)
{
    int fd;
    char readbuf[1024+1];
    ssize_t nread;
    char *s;

    fd = open("/proc/self/status", O_RDONLY);
    if (UNLIKELY(fd < 0)) {
        DLOG("Failed to open /proc/self/status: %s", strerror(errno));
        return 0;
    }
    nread = read(fd, readbuf, sizeof(readbuf)-1);
    if (UNLIKELY(nread < 0)) {
        DLOG("Failed to read from /proc/self/status: %s", strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    readbuf[nread] = '\0';

    s = readbuf;
    while (*s) {
        char *eol = s + strcspn(s, "\n");
        if (UNLIKELY(!*eol)) {
            break;
        }
        *eol++ = 0;
        char *tag = s;
        s = eol;
        char *value = tag + strcspn(tag, ":");
        ASSERT(*value, continue);
        *value++ = 0;
        if (strcmp(tag, "VmRSS") == 0) {
            return (int64_t)strtol(value, NULL, 10) * 1024;
        }
    }

    DLOG("Failed to find VmRSS tag in /proc/self/status");
    return 0;
}

/*-----------------------------------------------------------------------*/

int64_t linux_get_free_memory(void)
{
    int fd;
    char readbuf[1024+1];
    ssize_t nread;
    char *s;

    fd = open("/proc/meminfo", O_RDONLY);
    if (UNLIKELY(fd < 0)) {
        DLOG("Failed to open /proc/meminfo: %s", strerror(errno));
        return 0;
    }
    nread = read(fd, readbuf, sizeof(readbuf)-1);
    if (UNLIKELY(nread < 0)) {
        DLOG("Failed to read from /proc/meminfo: %s", strerror(errno));
        close(fd);
        return 0;
    }
    close(fd);
    readbuf[nread] = '\0';

    s = readbuf;
    int found_memfree = 0, found_buffers = 0, found_cached = 0;
    int64_t memfree = 0, buffers = 0, cached = 0;
    while (*s && (!found_memfree || !found_buffers || !found_cached)) {
        char *eol = s + strcspn(s, "\n");
        if (UNLIKELY(!*eol)) {
            break;
        }
        *eol++ = 0;
        char *tag = s;
        s = eol;
        char *value = tag + strcspn(tag, ":");
        ASSERT(*value, continue);
        *value++ = 0;
        if (strcmp(tag, "MemFree") == 0) {
            found_memfree = 1;
            memfree += (int64_t)strtol(value, NULL, 10) * 1024;
        } else if (strcmp(tag, "Buffers") == 0) {
            found_buffers = 1;
            buffers += (int64_t)strtol(value, NULL, 10) * 1024;
        } else if (strcmp(tag, "Cached") == 0) {
            found_cached = 1;
            cached += (int64_t)strtol(value, NULL, 10) * 1024;
        }
    }

    if (UNLIKELY(!found_memfree)) {
        DLOG("Failed to find MemFree tag in /proc/meminfo");
        return 0;
    }
    if (UNLIKELY(!found_buffers)) {
        DLOG("Failed to find Buffers tag in /proc/meminfo");
        return 0;
    }
    if (UNLIKELY(!found_cached)) {
        DLOG("Failed to find Cached tag in /proc/meminfo");
        return 0;
    }
    return memfree + buffers + cached;
}

/*************************************************************************/
/*************************************************************************/
