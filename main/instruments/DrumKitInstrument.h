#pragma once

#include <cstdint>
#include <variant>

#include "IInstrument.h"
#include "Patch.h"
#include "synth/modules/DrumVoices.h"

class DrumKitInstrument : public IInstrument {
public:
    void setPatch(const synth::patch::Patch& patch) noexcept;

    const synth::patch::Patch& patch() const noexcept { return patch_; }

    const char* name() const noexcept override { return patch_.name; }
    uint8_t defaultMaxVoices() const noexcept override { return patch_.maxVoices; }
    synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override {
        return patch_.sameNoteMode;
    }

    void onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(uint8_t v) noexcept override { (void)v; }
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

    void rebuildNoteTable_() noexcept;

    synth::patch::Patch patch_{};
    PadResolve notePad_[128]{};
    Voice voices_[MAX_VOICES]{};
};
