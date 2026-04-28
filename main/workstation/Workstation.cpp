#include "workstation/Workstation.h"
#include "workstation/Params.h"

Workstation::Workstation(SynthEngine& synth, InstrumentManager& instruments, Sound& sound) noexcept
    : synth_(synth)
    , instruments_(instruments)
    , sound_(sound) {}

void Workstation::begin() noexcept {
    presetCount_ = 0;

    // Keep output stage at unity; use synth master gain as the main mix control.
    sound_.setAmplitude(1.0f);

    // Preset 0: Subtractive Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Sub Init";
        p.instrumentIndex = 0;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.25f };
        p.params[1] = { Params::Attack, 0.01f };
        p.params[2] = { Params::Decay, 0.10f };
        p.params[3] = { Params::Sustain, 0.70f };
        p.params[4] = { Params::Release, 0.20f };
    }

    // Preset 1: FM Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "FM Init";
        p.instrumentIndex = 1;
        p.paramCount = 7;
        p.params[0] = { Params::MasterGain, 0.25f };
        p.params[1] = { Params::Attack, 0.01f };
        p.params[2] = { Params::Decay, 0.10f };
        p.params[3] = { Params::Sustain, 0.70f };
        p.params[4] = { Params::Release, 0.20f };
        p.params[5] = { Params::FmModIndex, 0.50f };
        p.params[6] = { Params::FmModRatio, 0.50f };
    }

    // Preset 2: Piano Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Piano Init";
        p.instrumentIndex = 2;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.25f };
        p.params[1] = { Params::Attack, 0.001f };
        p.params[2] = { Params::Decay, 0.25f };
        p.params[3] = { Params::Sustain, 0.0f };
        p.params[4] = { Params::Release, 0.25f };
    }

    // Preset 3: EPiano Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "EPiano Init";
        p.instrumentIndex = 3;
        p.paramCount = 7;
        p.params[0] = { Params::MasterGain, 0.25f };
        p.params[1] = { Params::Attack, 0.005f };
        p.params[2] = { Params::Decay, 0.20f };
        p.params[3] = { Params::Sustain, 0.6f };
        p.params[4] = { Params::Release, 0.25f };
        p.params[5] = { Params::FmModIndex, 0.45f };
        p.params[6] = { Params::FmModRatio, 0.55f };
    }

    // Preset 4: Strings Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Strings Init";
        p.instrumentIndex = 4;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.22f };
        p.params[1] = { Params::Attack, 0.30f };
        p.params[2] = { Params::Decay, 0.40f };
        p.params[3] = { Params::Sustain, 0.8f };
        p.params[4] = { Params::Release, 0.60f };
    }

    // Preset 5: Bass Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Bass Init";
        p.instrumentIndex = 5;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.22f };
        p.params[1] = { Params::Attack, 0.005f };
        p.params[2] = { Params::Decay, 0.10f };
        p.params[3] = { Params::Sustain, 0.7f };
        p.params[4] = { Params::Release, 0.10f };
    }

    // Preset 6: Drums Init
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Drums Init";
        p.instrumentIndex = 6;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.25f };
        p.params[1] = { Params::Attack, 0.001f };
        p.params[2] = { Params::Decay, 0.10f };
        p.params[3] = { Params::Sustain, 0.0f };
        p.params[4] = { Params::Release, 0.10f };
    }

    loadPreset(0);
}

void Workstation::handleControlChange(uint8_t channel, uint8_t control, uint8_t value) noexcept {
    if (channel != 15) return;

    if (control == 20) {
        synth_.setParam(Params::MasterGain, value / 127.0f);
        return;
    }

    if (control == 112 && value == 127) {
        nextInstrument();
        return;
    }

    // ADSR (kept compatible with current mapping)
    if (control == 12) { setParamOnCurrent(Params::Attack, value / 127.0f); return; }
    if (control == 13) { setParamOnCurrent(Params::Decay, value / 127.0f); return; }
    if (control == 14) { setParamOnCurrent(Params::Sustain, value / 127.0f); return; }
    if (control == 15) { setParamOnCurrent(Params::Release, value / 127.0f); return; }

    // FM quick controls (example mapping)
    if (control == 16) { setParamOnCurrent(Params::FmModIndex, value / 127.0f); return; }
    if (control == 17) { setParamOnCurrent(Params::FmModRatio, value / 127.0f); return; }
}

void Workstation::handleProgramChange(uint8_t channel, uint8_t program) noexcept {
    (void)channel;
    if (presetCount_ == 0) return;
    loadPreset(static_cast<uint8_t>(program % presetCount_));
}

void Workstation::nextInstrument() noexcept {
    const uint8_t count = instruments_.getInstrumentCount();
    currentInstrument_ = static_cast<uint8_t>((currentInstrument_ + 1) % count);

    IInstrument* inst = instruments_.select(currentInstrument_);
    if (inst) {
        synth_.switchInstrument(inst);
    }
}

void Workstation::loadPreset(uint8_t presetIndex) noexcept {
    if (presetCount_ == 0) return;
    currentPreset_ = static_cast<uint8_t>(presetIndex % presetCount_);

    const Preset& preset = presets_[currentPreset_];
    currentInstrument_ = preset.instrumentIndex;

    IInstrument* inst = instruments_.select(currentInstrument_);
    if (!inst) return;

    synth_.switchInstrument(inst);
    applyPresetToInstrument(preset, inst);
}

void Workstation::applyPresetToInstrument(const Preset& preset, IInstrument* instrument) noexcept {
    if (!instrument) return;
    for (uint8_t i = 0; i < preset.paramCount; ++i) {
        synth_.setParam(preset.params[i].id, preset.params[i].value);
    }
}

void Workstation::setParamOnCurrent(uint16_t paramId, float normalized) noexcept {
    (void)instruments_;
    synth_.setParam(paramId, normalized);
}

