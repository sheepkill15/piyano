#pragma once

#include <cstdint>
#include "synth/voice/VoiceAllocator.h"

struct PatchConfig {
    enum class Kind : uint8_t { Subtractive, FM, Piano, EPiano, Strings, Bass, Drums };
    Kind kind = Kind::Subtractive;
    synth::voice::SameNoteMode sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
    uint8_t maxVoices = 8;

    // generic patch params
    float fmModIndex = 5.0f;
    float fmModRatio = 2.0f;
};

namespace PatchBuilder {
    inline PatchConfig Subtractive() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::Subtractive;
        c.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
        c.maxVoices = 8;
        return c;
    }

    inline PatchConfig FM() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::FM;
        c.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
        c.maxVoices = 8;
        c.fmModIndex = 5.0f;
        c.fmModRatio = 2.0f;
        return c;
    }

    inline PatchConfig Piano() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::Piano;
        c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
        c.maxVoices = 8;
        return c;
    }

    inline PatchConfig EPiano() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::EPiano;
        c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
        c.maxVoices = 8;
        c.fmModIndex = 4.5f;
        c.fmModRatio = 2.2f;
        return c;
    }

    inline PatchConfig Strings() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::Strings;
        c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
        c.maxVoices = 8;
        return c;
    }

    inline PatchConfig Bass() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::Bass;
        c.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
        c.maxVoices = 1;
        return c;
    }

    inline PatchConfig Drums() noexcept {
        PatchConfig c;
        c.kind = PatchConfig::Kind::Drums;
        c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
        c.maxVoices = 8;
        return c;
    }
}

