#pragma once

#include <cstdint>
#include "synth/voice/VoiceAllocator.h"

class PatchDriver;

struct PatchConfig {
    const PatchDriver* driver = nullptr;
    synth::voice::SameNoteMode sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
    uint8_t maxVoices = 8;
    float fmModIndex = 5.0f;
    float fmModRatio = 2.0f;
};
