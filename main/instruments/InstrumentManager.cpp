#include "InstrumentManager.h"
#include "PatchBuilder.h"
#include "PatchDriver.h"

InstrumentManager::InstrumentManager() noexcept {
    configs_[0] = PatchBuilder::Subtractive();
    configs_[1] = PatchBuilder::FM();
    configs_[2] = PatchBuilder::Piano();
    configs_[3] = PatchBuilder::EPiano();
    configs_[4] = PatchBuilder::Strings();
    configs_[5] = PatchBuilder::Bass();
    configs_[6] = PatchBuilder::Drums();

    currentIndex_ = 0;
    current_.init(configs_[0]);
}

IInstrument* InstrumentManager::select(uint8_t index) noexcept {
    if (index >= MAX_INSTRUMENTS) return nullptr;
    currentIndex_ = index;
    current_.init(configs_[index]);
    return &current_;
}

uint8_t InstrumentManager::getInstrumentCount() noexcept {
    return MAX_INSTRUMENTS;
}

const char* InstrumentManager::getName(uint8_t index) const noexcept {
    if (index >= MAX_INSTRUMENTS) {
        return "Unknown";
    }
    const PatchDriver* d = configs_[index].driver;
    return d ? d->name() : "Unknown";
}