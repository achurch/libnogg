/*
 * libnogg: a decoder library for Ogg Vorbis streams
 * Copyright (c) 2014 Andrew Church <achurch@achurch.org>
 *
 * This software may be copied and redistributed under certain conditions;
 * see the file "COPYING" in the source code distribution for details.
 * NO WARRANTY is provided with this software.
 */

#include "include/nogg.h"
#include "include/internal.h"
#include "src/util/float-to-int16.h"

#include <math.h>


void float_to_int16(int16_t *__restrict dest, const float *__restrict src, int count)
{
#if defined(__GNUC__) && defined(__amd64__)
    if (count >= 8) {
        const int loops = count/8;
        count %= 8;

        static const ALIGN(16) uint32_t sse2_data[] = {
            0x46FFFE00, 0x46FFFE00, 0x46FFFE00, 0x46FFFE00,  // 32767.0
            0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
            0x80000000, 0x80000000, 0x80000000, 0x80000000,
        };

        uint32_t mxcsr;
        __asm__("stmxcsr %0" : "=m" (mxcsr));
        const uint32_t saved_mxcsr = mxcsr;
        mxcsr &= ~(3<<13);  // RC (00 = round to nearest)
        mxcsr |= 1<<7;      // EM_INVALID
        __asm__("ldmxcsr %0" : /* no outputs */ : "m" (mxcsr));

        if ((((uintptr_t)src | (uintptr_t)dest) & 15) == 0) {
            __asm__ volatile(
                /* Load constants. */
                "movdqa (%[sse2_data]), %%xmm5\n"
                "movdqa 16(%[sse2_data]), %%xmm6\n"
                "movdqa 32(%[sse2_data]), %%xmm7\n"
                "0:\n"
                /* Load 8 samples from the source buffer into XMM0. */
                "movdqa (%[src]), %%xmm0\n"
                "movdqa 16(%[src]), %%xmm1\n"
                "add $32, %[src]\n"
                /* Scale each sample value by 32767 and apply bounds. */
                "mulps %%xmm5, %%xmm0\n"
                "mulps %%xmm5, %%xmm1\n"
                "movdqa %%xmm0, %%xmm2\n"
                "movdqa %%xmm1, %%xmm3\n"
                "pand %%xmm6, %%xmm0\n"  // Holds the absolute values.
                "pand %%xmm6, %%xmm1\n"
                "pand %%xmm7, %%xmm2\n"  // Holds the sign bits.
                "pand %%xmm7, %%xmm3\n"
                "minps %%xmm5, %%xmm0\n"
                "minps %%xmm5, %%xmm1\n"
                "por %%xmm2, %%xmm0\n"
                "por %%xmm3, %%xmm1\n"
                /* Convert floating-point values to integers and store. */
                "cvtps2dq %%xmm0, %%xmm2\n"
                "cvtps2dq %%xmm1, %%xmm3\n"
                "packssdw %%xmm3, %%xmm2\n"
                "movdqa %%xmm2, (%[dest])\n"
                "add $16, %[dest]\n"
                /* Loop until we run out of data. */
                "cmp %[src], %[src_limit]\n"
                "ja 0b\n"
                : [dest] "=r" (dest), [src] "=r" (src)
                : "0" (dest), "1" (src), [src_limit] "r" (src + loops*8),
                  [sse2_data] "r" (sse2_data)
            );
        } else {
            /* Exactly the same code as above, except that it uses movdqu
             * instead of movdqa to load/store sample data. */
            __asm__ volatile(
                /* Load constants. */
                "movdqa (%[sse2_data]), %%xmm5\n"
                "movdqa 16(%[sse2_data]), %%xmm6\n"
                "movdqa 32(%[sse2_data]), %%xmm7\n"
                "0:\n"
                /* Load 8 samples from the source buffer into XMM0. */
                "movdqu (%[src]), %%xmm0\n"
                "movdqu 16(%[src]), %%xmm1\n"
                "add $32, %[src]\n"
                /* Scale each sample value by 32767 and apply bounds. */
                "mulps %%xmm5, %%xmm0\n"
                "mulps %%xmm5, %%xmm1\n"
                "movdqa %%xmm0, %%xmm2\n"
                "movdqa %%xmm1, %%xmm3\n"
                "pand %%xmm6, %%xmm0\n"  // Holds the absolute values.
                "pand %%xmm6, %%xmm1\n"
                "pand %%xmm7, %%xmm2\n"  // Holds the sign bits.
                "pand %%xmm7, %%xmm3\n"
                "minps %%xmm5, %%xmm0\n"
                "minps %%xmm5, %%xmm1\n"
                "por %%xmm2, %%xmm0\n"
                "por %%xmm3, %%xmm1\n"
                /* Convert floating-point values to integers and store. */
                "cvtps2dq %%xmm0, %%xmm2\n"
                "cvtps2dq %%xmm1, %%xmm3\n"
                "packssdw %%xmm3, %%xmm2\n"
                "movdqu %%xmm2, (%[dest])\n"
                "add $16, %[dest]\n"
                /* Loop until we run out of data. */
                "cmp %[src], %[src_limit]\n"
                "ja 0b\n"
                : [dest] "=r" (dest), [src] "=r" (src)
                : "0" (dest), "1" (src), [src_limit] "r" (src + loops*8),
                  [sse2_data] "r" (sse2_data)
            );
        }
        __asm__("ldmxcsr %0" : /* no outputs */ : "m" (saved_mxcsr));
    }
#endif  // __GNUC__ && __amd64__

    for (int i = 0; i < count; i++) {
        const float sample = src[i];
        if (UNLIKELY(sample < -1.0f)) {
            dest[i] = -32767;
        } else if (LIKELY(sample <= 1.0f)) {
            dest[i] = (int16_t)roundf(sample * 32767.0f);
        } else {
            dest[i] = 32767;
        }
    }
}
