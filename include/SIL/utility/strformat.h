/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/utility/strformat.h: String formatting utility header.
 */

#ifndef SIL_UTILITY_STRFORMAT_H
#define SIL_UTILITY_STRFORMAT_H

/*************************************************************************/
/*************************************************************************/

/* Declarations of [v]strformat() and [v]strformat_check() are located in
 * ../base.h. */

/*-----------------------------------------------------------------------*/

/**
 * strformat_enable_fullwidth():  Set whether to use the "+" modifier to
 * convert numeric output from strformat() to fullwidth characters.  This
 * can be useful for languages such as Japanese which use wide (fullwidth)
 * characters by default: call this function unconditionally with a true
 * argument at program startup, then call strformat_set_fullwidth() with
 * an appropriate argument based on the user's selected language, for
 * example.  This allows format strings to use the "+" modifier without
 * regard to the current setting, and strformat() will output the correct
 * character type based on the strformat_set_fullwidth() toggle.
 *
 * If "enable" is true, the "+" modifier will cause signed numeric
 * conversions (the %d, %e, %E, %f, %F, %g, %G, and %i tokens) to be
 * formatted using fullwidth Unicode characters (U+FFxx) instead of
 * ASCII characters based on the setting of strformat_set_fullwidth().
 *
 * If "enable" is false (the default), the "+" modifier will behave as
 * specified in C99: nonnegative values for signed conversions will have a
 * "+" prefixed to the value.
 *
 * [Parameters]
 *     enable: True to use the "+" modifier for optional fullwidth
 *         character output; false to use the "+" modifier to prefix a
 *         "+" to nonnegative numeric values.
 */
extern void strformat_enable_fullwidth(int enable);

/*-----------------------------------------------------------------------*/

/**
 * strformat_set_fullwidth():  Set whether to use fullwidth characters or
 * ordinary ASCII characters for the "+" modifier (%+d, etc.) when this
 * processing is enabled with strformat_enable_fullwidth().  The default
 * is to use ordinary ASCII characters.
 *
 * If fullwidth conversion is disabled, this function has no effect, but
 * the state of the setting is retained in case the functionality is
 * enabled with a subsequent call to strformat_enable_fullwidth().
 *
 * [Parameters]
 *     fullwidth: True to use fullwidth characters (U+FFxx), false to use
 *         ordinary ASCII characters.
 */
extern void strformat_set_fullwidth(int fullwidth);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_UTILITY_STRFORMAT_H
