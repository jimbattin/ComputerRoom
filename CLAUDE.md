# DaisyCloudSeed Project Documentation

## Project Overview

This project implements a single guitar effect DSP for the Electrosmith Daisy Seed board mounted
in a PedalPCB Terrarium guitar pedal enclosure, published as **Computer Room: Algorithmic Reverb
for Terrarium** (`README.md:1`; the editor is titled "Computer Room LARC"): **CloudSeed**, an
advanced algorithmic reverb with up to 5 delay lines, 10 presets, and extensive control.
CloudSeed is based on the open-source CloudSeed VST plugin by ValdemarOrn, modified for mono
processing on embedded hardware (see "Lineage").

Features:
- Early and late reverberation stages, extensive modulation and diffusion (see "Reverb Engine")
- 10 presets defined in [presets.toml](presets.toml), embedded in the image and parsed at boot (see "Presets")
- Per-preset knob and toggle maps, with a secondary bank while FOOTSWITCH_2 is held (see "Controls")
- Delay line count toggled per preset between `default_delay_lines` and `max_delay_lines` (default SWITCH_1)
- Preset and bypass state persisted to QSPI flash (see "Settings persistence")
- Per-preset user save (FS1 held 5 s) and factory restore (FS1 + FS2 held 5 s) (see "User preset save / factory restore")
- Banks uploaded, read back, and reverted over USB-MIDI SysEx from a browser (see "USB-MIDI preset upload", "Browser Editor")
- Equal-power makeup gain on the wet path (see "Audio callback")
- FPU flush-to-zero enabled at boot (see "Realtime rules")

## Directory Structure

```
DaisyCloudSeed/
├── CloudSeed/             # Core reverb algorithm library (builds libcloudseed.a)
│   ├── ReverbController.h # LoadPreset(), parameter scaling (see "Core classes")
│   ├── ReverbChannel.h    # Core reverb architecture: delay lines, diffusers, modulation
│   ├── DelayLine.h        # Delay buffer implementation (reference only, rarely modified)
│   ├── DelayLineCount.h   # TotalLineCount (5)
│   ├── Parameter.h        # The 47 reverb parameters; names table in ParameterNames.h
│   ├── ReverseDelay.h     # Reverse voice engine
│   ├── AudioLib/          # Audio utilities (Biquad, filters, ShaRandom) (reference only, rarely modified)
│   ├── Utils/             # SHA256 utilities
│   ├── build/             # Library build artifacts (generated, not checked in)
│   ├── license.txt        # CloudSeed library licence
│   └── Makefile           # Library build configuration
│
├── libdaisy/              # Hardware abstraction layer (Git submodule; reference only, rarely modified)
├── DaisySP/               # DSP library (Git submodule; reference only, rarely modified)
├── Terrarium/             # Hardware definitions (Git submodule; reference only, rarely modified)
│
├── build/                 # Application build artifacts (generated, not checked in)
├── Makefile               # App build (BOOT_SRAM; dependent libraries built with the `libs` target)
├── src/                   # Firmware sources
│   ├── cloudseed.cpp          # main(), pedal state, controls, audio callback, preset load
│   ├── pedal_leds.h/.cpp      # LED1/LED2: preset blink, save/restore confirmation, FatalErrorLoop()
│   ├── pedal_storage.h/.cpp   # QSPI Settings + UserPresets (PedalStorage) + uploaded bank I/O, firmware+bank hash
│   ├── stored_bank.h          # Uploaded-bank QSPI layout + ValidStoredBankText() boot check (host-portable)
│   ├── sdram_pool.h/.cpp      # custom_pool_allocate() SDRAM bump pool + dedicated TOML parse arena (boot + USB upload)
│   ├── preset_protocol.h/.cpp # USB-MIDI SysEx preset upload protocol v1 (host-portable; docs/USB_MIDI.md)
│   ├── usb_midi_link.h/.cpp   # UsbMidiLink: receive via daisy::MidiUsbTransport, replies via UsbHandle::TransmitInternal()
│   ├── knob_bank.h            # KnobBank absolute knob takeover (park, 50 ms glide, track; host-portable)
│   ├── toggle_bank.h          # ToggleBank parked toggle takeover (host-portable)
│   ├── footswitch_gestures.h  # FootswitchGestures: FS1/FS2 tap, hold, 5 s save and restore chords (host-portable)
│   ├── preset_bank.h          # PresetBank/PresetData types + knob/toggle-map types + ParsePresetBank()
│   ├── preset_bank.cpp        # TOML -> PresetBank parser (host-portable, no libdaisy)
│   └── presets_toml.s         # .incbin that embeds presets.toml into the firmware image
├── tests/                 # Firmware host unit tests (`make test`; the editor's are in editor/src)
│   ├── check.h                # Minimal CHECK() macro + summary
│   ├── knob_bank_test.cpp
│   ├── toggle_bank_test.cpp
│   ├── footswitch_gestures_test.cpp
│   ├── preset_bank_test.cpp
│   ├── engine_alloc_test.cpp  # CloudSeed engine: SetParameter()/Process() never allocate
│   ├── preset_protocol_test.cpp # SysEx assembler, USB-MIDI packer, protocol state machine, end-to-end via the parser
│   ├── stored_bank_test.cpp   # ValidStoredBankText(): which stored bank the pedal boots
│   └── fixtures/two_presets.toml  # Parser fixture (Chorus + Through the Looking Glass)
├── docs/
│   ├── HARDWARE_TESTS.md      # On-pedal validation checklist per revision (USB-MIDI link, audio regressions)
│   ├── PERFORMANCE.md         # Record of applied performance/correctness fixes (with file/line anchors)
│   ├── USB_MIDI.md            # USB-MIDI preset upload protocol, for host (browser) authors
│   ├── assign-knobs.png       # README secondary-knob illustration
│   ├── assign-switches.png    # README secondary-toggle illustration
│   ├── editor.png             # README editor screenshot
│   ├── signal-flow.png        # README signal-flow screenshot
│   └── pedal.png              # README pedal photo
├── presets.toml           # Built-in preset bank (10 presets); an uploaded bank in QSPI can override it
├── third_party/tomlc99/   # Vendored TOML parser (MIT, commit in README.txt)
├── tools/                 # preset_check.cpp (host-side presets.toml validator),
│                          # usb_preset_host.py (Linux USB-MIDI test host for docs/HARDWARE_TESTS.md)
├── editor/                # Browser preset editor (Preact + Bun); see "Browser Editor"
├── CLAUDE.md              # This file - agent-facing project documentation
├── README.md              # User-facing docs ("Computer Room"): features, editor, USB upload, save/restore, knob/toggle maps, control table, build/flash
├── LICENSE                # MIT license (Erwin Coumans' Daisy port, Valdemar Erlingsson's CloudSeed, Marco Paland's printf)
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

### Terrarium Pedal Controls

**Knobs** (6 potentiometers):
- KNOB_1 through KNOB_6 = ADC channels 0,2,4,1,3,5 (`Terrarium/terrarium.h:20-28`); these are
  indices into `hw.knob[]`, wired in `DaisyPetal::InitAnalogControls()`
  (`libdaisy/src/daisy_petal.cpp:313-335`)

**Switches** (4 toggles):
- SWITCH_1 through SWITCH_4 = 2, 1, 0, 6 (`Terrarium/terrarium.h:10-18`). These are indices
  into `hw.switches[]`, **not** pin numbers; `DaisyPetal::InitSwitches()` maps them to Daisy
  Seed pins D10, D9, D8, D7 (`libdaisy/src/daisy_petal.cpp:12-18`, `:279-292`)

**Footswitches** (2):
- FOOTSWITCH_1 = `hw.switches[4]` (seed pin D25) - Bypass/Active
- FOOTSWITCH_2 = `hw.switches[5]` (seed pin D26) - Preset cycling

**LEDs** (2):
- LED_1 = seed pin 22, used via `hw.seed.GetPin(Terrarium::LED_1)` - Active indicator (on when not bypassed)
- LED_2 = seed pin 23 - preset indicator (see "LEDs")

**Audio**:
- Mono input/output (uses left channel only)

## Firmware Architecture

### Audio callback

`audioCallback()` at `src/cloudseed.cpp:452-504` runs once per 48-sample block, i.e. at 1 kHz:
1. Process analog/digital controls and push both LEDs with `UpdateLeds()` (`:455-457`, two
   `Led::Update()` calls). Once audio runs this is the only caller of `Update()`: it is a
   read-modify-write that would race with the main loop
2. `processFootswitches()` (see "Footswitch gestures"): one `gFootswitches.Update()` call with
   both switches' `Pressed()` / `FallingEdge()` returns `FootswitchEvents`. `toggleBypass` flips
   `state.bypass` and sets LED1 (the main loop persists it later), `cyclePreset` sets
   `triggerPresetChange`, `savePreset` sets `gSaveRequested`, `restorePreset` sets
   `triggerPresetRestore`
3. Read `state.bypass` once. While `state.presetChangeInProgress` **or** `state.uploadActive`
   is set - the main loop is rewriting `state.knobMap` / `state.toggleMap` and every engine
   parameter, or a USB upload session owns the reverb (see "USB-MIDI preset upload") - the
   callback copies the input to the output and returns (`:463-466`): no knob/toggle scan, no
   reverb, `reverseMix` does not advance
4. Read all six knobs with `hw.knob[kKnobIndex[i]].Value()` (`:468-470`) and all four toggles
   with `hw.switches[kToggleIndex[i]].Pressed()` (`:471-473`) and call
   `updateEngineControls(gFootswitches.PresetHeld() ? 1 : 0, knobPositions, togglePositions)`
   (`:475-476`, body `:352-391`), which holds every reverb write:
   - read every knob's current target value for the active bank with `knobTargetValue()`, then
     one `gKnobs.Scan(bank, reset, …)` call (`:357-364`) re-parks on any bank or preset
     transition (zero writes that block) and otherwise returns the knobs to write: glide steps
     for a knob taking over, then the knob's own position; `reset` is
     `state.controlResetPending`, read once and shared with the toggle scan (`:361`)
   - dispatch each returned write through `applyKnobTarget()` using `state.knobMap[bank][i]` —
     the cached copy, never `gPresets` (`:365-366`)
   - one `gToggles.Scan(bank, reset, togglePositions, …)` call (`:370-371`) re-parks the same
     way, then returns the levers to write: a flipped lever writes its position (up = on)
     immediately, no glide; `state.controlResetPending` is cleared after both scans (`:372`);
     dispatch each write through `applyToggleTarget()` using `state.toggleMap[bank][i]`
     (`:373-374`)
   - any knob or toggle write while in bank 1 calls `gFootswitches.MarkEdited()`, cancelling
     that hold's preset change (`:379-380`)
   - select the delay line count as `state.delayLinesMax ? state.maxDelayLines :
     state.defaultDelayLines` (the `"delay_lines.max"` toggle target, applied above if it was
     just flipped) and write `Parameter::LineCount` when it differs from
     `state.prevNumDelayLines` (`:385-390`). Both candidates are exact copies, so the `!=` test
     is exact; a preset with different default/max counts is re-applied on the first callback
     after the change
   Then, if `gSaveRequested` is set, publish the save snapshot (see "User preset save / factory
   restore"). This runs only past the preset-change gate, so a half-loaded preset is never
   captured
5. `refreshOutputLevels()` (`:491`, body `:395-410`): `makeupGain` and `scaledDryOut` are
   **not** recomputed per block: they depend only on `DryOut`/`EarlyOut`/`MainOut`, so they are
   derived into `state.makeupGain` / `state.scaledDryOut` only when `state.outputLevelsDirty` is
   set, and the output stage just reads them. The flag is set by `applyKnobTarget()` on a write
   to any of those three parameters and by `loadPreset()`. The dry/wet mix itself happens inside
   the reverb; the callback only scales the result by the equal-power makeup gain
   `makeupGain = OUTPUT_VOLUME_BOOST * (1 + sinf(wetBalance * HALF_PI) * MAKEUP_GAIN_STRENGTH)`,
   where `wetBalance = (earlyOut + mainOut) / (dryOut + earlyOut + mainOut + FLOAT_EPSILON)` and
   `HALF_PI` is a `float` constant, so no double-precision math is emitted; perceived loudness
   holds as the dry/wet balance changes. `scaledDryOut` is
   `reverb->GetScaledParameter(::Parameter::DryOut)`. Those three levels
   are read from `reverb->GetAllParameters()`, not from knob positions - with
   `[preset.knob_map]` a knob need not be mapped to any of them
6. `state.reverseEnabled` (the `"reverse.enabled"` toggle target) becomes `reverseTarget` (0 or
   1), and the left input channel is copied into `gInputBuffer` (`:495-496`)
7. `state.reverseDirectMix` (the `"reverse.direct_mix"` toggle target) picks the render helper
   (`:498-501`), each of which writes the output scaled by `makeupGain`:
   - Into-reverb, `reverseDirectMix` off, `renderReverseIntoReverb()` (`:417-434`): reverse the
     dry input into `gReverseBuffer`, scale it to the injected reverse, add that to the reverb
     input (`gReverbInputBuffer`), run `reverb->Process(gReverbInputBuffer, gWetBuffer, …)`,
     then `out = (wet − scaledDryOut * injectedReverse) * makeupGain`; subtracting
     `scaledDryOut * injectedReverse` cancels the reverb's dry pass-through of the injected
     reverse, so the forward dry stays clean and the reverse is heard only through the wet tail
   - Direct-mix, `reverseDirectMix` on, `renderReverseDirect()` (`:438-449`):
     `reverb->Process(gInputBuffer, gWetBuffer, …)`, record the reverb output into
     `reverseDelay` for backward playback, then
     `out = (wet + reversed * REVERSE_LEVEL * mix) * makeupGain`, the reverse audible on its own
   Both ramp the reverse via a smoothed mix toward `reverseTarget`, kept in a local during the
   sample loop and stored back to `state.reverseMix` afterwards, so the `"reverse.enabled"`
   toggle switches click-free (engine in `CloudSeed/ReverseDelay.h`)
8. If bypassed, overwrite the output with the input (`:502-503`). The reverb (and the reverse
   mix) is still processed while bypassed, deliberately, to suppress an audible 1 kHz whine: the
   `reverb->Process()` call inside whichever render helper runs

### Main loop

The `main()` while loop at `src/cloudseed.cpp:651-690` is free-running, with no sleep:
1. **Save / restore**: a published save snapshot is stored and a restore request is handled,
   each followed by the confirmation blink (see "User preset save / factory restore")
2. **Handle preset changes** (`:674-678`): clear `triggerPresetChange`, then
   `loadPresetGated(next)` (`:216-226`) sets `presetChangeInProgress` and
   `state.controlResetPending` (so the knob and toggle positions are re-snapshotted before the
   new preset loads), and between two `std::atomic_signal_fence(std::memory_order_seq_cst)`
   compiler barriers updates `state.currentPreset` and runs `loadPreset()`, then clears the flag
   to re-enable audio processing; `StartPresetBlink(state.blinkPattern)` follows.
   `loadPreset()` is synchronous, so there is no delay: the gate reopens as soon as it returns.
   The seven flags shared with the callback (`bypass`, `triggerPresetChange`,
   `triggerPresetRestore`, `saveSnapshotReady`, `presetChangeInProgress`, `uploadActive`,
   `controlResetPending`) are `volatile bool` (`:68-74`)
3. **USB preset upload/read/revert**: `serviceUsbPresetLink()` answers one received SysEx frame
   per pass and keeps `state.uploadActive` in step with the session (see "USB-MIDI preset upload")
4. **Persist settings**: `gStorage.ServiceSettingsSave()` - the coalesced flash write (see
   "Settings persistence")
5. **Update LEDs**: `ServiceConfirmBlink()`, else `ServicePresetBlink()` (`:686-687`)
6. `keepCoreBusy()` (`:689`, defined at `:517-523`) — deliberate busy work that keeps the core
   out of idle between callbacks and reduces an audible 1 kHz whine: it advances a `volatile`
   phase by 0.001 (wrapped at `TWO_PI`) and stores `sinf(phase)` to a `volatile` sink, so every
   pass performs a real FPU `sinf()` plus real loads and stores (a constant argument would be
   folded away at compile time). There is no loop delay; the loop spins

### Realtime rules

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
  - The USB-MIDI receive ISR shares the audio DMA's priority and does nothing but
    `SysExAssembler::Feed()`; everything else about an upload runs in the main loop (see
    "USB-MIDI preset upload")
- **Main loop**: Non-critical background tasks
  - Preset switching (includes buffer clearing)
  - Flash memory writes (settings, user preset save/restore)
  - USB preset link service, including the blocking COMMIT validation parse and QSPI write
    (see "USB-MIDI preset upload")
  - LED blink state machine and save/restore confirmation blink
- **FPU flush-to-zero** is enabled once at the top of `main()` before `hw.Init()`
  (`src/cloudseed.cpp:573`) to eliminate denormal stalls — see
  [docs/PERFORMANCE.md](docs/PERFORMANCE.md) §1

This separation prevents audio glitches during preset changes, flash writes, and USB uploads.

## Controls

File: [src/cloudseed.cpp](src/cloudseed.cpp) + `[preset.knob_map]` / `[preset.toggle_map]` in
[presets.toml](presets.toml)

Knobs and toggle switches are **not** hard-wired. Each preset's `[preset.knob_map]` /
`[preset.toggle_map]` names a primary (`_a`) and a secondary (`_b`) target per knob/toggle; the
secondary bank is selected while **the preset footswitch (FOOTSWITCH_2) is held**.

### Default control map

The shipped default map in every preset reproduces the historical assignment:

```cpp
//                        primary (_a)                      secondary (_b)
KNOB_1: output.DryOut                        | input.PreDelay
KNOB_2: output.EarlyOut                      | input.HighPass   (+ HiPassEnabled on first touch)
KNOB_3: output.MainOut                       | input.LowPass    (+ LowPassEnabled on first touch)
KNOB_4: late_diffusion.LateDiffusionFeedback | late.LineModAmount
KNOB_5: early.TapDecay                       | late.LineModRate
KNOB_6: late.LineDecay                       | reverse.delay    (20-2000 ms reverse window)

// Primary (_a) = secondary (_b) in every shipped preset.
SWITCH_1: "delay_lines.max"    off = the preset's default_delay_lines, on = its max_delay_lines
SWITCH_2: "early.Bloom"        Bloom: reverses the early tap gain order
SWITCH_3: "reverse.enabled"    reverse voice on/off (window length set by whichever knob maps to
          "reverse.delay"; see CloudSeed/ReverseDelay.h)
SWITCH_4: "reverse.direct_mix" off = reverse feeds the reverb wet path, on = direct output mix

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

### Footswitch gestures

[src/footswitch_gestures.h](src/footswitch_gestures.h): libdaisy's `Switch` is
an 8-bit shift register that `Debounce()` shifts at most once per ms (`libdaisy/src/hid/switch.cpp:34`), i.e. once
per 1 ms audio block: `Pressed()` is `state_ == 0xff` and clears
1 ms after a release, while `FallingEdge()` is `state_ == 0x80` and only fires 7 ms later
(`libdaisy/src/hid/switch.h:68-74`, `libdaisy/src/hid/switch.cpp:39-41`). `FootswitchGestures` tracks both
switches with private `bypassHeld` / `presetHeld` flags, each set on `Pressed()` and cleared
only on that switch's `FallingEdge()`, so a contact bounce mid-hold (which never produces `0x80`)
cannot drop a hold, and FS2's 6 ms tail before the edge stays in bank 1 (`PresetHeld()`).
`Update()` is called once per callback with all four accessors and returns a
`FootswitchEvents` struct:
- `toggleBypass`: FS1's release edge, unless its 5 s save already fired or the press was part of
  a chord
- `cyclePreset`: FS2's release edge, unless a secondary knob or toggle wrote since the press
  (`MarkEdited()`) or the press was part of a chord
- `savePreset`: FS1 held alone for `kLongHoldBlocks` (5000 blocks = 5 s,
  `src/footswitch_gestures.h:24`), counted from its `Pressed()` latch; fires once per hold,
  while still held
- `restorePreset`: both held for `kLongHoldBlocks` consecutive blocks; fires once, while held

A **chord** starts on the first block both holds are set and lasts until both are released, so
presses and releases may be arbitrarily staggered: while it lasts neither release fires, and a
chord shorter than 5 s does nothing at all. An FS1 hold that turns into a chord stops counting
toward a save. A falling edge without a prior `Pressed()` is ignored. The callback calls
`MarkEdited()` when a bank-1 `Scan()` returns at least one knob or toggle write (step 4 of
"Audio callback"), i.e. a secondary knob moved past `kKnobMoveThreshold` and wrote, or a
secondary toggle was flipped; brushing a knob below the threshold does not cancel the preset
change, and entering bank 1 re-parks both (zero writes), so pressing FS2 is never itself an edit.
The bank is derived from `PresetHeld()` after `Update()` in the same callback,
so the block that ends a hold already scans in bank 0. `processFootswitches()`
(`src/cloudseed.cpp:320-345`) reads each accessor once per callback,
because edge flags are only valid for the update in which they occur, and turns the events into
`state.bypass`, `state.triggerPresetChange`, `gSaveRequested`, and `state.triggerPresetRestore`.

### Knob take-over

[src/knob_bank.h](src/knob_bank.h): knobs are **absolute** - once a knob has taken
over, its position *is* the parameter value - but **parked** across transitions.
`KnobBank` is a class whose only public member is `Scan()`; its private `Reset()` snapshots every knob position and parks it; a parked knob writes nothing, so
power-up, a preset load, and entering or leaving the secondary bank never change a parameter on
their own, and a freshly loaded preset sounds exactly as authored until a knob is turned. A knob
goes live once it moves `kKnobMoveThreshold` (0.01 of travel) from its snapshot. It then
**glides** its target linearly from the target's current value to the pot's (possibly still
moving) position over `kKnobGlideBlocks` (50, `src/knob_bank.h:26`) audio blocks = 50 ms,
landing exactly on the pot.
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

Leaving the secondary bank re-parks too: a knob dialled in bank 1 is parked in bank 0, and its
next turn glides its primary target to the knob's position - a smooth jump, by design. There is
deliberately no pickup/crossing rule (it reintroduces the dead-zone feel).

`KnobBank::Scan()` is the whole per-callback decision, called from `updateEngineControls()`: it
re-parks on a bank change (`bank != activeBank`), when `state.controlResetPending` is set (by
`loadPresetGated()` before the load, and by `serviceUsbPresetLink()` when a USB upload session
ends, `src/cloudseed.cpp:566`; `updateEngineControls()` clears it after both the knob and the
toggle scan), or on the first call; a re-parking call returns zero writes. It never runs while
`state.presetChangeInProgress` or `state.uploadActive` is set (the callback passes audio through
and returns first) because the re-park must not straddle a `state.knobMap` rewrite;
`controlResetPending` simply stays set until the load or the upload session completes.
The glide start values come from `knobTargetValue()` (`src/cloudseed.cpp:286-307`), read for the
active bank every callback: knob values are stored verbatim in `parameters[]` by `SetParameter`,
so `GetAllParameters()` read-back is exact; the `reverse.delay` pseudo-target has no
`parameters[]` slot and is tracked in `state.reverseDelayNorm` instead (set by `loadPreset()`
from the preset's `[preset.params.reverse] delay`, then by the knob). A disabled input filter
reports its open end instead of its stored cutoff (`HighPass` 0.0 while `HiPassEnabled` < 0.5,
`LowPass` 1.0 while `LowPassEnabled` < 0.5): the engine skips it, so that is what is heard, and
`applyKnobTarget()` (`src/cloudseed.cpp:229-262`) enables it on the first write, so the glide
opens from there rather than stepping to the stored cutoff (about 2 kHz in Chorus and Dark
Plate; Rubi Ka Fields also ships with the low-pass off, at about 14.5 kHz).

`main()` settles the knob one-poles for 200 ms between `hw.StartAdc()` and `hw.StartAudio()`
(`src/cloudseed.cpp:633-649`). `AnalogControl` starts at 0.0 and only converges while
`ProcessAnalogControls()` runs, which otherwise first happens inside the audio callback: without
the settle loop the first snapshot would capture ~5 % of each real knob position and the
settling ramp itself would be read as a deliberate turn at every power-up.

**ADC smoothing**: libdaisy's default `AnalogControl` slew computes to `coeff_ = 1.0` at this
callback rate (`libdaisy/src/hid/ctrl.cpp:16` with a 1 kHz update and the 0.002 s default),
i.e. no filtering. `main()` re-tunes every knob to `KNOB_SMOOTHING_COEFF` (0.05, ~20 ms,
`src/cloudseed.cpp:50`) at `src/cloudseed.cpp:617-618`. Nothing may call
`hw.SetAudioBlockSize()` / `hw.SetAudioSampleRate()` afterwards: both re-run
`SetHidUpdateRates()` and overwrite the coefficient.

**Tuning**: the knob response constants are `KNOB_SMOOTHING_COEFF` (the ADC one-pole), and in
[src/knob_bank.h](src/knob_bank.h) `kKnobMoveThreshold` (how far a parked knob must move to take over),
`kKnobApplyEpsilon` (the smallest change a live knob re-writes), `kKnobRailWindow` (how close to a
stop reads as exactly 0.0 / 1.0), and `kKnobGlideBlocks` (takeover glide length in 1 ms blocks;
100 if the takeover still clicks, 25 if it feels laggy).

### Toggle take-over

[src/toggle_bank.h](src/toggle_bank.h): the four switches are also assignable
per preset (`[preset.toggle_map]`) and take over the same way knobs park, but with no glide -
every toggle target is on/off. `ToggleBank::Scan()` snapshots and parks every lever on the first
call, on a bank change, or when `forceReset` is set, exactly like `KnobBank::Scan()`; a
re-parking call returns zero writes. Once primed, a lever that differs from its parked snapshot
writes its position (up = `true`) immediately (no move threshold, no epsilon: the target is
binary so there is nothing to debounce beyond the snapshot compare) and keeps writing on every
later change. `updateEngineControls()` calls `gToggles.Scan(bank, reset, togglePositions,
toggleWrites)` right after the knob scan, reusing the same `reset` flag
(`state.controlResetPending`) and `bank`, then dispatches each write
through `applyToggleTarget()` (`src/cloudseed.cpp:266-281`): the three pseudo targets set a
`PedalState` bool read elsewhere in the callback (`delay_lines.max` → `state.delayLinesMax`,
`reverse.enabled` → `state.reverseEnabled`, `reverse.direct_mix` → `state.reverseDirectMix`),
and `ToggleTarget_Param` calls `reverb->SetParameter()` directly (no
`outputLevelsDirty`: no toggle parameter feeds the makeup gain).
Toggle positions are `Switch::Pressed()` (`state_ == 0xff`), so a lever switched on is seen after
8 ms and one switched off after 1 ms; `ToggleBank` adds no debounce and compares against the
last position it saw (`last[]`, `src/toggle_bank.h:31-40`).

Every toggle is read with `hw.switches[kToggleIndex[i]].Pressed()` - an 'ON' toggle counts as
pressed - not `.Read()`; `kToggleIndex` (`src/cloudseed.cpp:115-116`) maps presets.toml order
(SWITCH_1..SWITCH_4) to `hw.switches[]`. The reverse window length is not a toggle target: it
starts at the preset's `[preset.params.reverse] delay` and is then set by whichever knob maps to
`"reverse.delay"`.

A toggle that targets `input.HiPassEnabled` or `input.LowPassEnabled` **owns** that filter's
enable: `loadPreset()` computes `state.hiPassOwnedByToggle` / `state.lowPassOwnedByToggle` from
`state.toggleMap` via `toggleMapTargets()` (`src/cloudseed.cpp:171-178`, `:205-206`), and while
true `applyKnobTarget()` does not auto-enable the filter on the knob's first touch and
`knobTargetValue()` glides the knob from the stored cutoff instead of the open end
(`src/cloudseed.cpp:243-252`, `:294-302`) - the toggle is the only thing that switches the
filter on or off.

## Presets

### Preset bank

All preset data lives in [presets.toml](presets.toml) at the repo root and is the **built-in**
preset bank: it is embedded into the firmware image by `src/presets_toml.s` (`.incbin`, lands in
`.rodata` → SRAM) and is never modified on the pedal. A bank uploaded over USB-MIDI
(docs/USB_MIDI.md) is stored in QSPI instead and, if it is present and parses, overrides
presets.toml at boot until it is reverted or a different firmware image is flashed - see
"USB-MIDI preset upload". Whichever bank is active is parsed once at boot into the static
`gPresets` bank (`src/cloudseed.cpp:59`); there is no filesystem, and the active bank's
text is never modified in place. Changing the built-in bank means editing presets.toml and
reflashing; a sound saved on the pedal (FS1 held 5 s) is stored separately in QSPI and overrides
its preset's engine values, within whichever bank is active, until it is restored (see "User
preset save / factory restore").

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

All presets allow 5 delay lines except "Through the Looking Glass"
(`max_delay_lines = 4.0`) - it is CPU-intensive and crackles above that.

- Parsed by `ParsePresetBank()` ([src/preset_bank.cpp](src/preset_bank.cpp)) into `PresetBank`
  (`count` + `PresetData[16]`); `PresetData::params[]` is indexed by `(int)Parameter`. File
  format: see "presets.toml schema and parser rules"
- Applied with `CloudSeed::ReverbController::LoadPreset()` (see "Core classes")
- Cycles using modulo operator: `(currentPreset + 1) % gPresets.count` (`src/cloudseed.cpp:676`);
  the count comes from `gPresets.count`, so adding a preset needs no code change
- The preset index is persisted, so the order in presets.toml is frozen (see "Settings persistence")
- `loadPreset()` (`src/cloudseed.cpp:182-212`) is the only reader of `gPresets` after boot. It loads
  the preset's saved user edit if its `UserPreset` slot is valid, else the factory values:
  `LoadPreset()` with the chosen `params`, `applyReverseWindow()` with the chosen reverse window,
  and `state.delayLinesMax` / `state.reverseEnabled` / `state.reverseDirectMix` from the chosen
  toggle pseudo-values. It then copies the preset's `knobMap`, `toggleMap`, `defaultDelayLines`,
  `maxDelayLines` and blink timings (never user-edited) into `PedalState`, derives
  `hiPassOwnedByToggle` / `lowPassOwnedByToggle` via `toggleMapTargets()`, and sets
  `state.outputLevelsDirty`. The audio callback and `ServicePresetBlink()` (handed
  `state.blinkPattern` by the main loop) read only those cached copies, so they never touch the
  parsed bank
- The delay-line count is applied in the audio callback (`updateEngineControls()`, step 4 of
  "Audio callback")
- A parse failure is unrecoverable: `FatalErrorLoop()` (see "LEDs"). `make` validates
  presets.toml before embedding it, so a rejected file cannot be built into firmware in the
  first place

### presets.toml schema and parser rules

Each `[[preset]]` carries `name`, `blinks`, `led_on_ms`, `led_off_ms`, `led_pause_ms`,
`default_delay_lines`, `max_delay_lines`, a `[preset.knob_map]` table, a `[preset.toggle_map]`
table, eight `[preset.params.*]` groups holding all 46 file-controlled parameters (see the
parameter reference in the TOML header; `Bloom` is a file parameter, in
`[preset.params.early]`), a ninth group `[preset.params.reverse]` (`delay`, `enabled`,
`direct_mix`), and a tenth `[preset.params.delay_lines]` (`max`). Neither of the last two groups
has a `Parameter` slot, so `parseParams()` (`src/preset_bank.cpp:394-438`) parses them
separately from `kGroups`. `parseKnobMap()`/`parseKnobTarget()` parse the knob map into
`PresetData::knobMap[bank][knob]`, `parseToggleMap()`/`parseToggleTarget()` the toggle map into
`PresetData::toggleMap[bank][toggle]`; `splitTarget()` / `resolveParamTarget()` / `groupIs()`
are shared by both (all in [src/preset_bank.cpp](src/preset_bank.cpp)).

Rules the parser enforces (a violation fails `make` before the blob is embedded; if one were
ever flashed it would stop boot in `FatalErrorLoop()`). An uploaded bank that breaks them is
rejected at COMMIT (`validateUpload()` → `ParsePresetText()`), and a stored bank that fails to
parse at boot falls back to the embedded presets.toml (see "USB-MIDI preset upload"):
- presets.toml must be plain ASCII, comments included (`line L, column C: non-ASCII byte 0xNN`):
  tomlc99 passes plain `char`s to `isdigit()`, which is undefined for bytes >= 0x80, so
  `ParsePresetBank()` rejects them in `checkAscii()` before parsing
  (`src/preset_bank.cpp:814-837`, called at `:848`)
- All eight `[preset.params.*]` parameter groups must be present, each containing exactly its
  own keys - 46 parameters total. Group membership is defined by `kGroups` in
  [src/preset_bank.cpp](src/preset_bank.cpp) and mirrored by the reference comment at the top of
  presets.toml and by `GROUPS` / `TOGGLE_PARAMS` in editor/src/model/schema.ts. Missing, unknown
  and misplaced parameters, and unknown preset- and root-level keys, are rejected
- `[preset.params.reverse]` must be present with exactly `delay`, `enabled` and `direct_mix`,
  each a number 0..1 (`missing [preset.params.reverse]`, `missing or non-numeric
  'reverse.delay'`, `'reverse.delay' = V out of range 0..1`,
  `[preset.params.reverse]: unknown key 'K'`). `delay` is the value `reverse.delay` starts at
  when the preset loads: 20 + Response3Oct(delay) × 1980 ms; `enabled` and `direct_mix` are the
  toggle pseudo-targets' stored starting state
- `[preset.params.delay_lines]` must be present with exactly `max`, a number 0..1: the stored
  starting state of the `"delay_lines.max"` toggle target
- `LineCount` alone must NOT appear: it is driven live by the `"delay_lines.max"` toggle target,
  which selects `default_delay_lines` (off) or `max_delay_lines` (on)
- `[preset.knob_map]` must be present with all twelve `knobN_a` / `knobN_b` keys. Each value
  is a quoted `"group.Parameter"` whose parameter really belongs to that group, or the
  pseudo-target `"reverse.delay"`. Unknown knob keys, unknown groups/parameters, cross-group
  targets, and runtime parameters are all rejected
- `[preset.toggle_map]` must be present with all eight `toggleN_a` / `toggleN_b` keys
  (N = 1..4 = SWITCH_1..SWITCH_4). Each value is one of the three pseudo-targets
  `"delay_lines.max"`, `"reverse.enabled"`, `"reverse.direct_mix"`, or a quoted
  `"group.Parameter"` naming one of the twelve on/off parameters in `kToggleParams`:
  `early.Bloom`, `input.HiPassEnabled`, `input.LowPassEnabled`,
  `early_diffusion.DiffusionEnabled`, `early_diffusion.DiffusionStages`,
  `late_diffusion.LateDiffusionEnabled`, `late_diffusion.LateDiffusionStages`,
  `late_eq.LowShelfEnabled`, `late_eq.HighShelfEnabled`, `late_eq.CutoffEnabled`,
  `late.LateStageTap`, `late.Interpolation`. Unknown toggle keys, unknown groups/parameters,
  cross-group targets and runtime parameters are rejected; a continuous parameter (e.g.
  `late.LineDecay`) is rejected with `'Name' is not an on/off parameter`: a lever would slam it
  to 0 or 1
- `blinks` is a TOML integer 1..20 (`blinks = 11.0` is rejected too: `preset N: blinks out of
  range 1..20`, `src/preset_bank.cpp:733-737`); `default_delay_lines` and `max_delay_lines` are
  each a whole number 1..`TotalLineCount` (5, `CloudSeed/DelayLineCount.h`; checked by
  `readLineCount()`; a fraction or `nan` fails with `preset N: <key> must be a whole number
  1..5`), and `default_delay_lines` must not exceed `max_delay_lines`
  (`preset N: default_delay_lines exceeds max_delay_lines`) - the toggle's off state must never
  exceed a preset's CPU cap; the ms fields are optional whole
  numbers 0..60000, written `300` or `300.0` (defaults 150/150/5000 when absent; a fraction, a
  string or an out-of-range value fails with `preset N: <key> must be a whole number 0..60000`);
  `name` must be non-empty (`preset N: empty name`, `:723`) and at most 31 bytes
  (`kMaxPresetNameLen` - 1; longer fails with `preset N: name longer than 31 bytes`); the preset
  count must be 1..`kMaxPresets` (16) (`preset count C out of range 1..16`, `:783-786`)
- Every parameter value must be finite and within 0.0-1.0 (`preset N: 'Name' = V out of range
  0..1`): `AudioLib::ValueTables::Get` indexes its tables with the raw value, so anything else
  would read out of bounds on the pedal. The real-unit ranges are listed in the TOML header and
  implemented by `ReverbController::GetScaledParameter`
- The document must fit the boot parse arena (see "Boot parse and TOML arena")

### Boot parse and TOML arena

Preset text - the embedded presets.toml, or an uploaded bank read from QSPI - is parsed from a
dedicated 512 KB `DSY_SDRAM_BSS` arena that is not part of `custom_pool` (`toml_arena`,
`src/sdram_pool.cpp:49-59`; parsing itself is `ParsePresetText()`, `src/sdram_pool.cpp:63-69`, a
wrapper that resets the arena around the host-portable `ParsePresetBankText()`,
`src/preset_bank.cpp:870`). The same arena and parser run again at runtime to validate a USB
upload before anything is written to flash (see "USB-MIDI preset upload"). Every parse
starts the arena at offset 0 and resets it on return, so boot and an upload validation never
overlap, and the reverb's `custom_pool` is unaffected either way. Peak measured usage at boot is
142,840 B on x86-64 (smaller on 32-bit ARM). Permanent SDRAM cost of the TOML parse arena:
512 KB, whether or not a USB upload ever happens. `EmbeddedPresetText()`
(`src/sdram_pool.cpp:71-74`) hands out the embedded presets.toml; it returns
`presets_toml_len - 1` because `src/presets_toml.s` appends a NUL (`src/sdram_pool.cpp:72`).

### Settings persistence

The current preset and the bypass state are both automatically saved to and loaded from QSPI flash memory, so they persist across power cycles. Writes are coalesced: flash is written `SETTINGS_SAVE_DELAY_MS` (3000 ms) after the last change.

The preset index is persisted, so **the order in presets.toml is frozen**; reordering or deleting
entries requires bumping `SETTINGS_VERSION` (`src/pedal_storage.cpp:5`).

**Implementation** ([src/pedal_storage.h](src/pedal_storage.h),
[src/pedal_storage.cpp](src/pedal_storage.cpp)): `PedalStorage` owns both
`PersistentStorage` instances. It is constructed in `src/cloudseed.cpp` as
`gStorage(hw.seed.qspi)` (`src/cloudseed.cpp:121`), after `hw` (`:119`) in the same translation
unit, because `PersistentStorage` needs the board's `QSPIHandle&` at construction.

**Settings Structure** (`src/pedal_storage.h:15-29`):
```cpp
// abridged; see src/pedal_storage.h
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
1. **On startup**: `main()` (`src/cloudseed.cpp:622-625`) calls `gStorage.Init(active.hash)`
   (see "User preset save / factory restore"), then takes `RestoredPreset(gPresets.count)` /
   `RestoredBypass()` into `state` and calls `loadPreset()`
2. **On preset change / bypass toggle**: nothing is written yet. `loadPresetGated()` updates
   `state.currentPreset` in the main loop; the audio callback flips `state.bypass`
   (`processFootswitches()`)
3. **In main loop**: `gStorage.ServiceSettingsSave(state.currentPreset, state.bypass)`
   (`src/pedal_storage.cpp:107-120`, called on every pass at `src/cloudseed.cpp:683`) mirrors
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
- LED1 is re-synced to the restored bypass state after load (`src/cloudseed.cpp:629-630`)
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
// abridged; see src/pedal_storage.h
struct UserPreset {
    float    params[(int)::Parameter::Count];  // reverb->GetAllParameters() at save time
    float    reverseDelay;                     // state.reverseDelayNorm at save time
    float    delayLinesMax;                    // state.delayLinesMax at save time, 0.0 or 1.0
    float    reverseEnabled;                   // state.reverseEnabled at save time, 0.0 or 1.0
    float    reverseDirectMix;                 // state.reverseDirectMix at save time, 0.0 or 1.0
    uint32_t valid;                            // USER_PRESET_VALID, else load from presets.toml
};  // 208 B
struct UserPresets { uint32_t identity; UserPreset presets[kMaxPresets]; }; // 3332 B
```

- Every field is 4 bytes wide, so there is no padding and `operator!=` is a `memcmp`. A
  `static_assert` keeps `sizeof(UserPresets)` + the 4 B `PersistentStorage` state word within
  one sector; it breaks if `kMaxPresets` goes above 19
- A slot is used only when `valid == USER_PRESET_VALID` (1); erased flash reads 0xFFFFFFFF.
  `valid` is the last field, and QSPI pages are programmed in ascending order, so a write cut
  off by power loss can leave a slot invalid (factory) but never half-written
- `params` is the full `GetAllParameters()` snapshot. `LineCount` comes along but is ignored,
  because `LoadPreset()` skips it. `Bloom` is saved like every other parameter. The
  knob map, toggle map, `default_delay_lines`/`max_delay_lines`, and blink timing are not saved;
  `delay_lines.max` is saved as its on/off state

**Firmware + bank identity** (`firmwareImageHash()`, `src/pedal_storage.cpp:24-31`): FNV-1a over
the words between the linker symbols `_stext` and `_etext`, which bound `.text` + `.rodata`
(`libdaisy/core/STM32H750IB_sram.lds:36,47`) and include the embedded presets.toml.
`PedalStorage::Init(presetBankHash)` (`src/pedal_storage.cpp:41-56`, called from `main()` at
`src/cloudseed.cpp:622` as `gStorage.Init(active.hash)`, before the first `loadPreset()`) mixes
that image hash with the FNV-1a hash of whichever bank text is active
(`(firmwareImageHash() ^ presetBankHash) * 16777619u`) into `UserPresets::identity`, and calls
`RestoreDefaults()` (one sector erase) when the stored `identity` differs. Any code change, any
presets.toml change, a USB upload, or a revert therefore discards every saved preset - all four
change one of the two hashes that make up `identity`. A byte-identical reflash of the same bank
keeps them, because nothing distinguishes it from a reboot. `Settings` (preset index, bypass)
are unaffected by either hash.

**Save** (FS1 held 5 s): `processFootswitches()` sets `gSaveRequested`. On the next callback
with the preset-change gate open, if the previous snapshot has been consumed, the callback
copies `GetAllParameters()`, `state.reverseDelayNorm` and the three toggle pseudo-values into
`gSaveSnapshot`, marks it valid, and publishes it with `state.saveSnapshotReady`
(`src/cloudseed.cpp:480-490`). The main loop (`:654-661`) copies the
snapshot into `gStorage.UserSlot(state.currentPreset)`, clears the flag, calls
`gStorage.SaveUserPresets()` (a
blocking erase + write in the main loop; audio keeps running from SRAM), and starts the
confirmation blink.

**Restore** (FS1 + FS2 held 5 s): the callback sets `state.triggerPresetRestore`. The main
loop (`src/cloudseed.cpp:662-668`) clears the slot to `UserPreset{}` (invalid), reloads the preset through
`loadPresetGated()`, which parks the knobs and toggles and drops any unsaved tweaks, calls
`SaveUserPresets()`, and starts the confirmation blink. When the slot was already empty
`SaveUserPresets()` erases and writes nothing (`PersistentStorage` writes only when the data
differs from flash, `libdaisy/src/util/PersistentStorage.h:96-99`).

Both handlers run before the preset-change block in the main loop, so a snapshot is always
stored into the preset that was loaded when it was taken.

**Confirmation**: LED1 and LED2 blink together 3 × 80 ms, even when bypassed (see "LEDs").

### USB-MIDI preset upload

The Seed's micro-USB port enumerates as a class-compliant USB-MIDI device at every boot
(`gMidiLink.Init()`, `src/cloudseed.cpp:647`, immediately before `hw.StartAudio()`); no driver
and no button press are needed. A Web MIDI host (typically a browser page, see "Browser Editor")
can upload a complete `presets.toml` text over SysEx, read the active bank's text back, or revert
to the bank built into the firmware. Protocol v1 (`F0 7D 43 53 <cmd> ... F7`;
INFO/BEGIN/DATA/COMMIT/REVERT/READ/ABORT) is defined normatively in
[src/preset_protocol.h](src/preset_protocol.h) and documented for host authors in
[docs/USB_MIDI.md](docs/USB_MIDI.md). Audio passes through dry for the duration of a session.

**Link** ([src/usb_midi_link.h](src/usb_midi_link.h), [src/usb_midi_link.cpp](src/usb_midi_link.cpp)):
`UsbMidiLink` drives `daisy::MidiUsbTransport` directly for receive - libdaisy's `MidiHandler`
truncates SysEx at 128 B (`libdaisy/src/hid/MidiEvent.h:2`) - and its USB-interrupt callback does
nothing but `SysExAssembler::Feed()` (`src/preset_protocol.h`), because the USB OTG FS interrupt
shares priority 0 with the audio DMA (`libdaisy/src/usbd/usbd_conf.c:102-107`,
`libdaisy/src/sys/dma.c:17-50`). Replies bypass `MidiUsbTransport::Tx()`, which splits SysEx and
sends the closing F7 as a packet of its own (`libdaisy/src/hid/usb_midi.cpp:294-318`): they are
packed into USB-MIDI 1.0 event packets by `PackSysExUsbMidi()` and sent with
`UsbHandle::TransmitInternal()`, the same CDC path. `SendSysEx()` tries `TransmitInternal()` once
plus `kTxRetries` (10) retries 100 µs apart and drops the reply if the endpoint stays busy
(`src/usb_midi_link.cpp:27-36`).

**Main loop** (`serviceUsbPresetLink()`, `src/cloudseed.cpp:528-570`, called every pass at `:681`,
before `gStorage.ServiceSettingsSave()`): `UsbMidiLink::Service()` (called first, `:529`)
restarts reception after a ring-buffer overflow (`src/usb_midi_link.cpp:16-21`). It then answers
one received SysEx frame through `PresetProtocol::Handle()`, performs the resulting
`WriteStoredBank()` (COMMIT) or `EraseStoredBank()` (REVERT) on `gStorage`, sends the reply, and -
on a successful COMMIT/REVERT, or a failed COMMIT while the pedal was running an uploaded bank
(`destroyedActive`, `src/cloudseed.cpp:548-551`; on the built-in bank a `FlashError` reply leaves
the pedal running) - flushes the pending settings save (`gStorage.FlushSettingsSave()`,
`src/pedal_storage.cpp:122-128`), waits 100 ms (`System::Delay(100)`) so the reply's USB transfer
completes, and reboots with `NVIC_SystemReset()` (`src/cloudseed.cpp:553-555`; the bootloader
reloads the app, which boots the new bank). It also `Tick()`s the 5 s session timeout and keeps
`state.uploadActive` (a `volatile bool`) in step with the session: while a
session is open the audio callback passes the input through and skips the reverb
(`state.presetChangeInProgress || state.uploadActive`); when the session
ends, `reverb->ClearBuffers()` runs and `state.controlResetPending` is set before `uploadActive`
is cleared, so knobs and toggles moved during the session re-park instead of jumping the engine.
The COMMIT validation parse and QSPI write block the main loop (hosts allow up to 10 s for the
reply) while the callback's passthrough gate is held closed; the REVERT one-sector erase needs no
session, so the gate is normally open during it.

**Validation and storage**: an upload is checked with the firmware's own parser
(`validateUpload()` → `ParsePresetText()` into a dedicated `PresetBank gUploadCheck`,
`src/cloudseed.cpp:139-143`) - the same rules `make presets-check` enforces - before anything
reaches flash. QSPI layout
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
matches. `EraseStoredBank()` erases only the header sector (`src/pedal_storage.cpp:98-101`);
`WriteStoredBank()`'s read-back invalidates the D-cache for the mapped range before comparing
(`src/pedal_storage.cpp:75-81`); a `static_assert` keeps the text below 0x40000
(`src/stored_bank.h:18-19`).

Protocol limits: `kChunkBytes` 240, `SysExAssembler::kMaxFrame` 256 (longer frames dropped;
single slot, bytes arriving while a frame is `Ready()` are dropped), `ParseError` message capped
at 200 chars (`src/preset_protocol.h:41-45`, `src/preset_protocol.h:79-82`,
`src/preset_protocol.cpp:19`). REVERT also ends an open session. INFO and READ are served from
the `ActiveBank` passed to `gPresetProtocol.Init()` (`src/cloudseed.cpp:646`).

**Boot** (`src/cloudseed.cpp:584-598`): the stored bank is used if `StoredBankText()` returns one
and it parses; otherwise the embedded presets.toml (`EmbeddedPresetText()`, see "Boot parse and
TOML arena") is used, and a parse failure there is the unrecoverable `FatalErrorLoop()` case.
Either way `gStorage.Init(active.hash)` mixes the active bank's FNV-1a hash into the user-preset
`identity` (see "User preset save / factory restore"), so an upload, a revert, or a reflash all wipe saved
user presets.

## Browser Editor

[editor/](editor/) (user docs in README.md "Preset editor (browser)") is a Preact app on Bun
(>= 1.4.2, no Node/npm) that edits every preset field, shows the signal flow, and uploads, reads
back and reverts banks: a second host for protocol v1 (see "USB-MIDI preset upload"), for Chrome
and Firefox with one code path.

Layout: `index.html` (app entry, title "Computer Room LARC") -> `src/main.tsx` -> `src/app.tsx`;
`src/midi/` protocol v1 client + Web MIDI transport; `src/model/` bank text model + validator
mirroring src/preset_bank.cpp, schema, real-unit scaling, signal-flow model, editor state;
`src/components/` + `src/styles/larc.css` LARC UI; `src/io/` file open/save; `serve.ts`
`Bun.serve` dev server (HTML import, HMR) + dist/ preview; `bunfig.toml`; `toml-text-plugin.ts`
(dev server: .toml as text); `*.test.ts(x)` bun:test suites (happy-dom preloaded by
`test-setup.ts`); `bun.lock`; `tsconfig.json`. Dependencies: `preact`, `@picocss/pico`, and
`smol-toml` (bank.ts's TOML front end - not tomlc99).

Commands (from editor/ unless noted):
- `make editor` (repo root): `bun install --frozen-lockfile`, then the dev server
- `bun run dev`: dev server at http://localhost:5174
- `bun run build`: `tsc --noEmit` + `bun build` into dist/ (gitignored)
- `bun run preview`: serves dist/ on 127.0.0.1:4174
- `bun test`, `bun run typecheck`

Keep it in step with the firmware:
- `editor/src/midi/protocol.ts` / `client.ts` port `src/preset_protocol.h` (INFO offsets as in
  `tools/usb_preset_host.py`). `PresetLink` is stop-and-wait, and resends only INFO, READ and
  DATA. `upload()` / `revert()` return `null` when the pedal cannot be verified after its
  reboot: `waitForReboot()` gave up after 20 s, or INFO went unanswered. COMMIT `Ok` has
  already stored the bank at that point. Reply timeouts: 1 s, COMMIT 10 s, REVERT 5 s
  (`editor/src/midi/client.ts:52-54`)
- `editor/src/midi/webMidi.ts` finds the pedal by `/daisy/i` in the port name or manufacturer,
  and after a reboot re-polls `requestMIDIAccess()` (no `onstatechange`). Each poll attempt is
  capped at 2 s: in Firefox a Web MIDI call on a port that vanished during the reboot stayed
  pending forever, which hung the upload. Observed 2026-09-26: an occasional re-enumeration
  returns a garbled USB product string (`/proc/asound/cards` shows `USB-Audio - Љ`), so no host
  finds "Daisy" until the pedal is re-plugged. `[INFERENCE]` libdaisy serves every string
  descriptor from one shared static buffer, `USBD_StrDesc` (`libdaisy/src/usbd/usbd_desc.c:289`)
- `editor/src/model/bank.ts` `loadBank()` re-implements `ParsePresetBankText()` with the
  exact messages. Its tests port `kRejects` / `kAccepts` from `tests/preset_bank_test.cpp`
  verbatim, so a new parser rule or message needs the same change there. Field edits patch only the
  value token of one line (an absent optional LED timing is inserted after `blinks`;
  duplicate/delete copy or remove whole preset blocks), so an unedited bank round-trips byte-identically (same FNV-1a as
  the pedal). It parses with `smol-toml`, not tomlc99, so a tomlc99 syntax quirk is not
  mirrored automatically
- `editor/src/model/schema.ts` mirrors `kGroups`, `kToggleParams` and the presets.toml
  parameter reference. `editor/src/model/scale.ts` ports `GetScaledParameter` and
  `ValueTables` for display only
- `editor/src/model/bank.ts` hard-codes `TOTAL_LINE_COUNT = 5` and `schema.ts` copies the
  knob/toggle target lists, so a TotalLineCount change or a new pseudo-target needs them too
  (see "Common Modifications" 1 and 4); `editor/src/model/state.ts:188-191` and `FlowPanel.tsx`
  also consume `TOTAL_LINE_COUNT`
- `editor/src/model/flow.ts` (drawn by `editor/src/components/FlowPanel.tsx`) models the signal
  path of `ReverbChannel::Process()` / `UpdateLines()`, `DelayLine::Process()` (slot order by
  `LateStageTap`; shelves and cutoff in the feedback path only) and the two reverse render
  helpers in `src/cloudseed.cpp`, and its explainer texts describe them. A change to the
  engine's routing, a new parameter, or a new pseudo-target needs the matching `BLOCKS` /
  `EFFECT` entry there; `flow.test.ts` fails when a `PARAM_KEYS` key has no block or effect, or
  when an `EFFECT` key or a `BLOCKS[*].keys` entry is not a `PARAM_KEYS` key

**Tests**: `cd editor && bun test` (App, FlowPanel, Fader, flow, state, bank, scale, client,
protocol); `make test` does not run them. They use the same tests/fixtures/two_presets.toml, and
`bank.test.ts` ports `kRejects` / `kAccepts` from `tests/preset_bank_test.cpp`. They also read
the live presets.toml and expect exactly 10 presets, so adding or removing a preset fails
`bun test` (not `make test`). Both TOML files are imported `with { type: 'text' }` (tests and
`src/io/files.ts`): a bare `.toml` import would make Bun parse the file instead of handing over
its exact bytes. Bun's dev-server bundler ignores that attribute (observed on Bun 1.4.2:
`bun run dev` inlined the parsed table), so `bunfig.toml` `[serve.static]` loads `.toml` as text
through `editor/toml-text-plugin.ts`; `bun build` and `bun test` honour the attribute on their
own.

## LEDs

LED2 uses a state machine to blink the preset number continuously (runs in the main loop). The
system is defined in [src/pedal_leds.cpp](src/pedal_leds.cpp) (`BlinkPattern` in
`src/pedal_leds.h:9-14`, `BlinkState` `src/pedal_leds.cpp:10-16`; `StartPresetBlink` `:57-64`;
`ServicePresetBlink` `:67-113`). The LED objects are file statics `gLed1`/`gLed2` (`:18-19`,
initialised by `LedsInit()`, `:25-31`), reachable only through the `pedal_leds.h` functions. The
pattern itself is `state.blinkPattern`, cached by `loadPreset()` and passed in by the main loop;
there is no per-call lookup function. Both functions only `Set()` LED2; the audio callback's
`UpdateLeds()` pushes it to the pin. LED1 is set by the bypass toggle in `processFootswitches()`.

**Key Components**:
- `BlinkPattern` struct: Defines blink timing parameters
- `BlinkState` struct: Maintains blink state machine
- `ServicePresetBlink(bypass, pattern)`: Main loop function that manages LED transitions
- `state.blinkPattern`: the active preset's pattern, refreshed by `loadPreset()`
- `StartPresetBlink()`: Initiates a new blink sequence

**Behavior**:
- Blinks continuously when pedal is active (not bypassed)
- Turns off completely when bypassed
- Blinks N times where N = the preset's `blinks` value (1-20; the preset number in the
  shipped presets.toml)
- `led_on_ms` on, `led_off_ms` off per blink (150 / 150, the default and the shipped value)
- `led_pause_ms` pause between sequences (5000 ms, the default and the shipped value)
- Automatically restarts sequence after pause

**Save / restore confirmation**: `StartConfirmBlink()` / `ServiceConfirmBlink()`
(`src/pedal_leds.cpp:116-139`) drive LED1 and LED2 together for `CONFIRM_BLINKS` (3) on/off
cycles of `CONFIRM_BLINK_MS` (80 ms) (`:6-7`), even when bypassed. While the blink runs,
`ServicePresetBlink()` is skipped. When it ends, LED1 is re-set to the bypass state and LED2
goes back to the preset pattern.

**Fatal error**: `FatalErrorLoop()` (`src/pedal_leds.cpp:45-54`) blinks both LEDs together at
5 Hz (100 ms on, 100 ms off) forever and never starts audio. It has three callers: the active
preset text failed to parse (`src/cloudseed.cpp:595`), the audio block size is not
`AUDIO_BUFFER_SIZE` (48) (`src/cloudseed.cpp:582`), or the SDRAM pool is exhausted
(`src/sdram_pool.cpp:34`).

To change LED2 behavior, modify `ServicePresetBlink()`.

## Reverb Engine

### Parameters

47 reverb parameters enumerated in [CloudSeed/Parameter.h](CloudSeed/Parameter.h) (`Parameter::Count` == 47, `CloudSeed/Parameter.h:8-88`):

**Input Stage**: InputMix, PreDelay, HighPass, LowPass

**Early Reverb**: TapCount, TapLength, TapGain, TapDecay, Bloom, DiffusionEnabled, DiffusionStages, DiffusionDelay, DiffusionFeedback

**Late Reverb**: LineCount, LineDelay, LineDecay, LateDiffusionEnabled, LateDiffusionStages, LateDiffusionDelay, LateDiffusionFeedback

**Modulation**: EarlyDiffusionModAmount, EarlyDiffusionModRate, LineModAmount, LineModRate, LateDiffusionModAmount, LateDiffusionModRate

**Output**: DryOut, PredelayOut, EarlyOut, MainOut

**Frequency Response**: PostLowShelfGain, PostLowShelfFrequency, PostHighShelfGain, PostHighShelfFrequency, PostCutoffFrequency

**Seeds/Switches/Effects**: TapSeed, DiffusionSeed, DelaySeed, PostDiffusionSeed, CrossSeed, HiPassEnabled, LowPassEnabled, LowShelfEnabled, HighShelfEnabled, CutoffEnabled, LateStageTap, Interpolation

Parameter names table: `CloudSeed/ParameterNames.h`.

### Core classes

**ReverbController** ([CloudSeed/ReverbController.h](CloudSeed/ReverbController.h)):
- Main reverb controller
- Preset application: `LoadPreset(const float* values)` (`CloudSeed/ReverbController.h:47-60`)
  copies a parsed `PresetData::params[]` into `parameters[]`, skipping only `LineCount` (written
  at audio rate from the `"delay_lines.max"` toggle target; `Bloom` is preset data like
  everything else), then re-applies all 47 slots through `SetParameter`. The constructor loads
  no preset - `main()` parses the active bank and calls `LoadPreset()` before audio starts
- Single channel: `channelR` and all right-channel buffers are commented out
  (`CloudSeed/ReverbController.h:27`, `:29`, `:31`, `:37`, `:72`, `:173`, `:181`, `:194`, `:198`, `:200`, `:206`)
- Fixed internal block size `static const int bufferSize = 48` (`CloudSeed/ReverbController.h:23`),
  backing fixed-size member arrays (`:28-32`)
- Public API: `LoadPreset(const float*)` `:47`, `GetAllParameters()` `:80`,
  `SetParameter(Parameter, float)` `:167`, `ClearBuffers()` `:178`,
  `Process(float* input, float* output, int bufferSize)` `:184`
- Parameter scaling (the normalized 0.0-1.0 → real-unit mapping documented in presets.toml):
  `GetScaledParameter` `:85`. `Bloom` follows the same `< 0.5 ? 0 : 1` rule as every other
  on/off parameter (`:101`)

**ReverbChannel** ([CloudSeed/ReverbChannel.h](CloudSeed/ReverbChannel.h)):
- Builds `TotalLineCount` (5) delay lines for the mono implementation
  ([CloudSeed/DelayLineCount.h](CloudSeed/DelayLineCount.h)); `SetParameter(LineCount)` clamps the
  active count to 1..`TotalLineCount` (`CloudSeed/ReverbChannel.h:211-221`)
- Contains delay lines, diffusers, modulation

**DelayLine** ([CloudSeed/DelayLine.h](CloudSeed/DelayLine.h)):
- Core delay buffer implementation (263 lines)
- The delay buffer itself is SDRAM-pool-backed (see "SDRAM pool")
- `tempBuffer`, `mixedBuffer`, and `filterOutputBuffer` are real heap allocations
  (`CloudSeed/DelayLine.h:52-54`, freed at `:74-76`)

**AllpassDiffuser, MultitapDiffuser**: Diffusion stages
**ModulatedAllpass, ModulatedDelay**: Modulated processing

### SDRAM pool

CloudSeed requires massive delay buffers:

```cpp
// src/sdram_pool.cpp:19-38 (comments omitted)
static constexpr size_t alignUp8(size_t n) {
    return (n + 7u) & ~static_cast<size_t>(7u);
}

constexpr size_t CUSTOM_POOL_SIZE = 48u * 1024u * 1024u;
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

This custom allocator backs every delay buffer and the objects that own them: `ModulatedDelay`
(`CloudSeed/ModulatedDelay.h:41-42`), `ModulatedAllpass` (`CloudSeed/ModulatedAllpass.h:44-45`),
`MultitapDiffuser` (`CloudSeed/MultitapDiffuser.cpp:14-15`), and `ReverbChannel`'s `DelayLine`
objects and channel buffers (`CloudSeed/ReverbChannel.h:77-78`, `:99-101`; its destructor runs
`~DelayLine` explicitly without freeing, `:107-113`). It is declared in
[src/sdram_pool.h](src/sdram_pool.h). The reverse voice's `reverseDelayBuffer` is a separate
`DSY_SDRAM_BSS` array in `src/cloudseed.cpp:132`, outside the pool, and so is the TOML parse
arena (see "Boot parse and TOML arena").

Bump allocator, no free. The pool is aligned to the 32-byte M7 D-cache line and every block is
rounded up to 8 bytes. Exhaustion is a fatal boot error (`FatalErrorLoop()`, see "LEDs")
rather than a null return, because a write through 0x0 would land silently in ITCMRAM.
`custom_pool_allocate` keeps external linkage and this exact signature: the CloudSeed headers
declare it `extern`. Callers placement-new into it, so destructors of pool-backed objects must
not call `delete` - see [docs/PERFORMANCE.md](docs/PERFORMANCE.md) §6.

## Build System

### Checkout and prerequisites

`libdaisy/`, `DaisySP/` and `Terrarium/` are git submodules (`.gitmodules`); the build reads
all three (`Makefile:16-24`, `:42`), so a checkout without them fails at the first `include`.

```bash
git clone --recurse-submodules https://github.com/jimbattin/DaisyCloudSeed.git
# or, in an existing clone / after a pull that moves a submodule:
git submodule update --init --recursive
```

The superproject pins exact commits: libdaisy `v8.1.0`, DaisySP `V1.0.0`, Terrarium `main` at
`cd6c80d`. The `branch =` values in `.gitmodules` are those tag names, not branches, so never use
`git submodule update --remote`: it would move the submodules off the pinned commits.
`--recursive` is required: libdaisy v7+ moved CMSIS, the STM32H7 HAL and the USB device library
into nested submodules (`libdaisy/.gitmodules`), and neither libdaisy nor the app compiles
without them. It also fetches `libdaisy/tests/googletest` and `DaisySP/DaisySP-LGPL`, which the
firmware does not need (DaisySP's `make` builds the LGPL library only if it is present,
`DaisySP/Makefile:227-230`). A full recursive checkout of libdaisy is about 330 MB of working
tree plus 480 MB of git objects; `--shallow-submodules` on the clone reduces the git part.

Tools: the Daisy Toolchain (`arm-none-eabi-gcc`, `make`, `dfu-util`; this tree builds with Arm GNU
Toolchain 13.3.Rel1) and a host `gcc`/`g++` (`HOSTCC`/`HOSTCXX`, `Makefile:50-51`) - plain `make`
needs the host compilers too, because it builds `preset_check` to validate presets.toml.
Then `make libs` once (and after every submodule update), then `make`.

### Building libraries

```bash
# Rebuild all libraries (libcloudseed, DaisySP, libdaisy).
# Makefile:138-141 runs `clean all` in each, so this is a full rebuild of all three.
make libs

# Or build individually:
cd CloudSeed && make
cd libdaisy && make
cd DaisySP && make
```

### Building the firmware

```bash
# CloudSeed
make
```

`make` also runs `$(MAKE) -C CloudSeed` on every invocation (`Makefile:32-39`): the sub-make
is incremental and rebuilds `libcloudseed.a` only when a library source changed, and the ELF
relinks only when the archive's mtime moved. A CloudSeed edit therefore never needs `make libs`.

### Checking presets

```bash
# Validity check with the firmware's own parser, without building firmware.
# The same check runs automatically as part of `make`.
make presets-check
```

It builds `build/preset_check` from `tools/preset_check.cpp`, `src/preset_bank.cpp`, and the
vendored tomlc99 (compiled as C) using `HOSTCC`/`HOSTCXX` (default `gcc`/`g++`), then runs
`preset_check --validate presets.toml`. Unlike the stamp-gated check inside `make`, it re-runs
every time. `preset_check` requires exactly one mode: `--validate` (one-line summary; exit 1
with the parser's message on stderr if the file is rejected), `--print-knob-map`, or
`--print-toggle-map`; anything else prints usage and exits 2. A rejected file exits 1 in every
mode; an unreadable file prints `cannot open <path>` and exits 2; exhausting the arena replica
prints `boot parse arena exhausted …` and exits 1 (`tools/preset_check.cpp:59-66`, `:136`,
`:145-150`).

`make` cannot produce firmware from a presets.toml the parser would reject: the embedded blob
(`$(BUILD_DIR)/presets_toml.o`) depends on `$(BUILD_DIR)/presets.valid`, whose recipe is
`preset_check --validate presets.toml` (`Makefile:53-70`). `preset_check` calls
`ParsePresetBankText()` with the file's full length, exactly as the pedal does at boot, so an
empty file or an embedded NUL byte is rejected too. It enforces every rule in "presets.toml
schema and parser rules", including the arena fit: the host tool allocates through a replica of
`TOML_ARENA_SIZE` (512 KB, 8-byte aligned, no reuse), so `presets.toml: 10 presets valid, boot
arena peak 142840 of 524288 bytes` is the same peak the pedal sees. Host pointers are 64-bit, so
the reported peak over-estimates the 32-bit target: a pass here implies a fit on hardware. A
near-miss should be fixed by raising `TOML_ARENA_SIZE` (`src/sdram_pool.cpp:49`), not by
loosening the host check.

Preset values are free to change: `make presets-check` fails only on input the parser rejects.
There is no golden-value comparison; the original factory values are recoverable from git
history.

### Host unit tests

```bash
make test
```

Builds seven host executables into `build/` with `HOSTCC`/`HOSTCXX` (`Makefile:80-121`) and runs them;
no ARM toolchain, firmware build or libdaisy is involved. Each prints `<suite>: N checks, 0 failed` and
exits non-zero on any failed `CHECK()` ([tests/check.h](tests/check.h)):
- `knob_bank_test` - `KnobBank` parking, move threshold, 50-block takeover glide, apply
  epsilon, rails, bank change, reset mid-glide
- `toggle_bank_test` - `ToggleBank` parking on first scan / bank change / `forceReset`, and
  immediate writes after a flip
- `footswitch_gestures_test` - `FootswitchGestures` driven through a model of libdaisy's 8-bit
  `Switch` shift register: taps, 5 s save, FS2 hold + `MarkEdited()`, staggered chords, the
  restore chord, bounces, and a falling edge without a prior `Pressed()`
- `preset_bank_test tests/fixtures/two_presets.toml` - `ParsePresetBank()` on a two-preset
  fixture (Chorus + Through the Looking Glass), then a table of single-line mutations that
  must each be rejected with a specific message (non-ASCII bytes in a value and in a comment
  among them), a check of the line and column reported for a non-ASCII byte, two
  mutations that must be accepted, a float `led_on_ms = 300.0` that must be honoured, and a
  31-byte name that must be kept whole. `ParsePresetBankText()` is checked to parse exactly
  `length` bytes of text with no NUL terminator, to leave its source untouched, to release
  every allocation on success and on failure, and to reject empty text, embedded NUL bytes,
  and a failed scratch allocation
- `engine_alloc_test` - the whole CloudSeed library compiled for the host, with counting
  replacements for `operator new` and `custom_pool_allocate`: after boot, every parameter is
  swept 0 → 0.25 → 0.5 → 0.75 → 1 → 0.5 through `SetParameter()` with a `Process()` block after each write, and
  must cause zero heap and zero pool allocations (see "Realtime rules")
- `preset_protocol_test tests/fixtures/two_presets.toml` - `SysExAssembler` framing (split
  frames, real-time bytes, overflow, aborted, empty and restarted frames), `PackSysExUsbMidi()`
  CIN/padding, and the `PresetProtocol` state machine: INFO fields, BEGIN length checks and
  restart, chunked upload + COMMIT including a full 98,304-byte upload and an overrun at that
  size, duplicate/gapped/wrapped sequence numbers, length/hash mismatches (hash bits 28-31
  included), `ParseError` and `FlashError` replies, READ chunking, the 5 s session timeout,
  ABORT, REVERT, and foreign/unknown SysEx. Every reply is checked to be one well-formed
  7-bit SysEx message. An end-to-end case uploads the fixture through USB-MIDI packets and the
  real parser, and checks that a rejected bank's COMMIT reply carries the parser's own message
- `stored_bank_test` - `ValidStoredBankText()`: erased flash, wrong magic, a different firmware
  image, corrupted text or hash, and the 1..`kMaxTextBytes` length bounds

The editor's `bun:test` suite is separate (see "Browser Editor").

The parser test deliberately uses its own fixture, not presets.toml, so editing preset values
never breaks `make test`; the live file stays covered by `make presets-check` and the build
gate.

### Build outputs

Located in `build/`:
- **{target}.bin** - Binary for DFU flashing via USB
- **{target}.elf** - ELF executable with debug symbols
- **{target}.hex** - Intel HEX format
- **{target}.map** - Linker map file

This project builds with `APP_TYPE = BOOT_SRAM` (`Makefile:6`): the application is loaded into
SRAM from QSPI flash by the Daisy bootloader, which is what allows room for all ten presets.

### Flashing to hardware

```bash
# One time (or after bootloader updates): flash the Daisy bootloader over DFU
make program-boot

# Then reset the Daisy, hold BOOT until the LED blinks rapidly, and flash the app
make program-dfu
```

Both targets come from `libdaisy/core/Makefile:348-352` and require `dfu-util`. `program-boot`
flashes `BOOT_BIN`, the bootloader shipped with libdaisy (`dsy_bootloader_v6_4-intdfu-2000ms.bin`
at v8.1.0, `libdaisy/core/Makefile:220`); a pedal still running an older bootloader keeps working
but misses its fixes (v6.3+ carries libdaisy's QSPI write-protect fix), so re-run it after a
libdaisy bump that ships a new one. A `BOOT_SRAM` build cannot be flashed with `make program`
(openocd) - libdaisy errors out on that path (`libdaisy/core/Makefile:340-341`). Same procedure
as `README.md:89-100`.

### Compiler configuration

**Platform**: ARM GCC (`arm-none-eabi-gcc`)
**CPU**: Cortex-M7 (`-mcpu=cortex-m7`; app `libdaisy/core/Makefile:90`, library `CloudSeed/Makefile:70`)
**Optimization**: `-O3` (`Makefile:20` overrides libdaisy's `OPT ?= -O2`,
`libdaisy/core/Makefile:29`; library `CloudSeed/Makefile:20`)
**FPU**: Hard float (`-mfpu=fpv5-d16 -mfloat-abi=hard`; app `libdaisy/core/Makefile:93-96`, library `CloudSeed/Makefile:73-76`)
**Language**: C++14 (`-std=gnu++14`; app `libdaisy/core/Makefile:205`, library `CloudSeed/Makefile:67`)
**Float flags**: `-ffast-math` on both the app (`Makefile:44`) and the library
(`CloudSeed/Makefile:106`); the library additionally uses `-fno-exceptions`,
`-finline-functions`, and `-fno-aggressive-loop-optimizations` (`CloudSeed/Makefile:104-108`)
**App type**: `BOOT_SRAM` (see "Build outputs")
**Host tools and tests**: `-std=gnu++14 -O1` (`Makefile:58`, `:83`); the tomlc99 host object
`-std=gnu11 -O1` (`:54`)

## Common Modifications

### 1. Changing control mappings

**File**: [presets.toml](presets.toml) - no C++ changes required for a remap.

**Example**: Swap KNOB_1 and KNOB_2 in one preset - edit that preset's `[preset.knob_map]`:

```toml
knob1_a = "output.EarlyOut"
knob2_a = "output.DryOut"
```

Then `make` (which validates the file) and `make program-dfu`. The map is per preset, so the
same two lines must be edited in every preset that should share the scheme. Verify the
resolved map with `./build/preset_check --print-knob-map presets.toml`.

**Example**: Put the input low-pass on KNOB_6's secondary bank:

```toml
knob6_b = "input.LowPass"
```

`applyKnobTarget()` turns `LowPassEnabled` on the first time that
knob is moved, so the filter is audible even in presets that ship with it off; the takeover
glide starts from the open filter (`knobTargetValue()`), so enabling it does not step the tone.
`HighPass` gets the same treatment via `HiPassEnabled`; no other gated parameter does - for the
shelves, the in-loop cutoff, and the diffusers, set the matching `*Enabled` value in
`[preset.params.*]`. Either filter's enable can instead be owned by a toggle target (see
"Toggle take-over"), in which case
`applyKnobTarget()` leaves the enable alone and the knob only moves the cutoff.

`applyKnobTarget()` is also where the output-level cache is invalidated: its `switch` sets
`state.outputLevelsDirty` for `DryOut`/`EarlyOut`/`MainOut`. Any new code path that writes one
of those three parameters outside `loadPreset()` must set that flag, or the makeup gain and the
dry-cancellation scale will stay at their old values.

**Example**: reassign SWITCH_2 (Bloom by default) to enable the late low-pass instead - edit
that preset's `[preset.toggle_map]`:

```toml
toggle2_a = "late_eq.CutoffEnabled"
toggle2_b = "late_eq.CutoffEnabled"
```

Verify with `./build/preset_check --print-toggle-map presets.toml`. The allowed toggle values
are listed in "presets.toml schema and parser rules".

**Adding a non-parameter (pseudo) target** - like `reverse.delay` for knobs or
`delay_lines.max` for toggles - requires C++: a new `KnobTargetKind` / `ToggleTargetKind` in
[src/preset_bank.h](src/preset_bank.h), a branch in `parseKnobTarget()` / `parseToggleTarget()`
([src/preset_bank.cpp](src/preset_bank.cpp)), and a branch in `applyKnobTarget()` /
`applyToggleTarget()` (see "Controls"), plus the matching `parseKnobTarget()` /
`parseToggleTarget()` branch in editor/src/model/bank.ts. This is also the way to change what a
switch does in every preset without touching presets.toml (e.g. a fourth toggle pseudo-target).
If the target carries a per-preset starting value (as `reverse.*` and `delay_lines.max` do),
also parse it in `parseParams()` into `PresetData`, add it to `UserPreset`
(`src/pedal_storage.h`), load it in `loadPreset()` (`src/cloudseed.cpp:192-197`) and capture it
in the save snapshot (see "User preset save / factory restore"). Editor: `KNOB_TARGETS` /
`TOGGLE_TARGETS`, `PSEUDO_GROUPS`, `DEFS`, `SWITCHES`, `PAGES` in
`editor/src/model/schema.ts`, `formatValue` in `editor/src/model/scale.ts`, and a
`BLOCKS`/`EFFECT` entry in `editor/src/model/flow.ts` (`flow.test.ts` fails without it).

### 2. Modifying parameter ranges

Knobs are read raw: `hw.knob[kKnobIndex[i]].Value()` (step 4 of "Audio callback") yields
0.0-1.0 and is handed straight to `SetParameter`, which applies the engine's own scaling
(`ReverbController::GetScaledParameter`). There is no per-knob min/max.

To restrict a knob's travel, scale in `applyKnobTarget()` before the
`SetParameter` call, e.g. `value = 0.5f + 0.5f * value;` for the upper half of the range, **and**
apply the inverse for that parameter in `knobTargetValue()`, e.g.
`(v - 0.5f) / 0.5f` clamped to 0..1. The takeover glide runs in knob space from
`knobTargetValue()` to the pot, so without the inverse its first step writes `0.5 + 0.5 * stored`
instead of `stored` - a jump. A stored value outside the knob's range still jumps to the nearest
end of that range on the first write. Both changes affect every preset that maps a knob to that
parameter.

### 3. Adding or editing presets

**File**: [presets.toml](presets.toml) - no C++ changes required.

Append a new `[[preset]]` table at the end of the file (appending keeps the existing indices,
which are persisted to QSPI flash):

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
# is active only while the preset footswitch (FS2) is held down.
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
Bloom     = 0.0

# ... the remaining six [preset.params.*] groups (early_diffusion, late, late_diffusion, late_eq, seeds, output), every key required

# Delay lines: stored state of the "delay_lines.max" toggle target
# (off = this preset's default_delay_lines, on = its max_delay_lines).
[preset.params.delay_lines]
max = 0.0

# Reverse voice: window length (knob target "reverse.delay") and the stored state of
# the toggle targets "reverse.enabled" (voice on) and "reverse.direct_mix" (routing).
[preset.params.reverse]
delay      = 0.4771
enabled    = 0.0
direct_mix = 0.0
```

The file must satisfy "presets.toml schema and parser rules". Then rebuild and reflash:
`make && make program-dfu` - validation is part of `make` (`make presets-check` runs the same
check without building firmware). The preset count comes from `gPresets.count`, and the modulo
cycling adapts automatically.

**Reordering or deleting presets** invalidates saved settings: bump `SETTINGS_VERSION` in the
same change so stale flash contents are discarded. Saved user presets need nothing: any
presets.toml change alters the firmware hash, which discards them all at the next boot.

**Blink pattern customization**: per preset, via `blinks`, `led_on_ms`, `led_off_ms`, and
`led_pause_ms`.

### 4. Changing the number of delay lines

**File**: [CloudSeed/DelayLineCount.h](CloudSeed/DelayLineCount.h)

```cpp
constexpr int TotalLineCount = 5;  // CloudSeed/DelayLineCount.h:14
```

`TotalLineCount` is the single definition of how many `DelayLine`s `ReverbChannel` builds, and
everything that depends on it follows automatically:
- `ReverbChannel::SetParameter(LineCount)` clamps the active count to 1..`TotalLineCount`
  (see "Core classes"), so no caller can make `Process` loop past the lines that
  exist (`CloudSeed/ReverbChannel.h:401`, `:404`). The lower bound matters too:
  `ReverbController::LoadPreset()` re-applies the stored `LineCount`, which is 0 at boot until
  the audio callback sets the real count
- `readLineCount()` (`src/preset_bank.cpp:195-210`) validates `default_delay_lines` /
  `max_delay_lines` against it, so `make` rejects a preset asking for more lines than exist,
  naming the preset and key: `preset N: max_delay_lines must be a whole number 1..<TotalLineCount>`.
  The `preset_check`, `preset_bank_test` and `preset_protocol_test` rules list the header as a
  prerequisite (`Makefile:56-57`, `:99-100`, `:105-107`) and `engine_alloc_test` picks it up
  through `CLOUDSEED_HEADERS` (`:91`, `:102-103`), so all are rebuilt when it changes

After lowering it, `make` fails until every preset's `default_delay_lines` / `max_delay_lines`
fits (nine ship `max_delay_lines = 5.0`). Also update the `1..5` wording in presets.toml's
header and this file, the delay-line counts in README.md (`README.md:37` "4 or 5", `:51` "(5 in
this fork)", `:354` "(5, or 4 ...)"), the expected `1..5` messages and 5.0 values in
`tests/preset_bank_test.cpp` / `tests/fixtures/two_presets.toml` / `editor/src/model/bank.test.ts`
/ `editor/src/model/flow.test.ts` (`editor/src/model/flow.test.ts:56` expects 5),
`TOTAL_LINE_COUNT` in `editor/src/model/bank.ts`, and the "up to 5" explainer text in
`editor/src/model/flow.ts` (`editor/src/model/flow.ts:113`). Raising it costs SDRAM pool
memory and CPU per line ("Through the Looking Glass" already crackles above 4).

### 5. Adjusting the audio buffer size

**File**: [src/cloudseed.cpp](src/cloudseed.cpp) (`main()`)

```cpp
// Current: 48 samples per block
hw.StartAdc();
// ... 200 ms knob one-pole settle loop (see "Knob take-over" above) ...
// ... gPresetProtocol.Init(...); gMidiLink.Init(); ...
hw.StartAudio(audioCallback);
```

Three sizes are coupled and must change together:
- `DaisyPetal::Init()` sets the hardware block size to 48 (`libdaisy/src/daisy_petal.cpp:90`);
  override it with `hw.SetAudioBlockSize(n)` immediately after `hw.Init()`
  (`src/cloudseed.cpp:574`). It must precede the block-size guard (see "LEDs") and the knob
  `SetCoeff()` loop (see "Knob take-over"): it re-runs `SetHidUpdateRates()`, which overwrites
  every knob's one-pole coefficient (`AnalogControl::SetSampleRate`,
  `libdaisy/src/hid/ctrl.cpp:49-54`)
- `AUDIO_BUFFER_SIZE` (`src/cloudseed.cpp:31`) sizes the file-scope `gInputBuffer`, `gWetBuffer`,
  `gReverseBuffer` and `gReverbInputBuffer` (`src/cloudseed.cpp:315-318`) and every callback loop;
  `main()` stops in `FatalErrorLoop()` if `hw.AudioBlockSize()` differs
- `ReverbController::bufferSize` (see "Core classes") sizes the controller's fixed member arrays

Raising the hardware block size alone stops boot at that guard; raising it with
`AUDIO_BUFFER_SIZE` but not `bufferSize` overruns the controller's arrays.
Smaller blocks = lower latency, higher CPU load; larger blocks = higher latency, lower CPU load.

These are counted in callbacks and assume the 1 ms block (48 samples at 48 kHz); rescale them
with the block period:
- `kKnobGlideBlocks`: 50 blocks = the 50 ms takeover glide (see "Knob take-over")
- `kLongHoldBlocks`: 5000 blocks = the 5 s save / restore holds (see "Footswitch gestures")
- `KNOB_SMOOTHING_COEFF`: a per-callback one-pole coefficient, ~20 ms at 1 kHz
- libdaisy's `Switch::Debounce()` shifts at most once per ms: with longer blocks it shifts once
  per block, so the 8-shift press latch and 7-shift release edge stretch with the block

Everything timed with `System::GetNow()` / `System::Delay()` (the blink patterns, the
confirmation blink, the 3 s settings save, the 200 ms knob settle loop) is independent of the
block size.

### 6. Reading a knob input as CV

The Terrarium has no CV jacks. A knob's ADC input, after a hardware modification (0-3.3 V at the
Seed pin; a 5 V source needs a divider), reads the same way:

```cpp
// Read CV directly (identical to what the knob scan uses):
float cv_value = hw.knob[Terrarium::KNOB_1].Value();
// cv_value is 0.0-1.0 regardless of input voltage
```

## Debugging

### Serial debug output

```cpp
// src/cloudseed.cpp already includes daisy_petal.h (-> daisy_seed.h) and uses namespace daisy.

// In setup (true would block boot until a USB serial host opens the port):
hw.seed.StartLog(false);

// Anywhere:
hw.seed.PrintLine("Debug: value = %f", some_value);
```

`StartLog()` puts the same micro-USB port into USB CDC (serial) mode, and the USB-MIDI preset
link (`gMidiLink.Init()` in `main()`) needs it in MIDI mode. Remove the `gMidiLink.Init()`
call while logging; USB preset upload is unavailable in such a build.

### LED indicators

```cpp
// Terrarium LEDs are the daisy::Led file statics gLed1/gLed2 in src/pedal_leds.cpp (see "LEDs").
// DaisyPetal has no led1/led2 members - it exposes SetRingLed/SetFootswitchLed/ClearLeds for
// the Daisy Petal board's own I2C LED driver, which Terrarium does not use.
// For a debug value, add a setter next to SetBypassLed():
gLed2.Set(parameter_value);  // 0.0-1.0
```

`Set()` is enough once audio runs: the audio callback calls `Led::Update()` for both LEDs every
block (step 1 of "Audio callback"), and nothing else may call it after `hw.StartAudio()` (it is a
read-modify-write that races with the callback). Only before `StartAudio()` must you call
`UpdateLeds()` yourself.

Caveat: `ServicePresetBlink()` (LED2, every main-loop pass), the bypass toggle (LED1) and the
save/restore confirmation blink (both) overwrite debug values (see "LEDs"); remove those calls
while debugging with an LED.

### Common issues

**Build Errors**:
- Ensure submodules are initialized: `git submodule update --init --recursive`
- Rebuild libraries: `make libs` (runs `clean all` in CloudSeed, DaisySP, and libdaisy)
- Clean build: `make clean && make`
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
- Verify ADC channel mapping in Terrarium
- Check the knob smoothing coefficient (`KNOB_SMOOTHING_COEFF`) and the
  takeover constants in [src/knob_bank.h](src/knob_bank.h): a knob brushed by accident taking over its
  target means `kKnobMoveThreshold` is too low (raise it to 0.02); a parameter that is re-written
  while nobody touches a live knob means `kKnobApplyEpsilon` is below the ADC noise; a stop that
  does not reach exact silence / full level means `kKnobRailWindow` is narrower than the pot's
  bleed or top gap. Measure the ADC before changing any of them
- Confirm the knob is mapped where you expect: `./build/preset_check --print-knob-map presets.toml`
- Test with direct reads: `hw.knob[x].Value()`

## Performance and Memory

### CPU usage

**CloudSeed**: Heavy processing
- 5 delay lines with modulation
- Multiple diffusion stages
- Extensive filtering
- Estimated at ~70-80% CPU. **This is an unmeasured estimate** - no profiling artifact exists
  in the repo, and it predates the denormal-stall (FPU flush-to-zero) and `std::map`
  parameter-lookup fixes documented in [docs/PERFORMANCE.md](docs/PERFORMANCE.md), so real headroom is
  better than this figure suggests

### Memory usage

**CloudSeed** (from the linker's `--print-memory-usage` report, `make libs && make` on this tree):
- SDRAM: 53,138,176 B of 64MB (79.18%), entirely static `DSY_SDRAM_BSS`: `custom_pool`
  50,331,648 B + `reverseDelayBuffer` 768,000 B (192,000 floats = 4 s @ 48 kHz, sized so the
  2000 ms max reverse window clears `ReverseDelay`'s `size / 2` clamp) +
  `CloudSeed::FastSin::data` 131,072 B + `AudioLib::ValueTables` tables 1,280,032 B (see the
  `.sdram_bss` section in `build/cloudseed.map`) + the 512 KB `toml_arena` (not part of
  `custom_pool`; `src/sdram_pool.cpp`, used at boot and to validate USB uploads) + the 96 KiB
  `gUploadText` upload receive buffer + the ~4.8 KB `gUploadCheck` validation `PresetBank`
  (`src/cloudseed.cpp`)
- The runtime heap is not in this report: it grows from `end` in RAM_D2
  (`libdaisy/core/STM32H750IB_sram.lds:239-250`), which is where `DelayLine`'s `tempBuffer`,
  `mixedBuffer`, and `filterOutputBuffer` land
- SRAM (`.text`+`.data`, `BOOT_SRAM` region): 233,112 B of 480KB (47.43%). Of that, the
  embedded `presets.toml` blob is 49,028 B (`build/presets_toml.o` - it carries the
  per-preset `[preset.knob_map]`, `[preset.toggle_map]`, `[preset.params.reverse]` and
  `[preset.params.delay_lines]` tables), tomlc99 is 14,371 B, and `preset_bank.o` is 8,285 B
- DTCMRAM: 45,420 B of 128KB (34.65%), including about 15.6 KB of libdaisy's USB stack pulled in
  by `UsbMidiLink`: the four 2 KB `UserRxBufferFS`/`UserTxBufferFS`/
  `UserRxBufferHS`/`UserTxBufferHS` ring buffers, `midi_usb_handle` (2,112 B),
  `hpcd_USB_OTG_FS` / `hpcd_USB_OTG_HS` (1,292 B each), the two 732 B `hUsbDeviceFS`/`hUsbDeviceHS`
  device handles, `hhcd_USB_OTG_HS` (772 B, USB host HAL) and `USBD_StrDesc` (512 B), plus
  `gMidiLink` (616 B) and `gPresetProtocol` (312 B). Also includes the 4,804 B `gPresets` bank
  (40 B of that per preset slot is the knob + toggle maps: 24 B knobs, 16 B toggles), the
  6,732 B `gStorage` (6,676 B of it is the user-preset `PersistentStorage`, which keeps a
  defaults copy and a live copy of the 3,332 B `UserPresets`), and the 208 B `gSaveSnapshot`
- RAM_D2_DMA: 17,956 B of 32KB (54.80%): the 16 KB audio DMA buffer, `daisy_petal.o`/`adc.o`
  buffers, and libdaisy's 988 B `hUsbHostHS` USB-host handle (`usb_host.o`), linked in by
  `MidiUsbTransport`
- QSPI: `Settings` in sector 0 (offset 0), `UserPresets` in sector 1 (offset 0x1000), and the
  uploaded-bank `StoredBankHeader` (offset 0x10000) + text (offset 0x11000), all inside the
  256 KB below the bootloader's program area at 0x90040000
- Boot time: about +10 ms for `MidiUsbTransport::Impl::Init()` (`System::Delay(10)`,
  `libdaisy/src/hid/usb_midi.cpp:125`); the SDRAM buffers cost nothing at boot, since
  `.sdram_bss` is `NOLOAD` and never zeroed

### Optimization tips

See [docs/PERFORMANCE.md](docs/PERFORMANCE.md) for the concrete list of performance/correctness
fixes applied to the CloudSeed DSP (FPU denormal handling, parameter storage, the cached
per-line gain, hot-loop modulo/precision fixes, placement-new/delete destructor safety, build flags, the redundant
bypass copy, allocation-free parameter updates, engine state defined before its first
read, and the idle-free USB-MIDI preset link), each with the exact file/line and pattern it addresses.

## Lineage

This fork differs from the original CloudSeed and its predecessors (`CloudSeed/DelayLineCount.h`):
- **Original CloudSeed plugin**: 8 (or 12) delay lines, stereo
- **DaisyCloudSeed (Daisy Patch)**: 2 delay lines, stereo
- **GuitarML Terrarium fork**: 4 delay lines, mono
- **This fork**: 5 delay lines, mono (`constexpr int TotalLineCount = 5;`)

The trade-off: More delay lines in mono = richer reverb tail.

Changes the GuitarML Terrarium fork introduced:
1. Adapted for Terrarium hardware (mono, 6 knobs, 4 switches)
2. Increased delay line count from 2 (Daisy Patch) to 4 (GuitarML) and then 5 (this fork)
3. Added preset cycling via footswitch
4. Simplified control scheme for guitar pedal use
5. Added delay line switching via toggle switches

Recent changes: `git log --oneline -10`; current branch: `git branch --show-current`.

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

## Quick Reference

### Build & flash
```bash
make clean         # Clean previous build
make libs          # Rebuild libdaisy, DaisySP, and libcloudseed (clean all)
make presets-check # Validate presets.toml without building (also run automatically by `make`)
make test          # Host unit tests (knob/toggle/footswitch state machines, preset parser, engine allocations, USB preset protocol, stored-bank check)
make               # Build CloudSeed
make program-boot  # One time: flash the Daisy bootloader (BOOT_SRAM prerequisite)
make program-dfu   # Flash the app (reset, hold BOOT until rapid blink, then run)
make editor        # Browser preset editor: bun install + dev server (http://localhost:5174); needs Bun
cd editor && bun test   # the editor's test suite (not part of make test)
```

### File locations
- Control mapping: `[preset.knob_map]` / `[preset.toggle_map]` in `presets.toml`; dispatch in
  `updateEngineControls()`, `applyKnobTarget()`, `applyToggleTarget()` (src/cloudseed.cpp)
- Preset data: `presets.toml` (embedded via `src/presets_toml.s`, parsed by `src/preset_bank.cpp`)
- Preset application: `ReverbController::LoadPreset()` (CloudSeed/ReverbController.h)
- Parameters: `CloudSeed/Parameter.h`; names table: `CloudSeed/ParameterNames.h`
- Hardware config: `Terrarium/terrarium.h`
- USB-MIDI preset upload: [docs/USB_MIDI.md](docs/USB_MIDI.md) (protocol, for host authors);
  normative definition [src/preset_protocol.h](src/preset_protocol.h); link layer
  [src/usb_midi_link.h](src/usb_midi_link.h)
- Hardware validation checklist: [docs/HARDWARE_TESTS.md](docs/HARDWARE_TESTS.md), driven by
  `tools/usb_preset_host.py`
- Browser preset editor: [editor/](editor/) (see "Browser Editor")

### Where to change what
- Remap knobs/toggles → "Common Modifications" 1
- Restrict a knob's range → "Common Modifications" 2
- Knob feel constants → "Knob take-over"
- Add/edit presets and blink timings → "Common Modifications" 3
- Delay-line count → "Common Modifications" 4
- Audio block size → "Common Modifications" 5
- Footswitch gestures → "Footswitch gestures"
- LED behavior → "LEDs"
- Parser rule or schema change → "presets.toml schema and parser rules" + "Browser Editor"
- New persistent setting → "Settings persistence"
- User preset store → "User preset save / factory restore"
