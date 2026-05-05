#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

#include "synth/Constants.h"
#include "engine/SynthEngine.h"
#include "engine/Sound.h"
#include "instruments/InstrumentManager.h"
#include "instruments/Patch.h"

class Workstation {
public:
    Workstation() noexcept = default;

    void begin() noexcept;

    void noteOn(uint8_t channel, uint8_t note, float vel) noexcept;
    void noteOff(uint8_t channel, uint8_t note) noexcept;
    void update(const float dt) noexcept { synth_.update(dt); }

    // Renders interleaved stereo (L,R,...) and writes to I2S.
    void renderAndWrite() noexcept;

    void handleControlChange(uint8_t channel, uint8_t control, uint8_t value) noexcept;
    void handleProgramChange(uint8_t channel, uint8_t program) noexcept;
    void handlePitchBend(uint8_t channel, uint16_t bend14) noexcept;

    [[nodiscard]] uint8_t currentPresetIndex() const noexcept { return currentPreset_; }
    [[nodiscard]] uint8_t presetCount() const noexcept { return presetCount_; }
    [[nodiscard]] const char* currentPresetName() const noexcept {
        return presetCount_ == 0 ? "" : presets_[currentPreset_].name;
    }

private:
    using PatchFactory = synth::patch::Patch (*)();

    void registerPreset_(PatchFactory factory) noexcept;
    void loadPreset_(uint8_t presetIndex) noexcept;
    void nextPreset_() noexcept;
    void applyAmpEnv_(const synth::patch::Patch& patch) noexcept;
    void applyMaster_() noexcept;

    SynthEngine synth_{};
    InstrumentManager instruments_{};
    Sound sound_{};

    static constexpr uint8_t MAX_PRESETS = synth::cfg::kWorkstationMaxPresets;
    std::array<synth::patch::Patch, MAX_PRESETS> presets_{};
    uint8_t presetCount_ = 0;
    uint8_t currentPreset_ = 0;

    float masterGain_ = 0.75f;
    float expression_ = 1.0f;
    float pitchBendNorm_ = 0.0f; // -1..+1

    bool sustainDown_ = false;
    std::array<bool, synth::cfg::kMidiNoteCount> keyDown_{};
    std::array<bool, synth::cfg::kMidiNoteCount> sustained_{};
};
