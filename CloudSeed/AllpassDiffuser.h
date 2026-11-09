
#pragma once

#include <cmath>
#include "ModulatedAllpass.h"
#include "AudioLib/ShaRandom.h"

namespace CloudSeed
{
	class AllpassDiffuser
	{
	public:
		static const int MaxStageCount = 2;
		// top of DiffusionDelay / LateDiffusionDelay (ReverbController::GetScaledParameter)
		static const int MaxDelayMs = 100;

	private:
		int samplerate;

		ModulatedAllpass* filters[MaxStageCount];  // filled once, in the constructor
		int delay;
		AudioLib::SeedSeries<MaxStageCount * 3> seeds;  // delay, mod amount, mod rate per stage
		
	public:
		int Stages;

		AllpassDiffuser(int bufferSize, int samplerate)
		{
			// reads reach at most 2 * SampleDelay: SampleDelay <= delay <= MaxDelayMs, and
			// ModulatedAllpass::Update keeps ModAmount < SampleDelay
			const int ring = 2 * (MaxDelayMs * samplerate / 1000) + 2;
			for (int i = 0; i < MaxStageCount; i++)
				filters[i] = new ModulatedAllpass(bufferSize, ring, 100);

			// Update() reads delay before any preset sets it: start from the filters'
			// own 100-sample delay. Their modulation starts off (ModRate 0).
			delay = 100;
			seeds.SetSeed(23456);
			Update();
			Stages = 1;

			this->samplerate = samplerate;
		}

		~AllpassDiffuser()
		{
			for (auto filter : filters)
				delete filter;
		}


		void SetSeed(int seed)
		{
			seeds.SetSeed(seed);
			Update();
		}

		void SetCrossSeed(float crossSeed)
		{
			seeds.SetCrossSeed(crossSeed);
			Update();
		}


		void SetModulationEnabled(bool value)
		{
			for (auto filter : filters)
				filter->ModulationEnabled = value;
		}

		void SetInterpolationEnabled(bool enabled)
		{
			for (auto filter : filters)
				filter->InterpolationEnabled = enabled;
		}

		float* GetOutput()
		{
			return filters[Stages - 1]->GetOutput();
		}

		
		void SetDelay(int delaySamples)
		{
			delay = delaySamples;
			Update();
		}

		void SetFeedback(float feedback)
		{
			for (auto filter : filters)
				filter->Feedback = feedback;
		}

		void SetModAmount(float amount)
		{
			for (int i = 0; i < MaxStageCount; i++)
			{
				filters[i]->ModAmount = amount * (0.85f + 0.3f * seeds[MaxStageCount + i]);
			}
		}

		void SetModRate(float rate)
		{
			for (int i = 0; i < MaxStageCount; i++)
				filters[i]->ModRate = rate * (0.85f + 0.3f * seeds[MaxStageCount * 2 + i]) / samplerate;
		}

		void Process(const float* input, int sampleCount)
		{
			filters[0]->Process(input, sampleCount);

			for (int i = 1; i < Stages; i++)
			{
				filters[i]->Process(filters[i - 1]->GetOutput(), sampleCount);
			}
		}

		void ClearBuffers()
		{
			for (int i = 0; i < MaxStageCount; i++)
				filters[i]->ClearBuffers();
		}

	private:
		void Update()
		{
			for (int i = 0; i < MaxStageCount; i++)
			{
				// 0.1 ... 1.0 of delay; double, as upstream: it sets an integer sample count
				filters[i]->SampleDelay = (int)(delay * (std::pow(10.0, (double)seeds[i]) * 0.1));
			}
		}

	};
}
