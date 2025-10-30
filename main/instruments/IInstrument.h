#ifndef IINSTRUMENT_H
#define IINSTRUMENT_H

#include <cstdint>
#include <cstring>

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
            onVoiceStart(v, frequency, velocity);
        }

        virtual void noteOff(float frequency) noexcept {
            // Deactivate the first matching voice
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                if (voices_[i].active && voices_[i].frequency == frequency) {
                    voices_[i].active = false;
                    onVoiceStop(i);
                    break;
                }
            }
        }

        virtual void process(float* outBuffer, uint64_t numSamples) noexcept {
            std::memset(outBuffer, 0, static_cast<size_t>(numSamples) * sizeof(float));
            bool any = false;
            for (uint8_t i = 0; i < MAX_VOICES; ++i) {
                if (!voices_[i].active) continue;
                any = true;
                renderAddVoice(i, outBuffer, numSamples);
            }
            if (!any) {
                // already zeroed
                return;
            }
        }

        // Store sample rate centrally
        virtual void setSampleRate(float sr) noexcept { sampleRate_ = sr; }

    protected:
        static constexpr uint8_t MAX_VOICES = 8;

        struct VoiceState { float frequency = 0.0f; float velocity = 0.0f; bool active = false; };

        // Subclasses implement per-voice rendering (must add into outBuffer)
        virtual void renderAddVoice(uint8_t voiceIndex, float* outBuffer, uint64_t numSamples) noexcept = 0;

        // Optional hooks for subclasses on voice transitions
        virtual void onVoiceStart(uint8_t voiceIndex, float frequency, float velocity) noexcept { (void)voiceIndex; (void)frequency; (void)velocity; }
        virtual void onVoiceStop(uint8_t voiceIndex) noexcept { (void)voiceIndex; }

        inline float getSampleRate() const noexcept { return sampleRate_; }
        inline const VoiceState* getVoices() const noexcept { return voices_; }
        inline VoiceState* getMutableVoices() noexcept { return voices_; }

    private:
        inline uint8_t allocateVoice_() noexcept {
            // Find free
            for (uint8_t i = 0; i < MAX_VOICES; ++i) if (!voices_[i].active) return i;
            // Steal round-robin
            uint8_t v = nextVoiceIndex_;
            nextVoiceIndex_ = static_cast<uint8_t>((nextVoiceIndex_ + 1) % MAX_VOICES);
            return v;
        }

        float sampleRate_ = 44100.0f;
        VoiceState voices_[MAX_VOICES] = {};
        uint8_t nextVoiceIndex_ = 0;
};
#endif