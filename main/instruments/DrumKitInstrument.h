#pragma once

#include <array>
#include <cstdint>
#include <variant>

#include "IInstrument.h"
#include "Patch.h"
#include "synth/Constants.h"
#include "synth/modules/DrumVoices.h"

class DrumKitInstrument : public IInstrument {
public:
    void setPatch(const synth::patch::Patch& patch) noexcept;

    [[nodiscard]] const char* name() const noexcept override { return name_; }
    [[nodiscard]] uint8_t defaultMaxVoices() const noexcept override { return maxVoices_; }
    [[nodiscard]] synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override {
        return sameNoteMode_;
    }

    void onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(const uint8_t v) noexcept override { (void)v; }
    void renderAddVoice(uint8_t v, float* outMono, uint64_t numSamples) noexcept override;
    float voicePan(uint8_t v) const noexcept override;

private:
    static constexpr uint8_t MAX_VOICES = synth::patch::MAX_VOICES_CAP;

    struct PadResolve {
        synth::patch::DrumPadKind kind = synth::patch::DrumPadKind::Hat;
        float pan = 0.0f;
        float velScale = 1.0f;
    };

    using DrumBody = std::variant<std::monostate,
                                  synth::modules::Kick,
                                  synth::modules::Snare,
                                  synth::modules::Hat>;

    struct Voice {
        float pan = 0.0f;
        DrumBody body{};
    };

    const char* name_ = "Init";
    uint8_t maxVoices_ = 8;
    synth::voice::SameNoteMode sameNoteMode_ =
        synth::voice::SameNoteMode::SingleVoicePerKey;

    std::array<PadResolve, synth::cfg::kMidiNoteCount> notePad_{};
    std::array<Voice, MAX_VOICES> voices_{};
};
