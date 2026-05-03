#pragma once

#include <cstddef>
#include <cstdint>

#include "IInstrument.h"
#include "Patch.h"

#include "synth/modules/Adsr.h"
#include "synth/modules/Exciter.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Lfo.h"
#include "synth/modules/Noise.h"
#include "synth/modules/Oscillator.h"

// Generic instrument: a self-contained per-voice DSP graph. setPatch() unpacks a
// synth::patch::Patch once into the per-voice modules (Adsr, Lfo, Svf, ...) and
// the few synth-graph fields that don't have a natural module home (oscillator
// wiring, filter modulation routing, resonator/exciter params). After that, the
// patch is no longer referenced; render runs entirely off the instrument's own
// modules and fields.
class ModularInstrument : public IInstrument {
public:
    ModularInstrument() noexcept = default;

    void setPatch(const synth::patch::Patch& patch) noexcept;

    const char* name() const noexcept override { return name_; }
    uint8_t defaultMaxVoices() const noexcept override { return maxVoices_; }
    synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override {
        return sameNoteMode_;
    }

    void onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(uint8_t v) noexcept override;
    void renderAddVoice(uint8_t v, float* outMono, uint64_t numSamples) noexcept override;
    float voicePan(uint8_t v) const noexcept override;
    bool setParam(uint16_t paramId, float value) noexcept override;

private:
    static constexpr uint8_t MAX_OSCS = synth::patch::MAX_OSCS;
    static constexpr uint8_t MAX_ENVS = synth::patch::MAX_ENVS;
    static constexpr uint8_t MAX_LFOS = synth::patch::MAX_LFOS;
    static constexpr uint8_t MAX_FILTERS = synth::patch::MAX_FILTERS;
    static constexpr uint8_t MAX_VOICES = synth::patch::MAX_VOICES_CAP;

    static constexpr std::size_t kResonatorBufSamples = 4096;
    static_assert((kResonatorBufSamples & (kResonatorBufSamples - 1)) == 0,
                  "kResonatorBufSamples must be a power of two");
    static constexpr std::size_t kResonatorMask = kResonatorBufSamples - 1;

    enum class NoiseKind : uint8_t { Off, White, Pinkish };

    // Per-osc-slot synth-graph data. There is no Oscillator module instance in
    // this synth (phase/feedback live inline in the voice for FM routing), so
    // the slot's wiring (wave, level, FM links, vibrato source, ...) lives here.
    struct OscSlot {
        synth::modules::OscWave wave = synth::modules::OscWave::Sine;
        float pitchRatio = 1.0f;
        float pitchSemitones = 0.0f;
        float pitchCents = 0.0f;
        float pulseWidth = 0.5f;
        float level = 0.0f;
        int8_t fmFrom = -1;
        float fmIndex = 0.0f;
        float fmFeedback = 0.0f;
        int8_t modEnvIndex = -1;
        float velToFmIndex = 0.0f;
        int8_t vibratoLfoIndex = -1;
        float vibratoSemitones = 0.0f;
        bool isFmModulator = false;
    };

    // Per-filter-slot routing. The Svf module on each voice stores `mode` and
    // its tuned `g`/`k`, but the base/peak cutoff and the modulation routing
    // (env, LFO, velocity) needed to retune at note-on or in render live here.
    struct FilterSlot {
        synth::modules::SvfMode mode = synth::modules::SvfMode::LowPass;
        float cutoffHz = 1000.0f;
        float cutoffPeakHz = 1000.0f;
        float resonance = 0.20f;
        int8_t envIndex = -1;
        float velToCutoffHz = 0.0f;
        int8_t lfoIndex = -1;
        float lfoCutoffHz = 0.0f;
    };

    struct VoiceState {
        bool active = false;
        float freq = 0.0f;
        float vel = 0.0f;
        uint8_t note = 0;
        float keyT = 0.0f;
        float pan = 0.0f;

        // Per-osc-slot per-voice state.
        float oscPhase[MAX_OSCS] = {};
        float oscFb[MAX_OSCS] = {};
        float oscOut[MAX_OSCS] = {};
        int   oscSawTbl[MAX_OSCS] = {};

        // Modules carry their own configuration. setPatch() bakes a/d/s/r into
        // each Adsr, wave/rateHz into each Lfo, mode/cutoff into each Svf,
        // color into each PinkishNoise. Note-on only triggers/resets state.
        synth::modules::Adsr modEnvs[MAX_ENVS];     // index 0 unused (engine drives amp env)
        synth::modules::Lfo lfos[MAX_LFOS];
        synth::modules::Svf filters[MAX_FILTERS];
        synth::modules::WhiteNoise white{};
        synth::modules::PinkishNoise pink{};
        synth::modules::ClickExciter click{};
        synth::modules::NoiseBurst burst{};

        // Karplus-Strong body (no dedicated module).
        std::size_t resoIdx = 0;
        std::size_t resoDelay = 1;
        float resoFb = 0.998f;
        float resoDamp = 0.30f;
        float resoLp = 0.0f;
        synth::modules::Svf bodyHp{};
        synth::modules::OnePoleLP outLp{};
    };

    void resetVoice_(uint8_t v) noexcept;
    float* voiceResoBuf_(uint8_t v) noexcept;

    void renderSynth_(uint8_t voiceIdx, float* out, uint64_t n) noexcept;

    void recomputeOscDp_(VoiceState& s, const float vibSemis[MAX_LFOS], float invSr,
                         float oscFreq[MAX_OSCS], float oscDp[MAX_OSCS]) const noexcept;

    // ===== Instrument-level config (built by setPatch()) =====
    const char* name_ = "Init";
    uint8_t maxVoices_ = 8;
    synth::voice::SameNoteMode sameNoteMode_ =
        synth::voice::SameNoteMode::SingleVoicePerKey;

    OscSlot oscs_[MAX_OSCS] = {};
    uint8_t oscCount_ = 0;

    uint8_t envCount_ = 1;          // module config (a/d/s/r) lives on each voice's Adsr.

    // perVoicePhase per LFO slot. At note-on each voice sets its lfo phase to
    // lfoStartPhase_[i] * voiceIndex. Per-voice rate spread is baked directly
    // into the voice's Lfo.rateHz in setPatch().
    float lfoStartPhase_[MAX_LFOS] = {};
    uint8_t lfoCount_ = 0;

    FilterSlot filters_[MAX_FILTERS] = {};
    uint8_t filterCount_ = 0;

    NoiseKind noiseKind_ = NoiseKind::Off;
    float noiseLevel_ = 0.0f;

    bool driveEnabled_ = false;
    float drivePreGain_ = 1.0f;
    float drivePostGain_ = 1.0f;

    bool resonatorEnabled_ = false;
    float resoFeedback_ = 0.998f;
    float resoDampLowKey_ = 0.30f;
    float resoDampHighKey_ = 0.75f;
    float resoBodyMix_ = 0.85f;
    float resoBodyHpHz_ = 45.0f;
    float resoOutputLpHzBase_ = 2800.0f;
    float resoOutputLpHzVel_ = 5500.0f;
    float resoHarmAmount_ = 0.15f;

    bool exciterEnabled_ = false;
    float clickLevelBase_ = 0.55f;
    float clickLevelVel_ = 0.65f;
    float clickDecay_ = 0.92f;
    float burstLevelBase_ = 0.20f;
    float burstLevelVel_ = 0.40f;
    float burstDecay_ = 0.998f;

    float panSpread_ = 0.0f;
    float outputGain_ = 1.0f;
    float fmIndexScale_ = 1.0f;
    float fmRatioScale_ = 1.0f;

    VoiceState voices_[MAX_VOICES] = {};
    float resoPool_[MAX_VOICES * kResonatorBufSamples] = {};
};
