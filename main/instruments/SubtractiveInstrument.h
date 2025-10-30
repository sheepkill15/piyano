#ifndef SUBTRACTIVEINSTRUMENT_H
#define SUBTRACTIVEINSTRUMENT_H

#include "IInstrument.h"

class SubtractiveInstrument : public IInstrument {
    public:
        void noteOn(float frequency, float velocity) noexcept override;
        void noteOff(float frequency) noexcept override;
    
    private:
        void renderAddVoice(uint8_t voiceIndex, float* out, uint64_t numSamples) noexcept override;
        void onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept override;

        float baseFreq_[MAX_VOICES] = {};
        float amplitude_[MAX_VOICES] = {};
        float phase_[MAX_VOICES] = {};
};
#endif