#pragma once

#include <array>
#include <cstdint>

#include "Patch.h"
#include "engine/AudioContext.h"
#include "synth/Constants.h"
#include "synth/dsp/Approx.h"

#include "synth/modules/Oscillator.h"

namespace synth::modules {

struct FmOscBank {
    static constexpr uint8_t MAX_OSCS = synth::patch::MAX_OSCS;
    static constexpr uint8_t MAX_ENVS = synth::patch::MAX_ENVS;
    static constexpr uint8_t MAX_LFOS = synth::patch::MAX_LFOS;

    std::array<Oscillator, MAX_OSCS> oscs{};
    std::array<float, MAX_OSCS> oscFb{};
    std::array<float, MAX_OSCS> oscOut{};

    std::array<float, MAX_OSCS> oscFreq{};
    std::array<float, MAX_OSCS> oscDp{};

    void reset() noexcept {
        for (uint8_t i = 0; i < MAX_OSCS; ++i) {
            oscs[i].reset(0.0f);
            oscFb[i] = 0.0f;
            oscOut[i] = 0.0f;
            oscFreq[i] = 0.0f;
            oscDp[i] = 0.0f;
        }
    }

    void noteOn(const synth::patch::Patch& patch, const uint8_t voiceIndex, const float baseHz) noexcept {
        (void)voiceIndex;
        for (uint8_t i = 0; i < patch.oscCount && i < MAX_OSCS; ++i) {
            oscs[i].reset((i == 1) ? 0.5f : 0.0f);
            oscFb[i] = 0.0f;
            oscOut[i] = 0.0f;

            const auto& od = patch.oscs[i];
            oscs[i].setWave(toOscWave_(od.shape));
            oscs[i].pw = od.pulseWidth;
            oscs[i].configurePitch(od, baseHz, patch.fmRatioScale);
            oscFreq[i] = oscs[i].freqHz;
            oscDp[i] = oscFreq[i] * engine::gAudio.invSampleRate;
        }
    }

    void recomputePitch(const synth::patch::Patch& patch,
                        const float baseHz,
                        const std::array<float, MAX_LFOS>& lfoBuf) noexcept {
        for (uint8_t i = 0; i < patch.oscCount && i < MAX_OSCS; ++i) {
            const auto& od = patch.oscs[i];
            const float ratio = od.pitchRatio * (od.isFmModulator ? patch.fmRatioScale : 1.0f);
            float vibOctaves = 0.0f;
            if (od.vibratoLfoIndex >= 0 && static_cast<uint8_t>(od.vibratoLfoIndex) < patch.lfoCount) {
                vibOctaves =
                    lfoBuf[static_cast<uint8_t>(od.vibratoLfoIndex)] * od.vibratoSemitones * (1.0f / 12.0f);
            }
            const float octaves =
                (od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f) + vibOctaves;
            const float f = baseHz * ratio * dsp::exp2Fast(octaves);
            oscFreq[i] = f;
            oscs[i].setFreqHz(f);
            oscDp[i] = f * engine::gAudio.invSampleRate;
        }
    }

    float tickMix(const synth::patch::Patch& patch,
                  const std::array<float, MAX_ENVS>& envLevels,
                  const float vel) noexcept {
        constexpr float kRadiansToPhase01 = synth::cfg::kRadiansToPhase01;
        float mix = 0.0f;

        for (uint8_t oi = 0; oi < patch.oscCount && oi < MAX_OSCS; ++oi) {
            const auto& od = patch.oscs[oi];

            float phaseOffset01 = 0.0f;
            if (od.fmFeedback != 0.0f) {
                phaseOffset01 += oscFb[oi] * od.fmFeedback * kRadiansToPhase01;
            }
            if (od.fmFrom >= 0 && od.fmFrom < patch.oscCount) {
                float idx = od.fmIndex;
                if (patch.oscs[od.fmFrom].isFmModulator) idx *= patch.fmIndexScale;
                if (od.modEnvIndex >= 1 && od.modEnvIndex < MAX_ENVS) {
                    idx *= envLevels[static_cast<uint8_t>(od.modEnvIndex)];
                }
                if (od.velToFmIndex > 0.0f) {
                    const float velMix = od.velToFmIndex;
                    idx *= (1.0f - velMix) + velMix * (0.25f + 0.75f * vel);
                }
                phaseOffset01 += oscOut[static_cast<uint8_t>(od.fmFrom)] * idx * kRadiansToPhase01;
            }

            oscs[oi].pw = od.pulseWidth;
            const float y = oscs[oi].tickDp(oscDp[oi], phaseOffset01);
            oscOut[oi] = y;
            oscFb[oi] = y;

            const float lvl = od.level;
            if (lvl != 0.0f) mix += y * lvl;
        }
        return mix;
    }

private:
    static OscWave toOscWave_(const synth::patch::OscShape s) noexcept {
        using P = synth::patch::OscShape;
        using W = OscWave;
        switch (s) {
            case P::Sine:     return W::Sine;
            case P::Saw:      return W::Saw;
            case P::Triangle: return W::Triangle;
            case P::Pulse:    return W::Pulse;
        }
        return W::Sine;
    }
};

} // namespace synth::modules

