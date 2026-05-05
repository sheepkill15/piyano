// VoiceAllocator.h
// Standalone polyphony allocator + note→voice mapping.
// Kept minimal so it can be reused by future patch/voice processors.
#pragma once

#include <cstdint>
#include <array>

#include "synth/Constants.h"
#include "synth/dsp/Approx.h"
#include "synth/dsp/Util.h"

namespace synth::voice {

static constexpr uint8_t kMaxNotes = cfg::kMidiNoteCount;
static constexpr uint8_t kInvalidVoice = cfg::kInvalidVoiceId;

enum class SameNoteMode : uint8_t {
    SingleVoicePerKey,
    MultiVoicePerKey
};

struct NoteEvent {
    uint8_t note = 0;
    float velocity = 0.0f;   // 0..1
    float frequency = 0.0f;  // Hz
};

struct VoiceState {
    uint8_t note = 0;
    float velocity = 0.0f;
    float frequency = 0.0f;
    bool gate = false; // true while key held; release can continue after gate=false
};

template <uint8_t MaxVoices>
class VoiceAllocator {
public:
    static constexpr uint8_t kMaxVoices = MaxVoices;

    void reset() noexcept {
        for (auto& v : voices_) v = {};
        for (auto& c : noteVoiceCount_) c = 0;
        nextVoiceIndex_ = 0;
        maxVoices_ = MaxVoices;
    }

    void setMaxVoices(const uint8_t n) noexcept {
        maxVoices_ = dsp::clamp<uint8_t>(n, 1, MaxVoices);
        if (nextVoiceIndex_ >= maxVoices_) nextVoiceIndex_ = 0;
        // Trim per-note stacks (swap-remove to keep O(1) best-effort).
        for (uint8_t note = 0; note < kMaxNotes; ++note) {
            if (noteVoiceCount_[note] > maxVoices_) noteVoiceCount_[note] = maxVoices_;
        }
    }

    [[nodiscard]] uint8_t maxVoices() const noexcept { return maxVoices_; }

    [[nodiscard]] const VoiceState& voice(uint8_t i) const noexcept { return voices_[i]; }
    VoiceState& voiceMutable(uint8_t i) noexcept { return voices_[i]; }

    uint8_t noteOn(const NoteEvent& ev, const SameNoteMode mode) noexcept {
        if (mode == SameNoteMode::SingleVoicePerKey) {
            const uint8_t existing = lastVoiceForNote_(ev.note);
            if (existing != kInvalidVoice) {
                VoiceState& v = voices_[existing];
                v.note = ev.note;
                v.frequency = ev.frequency;
                v.velocity = ev.velocity;
                v.gate = true;
                return existing;
            }
        }

        const uint8_t v = allocateVoice_();
        detachVoiceFromNote_(v);
        VoiceState& vs = voices_[v];
        vs.note = ev.note;
        vs.frequency = ev.frequency;
        vs.velocity = ev.velocity;
        vs.gate = true;
        pushVoiceForNote_(ev.note, v);
        return v;
    }

    uint8_t noteOff(const uint8_t note, const SameNoteMode mode) noexcept {
        uint8_t v = kInvalidVoice;
        if (mode == SameNoteMode::MultiVoicePerKey) v = popVoiceForNote_(note);
        else v = takeLastVoiceForNote_(note);
        if (v == kInvalidVoice) return kInvalidVoice;
        voices_[v].gate = false;
        return v;
    }

private:
    uint8_t maxVoices_ = MaxVoices;
    std::array<VoiceState, MaxVoices> voices_ = {};
    uint8_t nextVoiceIndex_ = 0;

    std::array<uint8_t, kMaxNotes> noteVoiceCount_ = {};
    std::array<std::array<uint8_t, MaxVoices>, kMaxNotes> noteVoices_ = {};

    uint8_t allocateVoice_() noexcept {
        for (uint8_t step = 0; step < maxVoices_; ++step) {
            const auto i = static_cast<uint8_t>((nextVoiceIndex_ + step) % maxVoices_);
            if (!voices_[i].gate) {
                nextVoiceIndex_ = static_cast<uint8_t>((i + 1) % maxVoices_);
                return i;
            }
        }
        const uint8_t v = nextVoiceIndex_;
        nextVoiceIndex_ = static_cast<uint8_t>((nextVoiceIndex_ + 1) % maxVoices_);
        return v;
    }

    [[nodiscard]] uint8_t lastVoiceForNote_(const uint8_t note) const noexcept {
        if (note >= kMaxNotes) return kInvalidVoice;
        const uint8_t count = noteVoiceCount_[note];
        if (count == 0) return kInvalidVoice;
        return noteVoices_[note][count - 1];
    }

    void pushVoiceForNote_(const uint8_t note, const uint8_t voiceIndex) noexcept {
        if (note >= kMaxNotes) return;
        uint8_t& count = noteVoiceCount_[note];
        if (count >= maxVoices_) {
            noteVoices_[note][maxVoices_ - 1] = voiceIndex;
            count = maxVoices_;
            return;
        }
        noteVoices_[note][count] = voiceIndex;
        count = count + 1;
    }

    uint8_t popVoiceForNote_(const uint8_t note) noexcept {
        if (note >= kMaxNotes) return kInvalidVoice;
        uint8_t& count = noteVoiceCount_[note];
        while (count > 0) {
            const uint8_t v = noteVoices_[note][count - 1];
            count = count - 1;
            if (v < MaxVoices && voices_[v].note == note) return v;
        }
        return kInvalidVoice;
    }

    uint8_t takeLastVoiceForNote_(const uint8_t note) noexcept {
        if (note >= kMaxNotes) return kInvalidVoice;
        const uint8_t v = popVoiceForNote_(note);
        noteVoiceCount_[note] = 0;
        return v;
    }

    void detachVoiceFromNote_(const uint8_t voiceIndex) noexcept {
        const uint8_t note = voices_[voiceIndex].note;
        if (note >= kMaxNotes) return;
        uint8_t& count = noteVoiceCount_[note];
        for (uint8_t i = 0; i < count; ++i) {
            if (noteVoices_[note][i] == voiceIndex) {
                noteVoices_[note][i] = noteVoices_[note][count - 1];
                count = count - 1;
                return;
            }
        }
    }
};

} // namespace synth::voice

