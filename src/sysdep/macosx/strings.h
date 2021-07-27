/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/strings.h: Header for using OSX/iOS localizable strings.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_STRINGS_H
#define SIL_SRC_SYSDEP_MACOSX_STRINGS_H

#include <CoreFoundation/CoreFoundation.h>

/*************************************************************************/
/*************************************************************************/

/**
 * CopyStringResource:  Return the localized string for the given key,
 * according to the current system language.  The returned string must be
 * freed with CFRelease(), following the Create Rule.
 *
 * If the key does not exist in the string resource file, the key itself
 * is returned.
 *
 * [Parameters]
 *     key: String key (must be a string literal).
 * [Return value]
 *     Reference to a newly allocated CFString with the string text.
 */

#define CopyStringResource(key) \
    CFBundleCopyLocalizedString(CFBundleGetMainBundle(), \
                                CFSTR(key), CFSTR(key), CFSTR("SIL"))

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_STRINGS_H
