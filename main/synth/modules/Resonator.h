#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace synth::modules {

// Small fixed-size delay-line resonator for plucks/body.
// MaxDelaySamples must be >= sampleRate / minFreq.
template <std::size_t MaxDelaySamples>
struct Resonator {
    std::array<float, MaxDelaySamples> buf = {};
    std::size_t idx = 0;
    std::size_t delay = 1;
    float feedback = 0.90f; // 0.. <1
    float damp = 0.10f;     // 0..1 (lowpass in feedback)
    float lp = 0.0f;

    void reset() noexcept {
        for (auto& s : buf) s = 0.0f;
        idx = 0;
        lp = 0.0f;
    }

    void setDelaySamples(std::size_t d) noexcept {
        if (d < 1) d = 1;
        if (d >= MaxDelaySamples) d = MaxDelaySamples - 1;
        delay = d;
    }

    inline float tick(float x) noexcept {
        const std::size_t read = (idx + MaxDelaySamples - delay) % MaxDelaySamples;
        const float y = buf[read];
        lp += (y - lp) * damp;
        const float fb = lp * feedback;
        buf[idx] = x + fb;
        idx = (idx + 1) % MaxDelaySamples;
        return y;
    }
};

} // namespace synth::modules

