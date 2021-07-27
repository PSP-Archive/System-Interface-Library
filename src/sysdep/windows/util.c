/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/windows/util.c: Internal utility functions for Windows.
 */

#define IN_SYSDEP

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sysdep.h"
#include "src/sysdep/windows/internal.h"
#include "src/utility/utf8.h"

/*************************************************************************/
/*************************************************************************/

int strcmp_16(const uint16_t *a, const uint16_t *b)
{
    PRECOND(a != NULL, return b ? -1 : 0);
    PRECOND(b != NULL, return 1);

    for (; *a != 0 && *b != 0; a++, b++) {
        if (*a != *b) {
            break;
        }
    }
    return (int)*a - (int)*b;
}

/*-----------------------------------------------------------------------*/

uint16_t *strdup_16(const uint16_t *s)
{
    PRECOND(s != NULL, return NULL);

    int len = 0;
    while (s[len] != 0) {
        len++;
    }
    uint16_t *copy = mem_alloc((len+1)*2, 2, 0);
    if (UNLIKELY(!copy)) {
        return NULL;
    }
    memcpy(copy, s, (len+1)*2);
    return copy;
}

/*-----------------------------------------------------------------------*/

char *strdup_16to8(const uint16_t *s16)
{
    PRECOND(s16 != NULL, return NULL);

    /* Determine how big a buffer we need.  The maximum UTF-8 length of a
     * non-surrogate UTF-16 character is 3 bytes (U+FFFF => EF BF BF),
     * while the maximum length of a UTF-16 surrogate pair is 4 bytes
     * (U+10FFFF => F4 8F BF BF), so the worst-case scenario is 3 bytes
     * per 16-bit unit in the string. */
    int len16 = 0;
    while (s16[len16] != 0) {
        len16++;
    }
    const int max_len8 = len16 * 3;

    char *buffer = mem_alloc(max_len8 + 1, 1, 0);
    if (UNLIKELY(!buffer)) {
        return NULL;
    }

    convert_16to8(s16, buffer);
    return buffer;
}

/*-----------------------------------------------------------------------*/

void convert_16to8(const uint16_t *s16, char *buffer)
{
    char *s8 = buffer;
    for (int i = 0; s16[i] != 0; i++) {
        const uint16_t ch16 = s16[i];
        if (ch16 < 0x80) {
            *s8++ = (char)ch16;
        } else if (ch16 < 0x800) {
            *s8++ = 0xC0 | ((ch16 >> 6) & 0x1F);
            *s8++ = 0x80 | ((ch16 >> 0) & 0x3F);
        } else if (ch16 >= 0xD800 && ch16 <= 0xDBFF) {
            const uint16_t ch16_2 = s16[i+1];
            if (ch16_2 >= 0xDC00 && ch16_2 <= 0xDFFF) {
                const uint32_t codepoint =
                    0x10000 + ((ch16 & 0x3FF) << 10 | (ch16_2 & 0x3FF));
                *s8++ = 0xF0 | ((codepoint >> 18) & 0x07);
                *s8++ = 0x80 | ((codepoint >> 12) & 0x3F);
                *s8++ = 0x80 | ((codepoint >>  6) & 0x3F);
                *s8++ = 0x80 | ((codepoint >>  0) & 0x3F);
                i++;
            } else {
                *s8++ = 0xEF;
                *s8++ = 0xBF;
                *s8++ = 0xBD;
            }
        } else if (ch16 >= 0xDC00 && ch16 <= 0xDFFF) {
            *s8++ = 0xEF;
            *s8++ = 0xBF;
            *s8++ = 0xBD;
        } else {
            *s8++ = 0xE0 | ((ch16 >> 12) & 0x0F);
            *s8++ = 0x80 | ((ch16 >>  6) & 0x3F);
            *s8++ = 0x80 | ((ch16 >>  0) & 0x3F);
        }
    }
    *s8 = '\0';
}

/*-----------------------------------------------------------------------*/

uint16_t *strdup_8to16(const char *s8)
{
    PRECOND(s8 != NULL, return NULL);

    /* Determine how big a buffer we need.  The worst-case scenario is if
     * the entire UTF-8 string is 1-byte characters (ASCII), in which case
     * the UTF-16 string will have as many elements as the UTF-8 string
     * has bytes. */
    const int max_len16 = strlen(s8);

    uint16_t *buffer = mem_alloc((max_len16 + 1) * 2, 2, 0);
    if (UNLIKELY(!buffer)) {
        return NULL;
    }

    uint16_t *s16 = buffer;
    for (int32_t ch; (ch = utf8_read(&s8)) != 0; ) {
        if (ch < 0) {
            /* Ignore. */
        } else if ((ch >= 0xD800 && ch <= 0xDFFF) || ch >= 0x110000) {
            *s16++ = 0xFFFD;
        } else if (ch < 0x10000) {
            *s16++ = ch;
        } else {
            ch -= 0x10000;  // Now in the range [0,0xFFFFF].
            *s16++ = 0xD800 | (ch >> 10);
            *s16++ = 0xDC00 | (ch & 0x3FF);
        }
    }

    ASSERT(s16 - buffer <= max_len16);
    *s16 = 0;
    return buffer;
}

/*-----------------------------------------------------------------------*/

DWORD timeout_to_ms(float timeout)
{
    return timeout < 0 ? INFINITE : (DWORD)ceilf(timeout*1000);
}

/*-----------------------------------------------------------------------*/

char *windows_getenv(const char *name)
{
    PRECOND(name != NULL, return NULL);

    SetLastError(0);
    DWORD size = GetEnvironmentVariable(name, NULL, 0);
    if (size == 0) {
        /* Some versions of Windows (at least XP) and Wine (at least
         * through 1.7.33) return 0 instead of 1 for an empty variable,
         * so we also need to check the error code. */
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
            return NULL;
        } else {
            size = 1;
        }
    }

    char *buffer = mem_alloc(size, 1, MEM_ALLOC_TEMP);
    if (UNLIKELY(!buffer)) {
        DLOG("No memory for value of environment variable %s (%d bytes)",
             name, size);
        return NULL;
    }
    DWORD result = GetEnvironmentVariable(name, buffer, size);
    if (UNLIKELY(result != size-1)) {
        DLOG("Failed to copy value of environment variable %s", name);
        mem_free(buffer);
        return NULL;
    }
    if (result == 0) {
        /* Again, some versions of Windows and Wine are broken for this
         * case, so we need to manually terminate the (empty) string. */
        *buffer = '\0';
    }

    ASSERT(result < size);
    ASSERT(strlen(buffer) == result);
    return buffer;
}

/*-----------------------------------------------------------------------*/

const char *windows_strerror(DWORD code)
{
    static char buf[1000];

    /* Make sure FormatMessage() doesn't change the error code. */
    const DWORD saved_error_code = GetLastError();

    /* We force English to avoid corruption of UTF-8 by the stdio library;
     * otherwise we'd use MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT).  This
     * text normally isn't shown to users, so we don't worry too much about
     * this lack of locale support. */
    const unsigned int lang = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

    uint16_t message16[1000];
    char message[3000];
    int message_len = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code,
        lang, message16, lenof(message16), NULL);
    if (message_len > 0) {
        ASSERT(message_len < lenof(message16),
               message_len = lenof(message16) - 1; message16[message_len] = 0);
        if (message16[message_len-1] == '\n') {
            message16[--message_len] = 0;
        }
        if (message_len > 0 && message16[message_len-1] == '\r') {
            message16[--message_len] = 0;
        }
        convert_16to8(message16, message);
        strformat(buf, sizeof(buf), "%08X: %s", code, message);
    } else {
        strformat(buf, sizeof(buf), "%08X", code);
    }

    SetLastError(saved_error_code);
    return buf;
}

/*-----------------------------------------------------------------------*/

int windows_version(void)
{
    /* GetVersion() would be the obvious way to do this, but Microsoft
     * deprecated it in Windows 8.1, so we need to do more work ourselves. */
    static const int version_table[] = {
        /* Must be in latest-to-earliest order! */
        WINDOWS_VERSION_10,
        WINDOWS_VERSION_8_1,
        WINDOWS_VERSION_8,
        WINDOWS_VERSION_7,
        WINDOWS_VERSION_VISTA,
        WINDOWS_VERSION_XP,
        WINDOWS_VERSION_2000,
    };
    for (int i = 0; i < lenof(version_table); i++) {
        if (windows_version_is_at_least(version_table[i])) {
            return version_table[i];
        }
    }
    /* Under certain conditions, it seems that VerifyVersionInfo() can
     * fail on Windows XP.  This is probably a bug, so we fall back to
     * GetVersion() in that case.  We use GetProcAddress() to avoid
     * linking to the function statically in case it's eventually removed
     * from the API. */
    DLOG("VerifyVersionInfo() broken, using GetVersion() instead");
    DWORD (WINAPI *p_GetVersion)(void) =
        (void *)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetVersion");
    if (p_GetVersion) {
        return (*p_GetVersion)();
    }
    DLOG("GetVersion() unavailable, returning Windows 2000");
    return WINDOWS_VERSION_2000;
}

/*-----------------------------------------------------------------------*/

int windows_version_is_at_least(int version)
{
    OSVERSIONINFOEXW version_info;
    mem_clear(&version_info, sizeof(version_info));
    version_info.dwOSVersionInfoSize = sizeof(version_info);
    version_info.dwMajorVersion = (version >> 8) & 0xFF;
    version_info.dwMinorVersion = (version >> 0) & 0xFF;
    DWORD flags = (VER_MAJORVERSION | VER_MINORVERSION
                   | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR);
    ULONGLONG conditions = 0;
    VER_SET_CONDITION(conditions, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditions, VER_MINORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditions, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditions, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL);
    return VerifyVersionInfoW(&version_info, flags, conditions);
}

/*************************************************************************/
/*************************************************************************/
