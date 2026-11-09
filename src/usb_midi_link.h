#ifndef USB_MIDI_LINK_H
#define USB_MIDI_LINK_H

#include <stddef.h>
#include <stdint.h>

#include "daisy_seed.h"
#include "preset_protocol.h"

// The Seed's micro-USB port as a class-compliant USB-MIDI device carrying the preset
// protocol (preset_protocol.h) and MIDI live control (midi_control.h).
//
// Receive uses libdaisy's MidiUsbTransport directly: MidiHandler truncates SysEx at
// SYSEX_BUFFER_LEN = 128 (hid/MidiEvent.h:2). The USB OTG FS interrupt shares priority 0
// with the audio DMA (usbd/usbd_conf.c:102-107, sys/dma.c:17-50), so the receive callback
// does nothing but MidiInQueue::Feed(); the main loop Pop()s the queued SysEx frames and
// channel messages in arrival order.
//
// Send is a 4 KB queue of USB-MIDI 1.0 event packets, drained by Flush() from the main
// loop without ever waiting. Replies bypass MidiUsbTransport::Tx(), which splits at every
// status byte and so sends the closing F7 in a packet of its own (hid/usb_midi.cpp:294-318):
// they are packed by PackSysExUsbMidi() and sent with UsbHandle::TransmitInternal(), the
// same CDC path. Flush() copies up to 512 bytes into one of two transfer buffers and
// starts the IN transfer; the next Flush() fills the other buffer, so the buffer of the
// transfer in flight is never touched, and a busy endpoint simply leaves the queue for
// the next call. The state report (MidiReporter) is written straight into the queue
// tail, always leaving kReplyReserve free so a reply is never refused for lack of room.
class UsbMidiLink
{
public:
    // Room kept free for one protocol reply, packed.
    static constexpr size_t kReplyReserve = (PresetProtocol::kMaxReply + 2) / 3 * 4;

    void Init();     // boot, before hw.StartAudio(); blocks ~10 ms (usb_midi.cpp:125)
    void Service();  // main loop: restarts reception after a ring-buffer overflow
    // Main loop: the oldest received message (see MidiInQueue::Pop()); 0 = none.
    size_t Pop(uint8_t* out) { return in_.Pop(out); }

    // Main loop. Queues one complete SysEx message, F0 ... F7; false if it does not
    // fit or the device is not configured.
    bool QueueSysEx(const uint8_t* msg, size_t len);
    // Main loop. The queue tail and, in `cap`, how many bytes may be written there
    // (free space minus kReplyReserve; 0 while not configured). CommitReport(n) queues
    // the n bytes written, whole USB-MIDI packets only.
    uint8_t* ReserveReport(size_t& cap);
    void     CommitReport(size_t n) { queueLen_ += n; }
    size_t   QueueFree() const { return sizeof queue_ - queueLen_; }

    void Flush();                                   // main loop: starts the next IN transfer
    void FlushFor(uint32_t ms);                     // Flush() until empty or `ms` passed
    bool WaitForSpace(size_t bytes, uint32_t ms);   // Flush() until QueueFree() >= bytes

private:
    static void rxCallback(uint8_t* data, size_t size, void* context);  // USB ISR
    static bool configured();

    daisy::MidiUsbTransport transport_;
    daisy::UsbHandle        usb_;  // stateless; TransmitInternal() only
    MidiInQueue             in_;
    uint8_t queue_[4096];
    size_t  queueLen_ = 0;
    // Transfer buffers, alternated: one may be in flight while Flush() fills the other.
    uint8_t txBuf_[2][512];
    int     txCur_ = 0;
};

#endif
