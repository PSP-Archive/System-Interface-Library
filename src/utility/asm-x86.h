/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/asm-x86.h: Macros for inline x86 assembly.
 */

#ifndef SIL_SRC_UTILITY_ASM_X86_H
#define SIL_SRC_UTILITY_ASM_X86_H

/*************************************************************************/
/*************************************************************************/

/*
 * On the x86-64 architecture, memory accesses have to go through the
 * 64-bit Rxx registers (RAX, etc.), but 32-bit x86 doesn't have those
 * registers to begin with.  Label addresses also need to be taken using
 * the (%rip) format on x86-64, which isn't available on x86-32.  To paper
 * over these architecture differences, we define macros which can be
 * embedded in inline assembly strings to emit the proper code, much like
 * C99's PRI* macros for printf() format tokens.
 */

#ifdef SIL_ARCH_X86_64

# define PTRSIZE 8
# define PTRSIZE_STR "8"

# define RAX "%%rax"
# define RBX "%%rbx"
# define RCX "%%rcx"
# define RDX "%%rdx"
# define RSP "%%rsp"
# define RBP "%%rbp"
# define RSI "%%rsi"
# define RDI "%%rdi"

# define LABEL(l) #l"(%%rip)"

#else  // !SIL_ARCH_X86_64 -> must be SIL_ARCH_X86_32

# define PTRSIZE 4
# define PTRSIZE_STR "4"

# define RAX "%%eax"
# define RBX "%%ebx"
# define RCX "%%ecx"
# define RDX "%%edx"
# define RSP "%%esp"
# define RBP "%%ebp"
# define RSI "%%esi"
# define RDI "%%edi"

# define LABEL(l) #l

#endif

/*************************************************************************/
/*************************************************************************/

#endif  // SIL_SRC_UTILITY_ASM_X86_H
