#pragma once

#include "Patch.h"
#include "synth/modules/Noise.h"

namespace synth::modules {

struct NoiseGen {
    WhiteNoise white{};
    PinkishNoise pink{};

    float tick(const synth::patch::NoiseDef& nd) noexcept {
        if (nd.shape == synth::patch::NoiseShape::White) {
            return white.tick() * nd.level;
        }
        if (nd.shape == synth::patch::NoiseShape::Pinkish) {
            pink.color = nd.color;
            return pink.tick() * nd.level;
        }
        return 0.0f;
    }
};

} // namespace synth::modules

