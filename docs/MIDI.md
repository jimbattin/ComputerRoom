# MIDI implementation

This document is for anyone controlling the pedal from a MIDI controller, a DAW, or a host
program. The normative definition is the control table in
[src/midi_control.cpp](../src/midi_control.cpp) (`kMidiControls`) and the receive/send logic
in [src/midi_control.h](../src/midi_control.h); if this document and the code ever disagree, the
code wins. The SysEx preset upload protocol on the same port is described in
[USB_MIDI.md](USB_MIDI.md).

## 1. Overview

The Daisy Seed's micro-USB port is a class-compliant USB-MIDI device at every boot (port name
"Daisy Seed Built In"). Besides the SysEx preset protocol it carries live control in both
directions:

- **In:** every preset parameter of the playing preset (all 49 engine parameters except the
  runtime-only `LineCount`), the reverse window and the two reverse switches, the delay-line
  selection, the bypass state (Engage), and the preset (Program Change).
- **Out:** the pedal reports the same state whenever it changes: a knob turned, a toggle
  flipped, a footswitch pressed, a preset loaded, or a value received over MIDI.

Every control has a 7-bit Control Change **and** a 14-bit NRPN, with the same number. The
browser editor (`editor/`) uses NRPN only.

Not live: the structural fields of a bank (knob and toggle maps, `default_delay_lines` /
`max_delay_lines`, names, blink timing). Changing those still needs a bank upload.

## 2. Channels

- **Receive:** omni. Messages on any of the 16 channels are accepted; NRPN selection state is
  kept per channel.
- **Send:** channel 1 (status `B0` / `C0`).

## 3. Received messages

**Control Change.** CC `n` on a number from the table (section 5), value `c` 0-127, sets the
control to `c / 127`.

**NRPN.** CC99 (NRPN MSB) and CC98 (NRPN LSB) select the parameter; CC6 (Data Entry MSB) and
CC38 (Data Entry LSB) set it.
- A parameter is selected only while the NRPN MSB is 0, the LSB is not 127 (the null NRPN),
  and the LSB is a number from the table. The NRPN number equals the CC number.
- CC99 and CC98 each also forget the last data MSB.
- CC6 applies `q = msb << 7` at once (the LSB resets to 0, as MIDI 1.0 specifies).
- CC38 applies `q = msb << 7 | lsb`, using the last CC6 received since the parameter was
  selected (0 if none).
- The value is `q / 16383`, 0-16383.
- CC101 / CC100 (RPN select) deselect the NRPN until the next CC99 / CC98, so RPN data entry
  never writes a parameter.
- CC96 / CC97 (data increment / decrement) are ignored.

**Program Change.** Program `p` (0-based: `C0 00` is the first preset) loads preset `p` if the
active bank has more than `p` presets, otherwise it is ignored. A Program Change for the preset
already playing reloads it, which drops unsaved tweaks (section 8).

**Ignored:** every other CC (bank select, modulation, the channel-mode messages 120-127, ...),
notes, aftertouch, pitch bend, System Common and real-time messages.

**Upload sessions.** While a preset upload session is open (USB_MIDI.md, BEGIN until the
session ends) every control and program message is ignored.

## 4. Value mapping

| Kind | Received value `v` (0..1) | Reported value |
|---|---|---|
| cont | written to the engine verbatim | the engine value |
| on/off | on if `v >= 0.5` (CC 64-127, NRPN 8192-16383); stored as exactly 1.0 / 0.0 | CC 127 / 0, NRPN 16383 / 0 |
| lines, Terrarium | `v >= 0.5` selects `max_delay_lines`, otherwise `default_delay_lines` | Default 0, Max 1.0 |
| lines, HotHouse | `v < 0.25` one line, `v < 0.75` `default_delay_lines`, otherwise `max_delay_lines` (CC 0-31 / 32-95 / 96-127; NRPN 0-4095 / 4096-12287 / 12288-16383) | One 0, Default 0.5 (CC 64, NRPN 8192), Max 1.0 |
| Engage | on = active, off = bypassed | active 127 / 16383, bypassed 0 / 0 |

The engine maps a "cont" value to its real range itself (column "Range" below; the same
normalized 0..1 values as in `presets.toml`).

## 5. Control table

CC number = NRPN number (NRPN MSB 0, NRPN LSB = the number). The table order is the order in
which the pedal reports the controls.

| CC / NRPN | Hex | Key | Label | Kind | Range |
|---|---|---|---|---|---|
| 12 | 0C | `input.InputMix` | INMIX | cont | stereo blend; no effect in this mono fork |
| 13 | 0D | `input.PreDelay` | PREDLY | cont | 0-1000 ms |
| 14 | 0E | `input.HiPassEnabled` | HPF | on/off | input high-pass on/off |
| 15 | 0F | `input.HighPass` | HPFREQ | cont | 20-1000 Hz |
| 16 | 10 | `input.LowPassEnabled` | LPF | on/off | input low-pass on/off |
| 17 | 11 | `input.LowPass` | LPFREQ | cont | 400-20000 Hz |
| 18 | 12 | `early.TapCount` | TAPS | cont | 1-50 taps |
| 19 | 13 | `early.TapLength` | TAPLEN | cont | 0-500 ms |
| 20 | 14 | `early.TapGain` | TAPGN | cont | 2-decade gain curve |
| 21 | 15 | `early.TapDecay` | TAPDCY | cont | 0-1 |
| 22 | 16 | `early.Bloom` | BLOOM | on/off | reverses the early tap gain order |
| 23 | 17 | `early.Scatter` | SCATTR | on/off | early tap gains oscillate |
| 24 | 18 | `early.ScatterRate` | SCRATE | cont | 0.1-5 Hz (2-decade curve) |
| 25 | 19 | `early.ScatterDepth` | SCDPTH | cont | 0-1 |
| 26 | 1A | `early_diffusion.DiffusionEnabled` | DIFF | on/off | early diffuser on/off |
| 27 | 1B | `early_diffusion.DiffusionStages` | STAGES | cont | 1-2 stages |
| 28 | 1C | `early_diffusion.DiffusionDelay` | DIFDLY | cont | 10-100 ms |
| 29 | 1D | `early_diffusion.DiffusionFeedback` | DIFFB | cont | 0-1 |
| 30 | 1E | `early_diffusion.EarlyDiffusionModAmount` | MODAMT | cont | 0-2.5 ms |
| 31 | 1F | `early_diffusion.EarlyDiffusionModRate` | MODRT | cont | 0-5 Hz |
| 39 | 27 | `late.LineDelay` | LNDLY | cont | 20-1000 ms |
| 40 | 28 | `late.LineDecay` | DECAY | cont | 0.05-60 s |
| 41 | 29 | `late.LineModAmount` | MODAMT | cont | 0-2.5 ms |
| 42 | 2A | `late.LineModRate` | MODRT | cont | 0-5 Hz |
| 43 | 2B | `late.LateStageTap` | STGTAP | on/off | lines output before their delay |
| 44 | 2C | `late.Interpolation` | INTERP | on/off | late-diffuser interpolation |
| 45 | 2D | `late_diffusion.LateDiffusionEnabled` | DIFF | on/off | late diffuser on/off |
| 46 | 2E | `late_diffusion.LateDiffusionStages` | STAGES | cont | 1-2 stages |
| 47 | 2F | `late_diffusion.LateDiffusionDelay` | DIFDLY | cont | 10-100 ms |
| 48 | 30 | `late_diffusion.LateDiffusionFeedback` | DIFFB | cont | 0-1 |
| 49 | 31 | `late_diffusion.LateDiffusionModAmount` | MODAMT | cont | 0-2.5 ms |
| 50 | 32 | `late_diffusion.LateDiffusionModRate` | MODRT | cont | 0-5 Hz |
| 51 | 33 | `late_eq.LowShelfEnabled` | LOSHLF | on/off | low shelf on/off |
| 52 | 34 | `late_eq.PostLowShelfGain` | LOGAIN | cont | 2-decade gain curve |
| 53 | 35 | `late_eq.PostLowShelfFrequency` | LOFREQ | cont | 20-1000 Hz |
| 54 | 36 | `late_eq.HighShelfEnabled` | HISHLF | on/off | high shelf on/off |
| 55 | 37 | `late_eq.PostHighShelfGain` | HIGAIN | cont | 2-decade gain curve |
| 56 | 38 | `late_eq.PostHighShelfFrequency` | HIFREQ | cont | 400-20000 Hz |
| 57 | 39 | `late_eq.CutoffEnabled` | CUTOFF | on/off | in-loop low-pass on/off |
| 58 | 3A | `late_eq.PostCutoffFrequency` | CUTFRQ | cont | 400-20000 Hz |
| 59 | 3B | `seeds.TapSeed` | TAPSD | cont | seed = value x 1000000 |
| 60 | 3C | `seeds.DiffusionSeed` | DIFSD | cont | seed = value x 1000000 |
| 61 | 3D | `seeds.DelaySeed` | DLYSD | cont | seed = value x 1000000 |
| 62 | 3E | `seeds.PostDiffusionSeed` | PSTSD | cont | seed = value x 1000000 |
| 63 | 3F | `seeds.CrossSeed` | XSEED | cont | 0-1 seed series blend |
| 70 | 46 | `output.DryOut` | DRY | cont | 2-decade level curve |
| 71 | 47 | `output.PredelayOut` | PREDLY | cont | 2-decade level curve |
| 72 | 48 | `output.EarlyOut` | EARLY | cont | 2-decade level curve |
| 73 | 49 | `output.MainOut` | MAIN | cont | 2-decade level curve |
| 74 | 4A | `reverse.delay` | REVWIN | cont | 20-2000 ms (3-octave curve) |
| 75 | 4B | `reverse.enabled` | REV | on/off | reverse voice on/off |
| 76 | 4C | `reverse.direct_mix` | REVMIX | on/off | off = into the reverb, on = direct mix |
| 77 | 4D | `delay_lines.max` (Terrarium) / `delay_lines.select` (HotHouse) | LINES | lines | section 4 |
| 102 | 66 | Engage (bypass) | BYPASS | on/off | on = active, off = bypassed |

54 controls. The numbers 0, 1-11, 32-38, 64-69, 78-101 and 103-127 are unused, which keeps
clear of bank select, modulation, data entry, NRPN/RPN select, the sustain-pedal range,
volume/pan/expression and the channel-mode messages.

A seed value is applied exactly only over NRPN: a CC gives 128 steps, so most seeds a preset
stores are not reachable with a CC.

## 6. Sent messages

- **Change poll.** Every 10 ms the pedal compares its live state with what it last reported.
  It sends a Program Change first if the current preset differs, then, per control in table
  order, a CC if the 7-bit value changed and an NRPN (`B0 63 00`, `B0 62 n`, `B0 06 q>>7`,
  `B0 26 q&7F`; the LSB is always sent) if the 14-bit value changed. A slow knob turn
  therefore often produces NRPNs without CCs.
- **Quantisation:** `q = round(v * 16383)`, `c = round(v * 127)`, with `v` clamped to 0..1.
  On/off controls report 16383 / 0 and 127 / 0.
- **Full state.** After every preset load (FS2, a Program Change, a factory restore) and on a
  DUMP request, the pedal forgets what it reported and sends the complete state: Program
  Change, then CC + NRPN for all 54 controls.
- **No echo suppression.** A value received over MIDI is reported back like any other change.
  A host that sends values should ignore reports of the controls it just set (the editor
  ignores them for 300 ms).
- **Full send queue.** A report stops at the first control that does not fit in the 4 KB send
  queue and continues on a later poll, so values are never reported ahead of their preset's
  Program Change. Room for a SysEx reply is always kept free.

## 7. DUMP

SysEx `F0 7D 43 53 0A F7` makes the pedal queue the complete state report (section 6) and then
reply `F0 7D 43 53 4A 00 F7`. A host that has received the reply has received the whole dump.
See [USB_MIDI.md](USB_MIDI.md) section 7.

## 8. Interaction with the pedal

- **Knobs.** A MIDI write to a parameter that a knob of the active bank targets re-parks that
  knob at its current position: nothing jumps, and the next turn glides the parameter from the
  MIDI value to the knob's position over 50 ms, as after a preset change.
- **Toggles** are not affected: a lever writes only when it is flipped.
- **FS2 held** (secondary bank): MIDI writes are applied as usual; the re-park uses the bank
  that is active at that moment.
- **Program Change and Engage** are persisted like the footswitches: the preset index and the
  bypass state are saved to flash 3 s after the last change.
- **Parameter writes are unsaved tweaks**, exactly like knob turns: a preset change drops them
  unless they were saved into the preset with FS1 held 5 s, or uploaded as part of a bank.
- A preset load discards parameter writes that have been received but not applied yet.

## 9. Limits

- Receive queue: 4 KB of messages. A message that does not fit is dropped whole.
- Send queue: 4 KB.
- At most 8 parameter writes are applied per 1 ms audio block; more wait for the next block,
  and of several writes to the same control only the latest is applied.
- The main loop handles up to 64 received messages per pass.
- Latency: about 10-20 ms in each direction (one main-loop pass plus the 10 ms report poll).

## 10. MIDI Implementation Chart

| Function | Transmitted | Recognized | Remarks |
|---|---|---|---|
| Basic channel: default | 1 | 1-16 | omni |
| Basic channel: changed | x | x | |
| Mode | x | x | |
| Note number | x | x | |
| Velocity | x | x | |
| Aftertouch (key / channel) | x | x | |
| Pitch bend | x | x | |
| Control change 12-31, 39-63, 70-77, 102 | o | o | section 5 |
| Control change 6, 38, 98, 99 (NRPN) | o | o | MSB 0, LSB = control number |
| Control change 100, 101 (RPN) | x | o | deselects the NRPN only |
| Control change 96, 97 | x | x | |
| Program change | o (0 to presets - 1) | o (0 to presets - 1) | 0-based |
| System exclusive | o | o | USB_MIDI.md protocol, Identity Reply |
| System common | x | x | |
| System real time | x | x | ignored |
| Aux: local on/off, all notes off, active sensing, reset | x | x | |

o = yes, x = no.

## 11. Examples

| Bytes | Meaning |
|---|---|
| `B0 46 40` | CC 70 (DryOut) = 64, i.e. 0.504 |
| `B0 63 00  B0 62 46  B0 06 40  B0 26 00` | NRPN 70 (DryOut) = 8192, i.e. 0.50003 |
| `C0 02` | load the third preset |
| `B0 66 00` | Engage off: bypass |
| `B0 4D 7F` | lines = Max |
