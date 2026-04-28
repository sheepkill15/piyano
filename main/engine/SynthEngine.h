#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <cstdint>
#include "IInstrument.h"
#include "synth/voice/VoiceAllocator.h"
#include "synth/modules/Adsr.h"
#include "synth/mix/GlobalMixer.h"

class SynthEngine {
    public:
        void init(IInstrument* initialInstrument, float sampleRate) noexcept;

        void switchInstrument(IInstrument* newInstrument) noexcept;

        void noteOn(uint8_t note, float vel) noexcept;

        void noteOff(uint8_t note) noexcept;

        bool setParam(uint16_t paramId, float value) noexcept;

        // Renders interleaved stereo (L,R,L,R,...) of `nFrames` frames.
        void render(float* stereoLR, uint64_t nFrames) noexcept;

        void update(float dt) noexcept;

    private:
        IInstrument* instrument = nullptr;
        float sampleRate = 44100.0f;

        static constexpr uint8_t MAX_VOICES = 8;
        synth::voice::VoiceAllocator<MAX_VOICES> allocator_ = {};
        synth::modules::Adsr ampEnv_[MAX_VOICES] = {};
        float voiceVelocity_[MAX_VOICES] = {};
        synth::voice::SameNoteMode sameNoteMode_ = synth::voice::SameNoteMode::SingleVoicePerKey;
        synth::mix::GlobalMixer mixer_ = {};
    };
#endif
