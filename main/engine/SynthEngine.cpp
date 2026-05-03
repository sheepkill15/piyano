#include "SynthEngine.h"
#include "instruments/InstrumentManager.h"
#include "engine/AudioContext.h"
#include <cstring>
#include <cmath>

namespace {
constexpr uint64_t kBlockSize = 256;
// ~1/(3..4) per voice keeps 3–4 note chords under the soft limiter knee with
// resonant patches; still loud enough at unity master.
constexpr float kVoiceGain = 0.28f;
}

void SynthEngine::init(InstrumentManager* instruments) noexcept {
    instruments_ = instruments;
    allocator_.reset();
    for (auto& e : ampEnv_) e.reset();
    for (auto& v : voiceVelocity_) v = 0.0f;
    mixer_.init(engine::gAudio.sampleRate);
    if (instruments_) {
        allocator_.setMaxVoices(instruments_->defaultMaxVoices());
        sameNoteMode_ = instruments_->defaultSameNoteMode();
    }
}

void SynthEngine::bindInstruments(InstrumentManager* instruments) noexcept {
    instruments_ = instruments;
    allocator_.reset();
    for (auto& e : ampEnv_) e.reset();
    for (auto& v : voiceVelocity_) v = 0.0f;
    if (instruments_) {
        allocator_.setMaxVoices(instruments_->defaultMaxVoices());
        sameNoteMode_ = instruments_->defaultSameNoteMode();
    }
}

void SynthEngine::noteOn(uint8_t note, float vel) noexcept {
    if (!instruments_) return;
    synth::voice::NoteEvent ev;
    ev.note = note;
    ev.velocity = vel;
    ev.frequency = 440.0f * powf(2.0f, (static_cast<int>(note) - 69) / 12.0f);

    const uint8_t v = allocator_.noteOn(ev, sameNoteMode_);
    ampEnv_[v].noteOn();
    voiceVelocity_[v] = vel;

    VoiceContext ctx;
    ctx.note = note;
    ctx.velocity = vel;
    ctx.frequency = ev.frequency;
    instruments_->onVoiceStart(v, ctx);
}

void SynthEngine::noteOff(uint8_t note) noexcept {
    if (!instruments_) return;
    const uint8_t v = allocator_.noteOff(note, sameNoteMode_);
    if (v == synth::voice::kInvalidVoice) return;
    ampEnv_[v].noteOff();
    instruments_->onVoiceStop(v);
}

bool SynthEngine::setParam(uint16_t paramId, float value) noexcept {
    switch (paramId) {
        case 10: // MasterGain
            mixer_.setMasterGain(value);
            return true;
        case 1: // Attack
            for (auto& e : ampEnv_) e.attack_s = value;
            return true;
        case 2: // Decay
            for (auto& e : ampEnv_) e.decay_s = value;
            return true;
        case 3: // Sustain
            for (auto& e : ampEnv_) e.sustain = value;
            return true;
        case 4: // Release
            for (auto& e : ampEnv_) e.release_s = value;
            return true;
        default:
            break;
    }
            return instruments_ ? instruments_->setParam(paramId, value) : false;
}

void SynthEngine::render(float* stereoLR, uint64_t nFrames) noexcept {
    std::memset(stereoLR, 0, static_cast<size_t>(nFrames) * 2 * sizeof(float));
    if (!instruments_) return;

    const float dtPerSample = engine::gAudio.invSampleRate;

    static float voiceTmp[kBlockSize];
    static float envBuf[kBlockSize];

    uint64_t offset = 0;
    while (offset < nFrames) {
        const uint64_t block = (nFrames - offset > kBlockSize) ? kBlockSize : (nFrames - offset);

        for (uint8_t v = 0; v < MAX_VOICES; ++v) {
            if (!ampEnv_[v].isActive()) continue;

            // Per-sample envelope curve for the block.
            for (uint64_t i = 0; i < block; ++i) {
                ampEnv_[v].tick(dtPerSample);
                envBuf[i] = ampEnv_[v].level;
            }
            if (!ampEnv_[v].isActive()) {
                // Quick fade-out to zero (silence the tail to avoid clicks)
            }

            std::memset(voiceTmp, 0, static_cast<size_t>(block) * sizeof(float));
            instruments_->renderAddVoice(v, voiceTmp, block);

            // Velocity-aware gain (a bit of velocity sensitivity baked into the engine).
            const float vel = voiceVelocity_[v];
            const float velGain = 0.45f + 0.55f * vel;

            // Equal-power pan
            float pan = instruments_->voicePan(v);
            if (pan < -1.0f) pan = -1.0f;
            if (pan > 1.0f) pan = 1.0f;
            const float panAng = (pan + 1.0f) * 0.25f * static_cast<float>(M_PI); // 0..pi/2
            const float panL = cosf(panAng);
            const float panR = sinf(panAng);

            float* dst = stereoLR + offset * 2;
            const float gL = panL * velGain * kVoiceGain;
            const float gR = panR * velGain * kVoiceGain;
            for (uint64_t i = 0; i < block; ++i) {
                const float s = voiceTmp[i] * envBuf[i];
                dst[2 * i]     += s * gL;
                dst[2 * i + 1] += s * gR;
            }
        }

        offset += block;
    }

    mixer_.processStereo(stereoLR, nFrames);
}

void SynthEngine::update(float dt) noexcept {
    (void)dt;
    // Envelopes are now ticked per-sample inside render().
}
