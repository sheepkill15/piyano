#pragma once

#include <cstdint>

namespace Params {
    static constexpr uint16_t MasterGain = 10; // global mixer gain (0..1 typical)
    static constexpr uint16_t Expression = 11; // post-master gain scaler (0..1)

    static constexpr uint16_t Attack = 1;
    static constexpr uint16_t Decay = 2;
    static constexpr uint16_t Sustain = 3;
    static constexpr uint16_t Release = 4;

    static constexpr uint16_t FmModIndex = 100;
    static constexpr uint16_t FmModRatio = 101;

    static constexpr uint16_t FilterCutoff = 200;    // musical "brightness" macro
    static constexpr uint16_t FilterResonance = 201; // macro
    static constexpr uint16_t FilterLfoAmt = 202;    // macro

    static constexpr uint16_t Drive = 220;       // macro
    static constexpr uint16_t NoiseLevel = 230;  // macro
    static constexpr uint16_t NoiseColor = 231;  // macro

    static constexpr uint16_t Vibrato = 240;     // macro
    static constexpr uint16_t PitchBend = 250;   // -1..+1 normalized
}

