#ifndef MIDI_H
#define MIDI_H

#include <array>
#include <cstdint>

#include "UsbMidi.h"

class Midi {
public:
    Midi();
    ~Midi();
    
    void begin() const;
    void update() const;
    
    void onMidiMessage(void (*callback)(const std::array<uint8_t, 4>&));
    void onDeviceConnected(void (*callback)());
    void onDeviceDisconnected(void (*callback)());

private:
    UsbMidi* usbMidi;
    void (*midiMessageCallback)(const std::array<uint8_t, 4>&);
    
    static void handleMidiMessage(const std::array<uint8_t, 4>& data);
    static void handleDeviceConnected();
    static void handleDeviceDisconnected();
    
    static Midi* instance;
};

#endif // MIDI_H