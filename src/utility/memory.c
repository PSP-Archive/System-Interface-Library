/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/utility/memory.c: Memory manipulation utility functions.
 */

#include "src/base.h"
#include "src/sysdep.h"
#include "src/utility/memory.h"

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Local routine definitions. */

/**
 * block_transpose32:  Transpose a single block of 32-bit values.  Helper
 * function for mem_transpose32().
 *
 * [Parameters]
 *     dest: Destination memory pointer.
 *     src: Source memory pointer.
 *     width: Width of source block.
 *     height: Height of source block.
 *     src_stride: Row size of source region.
 *     dest_stride: Row size of destination region.
 */
static inline void block_transpose32(uint32_t *dest, const uint32_t *src,
                                     unsigned int width, unsigned int height,
                                     int src_stride, int dest_stride);

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

void mem_fill32(void *ptr, uint32_t val, size_t size)
{
    if (UNLIKELY(!ptr)) {
        DLOG("ptr == NULL");
        return;
    }
    size_t len = size/4;
    if (UNLIKELY(len == 0)) {
        return;
    }

#if defined(SIL_ARCH_X86) && defined(__GNUC__)
    /* If we have 16-byte alignment, use SSE2 to fill 16 or 128 bytes at
     * a time (unless the memory region is so small it would be faster to
     * just fill normally). */
    if ((((uintptr_t)ptr | ((uintptr_t)len * 4)) & 15) == 0 && len*4 > 128) {
        void *limit = (void *)((uintptr_t)ptr + (len/4)*16);
        void *dummy;
        __asm__ volatile(
            "movd %[val], %%xmm0        \n"
            "mov %[limit], %[dummy]     \n"
            "pshufd $0, %%xmm0, %%xmm0  \n"
            "sub %[ptr], %[dummy]       \n"
            "and $0x7F, %[dummy]        \n"
            "jz 1f                      \n"
         "0: movdqa %%xmm0, (%[ptr])    \n"
            "add $16, %[ptr]            \n"
            "sub $16, %[dummy]          \n"
            "jnz 0b                     \n"
         "1: add $128, %[ptr]           \n"
            "movdqa %%xmm0, -128(%[ptr])\n"
            "movdqa %%xmm0, -112(%[ptr])\n"
            "movdqa %%xmm0, -96(%[ptr]) \n"
            "movdqa %%xmm0, -80(%[ptr]) \n"
            "cmp %[ptr], %[limit]       \n"
            "movdqa %%xmm0, -64(%[ptr]) \n"
            "movdqa %%xmm0, -48(%[ptr]) \n"
            "movdqa %%xmm0, -32(%[ptr]) \n"
            "movdqa %%xmm0, -16(%[ptr]) \n"
            "jnz 1b                     \n"
            : [ptr] "=r" (ptr), [limit] "=r" (limit), [dummy] "=r" (dummy)
            : "0" (ptr), "1" (limit), [val] "r" (val)
            : "xmm0", "cc", "memory"
        );
        return;
    }
#endif

#if defined(SIL_ARCH_MIPS_32) && defined(__GNUC__)
    __asm__(
        ".set push; .set noreorder\n"
        "andi $t1, %[size], 0x1F  \n"
        "beqz $t1, 1f             \n"
        "addu $t0, %[ptr], %[size]\n"
     "0: addiu $t0, $t0, -4       \n"
        "addiu $t1, $t1, -4       \n"
        "bnez $t1, 0b             \n"
        "sw %[val], 0($t0)        \n"
        "beq $t0, %[ptr], 9f      \n"
        "nop                      \n"
     "1: sw %[val], -4($t0)       \n"
        "sw %[val], -8($t0)       \n"
        "sw %[val], -12($t0)      \n"
        "sw %[val], -16($t0)      \n"
        "sw %[val], -20($t0)      \n"
        "sw %[val], -24($t0)      \n"
        "sw %[val], -28($t0)      \n"
        "addiu $t0, $t0, -32      \n"
        "bne $t0, %[ptr], 1b      \n"
        "sw %[val], 0($t0)        \n"
     "9: .set pop"
        : /* no outputs */
        : [ptr] "r" (ptr), [val] "r" (val), [size] "r" (len*4)
        : "t0", "t1", "memory"
    );
    return;
#endif

#if defined(SIL_ARCH_ARM_32) && defined(__GNUC__)
    if (len >= 64/4) {
        register void *_ptr __asm__("r8") = ptr;
        register uint32_t _val __asm__("r0") = val;
        register size_t _size __asm__("r9") = len*4;
        __asm__ volatile(
            "push {r7}               \n"
            "ands r1, %[size], #0x1F \n"
            "add r10, %[ptr], %[size]\n"
            "beq 0f                  \n"
            "rsb r2, r1, #24         \n"
            "add %[ptr], %[ptr], r1  \n"
            "add pc, pc, r2          \n"
            "str r0, [%[ptr], #-28]  \n"
            "str r0, [%[ptr], #-24]  \n"
            "str r0, [%[ptr], #-20]  \n"
            "str r0, [%[ptr], #-16]  \n"
            "str r0, [%[ptr], #-12]  \n"
            "str r0, [%[ptr], #-8]   \n"
            "str r0, [%[ptr], #-4]   \n"
         "0: mov r1, r0              \n"
            "mov r2, r0              \n"
            "mov r3, r0              \n"
            "mov r4, r0              \n"
            "mov r5, r0              \n"
            "mov r6, r0              \n"
            "mov r7, r0              \n"
         "1: stmia %[ptr]!, {r0-r7}  \n"
            "cmp %[ptr], r10         \n"
            "bcc 1b                  \n"
            "pop {r7}"
            : [ptr] "=r" (_ptr)
            : "0" (_ptr), [val] "r" (_val), [size] "r" (_size)
            : "r1", "r2", "r3", "r4", "r5", "r6", "r10", "cc", "memory"
        );
        return;
    }
#endif

#if defined(SIL_ARCH_ARM_64) && defined(__GNUC__)
    const uintptr_t alignment = (uintptr_t)ptr & 4;
    if (len - alignment >= 64/4) {
        if (UNLIKELY(alignment != 0)) {
            *(uint32_t *)ptr = val;
            ptr = (uint32_t *)ptr + 1;
            len--;
        }
        register uint64_t val64 __asm__("x0") = (uint64_t)val<<32 | val;
        __asm__ volatile(
            "and x2, %[len], #0xF          \n"
            "add x1, %[ptr], %[len], lsl #2\n"
            "cbz x2, 1f                    \n"
            "tbz %[len], #0, 0f            \n"
            "str w0, [%[ptr]], #4          \n"
            "sub x2, x2, #1                \n"
            "cbz x2, 1f                    \n"
         "0: str x0, [%[ptr]], #8          \n"
            "sub x2, x2, #2                \n"
            "cbnz x2, 0b                   \n"
         "1: stp x0, x0, [%[ptr]], #64     \n"
            "stp x0, x0, [%[ptr], #-48]    \n"
            "stp x0, x0, [%[ptr], #-32]    \n"
            "stp x0, x0, [%[ptr], #-16]    \n"
            "cmp %[ptr], x1                \n"
            "b.cc 1b                       \n"
            : [ptr] "=r" (ptr), "=m" (*(uint32_t *)ptr)
            : "0" (ptr), "r" (val64), [len] "r" (len)
            : "x1", "x2", "memory"
        );
        return;
    }
#endif

    for (uint32_t i = 0; i < len; i++) {
        ((uint32_t *)ptr)[i] = val;
    }
}

/*-----------------------------------------------------------------------*/

void mem_transpose32(void *dest, const void *src,
                     unsigned int src_width, unsigned int src_height,
                     int src_stride, int dest_stride)
{
    if (UNLIKELY(!dest) || UNLIKELY(!src)) {
        DLOG("Invalid parameters: %p %p %u %u %d %d",
             dest, src, src_width, src_height, src_stride, dest_stride);
        return;
    }

    unsigned int block_size_ = SIL_UTILITY_MEMORY_TRANSPOSE_BLOCK_SIZE;
    ASSERT(block_size_ > 0, block_size_ = 16);
    const unsigned int block_size = block_size_;

    const uint32_t *src32 = (const uint32_t *)src;
    uint32_t *dest32 = (uint32_t *)dest;
    const int src_block_stride = src_stride * block_size;
    const int dest_block_stride = dest_stride * block_size;

    unsigned int block_row;
    for (block_row = 0; block_row < src_height/block_size;
         block_row++, src32 += src_block_stride, dest32 += block_size)
    {
        const uint32_t *row_src = src32;
        uint32_t *row_dest = dest32;
        unsigned int block_column;
        for (block_column = 0; block_column < src_width/block_size;
             block_column++, row_src += block_size,
                 row_dest += dest_block_stride)
        {
            block_transpose32(row_dest, row_src, block_size, block_size,
                              src_stride, dest_stride);
        }
        block_column *= block_size;
        if (block_column < src_width) {
            block_transpose32(row_dest, row_src,
                              src_width - block_column, block_size,
                              src_stride, dest_stride);
        }
    }
    block_row *= block_size;
    if (block_row < src_height) {
        const uint32_t *row_src = src32;
        uint32_t *row_dest = dest32;
        unsigned int block_column;
        for (block_column = 0; block_column < src_width/block_size;
             block_column++, row_src += block_size,
                 row_dest += dest_block_stride)
        {
            block_transpose32(row_dest, row_src,
                              block_size, src_height - block_row,
                              src_stride, dest_stride);
        }
        block_column *= block_size;
        if (block_column < src_width) {
            block_transpose32(row_dest, row_src,
                              src_width - block_column, src_height - block_row,
                              src_stride, dest_stride);
        }
    }
}

/*************************************************************************/
/**************************** Local routines *****************************/
/*************************************************************************/

static inline void block_transpose32(uint32_t *dest, const uint32_t *src,
                                     unsigned int width, unsigned int height,
                                     int src_stride, int dest_stride)
{
    for (unsigned int r = 0; r < height; r++, src += src_stride, dest += 1) {
        uint32_t *dest_row = dest;
        for (unsigned int c = 0; c < width; c++, dest_row += dest_stride) {
            *dest_row = src[c];
        }
    }
}

/*************************************************************************/
/*************************************************************************/
