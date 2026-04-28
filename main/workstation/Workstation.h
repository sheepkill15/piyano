#pragma once

#include "engine/SynthEngine.h"
#include "engine/Sound.h"
#include "instruments/InstrumentManager.h"
#include "workstation/Preset.h"

class Workstation {
public:
    Workstation(SynthEngine& synth, InstrumentManager& instruments, Sound& sound) noexcept;

    void begin() noexcept;

    void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) noexcept;
    void handleProgramChange(uint8_t channel, uint8_t program) noexcept;

    uint8_t currentInstrumentIndex() const noexcept { return currentInstrument_; }
    uint8_t currentPresetIndex() const noexcept { return currentPreset_; }

private:
    void nextInstrument() noexcept;
    void loadPreset(uint8_t presetIndex) noexcept;
    void applyPresetToInstrument(const Preset& preset, IInstrument* instrument) noexcept;

    void setParamOnCurrent(uint16_t paramId, float normalized) noexcept;

    SynthEngine& synth_;
    InstrumentManager& instruments_;
    Sound& sound_;

    uint8_t currentInstrument_ = 0;
    uint8_t currentPreset_ = 0;

    static constexpr uint8_t MAX_PRESETS = 16;
    Preset presets_[MAX_PRESETS] = {};
    uint8_t presetCount_ = 0;
};

