/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/utility/utf8.h: Header for UTF-8 utility routines.
 */

#ifndef SIL_UTILITY_UTF8_H
#define SIL_UTILITY_UTF8_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * utf8_read:  Read a single UTF-8 character from the given string pointer,
 * advancing the string pointer past the character.  If the string pointer
 * points to an invalid UTF-8 byte sequence, the pointer is advanced by
 * one byte and -1 is returned.
 *
 * [Parameters]
 *     strptr: Pointer to string pointer from which to read a character.
 * [Return value]
 *     Unicode character value, 0 if the string is empty, or -1 if an
 *     invalid byte sequence is encountered.
 */
extern int32_t utf8_read(const char **strptr);

/**
 * utf8_charlen:  Return the length of the UTF-8 byte sequence for the
 * single character at the given string pointer.
 *
 * [Parameters]
 *     s: String pointer (must be non-NULL).
 * [Return value]
 *     Length, in bytes, of the first UTF-8 byte sequence, or zero if the
 *     string is empty or the byte sequence is invalid.
 */
extern int utf8_charlen(const char *s);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_UTILITY_UTF8_H
