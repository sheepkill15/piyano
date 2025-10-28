#include "Sound.h"
#include "esp_log.h"
#include <math.h>
#include "freertos/FreeRTOS.h"

static const char *TAG = "SOUND";

Sound::Sound() 
    : sampleRate(44100)
    , masterAmplitude(0.1f)
    , tx_handle(nullptr)
{
    // Initialize I2S standard configuration
    std_cfg = i2s_std_config_t{
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(static_cast<uint32_t>(sampleRate)),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws = I2S_LRC_PIN,
            .dout = I2S_DIN_PIN,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
}

void Sound::begin() {
    ESP_LOGI(TAG, "Initializing Sound system...");
    
    // Configure I2S channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return;
    }
    
    // Initialize the standard configuration
    ret = i2s_channel_init_std_mode(tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S channel in std mode: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = i2s_channel_enable(tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        return;
    }
    
    ESP_LOGI(TAG, "Sound system initialized - Sample Rate: %d Hz", sampleRate);
}

Sound::~Sound() {
    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
    }
}

void Sound::setSampleRate(int rate) {
    sampleRate = rate;
    ESP_LOGI(TAG, "Sample rate set to: %d Hz", rate);
}

void Sound::setAmplitude(float amplitude) {
    if (amplitude > 1.0f) amplitude = 1.0f;
    else if (amplitude < 0.0f) amplitude = 0.0f;
    masterAmplitude = amplitude;
    ESP_LOGI(TAG, "Master amplitude set to: %.2f", masterAmplitude);
}

void Sound::write(const int16_t* data, size_t size) {
    if (!tx_handle) return;
    
    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(tx_handle, data, size * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
    }
}