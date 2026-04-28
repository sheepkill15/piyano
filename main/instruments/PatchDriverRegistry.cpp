#include "PatchDriverRegistry.h"

#include "PatchConfig.h"
#include "PatchDriver.h"
#include "workstation/Params.h"

#include "synth/dsp/Approx.h"
#include "synth/dsp/WaveTables.h"
#include "synth/modules/Adsr.h"
#include "synth/modules/DrumVoices.h"
#include "synth/modules/Exciter.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Lfo.h"
#include "synth/modules/Noise.h"
#include "synth/modules/Shaper.h"
#include "synth/modules/Smoother.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <new>

namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr std::size_t kPianoBufSamples = 4096; // ~93 ms at 44.1k - enough down to ~11 Hz
static_assert((kPianoBufSamples & (kPianoBufSamples - 1)) == 0,
              "kPianoBufSamples must be a power of two for bitmask wrap");
constexpr std::size_t kPianoBufMask = kPianoBufSamples - 1;
constexpr uint8_t kMaxVoicesCap = 8;

inline float clampf(float x, float lo, float hi) noexcept {
    return x < lo ? lo : (x > hi ? hi : x);
}

// =========================================================================
// Subtractive: dual saw + sub sine + SVF lowpass with its own filter env.
// =========================================================================
struct SubVoiceState {
    float freq = 0.0f;
    float vel = 0.0f;
    float ph1 = 0.0f;
    float ph2 = 0.0f;
    float subPh = 0.0f;
    int sawTbl1 = 0;
    int sawTbl2 = 0;
    synth::modules::Svf filter{};
    synth::modules::Adsr filtEnv{};
};

struct SubInstrumentState {
    float cutoffBase = 800.0f;   // Hz (filter env starts here, settles low)
    float cutoffPeak = 5500.0f;  // Hz peak right after attack
    float resonance = 0.35f;     // 0..1
    float detuneCents = 7.0f;    // detune between osc1 and osc2
    float drive = 1.1f;
};

struct SubtractiveDriver final : public PatchDriver {
    const char* name() const noexcept override { return "Subtractive"; }
    std::size_t voiceStateBytes() const noexcept override { return sizeof(SubVoiceState); }
    std::size_t instrumentStateBytes() const noexcept override { return sizeof(SubInstrumentState); }

    void constructInstrumentState(void* p) const noexcept override { new (p) SubInstrumentState(); }
    void destroyInstrumentState(void* p) const noexcept override { static_cast<SubInstrumentState*>(p)->~SubInstrumentState(); }

    void voiceStart(void* inst, void* voice, uint8_t /*vi*/, const VoiceContext& ctx, float sr,
                    const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<SubVoiceState*>(voice);
        auto* I = static_cast<SubInstrumentState*>(inst);
        s->freq = ctx.frequency;
        s->vel = ctx.velocity;
        s->ph1 = 0.0f;
        s->ph2 = 0.5f;
        s->subPh = 0.0f;
        const float f1 = ctx.frequency;
        const float f2 = ctx.frequency * synth::dsp::exp2Fast(I->detuneCents * (1.0f / 1200.0f));
        s->sawTbl1 = synth::dsp::sawTableIdx(f1);
        s->sawTbl2 = synth::dsp::sawTableIdx(f2);
        s->filter = synth::modules::Svf{};
        s->filter.mode = synth::modules::SvfMode::LowPass;
        s->filter.set(I->cutoffBase + (I->cutoffPeak - I->cutoffBase) * (0.5f + 0.5f * ctx.velocity), I->resonance, sr);
        s->filtEnv = synth::modules::Adsr{};
        s->filtEnv.attack_s  = 0.005f;
        s->filtEnv.decay_s   = 0.35f + 0.25f * (1.0f - ctx.velocity);
        s->filtEnv.sustain   = 0.25f;
        s->filtEnv.release_s = 0.20f;
        s->filtEnv.noteOn();
    }

    void voiceRenderAdd(void* inst, void* voice, uint8_t /*vi*/, float* out, uint64_t n, float sr,
                        const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<SubVoiceState*>(voice);
        auto* I = static_cast<SubInstrumentState*>(inst);
        const float dtSr = 1.0f / sr;
        const float invSr = 1.0f / sr;
        const float f1 = s->freq;
        const float f2 = s->freq * synth::dsp::exp2Fast(I->detuneCents * (1.0f / 1200.0f));
        const float fSub = s->freq * 0.5f;
        const float dp1 = f1 * invSr;
        const float dp2 = f2 * invSr;
        const float dpSub = fSub * invSr;
        const int t1 = s->sawTbl1;
        const int t2 = s->sawTbl2;
        const float drive = I->drive;

        for (uint64_t i = 0; i < n; ++i) {
            s->filtEnv.tick(dtSr);
            if ((i & 15) == 0) {
                const float fEnv = s->filtEnv.level;
                const float cutoff = I->cutoffBase + (I->cutoffPeak - I->cutoffBase) * fEnv;
                s->filter.set(cutoff, I->resonance, sr);
            }

            s->ph1 += dp1; if (s->ph1 >= 1.0f) s->ph1 -= 1.0f;
            s->ph2 += dp2; if (s->ph2 >= 1.0f) s->ph2 -= 1.0f;
            s->subPh += dpSub; if (s->subPh >= 1.0f) s->subPh -= 1.0f;

            const float saw1 = synth::dsp::sawLU(t1, s->ph1);
            const float saw2 = synth::dsp::sawLU(t2, s->ph2);
            const float sub  = synth::dsp::sineLU(s->subPh);

            float sig = (saw1 + saw2) * 0.35f + sub * 0.25f;
            sig = s->filter.tick(sig);
            sig = synth::dsp::tanhFast(sig * drive);
            out[i] += sig * 0.7f;
        }
    }
};

// =========================================================================
// FM family: 2-op feedback FM with modulator amp env (DX-style).
// =========================================================================
struct FmVoiceState {
    float freq = 0.0f;
    float vel = 0.0f;
    float carPh = 0.0f;
    float modPh = 0.0f;
    float modFb = 0.0f;
    synth::modules::Adsr modEnv{};
};

struct FmFamilyDriver final : public PatchDriver {
    const char* const label_;
    const float indexVelAmount_;
    const float modAttack_;
    const float modDecay_;
    const float modSustain_;
    const float modRelease_;
    const float feedback_;

    FmFamilyDriver(const char* label, float indexVelAmount,
                   float modAttack, float modDecay, float modSustain, float modRelease,
                   float feedback) noexcept
        : label_(label), indexVelAmount_(indexVelAmount),
          modAttack_(modAttack), modDecay_(modDecay), modSustain_(modSustain), modRelease_(modRelease),
          feedback_(feedback) {}

    const char* name() const noexcept override { return label_; }
    std::size_t voiceStateBytes() const noexcept override { return sizeof(FmVoiceState); }

    void voiceStart(void* /*inst*/, void* voice, uint8_t /*vi*/, const VoiceContext& ctx, float /*sr*/,
                    const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<FmVoiceState*>(voice);
        s->freq = ctx.frequency;
        s->vel = ctx.velocity;
        s->carPh = 0.0f;
        s->modPh = 0.0f;
        s->modFb = 0.0f;
        s->modEnv = synth::modules::Adsr{};
        s->modEnv.attack_s  = modAttack_;
        s->modEnv.decay_s   = modDecay_;
        s->modEnv.sustain   = modSustain_;
        s->modEnv.release_s = modRelease_;
        s->modEnv.noteOn();
    }

    void voiceRenderAdd(void* /*inst*/, void* voice, uint8_t /*vi*/, float* out, uint64_t n, float sr,
                        const PatchConfig& cfg) const noexcept override {
        auto* s = static_cast<FmVoiceState*>(voice);
        const float dtSr = 1.0f / sr;
        const float invSr = 1.0f / sr;
        const float dpC = s->freq * invSr;
        const float dpM = s->freq * cfg.fmModRatio * invSr;

        const float velMix = indexVelAmount_;
        const float velScale = (1.0f - velMix) + velMix * (0.25f + 0.75f * s->vel);
        const float baseIndex = cfg.fmModIndex * velScale;

        // Run the modulation in normalized phase units (so we can use the cheap
        // sineLU on a wrapped phase). The carrier sees an offset in phase units
        // computed from the modulator output.
        const float fbScale = feedback_ * (1.0f / kTwoPi); // feedback_ is in radians; convert to phase units
        const float idxPhScale = 1.0f / kTwoPi;            // mod->carrier index in phase units

        for (uint64_t i = 0; i < n; ++i) {
            s->modEnv.tick(dtSr);
            const float idx = baseIndex * s->modEnv.level;

            float modPhWrapped = s->modPh + s->modFb * fbScale;
            modPhWrapped -= floorf(modPhWrapped);
            const float modOut = synth::dsp::sineLU(modPhWrapped);
            s->modFb = modOut;

            float carPhMod = s->carPh + modOut * idx * idxPhScale;
            carPhMod -= floorf(carPhMod);
            const float carIn = synth::dsp::sineLU(carPhMod);

            s->carPh += dpC; if (s->carPh >= 1.0f) s->carPh -= 1.0f;
            s->modPh += dpM; if (s->modPh >= 1.0f) s->modPh -= 1.0f;

            out[i] += carIn * 0.85f;
        }
    }

    bool setParam(PatchConfig& cfg, uint16_t paramId, float value) const noexcept override {
        if (paramId == Params::FmModIndex) {
            value = clampf(value, 0.0f, 1.0f);
            cfg.fmModIndex = value * 8.0f;
            return true;
        }
        if (paramId == Params::FmModRatio) {
            value = clampf(value, 0.0f, 1.0f);
            // Snap-ish ratio: 0.5, 1, 2, 3, 4, ... biased to integer-ish ratios
            const float low = 0.5f;
            const float high = 8.0f;
            cfg.fmModRatio = low + value * (high - low);
            return true;
        }
        return false;
    }
};

// =========================================================================
// Piano: transient (click + filtered noise burst) -> Karplus-strong-ish body
//        + a small bell harmonic mix; per-note feedback/damping.
// =========================================================================
struct PianoBodyState {
    std::size_t idx = 0;
    std::size_t delay = 1;
    float feedback = 0.99f;
    float damp = 0.20f;
    float lp = 0.0f;
};

struct PianoInstrumentState {
    float* buf = nullptr;

    void alloc() noexcept {
        if (buf) return;
        buf = new (std::nothrow) float[static_cast<std::size_t>(kMaxVoicesCap) * kPianoBufSamples];
        if (buf) std::memset(buf, 0, sizeof(float) * static_cast<std::size_t>(kMaxVoicesCap) * kPianoBufSamples);
    }

    void freeBuf() noexcept {
        delete[] buf;
        buf = nullptr;
    }

    float* voiceBuf(uint8_t v) noexcept { return buf ? (buf + static_cast<std::size_t>(v) * kPianoBufSamples) : nullptr; }
};

struct PianoVoiceState {
    float freq = 0.0f;
    float vel = 0.0f;
    float ph1 = 0.0f;
    float ph2 = 0.0f;
    float ph3 = 0.0f;
    synth::modules::ClickExciter click{};
    synth::modules::NoiseBurst noise{};
    synth::modules::Svf bodyHP{};
    synth::modules::OnePoleLP outLP{};
    PianoBodyState body{};
};

struct PianoDriver final : public PatchDriver {
    const char* name() const noexcept override { return "Piano"; }
    std::size_t voiceStateBytes() const noexcept override { return sizeof(PianoVoiceState); }
    std::size_t instrumentStateBytes() const noexcept override { return sizeof(PianoInstrumentState); }

    void constructInstrumentState(void* ptr) const noexcept override { new (ptr) PianoInstrumentState(); }
    void destroyInstrumentState(void* ptr) const noexcept override {
        static_cast<PianoInstrumentState*>(ptr)->freeBuf();
        static_cast<PianoInstrumentState*>(ptr)->~PianoInstrumentState();
    }

    void instrumentInit(void* inst, float /*sr*/, const PatchConfig& /*cfg*/) const noexcept override {
        static_cast<PianoInstrumentState*>(inst)->alloc();
    }

    void voiceStart(void* inst, void* voice, uint8_t v, const VoiceContext& ctx, float sr,
                    const PatchConfig& /*cfg*/) const noexcept override {
        auto* st = static_cast<PianoVoiceState*>(voice);
        st->freq = ctx.frequency;
        st->vel = ctx.velocity;
        st->ph1 = 0.0f; st->ph2 = 0.0f; st->ph3 = 0.0f;

        st->click.trigger(0.55f + 0.65f * ctx.velocity, 0.92f);
        st->noise.trigger(0.20f + 0.40f * ctx.velocity, 0.998f);

        const float f = (ctx.frequency < 20.0f) ? 20.0f : ctx.frequency;
        std::size_t delay = static_cast<std::size_t>(sr / f);
        if (delay < 2) delay = 2;
        if (delay >= kPianoBufSamples) delay = kPianoBufSamples - 1;
        st->body.idx = 0;
        st->body.lp = 0.0f;
        st->body.delay = delay;

        const float keyT = clampf((ctx.note - 21) / 88.0f, 0.0f, 1.0f); // 0=A0..1=C8
        st->body.feedback = 0.9985f;
        st->body.damp     = 0.30f + 0.45f * keyT;

        st->bodyHP = synth::modules::Svf{};
        st->bodyHP.mode = synth::modules::SvfMode::HighPass;
        st->bodyHP.set(45.0f, 0.0f, sr);

        st->outLP.reset(0.0f);
        const float tone = 2800.0f + 5500.0f * ctx.velocity;
        st->outLP.setCutoffHz(tone, sr);

        if (auto* pinst = static_cast<PianoInstrumentState*>(inst)) {
            if (float* b = pinst->voiceBuf(v)) {
                std::memset(b, 0, sizeof(float) * kPianoBufSamples);
            }
        }
    }

    void voiceRenderAdd(void* inst, void* voice, uint8_t v, float* out, uint64_t n, float sr,
                        const PatchConfig& /*cfg*/) const noexcept override {
        auto* st = static_cast<PianoVoiceState*>(voice);
        auto* pinst = static_cast<PianoInstrumentState*>(inst);
        float* buf = pinst ? pinst->voiceBuf(v) : nullptr;
        const float f = st->freq;
        const float a = 0.20f + 0.55f * st->vel;
        const float invSr = 1.0f / sr;
        const float dp1 = f * invSr;
        const float dp2 = (f * 2.0f) * invSr;
        const float dp3 = (f * 3.0f) * invSr;
        const float harm2 = 0.20f;
        const float harm3 = 0.06f;
        PianoBodyState& bdy = st->body;

        if (buf) {
            std::size_t bdyIdx = bdy.idx;
            const std::size_t bdyDelay = bdy.delay;
            const float bdyFb = bdy.feedback;
            const float bdyDamp = bdy.damp;
            float bdyLp = bdy.lp;

            for (uint64_t i = 0; i < n; ++i) {
                const float exc = st->click.tick() + st->noise.tick();

                const std::size_t read = (bdyIdx + kPianoBufSamples - bdyDelay) & kPianoBufMask;
                const float body = buf[read];
                bdyLp += (body - bdyLp) * bdyDamp;
                buf[bdyIdx] = exc + bdyLp * bdyFb;
                bdyIdx = (bdyIdx + 1) & kPianoBufMask;

                st->ph1 += dp1; if (st->ph1 >= 1.0f) st->ph1 -= 1.0f;
                st->ph2 += dp2; if (st->ph2 >= 1.0f) st->ph2 -= 1.0f;
                st->ph3 += dp3; if (st->ph3 >= 1.0f) st->ph3 -= 1.0f;
                const float harm = synth::dsp::sineLU(st->ph1) * 0.55f
                                 + synth::dsp::sineLU(st->ph2) * harm2
                                 + synth::dsp::sineLU(st->ph3) * harm3;

                float mix = body * 0.85f + harm * 0.15f * a;
                mix = st->bodyHP.tick(mix);
                mix = st->outLP.tick(mix);
                out[i] += mix;
            }

            bdy.idx = bdyIdx;
            bdy.lp = bdyLp;
        } else {
            for (uint64_t i = 0; i < n; ++i) {
                (void)st->click.tick();
                (void)st->noise.tick();
                st->ph1 += dp1; if (st->ph1 >= 1.0f) st->ph1 -= 1.0f;
                st->ph2 += dp2; if (st->ph2 >= 1.0f) st->ph2 -= 1.0f;
                st->ph3 += dp3; if (st->ph3 >= 1.0f) st->ph3 -= 1.0f;
                const float harm = synth::dsp::sineLU(st->ph1) * 0.55f
                                 + synth::dsp::sineLU(st->ph2) * harm2
                                 + synth::dsp::sineLU(st->ph3) * harm3;
                float mix = harm * 0.15f * a;
                mix = st->bodyHP.tick(mix);
                mix = st->outLP.tick(mix);
                out[i] += mix;
            }
        }
    }

    float voicePan(const void* /*voice*/, uint8_t v) const noexcept override {
        constexpr float spread = 0.18f;
        return ((static_cast<int>(v) & 1) ? -spread : spread) * 0.5f;
    }
};

// =========================================================================
// Strings: dual saw + slow vibrato + slow filter sweep + ensemble pan spread.
// =========================================================================
struct StringsVoiceState {
    float freq = 0.0f;
    float vel = 0.0f;
    float ph1 = 0.0f;
    float ph2 = 0.0f;
    int sawTbl = 0;
    synth::modules::Svf filter{};
    synth::modules::Lfo vibLfo{};
    synth::modules::Lfo cutLfo{};
    float pan = 0.0f;
};

struct StringsDriver final : public PatchDriver {
    const char* name() const noexcept override { return "Strings"; }
    std::size_t voiceStateBytes() const noexcept override { return sizeof(StringsVoiceState); }

    void voiceStart(void* /*inst*/, void* voice, uint8_t vi, const VoiceContext& ctx, float sr,
                    const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<StringsVoiceState*>(voice);
        s->freq = ctx.frequency;
        s->vel = ctx.velocity;
        s->ph1 = 0.0f;
        s->ph2 = 0.5f;
        // Pick the saw mipmap based on detuned upper voice so we never alias.
        s->sawTbl = synth::dsp::sawTableIdx(ctx.frequency * 1.0058f);
        s->filter = synth::modules::Svf{};
        s->filter.mode = synth::modules::SvfMode::LowPass;
        s->filter.set(1500.0f + ctx.velocity * 3500.0f, 0.18f, sr);
        s->vibLfo = synth::modules::Lfo{};
        s->vibLfo.setSampleRate(sr);
        s->vibLfo.wave = synth::modules::LfoWave::Sine;
        s->vibLfo.rateHz = 5.2f + 0.3f * (vi & 1);
        s->vibLfo.phase = static_cast<float>(vi) * 0.137f;
        s->cutLfo = synth::modules::Lfo{};
        s->cutLfo.setSampleRate(sr);
        s->cutLfo.wave = synth::modules::LfoWave::Triangle;
        s->cutLfo.rateHz = 0.35f + 0.07f * vi;
        s->cutLfo.phase = static_cast<float>(vi) * 0.211f;

        const float t = (vi & 7) / 7.0f - 0.5f; // -0.5..+0.5
        s->pan = t * 1.2f;
        if (s->pan < -0.6f) s->pan = -0.6f;
        if (s->pan >  0.6f) s->pan =  0.6f;
    }

    void voiceRenderAdd(void* /*inst*/, void* voice, uint8_t /*vi*/, float* out, uint64_t n, float sr,
                        const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<StringsVoiceState*>(voice);
        const float invSr = 1.0f / sr;
        float dp1 = s->freq * invSr;
        float dp2 = s->freq * 1.0058f * invSr;
        const int tbl = s->sawTbl;
        for (uint64_t i = 0; i < n; ++i) {
            const float vib = s->vibLfo.tick();
            const float cutMod = s->cutLfo.tick();

            if ((i & 15) == 0) {
                const float fMod = s->freq * synth::dsp::exp2Fast(vib * 0.0035f);
                dp1 = fMod * invSr;
                dp2 = fMod * 1.0058f * invSr;
                const float cutoff = 1400.0f + 1100.0f * cutMod + 2200.0f * s->vel;
                s->filter.set(cutoff < 200.0f ? 200.0f : cutoff, 0.20f, sr);
            }

            s->ph1 += dp1; if (s->ph1 >= 1.0f) s->ph1 -= 1.0f;
            s->ph2 += dp2; if (s->ph2 >= 1.0f) s->ph2 -= 1.0f;
            const float saw1 = synth::dsp::sawLU(tbl, s->ph1);
            const float saw2 = synth::dsp::sawLU(tbl, s->ph2);

            float sig = (saw1 + saw2) * 0.40f;
            sig = s->filter.tick(sig);
            out[i] += sig * 0.85f;
        }
    }

    float voicePan(const void* voice, uint8_t /*vi*/) const noexcept override {
        return static_cast<const StringsVoiceState*>(voice)->pan;
    }
};

// =========================================================================
// Bass: saw + sub sine -> SVF lowpass with filter env -> drive.
// =========================================================================
struct BassVoiceState {
    float freq = 0.0f;
    float vel = 0.0f;
    float ph1 = 0.0f;
    float subPh = 0.0f;
    int sawTbl = 0;
    synth::modules::Svf filter{};
    synth::modules::Adsr filtEnv{};
};

struct BassInstrumentState {
    float drivePre = 1.6f;
    float drivePost = 0.75f;
    float reso = 0.45f;
};

struct BassDriver final : public PatchDriver {
    const char* name() const noexcept override { return "Bass"; }
    std::size_t voiceStateBytes() const noexcept override { return sizeof(BassVoiceState); }
    std::size_t instrumentStateBytes() const noexcept override { return sizeof(BassInstrumentState); }

    void constructInstrumentState(void* p) const noexcept override { new (p) BassInstrumentState(); }
    void destroyInstrumentState(void* p) const noexcept override { static_cast<BassInstrumentState*>(p)->~BassInstrumentState(); }

    void voiceStart(void* /*inst*/, void* voice, uint8_t /*vi*/, const VoiceContext& ctx, float sr,
                    const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<BassVoiceState*>(voice);
        s->freq = ctx.frequency;
        s->vel = ctx.velocity;
        s->ph1 = 0.0f;
        s->subPh = 0.0f;
        s->sawTbl = synth::dsp::sawTableIdx(ctx.frequency);
        s->filter = synth::modules::Svf{};
        s->filter.mode = synth::modules::SvfMode::LowPass;
        s->filter.set(180.0f, 0.45f, sr);
        s->filtEnv = synth::modules::Adsr{};
        s->filtEnv.attack_s  = 0.001f;
        s->filtEnv.decay_s   = 0.18f;
        s->filtEnv.sustain   = 0.30f;
        s->filtEnv.release_s = 0.10f;
        s->filtEnv.noteOn();
    }

    void voiceRenderAdd(void* inst, void* voice, uint8_t /*vi*/, float* out, uint64_t n, float sr,
                        const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<BassVoiceState*>(voice);
        auto* B = static_cast<BassInstrumentState*>(inst);
        const float dtSr = 1.0f / sr;
        const float invSr = 1.0f / sr;
        const float dp = s->freq * invSr;
        const float dpSub = (s->freq * 0.5f) * invSr;
        const int tbl = s->sawTbl;
        const float drivePre = B->drivePre;
        const float drivePost = B->drivePost;
        const float reso = B->reso;
        for (uint64_t i = 0; i < n; ++i) {
            s->filtEnv.tick(dtSr);
            if ((i & 15) == 0) {
                const float fenv = s->filtEnv.level;
                const float cutoff = 120.0f + (1800.0f + 1200.0f * s->vel) * fenv;
                s->filter.set(cutoff, reso, sr);
            }

            s->ph1 += dp; if (s->ph1 >= 1.0f) s->ph1 -= 1.0f;
            s->subPh += dpSub; if (s->subPh >= 1.0f) s->subPh -= 1.0f;
            const float saw = synth::dsp::sawLU(tbl, s->ph1);
            const float sub = synth::dsp::sineLU(s->subPh) * 0.7f;
            float sig = saw * 0.65f + sub * 0.45f;
            sig = s->filter.tick(sig);
            sig = synth::dsp::tanhFast(sig * drivePre) * drivePost;
            out[i] += sig;
        }
    }
};

// =========================================================================
// Drums: routes MIDI note -> kick/snare/hat module, with internal envelopes.
// =========================================================================
enum class DrumType : uint8_t { None, Kick, Snare, Hat };

struct DrumsVoiceState {
    DrumType drumType = DrumType::None;
    synth::modules::Kick kick{};
    synth::modules::Snare snare{};
    synth::modules::Hat hat{};
    float pan = 0.0f;
};

struct DrumsDriver final : public PatchDriver {
    const char* name() const noexcept override { return "Drums"; }
    std::size_t voiceStateBytes() const noexcept override { return sizeof(DrumsVoiceState); }

    void applySampleRateToVoices(void* /*inst*/, uint8_t maxVoices, unsigned char* voiceBytes, std::size_t voiceStride,
                                 float sampleRate) const noexcept override {
        if (!voiceBytes) return;
        for (uint8_t v = 0; v < maxVoices; ++v) {
            auto* s = reinterpret_cast<DrumsVoiceState*>(voiceBytes + static_cast<std::size_t>(v) * voiceStride);
            s->kick.setSampleRate(sampleRate);
            s->snare.setSampleRate(sampleRate);
            s->hat.setSampleRate(sampleRate);
        }
    }

    void voiceStart(void* /*inst*/, void* voice, uint8_t /*vi*/, const VoiceContext& ctx, float sr,
                    const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<DrumsVoiceState*>(voice);
        s->kick.setSampleRate(sr);
        s->snare.setSampleRate(sr);
        s->hat.setSampleRate(sr);

        if (ctx.note == 36) {
            s->drumType = DrumType::Kick;
            s->kick.trigger(ctx.velocity);
            s->pan = 0.0f;
        } else if (ctx.note == 38 || ctx.note == 40) {
            s->drumType = DrumType::Snare;
            s->snare.trigger(ctx.velocity);
            s->pan = -0.15f;
        } else if (ctx.note == 42 || ctx.note == 44 || ctx.note == 46) {
            s->drumType = DrumType::Hat;
            s->hat.trigger(ctx.velocity);
            s->pan = 0.30f;
        } else {
            s->drumType = DrumType::Hat;
            s->hat.trigger(ctx.velocity * 0.6f);
            s->pan = 0.20f;
        }
    }

    void voiceRenderAdd(void* /*inst*/, void* voice, uint8_t /*vi*/, float* out, uint64_t n, float /*sr*/,
                        const PatchConfig& /*cfg*/) const noexcept override {
        auto* s = static_cast<DrumsVoiceState*>(voice);
        switch (s->drumType) {
            case DrumType::Kick:  for (uint64_t i = 0; i < n; ++i) out[i] += s->kick.tick();  break;
            case DrumType::Snare: for (uint64_t i = 0; i < n; ++i) out[i] += s->snare.tick(); break;
            case DrumType::Hat:   for (uint64_t i = 0; i < n; ++i) out[i] += s->hat.tick();   break;
            default: break;
        }
    }

    float voicePan(const void* voice, uint8_t /*vi*/) const noexcept override {
        return static_cast<const DrumsVoiceState*>(voice)->pan;
    }
};

} // namespace

const PatchDriver* subtractivePatchDriverPtr() noexcept {
    static const SubtractiveDriver d;
    return &d;
}

const PatchDriver* fmPatchDriverPtr() noexcept {
    // Generic FM: medium attack, slow decay to half-sustain, longer release.
    static const FmFamilyDriver d("FM", /*velIdx*/0.6f, 0.001f, 0.40f, 0.65f, 0.30f, /*fb*/0.20f);
    return &d;
}

const PatchDriver* pianoPatchDriverPtr() noexcept {
    static const PianoDriver d;
    return &d;
}

const PatchDriver* ePianoPatchDriverPtr() noexcept {
    // EP tine character: instant attack, fast modulator decay, low sustain (DX-style bell).
    static const FmFamilyDriver d("EPiano", /*velIdx*/1.0f, 0.001f, 0.18f, 0.10f, 0.25f, /*fb*/0.10f);
    return &d;
}

const PatchDriver* stringsPatchDriverPtr() noexcept {
    static const StringsDriver d;
    return &d;
}

const PatchDriver* bassPatchDriverPtr() noexcept {
    static const BassDriver d;
    return &d;
}

const PatchDriver* drumsPatchDriverPtr() noexcept {
    static const DrumsDriver d;
    return &d;
}
