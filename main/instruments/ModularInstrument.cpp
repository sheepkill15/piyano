#include "ModularInstrument.h"

#include "workstation/Params.h"
#include "engine/AudioContext.h"

#include "synth/dsp/Approx.h"
#include "synth/dsp/WaveTables.h"

#include <cmath>
#include <cstring>
#include <new>

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kInv2Pi = 1.0f / kTwoPi;

inline float clampf(float x, float lo, float hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float oscShape(synth::patch::OscShape shape, float phase01, int sawTbl, float pw) noexcept {
    using S = synth::patch::OscShape;
    switch (shape) {
        case S::Sine:     return synth::dsp::sineLU(phase01);
        case S::Saw:      return synth::dsp::sawLU(sawTbl, phase01);
        case S::Triangle: {
            const float s = 2.0f * phase01 - 1.0f;
            return 2.0f * (fabsf(s) - 0.5f);
        }
        case S::Pulse: {
            const float p = clampf(pw, 0.01f, 0.99f);
            return (phase01 < p) ? 1.0f : -1.0f;
        }
    }
    return 0.0f;
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

ModularInstrument::~ModularInstrument() noexcept {
    freeResonatorBuffer_();
}

void ModularInstrument::ensureResonatorBuffer_() noexcept {
    if (!patch_.resonator.enabled) {
        freeResonatorBuffer_();
        return;
    }
    if (resoBuf_) return;
    const std::size_t n = static_cast<std::size_t>(MAX_VOICES) * kResonatorBufSamples;
    resoBuf_ = new (std::nothrow) float[n];
    if (resoBuf_) std::memset(resoBuf_, 0, sizeof(float) * n);
}

void ModularInstrument::freeResonatorBuffer_() noexcept {
    delete[] resoBuf_;
    resoBuf_ = nullptr;
}

float* ModularInstrument::voiceResoBuf_(uint8_t v) noexcept {
    if (!resoBuf_) return nullptr;
    return resoBuf_ + static_cast<std::size_t>(v) * kResonatorBufSamples;
}

void ModularInstrument::setPatch(const synth::patch::Patch& patch) noexcept {
    patch_ = patch;
    ensureResonatorBuffer_();
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
    for (uint8_t i = 0; i < synth::patch::MAX_OSCS; ++i) {
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
    if (float* b = voiceResoBuf_(v)) {
        std::memset(b, 0, sizeof(float) * kResonatorBufSamples);
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

    if (patch_.kind == synth::patch::PatchKind::DrumKit) {
        const auto& kit = patch_.drumKit;
        synth::patch::DrumPadKind kind = kit.fallback;
        float pan = kit.fallbackPan;
        float velScale = kit.fallbackVelScale;
        for (uint8_t i = 0; i < kit.padCount; ++i) {
            if (kit.pads[i].note == ctx.note) {
                kind = kit.pads[i].kind;
                pan = kit.pads[i].pan;
                velScale = kit.pads[i].velScale;
                break;
            }
        }
        s.drumKind = kind;
        s.pan = pan;
        const float vel = clampf(ctx.velocity * velScale, 0.0f, 1.0f);
        switch (kind) {
            case synth::patch::DrumPadKind::Kick:  s.kick.trigger(vel);  break;
            case synth::patch::DrumPadKind::Snare: s.snare.trigger(vel); break;
            case synth::patch::DrumPadKind::Hat:   s.hat.trigger(vel);   break;
        }
        return;
    }

    // Per-voice pan from patch spread (-spread..+spread alternating).
    if (patch_.panSpread > 0.0f) {
        const float t = (static_cast<int>(v) & 7) / 7.0f - 0.5f;
        s.pan = clampf(t * 2.0f * patch_.panSpread, -patch_.panSpread, patch_.panSpread);
    } else {
        s.pan = 0.0f;
    }

    for (uint8_t i = 0; i < patch_.oscCount && i < synth::patch::MAX_OSCS; ++i) {
        s.oscPhase[i] = (i == 1) ? 0.5f : 0.0f;
        s.oscFb[i] = 0.0f;
        s.oscOut[i] = 0.0f;
        const auto& od = patch_.oscs[i];
        const float ratio = od.pitchRatio * (od.isFmModulator ? patch_.fmRatioScale : 1.0f);
        const float f = ctx.frequency * ratio
                      * synth::dsp::exp2Fast((od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f));
        s.oscSawTbl[i] = synth::dsp::sawTableIdx(f > 1.0f ? f : 1.0f);
    }

    for (uint8_t i = 1; i < patch_.envCount && i < synth::patch::MAX_ENVS; ++i) {
        auto& e = s.modEnvs[i];
        e = synth::modules::Adsr{};
        e.attack_s = patch_.envs[i].attack;
        e.decay_s = patch_.envs[i].decay;
        e.sustain = patch_.envs[i].sustain;
        e.release_s = patch_.envs[i].release;
        e.noteOn();
    }

    for (uint8_t i = 0; i < patch_.lfoCount && i < synth::patch::MAX_LFOS; ++i) {
        auto& l = s.lfos[i];
        l = synth::modules::Lfo{};
        l.wave = toLfoWave(patch_.lfos[i].shape);
        l.rateHz = patch_.lfos[i].rateHz + patch_.lfos[i].perVoiceRateSpread * static_cast<float>(v);
        l.phase = patch_.lfos[i].perVoicePhase * static_cast<float>(v);
        l.phase -= floorf(l.phase);
    }

    for (uint8_t i = 0; i < patch_.filterCount && i < synth::patch::MAX_FILTERS; ++i) {
        auto& f = s.filters[i];
        f = synth::modules::Svf{};
        f.mode = toSvfMode(patch_.filters[i].mode);
        const float cutoff = patch_.filters[i].cutoffHz
                           + patch_.filters[i].velToCutoffHz * ctx.velocity;
        f.set(cutoff, patch_.filters[i].resonance, engine::gAudio.sampleRate);
    }

    if (patch_.noise.shape == synth::patch::NoiseShape::Pinkish) {
        s.pink.color = patch_.noise.color;
    }

    if (patch_.exciter.enabled) {
        const float clk = patch_.exciter.clickLevelBase + patch_.exciter.clickLevelVel * ctx.velocity;
        const float bst = patch_.exciter.noiseLevelBase + patch_.exciter.noiseLevelVel * ctx.velocity;
        s.click.trigger(clk, patch_.exciter.clickDecay);
        s.burst.trigger(bst, patch_.exciter.noiseDecay);
    }

    if (patch_.resonator.enabled) {
        const float f = (ctx.frequency < 20.0f) ? 20.0f : ctx.frequency;
        std::size_t delay = static_cast<std::size_t>(engine::gAudio.sampleRate / f);
        if (delay < 2) delay = 2;
        if (delay >= kResonatorBufSamples) delay = kResonatorBufSamples - 1;
        s.resoDelay = delay;
        s.resoIdx = 0;
        s.resoLp = 0.0f;
        s.resoFb = patch_.resonator.feedback;
        s.resoDamp = patch_.resonator.dampLowKey
                   + (patch_.resonator.dampHighKey - patch_.resonator.dampLowKey) * s.keyT;
        if (float* b = voiceResoBuf_(v)) {
            std::memset(b, 0, sizeof(float) * kResonatorBufSamples);
        }
        s.bodyHp = synth::modules::Svf{};
        s.bodyHp.mode = synth::modules::SvfMode::HighPass;
        s.bodyHp.set(patch_.resonator.bodyHpHz, 0.0f, engine::gAudio.sampleRate);
        s.outLp.reset(0.0f);
        const float tone = patch_.resonator.outputLpHzBase
                         + patch_.resonator.outputLpHzVel * ctx.velocity;
        s.outLp.setCutoffHz(tone, engine::gAudio.sampleRate);
    }
}

void ModularInstrument::onVoiceStop(uint8_t v) noexcept {
    if (v >= MAX_VOICES) return;
    VoiceState& s = voices_[v];
    for (uint8_t i = 1; i < synth::patch::MAX_ENVS; ++i) s.modEnvs[i].noteOff();
}

void ModularInstrument::renderAddVoice(uint8_t v, float* out, uint64_t n) noexcept {
    if (v >= MAX_VOICES || !out || n == 0) return;
    VoiceState& s = voices_[v];
    if (patch_.kind == synth::patch::PatchKind::DrumKit) {
        renderDrum_(s, out, n);
    } else {
        renderSynth_(s, out, n);
    }
}

void ModularInstrument::renderDrum_(VoiceState& s, float* out, uint64_t n) noexcept {
    switch (s.drumKind) {
        case synth::patch::DrumPadKind::Kick:
            for (uint64_t i = 0; i < n; ++i) out[i] += s.kick.tick();
            break;
        case synth::patch::DrumPadKind::Snare:
            for (uint64_t i = 0; i < n; ++i) out[i] += s.snare.tick();
            break;
        case synth::patch::DrumPadKind::Hat:
            for (uint64_t i = 0; i < n; ++i) out[i] += s.hat.tick();
            break;
    }
}

void ModularInstrument::renderSynth_(VoiceState& s, float* out, uint64_t n) noexcept {
    const synth::patch::Patch& p = patch_;
    const float sr = engine::gAudio.sampleRate;
    const float invSr = engine::gAudio.invSampleRate;
    const float dtSr = engine::gAudio.invSampleRate;
    const float fbScale = kInv2Pi;     // converts radians-feedback to phase units
    const float idxPhScale = kInv2Pi;  // converts radians-FM index to phase units

    const uint8_t voiceIdx = static_cast<uint8_t>(&s - voices_);
    float* resoBuf = voiceResoBuf_(voiceIdx);

    // Pre-tick LFOs/envelopes get computed inside the per-sample loop, but we
    // recompute pitch-dependent quantities every kPitchBatch samples to avoid
    // a per-sample exp2.
    constexpr uint64_t kPitchBatch = 16;

    // Cache per-osc values that change slowly.
    float oscFreq[synth::patch::MAX_OSCS] = {};
    float oscDp[synth::patch::MAX_OSCS] = {};

    auto recomputePitch = [&](float vibSemis[synth::patch::MAX_LFOS]) noexcept {
        for (uint8_t i = 0; i < p.oscCount && i < synth::patch::MAX_OSCS; ++i) {
            const auto& od = p.oscs[i];
            const float ratio = od.pitchRatio * (od.isFmModulator ? p.fmRatioScale : 1.0f);
            float vibOctaves = 0.0f;
            if (od.vibratoLfoIndex >= 0 && od.vibratoLfoIndex < synth::patch::MAX_LFOS) {
                vibOctaves = vibSemis[od.vibratoLfoIndex] * od.vibratoSemitones * (1.0f / 12.0f);
            }
            const float octaves = (od.pitchSemitones + od.pitchCents * 0.01f) * (1.0f / 12.0f) + vibOctaves;
            const float f = s.freq * ratio * synth::dsp::exp2Fast(octaves);
            oscFreq[i] = f;
            oscDp[i] = f * invSr;
        }
    };

    float vibLfoBuf[synth::patch::MAX_LFOS] = {};
    float lfoBuf[synth::patch::MAX_LFOS] = {};

    // Pre-populate LFO snapshot before first batch.
    for (uint8_t i = 0; i < p.lfoCount && i < synth::patch::MAX_LFOS; ++i) {
        lfoBuf[i] = 0.0f;
        vibLfoBuf[i] = 0.0f;
    }
    recomputePitch(vibLfoBuf);

    uint64_t batchTimer = 0;

    for (uint64_t i = 0; i < n; ++i) {
        // 1) Tick LFOs/envelopes.
        for (uint8_t li = 0; li < p.lfoCount && li < synth::patch::MAX_LFOS; ++li) {
            lfoBuf[li] = s.lfos[li].tick();
        }
        for (uint8_t ei = 1; ei < p.envCount && ei < synth::patch::MAX_ENVS; ++ei) {
            s.modEnvs[ei].tick(dtSr);
        }

        // 2) Refresh pitch every kPitchBatch.
        if (batchTimer == 0) {
            for (uint8_t li = 0; li < synth::patch::MAX_LFOS; ++li) vibLfoBuf[li] = lfoBuf[li];
            recomputePitch(vibLfoBuf);
        }
        batchTimer = (batchTimer + 1) & (kPitchBatch - 1);

        // 3) Render oscillators in order.
        for (uint8_t oi = 0; oi < p.oscCount && oi < synth::patch::MAX_OSCS; ++oi) {
            const auto& od = p.oscs[oi];
            float ph = s.oscPhase[oi] + oscDp[oi];
            ph -= floorf(ph);
            s.oscPhase[oi] = ph;

            float phaseMod = ph;
            if (od.fmFeedback != 0.0f) {
                phaseMod += s.oscFb[oi] * od.fmFeedback * fbScale;
            }
            if (od.fmFrom >= 0 && od.fmFrom < p.oscCount) {
                float idx = od.fmIndex;
                const auto& src = p.oscs[od.fmFrom];
                if (src.isFmModulator) idx *= p.fmIndexScale;
                if (od.modEnvIndex >= 1 && od.modEnvIndex < synth::patch::MAX_ENVS) {
                    idx *= s.modEnvs[od.modEnvIndex].level;
                }
                if (od.velToFmIndex > 0.0f) {
                    const float velMix = od.velToFmIndex;
                    idx *= (1.0f - velMix) + velMix * (0.25f + 0.75f * s.vel);
                }
                phaseMod += s.oscOut[od.fmFrom] * idx * idxPhScale;
            }
            phaseMod -= floorf(phaseMod);

            const float y = oscShape(od.shape, phaseMod, s.oscSawTbl[oi], od.pulseWidth);
            s.oscOut[oi] = y;
            s.oscFb[oi] = y;
        }

        // 4) Mix oscillators.
        float oscMix = 0.0f;
        for (uint8_t oi = 0; oi < p.oscCount && oi < synth::patch::MAX_OSCS; ++oi) {
            const float lvl = p.oscs[oi].level;
            if (lvl != 0.0f) oscMix += s.oscOut[oi] * lvl;
        }

        // 5) Noise.
        float noiseSample = 0.0f;
        if (p.noise.shape == synth::patch::NoiseShape::White) {
            noiseSample = s.white.tick() * p.noise.level;
        } else if (p.noise.shape == synth::patch::NoiseShape::Pinkish) {
            noiseSample = s.pink.tick() * p.noise.level;
        }

        // 6) Resonator (Karplus-strong style body) + exciter feeds it.
        float resoOut = 0.0f;
        if (p.resonator.enabled && resoBuf) {
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
        if (p.resonator.enabled) {
            const float a = 0.20f + 0.55f * s.vel;
            sig = resoOut * p.resonator.bodyMix
                + oscMix * p.resonator.harmAmount * a
                + noiseSample;
            sig = s.bodyHp.tick(sig);
            sig = s.outLp.tick(sig);
        } else {
            sig = oscMix + noiseSample;
        }

        // 8) Filter chain.
        for (uint8_t fi = 0; fi < p.filterCount && fi < synth::patch::MAX_FILTERS; ++fi) {
            const auto& fd = p.filters[fi];
            // Update cutoff cheaply every kPitchBatch (also handles env sweep).
            if ((i & (kPitchBatch - 1)) == 0) {
                float envLvl = 0.0f;
                if (fd.envIndex >= 1 && fd.envIndex < synth::patch::MAX_ENVS) {
                    envLvl = s.modEnvs[fd.envIndex].level;
                }
                float cut = fd.cutoffHz + (fd.cutoffPeakHz - fd.cutoffHz) * envLvl;
                cut += fd.velToCutoffHz * s.vel;
                if (fd.lfoIndex >= 0 && fd.lfoIndex < synth::patch::MAX_LFOS) {
                    cut += lfoBuf[fd.lfoIndex] * fd.lfoCutoffHz;
                }
                if (cut < 20.0f) cut = 20.0f;
                s.filters[fi].set(cut, fd.resonance, sr);
            }
            sig = s.filters[fi].tick(sig);
        }

        // 9) Drive.
        if (p.drive.enabled) {
            sig = synth::dsp::tanhFast(sig * p.drive.preGain) * p.drive.postGain;
        }

        out[i] += sig * p.outputGain;
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
            patch_.fmIndexScale = v * 2.0f; // 0..2 (1.0 = nominal)
            return true;
        }
        case Params::FmModRatio: {
            const float v = clampf(value, 0.0f, 1.0f);
            // Map 0..1 to 0.25..4.0 multiplier (centered around 1.0 at v=0.5).
            patch_.fmRatioScale = 0.25f + v * (4.0f - 0.25f);
            return true;
        }
        default:
            break;
    }
    return false;
}
