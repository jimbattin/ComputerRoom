#ifndef TOGGLE_BANK_H
#define TOGGLE_BANK_H

#include "preset_bank.h"

struct ToggleWrite
{
    int     toggle;    // 0..kMaxToggles-1, presets.toml toggle order
    uint8_t position;  // kToggleDown / kToggleMiddle / kToggleUp
};

// Parked toggle takeover, the lever counterpart of KnobBank. Reset() snapshots every
// lever and parks it: power-up, a preset load, and entering or leaving the secondary
// bank never change a target on their own, so a loaded preset keeps its stored toggle
// values whatever the levers say. A lever that differs from its snapshot takes over
// and writes its position; from then on it writes on every change. No glide: every
// toggle target has two or three discrete states. A board with fewer than
// kMaxToggles toggles passes a constant position for the missing ones, which
// therefore never write.
class ToggleBank
{
public:
    // One callback's worth of toggle handling. Re-parks when the bank changed, when
    // `forceReset` is set (a preset was loaded), or on the first call; a re-parking
    // call returns 0. Otherwise fills `writes` (capacity kMaxToggles), returns how many.
    int Scan(int bank, bool forceReset, const uint8_t* positions, ToggleWrite* writes)
    {
        if (!primed || forceReset || bank != activeBank)
        {
            Reset(bank, positions);
            return 0;
        }
        int n = 0;
        for (int i = 0; i < kMaxToggles; i++)
        {
            if (positions[i] != last[i])
            {
                last[i]            = positions[i];
                writes[n].toggle   = i;
                writes[n].position = positions[i];
                n++;
            }
        }
        return n;
    }

private:
    void Reset(int bank, const uint8_t* positions)
    {
        for (int i = 0; i < kMaxToggles; i++)
            last[i] = positions[i];
        activeBank = bank;
        primed     = true;
    }

    uint8_t last[kMaxToggles] = {};  // lever positions at the last Scan()
    int     activeBank        = 0;
    bool    primed            = false;
};

#endif
