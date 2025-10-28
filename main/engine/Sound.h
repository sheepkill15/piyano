#ifndef SOUND_H
#define SOUND_H

#include "driver/i2s_std.h"
#include <vector>

#define I2S_BCLK_PIN  GPIO_NUM_14
#define I2S_LRC_PIN   GPIO_NUM_12
#define I2S_DIN_PIN   GPIO_NUM_13

class Sound {
public:
    Sound();
    ~Sound();
    
    void begin();
    void write(const float* data, size_t size);
    
    // Configuration
    void setSampleRate(int rate);
    void setAmplitude(float amplitude);
    
    int sampleRate;
    float masterAmplitude;
    
    // I2S configuration
    i2s_chan_handle_t tx_handle;
    i2s_std_config_t std_cfg;
};

#endif
