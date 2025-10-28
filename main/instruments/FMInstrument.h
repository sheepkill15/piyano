#ifndef FMINSTRUMENT_H
#define FMINSTRUMENT_H

#include "IInstrument.h"

class FMInstrument : public IInstrument {
    public:
        void setSampleRate(float sr) noexcept override;
    
        void noteOn(float frequency, float velocity) noexcept override;
    
        void noteOff(float frequency) noexcept override;
    
        void process(float* out, uint64_t numSamples) noexcept override;
    
    private:
        float sampleRate_ = 44100.0f;
        float baseFreq_ = 440.0f;
        float amplitude_ = 0.0f;
        float modIndex_ = 2.0f;
        float modRatio_ = 1.5f;
        float carrierPhase_ = 0.0f, modPhase_ = 0.0f;
        bool active_ = false;
    };
#endif