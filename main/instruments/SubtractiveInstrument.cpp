#include "SubtractiveInstrument.h"
#include "IInstrument.h"
#include <cmath>
#include <cstring>

void SubtractiveInstrument::setSampleRate(float sr) noexcept {
    sampleRate_ = sr;
}

void SubtractiveInstrument::noteOn(float frequency, float velocity) noexcept {
    // Don't reset phase to avoid discontinuities
    freq_ = frequency;
    amplitude_ = velocity;
    active_ = true;
}

void SubtractiveInstrument::noteOff(float frequency) noexcept {
    active_ = false;
}

void SubtractiveInstrument::process(float* out, uint64_t numSamples) noexcept {
    if (!active_) {
        memset(out, 0, numSamples * sizeof(float));
        return;
    }
    for (size_t i = 0; i < numSamples; ++i) {
        float s = sinf(phase_) * amplitude_;
        out[i] = s;
        phase_ += 2.0f * M_PI * freq_ / sampleRate_;
        // Use fmod for better phase wrapping
        phase_ = fmodf(phase_, 2.0f * M_PI);
    }
}