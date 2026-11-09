#include "usb_midi_link.h"

#include <string.h>

#include "usbd_def.h"

using namespace daisy;

// libdaisy/src/hid/usb.cpp:16
extern "C" USBD_HandleTypeDef hUsbDeviceFS;

// C++14: odr-used static constexpr members need a namespace-scope definition.
constexpr size_t UsbMidiLink::kReplyReserve;

void UsbMidiLink::Init() {
    MidiUsbTransport::Config config;
    config.periph = MidiUsbTransport::Config::INTERNAL;
    transport_.Init(config);
    transport_.StartRx(rxCallback, this);
}

void UsbMidiLink::Service() {
    // MidiUsbTransport stops receiving when its ring buffer overflows
    // (hid/usb_midi.cpp:187-192); it drains on every packet, so this is a safety net.
    if (!transport_.RxActive())
        transport_.StartRx(rxCallback, this);
}

void UsbMidiLink::rxCallback(uint8_t* data, size_t size, void* context) {
    static_cast<UsbMidiLink*>(context)->in_.Feed(data, size);
}

// A host has configured the device. Reports are sent without a host request, and
// CDC_Transmit_FS() dereferences pClassData unchecked (usbd/usbd_cdc_if.c:301-303), so
// nothing is queued or transmitted before this holds (e.g. on 9 V power with no cable).
bool UsbMidiLink::configured() {
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != nullptr;
}

bool UsbMidiLink::QueueSysEx(const uint8_t* msg, size_t len) {
    if (!configured())
        return false;
    const size_t n = PackSysExUsbMidi(msg, len, queue_ + queueLen_, QueueFree());
    queueLen_ += n;
    return n > 0;
}

uint8_t* UsbMidiLink::ReserveReport(size_t& cap) {
    const size_t free = QueueFree();
    cap = configured() && free > kReplyReserve ? free - kReplyReserve : 0;
    return queue_ + queueLen_;
}

void UsbMidiLink::Flush() {
    if (queueLen_ == 0 || !configured())
        return;
    const size_t n = queueLen_ < sizeof txBuf_[0] ? queueLen_ : sizeof txBuf_[0];
    memcpy(txBuf_[txCur_], queue_, n);
    // CDC_Transmit_FS() returns busy while the previous IN transfer is in flight: the
    // queue is kept as it is and the next call retries.
    if (usb_.TransmitInternal(txBuf_[txCur_], n) != UsbHandle::Result::OK)
        return;
    queueLen_ -= n;
    memmove(queue_, queue_ + n, queueLen_);
    txCur_ ^= 1;
}

void UsbMidiLink::FlushFor(uint32_t ms) {
    const uint32_t start = System::GetNow();
    while (queueLen_ > 0 && System::GetNow() - start < ms)
        Flush();
}

bool UsbMidiLink::WaitForSpace(size_t bytes, uint32_t ms) {
    const uint32_t start = System::GetNow();
    while (QueueFree() < bytes && System::GetNow() - start < ms)
        Flush();
    return QueueFree() >= bytes;
}
