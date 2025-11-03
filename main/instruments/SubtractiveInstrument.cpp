#include "SubtractiveInstrument.h"
#include "IInstrument.h"
#include <cmath>
#include <cstring>

void SubtractiveInstrument::noteOn(float frequency, float velocity) noexcept {
    IInstrument::noteOn(frequency, velocity);
}

void SubtractiveInstrument::noteOff(float frequency) noexcept {
    IInstrument::noteOff(frequency);
}

void SubtractiveInstrument::onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept {
    baseFreq_[voiceIndex] = frequency;
    amplitude_[voiceIndex] = velocity;
    // keep existing phase for smoothness
}

void SubtractiveInstrument::renderAddVoice(uint8_t v, float* out, uint64_t numSamples) noexcept {
    const float sr = getSampleRate();
    float ph = phase_[v];
    const float f = baseFreq_[v];
    const float a = amplitude_[v];
    const float twoPi = 2.0f * static_cast<float>(M_PI);
    for (uint64_t i = 0; i < numSamples; ++i) {
        float envLevel = getEnvelope(v)->level;
        float s = sinf(ph) * a * envLevel;
        out[i] += s;
        ph += twoPi * f / sr;
        if (ph >= twoPi) ph = fmodf(ph, twoPi);
    }
    phase_[v] = ph;
}