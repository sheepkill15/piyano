#ifndef SOUND_H
#define SOUND_H

#include "driver/i2s_std.h"
#include <array>
#include <cstddef>
#include <cstdint>

#include "synth/Constants.h"

class Sound {
public:
    Sound();
    ~Sound();

    void begin();
    // Mono frames -> duplicated to L/R
    void write(const float* mono, size_t frames);
    // Interleaved stereo frames: LRLRLR...
    void writeStereoInterleaved(const float* stereoLR, size_t frames);

    // Configuration
    void setSampleRate(uint32_t rate);
    void setAmplitude(float amplitude);

    // I2S configuration
    i2s_chan_handle_t tx_handle;
    i2s_std_config_t std_cfg{};

    // DC blocking filter
    float dcFilterState_ = 0.0f;

private:
    static constexpr std::size_t kFrames = static_cast<std::size_t>(synth::cfg::kAudioRenderBlockSamples);
    std::array<int16_t, kFrames * 2> i2sBuf_{};
};

#endif
