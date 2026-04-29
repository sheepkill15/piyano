#include "Sound.h"
#include "engine/AudioContext.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <cstdlib>
#include <cstring>

static const char *TAG = "SOUND";

Sound::Sound()
    : sampleRate(44100)
    , masterAmplitude(1.0f)
    , tx_handle(nullptr)
{
    engine::gAudio.setSampleRate(static_cast<float>(sampleRate));
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
    engine::gAudio.setSampleRate(static_cast<float>(sampleRate));

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return;
    }

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
    if (i2sBuf_) {
        std::free(i2sBuf_);
        i2sBuf_ = nullptr;
        i2sBufFrames_ = 0;
    }
}

void Sound::setSampleRate(int rate) {
    sampleRate = rate;
    engine::gAudio.setSampleRate(static_cast<float>(sampleRate));
    ESP_LOGI(TAG, "Sample rate set to: %d Hz", rate);
}

void Sound::setAmplitude(float amplitude) {
    if (amplitude > 1.0f) amplitude = 1.0f;
    else if (amplitude < 0.0f) amplitude = 0.0f;
    masterAmplitude = amplitude;
    ESP_LOGI(TAG, "Master amplitude set to: %.2f", masterAmplitude);
}

void Sound::ensureBuffer_(size_t frames) noexcept {
    if (frames <= i2sBufFrames_ && i2sBuf_) return;
    if (i2sBuf_) std::free(i2sBuf_);
    i2sBuf_ = static_cast<int16_t*>(std::malloc(frames * 2 * sizeof(int16_t)));
    i2sBufFrames_ = i2sBuf_ ? frames : 0;
}

void Sound::write(const float* mono, size_t frames) {
    if (!tx_handle || !mono || frames == 0) return;

    ensureBuffer_(frames);
    if (!i2sBuf_) return;

    const float amp = masterAmplitude;
    for (size_t i = 0; i < frames; ++i) {
        float s = mono[i] * amp;
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
        const int16_t v = static_cast<int16_t>(s * 32767.0f);
        i2sBuf_[2 * i]     = v;
        i2sBuf_[2 * i + 1] = v;
    }

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(tx_handle, i2sBuf_, frames * 2 * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
    }
}

void Sound::writeStereoInterleaved(const float* stereoLR, size_t frames) {
    if (!tx_handle || !stereoLR || frames == 0) return;

    ensureBuffer_(frames);
    if (!i2sBuf_) return;

    const float amp = masterAmplitude;
    for (size_t i = 0; i < frames; ++i) {
        float l = stereoLR[2 * i]     * amp;
        float r = stereoLR[2 * i + 1] * amp;
        if (l > 1.0f) l = 1.0f; else if (l < -1.0f) l = -1.0f;
        if (r > 1.0f) r = 1.0f; else if (r < -1.0f) r = -1.0f;
        i2sBuf_[2 * i]     = static_cast<int16_t>(l * 32767.0f);
        i2sBuf_[2 * i + 1] = static_cast<int16_t>(r * 32767.0f);
    }

    size_t bytes_written;
    esp_err_t ret = i2s_channel_write(tx_handle, i2sBuf_, frames * 2 * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
    }
}
