#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <cstdint>
#include "IInstrument.h"

class SynthEngine {
    public:
        void init(IInstrument* initialInstrument, float sampleRate) noexcept;
    
        void switchInstrument(IInstrument* newInstrument) noexcept;
    
        void noteOn(uint8_t note, float vel) noexcept;
    
        void noteOff(uint8_t note) noexcept;
    
        void render(float* out, uint64_t nSamples) noexcept;

        void update(float dt) noexcept;
    
    private:
        IInstrument* instrument = nullptr;
        float sampleRate;
    };
#endif