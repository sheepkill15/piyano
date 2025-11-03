#ifndef IINSTRUMENT_H
#define IINSTRUMENT_H

#include <cstdint>
#include <cstring>
#include <cmath>

// Abstract base with shared voice handling and sample-rate storage
class IInstrument {
    public:
        virtual ~IInstrument() = default;

        // Polyphonic voice-handling
        virtual void noteOn(float frequency, float velocity) noexcept {
            uint8_t v = allocateVoice_();
            voices_[v].frequency = frequency;
            voices_[v].velocity = velocity;
            voices_[v].active = true;
            envelope_[v].phase = EnvState::Attack;
            envelope_[v].level = 0.0f;
            envelope_[v].timeInPhase = 0.0f;
            onVoiceStart(v, frequency, velocity);
        }

        virtual void noteOff(float frequency) noexcept {
            // Deactivate the first matching voice
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                if (voices_[i].active && voices_[i].frequency == frequency) {
                    voices_[i].active = false;
                    EnvState_& env = envelope_[i];
                    env.releaseStartLevel = env.level;
                    env.phase = EnvState::Release;
                    env.timeInPhase = 0.0f;
                    onVoiceStop(i);
                    break;
                }
            }
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

        struct VoiceState { float frequency = 0.0f; float velocity = 0.0f; bool active = false; };

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
        inline uint8_t allocateVoice_() noexcept {
            // First prioritize idle voices
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                if (envelope_[i].phase == EnvState::Idle) return i;
            }
            // Then prioritize voices in release phase
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                if (envelope_[i].phase == EnvState::Release) return i;
            }
            // Finally, steal round-robin from anything
            uint8_t v = nextVoiceIndex_;
            nextVoiceIndex_ = static_cast<uint8_t>((nextVoiceIndex_ + 1) % MAX_VOICES);
            return v;
        }

        float sampleRate_ = 44100.0f;
        VoiceState voices_[MAX_VOICES] = {};
        EnvState_ envelope_[MAX_VOICES] = {};
        uint8_t nextVoiceIndex_ = 0;
        
        float attack_ = 0.01f;
        float decay_ = 0.1f;
        float sustain_ = 0.7f;
        float release_ = 0.2f;
};
#endif