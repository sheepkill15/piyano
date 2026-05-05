#include "Sound.h"
#include "engine/AudioContext.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include <cassert>
#include <cstring>

#include "synth/dsp/Approx.h"
#include "synth/dsp/Util.h"

static auto TAG = "SOUND";

Sound::Sound()
    : tx_handle(nullptr)
{
    const auto sr = engine::gAudio.sampleRate;
    std_cfg = i2s_std_config_t{
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sr),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = synth::cfg::kI2sBclkPin,
            .ws = synth::cfg::kI2sLrcPin,
            .dout = synth::cfg::kI2sDinPin,
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

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, nullptr));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));

    ESP_LOGI(TAG, "Sound system initialized - Sample Rate: %lu Hz", static_cast<unsigned long>(engine::gAudio.sampleRate));
}

Sound::~Sound() {
    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
    }
}

void Sound::setSampleRate(const uint32_t rate) {
    engine::gAudio.setSampleRate(rate);
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(engine::gAudio.sampleRate);
    ESP_LOGI(TAG, "Sample rate set to: %lu Hz", static_cast<unsigned long>(engine::gAudio.sampleRate));
}

void Sound::setAmplitude(float amplitude) {
    amplitude = synth::dsp::clamp(amplitude, 0.0f, 1.0f);
    engine::gAudio.setMasterAmplitude(amplitude);
    ESP_LOGI(TAG, "Master amplitude set to: %.2f", engine::gAudio.masterAmplitude);
}

void Sound::write(const float* mono, const size_t frames) {
    const float amp = engine::gAudio.masterAmplitude;
    for (size_t i = 0; i < frames; ++i) {
        float s = mono[i] * amp;
        if (s > 1.0f) s = 1.0f;
        else if (s < -1.0f) s = -1.0f;
        const auto v = static_cast<int16_t>(s * 32767.0f);
        i2sBuf_[2 * i]     = v;
        i2sBuf_[2 * i + 1] = v;
    }

    size_t bytes_written;
    ESP_ERROR_CHECK(i2s_channel_write(tx_handle, i2sBuf_.data(), frames * 2 * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY));
}

void Sound::writeStereoInterleaved(const float* stereoLR, const size_t frames) {
    const float amp = engine::gAudio.masterAmplitude;
    for (size_t i = 0; i < frames; ++i) {
        const float l = synth::dsp::clamp(stereoLR[2 * i]     * amp, -1.0f, 1.0f);
        const float r = synth::dsp::clamp(stereoLR[2 * i + 1] * amp, -1.0f, 1.0f);
        i2sBuf_[2 * i]     = static_cast<int16_t>(l * 32767.0f);
        i2sBuf_[2 * i + 1] = static_cast<int16_t>(r * 32767.0f);
    }

    size_t bytes_written;
    ESP_ERROR_CHECK(i2s_channel_write(tx_handle, i2sBuf_.data(), frames * 2 * sizeof(int16_t),
                                      &bytes_written, portMAX_DELAY));
}
