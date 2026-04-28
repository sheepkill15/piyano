#pragma once

#include "instruments/IInstrument.h"
#include "instruments/PatchBuilder.h"

#include "synth/modules/Exciter.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Shaper.h"
#include "synth/modules/DrumVoices.h"
#include <cstddef>

class PatchInstrument : public IInstrument {
public:
    void init(const PatchConfig& cfg) noexcept;

    const char* name() const noexcept override;
    void setSampleRate(float sr) noexcept override;
    uint8_t defaultMaxVoices() const noexcept override { return cfg_.maxVoices; }
    synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override { return cfg_.sameNoteMode; }

    void onVoiceStart(uint8_t voiceIndex, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(uint8_t voiceIndex) noexcept override;
    void renderAddVoice(uint8_t voiceIndex, float* outMono, uint64_t numSamples) noexcept override;
    bool setParam(uint16_t paramId, float value) noexcept override;

private:
    static constexpr uint8_t MAX_VOICES = 8;
    static constexpr std::size_t PIANO_BUF_SAMPLES = 2048;
    float sampleRate_ = 44100.0f;
    PatchConfig cfg_ = {};

    // Shared simple oscillator-style state
    float freq_[MAX_VOICES] = {};
    float vel_[MAX_VOICES] = {};
    float ph1_[MAX_VOICES] = {};
    float ph2_[MAX_VOICES] = {};

    // Piano-ish
    synth::modules::ClickExciter click_[MAX_VOICES] = {};
    synth::modules::NoiseBurst noise_[MAX_VOICES] = {};
    float* pianoBuf_ = nullptr; // heap, size = MAX_VOICES * PIANO_BUF_SAMPLES
    struct PianoBodyState {
        std::size_t idx = 0;
        std::size_t delay = 1;
        float feedback = 0.90f;
        float damp = 0.10f;
        float lp = 0.0f;
    };
    PianoBodyState pianoBody_[MAX_VOICES] = {};

    // Strings/Bass
    synth::modules::Svf filter_[MAX_VOICES] = {};
    synth::modules::Drive drive_ = {};

    // Drums
    enum class DrumType : uint8_t { None, Kick, Snare, Hat };
    DrumType drumType_[MAX_VOICES] = {};
    synth::modules::Kick kick_[MAX_VOICES] = {};
    synth::modules::Snare snare_[MAX_VOICES] = {};
    synth::modules::Hat hat_[MAX_VOICES] = {};

    inline float* pianoVoiceBuf_(uint8_t v) noexcept {
        return pianoBuf_ ? (pianoBuf_ + static_cast<std::size_t>(v) * PIANO_BUF_SAMPLES) : nullptr;
    }
};

