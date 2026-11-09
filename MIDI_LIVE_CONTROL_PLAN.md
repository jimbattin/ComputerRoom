# MIDI live control: pedal ⇄ editor ⇄ MIDI controllers

## Context

The ask: define and document a MIDI implementation that covers every preset parameter, implement it in the firmware, and have the browser editor use it for live control in both directions. An edit in the editor is heard on the pedal at once; knob, toggle, footswitch and preset changes on the pedal show up in the editor within 50 ms; a generic MIDI controller can do everything the editor does. Update all relevant documentation.

User decisions (fixed):
- **Encoding**: every control has a 7-bit CC **and** a 14-bit NRPN. The editor uses only NRPN; editor values that it sends are snapped to the 14-bit grid.
- **Channel**: the pedal receives on any channel (omni) and sends on channel 1.
- **Preset loads, "whoever switches wins"**: picking a preset in the editor makes the pedal play the editor's version of it. A preset change on the pedal (footswitch, factory restore, a controller's Program Change) replaces the editor's values for that preset with the values the pedal plays.

Scope boundary: structural fields (knob/toggle maps, line counts, names, blink timing) are not live. They still need UPLOAD.

## Protocol (normative values used by every step)

**Control table.** The NRPN number is the CC number: NRPN MSB (CC99) is 0 and NRPN LSB (CC98) is the CC number. Order = firmware table order = editor table order.

| CC/NRPN | key | kind |
|---|---|---|
|12|input.InputMix|cont|
|13|input.PreDelay|cont|
|14|input.HiPassEnabled|on/off|
|15|input.HighPass|cont|
|16|input.LowPassEnabled|on/off|
|17|input.LowPass|cont|
|18|early.TapCount|cont|
|19|early.TapLength|cont|
|20|early.TapGain|cont|
|21|early.TapDecay|cont|
|22|early.Bloom|on/off|
|23|early.Scatter|on/off|
|24|early.ScatterRate|cont|
|25|early.ScatterDepth|cont|
|26|early_diffusion.DiffusionEnabled|on/off|
|27|early_diffusion.DiffusionStages|cont|
|28|early_diffusion.DiffusionDelay|cont|
|29|early_diffusion.DiffusionFeedback|cont|
|30|early_diffusion.EarlyDiffusionModAmount|cont|
|31|early_diffusion.EarlyDiffusionModRate|cont|
|39|late.LineDelay|cont|
|40|late.LineDecay|cont|
|41|late.LineModAmount|cont|
|42|late.LineModRate|cont|
|43|late.LateStageTap|on/off|
|44|late.Interpolation|on/off|
|45|late_diffusion.LateDiffusionEnabled|on/off|
|46|late_diffusion.LateDiffusionStages|cont|
|47|late_diffusion.LateDiffusionDelay|cont|
|48|late_diffusion.LateDiffusionFeedback|cont|
|49|late_diffusion.LateDiffusionModAmount|cont|
|50|late_diffusion.LateDiffusionModRate|cont|
|51|late_eq.LowShelfEnabled|on/off|
|52|late_eq.PostLowShelfGain|cont|
|53|late_eq.PostLowShelfFrequency|cont|
|54|late_eq.HighShelfEnabled|on/off|
|55|late_eq.PostHighShelfGain|cont|
|56|late_eq.PostHighShelfFrequency|cont|
|57|late_eq.CutoffEnabled|on/off|
|58|late_eq.PostCutoffFrequency|cont|
|59|seeds.TapSeed|cont|
|60|seeds.DiffusionSeed|cont|
|61|seeds.DelaySeed|cont|
|62|seeds.PostDiffusionSeed|cont|
|63|seeds.CrossSeed|cont|
|70|output.DryOut|cont|
|71|output.PredelayOut|cont|
|72|output.EarlyOut|cont|
|73|output.MainOut|cont|
|74|reverse.delay|cont|
|75|reverse.enabled|on/off|
|76|reverse.direct_mix|on/off|
|77|delay lines (`delay_lines.max` on Terrarium, `delay_lines.select` on HotHouse)|lines|
|102|Engage (bypass)|on/off|

54 controls (`kMidiControlCount = 54`). The unused numbers 0, 1-11, 32-38, 64-69, 78-101 and 103-127 stay free, which keeps clear of bank select, data entry, NRPN/RPN, the pedal CCs, volume/pan/expression and the channel-mode messages.

**Receive (any channel)**
- CC `c` on a table number: value `v = c/127`.
- NRPN: CC99 = MSB, CC98 = LSB select the parameter; CC6 = data MSB, CC38 = data LSB. CC6 applies `q = msb<<7` (LSB reset to 0, as MIDI 1.0 specifies). CC38 applies `q = msb<<7 | lsb`, using the last CC6 since the parameter was selected (0 if none). `v = q/16383`. A parameter is selected only while the NRPN MSB is 0, the LSB is not 127 and the LSB is a table number. CC101/CC100 (RPN select) deselects the NRPN until the next CC99/CC98. CC96/CC97 (increment/decrement) are ignored.
- Mapping by kind:
  - cont: `v` written verbatim.
  - on/off: on if `v >= 0.5` (CC 64-127, NRPN 8192-16383); written as exactly 1.0 / 0.0.
  - lines, Terrarium: `v >= 0.5` selects Max, otherwise Default.
  - lines, HotHouse: `v < 0.25` One, `v < 0.75` Default, otherwise Max. CC: 0-31 / 32-95 / 96-127; NRPN: 0-4095 / 4096-12286 / 12287-16383.
  - Engage: on = active, off = bypassed.
- Program Change `p` (0-based) loads preset `p` if `p < presetCount`, even when it is already the current preset (that reload drops unsaved tweaks). Otherwise it is ignored.
- Everything else is ignored, including bank select, notes and channel mode. All control messages are ignored while an upload session is open.

**Send (channel 1, i.e. status B0/C0)**
- The main loop compares the live state with what it last reported **every 10 ms**. It sends Program Change first when the current preset differs, then, per control in table order, a CC when the 7-bit value changed and an NRPN (CC99 0, CC98 n, CC6 q>>7, CC38 q&0x7F) when the 14-bit value changed.
- Quantisation: `q = lroundf(clamp01(v)*16383)` and `c = lroundf(clamp01(v)*127)`. On/off controls report q 16383/0 and c 127/0.
- Values reported for the lines control:
  - Terrarium: Default 0, Max 1.0.
  - HotHouse: One 0, Default 0.5 (q 8192, c 64), Max 1.0.
- Every preset load (FS2, PC, restore) and every DUMP request invalidates the shadow, so the full state is reported: PC + all 54 controls. Writes received over MIDI **are** echoed: the pedal does no echo suppression; the editor does that.

**New SysEx command `0x0A DUMP`** (protocol v1, additive like DFU/LOAD; `kProtocolVersion` stays 1).
- Request `F0 7D 43 53 0A F7`; reply `F0 7D 43 53 4A 00 F7`.
- A body makes it BadFrame. It is not a session message.
- The pedal queues the complete state report (PC + all controls) **before** the reply, so a host that has the reply has the whole dump.

**Exact 14→float conversion** (shared by firmware and editor so that editor text and pedal floats match bit for bit, seeds included):
- firmware `NormFromQ14(q) = (float)((double)q / d)` with `static volatile const double d = 16383.0` (volatile defeats `-ffast-math`'s reciprocal rewrite);
- editor `fromQ(q) = Math.fround(q / 16383)`.

## Approach

Steps 1-6 are firmware; steps 7-10 are the editor and depend on the protocol above only; step 11 is docs. `make test` and `make -C editor test check` pass after each step.

### 1. Host-portable MIDI control module — new `src/midi_control.h/.cpp`

Includes `preset_bank.h` (for `Board`, `DelayLinesMode`, `Parameter`). Contents:
- `enum class MidiTarget : uint8_t { Param, ReverseDelay, ReverseEnabled, ReverseDirectMix, DelayLines, Engage };`
- `struct MidiControlDef { uint8_t number; MidiTarget target; ::Parameter param; bool onOff; };` Non-Param entries use `Parameter::Unused`.
- `constexpr int kMidiControlCount = 54; extern const MidiControlDef kMidiControls[kMidiControlCount];` holds the table above. `onOff` is true for the 11 on/off Parameters, ReverseEnabled, ReverseDirectMix and Engage.
- `int MidiControlIndex(uint8_t number);` returns -1 when the number has no control.
- `float NormFromCc(uint8_t c)` (volatile 127.0 divisor) and `float NormFromQ14(uint16_t q)` (as in Protocol).
- `DelayLinesMode DelayLinesFromNorm(Board, float v)` and `float DelayLinesNorm(Board, DelayLinesMode)` implement the mappings above.
- `struct MidiEvent { enum class Type : uint8_t { None, Control, Program } type = Type::None; uint8_t index = 0; float value = 0.0f; };`
  - `index` is the `kMidiControls` index for Control and the program number for Program.
- `class MidiInDecoder { public: MidiEvent Feed(const uint8_t* msg, size_t len); private: struct Channel { uint8_t msb = 127, lsb = 127, dataMsb = 0; bool rpn = false; } ch_[16]; };`
  - Takes `[Bn cc v]` or `[Cn p]` and implements the receive rules above.
  - CC99 and CC98 each also clear `rpn` and reset `dataMsb` to 0.
- `class MidiReporter { public: static constexpr size_t kMaxBytes = 4 + kMidiControlCount * 20; MidiReporter() { Invalidate(); } void Invalidate(); size_t Collect(const float* values, int program, uint8_t* out, size_t cap); };`
  - Shadows: `int16_t lastQ_[]`, `int16_t lastCc_[]`, `int lastProgram_`, all -1 after `Invalidate()`.
  - Writes USB-MIDI packets on cable 0:
    - CC: `0x0B 0xB0 n v`;
    - PC: `0x0C 0xC0 p 0`;
    - NRPN: 4 CC packets.
  - Never writes past `cap`, and updates a shadow only for what it wrote.
  - If a PC is pending and there is no room for it, it writes nothing, so that controls are never reported ahead of their preset's PC.
  - At the first control that does not fit, it stops; that control and the rest go out on the next call.
  - Values are clamped with `!(v > 0) ? 0 : v > 1 ? 1 : v`, which also catches NaN.
- `struct ControlWrite { uint8_t control; float value; };`
- `class ControlMailbox { public: static constexpr int kMaxDrainPerBlock = 8; void Post(int control, float value); int Drain(ControlWrite* out, int max); void Clear(); private: float value_[kMidiControlCount] = {}; volatile bool pending_[kMidiControlCount] = {}; int cursor_ = 0; };`
  - `Post` (main loop): write the value, `std::atomic_signal_fence(seq_cst)`, then set pending.
  - `Drain` (audio callback): scan from `cursor_`, wrapping, take at most `max` pending entries, clear their flags, and leave `cursor_` after the last one taken. Latest value wins.
  - `Clear` (main loop, only while the preset-change gate is closed): clear every flag.
- **New test `tests/midi_control_test.cpp`**, built and run by `make test` (Makefile: add to `HOST_TESTS`, a rule like `cpu_load_test`'s compiling `tests/midi_control_test.cpp src/midi_control.cpp`, a `test:` line, and `src/midi_control.cpp` in `CPP_SOURCES`). Cases:
  - Table integrity:
    - numbers are unique and none is in {0, 6, 32, 38, 64-69, 96-101, 120-127};
    - every `Parameter` except `LineCount` appears exactly once as a Param target;
    - the onOff set is exactly the 14 listed.
  - Decoder:
    - CC 70 on channel 5 gives Control(DryOut index, 64/127);
    - CC 1 gives None;
    - NRPN 99 0 / 98 70 / 6 64 / 38 1 gives Control at 8192 then at 8193;
    - with the null NRPN (127/127), data is ignored;
    - after 101/100, data is ignored;
    - NRPN MSB 1 is ignored;
    - per-channel state stays separate;
    - `C3 05` gives Program 5;
    - CC 121 gives None.
  - `NormFromQ14(16383) == 1.0f`, `NormFromQ14(8192) >= 0.5f`, `NormFromQ14(8191) < 0.5f`.
  - `DelayLinesFromNorm` at the CC boundaries 31/32/95/96 (HotHouse) and 63/64 (Terrarium).
  - Reporter:
    - after `Invalidate`, the first `Collect` emits PC then for every control 1 CC packet + 4 NRPN packets (the exact bytes checked for DryOut at 0.5: `0B B0 46 40`, then `0B B0 63 00`, `0B B0 62 46`, `0B B0 06 40`, `0B B0 26 00`);
    - a second `Collect` with the same values emits 0 bytes;
    - a change smaller than 1/254 emits only the NRPN;
    - on/off 0.3 reports 0 / 0;
    - `cap = 30` emits PC + one control, and the next call continues with control 2;
    - with a PC pending and `cap = 3`, nothing is emitted.
  - Mailbox:
    - Post 0.2 then 0.4 to the same control drains once with 0.4;
    - 10 posts drain as 8 + 2;
    - `Clear` drops pending writes.

### 2. Knob re-park on an external write — `src/knob_bank.h`

- Add a public `void Repark(int knob) { repark[knob] = true; }`: callback only; parks one knob at its position in the next `Scan()`.
- Add a private `bool repark[kKnobCount] = {};`.
- In `Scan()`, after the existing Reset check and before the update loop, every flagged knob gets `snapshot[i] = positions[i]; live[i] = false; glideStep[i] = 0; applied[i] = 0.0f; repark[i] = false;`.
- `Reset()` clears `repark[i]` too.
- Extend `tests/knob_bank_test.cpp` with one case:
  - take knob 0 live, then `Repark(0)`;
  - a `Scan` at the same position writes nothing, while live knob 1 still writes when moved;
  - moving knob 0 by 0.02 produces a first write equal to glide step 1 from the new `currentValues[0]`: `cur + (pos - cur) / kKnobGlideBlocks`.

### 3. Receive queue + DUMP — `src/preset_protocol.h/.cpp`

**Replace `SysExAssembler` with `class MidiInQueue`** (clean cutover; same file):

```cpp
class MidiInQueue {
public:
    static constexpr size_t kMaxMessage = 256;   // SysEx bytes between F0 and F7
    static constexpr size_t kQueueBytes = 4096;  // records: u16 length + bytes
    void Feed(const uint8_t* data, size_t size); // USB ISR only
    size_t Pop(uint8_t* out);                    // main loop; out holds kMaxMessage; 0 = empty
};
```

`Feed` rules:
- Bytes F8-FF are ignored anywhere.
- `F0` starts a SysEx and drops any partial channel message.
- `F7` queues the SysEx body if it was not overflowed and is `latchable()` (the existing filter, unchanged).
- F1-F6 abort both a SysEx and a partial channel message.
- Status 80-EF aborts a SysEx and starts a channel message:
  - data length 1 for Cn/Dn, 2 otherwise;
  - running status is kept;
  - only Bn and Cn messages are queued, as `[status, d1(, d2)]`; the others are parsed for their length and dropped.
- A message that does not fit in the free ring space is dropped whole.

SPSC ring: `volatile size_t head_` (ISR) / `tail_` (main), with `std::atomic_signal_fence` around index publication. A popped message is a channel message iff `out[0] & 0x80`.

**DUMP:**
- Add `kCmdDump = 0x0A` and `UploadAction::Dump`.
- `case kCmdDump`: if the body is not empty, break (BadFrame); otherwise `reply(cmd, Ok)` and return `UploadAction::Dump`. The session timer is untouched.
- Update the header comment block (command list, "Single slot" remark).

**Tests (`tests/preset_protocol_test.cpp`):** rewrite `testAssembler`, `testAssemblerEdges`, `testAssemblerBurst` and `deliver()`/`uploadOverUsb()` against `MidiInQueue::Pop`. Changed expectation: a frame arriving while one is pending is now queued and popped second. New cases:
- `B0 46 40` between two of our SysEx frames pops in arrival order;
- `90 40 7F` (note on) and `E0 00 40` produce nothing and do not disturb a following CC;
- a CC inside a SysEx aborts the SysEx and is queued;
- filling the ring then pushing one more message drops it whole, and after a `Pop` new messages are accepted;
- `testDump`: reply bytes `F0 7D 43 53 4A 00 F7` and action Dump; with a body, BadFrame; `SessionActive()` unaffected.

### 4. Queued, non-blocking USB-MIDI link — `src/usb_midi_link.h/.cpp`

**RX:** the member `MidiInQueue in_`; `rxCallback` → `in_.Feed`. Replace `FrameReady/Frame/FrameLength/ReleaseFrame` with `size_t Pop(uint8_t* out) { return in_.Pop(out); }`.

**TX:** replace `SendSysEx` and `tx_` with:
- `uint8_t queue_[4096]; size_t queueLen_;`
- `uint8_t txBuf_[2][512]; int txCur_;`
- `static constexpr size_t kReplyReserve = (PresetProtocol::kMaxReply + 2) / 3 * 4;` (344)

API:
- `bool QueueSysEx(const uint8_t* msg, size_t len)`: packs with `PackSysExUsbMidi` straight into the queue tail; false if it does not fit or the device is not configured.
- `uint8_t* ReserveReport(size_t& cap)`: the tail and `cap = free - kReplyReserve`, or 0 while not configured, so replies always have room.
- `void CommitReport(size_t n)`.
- `size_t QueueFree() const`.
- `void Flush()`:
  1. returns if not configured or the queue is empty;
  2. copies `min(queueLen_, 512)` bytes (a multiple of 4, since entries are whole packets) into `txBuf_[txCur_]`;
  3. calls `usb_.TransmitInternal`;
  4. on OK, `memmove`s the queue down and toggles `txCur_`; on error, keeps everything and retries on the next call.

  Never touching the buffer of the transfer in flight is what makes it safe without busy polling.
- `void FlushFor(uint32_t ms)`: calls `Flush()` until the queue is empty or `ms` pass (`System::GetNow()`).
- `bool WaitForSpace(size_t bytes, uint32_t ms)`: calls `Flush()` until `QueueFree() >= bytes` or the timeout.

"Configured" means `hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceFS.pClassData != nullptr`. Use `#include "usbd_def.h"` and `extern "C" USBD_HandleTypeDef hUsbDeviceFS;` (defined in `libdaisy/src/hid/usb.cpp:16`). This is required: `CDC_Transmit_FS` dereferences `pClassData` unchecked (`libdaisy/src/usbd/usbd_cdc_if.c:301-303`), and reports are now sent without a host request.

Delete the retry constants. Rewrite the header comment to describe the queues.

### 5. Firmware integration — `src/cloudseed.cpp`

New statics:
- `static MidiInDecoder gMidiDecoder;`
- `static MidiReporter gMidiReporter;`
- `static ControlMailbox gMidiMailbox;`
- `static uint32_t gLastReportMs = 0;`
- `constexpr uint32_t kMidiReportIntervalMs = 10;`
- `constexpr int kMaxMidiMessagesPerPass = 64;`

Engine and callback changes:
- **Extract `static void setParameter(::Parameter p, float v)`**: `reverb->SetParameter`, plus `state.outputLevelsDirty = true` for DryOut/EarlyOut/MainOut. `applyKnobTarget()` calls it and keeps only the HighPass/LowPass auto-enable cases in its switch. `applyToggleTarget()`'s Param case calls it too.
- **`static void applyMidiWrites(int bank)`** (callback):
  - drain up to `kMaxDrainPerBlock`;
  - per write:
    - `Param` → `setParameter(c.param, c.onOff ? (v >= 0.5f ? 1.0f : 0.0f) : v)`;
    - `ReverseDelay` → `applyReverseWindow(v)`;
    - `ReverseEnabled` / `ReverseDirectMix` → the state bools `v >= 0.5f`;
    - `DelayLines` → `state.delayLines = DelayLinesFromNorm(kBoard, v)`;
    - `Engage` never reaches the mailbox.
  - Then `gKnobs.Repark(k)` for every knob whose `state.knobMap[bank][k]` targets that control: a Param with an equal `paramIndex`, or `KnobTarget_ReverseDelay` for ReverseDelay. There is no filter auto-enable and no `MarkEdited()`.
- In `processBlock()`, after the passthrough gate: compute `const int bank = gFootswitches.PresetHeld() ? 1 : 0;`, call `applyMidiWrites(bank)` before reading the knobs, and pass `bank` to `updateEngineControls`.

Main loop changes:
- **`loadPresetGated()`**: inside the gate, before `loadPreset()`, call `gMidiMailbox.Clear()`; after it, call `gMidiReporter.Invalidate()`.
- **`static void selectPreset(int index)`** = `loadPresetGated(index); StartPresetBlink(state.blinkPattern);`. Used by the FS2 block (`(currentPreset+1) % count`) and by Program Change.
- **`static float controlValue(const MidiControlDef& c, const float* params)`**:
  - Param → `params[(int)c.param]`;
  - ReverseDelay → `state.reverseDelayNorm`;
  - the two reverse bools → 1/0;
  - DelayLines → `DelayLinesNorm(kBoard, state.delayLines)`;
  - Engage → `state.bypass ? 0 : 1`.
- **`static void reportControls()`**:
  1. `std::atomic_signal_fence(seq_cst)` first, so callback-written state is re-read;
  2. fill `float values[kMidiControlCount]`;
  3. `size_t cap; uint8_t* out = gMidiLink.ReserveReport(cap); gMidiLink.CommitReport(gMidiReporter.Collect(values, state.currentPreset, out, cap));`.
- **`static void handleChannelMessage(const uint8_t* msg, size_t len)`**:
  - return if `gPresetProtocol.SessionActive()`;
  - decode;
  - Control + Engage → `const bool b = ev.value < 0.5f; state.bypass = b; SetBypassLed(b);`;
  - other Control → `gMidiMailbox.Post(ev.index, ev.value)`;
  - Program → `if (ev.index < gPresets.count) selectPreset(ev.index)`.
- **Rename `serviceUsbPresetLink()` → `serviceUsbMidi()`**, with the same call site (`:714`):
  - `gMidiLink.Service()`, `SetLoad`/`SetCurrentPreset` as now;
  - then pop up to 64 messages: channel messages go to `handleChannelMessage`, SysEx goes to the existing Handle/flash/reply/reboot body;
  - in that body, a `Dump` action does `gMidiLink.WaitForSpace(MidiReporter::kMaxBytes + UsbMidiLink::kReplyReserve, 50); gMidiReporter.Invalidate(); reportControls();` **before** the reply is queued;
  - replies use `gMidiLink.QueueSysEx(...)`;
  - the reboot path calls `gMidiLink.FlushFor(100)` before the existing `System::Delay(100)`;
  - then `Tick`, the passthrough-gate block unchanged, the 10 ms `reportControls()`, and finally `gMidiLink.Flush()`.

Persistence needs nothing new: PC and Engage change `state.currentPreset`/`state.bypass`, which `ServiceSettingsSave()` already persists.

### 6. Linux test host — `tools/usb_preset_host.py`

Add commands, and document them in the module docstring:
- `cc NUMBER VALUE`: raw `B0 n v`;
- `nrpn NUMBER VALUE14`: the 4 CCs;
- `pc PROGRAM`: `C0 p`, 0-based;
- `dump`: send DUMP; print every channel message decoded before the `4A` reply, then `DUMP: Ok`;
- `monitor SECONDS`: print the decoded channel messages.

Output lines are `PC p`, `CC n = v` and `NRPN n = q` (NRPN reassembled from 99/98/6/38). Decode the raw rawmidi byte stream: F0..F7 frames, `Bn x y`, `Cn p`.

`recv_frame()` keeps working unchanged: channel bytes never contain F0/F7, and they are discarded with the bytes before a frame.

### 7. Editor protocol layer (pure) — `editor/src/midi/protocol.ts`, new `editor/src/midi/control.ts`, `editor/src/model/bank.ts`

**`protocol.ts`:**
- `CMD.DUMP = 0x0a`.
- Rename `createSysexAssembler` → `createMidiAssembler(onMessage)`. It still emits SysEx F0..F7 as now and additionally emits complete channel voice messages (80-EF, length 1 for Cn/Dn, 2 otherwise, running status kept); stray data bytes are ignored.
- Update its call site in `webMidi.ts` and `protocol.test.ts` (new cases: a CC between two SysEx messages is emitted in order; `90 40 7F` is emitted as one 3-byte message).

**`control.ts`:**
- `export const MIDI_CONTROLS: readonly { number: number; key: string }[]`: the table, in the same order. The lines row uses key `"delay_lines"` and Engage uses `"engage"`.
- `export const paramKey = (key: string, board: Board) => key === "delay_lines" ? \`delay_lines.${delayLinesKey(board)}\` : key`.
- `numberOf(paramKey)`: both `delay_lines.*` keys map to 77.
- `controlByNumber(n)`.
- `Q_MAX = 16383`; `toQ(v) = Math.round(clamp01(v) * 16383)`; `fromQ(q) = Math.fround(q / 16383)`.
- `qFor(key, v)` by `PARAMS[key].kind` (engage is handled by callers):
  - switch: `isOn(v) ? 16383 : 0`;
  - select3: `toQ(Math.round(v * 2) / 2)`;
  - fader: `toQ(v)`.
- `sameAt14(key, v, q)`:
  - switch: `isOn(v) === (q >= 8192)`;
  - select3: `Math.round(v * 2) === Math.round(fromQ(q) * 2)`;
  - fader: `toQ(v) === q`.
- Encoders:
  - `nrpnMessages(n, q): Uint8Array[]`: four 3-byte messages on channel 1;
  - `programMessage(p)`: `[0xc0, p]`.
- `type LiveEvent = { type: "control"; number: number; q: number } | { type: "program"; program: number }`.
- `createLiveDecoder(on: (ev: LiveEvent) => void): (msg: Uint8Array) => void`:
  - per-channel NRPN state as in the firmware;
  - emits a control **only on CC38**: the pedal always sends the LSB, and this avoids applying the intermediate MSB-only value;
  - emits program on Cn;
  - ignores plain CCs and SysEx.

**`bank.ts`:** add `export function formatFloat32(f: number): string`. It returns the shortest `Number(f.toPrecision(p)).toString()` for p = 1..9 with `Math.fround(Number(s)) === f`, with `.0` appended when there is no `.`. Its fallback is `formatNorm(f)` (unreachable: 9 digits round-trip every float32).

New **`control.test.ts`**:
- every `PARAM_KEYS` entry resolves to a number for its board;
- the numbers are unique;
- the NRPN bytes for (70, 8192) are `B0 63 00 / B0 62 46 / B0 06 40 / B0 26 00`;
- the decoder emits only on CC38 and tracks channels separately;
- `qFor`/`sameAt14` at 8191/8192 and for select3 at q 4095/4096/12286/12287;
- for every q in 0..16383, `Math.fround(Number(formatFloat32(fromQ(q)))) === fromQ(q)` and `toQ(Number(formatFloat32(fromQ(q)))) === q`.

### 8. Editor link + live sync — `editor/src/midi/client.ts`, new `editor/src/midi/live.ts`

**`PresetLink`** gets:
- `async dump(): Promise<void> { await this.call(CMD.DUMP); }`;
- `sendControl(number, q)` and `sendProgram(p)`: direct `transport.send` of each message, not queued: fire-and-forget, and the pedal queues them;
- `onLive(cb: (ev: LiveEvent) => void): () => void`: subscribe with a `createLiveDecoder`.

Update the class comment and the `FakePedal` comment in `client.test.ts` (which mentions the single-slot assembler): the stop-and-wait queue now exists only so that each reply is matched to its request. Add a test that `dump()` sends `F0 7D 43 53 0A F7` and resolves on the `4A 00` reply.

**`live.ts`:**

```ts
export const LIVE_ECHO_MS = 300;
export interface LiveCallbacks {
  onPedalPreset(preset: number): void;            // the pedal changed preset (not during selectPreset)
  onPedalControl(preset: number, key: string, q: number): void; // key = paramKey(..., board)
  onBypass(bypassed: boolean): void;
}
export class LiveSync {
  constructor(link: PresetLink, board: Board, cb: LiveCallbacks, now: () => number = () => performance.now());
  get pedalPreset(): number | null;
  dispose(): void;
  sendParam(key: string, q: number): void;   // records lastSent[key] = now(); link.sendControl(numberOf(key), q)
  setBypass(bypassed: boolean): void;         // lastSent.engage; sendControl(102, bypassed ? 0 : 16383)
  selectPreset(preset: number, params: Record<string, number>): Promise<void>;
  pull(): Promise<void>;                      // await link.dump()
}
```

Event handling:
- program → set `pedalPreset`; call `onPedalPreset` unless a sync is active.
- control → `controlByNumber`; unknown numbers are ignored.
  - engage → `onBypass(q < 8192)`, unless engage was sent less than `LIVE_ECHO_MS` ago.
  - other → `key = paramKey(...)`:
    - while a sync is active, store it in `sync.reports.set(key, q)`;
    - otherwise drop it if `key` was sent less than `LIVE_ECHO_MS` ago or `pedalPreset === null`;
    - otherwise `onPedalControl(pedalPreset, key, q)`.

`selectPreset(preset, params)`:
1. `gen = ++this.gen`; set the sync to a fresh report map; set `pedalPreset = preset`; `link.sendProgram(preset)`.
2. `await link.dump()`. The DUMP reply orders after the PC load and the full dump, because the pedal processes received messages in order.
3. If `gen` is still current: for each `[key, v]` of `params` that has a number, call `sendParam(key, qFor(key, v))` when the report is missing or `!sameAt14(key, v, report)`.
4. In `finally`, clear the sync if it is still this one.

Effect: the pedal plays the editor's version; parameters equal at 14 bits keep the pedal's exact stored float.

New **`live.test.ts`** with its own fake transport. The fake keeps per-preset engine values and, on DUMP, emits PC + an NRPN for every control and then the `4A` reply; on an NRPN or PC it applies the value and echoes an NRPN report. Cases:
- `pull()` gives `onPedalPreset(p)` before `onPedalControl`s;
- the echo of a `sendParam` within 300 ms is not forwarded, while a report at +301 ms (injected `now`) is;
- `selectPreset(2, params)`, with two keys differing from the fake's preset 2 values:
  - the fake received PC 2, DUMP, then NRPN for exactly those two keys;
  - no `onPedalPreset`/`onPedalControl` call happened during the sync;
- an engage report gives `onBypass(true)` for q 0;
- a plain-CC-only report produces no callback.

### 9. Editor state — `editor/src/model/state.ts`

`EditorState` gets:
- `pedalHash: number | null`: the pedal bank this doc is, preset for preset; null when unknown or when the layout was changed;
- `pedalEpoch: number`: bumped whenever the doc newly becomes the pedal's bank.

Both start as null / 0 in `initialState`.

Actions:
- `load` gains `pedalHash?: number` → `pedalHash: a.pedalHash ?? null`, and `pedalEpoch` is bumped when `a.pedalHash !== undefined`.
- `{ type: "pedalBank"; hash: number }`: no-op if there is no doc or `s.pedalHash === a.hash`; else set it and bump the epoch.
- `duplicate` and `delete` set `pedalHash: null`.
- `{ type: "pedalPreset"; preset: number }`: if `s.pedalHash !== null` and the index is valid, `selected = preset`.
- `{ type: "liveParam"; preset: number; key: string; q: number }`:
  - return `s` if `s.pedalHash === null`, the preset or key is missing, or `sameAt14(key, current, q)`;
  - otherwise, by `PARAMS[key].kind`:
    - switch → value 1/0, literal `formatNorm`;
    - select3 → `Math.round(fromQ(q) * 2) / 2`, literal `formatNorm`;
    - fader → literal `formatFloat32(fromQ(q))`, value `Number(literal)`.
  - Patch preset `a.preset`, not `selected`: generalise the private `edit()` to `editAt(s, index, field, literal, update)`, and have `edit()` call it with `s.selected`.

The **`setParam`** action stays as it is (offline edits keep 6-decimal precision).

**`state.test.ts`** additions:
- `liveParam` is ignored with `pedalHash` null;
- an equivalent q leaves the doc text identical;
- a fader q patches the text so that `loadBank(docText)` returns `Math.fround(value) === fromQ(q)`;
- for a switch, q 8192 writes `1.0`;
- `duplicate` nulls `pedalHash`;
- `pedalBank` with the same hash does not bump the epoch.

### 10. Editor UI wiring — `editor/src/App.tsx`, `components/pages.tsx`, `components/DevicePanel.tsx`

**`App.tsx`:**
- Remove the LOAD-based preset follow: `pedalPresetRef`, the `pedalPreset` effect (`:92-101`), and the `pedalPresetRef` assignment in `readPedal`. The LOAD poll stays for the CPU meter.
- `readPedal`: `load(text, "pedal", null, board, fnv1a32(new TextEncoder().encode(text)))`, extending the `load` helper with that argument. Keep the INFO `currentPreset` select.
- `live = connected && !!link && !!info && state.pedalHash !== null && state.pedalHash === info.activeHash`. `liveRef.current = live && !busy` each render.
- `const [bypassed, setBypassed] = useState<boolean | null>(null)`.
- `liveSyncRef`: an effect on `[link, identity]` creates a `LiveSync(link, identity.board, …)` and disposes it on cleanup. Its callbacks:
  - `onPedalPreset` → `liveRef.current && dispatch({ type: "pedalPreset", preset })`;
  - `onPedalControl` → `liveRef.current && dispatch({ type: "liveParam", preset, key, q })`;
  - `onBypass` → `setBypassed`.
- On `linkPedal()` reset `bypassed` to null; on disconnect (`connected` false) the BYPASS key goes dark.
- inSync effect: `if (connected && inSync && info) dispatch({ type: "pedalBank", hash: info.activeHash })`, deps `[connected, inSync, info]`. This covers a successful UPLOAD and opening a file identical to the pedal's bank.
- Pull effect, deps `[connected, link, busy, state.pedalEpoch]`:
  - when `connected && link && !busy`, and `pulledRef.current` differs from `{ link, epoch }`, record it and `void liveSyncRef.current?.pull().catch(() => {})`;
  - firmware without DUMP answers BadFrame, so live updates then simply do not arrive.
- `onParam(key, value)`:
  - if `live && !busy && liveSyncRef.current?.pedalPreset === selected`: `q = qFor(key, value)`; return if `sameAt14(key, preset.params[key], q)`; else `dispatch({ type: "liveParam", preset: selected, key, q })` and `liveSync.sendParam(key, q)`;
  - otherwise `dispatch({ type: "setParam", key, value })`.
- `onSelect(i)`: `dispatch({ type: "select", preset: i })`; if `live && !busy && i !== selected`, call `liveSync.selectPreset(i, presets[i].params)`, and on rejection `setStatus(message)`.
- `onBypass`: `const next = !bypassed; setBypassed(next); liveSync.setBypass(next)`.

**`pages.tsx`:** `ParamPage` gets a prop `onParam: (key: string, value: number) => void`, used instead of `dispatch({ type: "setParam" … })` (`:97`). The `dispatch` prop is no longer needed there; remove it from ParamPage's props.

**`DevicePanel.tsx`:**
- new props `live: boolean`, `bypassed: boolean | null`, `onBypass: () => void`;
- the status LED shows `${p.inSync ? "SYNC" : "DIFF"}${p.live ? "  LIVE" : ""}`;
- add `<Key label="BYPASS" dark pressed={p.bypassed === true} disabled={offline || p.bypassed === null} onClick={p.onBypass} />` after REVERT.

### 11. Documentation

- **New `docs/MIDI.md`**, the MIDI implementation spec for controller and host authors, sections in this order:
  1. Overview: USB-MIDI port, both directions, which controls are covered.
  2. Channels (omni in, channel 1 out).
  3. Received messages: CC, NRPN (with the MSB/LSB rules), PC, ignored messages, ignored during an upload session.
  4. Value mapping per kind, including the per-board line ranges and Engage.
  5. The control table above, with columns for label and real-unit range. The ranges come from the presets.toml parameter reference.
  6. Sent messages: 10 ms change poll, order PC → CC → NRPN, full state after every preset load and DUMP, echo behaviour, quantisation.
  7. DUMP, linking to USB_MIDI.md.
  8. Interaction with the pedal:
     - knobs re-park after a MIDI write to their target;
     - toggles unaffected;
     - FS2 secondary bank;
     - PC and Engage are persisted like the footswitches;
     - parameter writes are unsaved tweaks: lost on a preset change unless saved with the FS1 hold or uploaded;
     - a preset load discards pending writes.
  9. Limits: 4 KB receive and send queues, at most 8 parameter writes applied per 1 ms block, about 10-20 ms latency.
  10. A MIDI Implementation Chart.
  11. Hex examples: `B0 46 40`; the NRPN for DryOut 8192; `C0 02`.
- **`docs/USB_MIDI.md`**:
  - DUMP section in §7;
  - §1 mentions live control and links to MIDI.md;
  - §4/§10: the pedal now sends unsolicited CC/NRPN/PC between replies, so hosts must skip non-SysEx messages;
  - received messages are queued (4 KB), not single-slot.
- **`README.md`**: a "MIDI control" section (what is controllable, link to docs/MIDI.md); in "Preset editor (browser)": live editing, LIVE indicator, BYPASS key, whoever-switches-wins semantics, 14-bit snapping of live edits.
- **`CLAUDE.md`**:
  - directory structure: the new files and tests;
  - USB-MIDI section: the queues, the link's configured guard, DUMP, `serviceUsbMidi()`;
  - a new "MIDI live control" subsection: table location, mailbox, reporter, knob re-park, echo/sync semantics;
  - the editor bullets:
    - `control.ts` mirrors `src/midi_control.cpp`'s table;
    - `live.ts`;
    - `pedalHash`/`pedalEpoch`;
    - LOAD no longer drives preset follow;
  - the Host unit tests list: `midi_control_test` plus the knob re-park and queue cases;
  - Common Modifications 1/4: a new parameter needs a table row in both `midi_control.cpp` and `control.ts`;
  - the memory table, refreshed from `make` / `make BOARD=hothouse` `--print-memory-usage` output;
  - the audio-flow step list: `applyMidiWrites` before the knob scan; main loop step 3.
- **`docs/HARDWARE_TESTS.md`**: add **H17 MIDI live control** (steps listed under Verification) and the new tool commands in Prerequisites.
- **`docs/PERFORMANCE.md:237`**: replace the `SysExAssembler` sentence with `MidiInQueue::Feed`.

## Critical files & anchors

- `src/cloudseed.cpp`: `processBlock()` `:463-517` (insert `applyMidiWrites` after the gate), `serviceUsbPresetLink()` `:550-598` (becomes `serviceUsbMidi`), `loadPresetGated()` `:211-221`, main loop FS2 block `:707-711`.
- `src/usb_midi_link.cpp`: `SendSysEx` `:27-36` (replaced). `libdaisy/src/usbd/usbd_cdc_if.c:297-311`: unchecked `pClassData` and `TxState` busy.
- `src/preset_protocol.cpp`: `SysExAssembler::Feed` `:54-74` and `latchable()` `:33-36` (reused), `Handle()` switch `:160-296`.
- `editor/src/App.tsx`: the LOAD follow effect `:92-101` (removed), `readPedal` `:209-220`, `ProgramKeys onSelect` `:346`.
- `editor/src/model/state.ts`: `edit()` `:152-157`, the reducer `:166-260`.

## Verification

1. `make test` (repo root, host g++): every suite reports `0 failed`, including the new `midi_control_test` and the Repark, MidiInQueue and DUMP cases.
2. `make` and `make BOARD=hothouse`: both link with no new warnings from `src/`.
3. `make -C editor test check` (needs Bun ≥ 1.4.2): `control.test.ts`, `live.test.ts`, and the state/protocol/client additions pass; `tsc` and Biome are clean. Then `make editor-html` builds.
4. Hardware (H17, user-run on a flashed pedal, `python3 tools/usb_preset_host.py`; close any editor tab first):
   - `monitor 10` while turning KNOB_1: `CC 70 = …` and `NRPN 70 = …` lines appear, about 100 per second at most.
   - `cc 70 0`: the dry signal goes silent. Then turn KNOB_1 slowly by a hair: the dry level glides from silence, with no jump.
   - `nrpn 40 16383`: LineDecay at maximum (a long tail).
   - `pc 2`: LED2 blinks 3 times, and `monitor` (in a second shell) shows `PC 2` followed by 54 CC + 54 NRPN lines.
   - `cc 102 0`: LED1 off (bypassed); `cc 102 127`: on. After a power cycle the pedal keeps the last state.
   - `dump`: prints PC + 108 control lines, then `DUMP: Ok`.
   - Editor (`make editor`, Chrome):
     - after page load the device LED shows `SYNC  LIVE`;
     - dragging DRY is heard at once;
     - turning KNOB_1 moves the DRY fader within about 50 ms;
     - FS2 tap: the editor follows the preset and its faders show the pedal's values;
     - edit a fader of P03, select P04, then P03 again: the pedal plays the edited P03;
     - FS1 + FS2 5 s restore on P03: the editor's P03 values return to the bank values;
     - BYPASS toggles LED1, and FS1 toggles the key.
   - `load` while dragging a fader and turning a knob: overruns stay 0.
   - With the USB cable disconnected (9 V power only) the pedal boots and runs normally for 1 minute: no hang, which proves the configured guard.

## Assumptions & contingencies

- If `CDC_Transmit_FS` rejects transfers larger than 64 bytes in MIDI mode (unexpected: today's READ replies are 344 bytes), lower the `txBuf_` chunk to 64 bytes. Nothing else changes.
- If the DTCM region overflows at link (expected use is about +10 KB of 128 KB), move the queues in `UsbMidiLink` and `MidiInQueue` to `DSY_SDRAM_BSS`. They are CPU-copied, never DMA.
