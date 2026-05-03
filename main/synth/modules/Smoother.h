#pragma once

namespace synth::modules {

// One-pole parameter smoother.
// Call setTimeConstant() after sample rate is known.
struct Smoother1p {
    float a = 0.0f;      // pole coefficient
    float y = 0.0f;      // state
    float target = 0.0f; // desired value

    void setTimeConstant(const float tauSeconds, const float sampleRate) noexcept {
        if (tauSeconds <= 0.0f) { a = 0.0f; return; }
        const float x = 1.0f / (tauSeconds * sampleRate);
        a = (x > 1.0f) ? 1.0f : x;
    }

    void reset(const float v = 0.0f) noexcept { y = v; target = v; }
    void setTarget(const float v) noexcept { target = v; }

    float tick() noexcept {
        y += (target - y) * a;
        return y;
    }
};

} // namespace synth::modules

