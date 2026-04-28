#pragma once

#include <cstdint>
#include "synth/voice/VoiceAllocator.h"

struct VoiceContext {
    uint8_t note = 0;
    float velocity = 0.0f;   // 0..1
    float frequency = 0.0f;  // Hz
};

// Patch-style instrument interface:
// - The host (SynthEngine) owns polyphony/envelopes/mixing.
// - An instrument provides per-voice state hooks + per-voice signal generation.
class IInstrument {
public:
    virtual ~IInstrument() = default;

    virtual const char* name() const noexcept = 0;

    virtual void setSampleRate(float sr) noexcept = 0;

    // Default polyphony hint; host may override.
    virtual uint8_t defaultMaxVoices() const noexcept { return 8; }
    virtual synth::voice::SameNoteMode defaultSameNoteMode() const noexcept {
        return synth::voice::SameNoteMode::SingleVoicePerKey;
    }

    virtual void onVoiceStart(uint8_t voiceIndex, const VoiceContext& ctx) noexcept = 0;
    virtual void onVoiceStop(uint8_t voiceIndex) noexcept { (void)voiceIndex; }

    // Render and ADD this voice into the host buffers.
    // Buffers are mono for now; stereo panning happens in the host until per-voice stereo is added.
    virtual void renderAddVoice(uint8_t voiceIndex, float* outMono, uint64_t numSamples) noexcept = 0;

    virtual bool setParam(uint16_t paramId, float value) noexcept { (void)paramId; (void)value; return false; }
};