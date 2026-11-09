# DaisyCloudSeed Project Documentation

## Project Overview

This project implements a single guitar effect DSP for the Electrosmith Daisy Seed board, built for one of two
guitar pedal enclosures selected at compile time (`src/board.h`, Makefile `BOARD`):

- **Terrarium** (PedalPCB, the default: `make`) - four two-position toggle switches
- **HotHouse** (Cleveland Music Co., `make BOARD=hothouse`) - three three-position toggles

1. **CloudSeed** - Advanced algorithmic reverb with up to 5 delay lines, 10 presets per board bank, and extensive control

## Directory Structure

```
DaisyCloudSeed/
├── CloudSeed/             # Core reverb algorithm library (builds libcloudseed.a)
│   ├── AudioLib/          # Audio utilities (Biquad, filters, ShaRandom)
│   ├── Utils/             # SHA256 utilities
│   ├── build/             # Library build artifacts (generated, not checked in)
│   └── Makefile           # Library build configuration
│
├── libdaisy/              # Hardware abstraction layer (Git submodule)
├── DaisySP/               # DSP library (Git submodule)
├── Terrarium/             # PedalPCB hardware definitions (Git submodule; no longer used by the build)
│
├── build/                 # Build artifacts (generated, not checked in): build/terrarium/, build/hothouse/
│                          # (cloudseed.*, preset_check, host tests), build/build_id.c, build/editor.html
├── Makefile               # App build (BOOT_SRAM; BOARD=terrarium|hothouse; dependent libraries built with the `libs` target)
├── src/                   # Firmware sources
│   ├── cloudseed.cpp          # main(), pedal state, controls, audio callback, preset load
│   ├── board.h                # PedalBoard: per-board pins (knobs, toggles, footswitches, LEDs) on daisy::DaisySeed
│   ├── pedal_leds.h/.cpp      # LED1/LED2: preset blink, save/restore confirmation, FatalErrorLoop()
│   ├── pedal_storage.h/.cpp   # QSPI Settings + UserPresets (PedalStorage) + uploaded bank I/O, firmware+bank hash
│   ├── stored_bank.h          # Uploaded-bank QSPI layout + ValidStoredBankText() boot check (host-portable)
│   ├── sdram_pool.h/.cpp      # custom_pool_allocate() SDRAM bump pool + dedicated TOML parse arena (boot + USB upload)
│   ├── preset_protocol.h/.cpp # USB-MIDI SysEx preset upload protocol v1 (host-portable; docs/USB_MIDI.md)
│   ├── usb_midi_link.h/.cpp   # UsbMidiLink: USB-MIDI receive/reply over daisy::MidiUsbTransport
│   ├── knob_bank.h            # KnobBank absolute knob takeover (park, 50 ms glide, track; host-portable)
│   ├── toggle_bank.h          # ToggleBank parked toggle takeover (host-portable)
│   ├── footswitch_gestures.h  # FootswitchGestures: FS1/FS2 tap, hold, 5 s save and restore chords (host-portable)
│   ├── preset_bank.h          # PresetBank/PresetData types + knob/toggle-map types + ParsePresetBank()
│   ├── preset_bank.cpp        # TOML -> PresetBank parser (host-portable, no libdaisy)
│   └── presets_toml.s         # .incbin that embeds the board's bank (presets.toml / presets_hothouse.toml)
├── tests/                 # Firmware host unit tests (`make test`; the editor's are in editor/src)
│   ├── check.h                # Minimal CHECK() macro + summary
│   ├── knob_bank_test.cpp
│   ├── toggle_bank_test.cpp
│   ├── footswitch_gestures_test.cpp
│   ├── preset_bank_test.cpp
│   ├── engine_alloc_test.cpp  # CloudSeed engine: SetParameter()/Process() never allocate
│   ├── preset_protocol_test.cpp # SysEx assembler, USB-MIDI packer, protocol state machine, Identity Reply, DFU, end-to-end via the parser
│   ├── stored_bank_test.cpp   # ValidStoredBankText(): which stored bank the pedal boots
│   └── fixtures/two_presets.toml  # Parser fixture (Chorus + Through the Looking Glass)
│       fixtures/two_presets_hothouse.toml  # The same two presets as a HotHouse bank
├── docs/
│   ├── HARDWARE_TESTS.md      # On-pedal validation checklist per revision (USB-MIDI link, audio regressions)
│   ├── PERFORMANCE.md         # Record of applied performance/correctness fixes (with file/line anchors)
│   ├── USB_MIDI.md            # USB-MIDI preset upload protocol, for host (browser) authors
│   └── pedal.png              # Pedal/control artwork
├── presets.toml           # Terrarium built-in preset bank (10 presets); an uploaded bank in QSPI can override it
├── presets_hothouse.toml  # HotHouse built-in preset bank (10 presets)
├── third_party/tomlc99/   # Vendored TOML parser (MIT, commit in README.txt)
├── tools/                 # preset_check.cpp (host-side preset bank validator, --board terrarium|hothouse),
│                          # usb_preset_host.py (Linux USB-MIDI test host for docs/HARDWARE_TESTS.md;
│                          # also sends the Identity Request and the DFU command)
├── editor/                # Browser preset editor (Preact, Web MIDI; Bun >= 1.4.2, no Node/npm):
│                          # src/midi/ protocol v1 client + Web MIDI transport; src/model/ bank
│                          # text model + validator mirroring src/preset_bank.cpp, schema,
│                          # real-unit scaling, signal-flow model, editor state; src/components/
│                          # + src/styles/larc.css LARC UI; src/io/ file open/save;
│                          # src/editor.html entry (links larc.css, which @imports Pico);
│                          # server.ts Bun.serve dev server (HTML import, HMR); build.ts
│                          # single-file Bun.build to dist/editor.html; Makefile (serve/html/
│                          # test/check/fmt/clean, stamped `bun install --frozen-lockfile`);
│                          # biome.jsonc (lint + format + import order); strict tsconfig.json
│                          # (noUncheckedIndexedAccess); bunfig.toml; toml-text-plugin.ts
│                          # (dev server: .toml as text); *.test.ts(x) bun:test suites
│                          # (happy-dom preloaded by test-setup.ts)
├── CLAUDE.md              # This file - agent-facing project documentation
├── README.md              # User-facing control table and build/flash instructions
├── LICENSE                # MIT license (this fork, Erwin Coumans' Daisy port, CloudSeed)
└── .gitmodules            # Submodule definitions (DaisySP, libdaisy, Terrarium)
```

## Hardware Platform

### Daisy Seed Specifications
- **MCU**: STM32H750 (Cortex-M7, 480MHz)
- **RAM**: 512KB AXI SRAM (the BOOT_SRAM app region, 480KB usable) plus 128KB DTCM, 64KB ITCM,
  288KB D2 and 64KB D3
- **SDRAM**: 64MB external (48MB allocated to CloudSeed)
- **Flash**: 8MB QSPI
- **FPU**: Hard float, double precision
- **Audio**: 24-bit, up to 96kHz (typically 48kHz)

### Pedal Controls (`src/board.h`)

Both boards are driven by `PedalBoard` (`src/board.h`), built directly on `daisy::DaisySeed`.
It replaces libdaisy's `DaisyPetal` and the Terrarium submodule's `terrarium.h`, which the app no
longer includes (the `Terrarium/` submodule stays checked out but is unused): `DaisyPetal` has no
D5/D6 switch and also starts an encoder, an expression ADC channel and an I2C LED driver that
neither board has. `PedalBoard::Init(blockSize)` sets the audio block size before initialising
the knobs, so their one-poles run at the real callback rate. The board is selected by the
Makefile (`BOARD ?= terrarium`, `Makefile:10-19`), which defines exactly one of
`CLOUDSEED_BOARD_TERRARIUM` / `CLOUDSEED_BOARD_HOTHOUSE`; `board.h` turns it into
`constexpr Board kBoard` and `kBoardToggles = ToggleCount(kBoard)`.

Common to both boards:
- **Knobs** (6): knob1..knob6 = seed pins D16..D21, `hw.knob[0..5]` in presets.toml order
- **Footswitches** (2): FOOTSWITCH_1 = D25 (Bypass/Active, `hw.Footswitch(0)`), FOOTSWITCH_2 =
  D26 (preset cycling, `hw.Footswitch(1)`)
- **LEDs** (2): LED_1 = D22 (`kLed1Pin`) - Active indicator (on when not bypassed); LED_2 = D23
  (`kLed2Pin`) - Preset indicator (blinks N times for preset N, continuous with 5 s pause)
- **Audio**: mono. The left input is processed and the result is copied to **both** outputs
  (`out[1] = out[0]`) in every path, including bypass and the preset-change / upload
  passthrough

**Terrarium switches** (4 two-position, toggle1..toggle4 = SWITCH_1..SWITCH_4): seed pins D10,
D9, D8, D7. A grounded pin is Up (`kToggleUp`), otherwise Down.

**HotHouse toggles** (3 three-position on/off/on, toggle1..toggle3 left to right), two pins each:

| Toggle | Up pin | Down pin |
|---|---|---|
| toggle1 | D9 | D10 |
| toggle2 | D7 | D8 |
| toggle3 | D5 | D6 |

Up pin grounded = Up, down pin grounded = Down, neither = Middle
(`PedalBoard::TogglePosition()`). Positions are `kToggleDown` 0 / `kToggleMiddle` 1 /
`kToggleUp` 2 (`src/preset_bank.h`). The vendor's own HotHouse firmwares use FS1 + FS2 held as a
DFU gesture; this firmware keeps FS1 + FS2 held 5 s = factory restore, and DFU is reached via
SysEx `0x08` or the Seed's BOOT/RESET buttons.

## CloudSeed Effect

### Architecture

CloudSeed is based on the open-source CloudSeed VST plugin by ValdemarOrn, modified for mono processing on embedded hardware.

**Key Features**:
- Delay line count toggled per preset between `default_delay_lines` and `max_delay_lines`
  (both set in the bank; assignable via `[preset.toggle_map]`, default toggle1), plus a single
  line on HotHouse (`delay_lines.select`)
- 10 presets per board, defined in [presets.toml](presets.toml) (Terrarium) /
  [presets_hothouse.toml](presets_hothouse.toml) (HotHouse), embedded in the image and parsed at boot
- Scatter: per-tap oscillating early-tap gains (`early.Scatter`, `ScatterRate`, `ScatterDepth`)
- Early and late reverberation stages
- Extensive modulation and diffusion
- 48MB SDRAM buffer allocation
- Preset and bypass state persisted to QSPI flash
- Per-preset user save (FS1 held 5 s) and factory restore (FS1 + FS2 held 5 s), stored in QSPI
  and discarded when the firmware image or the active preset bank changes (a reflash, a USB
  upload, or a revert)
- Presets can be uploaded, read back, and reverted over USB-MIDI SysEx from a browser
  (the editor/ web app; protocol in docs/USB_MIDI.md); the pedal enumerates as a class-compliant USB-MIDI device at every boot
- Equal-power makeup gain on the wet path
- FPU flush-to-zero enabled at boot (denormal stall elimination)

### Control Mapping

File: [src/cloudseed.cpp](src/cloudseed.cpp) + `[preset.knob_map]` / `[preset.toggle_map]` in
[presets.toml](presets.toml) / [presets_hothouse.toml](presets_hothouse.toml)

Knobs and toggle switches are **not** hard-wired. Each preset's `[preset.knob_map]` /
`[preset.toggle_map]` names a primary (`_a`) and a secondary (`_b`) target per knob/toggle; the
secondary bank is selected while **the preset footswitch (FOOTSWITCH_2) is held**. The shipped
default map in every preset reproduces the historical assignment (knobs identical on both boards):

```cpp
// Knob read: src/cloudseed.cpp:478-480; scan + dispatch: src/cloudseed.cpp:363-370;
// applyKnobTarget(): src/cloudseed.cpp:222-255
//                        primary (_a)                      secondary (_b)
KNOB_1: output.DryOut                        | input.PreDelay
KNOB_2: output.EarlyOut                      | input.HighPass   (+ HiPassEnabled on first touch)
KNOB_3: output.MainOut                       | input.LowPass    (+ LowPassEnabled on first touch)
KNOB_4: late_diffusion.LateDiffusionFeedback | late.LineModAmount
KNOB_5: early.TapDecay                       | late.LineModRate
KNOB_6: late.LineDecay                       | reverse.delay    (20-2000 ms reverse window)

// Toggle read: src/cloudseed.cpp:481-484 (hw.TogglePosition()); scan + dispatch:
// src/cloudseed.cpp:374-381; applyToggleTarget(): src/cloudseed.cpp:261-288.
// Primary (_a) = secondary (_b) in every shipped preset.
// Terrarium (presets.toml), two positions; on/off targets are on when Up:
SWITCH_1: "delay_lines.max"    off = the preset's default_delay_lines, on = its max_delay_lines
SWITCH_2: "early.Bloom"        Bloom: reverses the early tap gain order
SWITCH_3: "reverse.enabled"    reverse voice on/off (window length set by whichever knob maps to
          "reverse.delay"; see CloudSeed/ReverseDelay.h)
SWITCH_4: "reverse.direct_mix" off = reverse feeds the reverb wet path, on = direct output mix
// HotHouse (presets_hothouse.toml), three positions:     Down / Middle / Up
TOGGLE_1: "delay_lines.select" 1 line / default_delay_lines / max_delay_lines
TOGGLE_2: "early.envelope"     Scatter on, Bloom off / both off / Bloom on, Scatter off
TOGGLE_3: "reverse.mode"       reverse off / reverse into the reverb / reverse direct mix
// An on/off target on a HotHouse toggle is on only when Up (Middle = off).

FOOTSWITCH_1 tap: Bypass toggle on RELEASE (persisted to flash 3 s later)
FOOTSWITCH_1 held 5 s: save the current engine state into the current preset (fires while held;
          the release does not toggle bypass)
FOOTSWITCH_2 tap: Preset cycle on RELEASE
FOOTSWITCH_2 held: secondary knob and toggle bank; the release cycles the preset only if no
          secondary knob or toggle wrote a value during the hold
FOOTSWITCH_1 + FOOTSWITCH_2 held 5 s: restore the current preset to its active-bank values
          (fires while held). Once both have been down together neither release toggles
          bypass or cycles the preset (FootswitchGestures, src/footswitch_gestures.h)
```

**Footswitch gestures** ([src/footswitch_gestures.h](src/footswitch_gestures.h)): libdaisy's `Switch` is
an 8-bit shift register that `Debounce()` shifts at most once per ms (`switch.cpp:34`), i.e. once
per 1 ms audio block: `Pressed()` is `state_ == 0xff` and clears
1 ms after a release, while `FallingEdge()` is `state_ == 0x80` and only fires 7 ms later
(`libdaisy/src/hid/switch.h:68-74`, `switch.cpp:39-41`). `FootswitchGestures` tracks both
switches with private `bypassHeld` / `presetHeld` flags, each set on `Pressed()` and cleared
only on that switch's `FallingEdge()`, so a contact bounce mid-hold (which never produces `0x80`)
cannot drop a hold, and FS2's 6 ms tail before the edge stays in bank 1 (`PresetHeld()`).
`Update()` is called once per callback with all four accessors and returns a
`FootswitchEvents` struct:
- `toggleBypass`: FS1's release edge, unless its 5 s save already fired or the press was part of
  a chord
- `cyclePreset`: FS2's release edge, unless a secondary knob or toggle wrote since the press
  (`MarkEdited()`) or the press was part of a chord
- `savePreset`: FS1 held alone for `kLongHoldBlocks` (5000 blocks = 5 s), counted from its
  `Pressed()` latch; fires once per hold, while still held
- `restorePreset`: both held for `kLongHoldBlocks` consecutive blocks; fires once, while held

A **chord** starts on the first block both holds are set and lasts until both are released, so
presses and releases may be arbitrarily staggered: while it lasts neither release fires, and a
chord shorter than 5 s does nothing at all. An FS1 hold that turns into a chord no longer counts
toward a save. A falling edge without a prior `Pressed()` is ignored. The callback calls
`MarkEdited()` when a bank-1 `Scan()` returns at least one knob or toggle write
(`src/cloudseed.cpp:387-388`), i.e. a secondary knob moved past `kKnobMoveThreshold` and wrote, or a
secondary toggle was flipped; brushing a knob below the threshold does not cancel the preset
change, and entering bank 1 re-parks both (zero writes), so pressing FS2 is never itself an edit.
The bank is derived from `PresetHeld()` after `Update()` in the same callback (`src/cloudseed.cpp:486`),
so the block that ends a hold already scans in bank 0. `processFootswitches()`
(`src/cloudseed.cpp:327-352`) reads each accessor once per callback,
because edge flags are only valid for the update in which they occur, and turns the events into
`state.bypass`, `state.triggerPresetChange`, `gSaveRequested`, and `state.triggerPresetRestore`.

**Knob take-over** ([src/knob_bank.h](src/knob_bank.h)): knobs are **absolute** - once a knob has taken
over, its position *is* the parameter value - but **parked** across transitions.
`KnobBank` is a class whose only public member is `Scan()`; its private `Reset()` snapshots every knob position and parks it; a parked knob writes nothing, so
power-up, a preset load, and entering or leaving the secondary bank never change a parameter on
their own, and a freshly loaded preset sounds exactly as authored until a knob is turned. A knob
goes live once it moves `kKnobMoveThreshold` (0.01 of travel) from its snapshot. It then
**glides** its target linearly from the target's current value to the pot's (possibly still
moving) position over `kKnobGlideBlocks` (50) audio blocks = 50 ms, landing exactly on the pot.
The glide exists because engine parameters are not smoothed downstream
(`ReverbChannel::SetParameter` assigns the output levels directly,
`CloudSeed/ReverbChannel.h:317-328`), so a step would click. After the glide the knob writes its
own position whenever that changes by `kKnobApplyEpsilon` (0.001), which keeps ADC noise off the
engine.

Positions within `kKnobRailWindow` (0.003) of a stop are written as exactly 0.0 / 1.0: the top
ADC reading is at most 65535/65536, libdaisy documents ~0.002 of bleed at the bottom of the pots
(`libdaisy/src/hid/ctrl.cpp:3-4`), and the `Response2Dec` output levels are only silent below
0.00025. Rail values bypass the epsilon, so the last fraction of travel into a stop always lands
- this is what makes KNOB_1/2/3 at their CCW stops silence the pedal. The parked test uses the
raw position, so a knob *resting* against a stop at a transition stays parked and cannot slam its
target to the rail. A `Reset()` during a glide cancels it and leaves the target where the glide
had got to.

The top rail is reachable only since libdaisy v9.0.0. The ADC is initialised with the
default `OVS_32`, and libdaisy up to v8.1.0 programmed it with `Ratio = 31` while still shifting
right by 5 bits (the HAL writes `Ratio - 1` to `OVSR`), so every knob read 31/32 of its true
position and full CW stopped at about 0.969. v9.0.0 sets `Ratio = 32`
(`libdaisy/src/per/adc.cpp:279-282`): a given knob position now reads about 3 % higher, and full
CW lands on exactly 1.0.

Leaving the secondary bank re-parks too: a knob dialled in bank 1 is parked in bank 0, and its
next turn glides its primary target to the knob's position - a smooth jump, by design. There is
deliberately no pickup/crossing rule (it reintroduces the dead-zone feel).

`KnobBank::Scan()` is the whole per-callback decision, called from `updateEngineControls()`
(`src/cloudseed.cpp:363-370`): it re-parks on a bank change (`bank != activeBank`), when
`state.controlResetPending` is set (by `loadPresetGated()` before the load,
`src/cloudseed.cpp:209-219`), or on the first call; a re-parking call returns zero writes. It never
runs while `state.presetChangeInProgress` is set (the callback passes audio through and returns
first, `src/cloudseed.cpp:472-476`) because the re-park must not straddle a `state.knobMap` rewrite;
`controlResetPending` simply stays set until the load completes.
The glide start values come from `knobTargetValue()` (`src/cloudseed.cpp:293-314`), read for the
active bank every callback: knob values are stored verbatim in `parameters[]` by `SetParameter`,
so `GetAllParameters()` read-back is exact; the `reverse.delay` pseudo-target has no
`parameters[]` slot and is tracked in `state.reverseDelayNorm` instead (set by `loadPreset()`
from the preset's `[preset.params.reverse] delay`, then by the knob). A disabled input filter
reports its open end instead of its stored cutoff (`HighPass` 0.0 while `HiPassEnabled` < 0.5,
`LowPass` 1.0 while `LowPassEnabled` < 0.5): the engine skips it, so that is what is heard, and
`applyKnobTarget()` enables it on the first write, so the glide opens from there rather than
stepping to the stored cutoff (about 2 kHz in Chorus and Dark Plate).

`main()` settles the knob one-poles for 200 ms between `hw.StartAdc()` and `hw.StartAudio()`
(`src/cloudseed.cpp:652-668`). `AnalogControl` starts at 0.0 and only converges while
`ProcessAnalogControls()` runs, which otherwise first happens inside the audio callback: without
the settle loop the first snapshot would capture ~5 % of each real knob position and the
settling ramp itself would be read as a deliberate turn at every power-up.

**ADC smoothing**: libdaisy's default `AnalogControl` slew computes to `coeff_ = 1.0` at this
callback rate (`libdaisy/src/hid/ctrl.cpp:16` with a 1 kHz update and the 0.002 s default),
i.e. no filtering. `main()` re-tunes every knob to `KNOB_SMOOTHING_COEFF` (0.05, ~20 ms) at
`src/cloudseed.cpp:636-637`. Nothing may re-`Init()` the knobs afterwards (`PedalBoard::Init()`
does it once, after setting the block size), or the coefficient is overwritten.

**Toggle take-over** ([src/toggle_bank.h](src/toggle_bank.h)): the toggles (4 on Terrarium, 3 on
HotHouse; arrays sized `kMaxToggles` = 4, a HotHouse passes slot 3 as a constant Down so it never
writes) are also assignable per preset (`[preset.toggle_map]`) and take over the same way knobs
park, but with no glide - every toggle target has two or three discrete states.
`ToggleBank::Scan()` snapshots and parks every lever on the first
call, on a bank change, or when `forceReset` is set, exactly like `KnobBank::Scan()`; a
re-parking call returns zero writes. Once primed, a lever that differs from its parked snapshot
writes its position (`kToggleDown` 0 / `kToggleMiddle` 1 / `kToggleUp` 2) immediately (no move
threshold, no epsilon: the target is discrete so there is nothing to debounce beyond the
snapshot compare) and keeps writing on every later change. `updateEngineControls()` calls
`gToggles.Scan(bank, reset, togglePositions, toggleWrites)` right after the knob scan, reusing
the same `reset` flag (`state.controlResetPending`) and `bank` (`src/cloudseed.cpp:374-381`),
then dispatches each write through `applyToggleTarget()` (`src/cloudseed.cpp:261-288`). On/off
targets are on only with the lever Up (a HotHouse Middle is off): `delay_lines.max` sets
`state.delayLines` to `DelayLines_Max` / `DelayLines_Default`, `reverse.enabled` /
`reverse.direct_mix` set a `PedalState` bool read elsewhere in the callback, and
`ToggleTarget_Param` calls `reverb->SetParameter()` with 1.0 / 0.0. The three-way targets (HotHouse
only) use all three positions: `delay_lines.select` sets `state.delayLines` to the position
(`DelayLines_One` / `_Default` / `_Max`), `early.envelope` writes `Scatter` (on when Down) and
`Bloom` (on when Up), and `reverse.mode` sets `reverseEnabled` (not Down) and `reverseDirectMix`
(Up). No toggle target sets `outputLevelsDirty`: none feeds the makeup gain.

A toggle that targets `input.HiPassEnabled` or `input.LowPassEnabled` **owns** that filter's
enable: `loadPreset()` computes `state.hiPassOwnedByToggle` / `state.lowPassOwnedByToggle` from
`state.toggleMap` (`src/cloudseed.cpp:162-169`, `:198-199`), and while true `applyKnobTarget()` no
longer auto-enables the filter on the knob's first touch and `knobTargetValue()` glides the knob
from the stored cutoff instead of the open end (`src/cloudseed.cpp:236-245`, `:303-307`) - the toggle
is the only thing that switches the filter on or off.

### Presets

All preset data lives in the board's bank at the repo root - [presets.toml](presets.toml)
(Terrarium) or [presets_hothouse.toml](presets_hothouse.toml) (HotHouse) - and is the **built-in**
preset bank: the Makefile passes it as `PRESETS_TOML_FILE` to `src/presets_toml.s` (`.incbin`,
lands in `.rodata` → SRAM), and it is never modified on the pedal. A bank uploaded over USB-MIDI
(docs/USB_MIDI.md) is stored in QSPI instead and, if it is present and parses, overrides
the built-in bank at boot until it is reverted or a different firmware image is flashed - see
"USB-MIDI preset upload" below. Whichever bank is active is parsed once at boot into the static
`gPresets` bank (`src/cloudseed.cpp:57`); there is still no filesystem, and the active bank's
text is never modified in place. Changing the built-in bank means editing the TOML and
reflashing; a sound saved on the pedal (FS1 held 5 s) is stored separately in QSPI and overrides
its preset's engine values, within whichever bank is active, until it is restored (see "User
preset save / factory restore" below).

1. Chorus (1 blink)
2. Dull Echos (2 blinks)
3. Hyperplane (3 blinks)
4. Medium Space (4 blinks)
5. Noise in the Hallway (5 blinks)
6. Rubi Ka Fields (6 blinks)
7. Small Room (7 blinks)
8. 90s Are Back (8 blinks)
9. Through the Looking Glass (9 Blinks)
10. Dark Plate (10 Blinks)

Both banks hold the same ten presets (the HotHouse copy differs only in `board`, the toggle map
and `[preset.params.delay_lines] select`). All presets allow 5 delay lines except "Through the
Looking Glass" (`max_delay_lines = 4.0`) - it is CPU-intensive and crackles above that.

**Preset System Architecture**:
- Every bank starts with a root key `board = "terrarium"` or `board = "hothouse"`, checked first
  (`checkBoard()`, `src/preset_bank.cpp:823-852`) against the board the parser is run for
  (`kBoard` on the pedal, `--board` in `preset_check`): `missing board = "<thisboard>"`,
  `board '<x>' is not "terrarium" or "hothouse"`, or `bank is for board "<file>", this pedal is
  "<this>"`. The board is stored in `PresetBank::board`
- Each `[[preset]]` carries `name`, `blinks`, `led_on_ms`, `led_off_ms`, `led_pause_ms`,
  `default_delay_lines`, `max_delay_lines`, a `[preset.knob_map]` table, a `[preset.toggle_map]`
  table, eight `[preset.params.*]` groups holding all 49 file-controlled parameters (see the
  parameter reference in the TOML header - `Bloom`, `Scatter`, `ScatterRate` and
  `ScatterDepth` are in `[preset.params.early]`), a ninth group `[preset.params.reverse]`
  (`delay`, `enabled`, `direct_mix`), and a tenth `[preset.params.delay_lines]`: key `max` on
  Terrarium (0..1; Max when >= 0.5, else Default), key `select` on HotHouse (exactly 0, 0.5 or 1
  = One / Default / Max, else `preset N: 'delay_lines.select' must be 0, 0.5 or 1`); the other
  board's key is rejected as unknown. Both land in `PresetData::delayLines` (a
  `DelayLinesMode`). None of the last two groups has a `Parameter` slot, so `parseParams()`
  parses them separately from `kGroups` ([src/preset_bank.cpp](src/preset_bank.cpp))
- `[preset.knob_map]` requires all twelve `knobN_a` / `knobN_b` keys. Each value is a quoted
  `"group.Parameter"` naming a parameter of that same group, or the pseudo-target
  `"reverse.delay"`. Parsed by `parseKnobMap()`/`parseKnobTarget()`
  ([src/preset_bank.cpp](src/preset_bank.cpp)) into `PresetData::knobMap[bank][knob]`
- `[preset.toggle_map]` requires every `toggleN_a` / `toggleN_b` key for the board's
  `ToggleCount(board)` toggles: eight on Terrarium (N = 1..4 = SWITCH_1..SWITCH_4), six on
  HotHouse (N = 1..3; `toggle4_*` is an unknown key). Each value is a quoted
  `"group.Parameter"` naming an on/off parameter (`kToggleParams`: `Bloom`, `Scatter`, the
  seven filter/diffusion/shelf/cutoff enables, the two diffusion stage counts, `LateStageTap`,
  `Interpolation`), one of the binary pseudo-targets `"delay_lines.max"`, `"reverse.enabled"`,
  `"reverse.direct_mix"`, or - HotHouse only - one of the three-way pseudo-targets
  `"delay_lines.select"`, `"early.envelope"`, `"reverse.mode"` (Terrarium: `'<text>' needs a
  three-position toggle`). Other pseudo names fail with `'delay_lines' toggles only 'max' or
  'select'` / `'reverse' toggles only 'enabled', 'direct_mix' or 'mode'`. Parsed by
  `parseToggleMap()`/`parseToggleTarget()` ([src/preset_bank.cpp](src/preset_bank.cpp)) into
  `PresetData::toggleMap[bank][toggle]` (slots past `ToggleCount(board)` are zero);
  `splitTarget()` / `resolveParamTarget()` / `groupIs()` are shared with the knob-map parser
- `LineCount` alone must NOT appear in the file: it is driven live by the delay-line toggle
  target (`"delay_lines.max"` / `"delay_lines.select"`), which selects one line,
  `default_delay_lines` or `max_delay_lines`. The parser rejects it
- `default_delay_lines` and `max_delay_lines` are each a whole number 1..`TotalLineCount` (5,
  `CloudSeed/DelayLineCount.h`; checked by `readLineCount()`,
  [src/preset_bank.cpp](src/preset_bank.cpp)), and `default_delay_lines` must not exceed
  `max_delay_lines` - the toggle's off state must never exceed a preset's CPU cap
- Parsed by `ParsePresetBank()` ([src/preset_bank.cpp](src/preset_bank.cpp)) into `PresetBank`
  (`board` + `count` + `PresetData[16]`); `PresetData::params[]` is indexed by `(int)Parameter`.
  `ParsePresetBank()`, `ParsePresetBankText()` and `ParsePresetText()` all take the `Board`
- Applied with `CloudSeed::ReverbController::LoadPreset()`
  (`CloudSeed/ReverbController.h:47`), which copies every slot except `LineCount`
  and then re-applies all 50 through `SetParameter`
- Cycles using modulo operator: `(currentPreset + 1) % gPresets.count` (`src/cloudseed.cpp:695`)
- Preset index is persisted to QSPI flash, so **the order in presets.toml is frozen**;
  reordering or deleting entries requires bumping `SETTINGS_VERSION` (`src/pedal_storage.cpp:5`)
- `loadPreset()` (`src/cloudseed.cpp:173-205`) is the only reader of `gPresets` after boot. It loads
  the preset's saved user edit if its `UserPreset` slot is valid, else the factory values:
  `LoadPreset()` with the chosen `params`, `applyReverseWindow()` with the chosen reverse window,
  and `state.delayLines` (from `UserPreset::delayLines` 0.0/0.5/1.0, or `PresetData::delayLines`)
  / `state.reverseEnabled` / `state.reverseDirectMix` from the chosen
  toggle pseudo-values. It then copies the preset's `knobMap`, `toggleMap`, `defaultDelayLines`,
  `maxDelayLines` and blink timings (never user-edited) into `PedalState`, derives
  `hiPassOwnedByToggle` / `lowPassOwnedByToggle` via `toggleMapTargets()`
  (`src/cloudseed.cpp:162-169`), and sets `state.outputLevelsDirty`. The audio callback and
  `ServicePresetBlink()` (handed `state.blinkPattern` by the main loop) read only those cached copies
- The delay-line count is applied in the audio callback (`updateEngineControls()`) from
  `state.delayLines`: `DelayLines_One` = 1, `_Default` = `state.defaultDelayLines`, `_Max` =
  `state.maxDelayLines` (`src/cloudseed.cpp:390-399`)
- LED2 blinks continuously to indicate active preset (N blinks = preset N)
- A parse failure is unrecoverable: `FatalErrorLoop()` (`src/pedal_leds.cpp:45-54`, called at
  `src/cloudseed.cpp:614`) blinks both LEDs at 5 Hz forever and never starts audio. The same loop reports an
  exhausted SDRAM pool and an unexpected audio block size. `make` validates the board's bank
  before embedding it, so a rejected file cannot be built into firmware in the first place

**Boot-time memory**: preset text - the embedded bank, or an uploaded bank read from
QSPI - is parsed from a dedicated 512 KB `DSY_SDRAM_BSS` arena that is not part of `custom_pool`
(`toml_arena`, `src/sdram_pool.cpp:49-61`; parsing itself is `ParsePresetText()`,
`src/sdram_pool.cpp:63-70`, a wrapper that resets the arena around the host-portable
`ParsePresetBankText()`, `src/preset_bank.cpp:961`). The same arena and parser run again at runtime to validate a USB
upload before anything is written to flash (see "USB-MIDI preset upload" below). Every parse
starts the arena at offset 0 and resets it on return, so boot and an upload validation never
overlap, and the reverb's `custom_pool` is unaffected either way. Peak measured usage at boot is
147,096 B for presets.toml and 143,992 B for presets_hothouse.toml on x86-64 (smaller on
32-bit ARM). Permanent SDRAM cost of the TOML parse arena: 512 KB, whether or not a USB upload
ever happens.

### Preset Persistence

The current preset and the bypass state are both automatically saved to and loaded from QSPI flash memory, so they persist across power cycles. Writes are coalesced: flash is written `SETTINGS_SAVE_DELAY_MS` (3000 ms) after the last change.

**Implementation** ([src/pedal_storage.h](src/pedal_storage.h),
[src/pedal_storage.cpp](src/pedal_storage.cpp)): `PedalStorage` owns both
`PersistentStorage` instances. It is constructed in `src/cloudseed.cpp` as
`gStorage(hw.seed.qspi)` (`:109`), right after `hw`, because `PersistentStorage` needs the
board's `QSPIHandle&` at construction.

**Settings Structure** (`src/pedal_storage.h:15-29`):
```cpp
constexpr int SETTINGS_VERSION = 2;                 // src/pedal_storage.cpp:5
constexpr uint32_t SETTINGS_SAVE_DELAY_MS = 3000;  // src/pedal_storage.cpp:9

struct Settings {
    int  version;        // SETTINGS_VERSION for compatibility checking
    int  currentPreset;  // 0 .. gPresets.count-1
    bool bypass;         // Persisted bypass state (true = bypassed at last save)
    bool operator!=(const Settings& a) const;  // Required by PersistentStorage
};
```

**Storage Management**:
- Uses Daisy's `PersistentStorage<Settings>` class with QSPI flash
- Version control ensures compatibility when Settings struct changes
- Automatic defaults restoration if version mismatch detected
- Settings validated on load (invalid presets default to 0)

**Save/Load Workflow**:
1. **On startup**: `main()` (`src/cloudseed.cpp:641-644`) calls `gStorage.Init(active.hash)`
   (`src/pedal_storage.cpp:41-56`), then takes `RestoredPreset(gPresets.count)` /
   `RestoredBypass()` into `state` and calls `loadPreset()`
2. **On preset change / bypass toggle**: nothing is written yet. `loadPresetGated()` updates
   `state.currentPreset` in the main loop; the audio callback flips `state.bypass`
   (`src/cloudseed.cpp:335-338`)
3. **In main loop**: `gStorage.ServiceSettingsSave(state.currentPreset, state.bypass)`
   (`src/pedal_storage.cpp:107-120`, called on every pass at `src/cloudseed.cpp:702`) mirrors
   both values into the RAM copy and restarts a `SETTINGS_SAVE_DELAY_MS` timer whenever they
   differ. Once 3 s pass without another change it saves the `Settings` store, outside the
   audio callback. A burst of preset/bypass changes therefore costs one QSPI sector erase, and
   `Save()` skips the erase entirely when flash already matches; a power-off within 3 s of a
   change loses that change (the pedal boots in the last saved state)

**Resilience Features**:
- Invalid preset indices automatically default to preset 0 (Chorus)
  (`PedalStorage::RestoredPreset()`, `src/pedal_storage.cpp:58-61`);
  the corrected index is written back to flash 3 s after boot
- Version mismatch triggers `RestoreDefaults()` (`src/pedal_storage.cpp:54-55`); the defaults are then
  read straight-line, with no reload
- First boot / version mismatch defaults to preset 0 and `bypass = true` (`src/pedal_storage.cpp:42`)
- LED1 is re-synced to the restored bypass state after load (`src/cloudseed.cpp:648-649`)
- Non-blocking: flash writes happen in the main loop, not the audio callback
- Settings survive power cycles, firmware updates, and manual resets

**To add more persistent settings**: add the field to the `Settings` struct (`src/pedal_storage.h`), extend its `operator!=`, increment `SETTINGS_VERSION`, add a `PedalStorage` accessor that `main()` restores into `state` (like `RestoredBypass()`), and extend `ServiceSettingsSave()` (a new parameter, mirrored into the RAM copy and included in the change test).

### User preset save / factory restore

A sound can be saved into the current preset on the pedal and later reverted to its
version in the active bank. Saved edits survive power cycles but not a change of firmware
image or active bank.

**Storage** (`src/pedal_storage.cpp:11-13`, `src/pedal_storage.h:11-12`, `:31-54`, `:58-102`):
`PedalStorage`'s `PersistentStorage<UserPresets>` sits at QSPI offset `USER_PRESETS_QSPI_OFFSET` (0x1000). That is the 4 KB sector
after `Settings`, which sits at offset 0 in sector 0. The Daisy bootloader keeps programs at
0x90040000 and never touches the first 256 KB
(`libdaisy/doc/md/_a7_Getting-Started-Daisy-Bootloader.md:82`), and `QSPIHandle::Erase` works in
4 KB sectors, so each store erases only its own sector.
```cpp
struct UserPreset {
    float    params[(int)::Parameter::Count];  // reverb->GetAllParameters() at save time
    float    reverseDelay;                     // state.reverseDelayNorm at save time
    float    delayLines;                       // state.delayLines: 0.0 / 0.5 / 1.0 = One / Default / Max
    float    reverseEnabled;                   // "reverse.enabled", 0.0 or 1.0
    float    reverseDirectMix;                 // "reverse.direct_mix", 0.0 or 1.0
    uint32_t valid;                            // USER_PRESET_VALID, else load from the bank
};  // 220 B (50 params + 5 fields)
struct UserPresets { uint32_t identity; UserPreset presets[kMaxPresets]; }; // 3524 B
```

`identity` (renamed from `firmwareHash`) is `(firmwareImageHash() ^ presetBankHash) * 16777619u`
(`PedalStorage::Init()`, below): the firmware image hash mixed with the FNV-1a hash of whichever
preset bank text is active, so an upload, a revert, or a reflash all discard these edits, not
just a reflash.

- Every field is 4 bytes wide, so there is no padding and `operator!=` is a `memcmp`. A
  `static_assert` keeps `sizeof(UserPresets)` + the 4 B `PersistentStorage` state word within
  one sector; it breaks if `kMaxPresets` goes above 18 (floor((4096 - 4 - 4) / 220))
- A slot is used only when `valid == USER_PRESET_VALID` (1); erased flash reads 0xFFFFFFFF.
  `valid` is the last field, and QSPI pages are programmed in ascending order, so a write cut
  off by power loss can leave a slot invalid (factory) but never half-written
- `params` is the full `GetAllParameters()` snapshot. `LineCount` comes along but is ignored,
  because `LoadPreset()` skips it. `Bloom` and `Scatter` are preset data and are saved for real.
  `delayLines` stores the delay-line mode (`DelayLinesMode` / 2: 0.0 One, 0.5 Default, 1.0 Max;
  a Terrarium only ever stores 0.5 or 1.0); loading rounds it back. The knob map, toggle map,
  `default_delay_lines`/`max_delay_lines`, and blink timing are not saved

**Firmware + bank identity** (`firmwareImageHash()`, `src/pedal_storage.cpp:24-31`): FNV-1a over
the words between the linker symbols `_stext` and `_etext`, which bound `.text` + `.rodata`
(`libdaisy/core/STM32H750IB_sram.lds:36,47`) and include the embedded preset bank.
`PedalStorage::Init(presetBankHash)` (`src/pedal_storage.cpp:41-56`, called from `main()` at
`src/cloudseed.cpp:641` as `gStorage.Init(active.hash)`, before the first `loadPreset()`) mixes
that image hash with the FNV-1a hash of whichever bank text is active
(`(imageHash() ^ presetBankHash) * 16777619u`) into `UserPresets::identity`, and calls
`RestoreDefaults()` (one sector erase) when the stored `identity` differs. Any code change, any
built-in bank change, a USB upload, or a revert therefore discards every saved preset - all four
change one of the two hashes that make up `identity`. A byte-identical reflash of the same bank
keeps them, because nothing distinguishes it from a reboot. `Settings` (preset index, bypass)
are unaffected by either hash.

**Save** (FS1 held 5 s): `processFootswitches()` sets `gSaveRequested`. On the next callback
with the preset-change gate open, the callback copies `GetAllParameters()`, `state.reverseDelayNorm`
and the three toggle pseudo-values into `gSaveSnapshot`, marks it valid, and publishes it with
`state.saveSnapshotReady` (`src/cloudseed.cpp:491-501`). The main loop (`:673-680`) copies the
snapshot into `gStorage.UserSlot(state.currentPreset)`, clears the flag, calls
`gStorage.SaveUserPresets()` (a
blocking erase + write in the main loop; audio keeps running from SRAM), and starts the
confirmation blink.

**Restore** (FS1 + FS2 held 5 s): the callback sets `state.triggerPresetRestore`. The main
loop (`src/cloudseed.cpp:681-687`) clears the slot to `UserPreset{}` (invalid), reloads the preset through
`loadPresetGated()`, which parks the knobs and toggles and drops any unsaved tweaks, calls
`SaveUserPresets()`, and starts the confirmation blink. The save is skipped when the slot was already empty.

Both handlers run before the preset-change block in the main loop, so a snapshot is always
stored into the preset that was loaded when it was taken.

**Confirmation**: `StartConfirmBlink()` / `ServiceConfirmBlink()` (`src/pedal_leds.cpp:116-139`) drive
LED1 and LED2 together for `CONFIRM_BLINKS` (3) on/off cycles of `CONFIRM_BLINK_MS` (80 ms),
even when bypassed. While the blink runs, `ServicePresetBlink()` is skipped. When it ends, LED1 is
re-set to the bypass state and LED2 goes back to the preset pattern. This is distinct from
`FatalErrorLoop()`, which blinks both LEDs at 5 Hz forever.

### USB-MIDI preset upload

The Seed's micro-USB port enumerates as a class-compliant USB-MIDI device at every boot
(`gMidiLink.Init()`, `src/cloudseed.cpp:666`, immediately before `hw.StartAudio()`); no driver
and no button press are needed. A Web MIDI host (typically a browser page) can upload a complete
`presets.toml` text over SysEx, read the active bank's text back, revert to the bank built
into the firmware, or restart the pedal into the Daisy bootloader's DFU mode (for example from a
firmware-switching host, so `dfu-util` can flash with no buttons). Protocol v1 (`F0 7D 43 53 <cmd>
... F7`; INFO/BEGIN/DATA/COMMIT/REVERT/READ/ABORT/DFU) is defined normatively in
[src/preset_protocol.h](src/preset_protocol.h) and documented for host authors in
[docs/USB_MIDI.md](docs/USB_MIDI.md). `0x08 DFU` is additive, so `kProtocolVersion` stays 1 (the
editor rejects any other version). The pedal also answers the standard Universal Non-Real-Time
Identity Request (`F0 7E <id> 06 01 F7`) with `F0 7E 7F 06 02 7D 43 53 <board> 00 00 00 00 00 <build
id> F7`, where the member byte `<board>` (index 8 counting `F0`) is the firmware's `Board`: `00`
Terrarium, `01` HotHouse (`PresetProtocol::Init(..., buildId, board)`, passed `kBoard` at
`src/cloudseed.cpp:665`). That request is not a session message and leaves the session timer alone.
Hosts read the board from it before uploading: a bank for the other board is rejected at COMMIT.

**Build id**: the `Makefile` writes `build/build_id.c` (`char kBuildId[33] = "<id>";`) at parse
time, only when the id changes: `git rev-parse --short HEAD`, plus `+dirty` when `git status
--porcelain --untracked-files=no` is non-empty, or `unknown` outside git; `make BUILD_ID=xyz`
overrides it. `kBuildId` is deliberately non-const with a fixed size: it lands in `.data`, outside
the `_stext.._etext` range `firmwareImageHash()` covers, and a different id moves nothing in that
range, so a commit that changes no code keeps saved user presets and an uploaded bank. Do not make
it `const` (it would move into `.text`) or let its size follow the string.

**Assembler filter**: `SysExAssembler::Feed()` latches only frames `Handle()` can answer - our
`7D 43 53` header or a 4-byte Identity Request - and drops every other complete frame at its F7.
A host may send several devices' frames back to back in one transfer (e.g. every known
firmware's DFU command at once); without the filter the first foreign frame would occupy the
single slot and ours would be dropped.

**Link** ([src/usb_midi_link.h](src/usb_midi_link.h), [src/usb_midi_link.cpp](src/usb_midi_link.cpp)):
`UsbMidiLink` drives `daisy::MidiUsbTransport` directly for receive - libdaisy's `MidiHandler`
truncates SysEx at 128 B (`libdaisy/src/hid/MidiEvent.h:2`) - and its USB-interrupt callback does
nothing but `SysExAssembler::Feed()` (`src/preset_protocol.h`), because the USB OTG FS interrupt
shares priority 0 with the audio DMA (`libdaisy/src/usbd/usbd_conf.c:102-107`,
`libdaisy/src/sys/dma.c:17-50`). Replies bypass `MidiUsbTransport::Tx()`, which splits SysEx and
sends the closing F7 as a packet of its own (`libdaisy/src/hid/usb_midi.cpp:294-318`): they are
packed into USB-MIDI 1.0 event packets by `PackSysExUsbMidi()` and sent with
`UsbHandle::TransmitInternal()`, the same CDC path.

**Main loop** (`serviceUsbPresetLink()`, `src/cloudseed.cpp:541-587`, called every pass at `:700`,
before `gStorage.ServiceSettingsSave()`): answers one received SysEx frame through
`PresetProtocol::Handle()`, performs the resulting `WriteStoredBank()` (COMMIT) or
`EraseStoredBank()` (REVERT) on `gStorage`, sends the reply, and - on a successful COMMIT/REVERT,
a COMMIT that fails after already erasing the bank the pedal is currently running, or DFU -
flushes the pending settings save, waits 100 ms for the reply's USB transfer, and resets: DFU via
`System::ResetToBootloader(DAISY_INFINITE_TIMEOUT)` (the bootloader then waits for `dfu-util`
with no timeout), the others via `NVIC_SystemReset()` (the bootloader reloads the app, which
boots the new bank). It also `Tick()`s the 5 s session timeout and keeps
`state.uploadActive` (a `volatile bool`, `src/cloudseed.cpp:71`) in step with the session: while a
session is open the audio callback passes the input through and skips the reverb
(`state.presetChangeInProgress || state.uploadActive`, `src/cloudseed.cpp:472`); when the session
ends, `reverb->ClearBuffers()` runs and `state.controlResetPending` is set before `uploadActive`
is cleared, so knobs and toggles moved during the session re-park instead of jumping the engine.

**Validation and storage**: an upload is checked with the firmware's own parser
(`ParsePresetText()` with `kBoard` into a dedicated `PresetBank gUploadCheck`,
`src/cloudseed.cpp:127-134`) - the same rules `make presets-check` enforces, including the `board`
key, so a bank for the other board fails with `bank is for board "<file>", this pedal is
"<this>"` - before anything reaches flash. QSPI layout
(`src/stored_bank.h:15-26`): a `StoredBankHeader` (`magic` "CSB1", `length`, `textHash`,
`firmwareHash`) at `STORED_BANK_HEADER_OFFSET` (0x10000, its own 4 KB sector so `REVERT` erases
only it) and the text at `STORED_BANK_TEXT_OFFSET` (0x11000) up to
`PresetProtocol::kMaxTextBytes` (96 KiB). `PedalStorage::WriteStoredBank()`
(`src/pedal_storage.cpp:83-96`) erases both, writes and verifies the text, then writes and
verifies the header last, so a write cut off by power loss leaves no valid stored bank.
`PedalStorage::StoredBankText()` (`src/pedal_storage.cpp:67-73`) hands the memory-mapped header
and text to the host-portable `ValidStoredBankText()` (`src/stored_bank.h:33-43`), which accepts
the bank only if the magic matches, its `firmwareHash` matches the running image (the same
discard-on-reflash rule saved user presets follow), the length is 1..`kMaxTextBytes` (checked
before the text is hashed, so a corrupt header never reads past the region), and the text hash
matches.

**Boot** (`src/cloudseed.cpp:601-617`): the stored bank is used if `StoredBankText()` returns one
and it parses; otherwise the embedded bank (`EmbeddedPresetText()`,
`src/sdram_pool.cpp:72-75`) is used, and a parse failure there is the unrecoverable
`FatalErrorLoop()` case. Either way `gStorage.Init(active.hash)` mixes the active bank's FNV-1a
hash into the user-preset `identity` (see "Firmware + bank identity" above), so an upload, a
revert, or a reflash all wipe saved user presets.

**Tests** (`make test`): [tests/preset_protocol_test.cpp](tests/preset_protocol_test.cpp) covers
the SysEx assembler (including the foreign-frame filter on a multi-device burst), the USB-MIDI
packer, the Identity Reply, DFU, and the upload/read/revert state machine - framing,
sequencing, timeouts and error replies - plus an end-to-end upload of the parser fixture through
USB-MIDI packets and the real parser; [tests/stored_bank_test.cpp](tests/stored_bank_test.cpp)
covers `ValidStoredBankText()`; `ParsePresetBankText()` is covered in `preset_bank_test`. None
of them involve libdaisy.

**Browser editor** ([editor/](editor/), user docs in README.md "Preset editor (browser)"): a
Preact app on Bun (`make -C editor serve`, http://localhost:5174; `make -C editor test check`) that is a
second host for this protocol, for Chrome and Firefox with one code path. CONNECT sends the
Identity Request and reads the board from the member byte (no reply = terrarium, older
firmware). A board key next to PROJECT picks the built-in bank, locked to the pedal's board
while connected. UPLOAD refuses a bank whose `board` differs from the linked pedal's, using
the pedal's own message. With no pedal linked, a bank is checked against its own `board`
(missing or unknown = terrarium). The SWITCH page shows 3 TOGGLE columns (DOWN / MIDDLE / UP)
on hothouse and 4 SW columns (DOWN / UP) on terrarium, and the three-way targets are offered
on hothouse only. A SCATTER page holds the Scatter parameters, the hothouse LINES strip edits
`delay_lines.select`, and FlowPanel shows Bloom/Scatter on TAPS plus three-way target badges.
Keep it in step with the firmware:
- `editor/src/midi/protocol.ts` / `client.ts` port `src/preset_protocol.h` (INFO offsets as in
  `tools/usb_preset_host.py`; `parseIdentityReply()` reads the board at byte 8, and
  `PresetLink.identify()` sends the Identity Request). `PresetLink` is stop-and-wait, and resends only INFO, READ and
  DATA. `upload()` / `revert()` return `null` when the pedal cannot be verified after its
  reboot: `waitForReboot()` gave up after 20 s, or INFO went unanswered. COMMIT `Ok` has
  already stored the bank at that point
- `editor/src/midi/webMidi.ts` finds the pedal by `/daisy/i` in the port name or manufacturer,
  and after a reboot re-polls `requestMIDIAccess()` (no `onstatechange`). Each poll attempt is
  capped at 2 s: in Firefox a Web MIDI call on a port that vanished during the reboot stayed
  pending forever, which hung the upload. Observed 2026-09-26: an occasional re-enumeration
  returns a garbled USB product string (`/proc/asound/cards` shows `USB-Audio - Љ`), so no host
  finds "Daisy" until the pedal is re-plugged. `[INFERENCE]` libdaisy serves every string
  descriptor from one shared static buffer, `USBD_StrDesc` (`libdaisy/src/usbd/usbd_desc.c:289`)
- `editor/src/model/bank.ts` `loadBank()` re-implements `ParsePresetBankText()` with the
  exact messages, for both boards. Its tests port `kRejects` / `kAccepts` /
  `kHotHouseRejects` / `kHotHouseAccepts` / `kTerrariumBoardRejects` from
  `tests/preset_bank_test.cpp` verbatim, so a new parser rule or message needs the same change there. Field edits patch only the
  value token of one line (an absent optional LED timing is inserted after `blinks`;
  duplicate/delete copy or remove whole preset blocks), so an unedited bank round-trips byte-identically (same FNV-1a as
  the pedal)
- `editor/src/model/schema.ts` has a `Board` type plus per-board helpers (toggle count,
  target lists); `loadBank(text, pedal?)` returns the bank's `board`; `state.ts` has
  `mapSpec(kind, board)`. `schema.ts` mirrors `kGroups`, `kToggleParams` and the presets.toml
  parameter reference. `editor/src/model/scale.ts` ports `GetScaledParameter` and
  `ValueTables` for display only
- `editor/src/model/bank.ts` hard-codes `TOTAL_LINE_COUNT = 5` and `schema.ts` copies the
  knob/toggle target lists, so a TotalLineCount change or a new pseudo-target needs them too
  (see "Common Modifications" 1 and 4)
- `editor/src/model/flow.ts` (drawn by `editor/src/components/FlowPanel.tsx`) models the signal
  path of `ReverbChannel::Process()` / `UpdateLines()`, `DelayLine::Process()` (slot order by
  `LateStageTap`; shelves and cutoff in the feedback path only) and the two reverse render
  helpers in `src/cloudseed.cpp`, and its explainer texts describe them. A change to the
  engine's routing, a new parameter, or a new pseudo-target needs the matching `BLOCKS` /
  `EFFECT` entry there; `flow.test.ts` fails when a `PARAM_KEYS` key has no block or effect

### Parameters

50 reverb parameters enumerated in [CloudSeed/Parameter.h](CloudSeed/Parameter.h) (`Parameter::Count` == 50, `CloudSeed/Parameter.h:4-93`); 49 are file-controlled (`LineCount` is runtime-only):

**Input Stage**: InputMix, PreDelay, HighPass, LowPass

**Early Reverb**: TapCount, TapLength, TapGain, TapDecay, Bloom, DiffusionEnabled, DiffusionStages, DiffusionDelay, DiffusionFeedback

**Late Reverb**: LineCount, LineDelay, LineDecay, LateDiffusionEnabled, LateDiffusionStages, LateDiffusionDelay, LateDiffusionFeedback

**Modulation**: EarlyDiffusionModAmount, EarlyDiffusionModRate, LineModAmount, LineModRate, LateDiffusionModAmount, LateDiffusionModRate

**Output**: DryOut, PredelayOut, EarlyOut, MainOut

**Frequency Response**: PostLowShelfGain, PostLowShelfFrequency, PostHighShelfGain, PostHighShelfFrequency, PostCutoffFrequency

**Seeds/Switches/Effects**: TapSeed, DiffusionSeed, DelaySeed, PostDiffusionSeed, CrossSeed, HiPassEnabled, LowPassEnabled, LowShelfEnabled, HighShelfEnabled, CutoffEnabled, LateStageTap, Interpolation

**Early tap animation** (appended after `Interpolation`, in `[preset.params.early]` after `Bloom`):
- `Scatter` - on/off (on when >= 0.5, `CloudSeed/ReverbController.h:162`); toggle target
  `early.Scatter` (in `kToggleParams`), and Down on the HotHouse `early.envelope` target
- `ScatterRate` - 0.1 + Response2Dec(v) × 4.9 Hz (`:163`)
- `ScatterDepth` - linear 0..1 (`:164`)

While Scatter is on, each early tap's gain is scaled by 1 - depth × (1 + sin(2π·phase)) / 2, so
it oscillates between (1 - depth) and 1 of its static value; every tap has its own seeded phase
and a ±15 % rate spread, drawn from fixed seed slots after the tap pattern (`kScatterSeedBase`,
`CloudSeed/MultitapDiffuser.h:23-26`), so adding Scatter does not change the tap pattern. The
tap-vs-clean mix tap (`cleanTap`) is never animated. `MultitapDiffuser::Process()`
(`CloudSeed/MultitapDiffuser.cpp:38`) takes the unchanged static path unless `scatterActive`;
`ProcessScatter()` (`:77`) recomputes each tap's LFO once per block and ramps the gain linearly
per sample across it, and keeps running after Scatter is switched off until every factor is
back at 1 (no click). Its CPU cost is only paid while active and has not been measured on
hardware.

### Memory Architecture

CloudSeed requires massive delay buffers:

```cpp
// src/sdram_pool.cpp:19-38
static constexpr size_t alignUp8(size_t n) {
    return (n + 7u) & ~static_cast<size_t>(7u);
}

constexpr size_t CUSTOM_POOL_SIZE = 48u * 1024u * 1024u;  // 48MB
DSY_SDRAM_BSS __attribute__((aligned(32))) static char custom_pool[CUSTOM_POOL_SIZE];
static size_t pool_index = 0;

void* custom_pool_allocate(size_t size) {
    const size_t aligned = alignUp8(size);
    if (aligned > CUSTOM_POOL_SIZE - pool_index)
        FatalErrorLoop();  // callers placement-new into the result; 0x0 is ITCMRAM on the H750
    void* ptr = &custom_pool[pool_index];
    pool_index += aligned;
    return ptr;
}
```

This custom allocator manages SDRAM for delay lines. It is declared in
[src/sdram_pool.h](src/sdram_pool.h). The reverse voice's `reverseDelayBuffer` is a separate
`DSY_SDRAM_BSS` array in `src/cloudseed.cpp:120`, outside the pool.

Bump allocator, no free. The pool is aligned to the 32-byte M7 D-cache line and every block is
rounded up to 8 bytes. Exhaustion is a fatal boot error (`FatalErrorLoop()`, both LEDs at 5 Hz)
rather than a null return, because a write through 0x0 would land silently in ITCMRAM.
`custom_pool_allocate` keeps external linkage and this exact signature: the CloudSeed headers
declare it `extern`. Callers placement-new into it (e.g.
`CloudSeed/ModulatedDelay.h:41-42`), so destructors of pool-backed objects must not call
`delete` - see [docs/PERFORMANCE.md](docs/PERFORMANCE.md) §6.

The TOML parse arena is not part of this pool: it is a separate 512 KB `DSY_SDRAM_BSS`
buffer (`toml_arena`, `src/sdram_pool.cpp:49-61`), used by `ParsePresetText()`
(`src/sdram_pool.cpp:63-70`) both at boot and to validate a USB upload before it reaches
flash (see "USB-MIDI preset upload" above). `EmbeddedPresetText()` (`:72-75`) hands out the
embedded bank (presets.toml or presets_hothouse.toml).

### Core Classes

**ReverbController** ([CloudSeed/ReverbController.h](CloudSeed/ReverbController.h)):
- Main reverb controller
- Preset application: `LoadPreset(const float* values)` (`:47-60`) copies a parsed
  `PresetData::params[]` into `parameters[]`, skipping only `LineCount` (written at audio rate
  from the delay-line toggle target; `Bloom` is preset data like everything
  else), then re-applies all 50 slots through `SetParameter`. The
  constructor no longer loads any preset - `main()` parses the bank and calls `LoadPreset()`
  before audio starts
- Single channel: `channelR` and all right-channel buffers are commented out
  (`CloudSeed/ReverbController.h:27`, `:29`, `:31`, `:37`, `:72`, `:178`, `:186`, `:199`, `:203`, `:205`, `:211`)
- Fixed internal block size `static const int bufferSize = 48` (`CloudSeed/ReverbController.h:23`),
  backing fixed-size member arrays (`:28-32`)
- Public API: `LoadPreset(const float*)` `:47`, `SetParameter(Parameter, float)` `:172`,
  `ClearBuffers()` `:183`, `Process(float* input, float* output, int bufferSize)` `:189`
- Parameter scaling (the normalized 0.0-1.0 → real-unit mapping documented in presets.toml):
  `GetScaledParameter` `:85`. `Bloom` and `Scatter` follow the same `< 0.5 ? 0 : 1` rule as
  every other on/off parameter (`:101`, `:162`)

**ReverbChannel** ([CloudSeed/ReverbChannel.h](CloudSeed/ReverbChannel.h)):
- Builds `TotalLineCount` (5) delay lines for the mono implementation
  ([CloudSeed/DelayLineCount.h](CloudSeed/DelayLineCount.h)); `SetParameter(LineCount)` clamps the
  active count to 1..`TotalLineCount` (`CloudSeed/ReverbChannel.h:221-231`)
- Contains delay lines, diffusers, modulation

**DelayLine** ([CloudSeed/DelayLine.h](CloudSeed/DelayLine.h)):
- Core delay buffer implementation (263 lines)
- The delay buffer itself is SDRAM-pool-backed (placement-new into `custom_pool_allocate`,
  `CloudSeed/ModulatedDelay.h:41-42`)
- `tempBuffer`, `mixedBuffer`, and `filterOutputBuffer` are real heap allocations
  (`CloudSeed/DelayLine.h:52-54`, freed at `:74-76`)

**AllpassDiffuser, MultitapDiffuser**: Diffusion stages (`MultitapDiffuser` also runs Scatter, see "Parameters")
**ModulatedAllpass, ModulatedDelay**: Modulated processing

## Build System

### Checkout and prerequisites

`libdaisy/` and `DaisySP/` are git submodules (`.gitmodules`) that the build reads
(`Makefile:47-61`, `:76`), so a checkout without them fails at the first `include`. The
`Terrarium/` submodule is still checked out but no longer used: the app's pin maps live in
`src/board.h`.

```bash
git clone --recurse-submodules https://github.com/jimbattin/DaisyCloudSeed.git
# or, in an existing clone / after a pull that moves a submodule:
git submodule update --init --recursive
```

The superproject pins exact commits: libdaisy `v9.0.0`, DaisySP `V1.0.0`, Terrarium `main` at
`cd6c80d`. The `branch =` values in `.gitmodules` are those tag names, not branches, so never use
`git submodule update --remote`: it would move the submodules off the pinned commits.
`--recursive` is required: libdaisy v7+ moved CMSIS, the STM32H7 HAL and the USB device library
into nested submodules (`libdaisy/.gitmodules`), and neither libdaisy nor the app compiles
without them. It also fetches `libdaisy/tests/googletest` and `DaisySP/DaisySP-LGPL`, which the
firmware does not need (DaisySP's `make` builds the LGPL library only if it is present,
`DaisySP/Makefile:227-230`). A full recursive checkout of libdaisy is about 330 MB of working
tree plus 480 MB of git objects; `--shallow-submodules` on the clone reduces the git part.

Tools: the Daisy Toolchain (`arm-none-eabi-gcc`, `make`, `dfu-util`; this tree builds with Arm GNU
Toolchain 13.3.Rel1) and a host `gcc`/`g++` (`HOSTCC`/`HOSTCXX`, `Makefile:80-81`) - plain `make`
needs the host compilers too, because it builds `preset_check` to validate the board's bank.
Then `make libs` once (and after every submodule update), then `make` (or `make BOARD=hothouse`).

### Building Libraries

```bash
# Rebuild all libraries (libcloudseed, DaisySP, libdaisy).
# Makefile:181-185 runs `clean all` in each, so this is a full rebuild of all three.
make libs

# Or build individually:
cd CloudSeed && make
cd libdaisy && make
cd DaisySP && make
```

### Building Effects

```bash
make                 # Terrarium (BOARD=terrarium, the default): build/terrarium/, embeds presets.toml
make BOARD=hothouse  # HotHouse: build/hothouse/, embeds presets_hothouse.toml
```

`BOARD` (`Makefile:8-22`) selects the board define (`CLOUDSEED_BOARD_TERRARIUM` /
`CLOUDSEED_BOARD_HOTHOUSE`), the embedded bank, and `BUILD_DIR = build/<board>`, so both
boards' objects and outputs coexist; any other value is a make error. `build/build_id.c` is shared.
`make` also runs `$(MAKE) -C CloudSeed` on every invocation (`Makefile:63-70`): the sub-make
is incremental and rebuilds `libcloudseed.a` only when a library source changed, and the ELF
relinks only when the archive's mtime moved. A CloudSeed edit therefore never needs `make libs`.

### Checking presets

```bash
# Validity check with the firmware's own parser, without building firmware.
# The same check runs automatically as part of `make`.
make presets-check
```

It builds `build/terrarium/preset_check` (`build/<board>/` for a `BOARD=` override) from
`tools/preset_check.cpp`, `src/preset_bank.cpp`, and the
vendored tomlc99 (compiled as C) using `HOSTCC`/`HOSTCXX` (default `gcc`/`g++`), then runs
`preset_check --board terrarium --validate presets.toml` and `preset_check --board hothouse
--validate presets_hothouse.toml` (`Makefile:106-108`). Unlike the stamp-gated check inside
`make`, it re-runs every time. `preset_check [--board terrarium|hothouse]` (default terrarium;
the file's `board` key must name it) requires exactly one mode: `--validate` (one-line summary; exit 1
with the parser's message on stderr if the file is rejected), `--print-knob-map`, or
`--print-toggle-map`; anything else prints usage and exits 2.

`make` cannot produce firmware from a bank the parser would reject: the embedded blob
(`$(BUILD_DIR)/presets_toml.o`) depends on `$(BUILD_DIR)/presets.valid`, whose recipe is
`preset_check --board $(BOARD) --validate $(PRESETS_TOML)` (`Makefile:83-101`). `preset_check` calls
`ParsePresetBankText()` with the file's full length, exactly as the pedal does at boot, so an
empty file or an embedded NUL byte is rejected too. Validation covers non-ASCII bytes
(any byte >= 0x80, comments included, rejected with its line and column before tomlc99 sees the
text: `checkAscii()`, `src/preset_bank.cpp:899-924`), the `board` key, TOML syntax,
missing/unknown/misplaced parameters, unknown preset- and root-level keys, out-of-range
scalars (a parameter value that is non-finite or outside 0..1; `max_delay_lines` /
`default_delay_lines` that are not whole numbers 1..5 or where the default exceeds the max;
`delay_lines.select` not 0, 0.5 or 1; `blinks`, the ms fields), an unresolvable
`[preset.knob_map]` / `[preset.toggle_map]` target (unknown group/parameter, wrong group, a
runtime parameter, a three-way target on Terrarium, or - for toggles - a parameter not
in `kToggleParams`), and a document too large for the boot parse arena - the host tool allocates
through a replica of `TOML_ARENA_SIZE` (512 KB, 8-byte aligned, no reuse), so `presets.toml: 10
terrarium presets valid, boot arena peak 147096 of 524288 bytes` (and `presets_hothouse.toml:
10 hothouse presets valid, boot arena peak 143992 of 524288 bytes`) is the same peak the pedal sees. Host
pointers are 64-bit, so the reported peak over-estimates the 32-bit target: a pass here implies
a fit on hardware. A near-miss should be fixed by raising `TOML_ARENA_SIZE` (`src/sdram_pool.cpp:49`),
not by loosening the host check.

Preset values are free to change: `make presets-check` fails only on input the parser rejects.
There is no golden-value comparison; the original factory values are recoverable from git
history.

A file that somehow reaches the pedal broken is unrecoverable at runtime: `FatalErrorLoop()`
(`src/pedal_leds.cpp:45-54`, called at `src/cloudseed.cpp:614`) blinks both LEDs at 5 Hz forever and never starts
audio.

### Host unit tests

```bash
make test
```

Builds seven host executables into `build/terrarium/` with `HOSTCC`/`HOSTCXX` (`Makefile:110-151`) and runs them;
no ARM toolchain or firmware build is involved. Each prints `<suite>: N checks, 0 failed` and
exits non-zero on any failed `CHECK()` ([tests/check.h](tests/check.h)):
- `knob_bank_test` - `KnobBank` parking, move threshold, 50-block takeover glide, apply
  epsilon, rails, bank change, reset mid-glide
- `toggle_bank_test` - `ToggleBank` parking on first scan / bank change / `forceReset`, and
  immediate writes after a flip, including three-position values
- `footswitch_gestures_test` - `FootswitchGestures` driven through a model of libdaisy's 8-bit
  `Switch` shift register: taps, 5 s save, FS2 hold + `MarkEdited()`, staggered chords, the
  restore chord, bounces, and a falling edge without a prior `Pressed()`
- `preset_bank_test tests/fixtures/two_presets.toml tests/fixtures/two_presets_hothouse.toml` -
  `ParsePresetBank()` on a two-preset Terrarium fixture (Chorus + Through the Looking Glass),
  then a table of single-line mutations (`kRejects`) that
  must each be rejected with a specific message (non-ASCII bytes in a value and in a comment
  among them), a check of the line and column reported for a non-ASCII byte, mutations that
  must be accepted (`kAccepts`), a float `led_on_ms = 300.0` that must be honoured, and a
  31-byte name that must be kept whole; then the HotHouse fixture (the same two presets) with
  `kHotHouseRejects` / `kHotHouseAccepts`, the HotHouse-only targets on a Terrarium bank
  (`kTerrariumBoardRejects`), and the cross-board `board` mismatch. `ParsePresetBankText()` is checked to parse exactly
  `length` bytes of text with no NUL terminator, to leave its source untouched, to release
  every allocation on success and on failure, and to reject empty text, embedded NUL bytes,
  and a failed scratch allocation
- `engine_alloc_test` - the whole CloudSeed library compiled for the host, with counting
  replacements for `operator new` and `custom_pool_allocate`: after boot, every parameter is
  swept 0 → 0.25 → 0.5 → 0.75 → 1 → 0.5 through `SetParameter()` with a `Process()` block after each write, and
  must cause zero heap and zero pool allocations (see "Performance Architecture")
- `preset_protocol_test tests/fixtures/two_presets.toml` - `SysExAssembler` framing (split
  frames, real-time bytes, overflow, aborted, empty and restarted frames, and a burst of other
  devices' frames that must not hide ours), `PackSysExUsbMidi()`
  CIN/padding, and the `PresetProtocol` state machine: INFO fields, BEGIN length checks and
  restart, chunked upload + COMMIT including a full 98,304-byte upload and an overrun at that
  size, duplicate/gapped/wrapped sequence numbers, length/hash mismatches (hash bits 28-31
  included), `ParseError` and `FlashError` replies, READ chunking, the 5 s session timeout,
  ABORT, REVERT, DFU, the Identity Reply (any device id, board member byte, build id fallback/truncation/
  sanitising, session timer untouched), and foreign/unknown SysEx. Every reply is checked to be
  one well-formed 7-bit SysEx message. An end-to-end case uploads the fixture through USB-MIDI packets and the
  real parser, and checks that a rejected bank's COMMIT reply carries the parser's own message
- `stored_bank_test` - `ValidStoredBankText()`: erased flash, wrong magic, a different firmware
  image, corrupted text or hash, and the 1..`kMaxTextBytes` length bounds

The browser editor has its own `bun:test` suite, which `make test` does not run:
`make -C editor test` (App, FlowPanel, Fader, flow, state, bank, scale, client, protocol);
`make -C editor check` runs `tsc --noEmit` and `biome check`, and `make -C editor fmt` applies
Biome's formatting and safe fixes.
It uses the same tests/fixtures/two_presets.toml and two_presets_hothouse.toml, and
`bank.test.ts` ports `kRejects` / `kAccepts` (and the HotHouse tables) from
`tests/preset_bank_test.cpp`. It needs presets_hothouse.toml and
tests/fixtures/two_presets_hothouse.toml, reads both live banks and expects exactly 10 presets
in each, so adding a preset to either bank, or removing one, fails `bun test` (not `make test`).
Every TOML file (the two banks, the two fixtures) is imported `with { type: 'text' }` (tests and `src/io/files.ts`): a bare
`.toml` import would make Bun parse the file instead of handing over its exact bytes. Bun's
dev-server bundler ignores that attribute (observed on Bun 1.4.2: the dev server inlined the
parsed table), so `bunfig.toml` `[serve.static]` loads `.toml` as text through
`editor/toml-text-plugin.ts`; `Bun.build` (editor/build.ts) and `bun test` honour the attribute
on their own.

The parser test deliberately uses its own fixtures, not presets.toml / presets_hothouse.toml,
so editing preset values never breaks `make test`; the live files stay covered by `make
presets-check` and the build gate.

### Build Outputs

Located in `build/<board>/` (`build/terrarium/` or `build/hothouse/`):
- **{target}.bin** - Binary for DFU flashing via USB
- **{target}.elf** - ELF executable with debug symbols
- **{target}.hex** - Intel HEX format
- **{target}.map** - Linker map file
- **preset_check** and the host test executables

Located in `build/`:
- **build_id.c** - the generated build id (shared by both boards)
- **editor.html** - the browser editor as one stand-alone file (`make editor-html`: runs
  `make -C editor html`, a `Bun.build` with `compile: true, target: "browser"` in
  editor/build.ts, then copies editor/dist/editor.html here; JS/CSS and both boards' preset
  banks inlined; opens from `file://`). editor/Makefile rebuilds it only when editor/src,
  build.ts, the package files or a preset bank change

This project builds with `APP_TYPE = BOOT_SRAM` (`Makefile:6`): the application is loaded into
SRAM from QSPI flash by the Daisy bootloader, which is what allows room for all ten presets.

### Flashing to Hardware

```bash
# One time (or after bootloader updates): flash the Daisy bootloader over DFU
make program-boot

# Then reset the Daisy, hold BOOT until the LED blinks rapidly, and flash the app
make program-dfu                 # Terrarium: flashes build/terrarium/cloudseed.bin
make BOARD=hothouse program-dfu  # HotHouse: flashes build/hothouse/cloudseed.bin
```

Both targets come from `libdaisy/core/Makefile:348-352` and require `dfu-util`. `program-boot`
flashes `BOOT_BIN`, the bootloader shipped with libdaisy (`dsy_bootloader_v6_4-intdfu-2000ms.bin`
at v9.0.0, unchanged since v8.1.0, `libdaisy/core/Makefile:220`); a pedal still running an older bootloader keeps working
but misses its fixes (v6.3+ carries libdaisy's QSPI write-protect fix), so re-run it after a
libdaisy bump that ships a new one. A `BOOT_SRAM` build cannot be flashed with `make program`
(openocd) - libdaisy errors out on that path (`libdaisy/core/Makefile:340-341`). Same procedure
as README.md "Getting started".

### Compiler Configuration

**Platform**: ARM GCC (`arm-none-eabi-gcc`)
**CPU**: Cortex-M7 (`-mcpu=cortex-m7`, `CloudSeed/Makefile:70`)
**Optimization**: `-O3` (`Makefile:51`, `CloudSeed/Makefile:20`)
**FPU**: Hard float (`-mfpu=fpv5-d16 -mfloat-abi=hard`, `CloudSeed/Makefile:73-76`)
**Language**: C++14 (`-std=gnu++14`, `CloudSeed/Makefile:67`)
**Float flags**: `-ffast-math` on both the app (`Makefile:74`) and the library
(`CloudSeed/Makefile:106`); the library additionally uses `-fno-exceptions`,
`-finline-functions`, and `-fno-aggressive-loop-optimizations` (`CloudSeed/Makefile:104-108`)
**App type**: `BOOT_SRAM` (`Makefile:6`)

## Common Modifications

### 1. Changing Control Mappings

**File**: [presets.toml](presets.toml) / [presets_hothouse.toml](presets_hothouse.toml) - no C++ changes required for a remap.

**Example**: Swap KNOB_1 and KNOB_2 in one preset - edit that preset's `[preset.knob_map]`:

```toml
knob1_a = "output.EarlyOut"
knob2_a = "output.DryOut"
```

Then `make` (which validates the file) and `make program-dfu` (`BOARD=hothouse` for the
HotHouse bank). The map is per preset, so the
same two lines must be edited in every preset that should share the scheme. Verify the
resolved map with `./build/terrarium/preset_check --print-knob-map presets.toml` (HotHouse:
`./build/terrarium/preset_check --board hothouse --print-knob-map presets_hothouse.toml`).

**Example**: Put the input low-pass on KNOB_6's secondary bank:

```toml
knob6_b = "input.LowPass"
```

`applyKnobTarget()` (`src/cloudseed.cpp:222-255`) turns `LowPassEnabled` on the first time that
knob is moved, so the filter is audible even in presets that ship with it off; the takeover
glide starts from the open filter (`knobTargetValue()`), so enabling it does not step the tone.
`HighPass` gets the same treatment via `HiPassEnabled`; no other gated parameter does - for the
shelves, the in-loop cutoff, and the diffusers, set the matching `*Enabled` value in
`[preset.params.*]`. Either filter's enable can instead be owned by a toggle target (see
below), in which case
`applyKnobTarget()` leaves the enable alone and the knob only moves the cutoff.

`applyKnobTarget()` is also where the output-level cache is invalidated: its `switch` sets
`state.outputLevelsDirty` for `DryOut`/`EarlyOut`/`MainOut`. Any new code path that writes one
of those three parameters outside `loadPreset()` must set that flag, or the makeup gain and the
dry-cancellation scale will stay at their old values.

**Adding a non-parameter knob target** (like `reverse.delay`) requires C++: a new
`KnobTargetKind` in [src/preset_bank.h](src/preset_bank.h), a branch in `parseKnobTarget()`
([src/preset_bank.cpp](src/preset_bank.cpp)), and a branch in `applyKnobTarget()`
(`src/cloudseed.cpp:222-255`), plus the matching branch in `parseKnobTarget()` in
editor/src/model/bank.ts and the `KNOB_TARGETS` entry in editor/src/model/schema.ts.

**Example**: reassign SWITCH_2 (Bloom by default; HotHouse: toggle2) to enable the late low-pass instead - edit
that preset's `[preset.toggle_map]`:

```toml
toggle2_a = "late_eq.CutoffEnabled"
toggle2_b = "late_eq.CutoffEnabled"
```

Verify with `./build/terrarium/preset_check --print-toggle-map presets.toml`. A toggle value is
one of the pseudo-targets or a quoted `"group.Parameter"` naming one of the thirteen on/off
parameters in `kToggleParams` ([src/preset_bank.cpp](src/preset_bank.cpp)): `early.Bloom`,
`early.Scatter`, `input.HiPassEnabled`, `input.LowPassEnabled`, `early_diffusion.DiffusionEnabled`,
`early_diffusion.DiffusionStages`, `late_diffusion.LateDiffusionEnabled`,
`late_diffusion.LateDiffusionStages`, `late_eq.LowShelfEnabled`, `late_eq.HighShelfEnabled`,
`late_eq.CutoffEnabled`, `late.LateStageTap`, `late.Interpolation` - plus `delay_lines.max`,
`reverse.enabled`, `reverse.direct_mix` (on/off: on only with the lever Up, so a HotHouse Middle
is off), and on HotHouse only the three-way `delay_lines.select`, `early.envelope`,
`reverse.mode` (Terrarium rejects them with `'<text>' needs a three-position toggle`). A
continuous parameter (e.g. `late.LineDecay`) is
rejected with `'Name' is not an on/off parameter`: a lever would slam it to 0 or 1.

**Adding a non-parameter toggle target** (like `delay_lines.max`) requires C++: a new
`ToggleTargetKind` in [src/preset_bank.h](src/preset_bank.h) (after
`ToggleTarget_DelayLinesSelect` if it is three-way, so `IsThreeWayToggleTarget()` holds), a
branch in `parseToggleTarget()` ([src/preset_bank.cpp](src/preset_bank.cpp)), and a branch in
`applyToggleTarget()` (`src/cloudseed.cpp:261-288`), plus `parseToggleTarget()` in
editor/src/model/bank.ts and `TOGGLE_TARGETS` in editor/src/model/schema.ts.

### 2. Modifying Parameter Ranges

Knobs are read raw: `hw.knob[i].Value()` (`src/cloudseed.cpp:478-480`) yields 0.0-1.0
and is handed straight to `SetParameter`, which applies the engine's own scaling
(`ReverbController::GetScaledParameter`). There is no per-knob min/max any more - the six
`::daisy::Parameter` wrappers were removed when knob targets became data.

To restrict a knob's travel, scale in `applyKnobTarget()` (`src/cloudseed.cpp:222-255`) before the
`SetParameter` call, e.g. `value = 0.5f + 0.5f * value;` for the upper half of the range, **and**
apply the inverse for that parameter in `knobTargetValue()` (`src/cloudseed.cpp:293-314`), e.g.
`(v - 0.5f) / 0.5f` clamped to 0..1. The takeover glide runs in knob space from
`knobTargetValue()` to the pot, so without the inverse its first step writes `0.5 + 0.5 * stored`
instead of `stored` - a jump. A stored value outside the knob's range still jumps to the nearest
end of that range on the first write. Both changes affect every preset that maps a knob to that
parameter.

The knob response constants: `KNOB_SMOOTHING_COEFF` (`src/cloudseed.cpp:48`, the ADC one-pole), and in
[src/knob_bank.h](src/knob_bank.h) `kKnobMoveThreshold` (how far a parked knob must move to take over),
`kKnobApplyEpsilon` (the smallest change a live knob re-writes), `kKnobRailWindow` (how close to a
stop reads as exactly 0.0 / 1.0), and `kKnobGlideBlocks` (takeover glide length in 1 ms blocks;
100 if the takeover still clicks, 25 if it feels laggy).

### 3. Adding or Editing Presets

**File**: [presets.toml](presets.toml) (Terrarium) / [presets_hothouse.toml](presets_hothouse.toml) (HotHouse) - no C++ changes required.

Append a new `[[preset]]` table at the end of the file (appending keeps the existing indices,
which are persisted to QSPI flash). The file's root `board = "terrarium"` / `"hothouse"` key
stays at the top. A Terrarium preset:

```toml
# --------------------------------------------------------------------------
[[preset]]
name = "Your New Preset"
blinks = 11
led_on_ms = 150
led_off_ms = 150
led_pause_ms = 5000
default_delay_lines = 2.0
max_delay_lines = 5.0

# Knob assignments. Primary (_a) is the knob's normal function; secondary (_b)
# is active only while the preset footswitch (FS2) is held down. All twelve keys required.
[preset.knob_map]
knob1_a = "output.DryOut"
knob2_a = "output.EarlyOut"
knob3_a = "output.MainOut"
knob4_a = "late_diffusion.LateDiffusionFeedback"
knob5_a = "early.TapDecay"
knob6_a = "late.LineDecay"
knob1_b = "input.PreDelay"
knob2_b = "input.HighPass"
knob3_b = "input.LowPass"
knob4_b = "late.LineModAmount"
knob5_b = "late.LineModRate"
knob6_b = "reverse.delay"

# Toggle assignments. Primary (_a) is the switch's normal function; secondary (_b)
# is written by flipping the switch while the preset footswitch (FS2) is held down.
# All eight keys required (HotHouse: toggle1..toggle3, six keys).
[preset.toggle_map]
toggle1_a = "delay_lines.max"
toggle2_a = "early.Bloom"
toggle3_a = "reverse.enabled"
toggle4_a = "reverse.direct_mix"
toggle1_b = "delay_lines.max"
toggle2_b = "early.Bloom"
toggle3_b = "reverse.enabled"
toggle4_b = "reverse.direct_mix"

# Input stage: pre-delay and the input filters feeding the whole reverb.
[preset.params.input]
InputMix       = 0.0
PreDelay       = 0.07
HiPassEnabled  = 0.0
HighPass       = 0.0
LowPassEnabled = 0.0
LowPass        = 0.29

# Early reflections: the multi-tap delay that follows the input stage.
[preset.params.early]
# ...
Bloom        = 0.0
Scatter      = 0.0
ScatterRate  = 0.5
ScatterDepth = 0.5

# ... the remaining five [preset.params.*] groups, every key required

# Delay lines: stored state of the "delay_lines.max" toggle target
# (off = this preset's default_delay_lines, on = its max_delay_lines).
# HotHouse instead: select = 0.5 (0 = one line, 0.5 = default, 1 = max).
[preset.params.delay_lines]
max = 0.0

# Reverse voice: window length (knob target "reverse.delay") and the stored state of
# the toggle targets "reverse.enabled" (voice on) and "reverse.direct_mix" (routing).
[preset.params.reverse]
delay      = 0.4771
enabled    = 0.0
direct_mix = 0.0
```

Rules the parser enforces (a violation fails `make` before the blob is embedded; if one were
ever flashed it would stop boot and blink both LEDs):
- The bank must be plain ASCII, comments included (`line L, column C: non-ASCII byte 0xNN`):
  tomlc99 passes plain `char`s to `isdigit()`, which is undefined for bytes >= 0x80, so
  `ParsePresetBank()` rejects them before parsing (`src/preset_bank.cpp:899-924`, called from
  `ParsePresetBank()` at `:931`)
- The root key `board` must name the board the bank is built or uploaded for (`missing board =
  "<thisboard>"`, `board '<x>' is not "terrarium" or "hothouse"`, `bank is for board "<file>",
  this pedal is "<this>"`)
- All eight `[preset.params.*]` parameter groups must be present, each containing exactly its
  own keys - 49 parameters total. Group membership is defined by `kGroups` in
  [src/preset_bank.cpp](src/preset_bank.cpp) and mirrored by the reference comment at the top of
  presets.toml and by `GROUPS` / `TOGGLE_PARAMS` in editor/src/model/schema.ts
- `[preset.params.reverse]` must be present with exactly `delay`, `enabled` and `direct_mix`,
  each a number 0..1 (`missing [preset.params.reverse]`, `missing or non-numeric
  'reverse.delay'`, `'reverse.delay' = V out of range 0..1`,
  `[preset.params.reverse]: unknown key 'K'`). `delay` is the value `reverse.delay` starts at
  when the preset loads: 20 + Response3Oct(delay) × 1980 ms; `enabled` and `direct_mix` are the
  toggle pseudo-targets' stored starting state
- `[preset.params.delay_lines]` must be present with exactly one key: on Terrarium `max`, a
  number 0..1 (the stored state of `"delay_lines.max"`: Max when >= 0.5, else Default); on
  HotHouse `select`, exactly 0, 0.5 or 1 (One / Default / Max; else `preset N:
  'delay_lines.select' must be 0, 0.5 or 1`). The other board's key is an unknown key
- `LineCount` alone must not appear (the delay-line toggle target owns it at audio rate)
- `[preset.knob_map]` must be present with all twelve `knobN_a` / `knobN_b` keys. Each value
  is a quoted `"group.Parameter"` whose parameter really belongs to that group, or
  `"reverse.delay"`. Unknown knob keys, unknown groups/parameters, cross-group targets, and
  runtime parameters are all rejected
- `[preset.toggle_map]` must be present with every `toggleN_a` / `toggleN_b` key: eight on
  Terrarium (N = 1..4 = SWITCH_1..SWITCH_4), six on HotHouse (N = 1..3; `toggle4_*` is unknown).
  Each value is a quoted `"group.Parameter"` naming one of the
  thirteen on/off parameters in `kToggleParams` ([src/preset_bank.cpp](src/preset_bank.cpp)), one of
  the binary pseudo-targets `"delay_lines.max"`, `"reverse.enabled"`, `"reverse.direct_mix"`, or
  (HotHouse only; Terrarium: `'<text>' needs a three-position toggle`) one of the three-way
  `"delay_lines.select"`, `"early.envelope"`, `"reverse.mode"`.
  Unknown toggle keys, unknown groups/parameters, cross-group targets, runtime parameters, and
  continuous (non-boolean) parameters are all rejected
- `blinks` is 1..20; `default_delay_lines` and `max_delay_lines` are each a whole number
  1..`TotalLineCount` (5; a fraction or `nan` fails with `preset N: <key> must be a whole number 1..5`), and
  `default_delay_lines` must not exceed `max_delay_lines`
  (`preset N: default_delay_lines exceeds max_delay_lines`); the ms fields are optional whole
  numbers 0..60000, written `300` or `300.0` (defaults 150/150/5000 when absent; a fraction, a
  string or an out-of-range value fails with `preset N: <key> must be a whole number 0..60000`);
  `name` must be non-empty and at most 31 bytes (`kMaxPresetNameLen` - 1; longer fails with
  `preset N: name longer than 31 bytes`); at most `kMaxPresets` (16) presets
- Every parameter value must be finite and within 0.0-1.0 (`preset N: 'Name' = V out of range
  0..1`): `AudioLib::ValueTables::Get` indexes its tables with the raw value, so anything else
  would read out of bounds on the pedal. The real-unit ranges are listed in the TOML header and
  implemented by `ReverbController::GetScaledParameter`

Then rebuild and reflash: `make && make program-dfu` (`make BOARD=hothouse && make BOARD=hothouse
program-dfu`) - validation is part of `make` (`make presets-check` runs the same check on both
banks without building firmware).
`NUM_PRESETS` no longer exists - the count comes from `gPresets.count` and the modulo cycling
adapts automatically.

**Reordering or deleting presets** invalidates saved settings: bump `SETTINGS_VERSION`
(`src/pedal_storage.cpp:5`) in the same change so stale flash contents are discarded. Saved user
presets need nothing: any change to the embedded bank alters the firmware hash, which discards them all
at the next boot.

**Blink Pattern Customization**: per preset, via `blinks`, `led_on_ms`, `led_off_ms`, and
`led_pause_ms`.

### 4. Changing Number of Delay Lines

**File**: [CloudSeed/DelayLineCount.h](CloudSeed/DelayLineCount.h)

```cpp
constexpr int TotalLineCount = 5;  // CloudSeed/DelayLineCount.h:14
```

`TotalLineCount` is the single definition of how many `DelayLine`s `ReverbChannel` builds, and
everything that depends on it follows automatically:
- `ReverbChannel::SetParameter(LineCount)` clamps the active count to 1..`TotalLineCount`
  (`CloudSeed/ReverbChannel.h:221-231`), so no caller can make `Process` loop past the lines that
  exist (`:411`, `:414`). The lower bound matters too: `ReverbController::LoadPreset()` re-applies
  the stored `LineCount`, which is 0 at boot until the audio callback sets the real count
- `readLineCount()` (`src/preset_bank.cpp:198-214`) validates `default_delay_lines` /
  `max_delay_lines` against it, so `make` rejects a preset asking for more lines than exist,
  naming the preset and key: `preset N: max_delay_lines must be a whole number 1..<TotalLineCount>`.
  The `preset_check` and `preset_bank_test` rules list the header as a prerequisite, so they are
  rebuilt when it changes

After lowering it, `make` fails until every preset's `default_delay_lines` / `max_delay_lines`
fits (nine ship `max_delay_lines = 5.0` in each bank). Also update the `1..5` wording in both banks'
header and this file, the "up to 5" delay-line wording in README.md, the expected `1..5`
messages and 5.0 values in `tests/preset_bank_test.cpp` / `tests/fixtures/two_presets.toml` /
`tests/fixtures/two_presets_hothouse.toml` /
`editor/src/model/bank.test.ts`, and `TOTAL_LINE_COUNT` in `editor/src/model/bank.ts`. Raising it costs SDRAM pool
memory and CPU per line ("Through the Looking Glass" already crackles above 4).

### 5. Modifying Switch Behavior

**File**: [presets.toml](presets.toml) / [presets_hothouse.toml](presets_hothouse.toml) for
reassigning a switch (see "1. Changing Control Mappings" above); [src/board.h](src/board.h) for
the pins (`kSwitchPinTable`) and `PedalBoard::TogglePosition()`;
[src/cloudseed.cpp](src/cloudseed.cpp) for the mechanism itself
(`src/cloudseed.cpp:359-400` for `updateEngineControls()`, which scans and dispatches both knobs and
toggles; `src/cloudseed.cpp:261-288` for `applyToggleTarget()`; `src/cloudseed.cpp:504-512` for
`reverse.enabled`/`reverse.direct_mix` in the callback)

Every switch's function is data, resolved per preset by `[preset.toggle_map]`. The Terrarium
default map reproduces the historical assignment: SWITCH_1 selects the delay-line count
(`state.delayLines`, Default / Max); SWITCH_2 is Bloom
(`Parameter::Bloom`); SWITCH_3 toggles the reverse voice (`state.reverseEnabled`); SWITCH_4
selects reverse routing (`state.reverseDirectMix`; off = into the reverb, on = direct mix). The
HotHouse default map uses the three-way targets: toggle1 `delay_lines.select` (One / Default /
Max), toggle2 `early.envelope` (Scatter / neither / Bloom), toggle3 `reverse.mode` (off / into
reverb / direct). The
reverse window length is not a toggle target: it starts at the preset's
`[preset.params.reverse] delay` and is then set by whichever knob maps to `"reverse.delay"`.
Footswitch gestures (tap, hold, the 5 s save and the restore chord, including
`kLongHoldBlocks`) live in [src/footswitch_gestures.h](src/footswitch_gestures.h).

To change what a switch does without touching the banks (e.g. a completely different scheme
for every preset, or another pseudo-target), add a case to `ToggleTargetKind`
([src/preset_bank.h](src/preset_bank.h)), a branch in `parseToggleTarget()`
([src/preset_bank.cpp](src/preset_bank.cpp)), and a branch in `applyToggleTarget()`
(`src/cloudseed.cpp:261-288`), plus the editor's `parseToggleTarget()` / `TOGGLE_TARGETS`; see
"1. Changing Control Mappings" above.

Toggles are read with `hw.TogglePosition(i)` (`src/cloudseed.cpp:482-484`), which returns
`kToggleDown` / `kToggleMiddle` / `kToggleUp` from the debounced `Switch::Pressed()` of the
toggle's pin(s) (a grounded pin is pressed).

### 6. Adjusting Audio Buffer Size

**File**: [src/cloudseed.cpp](src/cloudseed.cpp) (`src/cloudseed.cpp:591`, `:652-668`)

```cpp
// Current: 48 samples per block
hw.StartAdc();
// ... 200 ms knob one-pole settle loop (see "Knob take-over" above) ...
hw.StartAudio(audioCallback);
```

Two sizes are coupled and must change together:
- `AUDIO_BUFFER_SIZE` (`src/cloudseed.cpp:29`) is also the hardware block size:
  `hw.Init(AUDIO_BUFFER_SIZE)` (`src/cloudseed.cpp:591`) sets it in `PedalBoard::Init()` before
  the knobs are initialised, so their one-poles run at the real callback rate (see "ADC
  smoothing"). It sizes the file-scope `gInputBuffer`, `gWetBuffer`,
  `gReverseBuffer` and `gReverbInputBuffer` (`src/cloudseed.cpp:322-325`) and every callback loop;
  `main()` stops in `FatalErrorLoop()` if `hw.AudioBlockSize()` differs (`src/cloudseed.cpp:598-599`)
- `ReverbController::bufferSize` (`CloudSeed/ReverbController.h:23`) sizes the controller's
  fixed member arrays

Raising `AUDIO_BUFFER_SIZE` but not `bufferSize` overruns the controller's arrays.
Smaller blocks = lower latency, higher CPU load; larger blocks = higher latency, lower CPU load.

These are counted in callbacks and assume the 1 ms block (48 samples at 48 kHz); rescale them
with the block period:
- `kKnobGlideBlocks` (`src/knob_bank.h:26`): 50 blocks = the 50 ms takeover glide
- `kLongHoldBlocks` (`src/footswitch_gestures.h:24`): 5000 blocks = the 5 s save / restore holds
- `KNOB_SMOOTHING_COEFF` (`src/cloudseed.cpp:48`): a per-callback one-pole coefficient, ~20 ms
  at 1 kHz
- libdaisy's `Switch::Debounce()` shifts at most once per ms (`libdaisy/src/hid/switch.cpp:34`):
  with longer blocks it shifts once per block, so the 8-shift press latch and 7-shift release
  edge stretch with the block

Everything timed with `System::GetNow()` / `System::Delay()` (the blink patterns, the
confirmation blink, the 3 s settings save, the 200 ms knob settle loop) is independent of the
block size.

### 7. Adding CV Control

Both boards' knob inputs can accept CV (0-3.3V or 0-5V with voltage divider):

```cpp
// Read CV directly (identical to what the knob scan uses):
float cv_value = hw.knob[0].Value();  // knob1 (D16)
// cv_value is 0.0-1.0 regardless of input voltage
```

### 8. LED2 Preset Indicator System

**Current Implementation**: LED2 uses a state machine to blink the preset number continuously (runs in main loop).

The system is defined in [src/pedal_leds.cpp](src/pedal_leds.cpp) (`BlinkPattern` in
[src/pedal_leds.h](src/pedal_leds.h):9-14, `BlinkState` `src/pedal_leds.cpp:10-16`;
`StartPresetBlink` :57-64; `ServicePresetBlink` :67-113). The LED objects are file statics
there, reachable only through the `pedal_leds.h` functions. The pattern itself is
`state.blinkPattern`, cached by `loadPreset()` and passed in by the main loop; there is no
per-call lookup function. Both functions only `Set()` LED2; the audio callback's
`UpdateLeds()` pushes it to the pin.

**Key Components**:
- `BlinkPattern` struct: Defines blink timing parameters
- `BlinkState` struct: Maintains blink state machine
- `ServicePresetBlink(bypass, pattern)`: Main loop function that manages LED transitions
- `state.blinkPattern`: the active preset's pattern, refreshed by `loadPreset()`
- `StartPresetBlink()`: Initiates a new blink sequence

**Behavior**:
- Blinks continuously when pedal is active (not bypassed)
- Turns off completely when bypassed
- Blinks N times where N = the preset's `blinks` value (1-20; the preset number in both
  shipped banks)
- `led_on_ms` on, `led_off_ms` off per blink (150 / 150, the default and the shipped value)
- `led_pause_ms` pause between sequences (5000 ms, the default and the shipped value)
- Automatically restarts sequence after pause
- Save / restore confirmation: `ServiceConfirmBlink()` takes over LED1 and LED2 for 3 × 80 ms
  on/off (even when bypassed) and `ServicePresetBlink()` is skipped until it finishes; LED2 then
  restarts its preset pattern and LED1 returns to the bypass state

**To modify LED2 behavior for custom purposes**, you would need to modify or replace `ServicePresetBlink()` (`src/pedal_leds.cpp`), called from the main loop. The current implementation is tightly integrated with preset indication.

## Code Navigation Tips

### Key Files for Modification

**Most Common**:
- [presets.toml](presets.toml) / [presets_hothouse.toml](presets_hothouse.toml) - all preset
  data **and the knob and toggle maps** per board (parsed at boot; validated by `make`)
- [src/board.h](src/board.h) - `PedalBoard`: per-board pins and toggle position decoding
- [src/cloudseed.cpp](src/cloudseed.cpp) - `main()`, pedal state, audio callback, knob/toggle dispatch, preset load
- [src/knob_bank.h](src/knob_bank.h) - absolute, parked knob take-over state machine (park, 50 ms glide, track)
- [src/toggle_bank.h](src/toggle_bank.h) - parked toggle take-over state machine (no glide)
- [src/footswitch_gestures.h](src/footswitch_gestures.h) - FS1/FS2 gesture state machine: taps, FS2 hold (secondary bank), FS1 5 s save, FS1 + FS2 5 s restore
- [src/pedal_leds.cpp](src/pedal_leds.cpp) - LED1/LED2: preset blink, save/restore confirmation, `FatalErrorLoop()`
- [src/pedal_storage.cpp](src/pedal_storage.cpp) - QSPI `Settings`, `UserPresets` and the uploaded preset bank (`PedalStorage`), firmware + bank identity hash

**Advanced**:
- [src/preset_bank.cpp](src/preset_bank.cpp) - TOML schema, group membership, and validation errors
- [src/sdram_pool.cpp](src/sdram_pool.cpp) - `custom_pool_allocate()` SDRAM pool and the TOML parse arena (boot parse and USB-upload validation)
- [tests/](tests/) - host unit tests (`make test`)
- [src/preset_protocol.h](src/preset_protocol.h) / [src/usb_midi_link.cpp](src/usb_midi_link.cpp) - USB-MIDI SysEx preset upload protocol and link
- [src/stored_bank.h](src/stored_bank.h) - uploaded-bank QSPI layout + `ValidStoredBankText()`
- [editor/](editor/) - browser preset editor; `editor/src/model/bank.ts` and `schema.ts` mirror src/preset_bank.cpp and must change with it
- [CloudSeed/ReverbController.h](CloudSeed/ReverbController.h) - `LoadPreset`, parameter scaling
- [CloudSeed/Parameter.h](CloudSeed/Parameter.h) - All available reverb parameters
- [CloudSeed/ReverbChannel.h](CloudSeed/ReverbChannel.h) - Core reverb architecture

**Reference Only** (usually don't modify):
- [CloudSeed/DelayLine.h](CloudSeed/DelayLine.h) - Delay buffer implementation
- [CloudSeed/AudioLib/](CloudSeed/AudioLib/) - Audio utilities
- Submodules (libdaisy, DaisySP; Terrarium is checked out but unused)

### Understanding Audio Flow

**CloudSeed Audio Callback** (`audioCallback()` at `src/cloudseed.cpp:461-517` - runs once per
48-sample block, i.e. at 1 kHz):
1. Process analog/digital controls and push both LEDs with `UpdateLeds()` (`:464-466`, two `Led::Update()` calls). Once
   audio runs this is the only caller of `Update()`: it is a read-modify-write that would race
   with the main loop
2. `processFootswitches()` (`:327-352`): one `gFootswitches.Update()` call with both switches'
   `Pressed()` / `FallingEdge()` returns `FootswitchEvents`. `toggleBypass` flips
   `state.bypass` and sets LED1 (the main loop persists it later), `cyclePreset` sets
   `triggerPresetChange`, `savePreset` sets `gSaveRequested`, `restorePreset` sets
   `triggerPresetRestore`
3. Read `state.bypass` once. While `state.presetChangeInProgress` **or** `state.uploadActive`
   is set - the main loop is rewriting `state.knobMap` / `state.toggleMap` and every engine
   parameter, or a USB upload session owns the reverb (`serviceUsbPresetLink()`, below) - the
   callback copies the input to both outputs and returns (`:472-476`): no knob/toggle scan, no
   reverb, `reverseMix` does not advance
4. Read all six knobs with `hw.knob[i].Value()` (`:478-480`) and the board's `kBoardToggles`
   toggles with `hw.TogglePosition(i)` (`:481-484`; slots past the board's count stay Down) and call
   `updateEngineControls(gFootswitches.PresetHeld() ? 1 : 0, knobPositions, togglePositions)`
   (`:486-487`, body `:359-400`), which holds every reverb write:
   - read every knob's current target value for the active bank with `knobTargetValue()`, then
     one `gKnobs.Scan(bank, reset, …)` call (`:363-368`) re-parks on any bank or preset
     transition (zero writes that block) and otherwise returns the knobs to write: glide steps
     for a knob taking over, then the knob's own position; `reset` is
     `state.controlResetPending`, read once and shared with the toggle scan (`:366`)
   - dispatch each returned write through `applyKnobTarget()` using `state.knobMap[bank][i]` —
     the cached copy, never `gPresets` (`:369-370`)
   - one `gToggles.Scan(bank, reset, togglePositions, …)` call (`:374-376`) re-parks the same
     way (`toggle_bank.h`), then returns the levers to write: a moved lever writes its
     position (Down / Middle / Up) immediately, no glide; dispatch each through `applyToggleTarget()`
     using `state.toggleMap[bank][i]` (`:378-380`)
   - any knob or toggle write while in bank 1 calls `gFootswitches.MarkEdited()`, cancelling
     that hold's preset change (`:387-388`)
   - select the delay line count from `state.delayLines` (`DelayLines_One` = 1, `_Default` =
     `state.defaultDelayLines`, `_Max` = `state.maxDelayLines`; set by the delay-line toggle
     target, applied above if it just moved) and write `Parameter::LineCount` when it differs from
     `state.prevNumDelayLines` (`:390-399`). All candidates are exact, so the `!=` test
     is exact; a preset with different default/max counts is re-applied on the first callback
     after the change
   Then, if `gSaveRequested` is set and the previous snapshot has been consumed, copy
   `GetAllParameters()`, `state.reverseDelayNorm`, `0.5 * state.delayLines` and the two reverse
   pseudo-values into `gSaveSnapshot` and publish it with `state.saveSnapshotReady` (`:491-501`). This runs only
   past the preset-change gate, so a half-loaded preset is never captured
5. `refreshOutputLevels()` (`:502`, body `:404-419`): `makeupGain` and `scaledDryOut` are
   **not** recomputed per block: they depend only on `DryOut`/`EarlyOut`/`MainOut`, so they are
   derived into `state.makeupGain` / `state.scaledDryOut` only when `state.outputLevelsDirty` is
   set, and the output stage just reads them. The flag is set by `applyKnobTarget()` on a write
   to any of those three parameters and by `loadPreset()`. The dry/wet mix itself happens inside
   the reverb; the callback only scales the result by
   `makeupGain = OUTPUT_VOLUME_BOOST * (1 + sinf(wetBalance * HALF_PI) * MAKEUP_GAIN_STRENGTH)`,
   where `wetBalance = (earlyOut + mainOut) / (dryOut + earlyOut + mainOut + FLOAT_EPSILON)` and
   `HALF_PI` is a `float` constant, so no double-precision math is emitted. Those three levels
   are read from `reverb->GetAllParameters()`, not from knob positions - with
   `[preset.knob_map]` a knob need not be mapped to any of them
6. `state.reverseEnabled` (the `"reverse.enabled"` toggle target) becomes `reverseTarget` (0 or
   1), and the left input channel is copied into `gInputBuffer` (`:506-507`)
7. `state.reverseDirectMix` (the `"reverse.direct_mix"` toggle target) picks the render helper
   (`:509-512`), each of which writes `out[0]` scaled by `makeupGain`:
   - Into-reverb, `reverseDirectMix` off, `renderReverseIntoReverb()` (`:426-443`): reverse the
     dry input into `gReverseBuffer`, scale it to the injected reverse, add that to the reverb
     input (`gReverbInputBuffer`), run `reverb->Process(gReverbInputBuffer, gWetBuffer, …)`,
     then `out = (wet − scaledDryOut * injectedReverse) * makeupGain`; subtracting
     `scaledDryOut * injectedReverse` cancels the reverb's dry pass-through of the injected
     reverse, so the forward dry stays clean and the reverse is heard only through the wet tail
   - Direct-mix, `reverseDirectMix` on, `renderReverseDirect()` (`:447-458`):
     `reverb->Process(gInputBuffer, gWetBuffer, …)`, record the reverb output into
     `reverseDelay` for backward playback, then
     `out = (wet + reversed * REVERSE_LEVEL * mix) * makeupGain`, the reverse audible on its own
   Both ramp the reverse via a smoothed mix toward `reverseTarget`, kept in a local during the
   sample loop and stored back to `state.reverseMix` afterwards, so the `"reverse.enabled"`
   toggle switches click-free (engine in `CloudSeed/ReverseDelay.h`)
8. If bypassed, overwrite `out[0]` with the input (`src/cloudseed.cpp:513-514`)
9. Mono: copy `out[0]` to `out[1]` (`src/cloudseed.cpp:516`), so both output jacks carry the
   same signal in every path

When `state.bypass` is set, output is a straight copy of the input — but the reverb (and the
reverse mix) is still processed, deliberately, to suppress an audible 1 kHz whine (the
`reverb->Process()` call inside whichever render helper runs, `src/cloudseed.cpp:426-458`). When
`state.presetChangeInProgress` or `state.uploadActive` is set, the reverb is skipped entirely and
the input is passed through to both outputs (`src/cloudseed.cpp:472-476`).

**CloudSeed Main Loop** (`main()` while loop at `src/cloudseed.cpp:670-709` - free-running, no sleep):
1. **Save / restore** (`:673-687`): a published `gSaveSnapshot` is copied into the current
   preset's `UserPreset` slot (`gStorage.UserSlot()`) and written with `gStorage.SaveUserPresets()`; a restore clears the
   slot, reloads the preset with `loadPresetGated()`, and saves. Both start the confirmation
   blink. See "User preset save / factory restore"
2. **Handle preset changes** (`:693-697`): clear `triggerPresetChange`, then
   `loadPresetGated(next)` (`:209-219`) sets `presetChangeInProgress` and
   `state.controlResetPending` (so the knob and toggle positions are re-snapshotted before the
   new preset loads), and between two `std::atomic_signal_fence(std::memory_order_seq_cst)`
   compiler barriers updates `state.currentPreset` and runs `loadPreset()`, then clears the flag
   to re-enable audio processing; `StartPresetBlink(state.blinkPattern)` follows.
   `loadPreset()` is synchronous, so there is no delay: the gate reopens as soon as it returns.
   The seven flags shared with the callback (`bypass`, `triggerPresetChange`,
   `triggerPresetRestore`, `saveSnapshotReady`, `presetChangeInProgress`, `uploadActive`,
   `controlResetPending`) are `volatile bool` (`:66-72`)
3. **USB preset upload/read/revert/DFU**: `serviceUsbPresetLink()` (`:700`, body `:541-587`) answers
   one received SysEx frame per pass (`UsbMidiLink` / `PresetProtocol`, see "USB-MIDI preset
   upload" above), performs the blocking QSPI write on COMMIT or the erase on REVERT, and keeps
   `state.uploadActive` in step with the session so the callback's passthrough gate opens and
   closes with it. A successful COMMIT or REVERT - and a COMMIT that fails after already
   erasing the bank the pedal is running - reboots via `NVIC_SystemReset()`; DFU resets into the
   bootloader via `System::ResetToBootloader()`
4. **Persist settings**: `gStorage.ServiceSettingsSave()` (`:702`) - the coalesced flash write described
   under Preset Persistence
5. **Update LEDs**: `ServiceConfirmBlink()`, else `ServicePresetBlink()` (`:705-706`)
6. `keepCoreBusy()` (`:708`, defined at `:530-536`) — deliberate busy work that keeps the core
   out of idle between callbacks and reduces an audible 1 kHz whine: it advances a `volatile`
   phase by 0.001 (wrapped at `TWO_PI`) and stores `sinf(phase)` to a `volatile` sink, so every
   pass performs a real FPU `sinf()` plus real loads and stores (a constant argument would be
   folded away at compile time). There is no loop delay; the loop spins

**Performance Architecture**:
- **Audio callback**: Time-critical, optimized for low latency
  - Control scanning (knobs, toggles, footswitch gestures), engine parameter writes, and audio
    processing
  - Sets trigger flags for heavy operations
  - No flash writes or preset loading
  - **Allocation-free**: every knob or toggle write goes through `ReverbChannel::SetParameter`
    inside the audio interrupt, so no parameter update may touch the heap or the SDRAM pool;
    `make test` enforces this (`tests/engine_alloc_test.cpp`). The seed-derived values every
    stage draws its delays, gains and modulation from live in fixed-size
    `AudioLib::SeedSeries<N>` members (`CloudSeed/AudioLib/ShaRandom.h:15-57`): SHA-256 runs only
    when a seed actually changes (`TapSeed`, `DiffusionSeed`, `DelaySeed`, `PostDiffusionSeed`),
    a `CrossSeed` write only re-blends the two cached series, and `UpdateLines()` and
    `MultitapDiffuser::Update()` read the cached values into fixed arrays. The per-write cost
    of the default knob targets (KNOB_5 `TapDecay`, KNOB_6 `LineDecay`, secondary KNOB_4/5
    `LineModAmount`/`LineModRate`) is therefore bounded arithmetic - see
    [docs/PERFORMANCE.md](docs/PERFORMANCE.md) §9
  - The USB-MIDI receive path shares this: the ISR (priority 0, same as the audio DMA) does
    nothing but `SysExAssembler::Feed()`; everything else about an upload runs in the main loop
- **Main loop**: Non-critical background tasks
  - Preset switching (includes buffer clearing)
  - Flash memory writes (settings, user preset save/restore)
  - USB preset link service (`serviceUsbPresetLink()`): services one SysEx frame per pass, and
    performs the COMMIT validation parse and QSPI write (blocking; hosts allow up to 10 s for
    the reply) while the callback's passthrough gate is held closed, and the REVERT one-sector
    erase (REVERT needs no session, so the gate is normally open during it)
  - LED blink state machine and save/restore confirmation blink
- **FPU flush-to-zero** is enabled once at the top of `main()` before `hw.Init()`
  (`src/cloudseed.cpp:590`) — see [docs/PERFORMANCE.md](docs/PERFORMANCE.md) §1

This separation prevents audio glitches during preset changes, flash writes, and USB uploads.


## Debugging

### Serial Debug Output

```cpp
// In src/cloudseed.cpp, add:
#include "daisy_seed.h"
using namespace daisy;

// In setup (true would block boot until a USB serial host opens the port):
hw.seed.StartLog(false);

// Anywhere:
hw.seed.PrintLine("Debug: value = %f", some_value);
```

`StartLog()` puts the same micro-USB port into USB CDC (serial) mode, and the USB-MIDI preset
link (`gMidiLink.Init()` in `main()`) needs it in MIDI mode. Remove the `gMidiLink.Init()`
call while logging; USB preset upload is unavailable in such a build.

### LED Indicators

```cpp
// The pedal LEDs are daisy::Led file statics gLed1/gLed2 in src/pedal_leds.cpp (:18-19,
// init LedsInit(kLed1Pin, kLed2Pin) :25-31, pins D22/D23 from src/board.h).
// For a debug value, add a setter next to SetBypassLed():
gLed2.Set(parameter_value);  // 0.0-1.0
```

`Set()` is enough once audio runs: the audio callback calls `Led::Update()` for both LEDs every
block (`src/cloudseed.cpp:466`), and nothing else may call it after `hw.StartAudio()` (it is a
read-modify-write that races with the callback). Only before `StartAudio()` must you call
`UpdateLeds()` yourself.

Caveat: `ServicePresetBlink()` (`src/pedal_leds.cpp:67-113`) runs on every main-loop pass and sets
LED2 at each blink transition (and turns it off while bypassed), so it overwrites debug values
unless that call is removed. LED1 is likewise re-set on every
bypass toggle (`src/cloudseed.cpp:337`), and both LEDs are driven by the save/restore confirmation
blink (`ServiceConfirmBlink()`, `src/pedal_leds.cpp:124-139`).

### Common Issues

**Build Errors**:
- Ensure submodules are initialized: `git submodule update --init --recursive`
- Rebuild libraries: `make libs` (runs `clean all` in CloudSeed, DaisySP, and libdaisy)
- Clean build: `make clean && make` (`make BOARD=hothouse clean` cleans `build/hothouse/`)
- Expected warnings on a clean `make`, none from `src/` or `CloudSeed/`: `array subscript has
  type 'char'` from the vendored `third_party/tomlc99/toml.c` (`isdigit()` on a `char`;
  unreachable, because `ParsePresetBank()` rejects every byte >= 0x80 before tomlc99 runs),
  `FP registers might be clobbered despite 'interrupt' attribute`
  from libdaisy's `Default_Handler` (`libdaisy/core/startup_stm32h750xx.c:1560`, an infinite loop
  that never returns), newlib's `_close`/`_read`/... `is not implemented and will always fail`,
  and `LOAD segment with RWX permissions` at link time

**Audio Issues**:
- Check buffer size (48 samples typical)
- Verify SDRAM allocation for CloudSeed
- Check parameter ranges (0.0-1.0 typical)

**Control Issues**:
- Verify the knob/switch pins in `src/board.h` for the board you built (`make` vs `make BOARD=hothouse`)
- Check the knob smoothing coefficient (`KNOB_SMOOTHING_COEFF`, `src/cloudseed.cpp:48`) and the
  takeover constants in [src/knob_bank.h](src/knob_bank.h): a knob brushed by accident taking over its
  target means `kKnobMoveThreshold` is too low (raise it to 0.02); a parameter that is re-written
  while nobody touches a live knob means `kKnobApplyEpsilon` is below the ADC noise; a stop that
  does not reach exact silence / full level means `kKnobRailWindow` is narrower than the pot's
  bleed or top gap. Measure the ADC before changing any of them
- Confirm the knob is mapped where you expect: `./build/terrarium/preset_check --print-knob-map presets.toml`
- Test with direct reads: `hw.knob[x].Value()`

## Performance Considerations

### CPU Usage

**CloudSeed**: Heavy processing
- 5 delay lines with modulation
- Multiple diffusion stages
- Extensive filtering
- Scatter (when on): a per-block LFO per early tap plus a per-sample gain ramp in
  `MultitapDiffuser::ProcessScatter()`; the static tap path runs otherwise. Unmeasured on hardware
  (see docs/HARDWARE_TESTS.md H15 for the "Through the Looking Glass" check)
- Estimated at ~70-80% CPU. **This is an unmeasured estimate** - no profiling artifact exists
  in the repo, and it predates the denormal-stall (FPU flush-to-zero) and `std::map`
  parameter-lookup fixes documented in [docs/PERFORMANCE.md](docs/PERFORMANCE.md), so real headroom is
  better than this figure suggests


### Memory Usage

**CloudSeed** (from the linker's `--print-memory-usage` report, `make libs && make` /
`make BOARD=hothouse` on this tree):

| Region | Terrarium | HotHouse |
|---|---|---|
| SRAM (`.text`+`.data`, 480KB `BOOT_SRAM`) | 229,296 B (46.65%) | 229,592 B |
| DTCMRAM (128KB) | 44,116 B (33.66%) | 44,188 B |
| RAM_D2_DMA (32KB) | 17,692 B (53.99%) | 17,692 B |
| SDRAM (64MB) | 53,138,368 B (79.18%) | 53,138,368 B |

- SDRAM is entirely static `DSY_SDRAM_BSS`: `custom_pool`
  50,331,648 B + `reverseDelayBuffer` 768,000 B (192,000 floats = 4 s @ 48 kHz, sized so the
  2000 ms max reverse window clears `ReverseDelay`'s `size / 2` clamp) +
  `CloudSeed::FastSin::data` 131,072 B + `AudioLib::ValueTables` tables 1,280,032 B (see the
  `.sdram_bss` section in `build/<board>/cloudseed.map`) + the 512 KB `toml_arena` (not part of
  `custom_pool`; `src/sdram_pool.cpp`, used at boot and to validate USB uploads) + the 96 KiB
  `gUploadText` upload receive buffer + the 5,000 B `gUploadCheck` validation `PresetBank`
  (`src/cloudseed.cpp`)
- The runtime heap is not in this report: it grows from `end` in RAM_D2
  (`libdaisy/core/STM32H750IB_sram.lds:239-250`), which is where `DelayLine`'s `tempBuffer`,
  `mixedBuffer`, and `filterOutputBuffer` (`CloudSeed/DelayLine.h:52-54`) land
- SRAM: of that, the embedded bank blob is 50,376 B (Terrarium, `build/terrarium/presets_toml.o`)
  / 50,712 B (HotHouse) - it carries the per-preset `[preset.knob_map]`, `[preset.toggle_map]`,
  `[preset.params.reverse]` and `[preset.params.delay_lines]` tables - tomlc99 is 14,371 B, and
  `preset_bank.o` is 9,178 B. The two boards differ only in the blob and the toggle code
- DTCMRAM: most of it beyond the app's own state is libdaisy's USB device stack, pulled in by
  `UsbMidiLink`: the four 2 KB `UserRxBufferFS`/`UserTxBufferFS`/
  `UserRxBufferHS`/`UserTxBufferHS` ring buffers, `midi_usb_handle` (2,112 B),
  `hpcd_USB_OTG_FS` / `hpcd_USB_OTG_HS` (1,292 B each), the two 732 B `hUsbDeviceFS`/`hUsbDeviceHS`
  descriptors and `USBD_StrDesc` (512 B), plus
  `gMidiLink` (616 B) and `gPresetProtocol` (316 B). Also includes the 5,000 B `gPresets` bank
  (40 B of each 312 B preset slot is the knob + toggle maps: 24 B knobs, 16 B toggles), the
  7,116 B `gStorage` (most of it the user-preset `PersistentStorage`, which keeps a
  defaults copy and a live copy of the 3,524 B `UserPresets`), and the 220 B `gSaveSnapshot`.
  Both regions are smaller than before the HotHouse port (SRAM 234,096 B, DTCMRAM 45,436 B with
  `DaisyPetal`), despite the larger parameter set
- RAM_D2_DMA includes the `MidiUsbTransport` rx/tx buffers
- QSPI: `Settings` in sector 0 (offset 0), `UserPresets` in sector 1 (offset 0x1000), and the
  uploaded-bank `StoredBankHeader` (offset 0x10000) + text (offset 0x11000), all inside the
  256 KB below the bootloader's program area at 0x90040000
- Boot time: about +10 ms for `MidiUsbTransport::Impl::Init()` (`System::Delay(10)`,
  `libdaisy/src/hid/usb_midi.cpp:125`); the new SDRAM buffers cost nothing at boot, since
  `.sdram_bss` is `NOLOAD` and never zeroed

### Optimization Tips

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for the concrete list of performance/correctness
fixes applied to the CloudSeed DSP (FPU denormal handling, parameter storage, the cached
per-line gain, hot-loop modulo/precision fixes, placement-new/delete destructor safety, build flags, the redundant
bypass copy, allocation-free parameter updates, engine state defined before its first
read, and the idle-free USB-MIDI preset link), each with the exact file/line and pattern it addresses.

## Further Resources

### Documentation
- Daisy Wiki: https://github.com/electro-smith/DaisyWiki/wiki
- libdaisy API: https://electro-smith.github.io/libDaisy/
- DaisySP API: https://electro-smith.github.io/DaisySP/

### Source Projects
- CloudSeed VST: https://github.com/ValdemarOrn/CloudSeed
- GuitarML fork: https://github.com/GuitarML/DaisyCloudSeed

### Hardware
- Daisy Seed: https://www.electro-smith.com/daisy/daisy
- PedalPCB Terrarium: https://www.pedalpcb.com/product/pcb351/
- Cleveland Music Co. HotHouse (second supported board, `make BOARD=hothouse`)

## Project-Specific Notes

### Mono vs Stereo

This fork differs from the original CloudSeed and its predecessors (`CloudSeed/DelayLineCount.h`):
- **Original CloudSeed plugin**: 8 (or 12) delay lines, stereo
- **DaisyCloudSeed (Daisy Patch)**: 2 delay lines, stereo
- **GuitarML Terrarium fork**: 4 delay lines, mono
- **This fork**: 5 delay lines, mono (`constexpr int TotalLineCount = 5;`)

The trade-off: More delay lines in mono = richer reverb tail.

### GuitarML Modifications

Key changes in this fork:
1. Adapted for Terrarium hardware (mono, 6 knobs, 4 switches); this fork also builds for the
   HotHouse (3 three-position toggles, `src/board.h`)
2. Increased delay line count from 2 (Daisy Patch) to 4 (GuitarML) and then 5 (this fork)
3. Added preset cycling via footswitch
4. Simplified control scheme for guitar pedal use
5. Added delay line switching via toggle switches
6. **Bypass state persisted to flash** alongside the preset (`SETTINGS_VERSION = 2`, `src/pedal_storage.cpp:5`; `Settings` at `src/pedal_storage.h:15-29`)
7. **Persistent preset storage** in QSPI flash memory with version control
8. **LED2 blink pattern system** for visual preset indication
9. **TOML-defined presets**: all preset data lives in [presets.toml](presets.toml) (and [presets_hothouse.toml](presets_hothouse.toml)), embedded in
   the firmware image with `.incbin` and parsed at boot by a vendored tomlc99 into a static
   `PresetBank`; `make` (and `make presets-check`, without building) rejects a file the
   parser would fail
10. **Performance optimization**: Moved preset switching and flash writes from audio callback to main loop;
    flash writes are coalesced 3 s after the last change (`PedalStorage::ServiceSettingsSave()`)
11. **Modulo-based preset cycling** for cleaner wraparound logic
12. **Through the Looking Glass Preset** enabled by adopting a `max_delay_lines` value for each preset
13. **Equal-power makeup gain** on the wet path, so perceived loudness holds as the dry/wet
    balance changes (`refreshOutputLevels()`, `src/cloudseed.cpp:404-419`, applied by the render
    helpers at `:426-458`), derived from `reverb->GetAllParameters()` only
    when `DryOut`/`EarlyOut`/`MainOut` change (`state.outputLevelsDirty`), not per block
14. **1 kHz whine mitigation**: the reverb is still processed while bypassed
    (`src/cloudseed.cpp:504-514`) and every main-loop pass calls `keepCoreBusy()`
    (`src/cloudseed.cpp:530-536`, `:708`): a real FPU `sinf()` of a `volatile` phase plus `volatile`
    loads and stores, so the core never idles between callbacks
15. **FPU flush-to-zero enabled at boot** to eliminate denormal stalls (`src/cloudseed.cpp:590`)
16. **`BOOT_SRAM` app type** (`Makefile:6`): the app is loaded into SRAM from QSPI flash by
    the Daisy bootloader, leaving room for all ten presets
17. **Per-preset knob mapping** (`[preset.knob_map]` in presets.toml): every knob has a primary
    and a secondary target, the secondary bank selected by holding the preset footswitch. Targets
    are `"group.Parameter"` strings plus the pseudo-target `"reverse.delay"` (the reverse window
    length, formerly a SWITCH_3 overload of KNOB_4; its per-preset start value is
    `[preset.params.reverse] delay`). Knobs are **absolute but parked**
    ([src/knob_bank.h](src/knob_bank.h)): after power-up, a preset load, or a bank change a knob writes
    nothing until it is turned, then glides its target to the pot's position over 50 ms and
    tracks it 1:1; the stops land exactly on 0.0 / 1.0
18. **Footswitch gestures** (`processFootswitches()`, `src/cloudseed.cpp:327-352`): tapping
    FOOTSWITCH_2 cycles the preset on release; holding it selects the secondary knob and toggle
    bank, and its release cycles only if no secondary knob or toggle wrote during the hold.
    FOOTSWITCH_1 toggles bypass on release. Both are
    tracked by `FootswitchGestures` ([src/footswitch_gestures.h](src/footswitch_gestures.h)), which holds
    each switch from `Pressed()` until `FallingEdge()` so a bounce cannot drop it, and treats any
    overlap of the two as a chord whose releases do nothing
19. **Active preset configuration cached in `PedalState`** (`loadPreset()`,
    `src/cloudseed.cpp:173-205`): `knobMap`, `toggleMap`, `defaultDelayLines`, `maxDelayLines` and
    the blink timings are copied out of `gPresets` on load, so the audio callback and the blink
    state machine never touch the parsed bank. While `state.presetChangeInProgress` is set the
    callback passes audio through and makes no engine write, so a load cannot be read
    half-applied
20. **Per-preset user save / factory restore**: FOOTSWITCH_1 held 5 s stores the engine state
    (Bloom, Scatter and the toggle pseudo-values included; the delay-line mode is stored as
    0.0 / 0.5 / 1.0 = One / Default / Max, the line counts always come from the bank) and the reverse window into the
    current preset's slot of a QSPI
    `UserPresets` store (offset 0x1000); FOOTSWITCH_1 + FOOTSWITCH_2 held 5 s clears the slot
    and reloads the active bank's values. Both are confirmed by LED1 + LED2 blinking 3 × 80 ms.
    The store is stamped with `identity` (the firmware image hash mixed with the active bank's
    hash) and discarded when it differs, so saved edits never outlive the firmware or preset
    bank that wrote them (see "User preset save / factory
    restore")
21. **Per-preset toggle mapping** (`[preset.toggle_map]` in presets.toml): every switch has a
    primary and a secondary target, the secondary bank shared with the knobs (held preset
    footswitch). Targets are `"group.Parameter"` strings naming one of thirteen on/off parameters,
    or one of the pseudo-targets `"delay_lines.max"`, `"reverse.enabled"`, `"reverse.direct_mix"`
    (plus the HotHouse three-way `"delay_lines.select"`, `"early.envelope"`, `"reverse.mode"`).
    Toggles are **parked** the same way knobs are ([src/toggle_bank.h](src/toggle_bank.h)): after
    power-up, a preset load, or a bank change a lever writes nothing until it is moved, then
    writes its position with no glide. Each preset also
    gets a required `default_delay_lines` next to `max_delay_lines`: the `"delay_lines.max"`
    toggle target switches the line count between them (off = default, on = max), replacing the
    old hard-coded 2-line default
22. **USB-MIDI preset upload**: the Seed's micro-USB port is a class-compliant USB-MIDI device
    at every boot (`gMidiLink.Init()`, `src/cloudseed.cpp:666`); a host can upload a complete
    `presets.toml` text, read the active bank back, or revert to the built-in bank over SysEx
    protocol v1 ([src/preset_protocol.h](src/preset_protocol.h),
    [docs/USB_MIDI.md](docs/USB_MIDI.md)). An upload is validated with the firmware's own parser
    before anything reaches flash, is stored in QSPI (`PedalStorage::WriteStoredBank()`), and a
    successful commit or revert reboots the pedal. Uploading, reverting, or reflashing all wipe
    saved user presets, because each changes one of the two hashes (firmware image, active
    bank text) that make up the user-preset `identity` (see "Firmware + bank identity" under "User preset save / factory restore")
23. **Browser preset editor** ([editor/](editor/)): a Preact Web MIDI app on Bun that edits every
    preset field, shows the signal flow, and uploads, reads back and reverts banks over
    protocol v1 (see "USB-MIDI preset upload")
24. **Two boards**: `make` builds for the Terrarium, `make BOARD=hothouse` for the HotHouse
    (`src/board.h` `PedalBoard` on `DaisySeed` replaces `DaisyPetal`); each embeds its own bank
    and rejects the other board's (`board` root key; the Identity Reply member byte names the
    board). Both output jacks carry the mono signal. The HotHouse three-position toggles drive
    the three-way targets, and the new Scatter early-tap animation (`Scatter`, `ScatterRate`,
    `ScatterDepth`) is available on both

### Version Information

Check git history for recent changes:
```bash
git log --oneline -10
```

Run `git branch --show-current` for the current branch and the command above for recent
changes; do not restate them here.

## Quick Reference Card

### Build & Flash
```bash
make clean         # Clean previous build (build/terrarium/; BOARD=hothouse for build/hothouse/)
make libs          # Rebuild libdaisy, DaisySP, and libcloudseed (clean all)
make presets-check # Validate presets.toml and presets_hothouse.toml without building (the board's bank is also checked by `make`)
make test          # Host unit tests (knob/toggle/footswitch state machines, preset parser for both boards, engine allocations, USB preset protocol, stored-bank check)
make               # Build CloudSeed for the Terrarium: build/terrarium/cloudseed.bin
make BOARD=hothouse  # Build for the HotHouse: build/hothouse/cloudseed.bin
make program-boot  # One time: flash the Daisy bootloader (BOOT_SRAM prerequisite)
make program-dfu   # Flash the app (reset, hold BOOT until rapid blink, then run; BOARD=hothouse for the HotHouse)
make editor        # Browser preset editor: make -C editor serve (bun install + dev server, http://localhost:5174); needs Bun
make editor-html   # Browser preset editor as one stand-alone file: build/editor.html (opens from file://)
make -C editor test check   # the editor's tests, tsc and Biome (not part of make test)
```

### File Locations
- Control mapping: `[preset.knob_map]` / `[preset.toggle_map]` in `presets.toml` /
  `presets_hothouse.toml`; dispatch at
  `src/cloudseed.cpp:369-370` (knobs) / `:378-380` (toggles), `applyKnobTarget()`
  `src/cloudseed.cpp:222-255`, `applyToggleTarget()` `src/cloudseed.cpp:261-288`
- Preset data: `presets.toml` (Terrarium) / `presets_hothouse.toml` (HotHouse), embedded via
  `src/presets_toml.s`, parsed by `src/preset_bank.cpp`
- Preset application: `CloudSeed/ReverbController.h:47` (`LoadPreset`)
- Parameters: `CloudSeed/Parameter.h`; names table: `CloudSeed/ParameterNames.h`
- Hardware config: `src/board.h` (`PedalBoard`, pins per board)
- USB-MIDI preset upload: [docs/USB_MIDI.md](docs/USB_MIDI.md) (protocol, for host authors);
  normative definition [src/preset_protocol.h](src/preset_protocol.h); link layer
  [src/usb_midi_link.h](src/usb_midi_link.h)
- Hardware validation checklist: [docs/HARDWARE_TESTS.md](docs/HARDWARE_TESTS.md), driven by
  `tools/usb_preset_host.py`
- Browser preset editor: [editor/](editor/) (`bun run dev`; see "USB-MIDI preset upload")

### Key Concepts
- Buffer size: 48 samples
- Sample rate: 48kHz (typical)
- SDRAM pool: 48MB for the reverb (`custom_pool`), plus a separate 512 KB TOML parse arena used
  at boot and to validate USB uploads (peak measured 147,096 B for presets.toml, 143,992 B for
  presets_hothouse.toml, on x86-64)
- Delay lines: 5 (mono), toggled per preset between `default_delay_lines` and
  `max_delay_lines` (default toggle1: `delay_lines.max` on Terrarium; `delay_lines.select` on
  HotHouse, which adds a single line in the Down position)
- Presets: 10 per board bank, `gPresets.count` at runtime (max `kMaxPresets` = 16)
- Boards: Terrarium (default, 4 two-position switches) and HotHouse (`make BOARD=hothouse`, 3
  three-position toggles); mono, both outputs carry the signal; outputs in `build/<board>/`
- Persistent storage: preset index + bypass, QSPI flash, written 3 s after the last change,
  `SETTINGS_VERSION = 2` - preset order in presets.toml is frozen unless the version is bumped
- User presets: FS1 held 5 s saves the current sound into the current preset, FS1 + FS2 held
  5 s restores it to the active bank's values; stored in QSPI at offset 0x1000 and wiped
  whenever the firmware image or the active bank's text changes (a different firmware image, a USB upload, or a
  revert). The preset bank text itself is never modified
- App type: `BOOT_SRAM` (app runs from SRAM, loaded by the Daisy bootloader)
- LED2: Continuous blink pattern indicates preset number; both LEDs blinking together at 5 Hz
  with no audio is `FatalErrorLoop()`: the embedded TOML failed to parse, the SDRAM pool is
  exhausted, or the audio block size is not `AUDIO_BUFFER_SIZE` (48). Both LEDs blinking 3
  times quickly is the save/restore confirmation
- Knob banks: 2 per preset (`knobN_a` primary, `knobN_b` while the preset footswitch is held);
  knobs are absolute but parked - nothing moves until a knob is turned after a preset or bank
  change, then the target glides to the knob's position over 50 ms (no step) and tracks it; the
  stops land exactly on 0.0 / 1.0
- Toggle banks: 2 per preset (`toggleN_a` primary, `toggleN_b` while the preset footswitch is
  held); toggles are parked the same way but with no glide - a moved lever writes its position
  (Down / Middle / Up) immediately ([src/toggle_bank.h](src/toggle_bank.h)); on/off targets are on only Up
- USB-MIDI preset upload: the pedal is a class-compliant USB-MIDI device at every boot; a host
  can upload, read back, or revert the active preset bank over SysEx (docs/USB_MIDI.md), restart
  the pedal into DFU mode (`0x08`), and read its board and build id with the MIDI Identity Request. An
  uploaded bank (for the pedal's own board) lives in QSPI and overrides the built-in bank until it is reverted or a different
  firmware image is flashed; audio passes through dry for the duration of a session

### Quick Modifications
1. Control mapping → `[preset.knob_map]` / `[preset.toggle_map]` in `presets.toml` /
   `presets_hothouse.toml` (verify with
   `./build/terrarium/preset_check --print-knob-map presets.toml` /
   `./build/terrarium/preset_check --board hothouse --print-toggle-map presets_hothouse.toml`)
2. Knob feel → `KNOB_SMOOTHING_COEFF` (`src/cloudseed.cpp:48`), `kKnobMoveThreshold`,
   `kKnobApplyEpsilon`, `kKnobRailWindow`, `kKnobGlideBlocks` ([src/knob_bank.h](src/knob_bank.h))
3. Add/modify presets → `presets.toml` / `presets_hothouse.toml` (`make` validates the board's bank; `make presets-check` validates both)
4. Blink patterns → `blinks` / `led_on_ms` / `led_off_ms` / `led_pause_ms` in the bank
5. Switch logic → reassign in `[preset.toggle_map]` (no C++ needed; see "1. Changing Control
   Mappings"), or in code: `src/cloudseed.cpp:261-288` (`applyToggleTarget()`),
   `src/cloudseed.cpp:390-399` (delay line count from `state.delayLines`),
   `src/cloudseed.cpp:504-512` (`reverse.enabled` / `reverse.direct_mix` in the callback),
   `src/cloudseed.cpp:327-352` (footswitches), [src/board.h](src/board.h) (pins, toggle positions),
   [src/footswitch_gestures.h](src/footswitch_gestures.h) (taps, FS2 hold, 5 s save / restore gestures)
6. LED2 blink behavior → `src/pedal_leds.cpp:67-113` (`ServicePresetBlink`); save/restore
   confirmation → `src/pedal_leds.cpp:116-139` (`CONFIRM_BLINKS`, `CONFIRM_BLINK_MS` at `:6-7`)
7. Knob map schema/validation → `parseKnobMap()` / `parseKnobTarget()` in
   [src/preset_bank.cpp](src/preset_bank.cpp)
8. Toggle map schema/validation → `parseToggleMap()` / `parseToggleTarget()` in
   [src/preset_bank.cpp](src/preset_bank.cpp); shared with knobs via `splitTarget()` /
   `resolveParamTarget()`
9. User preset store → `UserPreset` / `UserPresets` (`src/pedal_storage.h:34-51`), `PedalStorage::Init()`
   (`src/pedal_storage.cpp:41-56`), save/restore handling in the main loop (`src/cloudseed.cpp:673-687`)
