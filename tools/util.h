/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/util.h: Header for tool utility functions.
 */

#ifndef SIL_TOOLS_UTIL_H
#define SIL_TOOLS_UTIL_H

/*************************************************************************/
/*************************************************************************/

/**
 * read_file:  Read the given file into memory.
 *
 * The errno variable will be set appropriately on error.
 *
 * [Parameters]
 *     filename: Path of filename to read.
 *     size_ret: Pointer to variable to receive file size, in bytes.
 * [Return value]
 *     File data buffer (allocated with malloc()), or NULL on error.
 */
extern void *read_file(const char *filename, uint32_t *size_ret);

/**
 * write_file:  Write the given file to the filesystem.
 *
 * The errno variable will be set appropriately on error.
 *
 * [Parameters]
 *     filename: Path of filename to write.
 *     data: Pointer to data buffer.
 *     size: Data size, in bytes.
 * [Return value]
 *     True on success, false on error.
 */
extern int write_file(const char *filename, const void *data, uint32_t size);

/*-----------------------------------------------------------------------*/

/**
 * utf8_read:  Read a single UTF-8 character from the given string pointer,
 * advancing the string pointer past the character.  If the string pointer
 * points to an invalid UTF-8 byte sequence, the pointer is advanced by
 * one byte and -1 is returned.
 *
 * This is identical to the code in ../utility/utf8.c; it is copied here
 * to avoid dependencies on core headers.
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
 * This is identical to the code in ../utility/utf8.c; it is copied here
 * to avoid dependencies on core headers.
 *
 * [Parameters]
 *     s: String pointer (must be non-NULL).
 * [Return value]
 *     Length in bytes of first UTF-8 byte sequence, or zero if the string
 *     is empty or the byte sequence is invalid.
 */
extern int utf8_charlen(const char *s);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_TOOLS_UTIL_H
