#pragma once

#include <cmath>

#include "synth/dsp/Approx.h"

namespace synth::modules {

struct Drive {
    float preGain = 1.0f;
    float postGain = 1.0f;

    [[nodiscard]] float tick(const float x) const noexcept {
        const float y = dsp::tanhFast(x * preGain);
        return y * postGain;
    }
};

inline float softClip(const float x, const float limit = 0.95f) noexcept {
    return dsp::softClipFast(x, limit);
}

} // namespace synth::modules
