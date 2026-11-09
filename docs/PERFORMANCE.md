# CloudSeed/Terrarium Performance Optimizations

This document records the concrete performance and correctness fixes applied to the
CloudSeed reverb DSP running on the Daisy Seed (STM32H750, Cortex-M7 at 400 MHz, 48-sample
audio block, ~1ms budget). Each section names the exact pattern found, the file/line it
lived at, and the fix applied. No control mapping, preset, or audio topology changed. File/line
anchors in each section are as of that change; §12 (the engine audit) later removed or moved
several of the code paths §1-§10 describe.

## 1. FPU flush-to-zero (denormal stall elimination)

**Files**: `src/cloudseed.cpp`

Reverb feedback paths decay exponentially toward zero every tail: `DelayLine::Process`'s
feedback multiply (`CloudSeed/DelayLine.h:198`), `ModulatedAllpass::Process*`'s feedback
(`CloudSeed/ModulatedAllpass.h:89,127`), and `Biquad::Process`'s IIR state
(`CloudSeed/AudioLib/Biquad.h:57-61`). Once a value enters the subnormal range (~1e-38),
the Cortex-M7 FPU takes a multi-cycle microcoded slow path per operation instead of
single-cycle. The codebase already had three ad-hoc guards (`ReverbChannel.h:373-380`,
`AudioLib/Hp1.h:63-66`, `AudioLib/Lp1.h:59-62`). They were not denormal guards: the first was
a -90 dBFS gate on the input, the other two a zero-snap of the one-pole filters' state, and
none covered the delay-line/allpass feedback state where the problem originates.

**Fix**: set the FPU's Flush-to-Zero bit (FPSCR bit 24) once at boot, in `main()` before
`hw.Init()`:

```cpp
__set_FPSCR(__get_FPSCR() | (1u << 24)); // FZ: flush denormals to zero in hardware
```

Added `#include "cmsis_gcc.h"` for the `__set_FPSCR`/`__get_FPSCR` CMSIS intrinsics. The
three guards were left in place at the time and removed in §12.

## 2. Replace `std::map<Parameter,float>` with a flat array

**File**: `CloudSeed/ReverbChannel.h`

`parameters` was declared `map<Parameter, float>` — every `SetParameter` call and every
`UpdateLines()` read did a red-black-tree lookup (pointer-chasing, cache-unfriendly) for a
dense enum with 45 values (`CloudSeed/Parameter.h`). The sibling class
`ReverbController` already used the correct pattern: `float parameters[(int)Parameter::Count]`.

**Fix**: changed `parameters` to `float parameters[(int)Parameter::Count]`, removed
`#include <map>`, and updated every access site (constructor init loop, `SetSamplerate`'s
lambda, `SetParameter`, and the six `UpdateLines()` reads) to index with `(int)Parameter::X`.

## 3. Cache `perLineGain` instead of recomputing per block

**File**: `CloudSeed/ReverbChannel.h`

`GetPerLineGain()` computed `1.0 / std::sqrt(lineCount)` in double precision on every
`Process()` call (~1000×/sec at a 48-sample block), even though `lineCount` only changes
when the user toggles a delay-line switch. A dead commented-out line at the `LineCount`
parameter case (`//perLineGain = GetPerLineGain(); // In original Cloud Seed`) showed this
was the intended fix.

**Fix**: added a `float perLineGain` member, initialized in the constructor and
recomputed only in the `Parameter::LineCount` case of `SetParameter`. `Process()` now
reads the member directly instead of calling `GetPerLineGain()` (deleted). This also
fixes an incidental double-precision promotion: the cached value is computed with
`std::sqrt((float)lineCount)`, resolving to the single-precision `sqrtf` overload.

## 4. Remove per-tap integer modulo in `MultitapDiffuser::Process`

**File**: `CloudSeed/MultitapDiffuser.h` at the time; the loop now lives in
`CloudSeed/MultitapDiffuser.cpp:49-50`

Inside the per-sample tap loop (up to `MaxTaps = 50` taps):
`auto idx = (index + tapPos[j]) % len;`. `len` is a one-second delay buffer size in
samples (not a power of two), so this compiled to a full integer division per tap per
sample. `index` is kept in `[0, len)` and `tapPos[j] < length <= len` is guaranteed by
`Update()`, so `index + tapPos[j]` never reaches `2*len` — a single conditional
subtraction suffices, matching the wraparound technique already used elsewhere
(`ModulatedDelay.h`, `ModulatedAllpass.h`).

**Fix**:
```cpp
auto idx = index + tapPos[j];
if (idx >= len) idx -= len;
```

## 5. Fix double-precision `fmod` literal promotion

**Files**: `CloudSeed/ModulatedDelay.h:96`, `CloudSeed/ModulatedAllpass.h:150`

Both did `modPhase = std::fmod(modPhase, 1.0);` where `modPhase` is a `float` member. The
`1.0` literal is a `double`, forcing the `double` overload of `fmod` (promote, call,
narrow) instead of `fmodf`. This runs in `Update()`, called every 8 samples per instance,
across up to ~18 modulated delay/allpass instances at 5 active lines.

**Fix**: changed the literal to `1.0f` in both files, selecting the float-only `fmodf`
overload with no promotion/narrowing.

## 6. Fix undefined behavior in destructors mismatching `placement new`/`delete`

**Files**: `CloudSeed/ModulatedDelay.h`, `CloudSeed/ModulatedAllpass.h`,
`CloudSeed/MultitapDiffuser.h`, `CloudSeed/ReverbChannel.h`, `CloudSeed/DelayLine.h`

The custom SDRAM allocator (`src/sdram_pool.cpp`'s `custom_pool_allocate`) is a
bump allocator with no free function — by design, since the `ReverbController` is
constructed once and lives for the process lifetime. Most buffers are correctly
constructed with placement `new (custom_pool_allocate(...)) T[...]` into this pool, but
five destructors called plain `delete`/`delete[]` on those placement-new'd pointers
anyway — undefined behavior (can corrupt the pool's bump-allocation bookkeeping), latent
because these destructors are never invoked in the shipped firmware but a real bug for any
future code path that does destroy one of these objects.

**Fix**:
- `ModulatedDelay`, `ModulatedAllpass`, `MultitapDiffuser`: destructors no longer call
  `delete` on their pool-backed buffers (replaced with an explanatory comment).
- `ReverbChannel`: destructor now explicitly placement-destroys each `DelayLine`
  (`line->~DelayLine()`) instead of `delete line`, and no longer deletes the pool-backed
  `tempBuffer`/`lineOutBuffer`/`outBuffer`.
- `DelayLine`: `tempBuffer`/`mixedBuffer`/`filterOutputBuffer` are the one exception — they
  are real heap allocations (`new float[bufferSize]`, not pool-backed). The bug there was
  `delete` (singular) against a `new[]` allocation; fixed to `delete[]` to match.

`AllpassDiffuser.h`'s destructor (`delete filter` on plain `new ModulatedAllpass(...)`
pointers) was already correctly paired and left untouched.

## 7. Bump the application build to `-O3` + `-ffast-math`

**Files**: `./Makefile`, `CloudSeed/Makefile`

`src/cloudseed.cpp` built through `libdaisy/core/Makefile`'s default
`OPT ?= -O2`, one level below the `-O3` the `CloudSeed/` library itself already used.

**Fix**: `./Makefile` now sets `OPT = -O3` before including
`$(SYSTEM_FILES_DIR)/Makefile` (must precede the include since it uses a weak `?=`
default), and appends `CPPFLAGS += -ffast-math` after the include. `CloudSeed/Makefile`
adds `-ffast-math` to its existing `CPPFLAGS` list alongside `-fno-exceptions`, so the
library and application compile with matching floating-point semantics. This lets GCC
fuse the many multiply-accumulate patterns in `Biquad::Process`, `DelayLine::Process`'s
feedback mix, and the allpass/multitap taps into single-cycle Cortex-M7 VFMA instructions,
and drops `errno`-setting overhead from `pow`/`sqrt`/`log10` calls.

**Verified**: build succeeds with no errors; `.text`+`.data` = 140000 bytes (28.48% of the
480KB `BOOT_SRAM` region), comfortably within budget.

## 8. Remove redundant bypass-buffer copy in the audio callback

**File**: `src/cloudseed.cpp`

`audioBypassBuffer[AUDIO_BUFFER_SIZE]` existed purely to echo the input back out when
bypassed, filled by a full per-sample copy loop every block regardless of bypass state.
Since `in[0][i]` remains valid for the whole callback, the buffer and its copy were
unnecessary.

**Fix**: deleted `audioBypassBuffer` and its copy loop; both bypass output paths
(`state.bypass` branch and the preset-change-in-progress branch) now write
`out[0][i] = in[0][i];` directly.

## 9. Allocation-free parameter updates (seed series cached)

**Files**: `CloudSeed/AudioLib/ShaRandom.h`, `CloudSeed/AudioLib/ShaRandom.cpp`,
`CloudSeed/Utils/Sha256.h`, `CloudSeed/Utils/Sha256.cpp`, `CloudSeed/MultitapDiffuser.h`,
`CloudSeed/MultitapDiffuser.cpp`, `CloudSeed/AllpassDiffuser.h`, `CloudSeed/ReverbChannel.h`;
test `tests/engine_alloc_test.cpp`

Knob and toggle targets are applied inside the audio callback, and 17 of the 47 parameters (the count at the time)
allocated on the heap when written. `ShaRandom::Generate()` built its result from
`std::vector`s, and each `sha256()` call returned a new one. `ReverbChannel::UpdateLines()`
regenerated the delay-line seed series (4 SHA-256 digests) on every `LineDelay`, `LineDecay`,
`LineModAmount`, `LineModRate` and `LateDiffusionMod*` write, although the series depends only
on `DelaySeed` and `CrossSeed`. `MultitapDiffuser::Update()` built three temporary vectors for
every `TapCount`, `TapLength`, `TapGain`, `TapDecay` and `Bloom` write, and `Process()`
copy-assigned them. Measured on the host with counting allocators, over six writes each:
`LineDecay` made 210 heap allocations, `TapDecay` 18, and `CrossSeed` 2508 (it re-hashed every
stage's series, and each `DelayLine` diffuser hashed twice via `SetSeed` then `SetCrossSeed`).

**Fix**:
- `sha256()` writes into a caller buffer, and `ShaRandom::Generate(seed, out, count)` fills a
  caller array. The algorithm is unchanged: each digest hashes the first 8 bytes of the
  previous one.
- New `AudioLib::SeedSeries<N>` (`ShaRandom.h`) holds the series of `seed` and of `~seed` in
  fixed arrays plus their cross-seed blend. It re-hashes only when the seed actually changes,
  and a `CrossSeed` write only re-blends. `MultitapDiffuser` (`2 * MaxTaps` values),
  `AllpassDiffuser` (`MaxStageCount * 3`) and `ReverbChannel` (`TotalLineCount * 3`, the line
  seeds) each own one. `UpdateLines()` reads the cached line seeds and never hashes.
- `MultitapDiffuser`'s tap gains, positions, their `*Temp` copies and `tapData` are
  `MaxTaps`-sized arrays. `Update()` clamps the tap count to 1..`MaxTaps`, a limit
  `GetScaledParameter` already keeps.

**Result**: `tests/engine_alloc_test.cpp` (part of `make test`) sweeps every parameter
through `SetParameter()` and `Process()` after boot and requires zero heap and zero pool
allocations; it failed for 17 parameters before this change. The output is unchanged:
rendering all ten presets while sweeping the seeds, `CrossSeed`, `LineDecay`, `TapDecay`,
`TapCount`, `LineModAmount` and `Bloom` is byte-identical to the old engine at host
`-O2`, and at `-O3 -ffast-math -mfma -fno-tree-vectorize`. With the host auto-vectorizer on, the
two differ only by float reassociation, at most 92 dB below peak; the Cortex-M7's
scalar-only FPU gives GCC nothing to vectorize with. Code size: with `MultitapDiffuser` all in
its header, `-O3` inlined the smaller `MultitapDiffuser::Update()` into each of its
`SetParameter` cases (`ReverbChannel::SetParameter` 8,128 → 16,416 B, +7.5 KB of SRAM). The
class is now split into `MultitapDiffuser.h` / `MultitapDiffuser.cpp`, so `Update()` is one
664 B out-of-line function: `SetParameter` is 12,042 B and SRAM is 4,312 B smaller than with
the header-only version. The render is byte-identical across the split at both host flag sets
above.

## 10. Engine state defined before its first read

**Files**: `CloudSeed/ReverbChannel.h`, `CloudSeed/DelayLine.h`, `CloudSeed/AllpassDiffuser.h`

Several engine members were read before anything had written them. The `ReverbController` is
heap-allocated and each `DelayLine` is placement-new'd into the SDRAM pool, whose
`.sdram_bss` section is `NOLOAD` and never zeroed (`libdaisy/core/STM32H750IB_sram.lds:169`),
so those reads returned whatever the memory held:
- `SetParameter(DiffusionEnabled)` and `SetParameter(LateDiffusionEnabled)` compare the new
  value with the old `diffuserEnabled` / `DelayLine::DiffuserEnabled` to decide whether to clear
  the diffuser (`CloudSeed/ReverbChannel.h:143`, `:182`); the preset load's first write compared
  against garbage.
- `AllpassDiffuser`'s constructor called `Update()`, which reads `delay`, and
  `SetSamplerate()` → `SetModRate(modRate)` before either was set. Built over memory filled
  with `0xBE` at host `-O0`, its two stages got `SampleDelay` −932,195,305 and −519,973,246,
  which would index far outside the 19,200-sample allpass buffer if a block ran before the
  preset load.

**Fix**: the constructors give these members a defined starting value:
`CloudSeed/ReverbChannel.h:64-73`, `CloudSeed/DelayLine.h:40-47`, and
`CloudSeed/AllpassDiffuser.h:35-37` (`delay = 100`, matching the stage default
`ModulatedAllpass` is built with). The `modRate` member and the constructor's
`SetModRate()` call were later removed (§13): the stages already start
at `ModRate = 0`. The line-seed series that `UpdateLines()` reads before `DelaySeed` is applied
is zero-initialised by `SeedSeries` (§9).

**Result**: the preset load still overwrites every one of these before audio starts, so the
sound is unchanged; the same `0xBE` check now yields `SampleDelay` 85 and 47, `ModRate` 0 at
`-O0` and `-O1`.

## 11. USB-MIDI preset link: no idle cost

**Files**: `src/usb_midi_link.cpp`, `src/preset_protocol.cpp`, `src/cloudseed.cpp`
(`serviceUsbMidi()`, the callback's passthrough gate)

The preset upload link (docs/USB_MIDI.md) keeps USB-MIDI running at every boot, next to the
audio callback. The USB OTG FS interrupt shares priority 0 with the audio DMA interrupts
(`libdaisy/src/usbd/usbd_conf.c:102-107`, `libdaisy/src/sys/dma.c:17-50`), so time spent in it
delays the callback directly.

**Fix**: the design keeps the audio path's cost at one flag test:
- The stack raises no interrupts while the link is idle or unplugged: SOF interrupts are off
  and VBUS sensing is on (`libdaisy/src/usbd/usbd_conf.c:420,424`).
- The receive callback (USB ISR) only feeds bytes to `MidiInQueue::Feed` (byte framing into an
  SPSC ring, no allocation). Decoding, replies, MIDI live-control reports, validation (a full
  TOML parse) and QSPI writes run in the main loop; sends are queued and handed to the USB
  stack without busy-waiting.
- MIDI live-control writes (docs/MIDI.md) reach the callback through `ControlMailbox`: the
  callback drains at most 8 pending writes per block, each a bounded `SetParameter`.
- During an upload session the callback passes audio through
  (`state.presetChangeInProgress || state.uploadActive`), so COMMIT's parse and flash write
  never compete with the reverb.
- Outside a session, INFO and READ traffic costs the callback only the interrupt time to
  receive a USB packet.

**Result**: the audio callback compiles to the same 33 calls as before, with one added `ldrb`
for the gate (`arm-none-eabi-objdump` of `audioCallback` before and after, registers
normalised). Memory cost, from the linker report:
- DTCMRAM +15,136 B, mostly libdaisy's USB device stack: `UserRx/TxBufferFS/HS` 4 × 2 KB,
  `midi_usb_handle` 2,112 B, `hpcd_USB_OTG_FS` 1,292 B.
- SRAM +10,972 B.
- SDRAM +627,424 B: the 512 KB parse arena, which is now separate from `custom_pool`, the
  96 KiB upload buffer, and the 4,804 B validation `PresetBank`.

`.sdram_bss` is never zeroed, so the SDRAM buffers add no boot time. The USB init adds about
10 ms (`libdaisy/src/hid/usb_midi.cpp:125`).

## 12. Engine audit (2026-11)

**Files**: everything under `CloudSeed/`, `src/board.h`, `src/cloudseed.cpp` (main loop),
`Makefile`, `CloudSeed/Makefile`; new test `tests/allpass_clear_test.cpp`

An audit of the engine for M7 practice. It drew on the M7 findings in dp4's
`daisy/README.md` "Performance (real time)" items 6-7. The firmware disassembly showed
per-sample double-precision math in `ReverbChannel::Process` and `AllpassDiffuser::Process`, member
reloads and stores in every hot loop, an SDRAM sweep inside the audio callback, and buffers
sized far beyond what is read. No hardware was available for A/B load measurements, so every
object stays in its memory region; placement moves are listed as candidates below.

Method: a throwaway host harness rendered the engine through `ReverbController` on memory
filled with `0x5A`, so any read of an unwritten slot changes the output. The scenarios were:
- all ten presets: noise, an impulse, then the tail;
- a sweep of every parameter;
- all parameters at 0 and at 1;
- `ReverseDelay` at three window lengths.

Steps 1-4 below rendered byte-identical to the pre-audit tree. Steps 5-6 were allowed
inaudible changes and were compared against the previous step.

Two of the planned scenarios turned out to test little. The all-0 run is silent, because
every output level is 0. In the all-1 run, the 1 s pre-delay keeps the reverb out of the
rendered second, so only the dry signal is heard. Two audible variants were added afterwards:
all-0 with the four output levels at 1, and all-1 with no pre-delay. They were compared
pre-audit -> final only. Both are finite, with |RMS delta| < 0.0001 dB and |peak delta| <=
0.0003 dB; the residuals are -117.8 dB and -58.2 dB.

1. **Dead code and header hygiene** (byte-identical):
   - Removed `Default.h`, `Utils::BitcrushAndReduce` / `Gain2DB`, the unused `MathDefs`
     macros, and the four never-read `ValueTables` (`Response2Oct`, `5Oct`, `6Oct`, `4Dec`).
     The remaining tables are 4001 entries, the index range `Get()` reads, instead of 40001.
   - Removed the whole `SetSamplerate` / `GetSamplerate` chain and `GetParameterCount`.
   - `Biquad` is reduced to the two shelves `DelayLine` uses.
   - Removed `ModulatedAllpass::Get`, `ReverbChannel::GetLineOutput` + `lineOutBuffer`
     (written every block, never read), the `ChannelLR` / `crossSeed` stereo remnants (always
     Left / 0), the self-includes, and `using namespace` in headers.
   - The `CrossSeed` case keeps its `UpdateLines()` call (`CloudSeed/ReverbChannel.h:244-252`).
     The audit plan assumed it recomputed unchanged values, but it does not: the line
     diffusers' mod amount and rate are scaled by their seeds, which `PostDiffusionSeed`
     changes without re-applying them. `LoadPreset()` writes `CrossSeed` after
     `PostDiffusionSeed`, so this call is what applies them; removing it changed eight of the
     ten presets.
2. **Buffer wiring** (byte-identical):
   - `ModulatedAllpass` takes its ring length from `AllpassDiffuser` (`CloudSeed/AllpassDiffuser.h:27-34`):
     `2 * (MaxDelayMs * samplerate / 1000) + 2` = 9602 samples. Before, the ring was 19200 and
     the 150 ms size, 7200, was passed as the *output block* length, where 48 are used.
   - `MultitapDiffuser::output` is one block (48), not 48000.
   - `AllpassDiffuser::filters` and `ReverbChannel::lines` are fixed arrays, so no
     `std::vector` code is linked.
   - `Feedback`, `ModulationEnabled`, `DelayLine::feedback` and the four output levels are
     defined before the preset load.
3. **Lazy allpass clear** (byte-identical):
   - `SetParameter(DiffusionEnabled / LateDiffusionEnabled)` runs in the audio callback when a
     knob, toggle or MIDI write flips it, and called `ModulatedAllpass::ClearBuffers()`, which
     zeroed the whole ring. For a late-diffusion flip that was about 1 MB of SDRAM writes.
   - `ClearBuffers()` now sets a `written` count to 0. Until the ring has been rewritten (~200 ms),
     `Process()` runs an out-of-line `ProcessFresh()` that reads every slot not yet written as
     0 (`CloudSeed/ModulatedAllpass.h:73-140`).
   - `tests/allpass_clear_test.cpp` checks this against an allpass with a truly zeroed ring.
     With modulation off, on with interpolation, and on without, it matches sample for
     sample. The test fails if stale samples are replayed.
4. **Hot loops** (byte-identical):
   - `ModulatedDelay::Process` and `ModulatedAllpass::ProcessWithMod` run in segments between
     `Update()` calls, with indices, gains and feedback in locals. `ProcessNoMod`, `Biquad` and
     `MultitapDiffuser::Process` use locals too: the float stores can alias the members, so the
     compiler reloaded them every sample. The multitap tap loop now has no store
     (`vfma` into a register, one store per sample).
   - `MultitapDiffuser`'s `*Temp` double buffer and `isDirty` are gone: every `SetParameter`
     runs in the callback or behind its passthrough gate, never during `Process()`.
   - The `tempBuffer` copies are gone from `DelayLine`, `ReverbChannel` and
     `ReverbController`; the stages read each other's outputs directly.
5. **Inaudible removals**:
   - The -90 dBFS input gate (`n * n < 1e-9`) and the `Lp1` / `Hp1` zero-snap are removed;
     flush-to-zero (§1) covers denormals.
   - `Hp1` is now `Lp1` plus a subtraction (`CloudSeed/AudioLib/Hp1.h`). Its state is cleared
     by `ReverbChannel::ClearBuffers()`, which before cleared the wrong field
     (`highPass.Output`).
   - With both input filters off, nothing is copied.
   - Against step 4: every scenario finite, |RMS delta| <= 0.0001 dB, |peak delta| <= 0.0001
     dB, residual -85.8 dB or lower.
6. **Float-only continuous math**:
   - Gains, coefficients, modulation depth and rate, and the LFO lookup are float:
     `GetScaledParameter`, `UpdateLines`, `AllpassDiffuser::SetMod*`, `Lp1::Update` (rewritten
     with `1 - cos w = 2 sin^2(w/2)` so float keeps precision at 20 Hz), `Biquad::Update`, and
     `Utils::DB2gain`.
   - The per-line gain is folded into the mix (`Utils::Gain` removed).
   - Integer sample counts, seeds and stage counts keep upstream's double arithmetic, written
     as explicit `(double)` casts, so they come out unchanged.
   - `FastSin::Get` masks its index (power-of-two table), and `FastSin::Step` /
     `RandomPhase` replace the phase-advance + `fmod` duplicated in `ModulatedDelay` and
     `ModulatedAllpass`.
   - `ReverseDelay::Window` reads the quarter wave from the `FastSin` table instead of a
     per-sample `std::sin` and division.
   - `-Wdouble-promotion` is on in `CloudSeed/Makefile` (`-Werror`), and as an error in the
     `engine_alloc_test` build, which compiles every engine header.
   - Against step 5: every scenario finite, |RMS delta| <= 0.0002 dB, |peak delta| <= 0.0005
     dB. The residual is -45.7 dB at worst (preset 1, Chorus): its modulated delays drift in
     phase, while the level and spectrum stay the same.

HotHouse: `keepCoreBusy()` now runs on the Terrarium only (`kKeepCoreBusy`, `src/board.h`).
The HotHouse image contains no `gBusyPhase` / `gBusySink`. Both boards keep running the reverb
while bypassed.

**Results**:

Linker report, before -> after:

|Region|Terrarium before|Terrarium after|HotHouse before|HotHouse after|
|---|---|---|---|---|
|SRAM|234,232 B|230,008 B|234,376 B|230,056 B|
|DTCMRAM|53,884 B|53,884 B|53,956 B|53,948 B|
|SDRAM|53,138,368 B|51,922,352 B|53,138,368 B|51,922,352 B|

The SDRAM line drops by the 1,216,016 B of unused `ValueTables`. `custom_pool` is a fixed
48 MB reservation, so the pool saving does not show in that report. Measured on the host
(x86-64; 32-bit ARM is slightly smaller): the engine's pool use went from 3,767,096 B to
2,770,544 B, and its heap use, including the D2-heap line buffers, from 9,136 B to 7,088 B.

Double-precision and libm instructions per function (`arm-none-eabi-objdump` of
`build/terrarium/cloudseed.elf`, counting `.f64`, `__aeabi_d*` and calls to `pow` / `sin` /
`cos` / `fmod` / `sqrt` ...):

|Function|Before|After|
|---|---|---|
|`ReverbChannel::Process`|23|0|
|`AllpassDiffuser::Process`|12|0|
|`MultitapDiffuser::ProcessScatter`|3|0|
|`ReverbChannel::SetParameter`|1018|492|
|`ReverbController::GetScaledParameter`|17|11|
|`MultitapDiffuser::Update`|30|9|
|`Biquad` ctor / `Update` / `SetGainDb` / `SetSamplerate`|19|0|
|`ReverbChannel` constructor (boot)|191|39|
|`ValueTables::Init` (boot)|46|25|

The 492 left in `SetParameter` are the explicit integer sites: `Ms2Samples`, the line delay
and its clamp in `UpdateLines`, and `AllpassDiffuser::Update`'s `pow(10, seed)`. These are
inlined into each case that calls them. No audio-rate function has any.

**Measured on the pedal** (HotHouse, 400 MHz, 2026-11-09). A is the same working tree with the
pre-audit engine (`CloudSeed/` from HEAD, `keepCoreBusy()` on); B is the audited tree. Both
were flashed over DFU and driven over USB-MIDI by `tools/usb_preset_host.py`. Each value is
one `LOAD` window (1 s), taken after the preset or setting had applied:

|Preset (default lines)|A avg / max|B avg / max|
|---|---|---|
|1 Chorus|23.7 / 31.4 %|20.1 / 26.7 %|
|2 Dull Echos|19.9 / 26.2 %|16.3 / 22.1 %|
|3 Hyperplane|32.0 / 39.5 %|26.9 / 33.7 %|
|4 Medium Space|36.9 / 46.9 %|29.3 / 37.9 %|
|5 Noise in the Hallway|43.8 / 57.2 %|37.0 / 43.9 %|
|6 Rubi Ka Fields|35.9 / 44.5 %|30.3 / 37.3 %|
|7 Small Room|27.4 / 35.9 %|23.7 / 31.2 %|
|8 90s Are Back|11.0 / 17.6 %|9.1 / 17.8 %|
|9 Through the Looking Glass|56.4 / 66.8 %|45.7 / 53.8 %|
|10 Dark Plate|20.4 / 26.9 %|17.1 / 24.0 %|
|9 at max (4 lines)|61.2 / 89.8 %|58.8 / 70.6 %|

Over the ten presets the average load is 5.2 points lower (17 % relative), and the peaks are
lower too. Enable flips sent over MIDI (NRPN 45 / 26, the same `SetParameter` path as a
toggle), ten per second, Chorus at 5 lines:

|Flip|A max / overruns per window|B max / overruns|
|---|---|---|
|`LateDiffusionEnabled`|558-590 % / 1-9|39-44 % / 0|
|`DiffusionEnabled`|132-152 % / 1-8|42-43 % / 0|

Before the audit a late-diffusion flip stalled the callback for about 5.6 ms (five or six lost
blocks); after it, a flip costs nothing measurable. Per-stage cost on B (Chorus, steady
windows): one more line about 4.6 points (15.8 % at 1 line, 34.3 % at 5); 1-stage late
diffusion about 2.3 points per line; each early tap about 0.6-0.75 points (the tap reads
stream from SDRAM); 1 line, 1 tap and no early diffusion 6.0 %.

**Identified, not changed**:
- A knob mapped to a seed re-hashes its series (SHA-256) on every write: `TapSeed` costs 50
  compressions per block while the knob moves.
- Upstream's integer division `count / MaxTaps` in the multitap gain
  (`CloudSeed/MultitapDiffuser.cpp:156-158`): fixing it would change every preset's early level.
- `InputMix` is stored but does nothing; removing it touches the parser, the MIDI table and
  the editor.
- Placement candidates that need a LOAD A/B on hardware:
  - engine control state and the 48-float scratch buffers from the SDRAM pool / D2 heap into
    DTCM;
  - ITCM for `audioCallback`, `ReverbChannel::Process`, `AllpassDiffuser::Process` and
    `MultitapDiffuser::Process*` (~12.5 KB), via an app `LDSCRIPT`
    (`libdaisy/core/Makefile:231` honours an override);
  - the SDRAM MPU region 1 attribute: it is TEX0 C1 B1, write-back no-write-allocate
    (`libdaisy/src/sys/system.cpp`);
  - libdaisy's `-fno-move-loop-invariants`.
- Rings still larger than needed: the `DelayLine` 2 s ring (1.5 s + modulation needed) and the
  multitap 1 s ring (0.5 s needed).
- The 48 MB `custom_pool` reservation against ~3 MB used.
- No stack guard, no null-page MPU region, and an unbounded `_sbrk`.

Verification: `make test` (now with `allpass_clear_test`), `make` and `make BOARD=hothouse`
link with no warnings from `src/` or `CloudSeed/`, and `make presets-check` passes. On the
pedal: the tables above, and docs/HARDWARE_TESTS.md H18.

## 13. Dead-code sweep (2026-11)

**Files**: `CloudSeed/` (`ReverbChannel.h`, `ReverbController.h`, `AllpassDiffuser.h`,
`DelayLine.h`, `ModulatedAllpass.h`, `ModulatedDelay.h`, `ReverseDelay.h`, `Utils.h`,
`FastSin.cpp`, `AudioLib/ValueTables.*`, `Makefile`), `src/` (`preset_bank.*`,
`preset_protocol.cpp`, `knob_bank.h`, `footswitch_gestures.h`, `cloudseed.cpp`,
`pedal_storage.*`, `sdram_pool.cpp`), `tests/preset_bank_test.cpp`,
`tests/preset_protocol_test.cpp`

**Bug fixed**: `CloudSeed/Makefile` generated its dependency files with
`-MT"$(@:%.o=%.d)"`, so each `.d` named itself instead of the object as the target, and a
header edit (e.g. `MultitapDiffuser.h`) did not rebuild the library objects that include it.
The app, which includes the same headers, could then disagree with `libcloudseed.a` about an
object's layout. The `-MT` is gone; `.d` files now name the `.o`.

**Removed** (each verified unused by a repo-wide search):
- Engine: `ReverbChannel::bufferSize`, `postDiffusionSeed` and `UpdatePostDiffusion()` (the
  `PostDiffusionSeed` case seeds the lines itself), the `tempBuffer` clear in `ClearBuffers()`
  (`Process()` writes it before reading), the constructor's redundant
  `SetInterpolationEnabled(true)`; `AllpassDiffuser::modRate` and its constructor
  `SetModRate()` call; the `Biquad::Slope = 1` writes in `DelayLine` (the constructor's
  default); two `ReverseDelay` clamps that cannot fire (`Xf > W/2`, `hop < 1`); an unreachable
  `return` in `GetScaledParameter()`; unused includes; the library Makefile's C/assembler
  rules, tools and `C_DEFS`. `ModulatedAllpass::ModulationUpdateRate` is now private
- Firmware: `PresetBank::board` and `PresetData::name` (written by the parser, read only by
  tests; `gPresets` and `gUploadCheck` shrink from 5,000 to 4,484 B each), an unreachable
  `!primed` test and a duplicated park sequence in `KnobBank` (now `park()`), a duplicated
  Identity Request / header test in `PresetProtocol`, a redundant COMMIT FlashError check
  in `preset_protocol_test` (`testFlashFailures()` covers it), unused includes in
  `sdram_pool.cpp`. Every control write in `src/cloudseed.cpp` (including the filter
  auto-enable and the HotHouse `early.envelope` target) now goes through `setParameter()`

**Identified, not changed**: `ModulatedAllpass::Update()` permanently shrinks the public
`ModAmount` once it reaches `SampleDelay`, so turning the diffusion delay down and back up
leaves the modulation reduced until `ModAmount` is written again (upstream behaviour; fixing
it changes the sound). The `Interpolation` parameter reaches only the late-line diffusers.

**Verification**: a throwaway host harness rendered six random presets (3,000 blocks each,
random parameter writes, a mid-run `ClearBuffers()`, the reverse voice) through the engine
at `HEAD` and after the sweep, with the SDRAM-pool stand-in filled with `0xAB`: the FNV-1a of
every output sample is identical (`d46b8b252f5c77d0`). `make test`, `make presets-check`,
`make` and `make BOARD=hothouse` pass.

## Verification performed

1. **Build** (at the time of §1-§8; current memory figures are in CLAUDE.md, "Memory
   Usage"): `make libs && make clean && make` completes with no errors and produces
   `build/cloudseed.bin`/`.elf`/`.hex` (now `build/<board>/`). Memory usage then: SDRAM 77.10% of 64MB, SRAM
   (`.text`+`.data`) 28.48% of 480KB `BOOT_SRAM`.
2. **Host-side behavioral smoke test**: a throwaway harness linked `ReverbController`
   against a `malloc`-backed stand-in for `custom_pool_allocate`, drove `LineCount`
   through 1-5, and processed an impulse-then-silence buffer through the pre-fix
   (steps 2-5 reverted) and post-fix code. Output was bit-identical across all 480
   samples, exit code 0, no NaN/Inf in either build.
3. **USB-MIDI link (§11)**:
   - `make test` runs `preset_protocol_test`, which covers framing, USB-MIDI packing, sequencing, errors and timeouts.
   - A throwaway host simulator drove the protocol module and the real preset parser through a raw pty with a stdlib-only Python host. It uploaded presets.toml with one preset changed, read it back byte-identical, got the parser's message for a `blinks = 99` file, timed out a lone BEGIN after 5 s, and reverted.
   - The before/after callback disassembly comparison is described in §11.
