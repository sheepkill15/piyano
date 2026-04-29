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

    virtual const char* name() const noexcept = 0;

    virtual uint8_t defaultMaxVoices() const noexcept { return 8; }
    virtual synth::voice::SameNoteMode defaultSameNoteMode() const noexcept {
        return synth::voice::SameNoteMode::SingleVoicePerKey;
    }

    virtual void onVoiceStart(uint8_t voiceIndex, const VoiceContext& ctx) noexcept = 0;
    virtual void onVoiceStop(uint8_t voiceIndex) noexcept { (void)voiceIndex; }

    // Render and ADD this voice into a mono buffer. Engine handles pan/sum into stereo bus.
    virtual void renderAddVoice(uint8_t voiceIndex, float* outMono, uint64_t numSamples) noexcept = 0;

    // -1..+1 stereo pan position for the given voice. Default centered.
    virtual float voicePan(uint8_t voiceIndex) const noexcept { (void)voiceIndex; return 0.0f; }

    virtual bool setParam(uint16_t paramId, float value) noexcept { (void)paramId; (void)value; return false; }
};
