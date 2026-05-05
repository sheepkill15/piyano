#pragma once

#include <cstdint>
#include <cmath>

#include "synth/dsp/Util.h"
#include "synth/dsp/WaveTables.h"
#include "engine/AudioContext.h"

namespace synth::modules {

enum class OscWave : uint8_t { Sine, Triangle, Saw, Pulse };

template<OscWave W>
float sampleOscWave(const float phase01, const int sawTblIdx, const float pw) noexcept {
    if constexpr (OscWave::Sine == W) {
        return dsp::sineLU(phase01);
    }
    else if constexpr (OscWave::Triangle == W) {
        const float s = 2.0f * phase01 - 1.0f;
        return 2.0f * (fabsf(s) - 0.5f);
    }
    else if constexpr (OscWave::Saw == W) {
        return dsp::sawLU(sawTblIdx, phase01);
    }
    else if constexpr (OscWave::Pulse == W) {
        const float p = dsp::clamp(pw, 0.01f, 0.99f);
        return (phase01 < p) ? 1.0f : -1.0f;
    }
    return 0.0f;
}

struct Oscillator {
    float phase = 0.0f;   // 0..1
    float freqHz = 440.0f;
    float pw = 0.5f;      // 0..1 (pulse width)
    int sawTblIdx = 0;
    OscWave wave = OscWave::Sine;
    decltype(sampleOscWave<OscWave::Sine>)* oscWaveFn = sampleOscWave<OscWave::Sine>;

    void setWave(const OscWave w) noexcept {
        wave = w;
        switch (wave) {
            case OscWave::Sine:     oscWaveFn = sampleOscWave<OscWave::Sine>; break;
            case OscWave::Triangle: oscWaveFn = sampleOscWave<OscWave::Triangle>; break;
            case OscWave::Saw:      oscWaveFn = sampleOscWave<OscWave::Saw>; break;
            case OscWave::Pulse:    oscWaveFn = sampleOscWave<OscWave::Pulse>; break;
        }
    }

    void reset(const float ph = 0.0f) noexcept { phase = ph; }

    void setFreqHz(const float fHz) noexcept {
        freqHz = fHz;
        const float f = (fHz > 1.0f) ? fHz : 1.0f;
        sawTblIdx = dsp::sawTableIdx(f);
    }

    void configurePitch(const synth::patch::OscDef& od, const float baseHz, const float fmRatioScale) noexcept {
        const float ratio = od.pitchRatio * (od.isFmModulator ? fmRatioScale : 1.0f);
        const float octaves = (od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f);
        setFreqHz(baseHz * ratio * dsp::exp2Fast(octaves));
    }

    float tick() noexcept { return tickDp(freqHz * engine::gAudio.invSampleRate, 0.0f); }

    float tickDp(const float dp, const float phaseOffset01) noexcept {
        phase += dp;
        if (phase >= 1.0f) phase -= 1.0f;
        else if (phase < 0.0f) phase += 1.0f;

        float ph = phase + phaseOffset01;
        ph -= static_cast<float>(static_cast<int>(ph));
        if (ph < 0.0f) ph += 1.0f;

        return oscWaveFn(ph, sawTblIdx, pw);
    }
};

} // namespace synth::modules
