#include "InstrumentManager.h"

InstrumentManager::InstrumentManager() noexcept {
    instruments_[0] = &subtractive_;
    instruments_[1] = &fm_;
}

IInstrument* InstrumentManager::get(uint8_t index) noexcept {
    if (index < MAX_INSTRUMENTS) return instruments_[index];
    return nullptr;
}

uint8_t InstrumentManager::getInstrumentCount() noexcept {
    return MAX_INSTRUMENTS;
}

const char* InstrumentManager::getName(uint8_t index) const noexcept {
    switch (index) {
        case 0: return "Subtractive";
        case 1: return "FM";
        default: return "Unknown";
    }
}