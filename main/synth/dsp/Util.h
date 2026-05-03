#pragma once

#include <cmath>

namespace synth::dsp {

template<typename T>
T clamp(const T x, const T lo, const T hi) noexcept {
    return x < lo ? lo : x > hi ? hi : x;
}

inline float midiNoteToHz(const uint8_t note) noexcept {
    return 440.0f * powf(2.0f, (static_cast<float>(note) - 69.0f) / 12.0f);
}

} // namespace synth::dsp
