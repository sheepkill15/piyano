#include "FMInstrument.h"
#include "IInstrument.h"
#include "workstation/Params.h"
#include <cmath>
#include <cstring>

bool FMInstrument::setParam(uint16_t paramId, float value) noexcept {
    if (IInstrument::setParam(paramId, value)) return true;
    switch (paramId) {
        case Params::FmModIndex:
            // Map 0..1 -> 0..10
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            modIndex_ = value * 10.0f;
            return true;
        case Params::FmModRatio:
            // Map 0..1 -> 0.25..8.0
            if (value < 0.0f) value = 0.0f;
            if (value > 1.0f) value = 1.0f;
            modRatio_ = 0.25f + value * (8.0f - 0.25f);
            return true;
        default:
            return false;
    }
}

void FMInstrument::onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept {
    baseFreq_[voiceIndex] = frequency;
    amplitude_[voiceIndex] = velocity;
}

void FMInstrument::renderAddVoice(uint8_t v, float* out, uint64_t numSamples) noexcept {
    const float sr = getSampleRate();
    float cph = carrierPhase_[v];
    float mph = modPhase_[v];
    const float f = baseFreq_[v];
    const float a = amplitude_[v];
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    
    for (uint64_t i = 0; i < numSamples; ++i) {
        float envLevel = getEnvelope(v)->level;
        float mod = sinf(mph) * modIndex_;
        float s = sinf(cph + mod) * a * envLevel;
        out[i] += s;
        mph += twoPi * (f * modRatio_) / sr;
        cph += twoPi * f / sr;
        if (mph >= twoPi) mph = fmodf(mph, twoPi);
        if (cph >= twoPi) cph = fmodf(cph, twoPi);
    }
    carrierPhase_[v] = cph;
    modPhase_[v] = mph;
}