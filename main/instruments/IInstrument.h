#ifndef IINSTRUMENT_H
#define IINSTRUMENT_H

#include <cstdint>
#include <cstring>
#include <cmath>

// Abstract base with shared voice handling and sample-rate storage
class IInstrument {
    public:
        virtual ~IInstrument() = default;

        enum class SameNoteMode : uint8_t {
            SingleVoicePerKey,   // synth-style: reuse existing voice for the key
            MultiVoicePerKey     // piano-style: stack multiple voices per key
        };

        void setSameNoteMode(SameNoteMode mode) noexcept { sameNoteMode_ = mode; }
        SameNoteMode sameNoteMode() const noexcept { return sameNoteMode_; }

        void setMaxVoices(uint8_t n) noexcept {
            if (n < 1) n = 1;
            if (n > MAX_VOICES) n = MAX_VOICES;
            maxVoices_ = n;
            if (nextVoiceIndex_ >= maxVoices_) nextVoiceIndex_ = 0;
        }
        uint8_t maxVoices() const noexcept { return maxVoices_; }

        // Polyphonic voice-handling
        virtual void noteOn(uint8_t note, float velocity) noexcept {
            const float frequency = midiNoteToFrequency(note);

            if (sameNoteMode_ == SameNoteMode::SingleVoicePerKey) {
                const uint8_t existing = lastVoiceForNote_(note);
                if (existing != kInvalidVoice) {
                    // Reuse existing voice for this note (retrigger)
                    voices_[existing].note = note;
                    voices_[existing].frequency = frequency;
                    voices_[existing].velocity = velocity;
                    voices_[existing].active = true;
                    envelope_[existing].phase = EnvState::Attack;
                    envelope_[existing].level = 0.0f;
                    envelope_[existing].timeInPhase = 0.0f;
                    onVoiceStart(existing, frequency, velocity);
                    return;
                }
            }

            uint8_t v = allocateVoice_(note);
            voices_[v].note = note;
            voices_[v].frequency = frequency;
            voices_[v].velocity = velocity;
            voices_[v].active = true;
            envelope_[v].phase = EnvState::Attack;
            envelope_[v].level = 0.0f;
            envelope_[v].timeInPhase = 0.0f;
            onVoiceStart(v, frequency, velocity);
            pushVoiceForNote_(note, v);
        }

        virtual void noteOff(uint8_t note) noexcept {
            uint8_t v = kInvalidVoice;

            if (sameNoteMode_ == SameNoteMode::MultiVoicePerKey) {
                v = popVoiceForNote_(note);
            } else {
                v = takeLastVoiceForNote_(note);
            }

            if (v == kInvalidVoice) return;
            if (envelope_[v].phase == EnvState::Idle) return;

            voices_[v].active = false;
            EnvState_& env = envelope_[v];
                    env.releaseStartLevel = env.level;
                    env.phase = EnvState::Release;
                    env.timeInPhase = 0.0f;
            onVoiceStop(v);
        }

        virtual void process(float* outBuffer, uint64_t numSamples) noexcept {
            std::memset(outBuffer, 0, static_cast<size_t>(numSamples) * sizeof(float));
            bool any = false;
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                if (envelope_[i].phase == EnvState::Idle) {
                    voices_[i].active = false;
                    continue;
                }
                if (voices_[i].active || envelope_[i].phase == EnvState::Release) {
                    any = true;
                    renderAddVoice(i, outBuffer, numSamples);
                }
            }
            if (!any) return; // already zeroed
            // Apply soft clipping to all voices
            for (uint64_t i = 0; i < numSamples; ++i) {
                outBuffer[i] = softClip(outBuffer[i]);
            }
        }
        inline float softClip(float x) noexcept { const float limit = 0.95f; return tanhf(x / limit) * limit; }

        // Store sample rate centrally
        virtual void setSampleRate(float sr) noexcept { sampleRate_ = sr; }

        virtual bool setParam(uint16_t paramId, float value) noexcept {
            switch (paramId) {
                case 1: setAttack(value); return true;
                case 2: setDecay(value); return true;
                case 3: setSustain(value); return true;
                case 4: setRelease(value); return true;
                default: return false;
            }
        }

        // ADSR envelope controls
        void setAttack(float seconds) noexcept { attack_ = seconds; }
        void setDecay(float seconds) noexcept { decay_ = seconds; }
        void setSustain(float level) noexcept { sustain_ = level; }
        void setRelease(float seconds) noexcept { release_ = seconds; }
        void updateEnvelope(float dt) noexcept {
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                EnvState_& env = envelope_[i];
                env.timeInPhase += dt;
                
                switch (env.phase) {
                    case EnvState::Attack: {
                        if (attack_ > 0.0f && env.timeInPhase < attack_) {
                            env.level = env.timeInPhase / attack_;
                        } else {
                            env.level = 1.0f;
                            env.phase = EnvState::Decay;
                            env.timeInPhase = 0.0f;
                        }
                        break;
                    }
                    case EnvState::Decay: {
                        if (decay_ > 0.0f && env.timeInPhase < decay_) {
                            float t = env.timeInPhase / decay_;
                            env.level = 1.0f + (sustain_ - 1.0f) * t;
                        } else {
                            env.level = sustain_;
                            env.phase = EnvState::Sustain;
                            env.timeInPhase = 0.0f;
                        }
                        break;
                    }
                    case EnvState::Sustain: {
                        env.level = sustain_;
                        break;
                    }
                    case EnvState::Release: {
                        if (release_ > 0.0f && env.timeInPhase < release_) {
                            float t = env.timeInPhase / release_;
                            env.level = env.releaseStartLevel * (1.0f - t);
                        } else {
                            env.level = 0.0f;
                            env.phase = EnvState::Idle;
                        }
                        break;
                    }
                    case EnvState::Idle: {
                        env.level = 0.0f;
                        break;
                    }
                }
            }
        }
    protected:
        static constexpr uint8_t MAX_VOICES = 8;
        static constexpr uint8_t MAX_NOTES = 128;
        static constexpr uint8_t kInvalidVoice = 0xFF;

        struct VoiceState { uint8_t note = 0; float frequency = 0.0f; float velocity = 0.0f; bool active = false; };

        enum class EnvState { Attack, Decay, Sustain, Release, Idle };
        struct EnvState_ {
            EnvState phase = EnvState::Idle;
            float level = 0.0f;
            float timeInPhase = 0.0f;
            float releaseStartLevel = 0.0f;
        };

        // Subclasses implement per-voice rendering (must add into outBuffer)
        virtual void renderAddVoice(uint8_t voiceIndex, float* outBuffer, uint64_t numSamples) noexcept = 0;

        // Optional hooks for subclasses on voice transitions
        virtual void onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept { (void)voiceIndex; (void)frequency; (void)velocity; }
        virtual void onVoiceStop(uint8_t voiceIndex) noexcept { (void)voiceIndex; }

        inline float getSampleRate() const noexcept { return sampleRate_; }
        inline const VoiceState* getVoices() const noexcept { return voices_; }
        inline VoiceState* getMutableVoices() noexcept { return voices_; }
        
        
        
        inline const EnvState_* getEnvelope(uint8_t v) const noexcept { return &envelope_[v]; }

    private:
        static inline float midiNoteToFrequency(uint8_t note) noexcept {
            return 440.0f * powf(2.0f, (static_cast<int>(note) - 69) / 12.0f);
        }

        inline void detachVoiceFromNote_(uint8_t voiceIndex) noexcept {
            const uint8_t note = voices_[voiceIndex].note;
            if (note >= MAX_NOTES) return;
            uint8_t& count = noteVoiceCount_[note];
            for (uint8_t i = 0; i < count; ++i) {
                if (noteVoices_[note][i] == voiceIndex) {
                    // swap-remove
                    noteVoices_[note][i] = noteVoices_[note][static_cast<uint8_t>(count - 1)];
                    count = static_cast<uint8_t>(count - 1);
                    return;
                }
            }
        }

        inline uint8_t allocateVoice_(uint8_t newNote) noexcept {
            const uint8_t limit = maxVoices_;

            // First prioritize idle voices
            for (uint8_t i = 0; i < limit; ++i) {
                if (envelope_[i].phase == EnvState::Idle) return i;
            }

            // Then prioritize voices in release phase
            for (uint8_t i = 0; i < limit; ++i) {
                if (envelope_[i].phase == EnvState::Release) return i;
            }

            // Finally, steal round-robin from anything
            uint8_t v = nextVoiceIndex_;
            nextVoiceIndex_ = static_cast<uint8_t>((nextVoiceIndex_ + 1) % limit);
            (void)newNote;
            detachVoiceFromNote_(v);
            return v;
        }

        float sampleRate_ = 44100.0f;
        VoiceState voices_[MAX_VOICES] = {};
        EnvState_ envelope_[MAX_VOICES] = {};
        uint8_t nextVoiceIndex_ = 0;
        uint8_t maxVoices_ = MAX_VOICES;
        SameNoteMode sameNoteMode_ = SameNoteMode::SingleVoicePerKey;

        uint8_t noteVoiceCount_[MAX_NOTES] = {};
        uint8_t noteVoices_[MAX_NOTES][MAX_VOICES] = {};
        
        float attack_ = 0.01f;
        float decay_ = 0.1f;
        float sustain_ = 0.7f;
        float release_ = 0.2f;

        inline uint8_t lastVoiceForNote_(uint8_t note) const noexcept {
            if (note >= MAX_NOTES) return kInvalidVoice;
            const uint8_t count = noteVoiceCount_[note];
            if (count == 0) return kInvalidVoice;
            return noteVoices_[note][static_cast<uint8_t>(count - 1)];
        }

        inline void pushVoiceForNote_(uint8_t note, uint8_t voiceIndex) noexcept {
            if (note >= MAX_NOTES) return;
            uint8_t& count = noteVoiceCount_[note];
            if (count >= MAX_VOICES) {
                // Shouldn't happen with global max voices, but keep it safe.
                noteVoices_[note][MAX_VOICES - 1] = voiceIndex;
                count = MAX_VOICES;
                return;
            }
            noteVoices_[note][count] = voiceIndex;
            count = static_cast<uint8_t>(count + 1);
        }

        inline uint8_t popVoiceForNote_(uint8_t note) noexcept {
            if (note >= MAX_NOTES) return kInvalidVoice;
            uint8_t& count = noteVoiceCount_[note];
            while (count > 0) {
                const uint8_t v = noteVoices_[note][static_cast<uint8_t>(count - 1)];
                count = static_cast<uint8_t>(count - 1);
                if (v < MAX_VOICES && envelope_[v].phase != EnvState::Idle && voices_[v].note == note) return v;
            }
            return kInvalidVoice;
        }

        inline uint8_t takeLastVoiceForNote_(uint8_t note) noexcept {
            // Like pop, but clears all mapping for the note (single-voice semantics)
            if (note >= MAX_NOTES) return kInvalidVoice;
            uint8_t v = popVoiceForNote_(note);
            noteVoiceCount_[note] = 0;
            return v;
        }
};
#endif