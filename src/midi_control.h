#ifndef MIDI_CONTROL_H
#define MIDI_CONTROL_H

// MIDI live control (docs/MIDI.md): every file-controlled preset value plus Engage as
// a 7-bit CC and a 14-bit NRPN with the same number. Received on any channel, sent on
// channel 1. Host-portable: no libdaisy.

#include <stddef.h>
#include <stdint.h>

#include "preset_bank.h"

enum class MidiTarget : uint8_t { Param, ReverseDelay, ReverseEnabled, ReverseDirectMix,
                                  DelayLines, Engage };

struct MidiControlDef {
    uint8_t     number;  // CC number = NRPN LSB (NRPN MSB 0)
    MidiTarget  target;
    ::Parameter param;   // Param only; Parameter::Unused otherwise
    bool        onOff;   // on when >= 0.5, written and reported as exactly 1 / 0
};

constexpr int kMidiControlCount = 54;
extern const MidiControlDef kMidiControls[kMidiControlCount];

// kMidiControls index of a CC/NRPN number, or -1 when the number has no control.
int MidiControlIndex(uint8_t number);

// 7-bit and 14-bit values to 0..1. The divisor is volatile so -ffast-math cannot turn
// the division into a multiply by a rounded reciprocal: the editor computes the same
// value as Math.fround(q / 16383), bit for bit.
float NormFromCc(uint8_t c);
float NormFromQ14(uint16_t q);

// The delay-line control. Terrarium: >= 0.5 Max, else Default. HotHouse: < 0.25 One,
// < 0.75 Default, else Max. Reported as Default 0 / Max 1 (Terrarium) and One 0 /
// Default 0.5 / Max 1 (HotHouse).
DelayLinesMode DelayLinesFromNorm(Board board, float v);
float DelayLinesNorm(Board board, DelayLinesMode mode);

struct MidiEvent {
    enum class Type : uint8_t { None, Control, Program } type = Type::None;
    uint8_t index = 0;     // Control: kMidiControls index; Program: program number
    float   value = 0.0f;  // Control: 0..1
};

// Turns received channel messages, [Bn cc v] or [Cn p], into control writes and
// program changes. NRPN state is kept per channel: CC99/CC98 select a parameter
// (MSB 0, LSB a table number, not 127), CC6 applies q = msb << 7, CC38 applies
// q = msb << 7 | lsb with the last CC6 since the selection; CC101/CC100 deselect until
// the next CC99/CC98; CC96/CC97 are ignored.
class MidiInDecoder {
public:
    MidiEvent Feed(const uint8_t* msg, size_t len);

private:
    struct Channel {
        uint8_t msb = 127, lsb = 127, dataMsb = 0;
        bool    rpn = false;
    } ch_[16];
};

// Reports the live state as USB-MIDI 1.0 event packets on cable 0, channel 1: a
// Program Change when the program differs from the last one reported, then per
// control in table order a CC when its 7-bit value changed and an NRPN (CC99 0, CC98 n,
// CC6, CC38) when its 14-bit value changed.
class MidiReporter {
public:
    static constexpr size_t kMaxBytes = 4 + kMidiControlCount * 20;  // PC + CC + 4 NRPN CCs each

    MidiReporter() { Invalidate(); }
    // Forgets everything reported: the next Collect() reports the full state.
    void Invalidate();
    // `values[i]` is control i's 0..1 value. Writes at most `cap` bytes to `out` and
    // returns how many. A control that does not fit, and every later one, waits for the
    // next call; a pending Program Change that does not fit holds back everything.
    size_t Collect(const float* values, int program, uint8_t* out, size_t cap);

private:
    int16_t lastQ_[kMidiControlCount];
    int16_t lastCc_[kMidiControlCount];
    int     lastProgram_;
};

struct ControlWrite {
    uint8_t control;  // kMidiControls index
    float   value;
};

// Hands control writes from the main loop to the audio callback. One slot per control,
// so the latest value wins and nothing is ever dropped for lack of room.
class ControlMailbox {
public:
    static constexpr int kMaxDrainPerBlock = 8;

    void Post(int control, float value);       // main loop
    int  Drain(ControlWrite* out, int max);    // audio callback; round robin from the last take
    void Clear();                              // main loop, only while the callback is gated off

private:
    float         value_[kMidiControlCount]   = {};
    volatile bool pending_[kMidiControlCount] = {};
    int           cursor_ = 0;
};

#endif
