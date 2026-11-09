#pragma once

#include <cmath>
#include <cstdlib>
#include "AudioLib/MathDefs.h"

namespace CloudSeed
{
	class FastSin
	{
	private:
		static const int DataSize = 32768;
		static_assert((DataSize & (DataSize - 1)) == 0, "power of two");
		static DSY_SDRAM_BSS float data[DataSize];

	public:
		static void Init()
		{
			for (int i = 0; i < DataSize; i++)
			{
				data[i] = std::sin(2 * M_PI * i / (double)DataSize);
			}
		}

		// sin(2 pi index) for index 0..1. The mask makes index 1.0 read data[0].
		static float Get(float index)
		{
			return data[(int)(index * (float)DataSize) & (DataSize - 1)];
		}

		// Advances an LFO phase (0..1) by `increment` and returns its sine.
		// For phase >= 1 the wrap equals fmodf(phase, 1).
		static float Step(float& phase, float increment)
		{
			phase += increment;
			if (phase > 1.0f)
				phase -= (float)(int)phase;
			return Get(phase);
		}

		// Start phase of a modulated delay: 0.01..0.99, from std::rand().
		static float RandomPhase()
		{
			return 0.01f + 0.98f * ((float)std::rand() / (float)RAND_MAX);
		}
	};
}

