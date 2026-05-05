#include "ModularInstrument.h"

#include "workstation/Params.h"
#include "engine/AudioContext.h"

#include "synth/Constants.h"
#include "synth/dsp/Approx.h"
#include "synth/dsp/Util.h"
#include "synth/dsp/WaveTables.h"
#include "synth/modules/Resonator.h"

#include <array>
#include <cmath>
#include <cstring>

namespace {

constexpr float kTwoPi = synth::cfg::kTwoPi;
constexpr float kInv2Pi = synth::cfg::kInv2Pi;
constexpr float kRadiansToPhase01 = synth::cfg::kRadiansToPhase01;

constexpr uint64_t kPitchBatch = synth::cfg::kPitchRecomputeBatch;
static_assert((kPitchBatch & (kPitchBatch - 1)) == 0, "kPitchBatch must be a power of two");

 float wrap01(const float ph) noexcept {
    return (ph >= 1.0f) ? ph - 1.0f : ph;
}

 float wrap01Trunc(float ph) noexcept {
    ph -= static_cast<float>(static_cast<int>(ph));
    return (ph < 0.0f) ? ph + 1.0f : ph;
}

 synth::modules::OscWave toOscWave(const synth::patch::OscShape s) noexcept {
    using P = synth::patch::OscShape;
    using W = synth::modules::OscWave;
    switch (s) {
        case P::Sine:     return W::Sine;
        case P::Saw:      return W::Saw;
        case P::Triangle: return W::Triangle;
        case P::Pulse:    return W::Pulse;
    }
    return W::Sine;
}

 synth::modules::SvfMode toSvfMode(const synth::patch::FilterMode m) noexcept {
    using P = synth::patch::FilterMode;
    using S = synth::modules::SvfMode;
    switch (m) {
        case P::LowPass:  return S::LowPass;
        case P::BandPass: return S::BandPass;
        case P::HighPass: return S::HighPass;
    }
    return S::LowPass;
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

 void buildOscRenderOrder(const std::array<synth::patch::OscDef, synth::patch::MAX_OSCS>& oscs,
                          const uint8_t oscCount,
                          std::array<uint8_t, synth::patch::MAX_OSCS>& outOrder) noexcept {
    // Topological sort: fmFrom -> carrier. If cyclic/invalid, fall back to 0..N-1.
    std::array<uint8_t, synth::patch::MAX_OSCS> indeg{};
    std::array<std::array<uint8_t, synth::patch::MAX_OSCS>, synth::patch::MAX_OSCS> adj{};
    std::array<uint8_t, synth::patch::MAX_OSCS> adjCount{};

    for (uint8_t i = 0; i < oscCount; ++i) {
        const int8_t from = oscs[i].fmFrom;
        if (from >= 0 && static_cast<uint8_t>(from) < oscCount) {
            const uint8_t u = static_cast<uint8_t>(from);
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

    uint8_t outN = 0;
    while (qh != qt) {
        const uint8_t u = q[qh++];
        outOrder[outN++] = u;
        for (uint8_t k = 0; k < adjCount[u]; ++k) {
            const uint8_t v = adj[u][k];
            indeg[v] = static_cast<uint8_t>(indeg[v] - 1);
            if (indeg[v] == 0) q[qt++] = v;
        }
    }

    if (outN != oscCount) {
        for (uint8_t i = 0; i < oscCount; ++i) outOrder[i] = i;
    }
    for (uint8_t i = oscCount; i < synth::patch::MAX_OSCS; ++i) outOrder[i] = i;
 }

} // namespace


void ModularInstrument::setPatch(const synth::patch::Patch& patch) noexcept {
    if (patch.kind == synth::patch::PatchKind::DrumKit) {
        return;
    }

    patch_ = patch;

    buildOscRenderOrder(patch_.oscs, patch_.oscCount, oscRenderOrder_);

    outDrive_.preGain = patch_.drive.preGain;
    outDrive_.postGain = patch_.drive.postGain;

    // Bake module config straight onto every voice's modules. After this the
    // Patch can be discarded; per-voice modules carry everything they need.
    const float sr = engine::gAudio.sampleRate;
    for (uint8_t v = 0; v < MAX_VOICES; ++v) {
        VoiceState& vs = voices_[v];

        for (uint8_t i = 0; i < MAX_ENVS; ++i) {
            auto& e = vs.modEnvs[i];
            e.attack_s  = patch_.envs[i].attack;
            e.decay_s   = patch_.envs[i].decay;
            e.sustain   = patch_.envs[i].sustain;
            e.release_s = patch_.envs[i].release;
            e.refresh();
            e.reset();
        }

        for (uint8_t i = 0; i < MAX_LFOS; ++i) {
            auto& l = vs.lfos[i];
            l.wave = toLfoWave(patch_.lfos[i].shape);
            l.rateHz = patch_.lfos[i].rateHz
                     + patch_.lfos[i].perVoiceRateSpread * static_cast<float>(v);
            float ph = patch_.lfos[i].perVoicePhase * static_cast<float>(v);
            ph -= floorf(ph);
            l.phase = ph;
            l.shState = 0x13579BDFu;
            l.shValue = 0.0f;
        }

        for (uint8_t i = 0; i < MAX_FILTERS; ++i) {
            auto& f = vs.filters[i];
            f.reset();
            f.mode = toSvfMode(patch_.filters[i].mode);
            f.set(patch_.filters[i].cutoffHz, patch_.filters[i].resonance);
        }

        vs.pink.color = patch_.noise.color;
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
    for (uint8_t i = 0; i < MAX_OSCS; ++i) {
        s.oscPhase[i] = 0.0f;
        s.oscFb[i] = 0.0f;
        s.oscOut[i] = 0.0f;
        s.oscSawTbl[i] = 0;
    }
    for (auto& e : s.modEnvs) e.reset();
    for (auto& f : s.filters) f.reset();
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
    const float sr = engine::gAudio.sampleRate;

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

    for (uint8_t i = 0; i < patch_.oscCount && i < MAX_OSCS; ++i) {
        s.oscPhase[i] = (i == 1) ? 0.5f : 0.0f;
        s.oscFb[i] = 0.0f;
        s.oscOut[i] = 0.0f;

        const auto& od = patch_.oscs[i];
        const float ratio = od.pitchRatio * (od.isFmModulator ? patch_.fmRatioScale : 1.0f);
        const float octaves = (od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f);
        const float f = ctx.frequency * ratio * synth::dsp::exp2Fast(octaves);
        s.oscSawTbl[i] = synth::dsp::sawTableIdx(f > 1.0f ? f : 1.0f);
    }

    for (uint8_t i = 1; i < patch_.envCount && i < MAX_ENVS; ++i) {
        s.modEnvs[i].reset();
        s.modEnvs[i].noteOn();
    }

    for (uint8_t i = 0; i < patch_.lfoCount && i < MAX_LFOS; ++i) {
        s.lfos[i].phase = wrap01Trunc(patch_.lfos[i].perVoicePhase * static_cast<float>(v));
    }

    for (uint8_t i = 0; i < patch_.filterCount && i < MAX_FILTERS; ++i) {
        auto& f = s.filters[i];
        f.reset();
        const auto& fd = patch_.filters[i];
        const float cutoff = fd.cutoffHz + fd.velToCutoffHz * ctx.velocity;
        f.mode = toSvfMode(fd.mode);
        f.set(cutoff, fd.resonance);
    }

    if (patch_.exciter.enabled) {
        const float clk = patch_.exciter.clickLevelBase + patch_.exciter.clickLevelVel * ctx.velocity;
        const float bst = patch_.exciter.noiseLevelBase + patch_.exciter.noiseLevelVel * ctx.velocity;
        s.click.trigger(clk, patch_.exciter.clickDecay);
        s.burst.trigger(bst, patch_.exciter.noiseDecay);
    }

    if (patch_.resonator.enabled) {
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
    VoiceState& s = voices_[v];
    for (uint8_t i = 1; i < MAX_ENVS; ++i) s.modEnvs[i].noteOff();
}

void ModularInstrument::renderAddVoice(const uint8_t v, float* outMono, const uint64_t numSamples) noexcept {
    if (v >= MAX_VOICES || !outMono || numSamples == 0) return;
    renderSynth_(v, outMono, numSamples);
}

void ModularInstrument::renderSynth_(const uint8_t voiceIdx, float* out, const uint64_t n) noexcept {
    VoiceState& s = voices_[voiceIdx];
    const float invSr = engine::gAudio.invSampleRate;

    const bool resoOn = patch_.resonator.enabled;
    const bool driveOn = patch_.drive.enabled;
    float* const resoBuf = resoOn ? voiceResoBuf_(voiceIdx) : nullptr;

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

    std::array<float, MAX_OSCS> oscFreq{};
    std::array<float, MAX_OSCS> oscDp{};
    std::array<float, MAX_LFOS> lfoBuf{};
    std::array<float, MAX_LFOS> vibLfoBuf{};

    recomputeOscDp_(s, vibLfoBuf, invSr, oscFreq, oscDp);
    for (uint8_t oi = 0; oi < patch_.oscCount && oi < MAX_OSCS; ++oi) {
        const float f = (oscFreq[oi] > 1.0f) ? oscFreq[oi] : 1.0f;
        s.oscSawTbl[oi] = synth::dsp::sawTableIdx(f);
    }

    uint64_t batchTimer = 0;

    for (uint64_t i = 0; i < n; ++i) {
        for (uint8_t li = 0; li < patch_.lfoCount && li < MAX_LFOS; ++li) {
            lfoBuf[li] = s.lfos[li].tick();
        }
        for (uint8_t ei = 1; ei < patch_.envCount && ei < MAX_ENVS; ++ei) {
            s.modEnvs[ei].tick(invSr);
        }

        if (vibPitch) {
            if (batchTimer == 0) {
                for (uint8_t li = 0; li < MAX_LFOS; ++li) vibLfoBuf[li] = lfoBuf[li];
                recomputeOscDp_(s, vibLfoBuf, invSr, oscFreq, oscDp);
                for (uint8_t oi = 0; oi < patch_.oscCount && oi < MAX_OSCS; ++oi) {
                    const float f = (oscFreq[oi] > 1.0f) ? oscFreq[oi] : 1.0f;
                    s.oscSawTbl[oi] = synth::dsp::sawTableIdx(f);
                }
            }
            batchTimer = (batchTimer + 1) & (kPitchBatch - 1);
        }

        float oscMix = 0.0f;
        for (uint8_t ord = 0; ord < patch_.oscCount && ord < MAX_OSCS; ++ord) {
            const uint8_t oi = oscRenderOrder_[ord];
            const float y = renderOneOsc_(s, oi, oscDp[oi]);
            const float lvl = patch_.oscs[oi].level;
            if (lvl != 0.0f) oscMix += y * lvl;
        }

        float noiseSample = 0.0f;
        if (patch_.noise.shape == synth::patch::NoiseShape::White) {
            noiseSample = s.white.tick() * patch_.noise.level;
        } else if (patch_.noise.shape == synth::patch::NoiseShape::Pinkish) {
            noiseSample = s.pink.tick() * patch_.noise.level;
        }

        const float resoOut = (resoOn && resoBuf) ? tickResonator_(s, resoBuf) : 0.0f;

        float sig;
        if (resoOn) {
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
            updateFilterCutoffs_(s, lfoBuf);
        }
        for (uint8_t fi = 0; fi < patch_.filterCount && fi < MAX_FILTERS; ++fi) {
            sig = s.filters[fi].tick(sig);
        }

        if (driveOn) {
            sig = outDrive_.tick(sig);
        }

        out[i] += sig * patch_.outputGain;
    }
}

float ModularInstrument::renderOneOsc_(VoiceState& s, const uint8_t oi, const float dp) const noexcept {
    const auto& od = patch_.oscs[oi];

    const float ph = wrap01(s.oscPhase[oi] + dp);
    s.oscPhase[oi] = ph;

    float phaseMod = ph;
    if (od.fmFeedback != 0.0f) {
        phaseMod += s.oscFb[oi] * od.fmFeedback * kRadiansToPhase01;
    }
    if (od.fmFrom >= 0 && od.fmFrom < patch_.oscCount) {
        float idx = od.fmIndex;
        if (patch_.oscs[od.fmFrom].isFmModulator) idx *= patch_.fmIndexScale;
        if (od.modEnvIndex >= 1 && od.modEnvIndex < MAX_ENVS) {
            idx *= s.modEnvs[od.modEnvIndex].level;
        }
        if (od.velToFmIndex > 0.0f) {
            const float velMix = od.velToFmIndex;
            idx *= (1.0f - velMix) + velMix * (0.25f + 0.75f * s.vel);
        }
        phaseMod += s.oscOut[od.fmFrom] * idx * kRadiansToPhase01;
    }
    phaseMod = wrap01Trunc(phaseMod);

    const float y =
        synth::modules::sampleOscWave(toOscWave(od.shape), phaseMod, s.oscSawTbl[oi], od.pulseWidth);
    s.oscOut[oi] = y;
    s.oscFb[oi] = y;
    return y;
}

float ModularInstrument::tickResonator_(VoiceState& s, float* resoBuf) noexcept {
    const float exc = s.click.tick() + s.burst.tick();
    return synth::modules::tickKarplusRingPow2(resoBuf, kResonatorMask, s.resoIdx, s.resoDelay, s.resoLp,
                                               s.resoDamp, s.resoFb, exc);
}

void ModularInstrument::updateFilterCutoffs_(VoiceState& s,
                                             const std::array<float, MAX_LFOS>& lfoBuf) const noexcept {
    for (uint8_t fi = 0; fi < patch_.filterCount && fi < MAX_FILTERS; ++fi) {
        const auto& fd = patch_.filters[fi];

        float envLvl = 0.0f;
        if (fd.envIndex >= 1 && fd.envIndex < MAX_ENVS) {
            envLvl = s.modEnvs[fd.envIndex].level;
        }

        float cut = fd.cutoffHz + (fd.cutoffPeakHz - fd.cutoffHz) * envLvl;
        cut += fd.velToCutoffHz * s.vel;
        if (fd.lfoIndex >= 0 && fd.lfoIndex < MAX_LFOS) {
            cut += lfoBuf[fd.lfoIndex] * fd.lfoCutoffHz;
        }
        if (cut < 20.0f) cut = 20.0f;

        s.filters[fi].set(cut, fd.resonance);
    }
}

void ModularInstrument::recomputeOscDp_(const VoiceState& s, const std::array<float, MAX_LFOS>& vibSemis,
                                         const float invSr, std::array<float, MAX_OSCS>& oscFreq,
                                         std::array<float, MAX_OSCS>& oscDp) const noexcept {
    for (uint8_t i = 0; i < patch_.oscCount && i < MAX_OSCS; ++i) {
        const auto& od = patch_.oscs[i];
        const float ratio = od.pitchRatio * (od.isFmModulator ? patch_.fmRatioScale : 1.0f);

        float vibOctaves = 0.0f;
        if (od.vibratoLfoIndex >= 0
            && static_cast<uint8_t>(od.vibratoLfoIndex) < patch_.lfoCount) {
            vibOctaves = vibSemis[od.vibratoLfoIndex] * od.vibratoSemitones * (1.0f / 12.0f);
        }
        const float octaves =
            (od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f) + vibOctaves;

        const float f = s.freq * ratio * synth::dsp::exp2Fast(octaves);
        oscFreq[i] = f;
        oscDp[i] = f * invSr;
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
