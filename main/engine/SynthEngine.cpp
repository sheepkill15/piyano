#include "SynthEngine.h"
#include <cstring>
#include <cmath>

void SynthEngine::init(IInstrument* initialInstrument, float sampleRate) noexcept {
instrument = initialInstrument;
    this->sampleRate = sampleRate;
    allocator_.reset();
    for (auto& e : ampEnv_) e.reset();
    if (instrument) {
        instrument->setSampleRate(sampleRate);
        allocator_.setMaxVoices(instrument->defaultMaxVoices());
        sameNoteMode_ = instrument->defaultSameNoteMode();
    }
}

void SynthEngine::switchInstrument(IInstrument* newInstrument) noexcept {
    instrument = newInstrument;
    allocator_.reset();
    for (auto& e : ampEnv_) e.reset();
    if (instrument) {
        instrument->setSampleRate(sampleRate);
        allocator_.setMaxVoices(instrument->defaultMaxVoices());
        sameNoteMode_ = instrument->defaultSameNoteMode();
    }
}

void SynthEngine::noteOn(uint8_t note, float vel) noexcept {
    if (!instrument) return;
    synth::voice::NoteEvent ev;
    ev.note = note;
    ev.velocity = vel;
    ev.frequency = 440.0f * powf(2.0f, (static_cast<int>(note) - 69) / 12.0f);

    const uint8_t v = allocator_.noteOn(ev, sameNoteMode_);
    ampEnv_[v].noteOn();

    VoiceContext ctx;
    ctx.note = note;
    ctx.velocity = vel;
    ctx.frequency = ev.frequency;
    instrument->onVoiceStart(v, ctx);
}

void SynthEngine::noteOff(uint8_t note) noexcept {
    if (!instrument) return;
    const uint8_t v = allocator_.noteOff(note, sameNoteMode_);
    if (v == synth::voice::kInvalidVoice) return;
    ampEnv_[v].noteOff();
    instrument->onVoiceStop(v);
}

bool SynthEngine::setParam(uint16_t paramId, float value) noexcept {
    // Keep ADSR compatible with existing param IDs (1..4).
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
    return instrument ? instrument->setParam(paramId, value) : false;
}

void SynthEngine::render(float* out, uint64_t nSamples) noexcept {
    std::memset(out, 0, static_cast<size_t>(nSamples) * sizeof(float));
    if (!instrument) return;

    static float voiceTmp[1024];
    if (nSamples > 1024) {
        // Fallback: render directly into out in chunks (no extra allocations)
        uint64_t offset = 0;
        while (offset < nSamples) {
            const uint64_t block = (nSamples - offset > 1024) ? 1024 : (nSamples - offset);
            for (uint8_t v = 0; v < MAX_VOICES; ++v) {
                if (!ampEnv_[v].isActive()) continue;
                std::memset(voiceTmp, 0, static_cast<size_t>(block) * sizeof(float));
                instrument->renderAddVoice(v, voiceTmp, block);
                const float env = ampEnv_[v].level;
                for (uint64_t i = 0; i < block; ++i) out[offset + i] += voiceTmp[i] * env;
            }
            offset += block;
        }
        return;
    }

    for (uint8_t v = 0; v < MAX_VOICES; ++v) {
        if (!ampEnv_[v].isActive()) continue;
        std::memset(voiceTmp, 0, static_cast<size_t>(nSamples) * sizeof(float));
        instrument->renderAddVoice(v, voiceTmp, nSamples);
        const float env = ampEnv_[v].level;
        for (uint64_t i = 0; i < nSamples; ++i) out[i] += voiceTmp[i] * env;
    }

    mixer_.processMono(out, nSamples);
}

void SynthEngine::update(float dt) noexcept {
    (void)instrument;
    for (auto& e : ampEnv_) e.tick(dt);
}