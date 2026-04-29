#pragma once

namespace engine {

struct AudioContext {
    float sampleRate = 44100.0f;
    float invSampleRate = 1.0f / 44100.0f;

    void setSampleRate(float sr) noexcept {
        if (sr <= 0.0f) sr = 44100.0f;
        sampleRate = sr;
        invSampleRate = 1.0f / sr;
    }
};

extern AudioContext gAudio;

} // namespace engine

