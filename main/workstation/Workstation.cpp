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

    // Preset 0: Subtractive - Pluck
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Sub Pluck";
        p.instrumentIndex = 0;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.45f };
        p.params[1] = { Params::Attack, 0.002f };
        p.params[2] = { Params::Decay, 0.16f };
        p.params[3] = { Params::Sustain, 0.25f };
        p.params[4] = { Params::Release, 0.10f };
    }

    // Preset 1: Subtractive - Pad
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Sub Pad";
        p.instrumentIndex = 0;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.35f };
        p.params[2] = { Params::Decay, 0.55f };
        p.params[3] = { Params::Sustain, 0.85f };
        p.params[4] = { Params::Release, 0.90f };
    }

    // Preset 2: FM - Bell
    {
        Preset& p = presets_[presetCount_++];
        p.name = "FM Bell";
        p.instrumentIndex = 1;
        p.paramCount = 7;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.001f };
        p.params[2] = { Params::Decay, 0.65f };
        p.params[3] = { Params::Sustain, 0.00f };
        p.params[4] = { Params::Release, 1.20f };
        p.params[5] = { Params::FmModIndex, 0.62f };
        p.params[6] = { Params::FmModRatio, 0.72f };
    }

    // Preset 3: FM - Brass
    {
        Preset& p = presets_[presetCount_++];
        p.name = "FM Brass";
        p.instrumentIndex = 1;
        p.paramCount = 7;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.030f };
        p.params[2] = { Params::Decay, 0.18f };
        p.params[3] = { Params::Sustain, 0.75f };
        p.params[4] = { Params::Release, 0.20f };
        p.params[5] = { Params::FmModIndex, 0.48f };
        p.params[6] = { Params::FmModRatio, 0.50f };
    }

    // Preset 4: Piano - Natural
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Piano Natural";
        p.instrumentIndex = 2;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.45f };
        p.params[1] = { Params::Attack, 0.001f };
        p.params[2] = { Params::Decay, 0.70f };
        p.params[3] = { Params::Sustain, 0.00f };
        p.params[4] = { Params::Release, 0.25f };
    }

    // Preset 5: Piano - Soft
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Piano Soft";
        p.instrumentIndex = 2;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.006f };
        p.params[2] = { Params::Decay, 0.85f };
        p.params[3] = { Params::Sustain, 0.00f };
        p.params[4] = { Params::Release, 0.35f };
    }

    // Preset 6: EPiano - Tines
    {
        Preset& p = presets_[presetCount_++];
        p.name = "EP Tines";
        p.instrumentIndex = 3;
        p.paramCount = 7;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.004f };
        p.params[2] = { Params::Decay, 0.28f };
        p.params[3] = { Params::Sustain, 0.55f };
        p.params[4] = { Params::Release, 0.32f };
        p.params[5] = { Params::FmModIndex, 0.48f };
        p.params[6] = { Params::FmModRatio, 0.56f };
    }

    // Preset 7: EPiano - Dyn Keys
    {
        Preset& p = presets_[presetCount_++];
        p.name = "EP DynKeys";
        p.instrumentIndex = 3;
        p.paramCount = 7;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.002f };
        p.params[2] = { Params::Decay, 0.22f };
        p.params[3] = { Params::Sustain, 0.45f };
        p.params[4] = { Params::Release, 0.25f };
        p.params[5] = { Params::FmModIndex, 0.42f };
        p.params[6] = { Params::FmModRatio, 0.52f };
    }

    // Preset 8: Strings - Slow
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Strings Slow";
        p.instrumentIndex = 4;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.40f };
        p.params[1] = { Params::Attack, 0.55f };
        p.params[2] = { Params::Decay, 0.65f };
        p.params[3] = { Params::Sustain, 0.90f };
        p.params[4] = { Params::Release, 1.10f };
    }

    // Preset 9: Strings - Swell
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Strings Swell";
        p.instrumentIndex = 4;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.36f };
        p.params[1] = { Params::Attack, 0.90f };
        p.params[2] = { Params::Decay, 0.75f };
        p.params[3] = { Params::Sustain, 0.95f };
        p.params[4] = { Params::Release, 1.40f };
    }

    // Preset 10: Bass - Pluck
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Bass Pluck";
        p.instrumentIndex = 5;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.45f };
        p.params[1] = { Params::Attack, 0.001f };
        p.params[2] = { Params::Decay, 0.14f };
        p.params[3] = { Params::Sustain, 0.22f };
        p.params[4] = { Params::Release, 0.08f };
    }

    // Preset 11: Bass - Legato
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Bass Legato";
        p.instrumentIndex = 5;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 0.45f };
        p.params[1] = { Params::Attack, 0.010f };
        p.params[2] = { Params::Decay, 0.12f };
        p.params[3] = { Params::Sustain, 0.75f };
        p.params[4] = { Params::Release, 0.12f };
    }

    // Preset 12: Drums - Tight
    // Drum voices have their own internal envelopes; we keep the host env flat so
    // it doesn't cut the tail. Long release so the kick can ring out after key release.
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Drums Tight";
        p.instrumentIndex = 6;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 1.10f };
        p.params[1] = { Params::Attack, 0.0005f };
        p.params[2] = { Params::Decay, 0.0f };
        p.params[3] = { Params::Sustain, 1.0f };
        p.params[4] = { Params::Release, 1.5f };
    }

    // Preset 13: Drums - Roomy
    {
        Preset& p = presets_[presetCount_++];
        p.name = "Drums Roomy";
        p.instrumentIndex = 6;
        p.paramCount = 5;
        p.params[0] = { Params::MasterGain, 1.10f };
        p.params[1] = { Params::Attack, 0.0005f };
        p.params[2] = { Params::Decay, 0.0f };
        p.params[3] = { Params::Sustain, 1.0f };
        p.params[4] = { Params::Release, 2.5f };
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

