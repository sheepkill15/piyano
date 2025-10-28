/*
 * ESP32 USB MIDI Host Library (ESP32 USB MIDI Omocha)
 * Copyright (c) 2025 ndenki
 * https://github.com/enudenki/esp32-usb-host-midi-library.git
 */
#include "UsbMidi.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "USBMIDI";

UsbMidi::UsbMidi()
    : _clientHandle(nullptr),
      _deviceHandle(nullptr),
      _midiOutTransfer(nullptr),
      _midiOutQueue(nullptr),
      _midiInterfaceNumber(0),
      _isMidiInterfaceFound(false),
      _areEndpointsReady(false),
      _isMidiOutBusy(false),
      _midiMessageCallback(nullptr),
      _deviceConnectedCallback(nullptr),
      _deviceDisconnectedCallback(nullptr)
{
    for (int i = 0; i < NUM_MIDI_IN_TRANSFERS; ++i) {
        _midiInTransfers[i] = nullptr;
    }
}

UsbMidi::~UsbMidi()
{
    _releaseDeviceResources();
    if (_clientHandle) {
        usb_host_client_deregister(_clientHandle);
    }
    usb_host_uninstall();
    if (_midiOutQueue) {
        vQueueDelete(_midiOutQueue);
    }
}

void UsbMidi::begin()
{
    _midiOutQueue = xQueueCreate(MIDI_OUT_QUEUE_SIZE, sizeof(uint8_t[4]));
    if (!_midiOutQueue) {
        ESP_LOGE(TAG, "Failed to create MIDI OUT queue");
    }

    const usb_host_config_t hostConfig = {
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    esp_err_t err = usb_host_install(&hostConfig);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "USB Host installed");
    } else {
        ESP_LOGE(TAG, "usb_host_install failed: 0x%x", err);
    }

    const usb_host_client_config_t clientConfig = {
        .is_synchronous = false,
        .max_num_event_msg = MAX_CLIENT_EVENT_MESSAGES,
        .async = { .client_event_callback = _clientEventCallback, .callback_arg = this }
    };
    err = usb_host_client_register(&clientConfig, &_clientHandle);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "USB Host client registered");
    } else {
        ESP_LOGE(TAG, "usb_host_client_register failed: 0x%x", err);
    }
}

void UsbMidi::update()
{
    esp_err_t err;

    err = usb_host_client_handle_events(_clientHandle, USB_EVENT_POLL_TICKS);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Error in usb_host_client_handle_events(): 0x%x", err);
    }
    err = usb_host_lib_handle_events(USB_EVENT_POLL_TICKS, nullptr);
    if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
        ESP_LOGE(TAG, "Error in usb_host_lib_handle_events(): 0x%x", err);
    }

    _processMidiOutQueue();
}

void UsbMidi::onMidiMessage(MidiMessageCallback callback)
{
    _midiMessageCallback = callback;
}

bool UsbMidi::sendMidiMessage(const uint8_t* message, uint8_t size)
{
    if (!_midiOutQueue) return false;
    if (size == 0 || size % 4 != 0) return false;

    size_t numMessages = size / 4;

    if (getQueueAvailableSize() < numMessages) {
        ESP_LOGW(TAG, "Not enough space in MIDI OUT queue. Message dropped.");
        return false;
    }

    for (size_t i = 0; i < numMessages; ++i) {
        const uint8_t* currentMessage = message + (i * 4);
        if (xQueueSend(_midiOutQueue, currentMessage, 0) != pdPASS) {
            ESP_LOGE(TAG, "Critical error: Failed to send to queue despite available space.");
            return false;
        }
    }

    return true;
}

bool UsbMidi::noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t message[4] = { (uint8_t)MidiCin::NOTE_ON, (uint8_t)(0x90 | (channel & 0x0F)), (uint8_t)(note & 0x7F), (uint8_t)(velocity & 0x7F) };
    return sendMidiMessage(message, 4);
}

bool UsbMidi::noteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{
    uint8_t message[4] = { (uint8_t)MidiCin::NOTE_OFF, (uint8_t)(0x80 | (channel & 0x0F)), (uint8_t)(note & 0x7F), (uint8_t)(velocity & 0x7F) };
    return sendMidiMessage(message, 4);
}

bool UsbMidi::controlChange(uint8_t channel, uint8_t controller, uint8_t value)
{
    uint8_t message[4] = { (uint8_t)MidiCin::CONTROL_CHANGE, (uint8_t)(0xB0 | (channel & 0x0F)), (uint8_t)(controller & 0x7F), (uint8_t)(value & 0x7F) };
    return sendMidiMessage(message, 4);
}

bool UsbMidi::programChange(uint8_t channel, uint8_t program)
{
    uint8_t message[4] = { (uint8_t)MidiCin::PROGRAM_CHANGE, (uint8_t)(0xC0 | (channel & 0x0F)), (uint8_t)(program & 0x7F), 0 };
    return sendMidiMessage(message, 4);
}

size_t UsbMidi::getQueueAvailableSize() const
{
    if (!_midiOutQueue) {
        return 0;
    }
    return uxQueueSpacesAvailable(_midiOutQueue);
}

void UsbMidi::onDeviceConnected(void (*callback)())
{
    _deviceConnectedCallback = callback;
}

void UsbMidi::onDeviceDisconnected(void (*callback)())
{
    _deviceDisconnectedCallback = callback;
}

void UsbMidi::_clientEventCallback(const usb_host_client_event_msg_t* eventMsg, void* arg)
{
    if (arg == nullptr) return;
    UsbMidi* instance = static_cast<UsbMidi*>(arg);
    instance->_handleClientEvent(eventMsg);
}

void UsbMidi::_handleClientEvent(const usb_host_client_event_msg_t* eventMsg)
{
    esp_err_t err;
    switch (eventMsg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            if (_deviceHandle) {
                ESP_LOGI(TAG, "Ignoring new device, one is already connected.");
                return;
            }
            ESP_LOGI(TAG, "New device connected (address: %d)", eventMsg->new_dev.address);

            err = usb_host_device_open(_clientHandle, eventMsg->new_dev.address, &_deviceHandle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open device: 0x%x", err);
                return;
            }

            const usb_config_desc_t* configDesc;
            err = usb_host_get_active_config_descriptor(_deviceHandle, &configDesc);
            if (err == ESP_OK) {
                _parseConfigDescriptor(configDesc);
            } else {
                ESP_LOGE(TAG, "Failed to get config descriptor: 0x%x", err);
                usb_host_device_close(_clientHandle, _deviceHandle);
                _deviceHandle = nullptr;
            }
            break;

        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            if (eventMsg->dev_gone.dev_hdl == _deviceHandle) {
                ESP_LOGI(TAG, "MIDI device disconnected.");
                _releaseDeviceResources();
                if (_deviceDisconnectedCallback) {
                    _deviceDisconnectedCallback();
                }
            }
            break;

        default:
            break;
    }
}

void UsbMidi::_parseConfigDescriptor(const usb_config_desc_t* configDesc)
{
    const uint8_t* p = &configDesc->val[0];
    const uint8_t* end = p + configDesc->wTotalLength;
    while (p < end) {
        const uint8_t bLength = p[0];

        if (bLength == 0 || (p + bLength) > end) break;

        const uint8_t bDescriptorType = p[1];
        switch (bDescriptorType) {
            case USB_B_DESCRIPTOR_TYPE_INTERFACE:
                ESP_LOGD(TAG, "Found Interface Descriptor");
                if (!_isMidiInterfaceFound) {
                    _findAndClaimMidiInterface(reinterpret_cast<const usb_intf_desc_t*>(p));
                }
                break;
            case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
                ESP_LOGD(TAG, "Found Endpoint Descriptor");
                if (_isMidiInterfaceFound && !_areEndpointsReady) {
                    _setupMidiEndpoints(reinterpret_cast<const usb_ep_desc_t*>(p));
                }
                break;
            default:
                ESP_LOGD(TAG, "Found other descriptor, type: 0x%02X", bDescriptorType);
                break;
        }
        p += bLength;
    }
}

void UsbMidi::_findAndClaimMidiInterface(const usb_intf_desc_t* intf)
{
    if (intf->bInterfaceClass == USB_CLASS_AUDIO && intf->bInterfaceSubClass == USB_AUDIO_SUBCLASS_MIDI_STREAMING) {
        esp_err_t err = usb_host_interface_claim(_clientHandle, _deviceHandle, intf->bInterfaceNumber, intf->bAlternateSetting);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Successfully claimed MIDI interface.");
            _midiInterfaceNumber = intf->bInterfaceNumber;
            _isMidiInterfaceFound = true;
        } else {
            ESP_LOGE(TAG, "Failed to claim MIDI interface: 0x%x", err);
            _isMidiInterfaceFound = false;
        }
    }
}

void UsbMidi::_setupMidiEndpoints(const usb_ep_desc_t* endpoint)
{
    if ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_BULK) return;

    if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK) {
        _setupMidiInEndpoint(endpoint);
    } else {
        _setupMidiOutEndpoint(endpoint);
    }

    if (!_areEndpointsReady && _midiOutTransfer && _midiInTransfers[0]) {
        _areEndpointsReady = true;
        if (_deviceConnectedCallback) _deviceConnectedCallback();
    }
}

void UsbMidi::_setupMidiInEndpoint(const usb_ep_desc_t* endpoint)
{
    for (int i = 0; i < NUM_MIDI_IN_TRANSFERS; i++) {
        if (_midiInTransfers[i] != nullptr) continue;

        esp_err_t err = usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0, &_midiInTransfers[i]);
        if (err == ESP_OK) {
            _midiInTransfers[i]->device_handle = _deviceHandle;
            _midiInTransfers[i]->bEndpointAddress = endpoint->bEndpointAddress;
            _midiInTransfers[i]->callback = _midiTransferCallback;
            _midiInTransfers[i]->context = this;
            _midiInTransfers[i]->num_bytes = endpoint->wMaxPacketSize;
            usb_host_transfer_submit(_midiInTransfers[i]);
        } else {
            _midiInTransfers[i] = nullptr;
        }
    }
}

void UsbMidi::_setupMidiOutEndpoint(const usb_ep_desc_t* endpoint)
{
    if (_midiOutTransfer != nullptr) return;

    esp_err_t err = usb_host_transfer_alloc(endpoint->wMaxPacketSize, 0, &_midiOutTransfer);
    if (err == ESP_OK) {
        _midiOutTransfer->device_handle = _deviceHandle;
        _midiOutTransfer->bEndpointAddress = endpoint->bEndpointAddress;
        _midiOutTransfer->callback = _midiTransferCallback;
        _midiOutTransfer->context = this;
    } else {
        _midiOutTransfer = nullptr;
    }
}

// This callback handles both IN and OUT transfer completions.
// It is always executed in the context of the same, single USB Host Library task.
void UsbMidi::_midiTransferCallback(usb_transfer_t* transfer)
{
    if (transfer == nullptr || transfer->context == nullptr) return;
    UsbMidi* instance = static_cast<UsbMidi*>(transfer->context);
    instance->_handleMidiTransfer(transfer);
}

void UsbMidi::_handleMidiTransfer(usb_transfer_t* transfer)
{
    if (_deviceHandle != transfer->device_handle) {
        return;
    }

    bool isInTransfer = (transfer->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK);

    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED) {
        if (isInTransfer) {
            for (int i = 0; i < transfer->actual_num_bytes; i += 4) {
                uint8_t* const p = &transfer->data_buffer[i];
                uint8_t codeIndexNumber = p[0] & 0x0F;

                // CIN 0 is for miscellaneous system messages, not musical data. We ignore them.
                if (codeIndexNumber != 0 && _midiMessageCallback) {
                    _midiMessageCallback(*reinterpret_cast<const uint8_t (*)[4]>(p));
                }   
            }
            usb_host_transfer_submit(transfer);
        } else { // OutTransfer
            // Explicitly release the lock for the completed transfer.
            _isMidiOutBusy.store(false);

            // Immediately try to process the next batch of messages from the queue
            // to maximize throughput, even if the main loop is slow.
            _processMidiOutQueue();
        }
    } else if (transfer->status != USB_TRANSFER_STATUS_CANCELED) {
        ESP_LOGE(TAG, "MIDI Transfer failed. Endpoint: 0x%02X, Status: %d", transfer->bEndpointAddress, transfer->status);
        if (isInTransfer) {
            usb_host_transfer_submit(transfer);
        } else {
            // On failure, we must release the lock so new transfers can be attempted.
            _isMidiOutBusy.store(false);
        }
    }
}

// This function may be called from both the main task and the USB Host task.
void UsbMidi::_processMidiOutQueue()
{
    if (!_areEndpointsReady || !_midiOutTransfer) {
        return;
    }

    // Atomically acquire the lock. If it's already busy, do nothing.
    bool expected = false;
    if (!_isMidiOutBusy.compare_exchange_strong(expected, true)) {
        return; // Lock acquisition failed, means it's already busy.
    }

    // From this point, we have successfully acquired the "lock" (_isMidiOutBusy is now true).

    if (uxQueueMessagesWaiting(_midiOutQueue) == 0) {
        _isMidiOutBusy.store(false); // Nothing to send, release the lock.
        return;
    }

    size_t bytesToSend = 0;
    uint8_t tempMessage[4];
    size_t maxPacketSize = _midiOutTransfer->data_buffer_size;

    while (bytesToSend + 4 <= maxPacketSize && uxQueueMessagesWaiting(_midiOutQueue) > 0) {
        if (xQueueReceive(_midiOutQueue, tempMessage, 0) == pdPASS) {
            memcpy(_midiOutTransfer->data_buffer + bytesToSend, tempMessage, 4);
            bytesToSend += 4;
        }
    }

    if (bytesToSend > 0) {
        _midiOutTransfer->num_bytes = bytesToSend;
        esp_err_t err = usb_host_transfer_submit(_midiOutTransfer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to submit MIDI OUT transfer: 0x%x", err);
            _isMidiOutBusy.store(false); // Release the lock on failure.
        }
        // On success, the lock will be released in the _handleMidiTransfer() callback
        // after this transfer completes, which might then trigger _processMidiOutQueue() again.
        ESP_LOGD(TAG, "MIDI OUT transfer submitted (result will be returned via callback).");
    } else {
        _isMidiOutBusy.store(false); // Should not happen if queue was not empty, but as a safeguard.
    }
}

void UsbMidi::_releaseDeviceResources()
{
    if (!_deviceHandle) return;

    ESP_LOGI(TAG, "Releasing MIDI device resources...");

    if (_midiOutQueue) {
        xQueueReset(_midiOutQueue);
    }

    for (int i = 0; i < NUM_MIDI_IN_TRANSFERS; ++i) {
        if (_midiInTransfers[i]) {
            usb_host_transfer_free(_midiInTransfers[i]);
            _midiInTransfers[i] = nullptr;
        }
    }
    if (_midiOutTransfer) {
        usb_host_transfer_free(_midiOutTransfer);
        _midiOutTransfer = nullptr;
    }
    if (_isMidiInterfaceFound) {
        usb_host_interface_release(_clientHandle, _deviceHandle, _midiInterfaceNumber);
    }

    usb_host_device_close(_clientHandle, _deviceHandle);

    _deviceHandle = nullptr;
    _isMidiInterfaceFound = false;
    _areEndpointsReady = false;
    _isMidiOutBusy.store(false); // Atomically reset the flag.
    ESP_LOGI(TAG, "MIDI device cleaned up.");
}