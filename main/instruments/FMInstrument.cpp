#include "FMInstrument.h"
#include "IInstrument.h"
#include <cmath>
#include <cstring>

void FMInstrument::setSampleRate(float sr) noexcept {
    sampleRate_ = sr;
}

void FMInstrument::noteOn(float frequency, float velocity) noexcept {
    baseFreq_ = frequency;
    // Don't reset phases to avoid discontinuities
    amplitude_ = velocity;
    active_ = true;
}

void FMInstrument::noteOff(float frequency) noexcept {
    active_ = false;
}

void FMInstrument::process(float* out, uint64_t numSamples) noexcept {
    if (!active_) {
        memset(out, 0, numSamples * sizeof(float));
        return;
    }
    for (uint64_t i = 0; i < numSamples; ++i) {
        float mod = sinf(modPhase_) * modIndex_;
        float s = sinf(carrierPhase_ + mod) * amplitude_;
        out[i] = s;
        modPhase_ += 2.0f * M_PI * (baseFreq_ * modRatio_) / sampleRate_;
        carrierPhase_ += 2.0f * M_PI * baseFreq_ / sampleRate_;
        // Use fmod for better phase wrapping
        modPhase_ = fmodf(modPhase_, 2.0f * M_PI);
        carrierPhase_ = fmodf(carrierPhase_, 2.0f * M_PI);
    }
}