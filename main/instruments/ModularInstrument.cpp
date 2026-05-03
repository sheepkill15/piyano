#include "ModularInstrument.h"

#include "workstation/Params.h"
#include "engine/AudioContext.h"

#include "synth/dsp/Approx.h"
#include "synth/dsp/WaveTables.h"

#include <cmath>
#include <cstring>

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kInv2Pi = 1.0f / kTwoPi;

inline float clampf(float x, float lo, float hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float oscShape(synth::modules::OscWave w, float phase01, int sawTbl, float pw) noexcept {
    using W = synth::modules::OscWave;
    switch (w) {
        case W::Sine:     return synth::dsp::sineLU(phase01);
        case W::Saw:      return synth::dsp::sawLU(sawTbl, phase01);
        case W::Triangle: {
            const float s = 2.0f * phase01 - 1.0f;
            return 2.0f * (fabsf(s) - 0.5f);
        }
        case W::Pulse: {
            const float p = clampf(pw, 0.01f, 0.99f);
            return (phase01 < p) ? 1.0f : -1.0f;
        }
    }
    return 0.0f;
}

inline synth::modules::OscWave toOscWave(synth::patch::OscShape s) noexcept {
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

inline synth::modules::SvfMode toSvfMode(synth::patch::FilterMode m) noexcept {
    switch (m) {
        case synth::patch::FilterMode::LowPass:  return synth::modules::SvfMode::LowPass;
        case synth::patch::FilterMode::BandPass: return synth::modules::SvfMode::BandPass;
        case synth::patch::FilterMode::HighPass: return synth::modules::SvfMode::HighPass;
    }
    return synth::modules::SvfMode::LowPass;
}

inline synth::modules::LfoWave toLfoWave(synth::patch::LfoShape s) noexcept {
    switch (s) {
        case synth::patch::LfoShape::Sine:       return synth::modules::LfoWave::Sine;
        case synth::patch::LfoShape::Triangle:   return synth::modules::LfoWave::Triangle;
        case synth::patch::LfoShape::Square:     return synth::modules::LfoWave::Square;
        case synth::patch::LfoShape::SampleHold: return synth::modules::LfoWave::SampleHold;
    }
    return synth::modules::LfoWave::Sine;
}

} // namespace

void ModularInstrument::recomputeOscDp_(VoiceState& s, const float vibSemis[MAX_LFOS],
                                         float invSr, float oscFreq[MAX_OSCS],
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

float* ModularInstrument::voiceResoBuf_(uint8_t v) noexcept {
    return resoPool_ + static_cast<std::size_t>(v) * kResonatorBufSamples;
}

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
        const auto& od = patch.oscs[i];
        OscSlot& s = oscs_[i];
        s.wave = toOscWave(od.shape);
        s.pitchRatio = od.pitchRatio;
        s.pitchSemitones = od.pitchSemitones;
        s.pitchCents = od.pitchCents;
        s.pulseWidth = od.pulseWidth;
        s.level = od.level;
        s.fmFrom = od.fmFrom;
        s.fmIndex = od.fmIndex;
        s.fmFeedback = od.fmFeedback;
        s.modEnvIndex = od.modEnvIndex;
        s.velToFmIndex = od.velToFmIndex;
        s.vibratoLfoIndex = od.vibratoLfoIndex;
        s.vibratoSemitones = od.vibratoSemitones;
        s.isFmModulator = od.isFmModulator;
    }

    envCount_ = patch.envCount;
    lfoCount_ = patch.lfoCount;
    for (uint8_t i = 0; i < MAX_LFOS; ++i) {
        lfoStartPhase_[i] = patch.lfos[i].perVoicePhase;
    }

    filterCount_ = patch.filterCount;
    for (uint8_t i = 0; i < MAX_FILTERS; ++i) {
        const auto& fd = patch.filters[i];
        FilterSlot& fs = filters_[i];
        fs.mode = toSvfMode(fd.mode);
        fs.cutoffHz = fd.cutoffHz;
        fs.cutoffPeakHz = fd.cutoffPeakHz;
        fs.resonance = fd.resonance;
        fs.envIndex = fd.envIndex;
        fs.velToCutoffHz = fd.velToCutoffHz;
        fs.lfoIndex = fd.lfoIndex;
        fs.lfoCutoffHz = fd.lfoCutoffHz;
    }

    switch (patch.noise.shape) {
        case synth::patch::NoiseShape::Off:     noiseKind_ = NoiseKind::Off;     break;
        case synth::patch::NoiseShape::White:   noiseKind_ = NoiseKind::White;   break;
        case synth::patch::NoiseShape::Pinkish: noiseKind_ = NoiseKind::Pinkish; break;
    }
    noiseLevel_ = patch.noise.level;

    driveEnabled_ = patch.drive.enabled;
    drivePreGain_ = patch.drive.preGain;
    drivePostGain_ = patch.drive.postGain;

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
    for (uint8_t v = 0; v < MAX_VOICES; ++v) {
        VoiceState& vs = voices_[v];
        for (uint8_t i = 0; i < MAX_ENVS; ++i) {
            auto& e = vs.modEnvs[i];
            e.attack_s = patch.envs[i].attack;
            e.decay_s = patch.envs[i].decay;
            e.sustain = patch.envs[i].sustain;
            e.release_s = patch.envs[i].release;
            e.reset();
        }
        for (uint8_t i = 0; i < MAX_LFOS; ++i) {
            auto& l = vs.lfos[i];
            l.wave = toLfoWave(patch.lfos[i].shape);
            l.rateHz = patch.lfos[i].rateHz
                     + patch.lfos[i].perVoiceRateSpread * static_cast<float>(v);
            l.phase = lfoStartPhase_[i] * static_cast<float>(v);
            l.phase -= floorf(l.phase);
            l.shState = 0x13579BDFu;
            l.shValue = 0.0f;
        }
        for (uint8_t i = 0; i < MAX_FILTERS; ++i) {
            auto& f = vs.filters[i];
            f.reset();
            f.mode = filters_[i].mode;
            f.set(filters_[i].cutoffHz, filters_[i].resonance, engine::gAudio.sampleRate);
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

void ModularInstrument::onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept {
    if (v >= MAX_VOICES) return;
    VoiceState& s = voices_[v];
    s.active = true;
    s.freq = ctx.frequency;
    s.vel = ctx.velocity;
    s.note = ctx.note;
    s.keyT = clampf((static_cast<float>(ctx.note) - 21.0f) / 88.0f, 0.0f, 1.0f);

    if (panSpread_ > 0.0f) {
        const float t = (static_cast<int>(v) & 7) / 7.0f - 0.5f;
        s.pan = clampf(t * 2.0f * panSpread_, -panSpread_, panSpread_);
    } else {
        s.pan = 0.0f;
    }

    for (uint8_t i = 0; i < oscCount_ && i < MAX_OSCS; ++i) {
        s.oscPhase[i] = (i == 1) ? 0.5f : 0.0f;
        s.oscFb[i] = 0.0f;
        s.oscOut[i] = 0.0f;
        const auto& od = oscs_[i];
        const float ratio = od.pitchRatio * (od.isFmModulator ? fmRatioScale_ : 1.0f);
        const float f = ctx.frequency * ratio
                      * synth::dsp::exp2Fast((od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f));
        s.oscSawTbl[i] = synth::dsp::sawTableIdx(f > 1.0f ? f : 1.0f);
    }

    for (uint8_t i = 1; i < envCount_ && i < MAX_ENVS; ++i) {
        s.modEnvs[i].reset();
        s.modEnvs[i].noteOn();
    }

    for (uint8_t i = 0; i < lfoCount_ && i < MAX_LFOS; ++i) {
        auto& l = s.lfos[i];
        l.phase = lfoStartPhase_[i] * static_cast<float>(v);
        l.phase -= floorf(l.phase);
    }

    for (uint8_t i = 0; i < filterCount_ && i < MAX_FILTERS; ++i) {
        auto& f = s.filters[i];
        f.reset();
        const float cutoff = filters_[i].cutoffHz + filters_[i].velToCutoffHz * ctx.velocity;
        f.set(cutoff, filters_[i].resonance, engine::gAudio.sampleRate);
    }

    if (exciterEnabled_) {
        const float clk = clickLevelBase_ + clickLevelVel_ * ctx.velocity;
        const float bst = burstLevelBase_ + burstLevelVel_ * ctx.velocity;
        s.click.trigger(clk, clickDecay_);
        s.burst.trigger(bst, burstDecay_);
    }

    if (resonatorEnabled_) {
        const float f = (ctx.frequency < 20.0f) ? 20.0f : ctx.frequency;
        std::size_t delay = static_cast<std::size_t>(engine::gAudio.sampleRate / f);
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
        s.bodyHp.set(resoBodyHpHz_, 0.0f, engine::gAudio.sampleRate);
        s.outLp.reset(0.0f);
        const float tone = resoOutputLpHzBase_ + resoOutputLpHzVel_ * ctx.velocity;
        s.outLp.setCutoffHz(tone, engine::gAudio.sampleRate);
    }
}

void ModularInstrument::onVoiceStop(uint8_t v) noexcept {
    if (v >= MAX_VOICES) return;
    VoiceState& s = voices_[v];
    for (uint8_t i = 1; i < MAX_ENVS; ++i) s.modEnvs[i].noteOff();
}

void ModularInstrument::renderAddVoice(uint8_t v, float* out, uint64_t n) noexcept {
    if (v >= MAX_VOICES || !out || n == 0) return;
    renderSynth_(v, out, n);
}

void ModularInstrument::renderSynth_(uint8_t voiceIdx, float* out, uint64_t n) noexcept {
    VoiceState& s = voices_[voiceIdx];
    const float sr = engine::gAudio.sampleRate;
    const float invSr = engine::gAudio.invSampleRate;
    const float dtSr = engine::gAudio.invSampleRate;
    const float fbScale = kInv2Pi;     // converts radians-feedback to phase units
    const float idxPhScale = kInv2Pi;  // converts radians-FM index to phase units

    const bool resoOn = resonatorEnabled_;
    const bool driveOn = driveEnabled_;
    float* resoBuf = resoOn ? voiceResoBuf_(voiceIdx) : nullptr;

    bool vibPitch = false;
    for (uint8_t oi = 0; oi < oscCount_ && oi < MAX_OSCS; ++oi) {
        const auto& od = oscs_[oi];
        if (od.vibratoLfoIndex >= 0
            && static_cast<uint8_t>(od.vibratoLfoIndex) < lfoCount_
            && od.vibratoSemitones != 0.0f) {
            vibPitch = true;
            break;
        }
    }

    // Pre-tick LFOs/envelopes get computed inside the per-sample loop, but we
    // recompute pitch-dependent quantities every kPitchBatch samples to avoid
    // a per-sample exp2 — only when an oscillator actually uses LFO vibrato.
    constexpr uint64_t kPitchBatch = 16;

    float oscFreq[MAX_OSCS] = {};
    float oscDp[MAX_OSCS] = {};

    float vibLfoBuf[MAX_LFOS] = {};
    float lfoBuf[MAX_LFOS] = {};

    for (uint8_t i = 0; i < lfoCount_ && i < MAX_LFOS; ++i) {
        lfoBuf[i] = 0.0f;
        vibLfoBuf[i] = 0.0f;
    }
    recomputeOscDp_(s, vibLfoBuf, invSr, oscFreq, oscDp);

    uint64_t batchTimer = 0;

    for (uint64_t i = 0; i < n; ++i) {
        // 1) Tick LFOs/envelopes.
        for (uint8_t li = 0; li < lfoCount_ && li < MAX_LFOS; ++li) {
            lfoBuf[li] = s.lfos[li].tick();
        }
        for (uint8_t ei = 1; ei < envCount_ && ei < MAX_ENVS; ++ei) {
            s.modEnvs[ei].tick(dtSr);
        }

        // 2) Refresh pitch when vibrato modulates osc phase increment.
        if (vibPitch) {
            if (batchTimer == 0) {
                for (uint8_t li = 0; li < MAX_LFOS; ++li) vibLfoBuf[li] = lfoBuf[li];
                recomputeOscDp_(s, vibLfoBuf, invSr, oscFreq, oscDp);
            }
            batchTimer = (batchTimer + 1) & (kPitchBatch - 1);
        }

        // 3) Render oscillators in order.
        for (uint8_t oi = 0; oi < oscCount_ && oi < MAX_OSCS; ++oi) {
            const auto& od = oscs_[oi];
            float ph = s.oscPhase[oi] + oscDp[oi];
            ph -= floorf(ph);
            s.oscPhase[oi] = ph;

            float phaseMod = ph;
            if (od.fmFeedback != 0.0f) {
                phaseMod += s.oscFb[oi] * od.fmFeedback * fbScale;
            }
            if (od.fmFrom >= 0 && od.fmFrom < oscCount_) {
                float idx = od.fmIndex;
                const auto& src = oscs_[od.fmFrom];
                if (src.isFmModulator) idx *= fmIndexScale_;
                if (od.modEnvIndex >= 1 && od.modEnvIndex < MAX_ENVS) {
                    idx *= s.modEnvs[od.modEnvIndex].level;
                }
                if (od.velToFmIndex > 0.0f) {
                    const float velMix = od.velToFmIndex;
                    idx *= (1.0f - velMix) + velMix * (0.25f + 0.75f * s.vel);
                }
                phaseMod += s.oscOut[od.fmFrom] * idx * idxPhScale;
            }
            phaseMod -= floorf(phaseMod);

            const float y = oscShape(od.wave, phaseMod, s.oscSawTbl[oi], od.pulseWidth);
            s.oscOut[oi] = y;
            s.oscFb[oi] = y;
        }

        // 4) Mix oscillators.
        float oscMix = 0.0f;
        for (uint8_t oi = 0; oi < oscCount_ && oi < MAX_OSCS; ++oi) {
            const float lvl = oscs_[oi].level;
            if (lvl != 0.0f) oscMix += s.oscOut[oi] * lvl;
        }

        // 5) Noise.
        float noiseSample = 0.0f;
        if (noiseKind_ == NoiseKind::White) {
            noiseSample = s.white.tick() * noiseLevel_;
        } else if (noiseKind_ == NoiseKind::Pinkish) {
            noiseSample = s.pink.tick() * noiseLevel_;
        }

        // 6) Resonator (Karplus-strong style body) + exciter feeds it.
        float resoOut = 0.0f;
        if (resoOn && resoBuf) {
            const float exc = s.click.tick() + s.burst.tick();
            const std::size_t read = (s.resoIdx + kResonatorBufSamples - s.resoDelay) & kResonatorMask;
            const float body = resoBuf[read];
            s.resoLp += (body - s.resoLp) * s.resoDamp;
            resoBuf[s.resoIdx] = exc + s.resoLp * s.resoFb;
            s.resoIdx = (s.resoIdx + 1) & kResonatorMask;
            resoOut = body;
        }

        // 7) Trunk signal.
        float sig;
        if (resoOn) {
            const float a = 0.20f + 0.55f * s.vel;
            sig = resoOut * resoBodyMix_
                + oscMix * resoHarmAmount_ * a
                + noiseSample;
            sig = s.bodyHp.tick(sig);
            sig = s.outLp.tick(sig);
        } else {
            sig = oscMix + noiseSample;
        }

        // 8) Filter chain.
        for (uint8_t fi = 0; fi < filterCount_ && fi < MAX_FILTERS; ++fi) {
            const auto& fd = filters_[fi];
            // Update cutoff cheaply every kPitchBatch (also handles env sweep).
            if ((i & (kPitchBatch - 1)) == 0) {
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
                s.filters[fi].set(cut, fd.resonance, sr);
            }
            sig = s.filters[fi].tick(sig);
        }

        // 9) Drive.
        if (driveOn) {
            sig = synth::dsp::tanhFast(sig * drivePreGain_) * drivePostGain_;
        }

        out[i] += sig * outputGain_;
    }
}

float ModularInstrument::voicePan(uint8_t v) const noexcept {
    if (v >= MAX_VOICES) return 0.0f;
    return voices_[v].pan;
}

bool ModularInstrument::setParam(uint16_t paramId, float value) noexcept {
    switch (paramId) {
        case Params::FmModIndex: {
            const float v = clampf(value, 0.0f, 1.0f);
            fmIndexScale_ = v * 2.0f; // 0..2 (1.0 = nominal)
            return true;
        }
        case Params::FmModRatio: {
            const float v = clampf(value, 0.0f, 1.0f);
            fmRatioScale_ = 0.25f + v * (4.0f - 0.25f);
            return true;
        }
        default:
            break;
    }
    return false;
}
