#ifndef IINSTRUMENT_H
#define IINSTRUMENT_H

#include <cstdint>

class IInstrument {
    public:
    virtual ~IInstrument() = default;

    virtual void noteOn(float frequency, float velocity) noexcept = 0;
    virtual void noteOff(float frequency) noexcept = 0;
    virtual void process(float* outBuffer, uint64_t numSamples) noexcept = 0;
    virtual void setSampleRate(float sr) noexcept = 0;
};    
#endif