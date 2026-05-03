#ifndef SOUND_H
#define SOUND_H

#include "driver/i2s_std.h"
#include <cstddef>
#include <cstdint>

#define I2S_BCLK_PIN  GPIO_NUM_14
#define I2S_LRC_PIN   GPIO_NUM_12
#define I2S_DIN_PIN   GPIO_NUM_13

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
    void setSampleRate(int rate);
    void setAmplitude(float amplitude);

    int sampleRate;
    float masterAmplitude;

    // I2S configuration
    i2s_chan_handle_t tx_handle;
    i2s_std_config_t std_cfg{};

    // DC blocking filter
    float dcFilterState_ = 0.0f;

private:
    void ensureBuffer_(size_t frames) noexcept;

    int16_t* i2sBuf_ = nullptr;
    size_t i2sBufFrames_ = 0;
};

#endif
