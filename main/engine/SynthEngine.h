#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <cstdint>
#include "IInstrument.h"

class SynthEngine {
    public:
        void init(IInstrument* initialInstrument, float sampleRate) noexcept;
    
        void switchInstrument(IInstrument* newInstrument) noexcept;
    
        void noteOn(float frequency, float vel) noexcept;
    
        void noteOff(float frequency) noexcept;
    
        void render(float* out, uint64_t nSamples) noexcept;

        void update(float dt) noexcept;
    
    private:
        IInstrument* instrument = nullptr;
        float sampleRate;
    };
#endif