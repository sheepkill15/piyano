#include "workstation/Workstation.h"
#include "workstation/Params.h"

#include "instruments/Presets.h"

Workstation::Workstation(SynthEngine& synth, InstrumentManager& instruments, Sound& sound) noexcept
    : synth_(synth)
    , instruments_(instruments)
    , sound_(sound) {}

void Workstation::registerPreset_(const PatchFactory factory) noexcept {
    if (!factory) return;
    if (presetCount_ >= MAX_PRESETS) return;
    presets_[presetCount_++] = factory();
}

void Workstation::begin() noexcept {
    presetCount_ = 0;

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
}

void Workstation::handleControlChange(const uint8_t channel, const uint8_t control, const uint8_t value) noexcept {
    if (channel != 15) return;

    if (control == 20) {
        synth_.setParam(Params::MasterGain, static_cast<float>(value) / 127.0f);
        return;
    }

    if (control == 112 && value == 127) {
        nextPreset_();
        return;
    }

    if (control == 12) { synth_.setParam(Params::Attack,  static_cast<float>(value) / 127.0f); return; }
    if (control == 13) { synth_.setParam(Params::Decay,   static_cast<float>(value) / 127.0f); return; }
    if (control == 14) { synth_.setParam(Params::Sustain, static_cast<float>(value) / 127.0f); return; }
    if (control == 15) { synth_.setParam(Params::Release, static_cast<float>(value) / 127.0f); return; }

    if (control == 16) { synth_.setParam(Params::FmModIndex, static_cast<float>(value) / 127.0f); return; }
    if (control == 17) { synth_.setParam(Params::FmModRatio, static_cast<float>(value) / 127.0f); return; }
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

void Workstation::applyAmpEnv_(const synth::patch::Patch& patch) const noexcept {
    if (patch.envCount == 0) return;
    const auto& e = patch.envs[0];
    synth_.setParam(Params::Attack,  e.attack);
    synth_.setParam(Params::Decay,   e.decay);
    synth_.setParam(Params::Sustain, e.sustain);
    synth_.setParam(Params::Release, e.release);
}
