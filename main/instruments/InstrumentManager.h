#ifndef INSTRUMENTMANAGER_H
#define INSTRUMENTMANAGER_H

#include "IInstrument.h"
#include "PatchInstrument.h"

class InstrumentManager {
    public:
        InstrumentManager() noexcept;
    
        // Rebuild the single live patch from its config and return it.
        IInstrument* select(uint8_t index) noexcept;
        // Current live patch instance.
        IInstrument* current() noexcept { return &current_; }
        uint8_t getInstrumentCount() noexcept;
        const char* getName(uint8_t index) const noexcept;
    
    private:
        static constexpr uint8_t MAX_INSTRUMENTS = 7;
        PatchConfig configs_[MAX_INSTRUMENTS] = {};
        PatchInstrument current_;
        uint8_t currentIndex_ = 0;
    };
#endif