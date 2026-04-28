#include "PatchInstrument.h"
#include "workstation/Params.h"
#include <cmath>
#include <new>
#include <cstring>

void PatchInstrument::init(const PatchConfig& cfg) noexcept {
    cfg_ = cfg;
    if (cfg_.kind != PatchConfig::Kind::Piano && pianoBuf_) {
        delete[] pianoBuf_;
        pianoBuf_ = nullptr;
    }
    if (cfg_.kind == PatchConfig::Kind::Piano && !pianoBuf_) {
        pianoBuf_ = new (std::nothrow) float[static_cast<std::size_t>(MAX_VOICES) * PIANO_BUF_SAMPLES];
        if (pianoBuf_) {
            std::memset(pianoBuf_, 0, sizeof(float) * static_cast<std::size_t>(MAX_VOICES) * PIANO_BUF_SAMPLES);
        }
    }
}

const char* PatchInstrument::name() const noexcept {
    switch (cfg_.kind) {
        case PatchConfig::Kind::Subtractive: return "Subtractive";
        case PatchConfig::Kind::FM: return "FM";
        case PatchConfig::Kind::Piano: return "Piano";
        case PatchConfig::Kind::EPiano: return "EPiano";
        case PatchConfig::Kind::Strings: return "Strings";
        case PatchConfig::Kind::Bass: return "Bass";
        case PatchConfig::Kind::Drums: return "Drums";
        default: return "Patch";
    }
}

void PatchInstrument::setSampleRate(float sr) noexcept {
    sampleRate_ = sr;
    for (uint8_t i = 0; i < MAX_VOICES; ++i) {
        kick_[i].setSampleRate(sr);
        snare_[i].setSampleRate(sr);
        hat_[i].setSampleRate(sr);
    }
}

bool PatchInstrument::setParam(uint16_t paramId, float value) noexcept {
    if (paramId == Params::FmModIndex) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        cfg_.fmModIndex = value * 10.0f;
        return true;
    }
    if (paramId == Params::FmModRatio) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        cfg_.fmModRatio = 0.25f + value * (8.0f - 0.25f);
        return true;
    }
    return false;
}

void PatchInstrument::onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept {
    freq_[v] = ctx.frequency;
    vel_[v] = ctx.velocity;

    switch (cfg_.kind) {
        case PatchConfig::Kind::Piano: {
            const float clickLevel = 0.15f + 0.35f * ctx.velocity;
            click_[v].trigger(clickLevel, 0.9985f);
            const float noiseLevel = 0.10f + 0.25f * ctx.velocity;
            noise_[v].trigger(noiseLevel, 0.9975f);

            const float f = (ctx.frequency < 20.0f) ? 20.0f : ctx.frequency;
            const std::size_t delay = static_cast<std::size_t>(sampleRate_ / f);
            PianoBodyState& st = pianoBody_[v];
            st.idx = 0;
            st.lp = 0.0f;
            st.delay = delay < 1 ? 1 : (delay >= PIANO_BUF_SAMPLES ? (PIANO_BUF_SAMPLES - 1) : delay);
            st.feedback = 0.965f - 0.10f * ctx.velocity;
            st.damp = 0.08f + 0.15f * (1.0f - ctx.velocity);

            if (float* buf = pianoVoiceBuf_(v)) {
                std::memset(buf, 0, sizeof(float) * PIANO_BUF_SAMPLES);
            }
            break;
        }
        case PatchConfig::Kind::Strings: {
            filter_[v].mode = synth::modules::SvfMode::LowPass;
            filter_[v].set(2500.0f + ctx.velocity * 4000.0f, 0.2f, sampleRate_);
            break;
        }
        case PatchConfig::Kind::Bass: {
            filter_[v].mode = synth::modules::SvfMode::LowPass;
            filter_[v].set(220.0f + ctx.velocity * 800.0f, 0.4f, sampleRate_);
            drive_.preGain = 1.8f;
            drive_.postGain = 0.7f;
            break;
        }
        case PatchConfig::Kind::Drums: {
            if (ctx.note == 36) {
                drumType_[v] = DrumType::Kick;
                kick_[v].trigger(ctx.velocity);
            } else if (ctx.note == 38) {
                drumType_[v] = DrumType::Snare;
                snare_[v].trigger(ctx.velocity);
            } else if (ctx.note == 42 || ctx.note == 44) {
                drumType_[v] = DrumType::Hat;
                hat_[v].trigger(ctx.velocity);
            } else {
                drumType_[v] = DrumType::Hat;
                hat_[v].trigger(ctx.velocity * 0.6f);
            }
            break;
        }
        default:
            break;
    }
}

void PatchInstrument::onVoiceStop(uint8_t voiceIndex) noexcept {
    (void)voiceIndex;
}

void PatchInstrument::renderAddVoice(uint8_t v, float* out, uint64_t n) noexcept {
    const float sr = sampleRate_;
    const float twoPi = 2.0f * static_cast<float>(M_PI);

    switch (cfg_.kind) {
        case PatchConfig::Kind::Subtractive: {
            float ph = ph1_[v];
            const float f = freq_[v];
            const float a = vel_[v];
            for (uint64_t i = 0; i < n; ++i) {
                out[i] += sinf(ph) * a;
                ph += twoPi * f / sr;
                if (ph >= twoPi) ph = fmodf(ph, twoPi);
            }
            ph1_[v] = ph;
            break;
        }
        case PatchConfig::Kind::FM:
        case PatchConfig::Kind::EPiano: {
            float cph = ph1_[v];
            float mph = ph2_[v];
            const float f = freq_[v];
            const float a = vel_[v];
            const float modIndex = cfg_.fmModIndex * ((cfg_.kind == PatchConfig::Kind::EPiano) ? (0.3f + 0.7f * a) : 1.0f);
            for (uint64_t i = 0; i < n; ++i) {
                const float mod = sinf(mph) * modIndex;
                out[i] += sinf(cph + mod) * a;
                mph += twoPi * (f * cfg_.fmModRatio) / sr;
                cph += twoPi * f / sr;
                if (mph >= twoPi) mph = fmodf(mph, twoPi);
                if (cph >= twoPi) cph = fmodf(cph, twoPi);
            }
            ph1_[v] = cph;
            ph2_[v] = mph;
            break;
        }
        case PatchConfig::Kind::Piano: {
            float ph1 = ph1_[v];
            float ph2 = ph2_[v];
            const float f = freq_[v];
            const float a = vel_[v];
            float* buf = pianoVoiceBuf_(v);
            PianoBodyState& st = pianoBody_[v];
            for (uint64_t i = 0; i < n; ++i) {
                float s = sinf(ph1) * (0.25f * a) + sinf(ph2) * (0.15f * a);
                ph1 += twoPi * f / sr;
                ph2 += twoPi * (f * 2.0f) / sr;
                if (ph1 >= twoPi) ph1 = fmodf(ph1, twoPi);
                if (ph2 >= twoPi) ph2 = fmodf(ph2, twoPi);

                const float exc = click_[v].tick() + noise_[v].tick();
                float body = 0.0f;
                if (buf) {
                    const std::size_t read = (st.idx + PIANO_BUF_SAMPLES - st.delay) % PIANO_BUF_SAMPLES;
                    body = buf[read];
                    st.lp += (body - st.lp) * st.damp;
                    const float fb = st.lp * st.feedback;
                    buf[st.idx] = (exc + s * 0.2f) + fb;
                    st.idx = (st.idx + 1) % PIANO_BUF_SAMPLES;
                }
                out[i] += (s + body) * 0.8f;
            }
            ph1_[v] = ph1;
            ph2_[v] = ph2;
            break;
        }
        case PatchConfig::Kind::Strings: {
            float p1 = ph1_[v];
            float p2 = ph2_[v];
            const float f = freq_[v];
            const float a = vel_[v];
            const float detune = 0.003f;
            for (uint64_t i = 0; i < n; ++i) {
                const float saw1 = 2.0f * (p1 / twoPi) - 1.0f;
                const float saw2 = 2.0f * (p2 / twoPi) - 1.0f;
                float s = (saw1 + saw2) * 0.45f;
                s = filter_[v].tick(s);
                out[i] += s * a;

                p1 += twoPi * f / sr;
                p2 += twoPi * (f * (1.0f + detune)) / sr;
                if (p1 >= twoPi) p1 = fmodf(p1, twoPi);
                if (p2 >= twoPi) p2 = fmodf(p2, twoPi);
            }
            ph1_[v] = p1;
            ph2_[v] = p2;
            break;
        }
        case PatchConfig::Kind::Bass: {
            float ph = ph1_[v];
            const float f = freq_[v];
            const float a = vel_[v];
            for (uint64_t i = 0; i < n; ++i) {
                ph += f / sr;
                ph -= floorf(ph);
                const float saw = 2.0f * ph - 1.0f;
                const float sub = sinf(2.0f * static_cast<float>(M_PI) * ph) * 0.7f;
                float s = (saw * 0.7f + sub * 0.3f);
                s = filter_[v].tick(s);
                s = drive_.tick(s);
                out[i] += s * a;
            }
            ph1_[v] = ph;
            break;
        }
        case PatchConfig::Kind::Drums: {
            switch (drumType_[v]) {
                case DrumType::Kick:
                    for (uint64_t i = 0; i < n; ++i) out[i] += kick_[v].tick();
                    break;
                case DrumType::Snare:
                    for (uint64_t i = 0; i < n; ++i) out[i] += snare_[v].tick();
                    break;
                case DrumType::Hat:
                    for (uint64_t i = 0; i < n; ++i) out[i] += hat_[v].tick();
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

