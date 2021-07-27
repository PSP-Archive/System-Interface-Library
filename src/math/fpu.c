/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/math/fpu.c: Functions for configuring the floating-point environment.
 */

#include "src/base.h"
#include "src/math/fpu.h"

#if defined(SIL_ARCH_X86)
# if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 5
/* Work around GCC 5.x silently including <mm_malloc.h> which defines
 * inline functions which call malloc(). */
#  define _MM_MALLOC_H_INCLUDED
# endif
# include <xmmintrin.h>
#endif

/*************************************************************************/
/*************************************************************************/

void fpu_configure(void)
{
    #if defined(SIL_ARCH_X86)

        uint32_t mxcsr = _mm_getcsr();
        mxcsr &= ~(3<<13);  // RC (00 = round to nearest)
        mxcsr |= 1<<6       // DAZ
              |  1<<7       // EM_INVALID
              |  1<<8       // EM_DENORMAL
              |  1<<9       // EM_ZERODIVIDE
              |  1<<10      // EM_OVERFLOW
              |  1<<11      // EM_UNDERFLOW
              |  1<<12      // EM_PRECISION
              |  1<<15;     // FZ
        _mm_setcsr(mxcsr);

    #elif defined(__GNUC__)

        #if defined(SIL_ARCH_ARM_32)

            #ifndef SIL_PLATFORM_ANDROID
                uint32_t fpscr;
                __asm__("vmrs %0, fpscr" : "=r" (fpscr));
                fpscr &= ~(0x9F<<8);  // Disable exceptions.
                fpscr &= ~(3<<22);    // Round to nearest value.
                fpscr |= 1<<24;       // Flush denormals to zero.
                __asm__("vmsr fpscr, %0" : /* no outputs */ : "r" (fpscr));
            #else
                /* Apparently the Android NDK compiler doesn't understand
                 * VMRS/VMSR (which go all the way back to VFPv2, so lack
                 * of NEON support isn't an excuse!), so we have to encode
                 * them manually. */
                __asm__ volatile(
                    ".word 0xEEF10A10\n"  // vmrs r0, fpscr
                    "and r0, r0, #~(0x9F<<8)\n"
                    "and r0, r0, #~(3<<22)\n"
                    "orr r0, r0, #(1<<24)\n"
                    ".word 0xEEE10A10\n"  // vmsr fpscr, r0
                    : /* no outputs */
                    : /* no inputs */
                    : "r0");
            #endif  // SIL_PLATFORM_ANDROID

        #elif defined(SIL_ARCH_ARM_64)

            uint64_t fpcr;
            __asm__("mrs %0, fpcr" : "=r" (fpcr));
            fpcr &= ~(0x9F<<8);  // Disable exceptions.
            fpcr &= ~(3<<22);    // Round to nearest value.
            fpcr |= 1<<24;       // Flush denormals to zero.
            __asm__("msr fpcr, %0" : /* no outputs */ : "r" (fpcr));

        #elif defined(SIL_ARCH_MIPS)

            uint32_t fpucr;
            __asm__("cfc1 %[fpucr], $31" : [fpucr] "=r" (fpucr));
            fpucr &= ~(0x1F << 7);  // Disable all exceptions.
            fpucr &= ~3;            // Round to nearest value.
            fpucr |= 1<<24;         // Flush denormals to zero.
            __asm__("ctc1 %[fpucr], $31" : : [fpucr] "r" (fpucr));

        #else  // !ARM && !MIPS && !X86

            #error Cannot set floating-point mode flags on this CPU!

        #endif

    #else  // !__GNUC__

        #error Cannot set floating-point mode flags with this compiler!

    #endif
}

/*************************************************************************/
/*************************************************************************/
