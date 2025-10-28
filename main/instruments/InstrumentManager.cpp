#include "InstrumentManager.h"

InstrumentManager::InstrumentManager() noexcept {
    instruments_[0] = &subtractive_;
    instruments_[1] = &fm_;
}

IInstrument* InstrumentManager::get(uint8_t index) noexcept {
    if (index < MAX_INSTRUMENTS) return instruments_[index];
    return nullptr;
}