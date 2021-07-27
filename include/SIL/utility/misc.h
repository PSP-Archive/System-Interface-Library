/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/utility/misc.h: Header for miscellaneous utility functions.
 */

#ifndef SIL_UTILITY_MISC_H
#define SIL_UTILITY_MISC_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/**
 * can_open_file, can_open_url:  Check whether the system is capable of
 * opening arbitrary local files in external programs or browsing the web,
 * respectively.  If this function returns false, open_file() resp.
 * open_url() will never succeed.  (However, a true return does not
 * guarantee that an open call will always succeed; it may still fail due
 * to insufficient resources, incorrect system settings, etc.)
 *
 * [Return value]
 *     True if the system is capable of opening local files or browsing the
 *     web, false if not.
 */
extern int can_open_file(void);
extern int can_open_url(void);

/**
 * console_printf:  Print a formatted string to the console device, if one
 * is available.  Standard printf() tokens may be used in the string.
 *
 * On systems without a console device (such as Windows when the program
 * is not linked in console mode), this function does nothing.
 *
 * [Parameters]
 *     format: printf()-style format string for text to output.
 *     ...: Format arguments for text.
 */
extern void console_printf(const char *format, ...) FORMAT(1,2);

/**
 * display_error:  Display an error message to the user.  Depending on the
 * system, this routine may wait for the user to confirm the error message
 * (for example, by clicking an "OK" button) before returning.
 *
 * [Parameters]
 *     message: Error message to display (may contain printf()-style
 *         formatting tokens).
 *     ...: Format arguments for error message.
 */
extern void display_error(const char *message, ...) FORMAT(1,2);

/**
 * get_system_language:  Return the system's preferred language as a
 * 2-letter ISO 639-1 language code (e.g., "en" for English), along with
 * an optional 2-letter ISO 3166 region code identifying the dialect
 * (e.g., "UK" for British English).  These value may change while the
 * program is running.
 *
 * The "index" parameter may be used to iterate through a sequence of
 * preferred languages on systems which support such a concept.  In this
 * case, index 0 returns the system's current default (most preferred)
 * language, and subsequent index values return less-preferred languages
 * in order of preference.  If the system only supports a single system
 * language, index 0 will return that language, and all other index values
 * will return false (failure).
 *
 * If the system reports a language preference for the given index but
 * does not include a dialect for the language, this function will return
 * true (success) but *dialect_ret will be an empty string.
 *
 * The returned strings are stored in a static buffer, and will be
 * overwritten on the next call to this function.  The values of
 * *language_ret and *dialect_ret are not modified if the function returns
 * false.
 *
 * [Parameters]
 *     index: Language preference index (0 = system default language).
 *     language_ret: Pointer to variable to receive the language code
 *         (may be NULL).
 *     dialect_ret: Pointer to variable to receive the dialect code
 *         (may be NULL).
 * [Return value]
 *     True if the language preference specified by index exists, false if not.
 */
extern int get_system_language(unsigned int index, const char **language_ret,
                               const char **dialect_ret);

/**
 * default_dialect_for_language:  Return a reasonable default dialect for
 * the given language; for example, this function returns "US" for language
 * "en", "JP" for "ja", and so on.  If there is no reasonable default for
 * the given language, an empty string is returned.
 *
 * This is intended only as a convenience function for client code, to
 * provide reasonable defaults on systems which do not provide dialect
 * information themselves.  Client code is free to use its own defaults
 * instead; for example, a program written for a British audience might
 * choose a default locale of "UK" for English, rather than the "US"
 * returned by this function.
 *
 * [Parameters]
 *     language: ISO 639-1 language code (as returned by
 *         get_system_language()).
 * [Return value]
 *     Default dialect, or the empty string if unknown.
 */
extern const char *default_dialect_for_language(const char *language);

/**
 * open_file, open_url:  Open a local file in an appropriate external
 * program, or open a URL in the user's browser.  Fails on systems without
 * relevant system capabilities (such as multitasking or networking), and
 * may fail for other system-dependent reasons, possibly without notifying
 * the caller of the failure (thus a nonzero return value from this
 * function does not guarantee that the file or URL has been opened).
 *
 * [Parameters]
 *     path: Path of file to open (for open_file()).
 *     url: URL to open (for open_url()).
 * [Return value]
 *     False if an error is known to have occurred, true otherwise.
 */
extern int open_file(const char *path);
extern int open_url(const char *url);

/**
 * reset_idle_timer:  Reset any system "idle timers", such as screensaver
 * activation or auto-suspend timers.  This function should be called
 * periodically (e.g., once per frame) during cutscenes or similar cases
 * in which no user input is expected for extended periods of time, to
 * prevent such system functionality from interrupting gameplay.
 */
extern void reset_idle_timer(void);

/**
 * set_performance_level:  Request the system to switch to a different
 * performance level.  The level can be any of:
 *
 * - A PERFORMANCE_LEVEL_* constant, requesting a generalized performance
 *   level;
 *
 * - Zero (equivalent to PERFORMANCE_LEVEL_DEFAULT), requesting the system's
 *   default performance level; or
 *
 * - A positive value, specifying a system-dependent parameter such as a
 *   clock speed (see the definition of sys_set_performance_level() in
 *   src/sysdep/.../misc.c for details on how each system interprets this
 *   value).
 *
 * Note that changing the performance level may take a significant amount of
 * time (hundreds of milliseconds) and may cause audiovisual interruptions.
 *
 * [Parameters]
 *     level: New performance level (PERFORMANCE_LEVEL_* or a
 *         system-dependent positive value).
 * [Return value]
 *     True on success, false on error.
 */
extern int set_performance_level(int level);
enum {
    PERFORMANCE_LEVEL_DEFAULT = 0,  // System default.
    PERFORMANCE_LEVEL_HIGH = -1,    // Best performance possible.
    PERFORMANCE_LEVEL_LOW = -2,     // Low performance, reduced power usage.
};

/**
 * split_args:  Split the string s into individual arguments at unquoted
 * whitespace characters, and return a newly-allocated (using mem_alloc())
 * array containing pointers to the arguments.  The pointers point into s,
 * which is destroyed by this function; the array is terminated with a
 * pointer value of NULL (thus the array has (*argc_ret)+1 elements
 * allocated).
 *
 * Quote handling follows POSIX shell rules:
 *    - Whitespace after a backslash or inside quotes does not end the
 *      argument.
 *    - Outside quotes, a backslash before any character escapes that
 *      character and the backslash is removed.  If the escaped character
 *      is a newline, it is also removed.
 *    - Inside single quotes, backslash has no special meaning.
 *    - Inside double quotes, backslash escapes only: $ ` " <newline>
 *
 * If insert_dummy is true, an element pointing to a constant empty string
 * will be inserted as the first element in the argument array.
 *
 * On error, *argc_ret and *argv_ret are left unmodified, but the input
 * string is still destroyed.
 *
 * [Parameters]
 *     s: String containing arguments to split (destroyed).
 *     insert_dummy: True to insert a NULL element as (*argv_ret)[0].
 *     argc_ret: Pointer to variable to receive the length of *argv_ret.
 *     argv_ret: Pointer to variable to receive the argument array.
 * [Return value]
 *     True on success, false on invalid parameter or out of memory.
 */
extern int split_args(char *s, int insert_dummy,
                      int *argc_ret, char ***argv_ret);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_UTILITY_MISC_H
