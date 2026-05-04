#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"

namespace synth::cfg {

// --- Patch graph (Patch.h, instruments, engine polyphony) ---
inline constexpr uint8_t kPatchMaxOscs = 4;
inline constexpr uint8_t kPatchMaxEnvs = 3;
inline constexpr uint8_t kPatchMaxLfos = 2;
inline constexpr uint8_t kPatchMaxFilters = 2;
inline constexpr uint8_t kPatchMaxVoices = 8;
inline constexpr uint8_t kPatchMaxDrumPads = 8;

// --- Wavetable dimensions (WaveTables.h / tools/gen_wavetables.py) ---
inline constexpr int kSineTableBits = 11;
inline constexpr int kSineTableSize = 2048;
inline constexpr int kSineTableMask = kSineTableSize - 1;
inline constexpr int kSawMipCount = 10;
inline constexpr int kSawTableSamples = 1024;
inline constexpr int kSawTableMask = kSawTableSamples - 1;
inline constexpr float kSawWavetableDesignSr = 44100.0f;
inline constexpr std::array<float, kSawMipCount> kSawBandMaxFreqHz = {{
    50.0f, 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3200.0f, 6400.0f, 12800.0f, 25600.0f,
}};

// --- MIDI / voice allocation ---
inline constexpr uint8_t kMidiNoteCount = 128;
inline constexpr uint8_t kInvalidVoiceId = 0xFF;

// --- Engine render (SynthEngine, main sound task) ---
inline constexpr uint64_t kAudioRenderBlockSamples = 256;
inline constexpr float kEngineVoiceGain = 0.28f;
inline constexpr uint64_t kPitchRecomputeBatch = 16;

// --- Modular resonator pool (ModularInstrument) ---
inline constexpr std::size_t kResonatorRingSamples = 4096;

// --- Workstation ---
inline constexpr uint8_t kWorkstationMaxPresets = 16;

// --- DSP math ---
inline constexpr float kTwoPi = 6.28318530717958647692f;
inline constexpr float kInv2Pi = 1.0f / kTwoPi;
inline constexpr float kRadiansToPhase01 = kInv2Pi;

// --- Master limiter (GlobalMixer) ---
inline constexpr float kMixerLimiterKnee = 0.75f;
inline constexpr float kMixerLimiterCeil = 0.98f;

// --- USB MIDI host (UsbMidi) ---
inline constexpr int kUsbMidiOutQueueSize = 128;
inline constexpr int kUsbMidiInTransfers = 2;
inline constexpr int kUsbMidiMaxClientEventMessages = 5;
inline constexpr int kUsbMidiEventPollTicks = 1;
inline constexpr int kUsbAudioSubclassMidiStreaming = 3;

// --- I2S pins (Sound) ---
inline constexpr gpio_num_t kI2sBclkPin = GPIO_NUM_14;
inline constexpr gpio_num_t kI2sLrcPin = GPIO_NUM_12;
inline constexpr gpio_num_t kI2sDinPin = GPIO_NUM_13;

static_assert((1 << kSineTableBits) == kSineTableSize, "kSineTableSize must match kSineTableBits");
static_assert((kSineTableSize & (kSineTableSize - 1)) == 0, "kSineTableSize must be power of two");
static_assert((kSawTableSamples & (kSawTableSamples - 1)) == 0, "kSawTableSamples must be power of two");
static_assert(kSawMipCount > 0);

} // namespace synth::cfg
