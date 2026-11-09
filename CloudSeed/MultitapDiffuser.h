#ifndef MULTITAPDIFFUSER
#define MULTITAPDIFFUSER

#include "AudioLib/ShaRandom.h"

namespace CloudSeed
{
	class MultitapDiffuser
	{
	public:
		static const int MaxTaps = 50;

	private:
		float* buffer;
		float* output;
		int len;

		int index;
		// Update() fills these, Process() plays the *Temp copies (swapped in on isDirty).
		// Fixed MaxTaps size: a knob write must not allocate.
		float tapGains[MaxTaps];
		int tapPosition[MaxTaps];
		// Update() draws two values per tap; Scatter reads a per-tap LFO phase and rate
		// spread from the fixed slots after them (kScatterSeedBase), so the tap pattern
		// does not change when Scatter is added.
		static const int kScatterSeedBase = 2 * MaxTaps;
		AudioLib::SeedSeries<4 * MaxTaps> seeds;
		int count;
		float length;
		float gain;
		float decay;

		bool isDirty;
		bool bloom;
		float tapGainsTemp[MaxTaps];
		int tapPositionTemp[MaxTaps];
		int countTemp;
		int cleanTapTemp;  // index of the tap-vs-clean mix tap: never animated
		int cleanTap;

		// Scatter: each tap's gain is scaled by 1 - depth * (1 + sin(2*pi*phase)) / 2,
		// i.e. it oscillates between (1 - depth) and 1 of its static value. The factor
		// is recomputed once per block and ramped linearly across it.
		bool  scatter;
		float scatterRate;   // cycles per sample (Hz / samplerate)
		float scatterDepth;  // 0..1
		bool  scatterActive; // modulated path in use: scatter on, or still ramping back to 1
		float scatterPhase[MaxTaps];  // 0..1
		float scatterMod[MaxTaps];    // modulation factor reached at the end of the last block

	public:
		// buffer/output are placement-new'd into the SDRAM bump-allocator pool
		// (custom_pool_allocate) and are never freed, so there is no destructor.
		MultitapDiffuser(int delayBufferSize);

		float* GetOutput() { return output; }

		void SetSeed(int seed)                  { seeds.SetSeed(seed); Update(); }
		void SetCrossSeed(float crossSeed)      { seeds.SetCrossSeed(crossSeed); Update(); }
		void SetTapCount(int tapCount)          { count = tapCount; Update(); }
		void SetTapLength(int tapLength)        { length = tapLength; Update(); }
		void SetTapDecay(float tapDecay)        { decay = tapDecay; Update(); }
		void SetTapGain(float tapGain)          { gain = tapGain; Update(); }
		void SetBloom(bool bloomOn)             { bloom = bloomOn; Update(); }
		void SetScatter(bool on)                { scatter = on; if (on) scatterActive = true; }
		void SetScatterRate(float cyclesPerSample) { scatterRate = cyclesPerSample; }
		void SetScatterDepth(float depth)       { scatterDepth = depth; }

		void Process(float* input, int sampleCount);
		void ClearBuffers();

	private:
		// Recomputes the tap positions and gains. Out of line on purpose: every
		// setter calls it, and inlining it into each ReverbChannel::SetParameter case
		// cost 7.5 KB of SRAM for code that runs once per knob write.
		void Update();
		// Process() while Scatter is on or ramping back to the static gains.
		void ProcessScatter(float* input, int sampleCount);
	};
}

#endif
