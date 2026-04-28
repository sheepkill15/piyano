#pragma once

#include "PatchConfig.h"
#include "PatchDriverRegistry.h"

namespace PatchBuilder {
inline PatchConfig Subtractive() noexcept {
    PatchConfig c;
    c.driver = subtractivePatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
    c.maxVoices = 8;
    return c;
}

inline PatchConfig FM() noexcept {
    PatchConfig c;
    c.driver = fmPatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
    c.maxVoices = 8;
    c.fmModIndex = 5.0f;
    c.fmModRatio = 2.0f;
    return c;
}

inline PatchConfig Piano() noexcept {
    PatchConfig c;
    c.driver = pianoPatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    c.maxVoices = 8;
    return c;
}

inline PatchConfig EPiano() noexcept {
    PatchConfig c;
    c.driver = ePianoPatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    c.maxVoices = 8;
    c.fmModIndex = 4.5f;
    c.fmModRatio = 2.2f;
    return c;
}

inline PatchConfig Strings() noexcept {
    PatchConfig c;
    c.driver = stringsPatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    c.maxVoices = 8;
    return c;
}

inline PatchConfig Bass() noexcept {
    PatchConfig c;
    c.driver = bassPatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::SingleVoicePerKey;
    c.maxVoices = 1;
    return c;
}

inline PatchConfig Drums() noexcept {
    PatchConfig c;
    c.driver = drumsPatchDriverPtr();
    c.sameNoteMode = synth::voice::SameNoteMode::MultiVoicePerKey;
    c.maxVoices = 8;
    return c;
}
} // namespace PatchBuilder
