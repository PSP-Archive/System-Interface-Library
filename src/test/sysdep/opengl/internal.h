/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/test/sysdep/opengl/internal.h: Declarations of internal helper
 * routines for OpenGL tests.
 */

#ifndef SIL_SRC_TEST_SYSDEP_OPENGL_INTERNAL_H
#define SIL_SRC_TEST_SYSDEP_OPENGL_INTERNAL_H

/*************************************************************************/
/*************************************************************************/

/**
 * opengl_has_features_uninitted:  Check whether the system's OpenGL
 * implementation supports all of the given features.  This function
 * should be called in place of opengl_has_features() when the graphics
 * subsystem has not been initialized.
 *
 * [Parameters]
 *     features: Features to check.
 * [Return value]
 *     True if all of the specified features are available, false otherwise.
 */
extern int opengl_has_features_uninitted(unsigned int features);

/**
 * opengl_has_formats_uninitted:  Check whether the system's OpenGL
 * implementation supports all of the given formats.  This function
 * should be called in place of opengl_has_formats() when the graphics
 * subsystem has not been initialized.
 *
 * [Parameters]
 *     formats: Formats to check.
 * [Return value]
 *     True if all of the specified formats are available, false otherwise.
 */
extern int opengl_has_formats_uninitted(unsigned int formats);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_TEST_SYSDEP_OPENGL_INTERNAL_H
