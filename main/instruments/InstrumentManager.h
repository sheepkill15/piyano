#pragma once

#include "IInstrument.h"
#include "ModularInstrument.h"
#include "Patch.h"

// Owns the single live ModularInstrument. The workstation calls loadPatch() to
// reconfigure it for the currently selected preset. There is no longer a fixed
// table of pre-defined "instruments": every preset describes its own modules.
class InstrumentManager {
public:
    InstrumentManager() noexcept = default;

    IInstrument* loadPatch(const synth::patch::Patch& patch) noexcept {
        instrument_.setPatch(patch);
        return &instrument_;
    }

    IInstrument* current() noexcept { return &instrument_; }
    const synth::patch::Patch& currentPatch() const noexcept { return instrument_.patch(); }

private:
    ModularInstrument instrument_{};
};
