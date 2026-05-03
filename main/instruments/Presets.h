#pragma once

#include "Patch.h"

// Each function returns a fully described Patch. To add a new preset, write a
// new factory here that picks any combination of oscillators/envelopes/LFOs/
// filters/noise/drive/resonator/exciter/drum-kit modules, wire them together,
// and register the factory in Workstation::begin().
namespace presets {

// =========================================================================
// SUBTRACTIVE family: dual saw + sub sine -> LP filter swept by env -> drive.
// =========================================================================
inline synth::patch::Patch subPluck() noexcept {
    using namespace synth::patch;
    Patch p;
    p.name = "Sub Pluck";
    p.kind = PatchKind::Synth;

    p.oscs[0].shape = OscShape::Saw;
    p.oscs[0].level = 0.35f;

    p.oscs[1].shape = OscShape::Saw;
    p.oscs[1].pitchCents = 7.0f;
    p.oscs[1].level = 0.35f;

    p.oscs[2].shape = OscShape::Sine;
    p.oscs[2].pitchSemitones = -12.0f;
    p.oscs[2].level = 0.25f;

    p.oscCount = 3;

    p.envs[0] = { 0.002f, 0.16f, 0.25f, 0.10f };
    p.envs[1] = { 0.005f, 0.35f, 0.25f, 0.20f };
    p.envCount = 2;

    p.filters[0].mode = FilterMode::LowPass;
    p.filters[0].cutoffHz = 800.0f;
    p.filters[0].cutoffPeakHz = 5500.0f;
    p.filters[0].resonance = 0.35f;
    p.filters[0].envIndex = 1;
    p.filterCount = 1;

    p.drive.enabled = true;
    p.drive.preGain = 1.1f;
    p.drive.postGain = 1.0f;

    p.outputGain = 0.7f;
    return p;
}

inline synth::patch::Patch subPad() noexcept {
    auto p = subPluck();
    p.name = "Sub Pad";
    p.envs[0] = { 0.35f, 0.55f, 0.85f, 0.90f };
    return p;
}

// =========================================================================
// FM family: 2-op sine FM. osc[0] = modulator (rendered first, silent on its
// own), osc[1] = carrier (fmFrom=0). Modulator carries env[1] which scales
// the carrier's fmIndex. Self-feedback lives on the modulator.
// =========================================================================
inline synth::patch::Patch makeFm(const char* name,
                                  synth::patch::EnvDef ampEnv,
                                  synth::patch::EnvDef modEnv,
                                  float fmIndexRad,
                                  float modPitchRatio,
                                  float velToFmIndex,
                                  float feedbackRad) noexcept {
    using namespace synth::patch;
    Patch p;
    p.name = name;
    p.kind = PatchKind::Synth;

    // Modulator first (so the carrier reads a same-sample value).
    p.oscs[0].shape = OscShape::Sine;
    p.oscs[0].pitchRatio = modPitchRatio;
    p.oscs[0].fmFeedback = feedbackRad;
    p.oscs[0].isFmModulator = true;
    p.oscs[0].level = 0.0f;

    // Carrier
    p.oscs[1].shape = OscShape::Sine;
    p.oscs[1].level = 0.85f;
    p.oscs[1].fmFrom = 0;
    p.oscs[1].fmIndex = fmIndexRad;
    p.oscs[1].modEnvIndex = 1;
    p.oscs[1].velToFmIndex = velToFmIndex;

    p.oscCount = 2;

    p.envs[0] = ampEnv;
    p.envs[1] = modEnv;
    p.envCount = 2;
    return p;
}

inline synth::patch::Patch fmBell() noexcept {
    return makeFm("FM Bell",
                  /*amp*/ { 0.001f, 0.65f, 0.00f, 1.20f },
                  /*mod*/ { 0.001f, 0.40f, 0.65f, 0.30f },
                  /*idx*/ 4.96f,
                  /*ratio*/ 5.90f,
                  /*velIdx*/ 0.6f,
                  /*fb*/ 0.20f);
}

inline synth::patch::Patch fmBrass() noexcept {
    return makeFm("FM Brass",
                  /*amp*/ { 0.030f, 0.18f, 0.75f, 0.20f },
                  /*mod*/ { 0.030f, 0.30f, 0.65f, 0.30f },
                  /*idx*/ 3.84f,
                  /*ratio*/ 4.25f,
                  /*velIdx*/ 0.6f,
                  /*fb*/ 0.20f);
}

// EPiano is also FM but with DX-tine character (fast modulator decay,
// stronger velocity-to-index, low feedback).
inline synth::patch::Patch epTines() noexcept {
    return makeFm("EP Tines",
                  /*amp*/ { 0.004f, 0.28f, 0.55f, 0.32f },
                  /*mod*/ { 0.001f, 0.18f, 0.10f, 0.25f },
                  /*idx*/ 3.84f,
                  /*ratio*/ 4.70f,
                  /*velIdx*/ 1.0f,
                  /*fb*/ 0.10f);
}

inline synth::patch::Patch epDynKeys() noexcept {
    return makeFm("EP DynKeys",
                  /*amp*/ { 0.002f, 0.22f, 0.45f, 0.25f },
                  /*mod*/ { 0.001f, 0.18f, 0.10f, 0.25f },
                  /*idx*/ 3.36f,
                  /*ratio*/ 4.40f,
                  /*velIdx*/ 1.0f,
                  /*fb*/ 0.10f);
}

// =========================================================================
// PIANO family: click+noise exciter -> Karplus-Strong body -> harmonics on top.
// =========================================================================
inline synth::patch::Patch pianoNatural() noexcept {
    using namespace synth::patch;
    Patch p;
    p.name = "Piano Natural";
    p.kind = PatchKind::Synth;

    p.oscs[0].shape = OscShape::Sine;
    p.oscs[0].level = 0.55f;
    p.oscs[1].shape = OscShape::Sine;
    p.oscs[1].pitchSemitones = 12.0f;
    p.oscs[1].level = 0.20f;
    p.oscs[2].shape = OscShape::Sine;
    p.oscs[2].pitchSemitones = 19.0f; // ~3rd harmonic
    p.oscs[2].level = 0.06f;
    p.oscCount = 3;

    p.envs[0] = { 0.001f, 0.70f, 0.00f, 0.25f };
    p.envCount = 1;

    p.exciter.enabled = true;
    p.exciter.clickLevelBase = 0.55f;
    p.exciter.clickLevelVel = 0.65f;
    p.exciter.clickDecay = 0.92f;
    p.exciter.noiseLevelBase = 0.20f;
    p.exciter.noiseLevelVel = 0.40f;
    p.exciter.noiseDecay = 0.998f;

    p.resonator.enabled = true;
    p.resonator.feedback = 0.9985f;
    p.resonator.dampLowKey = 0.30f;
    p.resonator.dampHighKey = 0.75f;
    p.resonator.bodyMix = 0.85f;
    p.resonator.bodyHpHz = 45.0f;
    p.resonator.outputLpHzBase = 2800.0f;
    p.resonator.outputLpHzVel = 5500.0f;
    p.resonator.harmAmount = 0.15f;

    p.panSpread = 0.18f;
    p.outputGain = 0.72f;
    p.maxVoices = 8;
    p.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    return p;
}

inline synth::patch::Patch pianoSoft() noexcept {
    auto p = pianoNatural();
    p.name = "Piano Soft";
    p.envs[0] = { 0.006f, 0.85f, 0.00f, 0.35f };
    return p;
}

// =========================================================================
// STRINGS: dual saw with vibrato LFO + slow filter LFO + ensemble pan spread.
// =========================================================================
inline synth::patch::Patch stringsSlow() noexcept {
    using namespace synth::patch;
    Patch p;
    p.name = "Strings Slow";
    p.kind = PatchKind::Synth;

    p.oscs[0].shape = OscShape::Saw;
    p.oscs[0].level = 0.40f;
    p.oscs[0].vibratoLfoIndex = 0;
    p.oscs[0].vibratoSemitones = 0.042f;

    p.oscs[1].shape = OscShape::Saw;
    p.oscs[1].pitchCents = 10.0f;
    p.oscs[1].level = 0.40f;
    p.oscs[1].vibratoLfoIndex = 0;
    p.oscs[1].vibratoSemitones = 0.042f;

    p.oscCount = 2;

    p.envs[0] = { 0.55f, 0.65f, 0.90f, 1.10f };
    p.envCount = 1;

    p.lfos[0].shape = LfoShape::Sine;
    p.lfos[0].rateHz = 5.2f;
    p.lfos[0].perVoiceRateSpread = 0.15f;
    p.lfos[0].perVoicePhase = 0.137f;

    p.lfos[1].shape = LfoShape::Triangle;
    p.lfos[1].rateHz = 0.35f;
    p.lfos[1].perVoiceRateSpread = 0.07f;
    p.lfos[1].perVoicePhase = 0.211f;
    p.lfoCount = 2;

    p.filters[0].mode = FilterMode::LowPass;
    p.filters[0].cutoffHz = 1400.0f;
    p.filters[0].cutoffPeakHz = 1400.0f;
    p.filters[0].resonance = 0.20f;
    p.filters[0].velToCutoffHz = 2200.0f;
    p.filters[0].lfoIndex = 1;
    p.filters[0].lfoCutoffHz = 1100.0f;
    p.filterCount = 1;

    p.outputGain = 0.85f;
    p.panSpread = 0.6f;
    p.maxVoices = 8;
    p.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    return p;
}

inline synth::patch::Patch stringsSwell() noexcept {
    auto p = stringsSlow();
    p.name = "Strings Swell";
    p.envs[0] = { 0.90f, 0.75f, 0.95f, 1.40f };
    p.outputGain = 0.80f;
    return p;
}

// =========================================================================
// BASS: saw + sub sine -> LP with filter env -> drive.
// =========================================================================
inline synth::patch::Patch bassPluck() noexcept {
    using namespace synth::patch;
    Patch p;
    p.name = "Bass Pluck";
    p.kind = PatchKind::Synth;

    p.oscs[0].shape = OscShape::Saw;
    p.oscs[0].level = 0.65f;

    p.oscs[1].shape = OscShape::Sine;
    p.oscs[1].pitchSemitones = -12.0f;
    p.oscs[1].level = 0.32f;

    p.oscCount = 2;

    p.envs[0] = { 0.001f, 0.14f, 0.22f, 0.08f };
    p.envs[1] = { 0.001f, 0.18f, 0.30f, 0.10f };
    p.envCount = 2;

    p.filters[0].mode = FilterMode::LowPass;
    p.filters[0].cutoffHz = 120.0f;
    p.filters[0].cutoffPeakHz = 1920.0f;
    p.filters[0].resonance = 0.45f;
    p.filters[0].envIndex = 1;
    p.filters[0].velToCutoffHz = 600.0f;
    p.filterCount = 1;

    p.drive.enabled = true;
    p.drive.preGain = 1.6f;
    p.drive.postGain = 0.75f;

    p.maxVoices = 1;
    p.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
    return p;
}

inline synth::patch::Patch bassLegato() noexcept {
    auto p = bassPluck();
    p.name = "Bass Legato";
    p.envs[0] = { 0.010f, 0.12f, 0.75f, 0.12f };
    return p;
}

// =========================================================================
// DRUMS: drum-kit map of MIDI notes -> Kick/Snare/Hat modules.
// =========================================================================
inline synth::patch::Patch drumsTight() noexcept {
    using namespace synth::patch;
    Patch p;
    p.name = "Drums Tight";
    p.kind = PatchKind::DrumKit;

    p.envs[0] = { 0.0005f, 0.0f, 1.0f, 1.5f };
    p.envCount = 1;

    p.drumKit.pads[0] = { 36, DrumPadKind::Kick,  0.00f, 1.0f };
    p.drumKit.pads[1] = { 38, DrumPadKind::Snare, -0.15f, 1.0f };
    p.drumKit.pads[2] = { 40, DrumPadKind::Snare, -0.15f, 1.0f };
    p.drumKit.pads[3] = { 42, DrumPadKind::Hat,   0.30f, 1.0f };
    p.drumKit.pads[4] = { 44, DrumPadKind::Hat,   0.30f, 1.0f };
    p.drumKit.pads[5] = { 46, DrumPadKind::Hat,   0.30f, 1.0f };
    p.drumKit.padCount = 6;
    p.drumKit.fallback = DrumPadKind::Hat;
    p.drumKit.fallbackPan = 0.20f;
    p.drumKit.fallbackVelScale = 0.6f;

    p.outputGain = 1.10f;
    p.maxVoices = 8;
    p.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    return p;
}

inline synth::patch::Patch drumsRoomy() noexcept {
    auto p = drumsTight();
    p.name = "Drums Roomy";
    p.envs[0] = { 0.0005f, 0.0f, 1.0f, 2.5f };
    return p;
}

} // namespace presets
