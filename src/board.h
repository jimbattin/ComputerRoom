#ifndef BOARD_H
#define BOARD_H

// The pedal hardware this firmware is built for, selected at compile time by the
// Makefile (BOARD=terrarium, the default, or BOARD=hothouse). Both boards are a Daisy
// Seed with six knobs on D16-D21, footswitches on D25/D26 and LEDs on D22/D23; they
// differ only in their toggle switches:
//   Terrarium (PedalPCB): four on/on switches, SWITCH_1..SWITCH_4 = D10, D9, D8, D7.
//     A grounded pin is Up.
//   HotHouse (Cleveland Music Co.): three on/off/on toggles with two pins each,
//     toggle 1 = D9 (up) / D10 (down), toggle 2 = D7 / D8, toggle 3 = D5 / D6.
//     Up pin grounded = Up, down pin grounded = Down, neither = Middle.
// Audio is mono on both: the left input is processed and the result is written to
// both outputs.
//
// Built directly on DaisySeed: libdaisy's DaisyPetal has no D5/D6 switch and also
// starts an encoder, an expression ADC channel and an I2C LED driver neither board has.

#include "daisy_seed.h"
#include "preset_bank.h"

#if defined(CLOUDSEED_BOARD_HOTHOUSE) == defined(CLOUDSEED_BOARD_TERRARIUM)
#error "define exactly one of CLOUDSEED_BOARD_HOTHOUSE, CLOUDSEED_BOARD_TERRARIUM (Makefile BOARD=)"
#endif

#if defined(CLOUDSEED_BOARD_HOTHOUSE)
constexpr Board kBoard = Board_HotHouse;
#else
constexpr Board kBoard = Board_Terrarium;
#endif

constexpr int kBoardToggles = ToggleCount(kBoard);

// Main-loop busy work (keepCoreBusy() in cloudseed.cpp) that reduces a 1 kHz whine on the
// Terrarium. Off on the HotHouse: its main loop spins without the artificial FPU/memory load.
constexpr bool kKeepCoreBusy = kBoard == Board_Terrarium;

// Namespace-scope constexpr, not static members: C++14 has no inline variables, so an
// odr-used static constexpr member would need an out-of-line definition.
constexpr daisy::Pin kLed1Pin = daisy::seed::D22;  // active indicator
constexpr daisy::Pin kLed2Pin = daisy::seed::D23;  // preset indicator

#if defined(CLOUDSEED_BOARD_HOTHOUSE)
// Up, down pin of each toggle.
constexpr int        kSwitchPins = 6;
constexpr daisy::Pin kSwitchPinTable[kSwitchPins] = {
    daisy::seed::D9, daisy::seed::D10, daisy::seed::D7,
    daisy::seed::D8, daisy::seed::D5,  daisy::seed::D6};
#else
// SWITCH_1..SWITCH_4.
constexpr int        kSwitchPins = 4;
constexpr daisy::Pin kSwitchPinTable[kSwitchPins] = {
    daisy::seed::D10, daisy::seed::D9, daisy::seed::D8, daisy::seed::D7};
#endif

class PedalBoard
{
public:

    daisy::DaisySeed     seed;
    daisy::AnalogControl knob[kKnobCount];  // knob1..knob6, presets.toml order

    // Clocks, SDRAM, QSPI, audio at `blockSize` samples, the six knobs and every
    // switch. The block size is set before the knobs are initialised, so their
    // one-poles run at the real callback rate.
    void Init(size_t blockSize)
    {
        seed.Configure();
        seed.Init(false);  // 400 MHz, as DaisyPetal::Init() ran it; true would boost to 480 MHz
        seed.SetAudioBlockSize(blockSize);

        const daisy::Pin kKnobPins[kKnobCount] = {
            daisy::seed::D16, daisy::seed::D17, daisy::seed::D18,
            daisy::seed::D19, daisy::seed::D20, daisy::seed::D21};
        daisy::AdcChannelConfig cfg[kKnobCount];
        for (int i = 0; i < kKnobCount; i++)
            cfg[i].InitSingle(kKnobPins[i]);
        seed.adc.Init(cfg, kKnobCount);
        for (int i = 0; i < kKnobCount; i++)
            knob[i].Init(seed.adc.GetPtr(i), seed.AudioCallbackRate());

        footswitch_[0].Init(daisy::seed::D25);
        footswitch_[1].Init(daisy::seed::D26);
        for (int i = 0; i < kSwitchPins; i++)
            switch_[i].Init(kSwitchPinTable[i]);
    }

    // Once per audio block.
    void ProcessAnalogControls()
    {
        for (int i = 0; i < kKnobCount; i++)
            knob[i].Process();
    }

    void ProcessDigitalControls()
    {
        footswitch_[0].Debounce();
        footswitch_[1].Debounce();
        for (int i = 0; i < kSwitchPins; i++)
            switch_[i].Debounce();
    }

    // 0 = FS1 (bypass), 1 = FS2 (preset).
    const daisy::Switch& Footswitch(int f) const { return footswitch_[f]; }

    // Position of toggle `t` (presets.toml order, 0..kBoardToggles-1): kToggleDown,
    // kToggleMiddle (HotHouse only) or kToggleUp.
    uint8_t TogglePosition(int t) const
    {
#if defined(CLOUDSEED_BOARD_HOTHOUSE)
        if (switch_[2 * t].Pressed())
            return kToggleUp;
        return switch_[2 * t + 1].Pressed() ? kToggleDown : kToggleMiddle;
#else
        return switch_[t].Pressed() ? kToggleUp : kToggleDown;
#endif
    }

    size_t AudioBlockSize() { return seed.AudioBlockSize(); }
    float  AudioSampleRate() { return seed.AudioSampleRate(); }
    void   StartAdc() { seed.adc.Start(); }
    void   StartAudio(daisy::AudioHandle::AudioCallback cb) { seed.StartAudio(cb); }

private:
    daisy::Switch footswitch_[2];
    daisy::Switch switch_[kSwitchPins];
};

#endif
