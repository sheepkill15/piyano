#include "DrumKitInstrument.h"

namespace {

inline float clampf(float x, float lo, float hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

} // namespace

void DrumKitInstrument::rebuildNoteTable_() noexcept {
    const auto& kit = patch_.drumKit;
    const PadResolve fb{kit.fallback, kit.fallbackPan, kit.fallbackVelScale};
    for (auto& e : notePad_) {
        e = fb;
    }
    for (uint8_t i = 0; i < kit.padCount; ++i) {
        const auto& pad = kit.pads[i];
        notePad_[pad.note] = {pad.kind, pad.pan, pad.velScale};
    }
}

void DrumKitInstrument::setPatch(const synth::patch::Patch& patch) noexcept {
    patch_ = patch;
    rebuildNoteTable_();
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
    std::visit(
        overloaded{[](std::monostate&) noexcept {},
                     [&](synth::modules::Kick& d) noexcept {
                         for (uint64_t i = 0; i < n; ++i) out[i] += d.tick();
                     },
                     [&](synth::modules::Snare& d) noexcept {
                         for (uint64_t i = 0; i < n; ++i) out[i] += d.tick();
                     },
                     [&](synth::modules::Hat& d) noexcept {
                         for (uint64_t i = 0; i < n; ++i) out[i] += d.tick();
                     }},
        voices_[v].body);
}

float DrumKitInstrument::voicePan(uint8_t v) const noexcept {
    if (v >= MAX_VOICES) return 0.0f;
    return voices_[v].pan;
}
