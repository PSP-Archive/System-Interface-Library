/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/math/fpu.h: Header for configuring the floating-point environment.
 */

#ifndef SIL_SRC_MATH_FPU_H
#define SIL_SRC_MATH_FPU_H

/*************************************************************************/
/*************************************************************************/

/**
 * fpu_configure:  Configure the CPU's floating-point environment.
 *
 * This function subsumes architecture-specific code to set processor
 * registers so that floating-point computations by SIL programs give
 * the same results regardless of environment.  Of necessity, much of
 * the implementation is architecture-dependent, but this code has been
 * extracted from the sysdep sources since it can generally be shared
 * across all platforms that use the same CPU family.
 *
 * This function is not called from common SIL code; the platform's
 * main() function or other initialization code should call it unless
 * the platform provides an alternative interface to perform the same
 * operations.
 *
 * The floating-point environment established by this function is:
 *     - Rounding mode is set to round-to-nearest.
 *     - Denormal results are flushed to zero.
 *     - If DEBUG is not defined, all floating-point exceptions are disabled.
 */
extern void fpu_configure(void);

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_MATH_FPU_H
