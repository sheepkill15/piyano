/*
 * ESP32 USB MIDI Host Library (ESP32 USB MIDI Omocha)
 * Copyright (c) 2025 ndenki
 * https://github.com/enudenki/esp32-usb-host-midi-library.git
 */
#ifndef USBMIDI_H
#define USBMIDI_H

#include <usb/usb_host.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <array>
#include <atomic>
#include <cstdint>

#include "synth/Constants.h"

#define USB_MIDI_DEBUG 1

#if USB_MIDI_DEBUG
#define USB_MIDI_LOG(format, ...)                 \
    do {                                          \
        if (this->_debugSerial) {                 \
            this->_debugSerial->printf(format, ##__VA_ARGS__); \
        }                                         \
    } while (0)
#else
#define USB_MIDI_LOG(format, ...) \
    do {                          \
    } while (0)
#endif

enum class MidiCin : uint8_t {
    NOTE_OFF = 0x08,
    NOTE_ON = 0x09,
    CONTROL_CHANGE = 0x0B,
    PROGRAM_CHANGE = 0x0C,
};

class UsbMidi {
public:
    using MidiMessageCallback = void (*)(const std::array<uint8_t, 4>&);
    

    UsbMidi();
    ~UsbMidi();

    void begin();
    void update();

    void onMidiMessage(MidiMessageCallback callback);

    bool sendMidiMessage(const uint8_t* message, uint8_t size = 4) const;
    [[nodiscard]] bool noteOn(uint8_t channel, uint8_t note, uint8_t velocity) const;
    [[nodiscard]] bool noteOff(uint8_t channel, uint8_t note, uint8_t velocity) const;
    [[nodiscard]] bool controlChange(uint8_t channel, uint8_t controller, uint8_t value) const;
    [[nodiscard]] bool programChange(uint8_t channel, uint8_t program) const;

    [[nodiscard]] size_t getQueueAvailableSize() const;

    void onDeviceConnected(void (*callback)());
    void onDeviceDisconnected(void (*callback)());

private:
    static void _clientEventCallback(const usb_host_client_event_msg_t* eventMsg, void* arg);
    static void _midiTransferCallback(usb_transfer_t* transfer);

    void _handleClientEvent(const usb_host_client_event_msg_t* eventMsg);
    void _handleMidiTransfer(usb_transfer_t* transfer);
    void _parseConfigDescriptor(const usb_config_desc_t* configDesc);
    void _findAndClaimMidiInterface(const usb_intf_desc_t* intf);
    void _setupMidiEndpoints(const usb_ep_desc_t* endpoint);
    void _setupMidiInEndpoint(const usb_ep_desc_t* endpoint);
    void _setupMidiOutEndpoint(const usb_ep_desc_t* endpoint);
    void _releaseDeviceResources();
    void _processMidiOutQueue();

    usb_host_client_handle_t _clientHandle;
    usb_device_handle_t _deviceHandle;
    usb_transfer_t* _midiOutTransfer;
    std::array<usb_transfer_t*, static_cast<std::size_t>(synth::cfg::kUsbMidiInTransfers)> _midiInTransfers{};
    QueueHandle_t _midiOutQueue;
    uint8_t _midiInterfaceNumber;
    bool _isMidiInterfaceFound;
    bool _areEndpointsReady;
    std::atomic<bool> _isMidiOutBusy; 
    MidiMessageCallback _midiMessageCallback;
    void (*_deviceConnectedCallback)();
    void (*_deviceDisconnectedCallback)();
};

#endif // USBMIDI_H