#pragma once

#include <cstdint>
#include <cmath>

#include "synth/Constants.h"
#include "synth/dsp/Approx.h"
#include "synth/modules/Smoother.h"

namespace synth::mix {

struct GlobalMixer {
    // Soft-knee limiter: fully transparent below kKnee, smooth tanh saturation
    // between kKnee and kCeil, asymptote at kCeil. Avoids the audible distortion
    // a global tanh produces in the "normal" signal range.
    static constexpr float kKnee = cfg::kMixerLimiterKnee;
    static constexpr float kCeil = cfg::kMixerLimiterCeil;
    static constexpr float kRange = kCeil - kKnee;

    modules::Smoother1p gainSmoother{};

    void init(const uint32_t sampleRate) noexcept {
        gainSmoother.setTimeConstant(0.020f, static_cast<float>(sampleRate));
        gainSmoother.reset(0.5f);
        gainSmoother.setTarget(0.5f);
    }

    void setMasterGain(const float g) noexcept {
        gainSmoother.setTarget(dsp::clamp(g, 0.0f, 4.0f));
    }

    static float softClip(const float x) noexcept {
        if (x >  kKnee) return  kKnee + kRange * dsp::tanhFast((x - kKnee) / kRange);
        if (x < -kKnee) return -kKnee + kRange * dsp::tanhFast((x + kKnee) / kRange);
        return x;
    }

    void processStereo(float* interleavedLR, const uint64_t nFrames) noexcept {
        for (uint64_t i = 0; i < nFrames; ++i) {
            const float g = gainSmoother.tick();
            interleavedLR[2 * i]     = softClip(interleavedLR[2 * i]     * g);
            interleavedLR[2 * i + 1] = softClip(interleavedLR[2 * i + 1] * g);
        }
    }
};

} // namespace synth::mix
