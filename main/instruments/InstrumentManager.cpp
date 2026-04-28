#include "InstrumentManager.h"

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
    if (index >= MAX_INSTRUMENTS) return "Unknown";
    switch (configs_[index].kind) {
        case PatchConfig::Kind::Subtractive: return "Subtractive";
        case PatchConfig::Kind::FM: return "FM";
        case PatchConfig::Kind::Piano: return "Piano";
        case PatchConfig::Kind::EPiano: return "EPiano";
        case PatchConfig::Kind::Strings: return "Strings";
        case PatchConfig::Kind::Bass: return "Bass";
        case PatchConfig::Kind::Drums: return "Drums";
        default: return "Unknown";
    }
}