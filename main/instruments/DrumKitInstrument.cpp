#include "DrumKitInstrument.h"

#include "synth/dsp/Util.h"

#include <type_traits>

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
        v.body.emplace<0>();
    }
}

void DrumKitInstrument::onVoiceStart(const uint8_t v, const VoiceContext& ctx) noexcept {
    if (v >= MAX_VOICES) return;
    const PadResolve& r = notePad_[ctx.note & 127];
    Voice& s = voices_[v];
    s.pan = r.pan;
    const float vel = synth::dsp::clamp(ctx.velocity * r.velScale, 0.0f, 1.0f);
    switch (r.kind) {
        case synth::patch::DrumPadKind::Kick:
            s.body.emplace<1>();
            std::get<1>(s.body).trigger(vel);
            break;
        case synth::patch::DrumPadKind::Snare:
            s.body.emplace<2>();
            std::get<2>(s.body).trigger(vel);
            break;
        case synth::patch::DrumPadKind::Hat:
            s.body.emplace<3>();
            std::get<3>(s.body).trigger(vel);
            break;
    }
}

void DrumKitInstrument::renderAddVoice(const uint8_t v, float* outMono, uint64_t numSamples) noexcept {
    if (v >= MAX_VOICES || !outMono || numSamples == 0) return;
    auto& body = voices_[v].body;
    std::visit([outMono, numSamples]<typename T0>(T0& d) {
        using T = std::decay_t<T0>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            (void)d;
        } else {
            for (uint64_t i = 0; i < numSamples; ++i) outMono[i] += d.tick();
        }
    }, body);
}

float DrumKitInstrument::voicePan(const uint8_t v) const noexcept {
    if (v >= MAX_VOICES) return 0.0f;
    return voices_[v].pan;
}
