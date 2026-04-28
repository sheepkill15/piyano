#include "PatchInstrument.h"
#include "PatchDriver.h"

#include <cstdlib>
#include <cstring>

PatchInstrument::~PatchInstrument() noexcept { release_(); }

void PatchInstrument::release_() noexcept {
    const PatchDriver* d = driver_;
    void* inst = inst_;
    unsigned char* v = voices_;
    inst_ = nullptr;
    voices_ = nullptr;
    driver_ = nullptr;

    if (d && inst) {
        d->destroyInstrumentState(inst);
    }
    std::free(inst);
    std::free(v);
}

unsigned char* PatchInstrument::voiceSlot_(uint8_t vi) noexcept {
    if (!driver_ || !voices_) {
        return nullptr;
    }
    const std::size_t stride = driver_->voiceStateBytes();
    return voices_ + static_cast<std::size_t>(vi) * stride;
}

void PatchInstrument::init(const PatchConfig& cfg) noexcept {
    release_();
    cfg_ = cfg;
    driver_ = cfg.driver;
    if (!driver_) {
        return;
    }

    const std::size_t isz = driver_->instrumentStateBytes();
    if (isz > 0) {
        inst_ = std::malloc(isz);
        if (inst_) {
            driver_->constructInstrumentState(inst_);
            driver_->instrumentInit(inst_, sampleRate_, cfg_);
        }
    }

    const std::size_t vsz = driver_->voiceStateBytes();
    if (vsz > 0 && cfg_.maxVoices > 0) {
        voices_ = static_cast<unsigned char*>(std::malloc(static_cast<std::size_t>(cfg_.maxVoices) * vsz));
        if (voices_) {
            std::memset(voices_, 0, static_cast<std::size_t>(cfg_.maxVoices) * vsz);
        }
    }

    if (voices_) {
        driver_->applySampleRateToVoices(inst_, cfg_.maxVoices, voices_, vsz, sampleRate_);
    }
}

const char* PatchInstrument::name() const noexcept { return driver_ ? driver_->name() : "Patch"; }

void PatchInstrument::setSampleRate(float sr) noexcept {
    sampleRate_ = sr;
    if (!driver_) {
        return;
    }
    driver_->setSampleRate(inst_, sr);
    if (voices_) {
        driver_->applySampleRateToVoices(inst_, cfg_.maxVoices, voices_, driver_->voiceStateBytes(), sr);
    }
}

bool PatchInstrument::setParam(uint16_t paramId, float value) noexcept {
    return driver_ ? driver_->setParam(cfg_, paramId, value) : false;
}

void PatchInstrument::onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept {
    if (!driver_) {
        return;
    }
    unsigned char* slot = voiceSlot_(v);
    if (!slot) {
        return;
    }
    driver_->voiceStart(inst_, slot, v, ctx, sampleRate_, cfg_);
}

void PatchInstrument::onVoiceStop(uint8_t voiceIndex) noexcept { (void)voiceIndex; }

void PatchInstrument::renderAddVoice(uint8_t v, float* out, uint64_t n) noexcept {
    if (!driver_) {
        return;
    }
    unsigned char* slot = voiceSlot_(v);
    if (!slot) {
        return;
    }
    driver_->voiceRenderAdd(inst_, slot, v, out, n, sampleRate_, cfg_);
}

float PatchInstrument::voicePan(uint8_t v) const noexcept {
    if (!driver_ || !voices_) return 0.0f;
    const std::size_t stride = driver_->voiceStateBytes();
    const unsigned char* slot = voices_ + static_cast<std::size_t>(v) * stride;
    return driver_->voicePan(slot, v);
}
