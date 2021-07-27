/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * include/SIL/sysdep/windows/common.h: Global declarations specific to
 * Windows.
 */

#ifndef SIL_SYSDEP_WINDOWS_COMMON_H
#define SIL_SYSDEP_WINDOWS_COMMON_H

EXTERN_C_BEGIN

/*************************************************************************/
/*************************************************************************/

/* Work around Windows environmental pollution. */
#undef near
#undef far
#define near near_
#define far far_
#undef _ptr32
#define _ptr32 _ptr32_

/*-----------------------------------------------------------------------*/

/**
 * windows_set_touch_to_mouse:  Convert touch input events to mouse events.
 * When enabled, all touch events will be passed to the default Windows
 * handler, which converts touch events to their mouse equivalents; the
 * touch events themselves will not be passed to the client program.
 *
 * This is intennded for programs which use mouse but not touch input,
 * allowing basic touch support without explicitly handling touch events.
 *
 * [Parameters]
 *     enable: True to enable touch-to-mouse conversion, false to disable.
 */
extern void windows_set_touch_to_mouse(int enable);

/*************************************************************************/
/*************************************************************************/

EXTERN_C_END

#endif  // SIL_SYSDEP_WINDOWS_COMMON_H
