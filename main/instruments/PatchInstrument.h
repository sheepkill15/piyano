#pragma once

#include "IInstrument.h"
#include "PatchConfig.h"

class PatchInstrument : public IInstrument {
public:
    ~PatchInstrument() noexcept override;
    void init(const PatchConfig& cfg) noexcept;

    const char* name() const noexcept override;
    void setSampleRate(float sr) noexcept override;
    uint8_t defaultMaxVoices() const noexcept override { return cfg_.maxVoices; }
    synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override { return cfg_.sameNoteMode; }

    void onVoiceStart(uint8_t voiceIndex, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(uint8_t voiceIndex) noexcept override;
    void renderAddVoice(uint8_t voiceIndex, float* outMono, uint64_t numSamples) noexcept override;
    float voicePan(uint8_t voiceIndex) const noexcept override;
    bool setParam(uint16_t paramId, float value) noexcept override;

private:
    void release_() noexcept;
    unsigned char* voiceSlot_(uint8_t v) noexcept;

    PatchConfig cfg_{};
    const PatchDriver* driver_{nullptr};
    void* inst_{nullptr};
    unsigned char* voices_{nullptr};
    float sampleRate_{44100.0f};
};
