#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/WaveTables.h"

namespace synth::modules {

enum class LfoWave : uint8_t { Sine, Triangle, Square, SampleHold };

struct Lfo {
    float sampleRate = 44100.0f;
    float phase = 0.0f; // 0..1
    float rateHz = 1.0f;
    LfoWave wave = LfoWave::Sine;
    uint32_t shState = 0x13579BDFu;
    float shValue = 0.0f;

    void setSampleRate(float sr) noexcept { sampleRate = sr; }
    void reset(float ph = 0.0f) noexcept { phase = ph; }

    inline float tick() noexcept {
        const float prev = phase;
        phase += rateHz / sampleRate;
        if (phase >= 1.0f) phase -= floorf(phase);

        const bool wrapped = phase < prev;
        switch (wave) {
            case LfoWave::Sine:
                return synth::dsp::sineLU(phase);
            case LfoWave::Triangle: {
                const float s = 2.0f * phase - 1.0f;
                return 2.0f * (fabsf(s) - 0.5f);
            }
            case LfoWave::Square: {
                return (phase < 0.5f) ? 1.0f : -1.0f;
            }
            case LfoWave::SampleHold: {
                if (wrapped) {
                    shState = shState * 1664525u + 1013904223u;
                    const uint32_t x = shState >> 9;
                    const float f = static_cast<float>(x) * (1.0f / 4194304.0f);
                    shValue = f * 2.0f - 1.0f;
                }
                return shValue;
            }
            default:
                return 0.0f;
        }
    }
};

} // namespace synth::modules
