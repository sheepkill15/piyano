#pragma once

#include <cstddef>
#include <cstdint>

#include "IInstrument.h"
#include "Patch.h"

#include "synth/modules/Adsr.h"
#include "synth/modules/DrumVoices.h"
#include "synth/modules/Exciter.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Lfo.h"
#include "synth/modules/Noise.h"

// Generic instrument: interprets a synth::patch::Patch as a per-voice DSP graph.
// All "instruments" in the workstation are built up from this single class by
// passing in different Patch values (built by the Presets/* helpers).
class ModularInstrument : public IInstrument {
public:
    ModularInstrument() noexcept = default;
    ~ModularInstrument() noexcept override;

    // Replaces the live patch. Resets every voice. Re-allocates the resonator
    // delay buffer iff the new patch needs it.
    void setPatch(const synth::patch::Patch& patch) noexcept;

    const synth::patch::Patch& patch() const noexcept { return patch_; }

    const char* name() const noexcept override { return patch_.name; }
    void setSampleRate(float sr) noexcept override;
    uint8_t defaultMaxVoices() const noexcept override { return patch_.maxVoices; }
    synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override {
        return patch_.sameNoteMode;
    }

    void onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(uint8_t v) noexcept override;
    void renderAddVoice(uint8_t v, float* outMono, uint64_t numSamples) noexcept override;
    float voicePan(uint8_t v) const noexcept override;
    bool setParam(uint16_t paramId, float value) noexcept override;

private:
    static constexpr uint8_t MAX_VOICES = synth::patch::MAX_VOICES_CAP;
    static constexpr std::size_t kResonatorBufSamples = 4096;
    static_assert((kResonatorBufSamples & (kResonatorBufSamples - 1)) == 0,
                  "kResonatorBufSamples must be a power of two");
    static constexpr std::size_t kResonatorMask = kResonatorBufSamples - 1;

    struct VoiceState {
        bool active = false;
        float freq = 0.0f;
        float vel = 0.0f;
        uint8_t note = 0;
        float keyT = 0.0f;
        float pan = 0.0f;

        float oscPhase[synth::patch::MAX_OSCS] = {};
        float oscFb[synth::patch::MAX_OSCS] = {};
        float oscOut[synth::patch::MAX_OSCS] = {};
        int   oscSawTbl[synth::patch::MAX_OSCS] = {};

        // Mod envelopes 1..; index 0 is engine-driven amp env (unused here).
        synth::modules::Adsr modEnvs[synth::patch::MAX_ENVS];

        synth::modules::Lfo lfos[synth::patch::MAX_LFOS];
        synth::modules::Svf filters[synth::patch::MAX_FILTERS];

        synth::modules::WhiteNoise white{};
        synth::modules::PinkishNoise pink{};

        std::size_t resoIdx = 0;
        std::size_t resoDelay = 1;
        float resoFb = 0.998f;
        float resoDamp = 0.30f;
        float resoLp = 0.0f;
        synth::modules::Svf bodyHp{};
        synth::modules::OnePoleLP outLp{};

        synth::modules::ClickExciter click{};
        synth::modules::NoiseBurst burst{};

        synth::modules::Kick kick{};
        synth::modules::Snare snare{};
        synth::modules::Hat hat{};
        synth::patch::DrumPadKind drumKind = synth::patch::DrumPadKind::Hat;
    };

    void resetVoice_(uint8_t v) noexcept;
    void applySampleRateToVoice_(uint8_t v) noexcept;
    void ensureResonatorBuffer_() noexcept;
    void freeResonatorBuffer_() noexcept;
    float* voiceResoBuf_(uint8_t v) noexcept;

    void renderSynth_(VoiceState& s, float* out, uint64_t n) noexcept;
    void renderDrum_(VoiceState& s, float* out, uint64_t n) noexcept;

    synth::patch::Patch patch_{};
    VoiceState voices_[MAX_VOICES] = {};
    float sampleRate_ = 44100.0f;
    float* resoBuf_ = nullptr;
};
