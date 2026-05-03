#pragma once

#include <cstdint>

#include "synth/dsp/Prng.h"

namespace synth::modules {

// Fast LCG white noise in [-1, 1].
struct WhiteNoise {
    uint32_t state = 0x12345678u;

    void seed(const uint32_t s) noexcept { state = (s == 0u) ? 0x12345678u : s; }

    float tick() noexcept { return dsp::prngLcg11(state); }
};

// Simple pink-ish: 1-pole lowpass on white.
struct PinkishNoise {
    WhiteNoise white;
    float z = 0.0f;
    float color = 0.05f; // 0..1

    float tick() noexcept {
        const float w = white.tick();
        const float c = (color < 0.0f) ? 0.0f : (color > 1.0f ? 1.0f : color);
        z += (w - z) * c;
        return z;
    }
};

} // namespace synth::modules

