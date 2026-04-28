#pragma once

#include <cmath>

#include "synth/dsp/Approx.h"

namespace synth::modules {

struct Drive {
    float preGain = 1.0f;
    float postGain = 1.0f;

    inline float tick(float x) const noexcept {
        const float y = synth::dsp::tanhFast(x * preGain);
        return y * postGain;
    }
};

inline float softClip(float x, float limit = 0.95f) noexcept {
    return synth::dsp::tanhFast(x / limit) * limit;
}

} // namespace synth::modules
