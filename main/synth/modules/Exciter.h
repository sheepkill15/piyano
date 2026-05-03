#pragma once

#include "synth/modules/Noise.h"

namespace synth::modules {

struct ClickExciter {
    float level = 0.0f;
    float decay = 0.0f; // per-sample decay multiplier (e.g. 0.999)

    void trigger(const float l, const float d) noexcept { level = l; decay = d; }

    float tick() noexcept {
        const float y = level;
        level *= decay;
        return y;
    }
};

struct NoiseBurst {
    WhiteNoise noise;
    float level = 0.0f;
    float decay = 0.0f;

    void trigger(const float l, const float d) noexcept { level = l; decay = d; }

    float tick() noexcept {
        const float y = noise.tick() * level;
        level *= decay;
        return y;
    }
};

} // namespace synth::modules

