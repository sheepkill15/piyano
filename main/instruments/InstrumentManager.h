#ifndef INSTRUMENTMANAGER_H
#define INSTRUMENTMANAGER_H

#include "IInstrument.h"
#include "SubtractiveInstrument.h"
#include "FMInstrument.h"

class InstrumentManager {
    public:
        InstrumentManager() noexcept;
    
        IInstrument* get(uint8_t index) noexcept;
    
    private:
        static constexpr uint8_t MAX_INSTRUMENTS = 2;
        IInstrument* instruments_[MAX_INSTRUMENTS];
        SubtractiveInstrument subtractive_;
        FMInstrument fm_;
    };
#endif