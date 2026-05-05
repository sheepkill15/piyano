#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "Midi.h"
#include "synth/Constants.h"
#include "workstation/Workstation.h"
#include <array>

static auto TAG = "PIYANO";

Midi midi;
Workstation workstation;

// MIDI callback functions
void onMidiNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity);
void onMidiNoteOff(uint8_t channel, uint8_t pitch);
void onMidiPitchBend(uint8_t channel, uint8_t lsb, uint8_t msb);
void handleMidiMessage(const std::array<uint8_t, 4>& data);

TaskHandle_t soundTaskHandle;

[[noreturn]] void soundTask(void *parameter) {
    for (;;) {
        workstation.renderAndWrite();
    }
}

extern "C" [[noreturn]] void app_main()
{
    ESP_LOGI(TAG, "Piyano - MIDI Piano with I2S Audio");

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << 4);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_4, 1);

    workstation.begin();
    
    // Initialize MIDI with custom handler
    midi.onMidiMessage(handleMidiMessage);
    midi.begin();
    xTaskCreatePinnedToCore(
        soundTask,          // task function
        "SoundTask",        // name
        8192,               // stack size
        nullptr,               // parameters
        18,                 // priority: keep above UI/MIDI so render meets realtime
        &soundTaskHandle,   // handle
        1                   // core number (0 or 1)
      );
    ESP_LOGI(TAG, "System ready!");

    // Main loop
    int64_t lastUpdate = 0;
    while (true) {
        midi.update();
        timeval tv_now{};
        gettimeofday(&tv_now, nullptr);
        int64_t time_us = tv_now.tv_sec * 1000000L + static_cast<int64_t>(tv_now.tv_usec);
        workstation.update(static_cast<float>(time_us - lastUpdate) / 1000000.0f);
        lastUpdate = time_us;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// MIDI message handler
void handleMidiMessage(const std::array<uint8_t, 4>& data) {
    auto cin = static_cast<MidiCin>(data[0] & 0x0F);
    uint8_t channel = data[1] & 0x0F;

    switch (cin) {
        case MidiCin::NOTE_ON: {
            uint8_t pitch = data[2];
            uint8_t velocity = data[3];
            ESP_LOGI(TAG, "NoteOn: ch:%d / pitch:%d / vel:%d", channel, pitch, velocity);
            onMidiNoteOn(channel, pitch, velocity);
            break;
        }

        case MidiCin::NOTE_OFF: {
            uint8_t pitch = data[2];
            ESP_LOGI(TAG, "NoteOff: ch:%d / key:%d", channel, pitch);
            onMidiNoteOff(channel, pitch);
            break;
        }

        case MidiCin::CONTROL_CHANGE: {
            uint8_t control = data[2];
            uint8_t value = data[3];
            workstation.handleControlChange(channel, control, value);
            ESP_LOGI(TAG, "Control Change: ch:%d / cc:%d / v:%d", channel, control, value);
            break;
        }

        case MidiCin::PROGRAM_CHANGE: {
            uint8_t program = data[2];
            workstation.handleProgramChange(channel, program);
            ESP_LOGI(TAG, "Program Change: program:%d", program);
            break;
        }
        case MidiCin::PITCH_BEND: {
            const uint8_t lsb = data[2] & 0x7F;
            const uint8_t msb = data[3] & 0x7F;
            onMidiPitchBend(channel, lsb, msb);
            break;
        }
        default:
            break;
    }
}

void onMidiNoteOn(const uint8_t channel, const uint8_t pitch, const uint8_t velocity) {
    const float amplitude = static_cast<float>(velocity) / 127.0f;
    if(velocity == 0) {
        workstation.noteOff(channel, pitch);
        return;
    }
    workstation.noteOn(channel, pitch, amplitude);
}

void onMidiNoteOff(const uint8_t channel, const uint8_t pitch) {
    workstation.noteOff(channel, pitch);
}

void onMidiPitchBend(const uint8_t channel, const uint8_t lsb, const uint8_t msb) {
    const uint16_t bend14 = static_cast<uint16_t>(lsb) | (static_cast<uint16_t>(msb) << 7);
    workstation.handlePitchBend(channel, bend14);
}
