#pragma once

#include <cstdint>

namespace synth::modules {

// Fast LCG white noise in [-1, 1].
struct WhiteNoise {
    uint32_t state = 0x12345678u;

    void seed(uint32_t s) noexcept { state = (s == 0u) ? 0x12345678u : s; }

    inline float tick() noexcept {
        state = state * 1664525u + 1013904223u;
        const uint32_t x = state >> 9; // 23 bits
        const float f = static_cast<float>(x) * (1.0f / 4194304.0f); // 2^22
        return f * 2.0f - 1.0f;
    }
};

// Simple pink-ish: 1-pole lowpass on white.
struct PinkishNoise {
    WhiteNoise white;
    float z = 0.0f;
    float color = 0.05f; // 0..1

    inline float tick() noexcept {
        const float w = white.tick();
        const float c = (color < 0.0f) ? 0.0f : (color > 1.0f ? 1.0f : color);
        z += (w - z) * c;
        return z;
    }
};

} // namespace synth::modules

