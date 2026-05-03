#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

namespace synth::modules {

// Power-of-two ring; lenMask = buffer length - 1.
inline float tickKarplusRingPow2(float* buf, const std::size_t lenMask, std::size_t& idx, const std::size_t delay,
                                 float& lp, const float damp, const float feedback, const float input) noexcept {
    const std::size_t bufLen = lenMask + 1;
    const std::size_t read = (idx + bufLen - delay) & lenMask;
    const float y = buf[read];
    lp += (y - lp) * damp;
    buf[idx] = input + lp * feedback;
    idx = (idx + 1) & lenMask;
    return y;
}

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

    void setDelaySamples(const std::size_t d) noexcept {
        delay = dsp::clamp(d, 1u, MaxDelaySamples - 1);
    }

    float tick(const float x) noexcept {
        if constexpr ((MaxDelaySamples & (MaxDelaySamples - 1)) == 0 && MaxDelaySamples > 0) {
            constexpr std::size_t kMask = MaxDelaySamples - 1;
            return tickKarplusRingPow2(buf.data(), kMask, idx, delay, lp, damp, feedback, x);
        }
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

