#include "workstation/Workstation.h"
#include "workstation/Params.h"

#include "engine/AudioContext.h"
#include "instruments/Presets.h"
#include "synth/dsp/WaveTables.h"

namespace {
inline float norm7(const uint8_t v) noexcept { return static_cast<float>(v) * (1.0f / 127.0f); }
}

void Workstation::renderAndWrite() noexcept {
    auto& stereoLR = sound_.stereoBlock();
    synth_.render(stereoLR);
    sound_.writeStereoInterleaved(stereoLR);
}

void Workstation::registerPreset_(const PatchFactory factory) noexcept {
    if (!factory) return;
    if (presetCount_ >= MAX_PRESETS) return;
    presets_[presetCount_++] = factory();
}

void Workstation::begin() noexcept {
    presetCount_ = 0;

    sound_.begin();
    synth_.init(&instruments_);

    // Output stage at unity; synth master gain is the main mix control.
    sound_.setAmplitude(1.0f);

    // The preset list. Each entry is a fully described Patch built from
    // modules. To add a new preset, write a new factory in Presets.h and
    // register it here.
    registerPreset_(presets::subPluck);
    registerPreset_(presets::subPad);
    registerPreset_(presets::fmBell);
    registerPreset_(presets::fmBrass);
    registerPreset_(presets::pianoNatural);
    registerPreset_(presets::pianoSoft);
    registerPreset_(presets::epTines);
    registerPreset_(presets::epDynKeys);
    registerPreset_(presets::stringsSlow);
    registerPreset_(presets::stringsSwell);
    registerPreset_(presets::bassPluck);
    registerPreset_(presets::bassLegato);
    registerPreset_(presets::drumsTight);
    registerPreset_(presets::drumsRoomy);

    if (presetCount_ > 0) loadPreset_(0);

    masterGain_ = 0.75f;
    expression_ = 1.0f;
    sustainDown_ = false;
    for (auto& k : keyDown_) k = false;
    for (auto& s : sustained_) s = false;
    applyMaster_();
}

void Workstation::applyMaster_() noexcept {
    const float g = masterGain_ * expression_;
    synth_.setParam(Params::MasterGain, g);
}

void Workstation::handlePitchBend(const uint8_t /*channel*/, const uint16_t bend14) noexcept {
    // MIDI pitch bend: 14-bit value 0..16383, center=8192.
    // We store normalized -1..+1 and let the synth decide range.
    constexpr float invCenter = 1.0f / 8192.0f;
    float n = (static_cast<int>(bend14) - 8192) * invCenter; // ~[-1, +1)
    if (n < -1.0f) n = -1.0f;
    if (n >  1.0f) n =  1.0f;
    pitchBendNorm_ = n;
    synth_.setParam(Params::PitchBend, n);
}

void Workstation::noteOn(const uint8_t /*channel*/, const uint8_t note, const float vel) noexcept {
    keyDown_[note & 127] = true;
    sustained_[note & 127] = false;
    synth_.noteOn(note, vel);
}

void Workstation::noteOff(const uint8_t /*channel*/, const uint8_t note) noexcept {
    keyDown_[note & 127] = false;
    if (sustainDown_) {
        sustained_[note & 127] = true;
        return;
    }
    sustained_[note & 127] = false;
    synth_.noteOff(note);
}

void Workstation::handleControlChange(const uint8_t channel, const uint8_t control, const uint8_t value) noexcept {
    (void)channel;

    // Sustain pedal
    if (control == 64) {
        const bool down = value >= 64;
        if (down == sustainDown_) return;
        sustainDown_ = down;
        if (!sustainDown_) {
            for (uint8_t n = 0; n < synth::cfg::kMidiNoteCount; ++n) {
                if (sustained_[n] && !keyDown_[n]) {
                    sustained_[n] = false;
                    synth_.noteOff(n);
                }
            }
        }
        return;
    }

    // Presets
    if (control == 112 && value == 127) { nextPreset_(); return; }

    // Standard CCs
    if (control == 7)  { masterGain_ = norm7(value); applyMaster_(); return; }    // Volume
    if (control == 11) { expression_ = norm7(value); applyMaster_(); return; }   // Expression
    if (control == 1)  { synth_.setParam(Params::Vibrato, norm7(value)); return; } // Mod wheel
    if (control == 74) { synth_.setParam(Params::FilterCutoff, norm7(value)); return; } // Brightness
    if (control == 71) { synth_.setParam(Params::FilterResonance, norm7(value)); return; } // Resonance
    if (control == 76) { synth_.setParam(Params::FilterLfoAmt, norm7(value)); return; } // FX 1 depth -> filter LFO amt

    if (control == 73) { synth_.setParam(Params::Attack,  norm7(value)); return; } // Attack
    if (control == 75) { synth_.setParam(Params::Decay,   norm7(value)); return; } // Decay
    if (control == 72) { synth_.setParam(Params::Sustain, norm7(value)); return; } // Sustain

    // Nektar Impact LX61+ factory reset can land on different presets.
    // Accept common aliases so controls still "do the expected thing".
    //
    // Faders (Preset 1 "GM Instrument"): 73,75,72,91,92,93,94,95,7
    // Some presets use 91 for Release; support that alias too.
    if (control == 91) { synth_.setParam(Params::Release, norm7(value)); return; }
    if (control == 92) { synth_.setParam(Params::NoiseLevel,  norm7(value)); return; } // FX depth 2
    if (control == 93) { synth_.setParam(Params::Drive,       norm7(value)); return; } // FX depth 3
    if (control == 94) { synth_.setParam(Params::NoiseColor,  norm7(value)); return; } // FX depth 4
    if (control == 95) { synth_.setParam(Params::FmModIndex,  norm7(value)); return; } // FX depth 5

    // Pots: 74,71,5,84,78,76,77,10
    if (control == 77) { synth_.setParam(Params::Vibrato, norm7(value)); return; } // vibrato depth
    if (control == 78) { synth_.setParam(Params::FmModRatio, norm7(value)); return; } // vibrato delay -> fm ratio macro
    if (control == 5)  { synth_.setParam(Params::NoiseColor, norm7(value)); return; } // portamento rate -> noise color
    if (control == 84) { synth_.setParam(Params::Drive, norm7(value)); return; } // portamento depth -> drive
    if (control == 10) { return; } // pan (engine has per-voice pan; ignore for now)

    // Buttons (toggle): 0,2,3,4,6,8,9,11,65
    if (control == 65) { return; } // portamento on/off (ignore)
    if (control == 0 && value == 127) { nextPreset_(); return; } // bank msb -> use as "next preset"

    // Older custom mapping (keep as extra)
    if (control == 20) { masterGain_ = norm7(value); applyMaster_(); return; }
    if (control == 12) { synth_.setParam(Params::Attack,  norm7(value)); return; }
    if (control == 13) { synth_.setParam(Params::Decay,   norm7(value)); return; }
    if (control == 14) { synth_.setParam(Params::Sustain, norm7(value)); return; }
    if (control == 15) { synth_.setParam(Params::Release, norm7(value)); return; }
    if (control == 16) { synth_.setParam(Params::FmModIndex, norm7(value)); return; }
    if (control == 17) { synth_.setParam(Params::FmModRatio, norm7(value)); return; }
}

void Workstation::handleProgramChange(uint8_t /*channel*/, const uint8_t program) noexcept {
    if (presetCount_ == 0) return;
    loadPreset_(static_cast<uint8_t>(program % presetCount_));
}

void Workstation::nextPreset_() noexcept {
    if (presetCount_ == 0) return;
    loadPreset_(static_cast<uint8_t>((currentPreset_ + 1) % presetCount_));
}

void Workstation::loadPreset_(const uint8_t presetIndex) noexcept {
    if (presetCount_ == 0) return;
    currentPreset_ = static_cast<uint8_t>(presetIndex % presetCount_);

    const synth::patch::Patch& patch = presets_[currentPreset_];

    instruments_.loadPatch(patch);
    synth_.bindInstruments(&instruments_);
    applyAmpEnv_(patch);
}

void Workstation::applyAmpEnv_(const synth::patch::Patch& patch) noexcept {
    if (patch.envCount == 0) return;
    const auto& e = patch.envs[0];
    synth_.setParam(Params::Attack,  e.attack);
    synth_.setParam(Params::Decay,   e.decay);
    synth_.setParam(Params::Sustain, e.sustain);
    synth_.setParam(Params::Release, e.release);
}
