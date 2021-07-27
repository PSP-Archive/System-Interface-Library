/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/util.c: Common utility functions for tool programs.
 */

#include "tool-common.h"
#include "util.h"

#include <errno.h>

/*************************************************************************/
/*************************************************************************/

void *read_file(const char *filename, uint32_t *size_ret)
{
    if (!filename) {
        fprintf(stderr, "read_file(): filename == NULL\n");
        errno = EINVAL;
        goto error_return;
    }
    if (!size_ret) {
        fprintf(stderr, "read_file(): size_ret == NULL\n");
        errno = EINVAL;
        goto error_return;
    }

    FILE *f = fopen(filename, "rb");
    if (!f) {
        goto error_return;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        goto error_fclose;
    }
    uint32_t filesize = ftell(f);
    uint8_t *data = malloc(filesize);
    if (!data) {
        goto error_fclose;
    }
    fseek(f, 0, SEEK_SET);
    if (fread(data, filesize, 1, f) != 1) {
        errno = EAGAIN;
        goto error_free;
    }
    fclose(f);
    *size_ret = filesize;
    return data;

  error_free:
    {
        const int errno_save = errno;
        free(data);
        errno = errno_save;
    }
  error_fclose:
    {
        const int errno_save = errno;
        fclose(f);
        errno = errno_save;
    }
  error_return:
    return NULL;
}

/*-----------------------------------------------------------------------*/

int write_file(const char *filename, const void *data, uint32_t size)
{
    if (!filename) {
        fprintf(stderr, "save_file(): filename == NULL\n");
        errno = EINVAL;
        goto error_return;
    }
    if (!data) {
        fprintf(stderr, "save_file(): data == NULL\n");
        errno = EINVAL;
        goto error_return;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        goto error_return;
    }
    if (size > 0 && fwrite(data, size, 1, f) != 1) {
        goto error_remove;
    }
    fclose(f);
    return 1;

  error_remove:
    {
        const int errno_save = errno;
        fclose(f);
        remove(filename);
        errno = errno_save;
    }
  error_return:
    return 0;
}

/*************************************************************************/
/*************************************************************************/

int32_t utf8_read(const char **strptr)
{
    PRECOND(strptr != NULL, return 0);
    PRECOND(*strptr != NULL, return 0);

    const unsigned char *us = (const unsigned char *)(*strptr);
    if (*us < 0x80) {
        if (*us) {  // Don't advance past the terminating null!
            (*strptr)++;
        }
        return *us;
    } else if (*us < 0xC0) {
      invalid:
        (*strptr)++;
        return -1;
    } else if (*us < 0xE0) {
        if (us[1] >= 0x80 && us[1] < 0xC0) {
            (*strptr) += 2;
            return (us[0] & 0x1F) <<  6
                 | (us[1] & 0x3F) <<  0;
        } else {
            goto invalid;
        }
    } else if (*us < 0xF0) {
        if (us[1] >= 0x80 && us[1] < 0xC0
         && us[2] >= 0x80 && us[2] < 0xC0) {
            (*strptr) += 3;
            return (us[0] & 0x0F) << 12
                 | (us[1] & 0x3F) <<  6
                 | (us[2] & 0x3F) <<  0;
        } else {
            goto invalid;
        }
    } else if (*us < 0xF8) {
        if (us[1] >= 0x80 && us[1] < 0xC0
         && us[2] >= 0x80 && us[2] < 0xC0
         && us[3] >= 0x80 && us[3] < 0xC0) {
            (*strptr) += 4;
            return (us[0] & 0x07) << 18
                 | (us[1] & 0x3F) << 12
                 | (us[2] & 0x3F) <<  6
                 | (us[3] & 0x3F) <<  0;
        } else {
            goto invalid;
        }
    } else if (*us < 0xFC) {
        if (us[1] >= 0x80 && us[1] < 0xC0
         && us[2] >= 0x80 && us[2] < 0xC0
         && us[3] >= 0x80 && us[3] < 0xC0
         && us[4] >= 0x80 && us[4] < 0xC0) {
            (*strptr) += 5;
            return (us[0] & 0x07) << 24
                 | (us[1] & 0x3F) << 18
                 | (us[2] & 0x3F) << 12
                 | (us[3] & 0x3F) <<  6
                 | (us[4] & 0x3F) <<  0;
        } else {
            goto invalid;
        }
    } else {
        if (us[1] >= 0x80 && us[1] < 0xC0
         && us[2] >= 0x80 && us[2] < 0xC0
         && us[3] >= 0x80 && us[3] < 0xC0
         && us[4] >= 0x80 && us[4] < 0xC0
         && us[5] >= 0x80 && us[5] < 0xC0) {
            (*strptr) += 6;
            return (us[0] & 0x07) << 30
                 | (us[1] & 0x3F) << 24
                 | (us[2] & 0x3F) << 18
                 | (us[3] & 0x3F) << 12
                 | (us[4] & 0x3F) <<  6
                 | (us[5] & 0x3F) <<  0;
        } else {
            goto invalid;
        }
    }
}

/*-----------------------------------------------------------------------*/

int utf8_charlen(const char *s)
{
    PRECOND(s != NULL, return 0);

    const unsigned char *us = (const unsigned char *)s;
    if (*us == 0) {
        return 0;
    } else if (*us < 0x80) {
        return 1;
    } else if (*us < 0xC0) {
        return 0;
    } else if (*us < 0xE0) {
        return us[1] >= 0x80 && us[1] < 0xC0 ? 2 : 0;
    } else if (*us < 0xF0) {
        return us[1] >= 0x80 && us[1] < 0xC0
            && us[2] >= 0x80 && us[2] < 0xC0 ? 3 : 0;
    } else if (*us < 0xF8) {
        return us[1] >= 0x80 && us[1] < 0xC0
            && us[2] >= 0x80 && us[2] < 0xC0
            && us[3] >= 0x80 && us[3] < 0xC0 ? 4 : 0;
    } else if (*us < 0xFC) {
        return us[1] >= 0x80 && us[1] < 0xC0
            && us[2] >= 0x80 && us[2] < 0xC0
            && us[3] >= 0x80 && us[3] < 0xC0
            && us[4] >= 0x80 && us[4] < 0xC0 ? 5 : 0;
    } else {
        return us[1] >= 0x80 && us[1] < 0xC0
            && us[2] >= 0x80 && us[2] < 0xC0
            && us[3] >= 0x80 && us[3] < 0xC0
            && us[4] >= 0x80 && us[4] < 0xC0
            && us[5] >= 0x80 && us[5] < 0xC0 ? 6 : 0;
    }
}

/*************************************************************************/
/*************************************************************************/
