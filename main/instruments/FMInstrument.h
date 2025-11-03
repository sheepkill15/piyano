#ifndef FMINSTRUMENT_H
#define FMINSTRUMENT_H

#include "IInstrument.h"

class FMInstrument : public IInstrument {
    public:
        void renderAddVoice(uint8_t voiceIndex, float* out, uint64_t numSamples) noexcept override;
        void onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept override;

    private:
        float baseFreq_[MAX_VOICES] = {};
        float amplitude_[MAX_VOICES] = {};
        float modIndex_ = 2.0f;
        float modRatio_ = 1.5f;
        float carrierPhase_[MAX_VOICES] = {};
        float modPhase_[MAX_VOICES] = {};
    };
#endif