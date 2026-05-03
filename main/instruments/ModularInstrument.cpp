#include "ModularInstrument.h"

#include "workstation/Params.h"
#include "engine/AudioContext.h"

#include "synth/dsp/Approx.h"
#include "synth/dsp/Util.h"
#include "synth/dsp/WaveTables.h"
#include "synth/modules/Resonator.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kInv2Pi = 1.0f / kTwoPi;
constexpr float kRadiansToPhase01 = kInv2Pi;

constexpr uint64_t kPitchBatch = 16;
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

} // namespace


void ModularInstrument::setPatch(const synth::patch::Patch& patch) noexcept {
    if (patch.kind == synth::patch::PatchKind::DrumKit) {
        return;
    }

    name_ = patch.name;
    maxVoices_ = patch.maxVoices;
    sameNoteMode_ = patch.sameNoteMode;
    panSpread_ = patch.panSpread;
    outputGain_ = patch.outputGain;
    fmIndexScale_ = patch.fmIndexScale;
    fmRatioScale_ = patch.fmRatioScale;

    oscCount_ = patch.oscCount;
    for (uint8_t i = 0; i < MAX_OSCS; ++i) {
        const auto& src = patch.oscs[i];
        OscSlot& dst = oscs_[i];
        dst.wave = toOscWave(src.shape);
        dst.pitchRatio = src.pitchRatio;
        dst.pitchSemitones = src.pitchSemitones;
        dst.pitchCents = src.pitchCents;
        dst.pulseWidth = src.pulseWidth;
        dst.level = src.level;
        dst.fmFrom = src.fmFrom;
        dst.fmIndex = src.fmIndex;
        dst.fmFeedback = src.fmFeedback;
        dst.modEnvIndex = src.modEnvIndex;
        dst.velToFmIndex = src.velToFmIndex;
        dst.vibratoLfoIndex = src.vibratoLfoIndex;
        dst.vibratoSemitones = src.vibratoSemitones;
        dst.isFmModulator = src.isFmModulator;
    }

    envCount_ = patch.envCount;
    lfoCount_ = patch.lfoCount;
    for (uint8_t i = 0; i < MAX_LFOS; ++i) {
        lfoStartPhase_[i] = patch.lfos[i].perVoicePhase;
    }

    filterCount_ = patch.filterCount;
    for (uint8_t i = 0; i < MAX_FILTERS; ++i) {
        const auto& src = patch.filters[i];
        FilterSlot& dst = filters_[i];
        dst.mode = toSvfMode(src.mode);
        dst.cutoffHz = src.cutoffHz;
        dst.cutoffPeakHz = src.cutoffPeakHz;
        dst.resonance = src.resonance;
        dst.envIndex = src.envIndex;
        dst.velToCutoffHz = src.velToCutoffHz;
        dst.lfoIndex = src.lfoIndex;
        dst.lfoCutoffHz = src.lfoCutoffHz;
    }

    switch (patch.noise.shape) {
        case synth::patch::NoiseShape::Off:     noiseKind_ = NoiseKind::Off;     break;
        case synth::patch::NoiseShape::White:   noiseKind_ = NoiseKind::White;   break;
        case synth::patch::NoiseShape::Pinkish: noiseKind_ = NoiseKind::Pinkish; break;
    }
    noiseLevel_ = patch.noise.level;

    driveEnabled_ = patch.drive.enabled;
    outDrive_.preGain = patch.drive.preGain;
    outDrive_.postGain = patch.drive.postGain;

    resonatorEnabled_ = patch.resonator.enabled;
    resoFeedback_ = patch.resonator.feedback;
    resoDampLowKey_ = patch.resonator.dampLowKey;
    resoDampHighKey_ = patch.resonator.dampHighKey;
    resoBodyMix_ = patch.resonator.bodyMix;
    resoBodyHpHz_ = patch.resonator.bodyHpHz;
    resoOutputLpHzBase_ = patch.resonator.outputLpHzBase;
    resoOutputLpHzVel_ = patch.resonator.outputLpHzVel;
    resoHarmAmount_ = patch.resonator.harmAmount;

    exciterEnabled_ = patch.exciter.enabled;
    clickLevelBase_ = patch.exciter.clickLevelBase;
    clickLevelVel_ = patch.exciter.clickLevelVel;
    clickDecay_ = patch.exciter.clickDecay;
    burstLevelBase_ = patch.exciter.noiseLevelBase;
    burstLevelVel_ = patch.exciter.noiseLevelVel;
    burstDecay_ = patch.exciter.noiseDecay;

    // Bake module config straight onto every voice's modules. After this the
    // Patch can be discarded; per-voice modules carry everything they need.
    const float sr = engine::gAudio.sampleRate;
    for (uint8_t v = 0; v < MAX_VOICES; ++v) {
        VoiceState& vs = voices_[v];

        for (uint8_t i = 0; i < MAX_ENVS; ++i) {
            auto& e = vs.modEnvs[i];
            e.attack_s  = patch.envs[i].attack;
            e.decay_s   = patch.envs[i].decay;
            e.sustain   = patch.envs[i].sustain;
            e.release_s = patch.envs[i].release;
            e.refresh();
            e.reset();
        }

        for (uint8_t i = 0; i < MAX_LFOS; ++i) {
            auto& l = vs.lfos[i];
            l.wave = toLfoWave(patch.lfos[i].shape);
            l.rateHz = patch.lfos[i].rateHz
                     + patch.lfos[i].perVoiceRateSpread * static_cast<float>(v);
            float ph = lfoStartPhase_[i] * static_cast<float>(v);
            ph -= floorf(ph);
            l.phase = ph;
            l.shState = 0x13579BDFu;
            l.shValue = 0.0f;
        }

        for (uint8_t i = 0; i < MAX_FILTERS; ++i) {
            auto& f = vs.filters[i];
            f.reset();
            f.mode = filters_[i].mode;
            f.set(filters_[i].cutoffHz, filters_[i].resonance);
        }

        vs.pink.color = patch.noise.color;
    }

    if (resonatorEnabled_) {
        std::memset(resoPool_, 0, sizeof(resoPool_));
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
    if (resonatorEnabled_) {
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

    if (panSpread_ > 0.0f) {
        const float t = static_cast<float>(static_cast<int>(v) & 7) / 7.0f - 0.5f;
        s.pan = synth::dsp::clamp(t * 2.0f * panSpread_, -panSpread_, panSpread_);
    } else {
        s.pan = 0.0f;
    }

    for (uint8_t i = 0; i < oscCount_ && i < MAX_OSCS; ++i) {
        s.oscPhase[i] = (i == 1) ? 0.5f : 0.0f;
        s.oscFb[i] = 0.0f;
        s.oscOut[i] = 0.0f;

        const auto& od = oscs_[i];
        const float ratio = od.pitchRatio * (od.isFmModulator ? fmRatioScale_ : 1.0f);
        const float octaves = (od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f);
        const float f = ctx.frequency * ratio * synth::dsp::exp2Fast(octaves);
        s.oscSawTbl[i] = synth::dsp::sawTableIdx(f > 1.0f ? f : 1.0f);
    }

    for (uint8_t i = 1; i < envCount_ && i < MAX_ENVS; ++i) {
        s.modEnvs[i].reset();
        s.modEnvs[i].noteOn();
    }

    for (uint8_t i = 0; i < lfoCount_ && i < MAX_LFOS; ++i) {
        s.lfos[i].phase = wrap01Trunc(lfoStartPhase_[i] * static_cast<float>(v));
    }

    for (uint8_t i = 0; i < filterCount_ && i < MAX_FILTERS; ++i) {
        auto& f = s.filters[i];
        f.reset();
        const float cutoff = filters_[i].cutoffHz + filters_[i].velToCutoffHz * ctx.velocity;
        f.set(cutoff, filters_[i].resonance);
    }

    if (exciterEnabled_) {
        const float clk = clickLevelBase_ + clickLevelVel_ * ctx.velocity;
        const float bst = burstLevelBase_ + burstLevelVel_ * ctx.velocity;
        s.click.trigger(clk, clickDecay_);
        s.burst.trigger(bst, burstDecay_);
    }

    if (resonatorEnabled_) {
        const float f = (ctx.frequency < 20.0f) ? 20.0f : ctx.frequency;
        auto delay = static_cast<std::size_t>(sr / f);
        if (delay < 2) delay = 2;
        if (delay >= kResonatorBufSamples) delay = kResonatorBufSamples - 1;

        s.resoDelay = delay;
        s.resoIdx = 0;
        s.resoLp = 0.0f;
        s.resoFb = resoFeedback_;
        s.resoDamp = resoDampLowKey_ + (resoDampHighKey_ - resoDampLowKey_) * s.keyT;
        std::memset(voiceResoBuf_(v), 0, sizeof(float) * kResonatorBufSamples);

        s.bodyHp.reset();
        s.bodyHp.mode = synth::modules::SvfMode::HighPass;
        s.bodyHp.set(resoBodyHpHz_, 0.0f);

        s.outLp.reset(0.0f);
        const float tone = resoOutputLpHzBase_ + resoOutputLpHzVel_ * ctx.velocity;
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
    const float sr = engine::gAudio.sampleRate;
    const float invSr = engine::gAudio.invSampleRate;

    const bool resoOn = resonatorEnabled_;
    const bool driveOn = driveEnabled_;
    float* const resoBuf = resoOn ? voiceResoBuf_(voiceIdx) : nullptr;

    bool vibPitch = false;
    for (uint8_t oi = 0; oi < oscCount_ && oi < MAX_OSCS; ++oi) {
        const auto& od = oscs_[oi];
        if (od.vibratoLfoIndex >= 0
            && od.vibratoLfoIndex < lfoCount_
            && od.vibratoSemitones != 0.0f) {
            vibPitch = true;
            break;
        }
    }

    float oscFreq[MAX_OSCS] = {};
    float oscDp[MAX_OSCS] = {};
    float lfoBuf[MAX_LFOS] = {};
    float vibLfoBuf[MAX_LFOS] = {};

    recomputeOscDp_(s, vibLfoBuf, invSr, oscFreq, oscDp);

    uint64_t batchTimer = 0;

    for (uint64_t i = 0; i < n; ++i) {
        for (uint8_t li = 0; li < lfoCount_ && li < MAX_LFOS; ++li) {
            lfoBuf[li] = s.lfos[li].tick();
        }
        for (uint8_t ei = 1; ei < envCount_ && ei < MAX_ENVS; ++ei) {
            s.modEnvs[ei].tick(invSr);
        }

        if (vibPitch) {
            if (batchTimer == 0) {
                for (uint8_t li = 0; li < MAX_LFOS; ++li) vibLfoBuf[li] = lfoBuf[li];
                recomputeOscDp_(s, vibLfoBuf, invSr, oscFreq, oscDp);
            }
            batchTimer = (batchTimer + 1) & (kPitchBatch - 1);
        }

        float oscMix = 0.0f;
        for (uint8_t oi = 0; oi < oscCount_ && oi < MAX_OSCS; ++oi) {
            const float y = renderOneOsc_(s, oi, oscDp[oi]);
            const float lvl = oscs_[oi].level;
            if (lvl != 0.0f) oscMix += y * lvl;
        }

        float noiseSample = 0.0f;
        if (noiseKind_ == NoiseKind::White) {
            noiseSample = s.white.tick() * noiseLevel_;
        } else if (noiseKind_ == NoiseKind::Pinkish) {
            noiseSample = s.pink.tick() * noiseLevel_;
        }

        const float resoOut = (resoOn && resoBuf) ? tickResonator_(s, resoBuf) : 0.0f;

        float sig;
        if (resoOn) {
            const float harmGain = 0.20f + 0.55f * s.vel;
            sig = resoOut * resoBodyMix_
                + oscMix * resoHarmAmount_ * harmGain
                + noiseSample;
            sig = s.bodyHp.tick(sig);
            sig = s.outLp.tick(sig);
        } else {
            sig = oscMix + noiseSample;
        }

        if ((i & (kPitchBatch - 1)) == 0) {
            updateFilterCutoffs_(s, lfoBuf);
        }
        for (uint8_t fi = 0; fi < filterCount_ && fi < MAX_FILTERS; ++fi) {
            sig = s.filters[fi].tick(sig);
        }

        if (driveOn) {
            sig = outDrive_.tick(sig);
        }

        out[i] += sig * outputGain_;
    }
}

float ModularInstrument::renderOneOsc_(VoiceState& s, const uint8_t oi, const float dp) const noexcept {
    const auto& od = oscs_[oi];

    const float ph = wrap01(s.oscPhase[oi] + dp);
    s.oscPhase[oi] = ph;

    float phaseMod = ph;
    if (od.fmFeedback != 0.0f) {
        phaseMod += s.oscFb[oi] * od.fmFeedback * kRadiansToPhase01;
    }
    if (od.fmFrom >= 0 && od.fmFrom < oscCount_) {
        float idx = od.fmIndex;
        if (oscs_[od.fmFrom].isFmModulator) idx *= fmIndexScale_;
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
        synth::modules::sampleOscWave(od.wave, phaseMod, s.oscSawTbl[oi], od.pulseWidth);
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
                                             const float lfoBuf[]) const noexcept {
    for (uint8_t fi = 0; fi < filterCount_ && fi < MAX_FILTERS; ++fi) {
        const auto& fd = filters_[fi];

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

void ModularInstrument::recomputeOscDp_(const VoiceState& s, const float vibSemis[MAX_LFOS],
                                         const float invSr, float oscFreq[MAX_OSCS],
                                         float oscDp[MAX_OSCS]) const noexcept {
    for (uint8_t i = 0; i < oscCount_ && i < MAX_OSCS; ++i) {
        const auto& od = oscs_[i];
        const float ratio = od.pitchRatio * (od.isFmModulator ? fmRatioScale_ : 1.0f);

        float vibOctaves = 0.0f;
        if (od.vibratoLfoIndex >= 0
            && static_cast<uint8_t>(od.vibratoLfoIndex) < lfoCount_) {
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
    return resoPool_ + static_cast<std::size_t>(v) * kResonatorBufSamples;
}

float ModularInstrument::voicePan(const uint8_t v) const noexcept {
    if (v >= MAX_VOICES) return 0.0f;
    return voices_[v].pan;
}

bool ModularInstrument::setParam(const uint16_t paramId, const float value) noexcept {
    switch (paramId) {
        case Params::FmModIndex: {
            const float v = synth::dsp::clamp(value, 0.0f, 1.0f);
            fmIndexScale_ = v * 2.0f;
            return true;
        }
        case Params::FmModRatio: {
            const float v = synth::dsp::clamp(value, 0.0f, 1.0f);
            fmRatioScale_ = 0.25f + v * (4.0f - 0.25f);
            return true;
        }
        default:
            break;
    }
    return false;
}
