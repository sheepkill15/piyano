#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "Midi.h"
#include "instruments/InstrumentManager.h"
#include "engine/SynthEngine.h"
#include "engine/Sound.h"
#include <vector>
#include <cmath>

static const char *TAG = "PIYANO";

// Global instances
Midi midi;
SynthEngine synth;
InstrumentManager manager;
Sound sound;

int currentInstrument = 0;

// MIDI callback functions
void onMidiNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity);
void onMidiNoteOff(uint8_t channel, uint8_t pitch);
void handleMidiMessage(const uint8_t (&data)[4]);

// Utility functions
float midiNoteToFrequency(uint8_t note);
int findActiveNote(uint8_t pitch);
void removeActiveNote(uint8_t pitch);

TaskHandle_t soundTaskHandle;

void soundTask(void *parameter) {
    const int bufferSize = 256;
    float buffer[bufferSize];
    
    for (;;) {
      synth.render(buffer, bufferSize);
      sound.write(buffer, bufferSize);
    }
}

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Piyano - MIDI Piano with I2S Audio");

    // Initialize GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << 4);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_4, 1);

    sound.begin();
    synth.init(manager.get(currentInstrument), static_cast<float>(sound.sampleRate));
    
    // Initialize MIDI with custom handler
    midi.onMidiMessage(handleMidiMessage);
    midi.begin();
    xTaskCreatePinnedToCore(
        soundTask,          // task function
        "SoundTask",        // name
        4096,               // stack size
        NULL,               // parameters
        1,                  // priority (1–5; higher = more priority)
        &soundTaskHandle,   // handle
        1                   // core number (0 or 1)
      );
    ESP_LOGI(TAG, "System ready!");

    // Main loop
    int64_t lastUpdate = 0;
    while (1) {
        midi.update();
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
        synth.update((time_us - lastUpdate) / 1000000.0f);
        lastUpdate = time_us;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// MIDI message handler
void handleMidiMessage(const uint8_t (&data)[4]) {
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
            if(channel == 15) {
                if(control == 20) {
                    // volume control
                    float volume = value / 127.0f;
                    sound.setAmplitude(volume);
                }
                if(control == 112 && value == 127) {
                    // instrument change - next
                    currentInstrument = (currentInstrument + 1) % manager.getInstrumentCount();
                    synth.switchInstrument(manager.get(currentInstrument));
                    ESP_LOGI(TAG, "Instrument changed to: %d", currentInstrument);
                }
                if(control == 12) { // attack
                    float attack = value / 127.0f;
                    ((FMInstrument*)manager.get(currentInstrument))->setAttack(attack);
                }
                if(control == 13) { // decay
                    float decay = value / 127.0f;
                    ((FMInstrument*)manager.get(currentInstrument))->setDecay(decay);
                }
                if(control == 14) { // sustain
                    float sustain = value / 127.0f;
                    ((FMInstrument*)manager.get(currentInstrument))->setSustain(sustain);
                }
                if(control == 15) { // release
                    float release = value / 127.0f;
                    ((FMInstrument*)manager.get(currentInstrument))->setRelease(release);
                }
            }
            ESP_LOGI(TAG, "Control Change: ch:%d / cc:%d / v:%d", channel, control, value);
            break;
        }

        case MidiCin::PROGRAM_CHANGE: {
            uint8_t program = data[2];
            ESP_LOGI(TAG, "Program Change: program:%d", program);
            break;
        }
        default:
            break;
    }
}

void onMidiNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) {
    float frequency = midiNoteToFrequency(pitch);
    float amplitude = velocity / 127.0f;
    
    synth.noteOn(frequency, amplitude);
}

void onMidiNoteOff(uint8_t channel, uint8_t pitch) {
    float frequency = midiNoteToFrequency(pitch);
    synth.noteOff(frequency);
}

float midiNoteToFrequency(uint8_t note) {
    // MIDI note 69 (A4) = 440 Hz
    return 440.0f * pow(2.0f, (note - 69) / 12.0f);
}