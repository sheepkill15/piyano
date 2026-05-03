#ifndef MIDI_H
#define MIDI_H

#include "UsbMidi.h"

class Midi {
public:
    Midi();
    ~Midi();
    
    void begin() const;
    void update() const;
    
    void onMidiMessage(void (*callback)(const uint8_t (&)[4]));
    void onDeviceConnected(void (*callback)());
    void onDeviceDisconnected(void (*callback)());

private:
    UsbMidi* usbMidi;
    void (*midiMessageCallback)(const uint8_t (&)[4]);
    
    static void handleMidiMessage(const uint8_t (&data)[4]);
    static void handleDeviceConnected();
    static void handleDeviceDisconnected();
    
    static Midi* instance;
};

#endif // MIDI_H