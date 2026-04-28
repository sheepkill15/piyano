#pragma once

#include <cstdint>
#include <cmath>

namespace synth::mix {

struct GlobalMixer {
    float masterGain = 0.25f; // linear
    float clipLimit = 0.95f;

    void setMasterGain(float g) noexcept { masterGain = g; }

    inline float softClip(float x) const noexcept {
        return tanhf(x / clipLimit) * clipLimit;
    }

    void processMono(float* buffer, uint64_t n) noexcept {
        for (uint64_t i = 0; i < n; ++i) {
            float x = buffer[i] * masterGain;
            buffer[i] = softClip(x);
        }
    }
};

} // namespace synth::mix

