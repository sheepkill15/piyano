#pragma once

#include <cstdint>

#include "synth/voice/VoiceAllocator.h"

namespace synth::patch {

constexpr uint8_t MAX_OSCS = 4;
constexpr uint8_t MAX_ENVS = 3;
constexpr uint8_t MAX_LFOS = 2;
constexpr uint8_t MAX_FILTERS = 2;
constexpr uint8_t MAX_VOICES_CAP = 8;

enum class OscShape : uint8_t { Sine, Saw, Triangle, Pulse };
enum class FilterMode : uint8_t { LowPass, BandPass, HighPass };
enum class LfoShape : uint8_t { Sine, Triangle, Square, SampleHold };
enum class NoiseShape : uint8_t { Off, White, Pinkish };
enum class PatchKind : uint8_t { Synth, DrumKit };

// One oscillator slot. Set level=0 to disable. fmFrom links it as a phase modulator
// target (DX-style 2-op stacking). modEnvIndex points at an env that scales fmIndex
// so you get DX bell-style decaying timbres. vibratoLfoIndex points at an LFO that
// modulates pitch by vibratoSemitones.
struct OscDef {
    OscShape shape = OscShape::Sine;
    float pitchRatio = 1.0f;     // multiplier of base note frequency (1=unison, 2=octave up)
    float pitchSemitones = 0.0f; // additive offset in semitones
    float pitchCents = 0.0f;     // additive offset in cents
    float pulseWidth = 0.5f;
    float level = 0.0f;          // mix level into the trunk (0 disables osc output)
    int8_t fmFrom = -1;          // index of another osc whose output modulates this phase
    float fmIndex = 0.0f;        // FM index in radians
    float fmFeedback = 0.0f;     // self phase feedback (radians)
    int8_t modEnvIndex = -1;     // env scales fmIndex (DX modulator envelope)
    float velToFmIndex = 0.0f;   // 0=velocity ignored, 1=full velocity scaling
    int8_t vibratoLfoIndex = -1; // pitch LFO source
    float vibratoSemitones = 0.0f;
    bool isFmModulator = false;  // if true, pitchRatio scales by patch.fmRatioScale and
                                 //         any osc that fmFrom=this multiplies fmIndex
                                 //         by patch.fmIndexScale
};

// ADSR. Used as amp env (env 0) and any number of mod envelopes.
struct EnvDef {
    float attack = 0.005f;
    float decay = 0.10f;
    float sustain = 0.70f;
    float release = 0.10f;
};

struct LfoDef {
    LfoShape shape = LfoShape::Sine;
    float rateHz = 5.0f;
    float perVoiceRateSpread = 0.0f;  // adds (vi & 1) * spread to rate per voice
    float perVoicePhase = 0.0f;       // adds vi * perVoicePhase to start phase
};

// State-variable filter slot in the serial filter chain. cutoffPeakHz is reached
// when envIndex's env is at 1.0; cutoffHz is the value at env=0.
struct FilterDef {
    FilterMode mode = FilterMode::LowPass;
    float cutoffHz = 1000.0f;
    float cutoffPeakHz = 1000.0f;
    float resonance = 0.20f;
    int8_t envIndex = -1;        // env that sweeps cutoff base->peak
    float velToCutoffHz = 0.0f;  // velocity scales cutoff peak by this many Hz
    int8_t lfoIndex = -1;        // LFO modulates cutoff (centered around current)
    float lfoCutoffHz = 0.0f;    // amplitude of LFO modulation in Hz
};

struct NoiseDef {
    NoiseShape shape = NoiseShape::Off;
    float level = 0.0f;
    float color = 0.05f;         // for pinkish: 0..1 lowpass coefficient
};

struct DriveDef {
    float preGain = 1.0f;
    float postGain = 1.0f;
    bool enabled = false;
};

// Karplus-strong-ish body resonator used by piano-style patches. Allocates a
// per-voice circular buffer of kResonatorBufSamples on demand.
struct ResonatorDef {
    bool enabled = false;
    float feedback = 0.998f;
    float dampLowKey = 0.30f;
    float dampHighKey = 0.75f;
    float bodyMix = 0.85f;       // delay-line vs harmonic mix
    float bodyHpHz = 45.0f;      // post-body high-pass
    float outputLpHzBase = 2800.0f;
    float outputLpHzVel = 5500.0f;
    float harm1 = 0.55f;
    float harm2 = 0.20f;
    float harm3 = 0.06f;
    float harmAmount = 0.15f;    // overall harmonic injection
};

// One-shot click + noise burst at note start (piano hammer transient).
struct ExciterDef {
    bool enabled = false;
    float clickLevelBase = 0.55f;
    float clickLevelVel = 0.65f;
    float clickDecay = 0.92f;
    float noiseLevelBase = 0.20f;
    float noiseLevelVel = 0.40f;
    float noiseDecay = 0.998f;
};

// Drum kit map: each pad triggers a synth::modules::Kick/Snare/Hat by note.
enum class DrumPadKind : uint8_t { Kick, Snare, Hat };

struct DrumPad {
    uint8_t note = 36;
    DrumPadKind kind = DrumPadKind::Kick;
    float pan = 0.0f;
    float velScale = 1.0f;
};

constexpr uint8_t MAX_DRUM_PADS = 8;

struct DrumKitDef {
    DrumPad pads[MAX_DRUM_PADS] = {};
    uint8_t padCount = 0;
    DrumPadKind fallback = DrumPadKind::Hat;
    float fallbackPan = 0.20f;
    float fallbackVelScale = 0.6f;
};

// Top-level patch description. Everything a preset needs to define an instrument.
struct Patch {
    const char* name = "Init";
    PatchKind kind = PatchKind::Synth;

    OscDef oscs[MAX_OSCS] = {};
    uint8_t oscCount = 0;

    EnvDef envs[MAX_ENVS] = {};   // envs[0] is amp env; engine drives it
    uint8_t envCount = 1;

    LfoDef lfos[MAX_LFOS] = {};
    uint8_t lfoCount = 0;

    FilterDef filters[MAX_FILTERS] = {};
    uint8_t filterCount = 0;       // serial chain in order

    NoiseDef noise = {};
    DriveDef drive = {};
    ResonatorDef resonator = {};
    ExciterDef exciter = {};
    DrumKitDef drumKit = {};

    float outputGain = 1.0f;
    float panSpread = 0.0f;        // -spread..+spread alternating per voice index

    uint8_t maxVoices = 8;
    synth::voice::SameNoteMode sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;

    // FM helpers exposed so MIDI CCs can globally scale FM character without
    // editing the patch. The synth path multiplies oscs[*].fmIndex by fmIndexScale
    // and oscs[*].pitchSemitones for modulators by fmRatioScale.
    float fmIndexScale = 1.0f;
    float fmRatioScale = 1.0f;
};

} // namespace synth::patch
