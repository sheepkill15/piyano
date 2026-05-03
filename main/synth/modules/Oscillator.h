#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/Util.h"
#include "synth/dsp/WaveTables.h"
#include "engine/AudioContext.h"

namespace synth::modules {

enum class OscWave : uint8_t { Sine, Triangle, Saw, Pulse };

inline float sampleOscWave(const OscWave w, const float phase01, const int sawTblIdx, const float pw) noexcept {
    switch (w) {
        case OscWave::Sine:
            return dsp::sineLU(phase01);
        case OscWave::Triangle: {
            const float s = 2.0f * phase01 - 1.0f;
            return 2.0f * (fabsf(s) - 0.5f);
        }
        case OscWave::Saw:
            return dsp::sawLU(sawTblIdx, phase01);
        case OscWave::Pulse: {
            const float p = dsp::clamp(pw, 0.01f, 0.99f);
            return (phase01 < p) ? 1.0f : -1.0f;
        }
    }
    return 0.0f;
}

struct Oscillator {
    float phase = 0.0f;   // 0..1
    float freqHz = 440.0f;
    float pw = 0.5f;      // 0..1 (pulse width)
    OscWave wave = OscWave::Sine;

    void reset(const float ph = 0.0f) noexcept { phase = ph; }

    float tick() noexcept {
        const float dp = freqHz * engine::gAudio.invSampleRate;
        phase += dp;
        if (phase >= 1.0f) phase -= 1.0f;
        else if (phase < 0.0f) phase += 1.0f;

        return sampleOscWave(wave, phase, dsp::sawTableIdx(freqHz), pw);
    }
};

} // namespace synth::modules
