#pragma once

#include <cmath>
#include <cstdint>

namespace synth::dsp {

// Pade-style fast tanh. Max error ~3e-3 in [-3, 3], saturates outside.
// 4 mul + 2 add + 1 div + 2 cmp; much cheaper than libm tanhf on Xtensa.
inline float tanhFast(float x) noexcept {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float softClipFast(float x, float limit = 0.95f) noexcept {
    return tanhFast(x / limit) * limit;
}

// Schraudolph-style approximation of 2^x; max relative error ~3% but plenty
// good for cents-of-pitch modulation. Defined for x in roughly [-126, 126].
inline float exp2Fast(float x) noexcept {
    union { float f; int32_t i; } u;
    const float v = (1 << 23) * (x + 126.94269504f);
    if (v < 0.0f)              u.i = 0;
    else if (v > 2139095040.f) u.i = 2139095040;
    else                       u.i = static_cast<int32_t>(v);
    return u.f;
}

// SVF/Chamberlin prewarp g = tan(pi*fc/fs). Padé (3,3) form; fc clamped to
// ~0.45*sr keeps w < 1.42 so denominator stays well behaved vs libm tanf.
inline float tanPrewarpFast(float w) noexcept {
    const float w2 = w * w;
    return (w * (15.0f - w2)) / (15.0f - 6.0f * w2);
}

} // namespace synth::dsp
