#pragma once

#include <cstdint>
#include "synth/voice/VoiceAllocator.h"

struct VoiceContext {
    uint8_t note = 0;
    float velocity = 0.0f;   // 0..1
    float frequency = 0.0f;  // Hz
};

class IInstrument {
public:
    virtual ~IInstrument() = default;

    [[nodiscard]] virtual const char* name() const noexcept = 0;

    [[nodiscard]] virtual uint8_t defaultMaxVoices() const noexcept { return 8; }
    [[nodiscard]] virtual synth::voice::SameNoteMode defaultSameNoteMode() const noexcept {
        return synth::voice::SameNoteMode::SingleVoicePerKey;
    }

    virtual void onVoiceStart(uint8_t voiceIndex, const VoiceContext& ctx) noexcept = 0;
    virtual void onVoiceStop(const uint8_t voiceIndex) noexcept { (void)voiceIndex; }

    // Render and ADD this voice into a mono buffer. Engine handles pan/sum into stereo bus.
    virtual void renderAddVoice(uint8_t voiceIndex, float* outMono, uint64_t numSamples) noexcept = 0;

    // -1..+1 stereo pan position for the given voice. Default centered.
    [[nodiscard]] virtual float voicePan(const uint8_t voiceIndex) const noexcept { (void)voiceIndex; return 0.0f; }

    virtual bool setParam(const uint16_t paramId, const float value) noexcept { (void)paramId; (void)value; return false; }
};
