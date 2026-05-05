#ifndef SYNTHENGINE_H
#define SYNTHENGINE_H

#include <array>
#include <cstdint>
#include "synth/Constants.h"
#include "synth/voice/VoiceAllocator.h"
#include "synth/modules/Adsr.h"
#include "synth/mix/GlobalMixer.h"

class InstrumentManager;

class SynthEngine {
    public:
        void init(InstrumentManager* instruments) noexcept;

        void bindInstruments(InstrumentManager* instruments) noexcept;

        void noteOn(uint8_t note, float vel) noexcept;

        void noteOff(uint8_t note) noexcept;

        bool setParam(uint16_t paramId, float value) noexcept;

        // Renders interleaved stereo (L,R,L,R,...) of `nFrames` frames.
        void render(float* stereoLR, uint64_t nFrames) noexcept;
        using StereoBlock = std::array<float, synth::cfg::kAudioRenderBlockSamples * 2>;
        void render(StereoBlock& stereoLR) noexcept { render(stereoLR.data(), synth::cfg::kAudioRenderBlockSamples); }

        void update(float dt) noexcept;

    private:
        InstrumentManager* instruments_ = nullptr;

        static constexpr uint8_t MAX_VOICES = synth::cfg::kPatchMaxVoices;
        synth::voice::VoiceAllocator<MAX_VOICES> allocator_ = {};
        std::array<synth::modules::Adsr, MAX_VOICES> ampEnv_{};
        std::array<float, MAX_VOICES> voiceVelocity_{};
        synth::voice::SameNoteMode sameNoteMode_ = synth::voice::SameNoteMode::SingleVoicePerKey;
        synth::mix::GlobalMixer mixer_ = {};
    };
#endif
