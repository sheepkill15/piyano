#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/Prng.h"
#include "synth/dsp/WaveTables.h"
#include "engine/AudioContext.h"

namespace synth::modules {

enum class LfoWave : uint8_t { Sine, Triangle, Square, SampleHold };

struct Lfo {
    float phase = 0.0f; // 0..1
    float rateHz = 1.0f;
    LfoWave wave = LfoWave::Sine;
    uint32_t shState = 0x13579BDFu;
    float shValue = 0.0f;

    void reset(const float ph = 0.0f) noexcept { phase = ph; }

    float tick() noexcept {
        const float prev = phase;
        phase += rateHz * engine::gAudio.invSampleRate;
        // Per-sample increment is small (rate << sampleRate), so a single
        // subtract is always enough to wrap back into [0, 1).
        if (phase >= 1.0f) phase -= 1.0f;

        const bool wrapped = phase < prev;
        switch (wave) {
            case LfoWave::Sine:
                return dsp::sineLU(phase);
            case LfoWave::Triangle: {
                const float s = 2.0f * phase - 1.0f;
                return 2.0f * (fabsf(s) - 0.5f);
            }
            case LfoWave::Square: {
                return (phase < 0.5f) ? 1.0f : -1.0f;
            }
            case LfoWave::SampleHold: {
                if (wrapped) shValue = dsp::prngLcg11(shState);
                return shValue;
            }
            default:
                return 0.0f;
        }
    }
};

} // namespace synth::modules
