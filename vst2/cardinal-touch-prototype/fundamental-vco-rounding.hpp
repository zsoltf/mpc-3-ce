#pragma once
#include <simd/functions.hpp>

// Local ARMv7 phase rounding; leave Rack's shared SIMD API unchanged.
namespace fundamental_vco {
using rack::simd::float_4;

#if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_ARM_NEON_A32V8_NATIVE)
static inline bool inRange(float32x4_t a) {
    // Subnormals, non-finite values and magnitudes >= 2^23 use Rack's path.
    const uint32x4_t bits = vandq_u32(vreinterpretq_u32_f32(a), vdupq_n_u32(0x7fffffffu));
    const uint32x4_t normalOrZero = vorrq_u32(vcgeq_u32(bits, vdupq_n_u32(0x00800000u)),
                                           vceqq_u32(bits, vdupq_n_u32(0)));
    const uint32x4_t valid = vandq_u32(normalOrZero, vcltq_u32(bits, vdupq_n_u32(0x4b000000u)));
    const uint32x2_t pair = vand_u32(vget_low_u32(valid), vget_high_u32(valid));
    return (vget_lane_u32(pair, 0) & vget_lane_u32(pair, 1)) == 0xffffffffu;
}

static inline float_4 result(float32x4_t value, float32x4_t original) {
    // Integer conversion loses the sign of zero; restore the original sign.
    simde__m128_private r;
    r.neon_f32 = vreinterpretq_f32_u32(vorrq_u32(vreinterpretq_u32_f32(value),
        vandq_u32(vreinterpretq_u32_f32(original), vdupq_n_u32(0x80000000u))));
    return float_4(simde__m128_from_private(r));
}
#endif

static inline float_4 floor(float_4 a) {
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_ARM_NEON_A32V8_NATIVE)
    const float32x4_t x = simde__m128_to_private(a.v).neon_f32;
    if (inRange(x)) {
        float32x4_t r = vcvtq_f32_s32(vcvtq_s32_f32(x));
        r = vsubq_f32(r, vreinterpretq_f32_u32(vandq_u32(vcgtq_f32(r, x),
            vreinterpretq_u32_f32(vdupq_n_f32(1.f)))));
        return result(r, x);
    }
#endif
    return rack::simd::floor(a);
}

static inline float_4 trunc(float_4 a) {
#if defined(SIMDE_ARM_NEON_A32V7_NATIVE) && !defined(SIMDE_ARM_NEON_A32V8_NATIVE)
    const float32x4_t x = simde__m128_to_private(a.v).neon_f32;
    if (inRange(x)) return result(vcvtq_f32_s32(vcvtq_s32_f32(x)), x);
#endif
    return rack::simd::trunc(a);
}
} // namespace fundamental_vco
