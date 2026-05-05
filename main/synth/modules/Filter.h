#pragma once

#include <cstdint>
#include <cmath>
#include "AudioContext.h"
#include "synth/dsp/Approx.h"
#include "synth/dsp/Util.h"

namespace {
    constexpr auto PI = static_cast<float>(M_PI);
}

namespace synth::modules {

struct OnePoleLP {
    float z = 0.0f;
    float a = 0.0f;

    void reset(const float v = 0.0f) noexcept { z = v; }

    void setCutoffHz(const float cutoffHz, const float sampleRate) noexcept {
        if (cutoffHz < 0.0f) {
            a = 0.0f;
            return;
        }
        const float x = cutoffHz / sampleRate;
        const float clamped = (x > 0.49f) ? 0.49f : x;
        a = clamped;
    }

    float tick(const float x) noexcept {
        z += (x - z) * a;
        return z;
    }
};

enum class SvfMode : uint8_t { LowPass, BandPass, HighPass };

// State-variable filter (Chamberlin style, simple/stable).
// `a1` caches the reciprocal of the v1 denominator so tick() does no division.
struct Svf {
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
    float g = 0.0f;
    float k = 0.0f;
    float a1 = 1.0f;
    SvfMode mode = SvfMode::LowPass;

    void reset() noexcept { ic1eq = 0.0f; ic2eq = 0.0f; }

    void set(float cutoffHz, float resonance) noexcept {
        const auto sampleRate = engine::gAudio.sampleRate;
        cutoffHz = dsp::clamp(cutoffHz, 5.0f, sampleRate * 0.45f);
        resonance = dsp::clamp(resonance, 0.0f, 1.0f);

        const float w = PI * (cutoffHz / sampleRate);
        g = dsp::tanPrewarpFast(w);
        // Map 0..1 to k (damping): k=2/Q, Q ~ 0.5..12
        const float q = 0.5f + resonance * (12.0f - 0.5f);
        k = 2.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    float tick(const float v0) noexcept {
        const float v1 = (g * (v0 - ic2eq) + ic1eq) * a1;
        const float v2 = ic2eq + g * v1;
        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        switch (mode) {
            case SvfMode::LowPass: return v2;
            case SvfMode::BandPass: return v1;
            case SvfMode::HighPass: return v0 - k * v1 - v2;
            default: return v2;
        }
    }
};

} // namespace synth::modules

