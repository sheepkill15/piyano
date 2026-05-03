#pragma once

#include <cstdint>

namespace synth::dsp {

// LCG step; output in [-1, 1] (same recipe as legacy WhiteNoise / LFO sample-hold).
inline float prngLcg11(uint32_t& state) noexcept {
    state = state * 1664525u + 1013904223u;
    const uint32_t x = state >> 9;
    const float f = static_cast<float>(x) * (1.0f / 4194304.0f);
    return f * 2.0f - 1.0f;
}

} // namespace synth::dsp
