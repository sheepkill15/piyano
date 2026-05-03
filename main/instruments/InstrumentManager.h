#pragma once

#include "DrumKitInstrument.h"
#include "IInstrument.h"
#include "ModularInstrument.h"
#include "Patch.h"

// Owns modular synth and drum-kit instruments. loadPatch() routes PatchKind::DrumKit
// to DrumKitInstrument and everything else to ModularInstrument.
class InstrumentManager {
public:
    InstrumentManager() noexcept = default;

    IInstrument* loadPatch(const synth::patch::Patch& patch) noexcept {
        if (patch.kind == synth::patch::PatchKind::DrumKit) {
            drums_.setPatch(patch);
            active_ = &drums_;
        } else {
            modular_.setPatch(patch);
            active_ = &modular_;
        }
        return active_;
    }

    IInstrument* current() noexcept { return active_; }

    const synth::patch::Patch& currentPatch() const noexcept {
        return active_ == &drums_ ? drums_.patch() : modular_.patch();
    }

private:
    ModularInstrument modular_{};
    DrumKitInstrument drums_{};
    IInstrument* active_{&modular_};
};
