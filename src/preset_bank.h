#ifndef PRESET_BANK_H
#define PRESET_BANK_H

#include <stddef.h>
#include <stdint.h>

#include "CloudSeed/Parameter.h"

constexpr int kMaxPresets       = 16;
constexpr int kMaxPresetNameLen = 32;

// The pedal a bank is written for: the root key `board` in the TOML. Each firmware
// build is for one board (board.h) and rejects a bank for the other.
enum Board : uint8_t { Board_Terrarium = 0, Board_HotHouse = 1 };

const char* BoardName(Board board);  // "terrarium" / "hothouse", the `board` values

constexpr int kKnobCount = 6;   // knob1..knob6
constexpr int kControlBanks = 2;  // 0 = primary ("_a"), 1 = secondary ("_b")
// Toggle slots: Terrarium toggle1..toggle4 = SWITCH_1..SWITCH_4 (two positions),
// HotHouse toggle1..toggle3 = its three toggles, left to right (three positions).
// Arrays are sized for the larger board; a HotHouse never reads or writes slot 3.
constexpr int kMaxToggles = 4;

constexpr int ToggleCount(Board board) { return board == Board_HotHouse ? 3 : 4; }

// A lever position. A Terrarium switch is only ever Down (off) or Up (on).
constexpr uint8_t kToggleDown   = 0;
constexpr uint8_t kToggleMiddle = 1;
constexpr uint8_t kToggleUp     = 2;

// What a knob writes to. Param targets index PresetData::params (i.e.
// (int)Parameter); ReverseDelay is the reverse-delay window length, which is not
// a reverb parameter.
enum KnobTargetKind : uint8_t { KnobTarget_Param = 0, KnobTarget_ReverseDelay = 1 };

struct KnobTarget {
    KnobTargetKind kind;
    uint8_t paramIndex;  // valid only when kind == KnobTarget_Param
};

// What a toggle switch writes to. On/off targets (Param and the three binary
// pseudo-targets) are on only with the lever Up, so a HotHouse middle position is
// off. The three-way targets need a HotHouse toggle and use all three positions.
enum ToggleTargetKind : uint8_t {
    ToggleTarget_Param            = 0,
    ToggleTarget_DelayLinesMax    = 1,  // "delay_lines.max": off = default_delay_lines, on = max_delay_lines
    ToggleTarget_ReverseEnabled   = 2,  // "reverse.enabled": reverse voice on/off
    ToggleTarget_ReverseDirectMix = 3,  // "reverse.direct_mix": off = into reverb, on = direct mix
    // Three-way (HotHouse only): Down / Middle / Up
    ToggleTarget_DelayLinesSelect = 4,  // "delay_lines.select": 1 line / default_delay_lines / max_delay_lines
    ToggleTarget_EarlyEnvelope    = 5,  // "early.envelope": Scatter / neither / Bloom
    ToggleTarget_ReverseMode      = 6,  // "reverse.mode": off / into reverb / direct mix
};

constexpr bool IsThreeWayToggleTarget(ToggleTargetKind kind) {
    return kind >= ToggleTarget_DelayLinesSelect;
}

struct ToggleTarget {
    ToggleTargetKind kind;
    uint8_t paramIndex;  // valid only when kind == ToggleTarget_Param
};

// Which delay-line count is active. The "delay_lines.max" target chooses between
// Default and Max; "delay_lines.select" can also choose One.
enum DelayLinesMode : uint8_t {
    DelayLines_One     = 0,  // a single line
    DelayLines_Default = 1,  // default_delay_lines
    DelayLines_Max     = 2,  // max_delay_lines
};

struct PresetData {
    char     name[kMaxPresetNameLen];
    int      blinks;
    uint32_t onDurationMs;
    uint32_t offDurationMs;
    uint32_t pauseAfterMs;
    float    defaultDelayLines;  // lines in DelayLines_Default; whole number 1..maxDelayLines
    float    maxDelayLines;
    float    params[(int)Parameter::Count];
    // [preset.params.reverse] delay: normalized reverse-window length (knob target "reverse.delay")
    float    reverseDelay;
    // Stored state of the toggle pseudo-targets: [preset.params.delay_lines] max
    // (Terrarium, 0..1, Max when >= 0.5, else Default) or select (HotHouse, 0 / 0.5 /
    // 1 = One / Default / Max); [preset.params.reverse] enabled / direct_mix, 0..1,
    // on when >= 0.5
    DelayLinesMode delayLines;
    float    reverseEnabled;
    float    reverseDirectMix;
    // [bank][knob]; bank 0 = primary, bank 1 = secondary (preset footswitch (FS2) held)
    KnobTarget knobMap[kControlBanks][kKnobCount];
    // [bank][toggle]; bank 0 = primary, bank 1 = secondary (preset footswitch (FS2) held).
    // Slots past ToggleCount(board) are zero.
    ToggleTarget toggleMap[kControlBanks][kMaxToggles];
};

struct PresetBank {
    Board      board;
    int        count;
    PresetData presets[kMaxPresets];
};

// Parses `toml` (NUL-terminated, MUTATED in place) into `bank` with the rules of
// `board`; the document's `board` key must name it. The text must be plain ASCII:
// any byte >= 0x80 is rejected before parsing. Every parser allocation goes through
// alloc/dealloc and is released before this returns. Returns true on success; on
// failure sets bank.count = 0 and writes a NUL-terminated message into err.
bool ParsePresetBank(char* toml, Board board, PresetBank& bank, char* err, int errLen,
                     void* (*alloc)(size_t), void (*dealloc)(void*));

// Parses `length` bytes of preset TOML that need not be NUL-terminated (the embedded
// blob, memory-mapped QSPI, a USB upload buffer) without modifying them: rejects
// empty text and embedded NUL bytes, then runs ParsePresetBank() on a NUL-terminated
// scratch copy that is the first allocation through `alloc`. Same result contract
// as ParsePresetBank().
bool ParsePresetBankText(const char* text, uint32_t length, Board board, PresetBank& bank,
                         char* err, int errLen, void* (*alloc)(size_t),
                         void (*dealloc)(void*));

#endif
