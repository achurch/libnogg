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

#ifdef ENABLE_ASM_ARM_NEON
# include <arm_neon.h>
#endif


void float_to_int16(int16_t *__restrict dest, const float *__restrict src, int count)
{
#if defined(ENABLE_ASM_X86_SSE2) && defined(__GNUC__)
    if (count >= 8) {
        const int loops = count/8;
        count %= 8;

        static const ALIGN(16) struct {uint32_t data[12];} sse2_data = {{
            0x46FFFE00, 0x46FFFE00, 0x46FFFE00, 0x46FFFE00,  // 32767.0
            0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF,
            0x80000000, 0x80000000, 0x80000000, 0x80000000,
        }};

        uint32_t mxcsr;
        __asm__("stmxcsr %0" : "=m" (mxcsr));
        const uint32_t saved_mxcsr = mxcsr;
        mxcsr &= ~(3<<13);  // RC (00 = round to nearest)
        mxcsr |= 1<<7;      // EM_INVALID
        __asm__("ldmxcsr %0" : /* no outputs */ : "m" (mxcsr));

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
              [sse2_data] "r" (&sse2_data), "m" (sse2_data)
            : "xmm0", "xmm1", "xmm2", "xmm3", "xmm5", "xmm6", "xmm7"
        );

        __asm__("ldmxcsr %0" : /* no outputs */ : "m" (saved_mxcsr));
    }
#endif  // ENABLE_ASM_X86_SSE2 && __GNUC__

#if defined(ENABLE_ASM_ARM_NEON) && defined(__GNUC__)
    register const float32x4_t k32767 = {32767, 32767, 32767, 32767};
    register const float32x4_t k0_5 = {0.5, 0.5, 0.5, 0.5};
    register const uint32x4_t k7FFFFFFF = {0x7FFFFFFF, 0x7FFFFFFF,
                                           0x7FFFFFFF, 0x7FFFFFFF};
    for (; count >= 8; src += 8, dest += 8, count -= 8) {
        register float32x4_t in0 = vld1q_f32(src);
        register float32x4_t in1 = vld1q_f32(src + 4);
        register float32x4_t in0_scaled = vmulq_f32(in0, k32767);
        register float32x4_t in1_scaled = vmulq_f32(in1, k32767);
        register uint32x4_t in0_abs = vandq_u32((uint32x4_t)in0_scaled,
                                                k7FFFFFFF);
        register uint32x4_t in1_abs = vandq_u32((uint32x4_t)in1_scaled,
                                                k7FFFFFFF);
        register uint32x4_t in0_sign = vbicq_u32((uint32x4_t)in0_scaled,
                                                 k7FFFFFFF);
        register uint32x4_t in1_sign = vbicq_u32((uint32x4_t)in1_scaled,
                                                 k7FFFFFFF);
        register uint32x4_t in0_sat = vminq_u32(in0_abs, (uint32x4_t)k32767);
        register uint32x4_t in1_sat = vminq_u32(in1_abs, (uint32x4_t)k32767);
        /* Note that we have to add 0.5 to the absolute values here because
         * vcvt always rounds toward zero. */
        register float32x4_t in0_adj = vaddq_f32((float32x4_t)in0_sat, k0_5);
        register float32x4_t in1_adj = vaddq_f32((float32x4_t)in1_sat, k0_5);
        register float32x4_t out0 = (float32x4_t)vorrq_u32((uint32x4_t)in0_adj,
                                                           in0_sign);
        register float32x4_t out1 = (float32x4_t)vorrq_u32((uint32x4_t)in1_adj,
                                                           in1_sign);
        register int32x4_t out0_32 = vcvtq_s32_f32(out0);
        register int32x4_t out1_32 = vcvtq_s32_f32(out1);
        /* GCC doesn't seem to be smart enough to put out0_16 and out1_16
         * in paired registers (and it ignores any explicit registers we
         * specify with asm("REG")), so do it manually. */
        //register int16x4_t out0_16 = vmovn_s32(out0_32);
        //register int16x4_t out1_16 = vmovn_s32(out1_32);
        //register int16x8_t out_16 = vcombine_s16(out0_16, out1_16);
        register int16x8_t out_16;
        __asm__(
            "vmovn.i32 %e0, %q1\n"
            "vmovn.i32 %f0, %q2\n"
            : "=&w" (out_16)
            : "w" (out0_32), "w" (out1_32)
        );
        vst1q_s16(dest, out_16);
    }
#endif  // ENABLE_ASM_X86_NEON && __GNUC__

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
