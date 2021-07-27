/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * tools/sse2.h: Macros for using SSE2 assembly on x86 or x86-64 CPUs.
 * Define USE_SSE2 and (if compiling for x86-64) ARCH_AMD64 before
 * including this header.
 */

#ifndef SIL_TOOLS_SSE2_H
#define SIL_TOOLS_SSE2_H

#ifdef USE_SSE2  // Only enable this file when SSE2 is available.

/*************************************************************************/

/* Macro to force the compiler not to use any of the XMM registers. */
#define SSE2_INIT \
    asm volatile("":::"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7")

/*************************************************************************/

/*
 * When building on AMD64 (x86_64), the base registers used in memory
 * access must be the 64-bit Rxx registers (RAX, RBX, etc.) rather than
 * the 32-bit Exx registers, but 32-bit x86 doesn't have the Rxx registers
 * in the first place.  Similarly, AMD64 has PC-relative addressing, but
 * on 32-bit x86 we have to use a relocation instead.  We use these macros
 * to allow writing code that will assemble on either CPU without needing
 * #ifdefs scattered throughout the assembly.
 */

#ifdef ARCH_AMD64

# define EAX "%%rax"
# define EBX "%%rbx"
# define ECX "%%rcx"
# define EDX "%%rdx"
# define ESP "%%rsp"
# define EBP "%%rbp"
# define ESI "%%rsi"
# define EDI "%%rdi"
# define LABEL(l) #l"(%%rip)"
# define PTRSIZE 8
# define PTRSIZE_STR "8"

#else

# define EAX "%%eax"
# define EBX "%%ebx"
# define ECX "%%ecx"
# define EDX "%%edx"
# define ESP "%%esp"
# define EBP "%%ebp"
# define ESI "%%esi"
# define EDI "%%edi"
# define LABEL(l) #l
# define PTRSIZE 4
# define PTRSIZE_STR "4"

#endif

/*************************************************************************/

#endif  // USE_SSE2
#endif  // SIL_TOOLS_SSE2_H
