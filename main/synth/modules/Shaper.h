#pragma once

#include <cmath>

namespace synth::modules {

struct Drive {
    float preGain = 1.0f;
    float postGain = 1.0f;

    inline float tick(float x) const noexcept {
        const float y = tanhf(x * preGain);
        return y * postGain;
    }
};

inline float softClip(float x, float limit = 0.95f) noexcept {
    return tanhf(x / limit) * limit;
}

} // namespace synth::modules

