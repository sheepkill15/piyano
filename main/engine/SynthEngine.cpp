#include "SynthEngine.h"
#include "instruments/InstrumentManager.h"
#include "engine/AudioContext.h"
#include "synth/Constants.h"
#include "synth/dsp/Util.h"
#include "workstation/Params.h"
#include <array>
#include <cstring>

namespace {
constexpr uint64_t kBlockSize = synth::cfg::kAudioRenderBlockSamples;
constexpr float kVoiceGain = synth::cfg::kEngineVoiceGain;
}

void SynthEngine::init(InstrumentManager* instruments) noexcept {
    instruments_ = instruments;
    allocator_.reset();
    for (auto& e : ampEnv_) { e.reset(); e.refresh(); }
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
    for (auto& e : ampEnv_) { e.reset(); e.refresh(); }
    for (auto& v : voiceVelocity_) v = 0.0f;
    if (instruments_) {
        allocator_.setMaxVoices(instruments_->defaultMaxVoices());
        sameNoteMode_ = instruments_->defaultSameNoteMode();
    }
}

void SynthEngine::noteOn(const uint8_t note, const float vel) noexcept {
    if (!instruments_) return;
    synth::voice::NoteEvent ev;
    ev.note = note;
    ev.velocity = vel;
    ev.frequency = synth::dsp::midiNoteToHz(note);

    const uint8_t v = allocator_.noteOn(ev, sameNoteMode_);
    ampEnv_[v].noteOn();
    voiceVelocity_[v] = vel;

    VoiceContext ctx;
    ctx.note = note;
    ctx.velocity = vel;
    ctx.frequency = ev.frequency;
    instruments_->onVoiceStart(v, ctx);
}

void SynthEngine::noteOff(const uint8_t note) noexcept {
    if (!instruments_) return;
    const uint8_t v = allocator_.noteOff(note, sameNoteMode_);
    if (v == synth::voice::kInvalidVoice) return;
    ampEnv_[v].noteOff();
    instruments_->onVoiceStop(v);
}

bool SynthEngine::setParam(const uint16_t paramId, const float value) noexcept {
    switch (paramId) {
        case Params::MasterGain:
            mixer_.setMasterGain(value);
            return true;
        case Params::Attack:
            for (auto& e : ampEnv_) { e.attack_s = value; e.refresh(); }
            return true;
        case Params::Decay:
            for (auto& e : ampEnv_) { e.decay_s = value; e.refresh(); }
            return true;
        case Params::Sustain:
            for (auto& e : ampEnv_) e.sustain = value;
            return true;
        case Params::Release:
            for (auto& e : ampEnv_) { e.release_s = value; e.refresh(); }
            return true;
        default:
            break;
    }
            return instruments_ ? instruments_->setParam(paramId, value) : false;
}

void SynthEngine::render(float* stereoLR, const uint64_t nFrames) noexcept {
    std::memset(stereoLR, 0, static_cast<size_t>(nFrames) * 2 * sizeof(float));
    if (!instruments_) return;

    const float dtPerSample = engine::gAudio.invSampleRate;

    static std::array<float, kBlockSize> voiceTmp;
    static std::array<float, kBlockSize> envBuf;

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

            std::memset(voiceTmp.data(), 0, static_cast<size_t>(block) * sizeof(float));
            instruments_->renderAddVoice(v, voiceTmp.data(), block);

            // Velocity-aware gain (a bit of velocity sensitivity baked into the engine).
            const float vel = voiceVelocity_[v];
            const float velGain = 0.45f + 0.55f * vel;

            // Equal-power pan
            const float pan = synth::dsp::clamp(instruments_->voicePan(v), -1.0f, 1.0f);
            const float panAng = (pan + 1.0f) * 0.25f * PI; // 0..pi/2
            const float panL = synth::dsp::sineLURad(panAng);
            const float panR = synth::dsp::sineLURad((PI / 2.0f) - panAng);

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

void SynthEngine::update(const float dt) noexcept {
    (void)dt;
    // Envelopes are now ticked per-sample inside render().
}
