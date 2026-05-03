#include "DrumKitInstrument.h"

namespace {

inline float clampf(float x, float lo, float hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}

} // namespace

void DrumKitInstrument::setPatch(const synth::patch::Patch& patch) noexcept {
    name_ = patch.name;
    maxVoices_ = patch.maxVoices;
    sameNoteMode_ = patch.sameNoteMode;

    const auto& kit = patch.drumKit;
    const PadResolve fb{kit.fallback, kit.fallbackPan, kit.fallbackVelScale};
    for (auto& e : notePad_) {
        e = fb;
    }
    for (uint8_t i = 0; i < kit.padCount; ++i) {
        const auto& pad = kit.pads[i];
        notePad_[pad.note] = {pad.kind, pad.pan, pad.velScale};
    }
    for (auto& v : voices_) {
        v.pan = 0.0f;
        v.body.template emplace<0>();
    }
}

void DrumKitInstrument::onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept {
    if (v >= MAX_VOICES) return;
    const PadResolve& r = notePad_[ctx.note & 127];
    Voice& s = voices_[v];
    s.pan = r.pan;
    const float vel = clampf(ctx.velocity * r.velScale, 0.0f, 1.0f);
    switch (r.kind) {
        case synth::patch::DrumPadKind::Kick:
            s.body.template emplace<1>();
            std::get<1>(s.body).trigger(vel);
            break;
        case synth::patch::DrumPadKind::Snare:
            s.body.template emplace<2>();
            std::get<2>(s.body).trigger(vel);
            break;
        case synth::patch::DrumPadKind::Hat:
            s.body.template emplace<3>();
            std::get<3>(s.body).trigger(vel);
            break;
    }
}

void DrumKitInstrument::renderAddVoice(uint8_t v, float* out, uint64_t n) noexcept {
    if (v >= MAX_VOICES || !out || n == 0) return;
    auto& body = voices_[v].body;
    switch (body.index()) {
        case 0:
            break;
        case 1: {
            auto& d = std::get<1>(body);
            for (uint64_t i = 0; i < n; ++i) out[i] += d.tick();
            break;
        }
        case 2: {
            auto& d = std::get<2>(body);
            for (uint64_t i = 0; i < n; ++i) out[i] += d.tick();
            break;
        }
        case 3: {
            auto& d = std::get<3>(body);
            for (uint64_t i = 0; i < n; ++i) out[i] += d.tick();
            break;
        }
        default:
            break;
    }
}

float DrumKitInstrument::voicePan(uint8_t v) const noexcept {
    if (v >= MAX_VOICES) return 0.0f;
    return voices_[v].pan;
}
