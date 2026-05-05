#pragma once

#include <cstdint>

namespace engine {

struct AudioContext {
    uint32_t sampleRate = 44100u;
    float invSampleRate = 1.0f / 44100.0f;
    float masterAmplitude = 1.0f;

    void setSampleRate(uint32_t sr) noexcept {
        if (sr == 0u) sr = 44100u;
        sampleRate = sr;
        invSampleRate = 1.0f / static_cast<float>(sr);
    }

    void setMasterAmplitude(float amp) noexcept {
        if (amp < 0.0f) amp = 0.0f;
        if (amp > 1.0f) amp = 1.0f;
        masterAmplitude = amp;
    }
};

extern AudioContext gAudio;

} // namespace engine

