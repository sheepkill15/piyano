#pragma once

#include <array>
#include <cstdint>

struct PresetParam {
    uint16_t id = 0;
    float value = 0.0f; // normalized 0..1 unless param defines otherwise
};

struct Preset {
    static constexpr size_t MAX_PARAMS = 16;

    const char* name = "Init";
    uint8_t instrumentIndex = 0;
    uint8_t paramCount = 0;
    std::array<PresetParam, MAX_PARAMS> params = {};
};

