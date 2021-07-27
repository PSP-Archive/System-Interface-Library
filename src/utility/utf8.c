/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/utf8.c: Utility routines for handling UTF-8-encoded strings.
 */

#include "src/base.h"
#include "src/utility/utf8.h"

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
