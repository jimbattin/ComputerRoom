#include <algorithm>
#include <cmath>
#include <new>
#include "MultitapDiffuser.h"
#include "FastSin.h"
#include "Utils.h"

extern void* custom_pool_allocate(size_t size);

namespace CloudSeed
{
	MultitapDiffuser::MultitapDiffuser(int delayBufferSize)
	{
		len = delayBufferSize;
		buffer = new (custom_pool_allocate(sizeof(float) * delayBufferSize)) float[delayBufferSize];
		output = new (custom_pool_allocate(sizeof(float) * delayBufferSize)) float[delayBufferSize];
		index = 0;
		count = 1;
		length = 1;
		gain = 1.0;
		decay = 0.0;
		bloom = false;
		cleanTap = 0;
		cleanTapTemp = 0;
		scatter = false;
		scatterRate = 0.0f;
		scatterDepth = 0.0f;
		scatterActive = false;
		for (int i = 0; i < MaxTaps; i++)
		{
			scatterPhase[i] = 0.0f;
			scatterMod[i] = 1.0f;
		}
		seeds.SetSeed(0);
		Update();
	}

	void MultitapDiffuser::Process(float* input, int sampleCount)
	{
		// prevents race condition when parameters are updated from Gui
		if (isDirty)
		{
			std::copy(tapGains, tapGains + count, tapGainsTemp);
			std::copy(tapPosition, tapPosition + count, tapPositionTemp);
			countTemp = count;
			cleanTapTemp = cleanTap;
			isDirty = false;
		}

		if (scatterActive)
		{
			ProcessScatter(input, sampleCount);
			return;
		}

		int* const tapPos = tapPositionTemp;
		float* const tapGain = tapGainsTemp;
		const int cnt = countTemp;

		for (int i = 0; i < sampleCount; i++)
		{
			if (index < 0) index += len;
			buffer[index] = input[i];
			output[i] = 0.0;

			for (int j = 0; j < cnt; j++)
			{
				auto idx = index + tapPos[j];
				if (idx >= len) idx -= len;
				output[i] += buffer[idx] * tapGain[j];
			}

			index--;
		}
	}

	void MultitapDiffuser::ProcessScatter(float* input, int sampleCount)
	{
		const int cnt = countTemp;
		const float invCount = 1.0f / sampleCount;
		float gainNow[MaxTaps];
		float gainStep[MaxTaps];
		bool settled = true;  // every tap's factor reaches 1 at the end of this block

		// One LFO step per block; the factor is ramped linearly across the block so a
		// fast rate or deep setting does not step at the 1 kHz block rate.
		for (int j = 0; j < cnt; j++)
		{
			float target = 1.0f;
			if (scatter && j != cleanTapTemp)
			{
				const float rate = scatterRate * (0.85f + 0.3f * seeds[kScatterSeedBase + MaxTaps + j]);
				float phase = scatterPhase[j] + rate * sampleCount;
				phase -= (int)phase;
				scatterPhase[j] = phase;
				float angle = phase + seeds[kScatterSeedBase + j];
				if (angle >= 1.0f)
					angle -= 1.0f;
				target = 1.0f - scatterDepth * 0.5f * (1.0f + FastSin::Get(angle));
			}
			const float base = tapGainsTemp[j];
			gainNow[j] = base * scatterMod[j];
			gainStep[j] = base * (target - scatterMod[j]) * invCount;
			scatterMod[j] = target;
			if (target != 1.0f)
				settled = false;
		}

		const int* const tapPos = tapPositionTemp;
		for (int i = 0; i < sampleCount; i++)
		{
			if (index < 0) index += len;
			buffer[index] = input[i];
			float acc = 0.0f;

			for (int j = 0; j < cnt; j++)
			{
				auto idx = index + tapPos[j];
				if (idx >= len) idx -= len;
				gainNow[j] += gainStep[j];
				acc += buffer[idx] * gainNow[j];
			}

			output[i] = acc;
			index--;
		}

		// Scatter switched off and every factor is back at 1: the static path resumes.
		if (!scatter && settled)
			scatterActive = false;
	}

	void MultitapDiffuser::ClearBuffers()
	{
		Utils::ZeroBuffer(buffer, len);
		Utils::ZeroBuffer(output, len);
		// A preset load clears the buffers: restart every tap's LFO from its seeded
		// phase, so a preset sounds the same each time it is loaded.
		for (int i = 0; i < MaxTaps; i++)
		{
			scatterPhase[i] = 0.0f;
			scatterMod[i] = 1.0f;
		}
		scatterActive = scatter;
	}

	void MultitapDiffuser::Update()
	{
		int s = 0;
		auto rand = [&]() {return seeds[s++]; };

		// The tap arrays hold MaxTaps; GetScaledParameter already keeps TapCount
		// within 1..MaxTaps.
		if (count < 1)
			count = 1;
		if (count > MaxTaps)
			count = MaxTaps;

		if (length < count)
			length = count;

		// used to adjust the volume of the overall output as it grows when we add more taps
		float tapCountFactor = 1.0 / (1 + std::sqrt(count / MaxTaps));

		float tapData[MaxTaps];

		auto sumLengths = 0.0;
		for (int i = 0; i < count; i++)
		{
			auto val = 0.1 + rand();
			tapData[i] = val;
			sumLengths += val;
		}

		auto scaleLength = length / sumLengths;
		tapPosition[0] = 0;

		for (int i = 1; i < count; i++)
		{
			tapPosition[i] = tapPosition[i - 1] + (int)(tapData[i] * scaleLength);
		}

		float lastTapPos = tapPosition[count - 1];
		int gainIndex = 0;

		for (int i = 0; i < count; i++)
		{
			// when decay set to 0, there is no decay, when set to 1, the gain at the last sample is 0.01 = -40dB
			auto g = std::pow(10, -decay * 2 * tapPosition[i] / (float)(lastTapPos + 1));
			auto tap = (2 * rand() - 1) * tapCountFactor;

			if (bloom) 
				gainIndex = count - (i + 1);
			else
				gainIndex = i;
			tapGains[gainIndex] = tap * g * gain;
		}
		// Set the tap vs. clean mix
		if (bloom)
			cleanTap = count - 1;
		else
			cleanTap = 0;
		tapGains[cleanTap] = (1 - gain);

		isDirty = true;
	}
}
