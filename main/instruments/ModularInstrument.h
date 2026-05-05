#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "IInstrument.h"
#include "Patch.h"
#include "synth/Constants.h"

#include "synth/modules/Adsr.h"
#include "synth/modules/Exciter.h"
#include "synth/modules/Filter.h"
#include "synth/modules/Lfo.h"
#include "synth/modules/Noise.h"
#include "synth/modules/Oscillator.h"
#include "synth/modules/Shaper.h"

// Generic instrument: a self-contained per-voice DSP graph. setPatch() unpacks a
// synth::patch::Patch once into the per-voice modules (Adsr, Lfo, Svf, ...) and
// the few synth-graph fields that don't have a natural module home (oscillator
// wiring, filter modulation routing, resonator/exciter params). After that, the
// patch is no longer referenced; render runs entirely off the instrument's own
// modules and fields.
class ModularInstrument : public IInstrument {
public:
    ModularInstrument() noexcept = default;

    void setPatch(const synth::patch::Patch& patch) noexcept;

    [[nodiscard]] const char* name() const noexcept override { return patch_.name; }
    [[nodiscard]] uint8_t defaultMaxVoices() const noexcept override { return patch_.maxVoices; }
    [[nodiscard]] synth::voice::SameNoteMode defaultSameNoteMode() const noexcept override {
        return patch_.sameNoteMode;
    }

    void onVoiceStart(uint8_t v, const VoiceContext& ctx) noexcept override;
    void onVoiceStop(uint8_t v) noexcept override;
    void renderAddVoice(uint8_t v, float* outMono, uint64_t numSamples) noexcept override;
    [[nodiscard]] float voicePan(uint8_t v) const noexcept override;
    bool setParam(uint16_t paramId, float value) noexcept override;

private:
    static constexpr uint8_t MAX_OSCS = synth::patch::MAX_OSCS;
    static constexpr uint8_t MAX_ENVS = synth::patch::MAX_ENVS;
    static constexpr uint8_t MAX_LFOS = synth::patch::MAX_LFOS;
    static constexpr uint8_t MAX_FILTERS = synth::patch::MAX_FILTERS;
    static constexpr uint8_t MAX_VOICES = synth::patch::MAX_VOICES_CAP;

    static constexpr std::size_t kResonatorBufSamples = synth::cfg::kResonatorRingSamples;
    static_assert((kResonatorBufSamples & (kResonatorBufSamples - 1)) == 0,
                  "kResonatorBufSamples must be a power of two");
    static constexpr std::size_t kResonatorMask = kResonatorBufSamples - 1;

    enum class NoiseKind : uint8_t { Off, White, Pinkish };

    struct VoiceState {
        bool active = false;
        float freq = 0.0f;
        float vel = 0.0f;
        uint8_t note = 0;
        float keyT = 0.0f;
        float pan = 0.0f;

        // Per-osc-slot per-voice state.
        std::array<float, MAX_OSCS> oscPhase{};
        std::array<float, MAX_OSCS> oscFb{};
        std::array<float, MAX_OSCS> oscOut{};
        std::array<int, MAX_OSCS> oscSawTbl{};

        // Modules carry their own configuration. setPatch() bakes a/d/s/r into
        // each Adsr, wave/rateHz into each Lfo, mode/cutoff into each Svf,
        // color into each PinkishNoise. Note-on only triggers/resets state.
        std::array<synth::modules::Adsr, MAX_ENVS> modEnvs{};     // index 0 unused (engine drives amp env)
        std::array<synth::modules::Lfo, MAX_LFOS> lfos{};
        std::array<synth::modules::Svf, MAX_FILTERS> filters{};
        synth::modules::WhiteNoise white{};
        synth::modules::PinkishNoise pink{};
        synth::modules::ClickExciter click{};
        synth::modules::NoiseBurst burst{};

        // Karplus-Strong body (no dedicated module).
        std::size_t resoIdx = 0;
        std::size_t resoDelay = 1;
        float resoFb = 0.998f;
        float resoDamp = 0.30f;
        float resoLp = 0.0f;
        synth::modules::Svf bodyHp{};
        synth::modules::OnePoleLP outLp{};
    };

    void resetVoice_(uint8_t v) noexcept;
    float* voiceResoBuf_(uint8_t v) noexcept;

    void renderSynth_(uint8_t voiceIdx, float* out, uint64_t n) noexcept;

    void recomputeOscDp_(const VoiceState& s, const std::array<float, MAX_LFOS>& vibSemis, const float invSr,
                         std::array<float, MAX_OSCS>& oscFreq, std::array<float, MAX_OSCS>& oscDp) const noexcept;

    float renderOneOsc_(VoiceState& s, uint8_t oi, float dp) const noexcept;
    float tickResonator_(VoiceState& s, float* resoBuf) noexcept;
    void updateFilterCutoffs_(VoiceState& s, const std::array<float, MAX_LFOS>& lfoBuf) const noexcept;

    // ===== Instrument-level config (built by setPatch()) =====
    synth::patch::Patch patch_{};
    std::array<uint8_t, MAX_OSCS> oscRenderOrder_{}; // modulators before carriers (FM dependencies)

    synth::modules::Drive outDrive_{};

    std::array<VoiceState, MAX_VOICES> voices_{};
    std::array<float, MAX_VOICES * kResonatorBufSamples> resoPool_{};
};
