#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace synth::dsp {

// ---------------------------------------------------------------------------
// Sine wavetable: 2048 samples + 1 wrap entry, linear interpolation.
// ---------------------------------------------------------------------------
constexpr int kSineBits = 11;
constexpr int kSineSize = 1 << kSineBits;        // 2048
constexpr int kSineMask = kSineSize - 1;

extern float gSineTable[kSineSize + 1];

// ---------------------------------------------------------------------------
// Bandlimited saw mipmap: one table per ~octave, sized for the highest
// fundamental in that band so harmonics never alias above Nyquist (designed
// at 44.1 kHz, safe for slightly higher rates because of the half-octave
// headroom). 1024 samples + wrap, linear interpolation.
// ---------------------------------------------------------------------------
constexpr int kSawTables = 10;
constexpr int kSawTableSize = 1024;
constexpr int kSawTableMask = kSawTableSize - 1;

extern float gSawTable[kSawTables][kSawTableSize + 1];
extern const float gSawTableMaxFreq[kSawTables];

void initWaveTables() noexcept;

// Linearly interpolated sine for phase in [0, 1). Caller must keep phase wrapped.
inline float sineLU(const float phase01) noexcept {
    const float idxF = phase01 * static_cast<float>(kSineSize);
    const int i = static_cast<int>(idxF);
    const float frac = idxF - static_cast<float>(i);
    const int i0 = i & kSineMask;
    const float a = gSineTable[i0];
    const float b = gSineTable[i0 + 1];
    return a + (b - a) * frac;
}

// Sine for an arbitrary radians angle (FM-style modulated phase). Handles wrap.
inline float sineLURad(const float angleRad) noexcept {
    constexpr float kInv2Pi = 0.15915494309189535f;
    float p = angleRad * kInv2Pi;
    p -= floorf(p);
    return sineLU(p);
}

// Pick a saw mipmap table for fundamental f (Hz). Cheap branchless-ish ladder.
inline int sawTableIdx(const float f) noexcept {
    int idx = 0;
    while (idx < kSawTables - 1 && f >= gSawTableMaxFreq[idx]) {
        ++idx;
    }
    return idx;
}

// Bandlimited sawtooth lookup; phase01 in [0, 1).
inline float sawLU(const int tableIdx, const float phase01) noexcept {
    const float idxF = phase01 * static_cast<float>(kSawTableSize);
    const int i = static_cast<int>(idxF);
    const float frac = idxF - static_cast<float>(i);
    const int i0 = i & kSawTableMask;
    const float* tbl = gSawTable[tableIdx];
    const float a = tbl[i0];
    const float b = tbl[i0 + 1];
    return a + (b - a) * frac;
}

} // namespace synth::dsp
