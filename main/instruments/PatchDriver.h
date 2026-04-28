#pragma once

#include <cstddef>
#include <cstdint>

#include "IInstrument.h"

struct PatchConfig;

class PatchDriver {
public:
    virtual ~PatchDriver() = default;

    virtual const char* name() const noexcept = 0;
    virtual std::size_t voiceStateBytes() const noexcept = 0;
    virtual std::size_t instrumentStateBytes() const noexcept { return 0; }

    virtual void constructInstrumentState(void* ptr) const noexcept { (void)ptr; }
    virtual void destroyInstrumentState(void* ptr) const noexcept { (void)ptr; }

    virtual void instrumentInit(void* inst, float sampleRate, const PatchConfig& cfg) const noexcept {
        (void)inst;
        (void)sampleRate;
        (void)cfg;
    }

    virtual void setSampleRate(void* inst, float sampleRate) const noexcept {
        (void)inst;
        (void)sampleRate;
    }

    virtual void applySampleRateToVoices(void* inst, uint8_t maxVoices, unsigned char* voiceBytes,
                                         std::size_t voiceStride, float sampleRate) const noexcept {
        (void)inst;
        (void)maxVoices;
        (void)voiceBytes;
        (void)voiceStride;
        (void)sampleRate;
    }

    virtual void voiceStart(void* inst, void* voice, uint8_t voiceIndex, const VoiceContext& ctx,
                            float sampleRate, const PatchConfig& cfg) const noexcept = 0;
    virtual void voiceRenderAdd(void* inst, void* voice, uint8_t voiceIndex, float* out, uint64_t n,
                                float sampleRate, const PatchConfig& cfg) const noexcept = 0;

    virtual float voicePan(const void* voice, uint8_t voiceIndex) const noexcept {
        (void)voice; (void)voiceIndex; return 0.0f;
    }

    virtual bool setParam(PatchConfig& cfg, uint16_t paramId, float value) const noexcept {
        (void)cfg;
        (void)paramId;
        (void)value;
        return false;
    }
};
