#include "FMInstrument.h"
#include "IInstrument.h"
#include <cmath>
#include <cstring>

void FMInstrument::noteOn(float frequency, float velocity) noexcept {
    // Base handles allocation and calls onVoiceStart
    IInstrument::noteOn(frequency, velocity);
}

void FMInstrument::noteOff(float frequency) noexcept {
    IInstrument::noteOff(frequency);
}

void FMInstrument::onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept {
    baseFreq_[voiceIndex] = frequency;
    amplitude_[voiceIndex] = velocity;
    // keep existing phases for smoothness
}

void FMInstrument::renderAddVoice(uint8_t v, float* out, uint64_t numSamples) noexcept {
    const float sr = getSampleRate();
    float cph = carrierPhase_[v];
    float mph = modPhase_[v];
    const float f = baseFreq_[v];
    const float a = amplitude_[v];
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    for (uint64_t i = 0; i < numSamples; ++i) {
        float mod = sinf(mph) * modIndex_;
        float s = sinf(cph + mod) * a;
        out[i] += s;
        mph += twoPi * (f * modRatio_) / sr;
        cph += twoPi * f / sr;
        if (mph >= twoPi) mph = fmodf(mph, twoPi);
        if (cph >= twoPi) cph = fmodf(cph, twoPi);
    }
    carrierPhase_[v] = cph;
    modPhase_[v] = mph;
}