// ModulatedAllpass::ClearBuffers() does not zero the delay ring (a diffusion enable
// flip calls it from the audio callback); instead every slot not rewritten since the
// clear must read as 0. Each case fills the ring with noise, clears, and then must
// match an allpass whose ring really was zeroed, sample for sample, until well past
// the point where the whole ring has been rewritten.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "check.h"
#include "CloudSeed/FastSin.h"
#include "CloudSeed/ModulatedAllpass.h"

// The firmware's SDRAM pool is never zeroed: hand out garbage, not zeros.
void* custom_pool_allocate(size_t size)
{
    void* p = malloc(size);
    memset(p, 0x5A, size);
    return p;
}

static uint32_t gSeed = 12345;
static float Noise()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return (float)(int32_t)gSeed * (0.25f / 2147483648.0f);
}

static const int kBlock = 48;
static const int kRing = 9602;
static const int kDelay = 300;
static const float kFeedback = 0.6f;

static bool MatchesZeroedAllpass(bool modulation, bool interpolation)
{
    CloudSeed::ModulatedAllpass ap(kBlock, kRing, kDelay);
    ap.Feedback = kFeedback;
    ap.ModAmount = 0.0f;  // Update(): delayA = kDelay, gainA = 1, gainB = 0
    ap.ModRate = 0.0f;
    ap.ModulationEnabled = modulation;
    ap.InterpolationEnabled = interpolation;

    float in[kBlock];
    for (int b = 0; b < 50; b++)  // stale content the clear must hide
    {
        for (float& x : in)
            x = Noise();
        ap.Process(in, kBlock);
    }
    ap.ClearBuffers();

    std::vector<float> ring(kRing, 0.0f);
    int w = 0;
    bool same = true;
    for (int b = 0; b < 300; b++)  // 14400 samples, past the 9602-sample fresh window
    {
        for (float& x : in)
            x = Noise();
        ap.Process(in, kBlock);
        const float* out = ap.GetOutput();
        for (int i = 0; i < kBlock; i++)
        {
            const float d = ring[(w - kDelay + kRing) % kRing];
            const float v = in[i] + d * kFeedback;
            ring[w] = v;
            const float y = d - v * kFeedback;
            w = (w + 1) % kRing;
            if (out[i] != y)
                same = false;
        }
    }
    return same;
}

int main()
{
    CloudSeed::FastSin::Init();
    CHECK(MatchesZeroedAllpass(false, false));
    CHECK(MatchesZeroedAllpass(true, true));
    CHECK(MatchesZeroedAllpass(true, false));
    return CheckSummary("allpass_clear_test");
}
