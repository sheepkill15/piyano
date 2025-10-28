#ifndef SUBTRACTIVEINSTRUMENT_H
#define SUBTRACTIVEINSTRUMENT_H

#include "IInstrument.h"

class SubtractiveInstrument : public IInstrument {
    public:
        void setSampleRate(float sr) noexcept override;
    
        void noteOn(float frequency, float velocity) noexcept override;
    
        void noteOff(float frequency) noexcept override;
    
        void process(float* out, uint64_t numSamples) noexcept override;
    
    private:
        float sampleRate_ = 44100.0f;
        float freq_ = 440.0f;
        float phase_ = 0.0f;
        float amplitude_ = 0.0f;
        bool active_ = false;
};
#endif