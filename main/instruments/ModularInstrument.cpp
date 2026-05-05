#include "ModularInstrument.h"

#include "workstation/Params.h"
#include "synth/dsp/Util.h"
#include "synth/modules/Resonator.h"

#include <cmath>
#include <cstring>

namespace {

 float wrap01Trunc(float ph) noexcept {
    ph -= static_cast<float>(static_cast<int>(ph));
    return (ph < 0.0f) ? ph + 1.0f : ph;
}

 synth::modules::LfoWave toLfoWave(const synth::patch::LfoShape s) noexcept {
    using P = synth::patch::LfoShape;
    using L = synth::modules::LfoWave;
    switch (s) {
        case P::Sine:       return L::Sine;
        case P::Triangle:   return L::Triangle;
        case P::Square:     return L::Square;
        case P::SampleHold: return L::SampleHold;
    }
    return L::Sine;
}

 void sortOscsForFmInPlace(std::array<synth::patch::OscDef, synth::patch::MAX_OSCS>& oscs,
                           const uint8_t oscCount) noexcept {
    // Topological sort: fmFrom -> carrier. If cyclic/invalid, fall back to 0..N-1.
    std::array<uint8_t, synth::patch::MAX_OSCS> indeg{};
    std::array<std::array<uint8_t, synth::patch::MAX_OSCS>, synth::patch::MAX_OSCS> adj{};
    std::array<uint8_t, synth::patch::MAX_OSCS> adjCount{};

    for (uint8_t i = 0; i < oscCount; ++i) {
        const int8_t from = oscs[i].fmFrom;
        if (from >= 0 && static_cast<uint8_t>(from) < oscCount) {
            const auto u = static_cast<uint8_t>(from);
            const uint8_t v = i;
            adj[u][adjCount[u]++] = v;
            indeg[v] = static_cast<uint8_t>(indeg[v] + 1);
        }
    }

    std::array<uint8_t, synth::patch::MAX_OSCS> q{};
    uint8_t qh = 0;
    uint8_t qt = 0;
    for (uint8_t i = 0; i < oscCount; ++i) {
        if (indeg[i] == 0) q[qt++] = i;
    }

    std::array<uint8_t, synth::patch::MAX_OSCS> order{};
    uint8_t outN = 0;
    while (qh != qt) {
        const uint8_t u = q[qh++];
        order[outN++] = u;
        for (uint8_t k = 0; k < adjCount[u]; ++k) {
            const uint8_t v = adj[u][k];
            indeg[v] = static_cast<uint8_t>(indeg[v] - 1);
            if (indeg[v] == 0) q[qt++] = v;
        }
    }

    if (outN != oscCount) return;

    std::array<uint8_t, synth::patch::MAX_OSCS> oldToNew{};
    for (uint8_t newIdx = 0; newIdx < oscCount; ++newIdx) {
        oldToNew[order[newIdx]] = newIdx;
    }

    const auto oldOscs = oscs;
    for (uint8_t newIdx = 0; newIdx < oscCount; ++newIdx) {
        auto od = oldOscs[order[newIdx]];
        if (od.fmFrom >= 0 && static_cast<uint8_t>(od.fmFrom) < oscCount) {
            od.fmFrom = static_cast<int8_t>(oldToNew[static_cast<uint8_t>(od.fmFrom)]);
        } else {
            od.fmFrom = -1;
        }
        oscs[newIdx] = od;
    }
 }

} // namespace


void ModularInstrument::setPatch(const synth::patch::Patch& patch) noexcept {
    if (patch.kind == synth::patch::PatchKind::DrumKit) {
        return;
    }

    patch_ = patch;
    sortOscsForFmInPlace(patch_.oscs, patch_.oscCount);

    outDrive_.preGain = patch_.drive.preGain;
    outDrive_.postGain = patch_.drive.postGain;

    for (uint8_t v = 0; v < MAX_VOICES; ++v) {
        VoiceState& s = voices_[v];

        for (uint8_t i = 0; i < MAX_ENVS; ++i) {
            auto& e = s.modEnvs[i];
            e.attack_s  = patch_.envs[i].attack;
            e.decay_s   = patch_.envs[i].decay;
            e.sustain   = patch_.envs[i].sustain;
            e.release_s = patch_.envs[i].release;
            e.refresh();
            e.reset();
        }

        for (uint8_t i = 0; i < MAX_LFOS; ++i) {
            auto& l = s.lfos[i];
            l.wave = toLfoWave(patch_.lfos[i].shape);
            l.rateHz = patch_.lfos[i].rateHz + patch_.lfos[i].perVoiceRateSpread * static_cast<float>(v);
            float ph = patch_.lfos[i].perVoicePhase * static_cast<float>(v);
            ph -= floorf(ph);
            l.phase = ph;
            l.shState = 0x13579BDFu;
            l.shValue = 0.0f;
        }

        s.filterBank.bakePatch(patch_);
        s.noise.pink.color = patch_.noise.color;
    }

    if (patch_.resonator.enabled) {
        std::memset(resoPool_.data(), 0, sizeof(resoPool_));
    }
    for (uint8_t v = 0; v < MAX_VOICES; ++v) {
        resetVoice_(v);
    }
}

void ModularInstrument::resetVoice_(uint8_t v) noexcept {
    VoiceState& s = voices_[v];
    s.active = false;
    s.freq = 0.0f;
    s.vel = 0.0f;
    s.note = 0;
    s.keyT = 0.0f;
    s.pan = 0.0f;
    s.oscBank.reset();
    for (auto& e : s.modEnvs) e.reset();
    for (auto& l : s.lfos) {
        (void)l;
    }
    s.filterBank.reset();
    s.resoIdx = 0;
    s.resoLp = 0.0f;
    s.bodyHp.reset();
    s.outLp.reset(0.0f);
    if (patch_.resonator.enabled) {
        std::memset(voiceResoBuf_(v), 0, sizeof(float) * kResonatorBufSamples);
    }
}

void ModularInstrument::onVoiceStart(const uint8_t v, const VoiceContext& ctx) noexcept {
    if (v >= MAX_VOICES) return;
    VoiceState& s = voices_[v];
    s.active = true;
    s.freq = ctx.frequency;
    s.vel = ctx.velocity;
    s.note = ctx.note;
    s.keyT = synth::dsp::clamp((static_cast<float>(ctx.note) - 21.0f) / 88.0f, 0.0f, 1.0f);

    if (patch_.panSpread > 0.0f) {
        const float t = static_cast<float>(static_cast<int>(v) & 7) / 7.0f - 0.5f;
        s.pan = synth::dsp::clamp(t * 2.0f * patch_.panSpread, -patch_.panSpread, patch_.panSpread);
    } else {
        s.pan = 0.0f;
    }

    s.oscBank.noteOn(patch_, v, ctx.frequency);

    for (uint8_t i = 1; i < patch_.envCount && i < MAX_ENVS; ++i) {
        s.modEnvs[i].reset();
        s.modEnvs[i].noteOn();
    }

    for (uint8_t i = 0; i < patch_.lfoCount && i < MAX_LFOS; ++i) {
        s.lfos[i].phase = wrap01Trunc(patch_.lfos[i].perVoicePhase * static_cast<float>(v));
    }

    s.filterBank.noteOn(patch_, ctx.velocity);

    if (patch_.exciter.enabled) {
        const float clk = patch_.exciter.clickLevelBase + patch_.exciter.clickLevelVel * ctx.velocity;
        const float bst = patch_.exciter.noiseLevelBase + patch_.exciter.noiseLevelVel * ctx.velocity;
        s.click.trigger(clk, patch_.exciter.clickDecay);
        s.burst.trigger(bst, patch_.exciter.noiseDecay);
    }

    if (patch_.resonator.enabled) {
        const float sr = engine::gAudio.sampleRate;
        const float f = (ctx.frequency < 20.0f) ? 20.0f : ctx.frequency;
        auto delay = static_cast<std::size_t>(sr / f);
        if (delay < 2) delay = 2;
        if (delay >= kResonatorBufSamples) delay = kResonatorBufSamples - 1;

        s.resoDelay = delay;
        s.resoIdx = 0;
        s.resoLp = 0.0f;
        s.resoFb = patch_.resonator.feedback;
        s.resoDamp = patch_.resonator.dampLowKey
                   + (patch_.resonator.dampHighKey - patch_.resonator.dampLowKey) * s.keyT;
        std::memset(voiceResoBuf_(v), 0, sizeof(float) * kResonatorBufSamples);

        s.bodyHp.reset();
        s.bodyHp.mode = synth::modules::SvfMode::HighPass;
        s.bodyHp.set(patch_.resonator.bodyHpHz, 0.0f);

        s.outLp.reset(0.0f);
        const float tone = patch_.resonator.outputLpHzBase + patch_.resonator.outputLpHzVel * ctx.velocity;
        s.outLp.setCutoffHz(tone, sr);
    }
}

void ModularInstrument::onVoiceStop(const uint8_t v) noexcept {
    if (v >= MAX_VOICES) return;
    for (uint8_t i = 1; i < MAX_ENVS; ++i) voices_[v].modEnvs[i].noteOff();
}

void ModularInstrument::renderAddVoice(const uint8_t v, float* outMono, const uint64_t numSamples) noexcept {
    if (v >= MAX_VOICES || !outMono || numSamples == 0) return;
    VoiceState& s = voices_[v];
    const float invSr = engine::gAudio.invSampleRate;
    float* const resoBuf = patch_.resonator.enabled ? voiceResoBuf_(v) : nullptr;

    std::array<float, MAX_LFOS> lfoBuf{};
    std::array<float, MAX_ENVS> envLevels{};

    bool vibPitch = false;
    for (uint8_t oi = 0; oi < patch_.oscCount && oi < MAX_OSCS; ++oi) {
        const auto& od = patch_.oscs[oi];
        if (od.vibratoLfoIndex >= 0
            && od.vibratoLfoIndex < patch_.lfoCount
            && od.vibratoSemitones != 0.0f) {
            vibPitch = true;
            break;
        }
    }

    constexpr uint64_t kPitchBatch = synth::cfg::kPitchRecomputeBatch;
    uint64_t batchTimer = 0;

    for (uint64_t i = 0; i < numSamples; ++i) {
        for (uint8_t li = 0; li < patch_.lfoCount && li < MAX_LFOS; ++li) {
            lfoBuf[li] = s.lfos[li].tick();
        }
        for (uint8_t ei = 1; ei < patch_.envCount && ei < MAX_ENVS; ++ei) {
            s.modEnvs[ei].tick(invSr);
            envLevels[ei] = s.modEnvs[ei].level;
        }

        if (vibPitch) {
            if (batchTimer == 0) s.oscBank.recomputePitch(patch_, s.freq, lfoBuf);
            batchTimer = (batchTimer + 1) & (kPitchBatch - 1);
        }

        const float oscMix = s.oscBank.tickMix(patch_, envLevels, s.vel);
        const float noiseSample = s.noise.tick(patch_.noise);

        const float resoOut =
            (patch_.resonator.enabled && resoBuf)
                ? synth::modules::tickKarplusRingPow2(resoBuf, kResonatorMask, s.resoIdx, s.resoDelay, s.resoLp,
                                                     s.resoDamp, s.resoFb, s.click.tick() + s.burst.tick())
                : 0.0f;

        float sig;
        if (patch_.resonator.enabled) {
            const float harmGain = 0.20f + 0.55f * s.vel;
            sig = resoOut * patch_.resonator.bodyMix
                + oscMix * patch_.resonator.harmAmount * harmGain
                + noiseSample;
            sig = s.bodyHp.tick(sig);
            sig = s.outLp.tick(sig);
        } else {
            sig = oscMix + noiseSample;
        }

        if ((i & (kPitchBatch - 1)) == 0) {
            s.filterBank.updateCutoffs(patch_, s.vel, lfoBuf, envLevels);
        }
        sig = s.filterBank.process(patch_, sig);

        if (patch_.drive.enabled) sig = outDrive_.tick(sig);

        outMono[i] += sig * patch_.outputGain;
    }
}

float* ModularInstrument::voiceResoBuf_(const uint8_t v) noexcept {
    return resoPool_.data() + static_cast<std::size_t>(v) * kResonatorBufSamples;
}

float ModularInstrument::voicePan(const uint8_t v) const noexcept {
    if (v >= MAX_VOICES) return 0.0f;
    return voices_[v].pan;
}

bool ModularInstrument::setParam(const uint16_t paramId, const float value) noexcept {
    switch (paramId) {
        case Params::FmModIndex: {
            const float v = synth::dsp::clamp(value, 0.0f, 1.0f);
            patch_.fmIndexScale = v * 2.0f;
            return true;
        }
        case Params::FmModRatio: {
            const float v = synth::dsp::clamp(value, 0.0f, 1.0f);
            patch_.fmRatioScale = 0.25f + v * (4.0f - 0.25f);
            return true;
        }
        default:
            break;
    }
    return false;
}
