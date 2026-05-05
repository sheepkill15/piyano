#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/Prng.h"
#include "synth/dsp/WaveTables.h"
#include "engine/AudioContext.h"

namespace synth::modules {

enum class LfoWave : uint8_t { Sine, Triangle, Square, SampleHold };

template<LfoWave W>
float sampleLfoWave(const float phase01, const bool wrapped, uint32_t& shState, float& shValue) {
    if constexpr (LfoWave::Sine == W) {
        return dsp::sineLU(phase01);
    }
    else if constexpr (LfoWave::Triangle == W) {
        const float s = 2.0f * phase01 - 1.0f;
        return 2.0f * (fabsf(s) - 0.5f);
    }
    else if constexpr (LfoWave::Square == W) {
        return (phase01 < 0.5f) ? 1.0f : -1.0f;
    }
    else if constexpr (LfoWave::SampleHold == W) {
        if (wrapped) shValue = dsp::prngLcg11(shState);
        return shValue;
    }
    return 0.0f;
}

struct Lfo {
    float phase = 0.0f; // 0..1
    float rateHz = 1.0f;
    LfoWave wave = LfoWave::Sine;
    uint32_t shState = 0x13579BDFu;
    float shValue = 0.0f;
    float (*waveFn)(float phase01, bool wrapped, uint32_t& shState, float& shValue) = sampleLfoWave<LfoWave::Sine>;

    void reset(const float ph = 0.0f) noexcept { phase = ph; }

    void setWave(const LfoWave w) noexcept {
        wave = w;
        switch (wave) {
            case LfoWave::Sine:       waveFn = sampleLfoWave<LfoWave::Sine>; break;
            case LfoWave::Triangle:   waveFn = sampleLfoWave<LfoWave::Triangle>; break;
            case LfoWave::Square:     waveFn = sampleLfoWave<LfoWave::Square>; break;
            case LfoWave::SampleHold: waveFn = sampleLfoWave<LfoWave::SampleHold>; break;
        }
    }

    float tick() noexcept {
        const float prev = phase;
        phase += rateHz * engine::gAudio.invSampleRate;
        // Per-sample increment is small (rate << sampleRate), so a single
        // subtract is always enough to wrap back into [0, 1).
        if (phase >= 1.0f) phase -= 1.0f;

        const bool wrapped = phase < prev;
        return waveFn(phase, wrapped, shState, shValue);
    }
};

} // namespace synth::modules
