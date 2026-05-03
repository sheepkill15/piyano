#include "Midi.h"
#include "esp_log.h"

static auto TAG = "MIDI";

Midi* Midi::instance = nullptr;

Midi::Midi() {
    usbMidi = new UsbMidi();
    midiMessageCallback = nullptr;
    instance = this;
}

Midi::~Midi() {
    delete usbMidi;
    instance = nullptr;
}

void Midi::begin() const
{
    usbMidi->onMidiMessage(handleMidiMessage);
    usbMidi->onDeviceConnected(handleDeviceConnected);
    usbMidi->onDeviceDisconnected(handleDeviceDisconnected);
    usbMidi->begin();
}

void Midi::update() const
{
    usbMidi->update();
}

void Midi::onMidiMessage(void (*callback)(const uint8_t (&)[4])) {
    // Store callback for use in static handler
    midiMessageCallback = callback;
}

void Midi::onDeviceConnected(void (*callback)()) {
    // Store callback for use in static handler
}

void Midi::onDeviceDisconnected(void (*callback)()) {
    // Store callback for use in static handler
}

void Midi::handleMidiMessage(const uint8_t (&data)[4]) {
    if (!instance || !instance->midiMessageCallback) return;
    
    // Call the user-provided callback
    instance->midiMessageCallback(data);
}

void Midi::handleDeviceConnected() {
    if (!instance) return;
    ESP_LOGI(TAG, "onMidiDeviceConnected");
}

void Midi::handleDeviceDisconnected() {
    if (!instance) return;
    ESP_LOGI(TAG, "onMidiDeviceDisconnected");
}