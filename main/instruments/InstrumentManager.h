#pragma once

#include <cstdint>

#include "DrumKitInstrument.h"
#include "IInstrument.h"
#include "ModularInstrument.h"
#include "Patch.h"

// Owns modular synth and drum-kit instruments. Hot paths use enum dispatch
// (no vtable) while types still implement IInstrument for clarity. The Patch
// passed to loadPatch() is consumed at load time only and is not retained.
class InstrumentManager {
public:
    enum class ActiveKind : uint8_t { Modular, Drums };

    InstrumentManager() noexcept = default;

    void loadPatch(const synth::patch::Patch& patch) noexcept {
        if (patch.kind == synth::patch::PatchKind::DrumKit) {
            drums_.setPatch(patch);
            activeKind_ = ActiveKind::Drums;
        } else {
            modular_.setPatch(patch);
            activeKind_ = ActiveKind::Modular;
        }
    }

    ActiveKind activeKind() const noexcept { return activeKind_; }

    IInstrument* current() noexcept {
        return activeKind_ == ActiveKind::Drums
                   ? static_cast<IInstrument*>(&drums_)
                   : static_cast<IInstrument*>(&modular_);
    }

    uint8_t defaultMaxVoices() const noexcept {
        return activeKind_ == ActiveKind::Drums ? drums_.defaultMaxVoices()
                                               : modular_.defaultMaxVoices();
    }

    synth::voice::SameNoteMode defaultSameNoteMode() const noexcept {
        return activeKind_ == ActiveKind::Drums ? drums_.defaultSameNoteMode()
                                               : modular_.defaultSameNoteMode();
    }

    void onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept {
        if (activeKind_ == ActiveKind::Drums) {
            drums_.onVoiceStart(v, ctx);
        } else {
            modular_.onVoiceStart(v, ctx);
        }
    }

    void onVoiceStop(uint8_t v) noexcept {
        if (activeKind_ == ActiveKind::Drums) {
            drums_.onVoiceStop(v);
        } else {
            modular_.onVoiceStop(v);
        }
    }

    void renderAddVoice(uint8_t v, float* outMono, uint64_t numSamples) noexcept {
        if (activeKind_ == ActiveKind::Drums) {
            drums_.renderAddVoice(v, outMono, numSamples);
        } else {
            modular_.renderAddVoice(v, outMono, numSamples);
        }
    }

    float voicePan(uint8_t v) const noexcept {
        return activeKind_ == ActiveKind::Drums ? drums_.voicePan(v) : modular_.voicePan(v);
    }

    bool setParam(uint16_t paramId, float value) noexcept {
        return activeKind_ == ActiveKind::Drums ? drums_.setParam(paramId, value)
                                               : modular_.setParam(paramId, value);
    }

private:
    ModularInstrument modular_{};
    DrumKitInstrument drums_{};
    ActiveKind activeKind_{ActiveKind::Modular};
};
