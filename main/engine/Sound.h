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
    using StereoBlock = std::array<float, synth::cfg::kAudioRenderBlockSamples * 2>;
    [[nodiscard]] StereoBlock& stereoBlock() noexcept { return stereoBlock_; }
    void writeStereoInterleaved(const StereoBlock& stereoLR);

    // Configuration
    void setSampleRate(uint32_t rate);
    void setAmplitude(float amplitude);

    // I2S configuration
    i2s_chan_handle_t tx_handle;
    i2s_std_config_t std_cfg{};

    // DC blocking filter
    float dcFilterState_ = 0.0f;

private:
    static constexpr std::size_t kFrames = synth::cfg::kAudioRenderBlockSamples;
    StereoBlock stereoBlock_{};
    std::array<int16_t, kFrames * 2> i2sBuf_{};
};

#endif
