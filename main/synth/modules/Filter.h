#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/Approx.h"

namespace synth::modules {

struct OnePoleLP {
    float z = 0.0f;
    float a = 0.0f;

    void reset(float v = 0.0f) noexcept { z = v; }

    void setCutoffHz(float cutoffHz, float sampleRate) noexcept {
        if (cutoffHz < 0.0f) cutoffHz = 0.0f;
        const float x = cutoffHz / sampleRate;
        const float clamped = (x > 0.49f) ? 0.49f : x;
        a = clamped;
    }

    inline float tick(float x) noexcept {
        z += (x - z) * a;
        return z;
    }
};

enum class SvfMode : uint8_t { LowPass, BandPass, HighPass };

// State-variable filter (Chamberlin style, simple/stable).
struct Svf {
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
    float g = 0.0f;
    float k = 0.0f;
    SvfMode mode = SvfMode::LowPass;

    void reset() noexcept { ic1eq = 0.0f; ic2eq = 0.0f; }

    void set(float cutoffHz, float resonance, float sampleRate) noexcept {
        if (cutoffHz < 5.0f) cutoffHz = 5.0f;
        if (cutoffHz > sampleRate * 0.45f) cutoffHz = sampleRate * 0.45f;
        if (resonance < 0.0f) resonance = 0.0f;
        if (resonance > 1.0f) resonance = 1.0f;

        const float w = static_cast<float>(M_PI) * (cutoffHz / sampleRate);
        g = synth::dsp::tanPrewarpFast(w);
        // Map 0..1 to k (damping): k=2/Q, Q ~ 0.5..12
        const float q = 0.5f + resonance * (12.0f - 0.5f);
        k = 2.0f / q;
    }

    inline float tick(float v0) noexcept {
        const float v1 = (g * (v0 - ic2eq) + ic1eq) / (1.0f + g * (g + k));
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

