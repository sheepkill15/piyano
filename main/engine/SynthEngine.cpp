#include "SynthEngine.h"
#include <cstring>

void SynthEngine::init(IInstrument* initialInstrument, float sampleRate) noexcept {
instrument = initialInstrument;
    this->sampleRate = sampleRate;
    instrument->setSampleRate(sampleRate);
}

void SynthEngine::switchInstrument(IInstrument* newInstrument) noexcept {
    instrument = newInstrument;
    instrument->setSampleRate(sampleRate);
}

void SynthEngine::noteOn(float frequency, float vel) noexcept {
    if (instrument) instrument->noteOn(frequency, vel);
}

void SynthEngine::noteOff(float frequency) noexcept {
    if (instrument) instrument->noteOff(frequency);
}

void SynthEngine::render(float* out, uint64_t nSamples) noexcept {
    if (instrument) instrument->process(out, nSamples);
    else memset(out, 0, nSamples * sizeof(float));
}

void SynthEngine::update(float dt) noexcept {
    if (instrument) instrument->updateEnvelope(dt);
}