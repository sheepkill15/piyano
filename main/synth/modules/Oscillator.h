#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/WaveTables.h"

namespace synth::modules {

enum class OscWave : uint8_t { Sine, Triangle, Saw, Pulse };

struct Oscillator {
    float sampleRate = 44100.0f;
    float phase = 0.0f;   // 0..1
    float freqHz = 440.0f;
    float pw = 0.5f;      // 0..1 (pulse width)
    OscWave wave = OscWave::Sine;

    void setSampleRate(float sr) noexcept { sampleRate = sr; }
    void reset(float ph = 0.0f) noexcept { phase = ph; }

    inline float tick() noexcept {
        const float dp = freqHz / sampleRate;
        phase += dp;
        if (phase >= 1.0f) phase -= 1.0f;
        else if (phase < 0.0f) phase += 1.0f;

        switch (wave) {
            case OscWave::Sine:
                return synth::dsp::sineLU(phase);
            case OscWave::Triangle: {
                const float s = 2.0f * phase - 1.0f;
                return 2.0f * (fabsf(s) - 0.5f);
            }
            case OscWave::Saw: {
                const int idx = synth::dsp::sawTableIdx(freqHz);
                return synth::dsp::sawLU(idx, phase);
            }
            case OscWave::Pulse: {
                const float p = (pw < 0.01f) ? 0.01f : (pw > 0.99f ? 0.99f : pw);
                return (phase < p) ? 1.0f : -1.0f;
            }
            default:
                return 0.0f;
        }
    }
};

} // namespace synth::modules
