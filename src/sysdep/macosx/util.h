/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/macosx/util.h: Header for utility functions internal to Mac
 * OS X code.
 */

#ifndef SIL_SRC_SYSDEP_MACOSX_UTIL_H
#define SIL_SRC_SYSDEP_MACOSX_UTIL_H

#include <CoreGraphics/CoreGraphics.h>

/*************************************************************************/
/*************************************************************************/

/**
 * macosx_get_application_name:  Return the name of the current
 * application, or "The application" (capitalized) if the application name
 * cannot be determined.
 *
 * [Return value]
 *     Application name.
 */
extern const char *macosx_get_application_name(void);

/**
 * macosx_get_application_support_path:  Return the pathname of the
 * "Application Support" directory for the current application.
 *
 * This function always succeeds.  The returned value is stored in a
 * static buffer.
 *
 * [Return value]
 *     "Application Support" directory pathname.
 */
extern const char *macosx_get_application_support_path(void);

/**
 * macosx_create_image:  Create an NSImage instance from the given pixel data.
 *
 * The return type is declared as "void *" to avoid errors when included in
 * C source.
 *
 * [Parameters]
 *     data: Pointer to RGBA8888-format pixel data.
 *     width: Width of image, in pixels.
 *     height: Height of image, in pixels.
 * [Return value]
 *     Newly created NSImage instance, or NULL on error.
 */
extern void *macosx_create_image(const void *data, int width, int height);

/**
 * macosx_window_frame_to_CGRect:  Convert window frame coordinates (origin
 * in the lower-left corner) to CoreGraphics-style coordinates (origin in
 * the upper-left corner).
 *
 * [Parameters]
 *     frame: Window frame to convert.
 * [Return value]
 *     "frame" converted to CoreGraphics-style coordinates.
 */
extern CGRect macosx_window_frame_to_CGRect(CGRect frame);

/**
 * macosx_show_dialog_formatted:  Call macosx_dialog() with the localized
 * strings looked up from the given resource IDs.  The dialog text is
 * treated as a printf()-style format string.
 *
 * [Parameters]
 *     title_id: Resource ID for dialog title.
 *     format_id: Resource ID for dialog text format string.
 *     ...: Format arguments for dialog text.
 */
extern void macosx_show_dialog_formatted(const char *title_id,
                                         const char *format_id, ...);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_SYSDEP_MACOSX_UTIL_H
