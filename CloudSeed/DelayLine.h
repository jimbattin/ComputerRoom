#ifndef DELAYLINE
#define DELAYLINE

#include "AudioLib/Lp1.h"
#include "ModulatedDelay.h"
#include "AllpassDiffuser.h"
#include "AudioLib/Biquad.h"
#include "Utils.h"

namespace CloudSeed
{
	class DelayLine
	{
	private:
		ModulatedDelay delay;
		AllpassDiffuser diffuser;
		AudioLib::Biquad lowShelf;
		AudioLib::Biquad highShelf;
		AudioLib::Lp1 lowPass;
		float* mixedBuffer;
		float* filterOutputBuffer;
		int bufferSize;
		float feedback;

	public:

		bool DiffuserEnabled;
		bool LowShelfEnabled;
		bool HighShelfEnabled;
		bool CutoffEnabled;
		bool LateStageTap;

		DelayLine(int bufferSize, int samplerate)
			: delay(bufferSize, samplerate * 2, 10000) // 2 second buffer, to prevent buffer overflow with modulation and randomness added (Which may increase effective delay)
			, diffuser(bufferSize, samplerate)
			, lowShelf(AudioLib::Biquad::FilterType::LowShelf, samplerate)
			, highShelf(AudioLib::Biquad::FilterType::HighShelf, samplerate)
			, lowPass(samplerate)
		{
			this->bufferSize = bufferSize;
			// A defined starting state: the lines live in the SDRAM pool, which is never
			// zeroed, and SetParameter(LateDiffusionEnabled) compares against the old
			// DiffuserEnabled before the preset sets it.
			DiffuserEnabled = false;
			LowShelfEnabled = false;
			HighShelfEnabled = false;
			CutoffEnabled = false;
			LateStageTap = false;
			feedback = 0.0f;
			mixedBuffer = new float[bufferSize];
			filterOutputBuffer = new float[bufferSize];

			lowShelf.SetGainDb(-20);
			lowShelf.Frequency = 20;

			highShelf.SetGainDb(-20);
			highShelf.Frequency = 19000;

			lowPass.SetCutoffHz(1000);
			lowShelf.Update();
			highShelf.Update();

			SetDiffuserSeed(1);
		}

		~DelayLine()
		{
			delete[] mixedBuffer;
			delete[] filterOutputBuffer;
		}

		void SetDiffuserSeed(int seed)
		{
			diffuser.SetSeed(seed);
		}

		void SetDelay(int delaySamples)
		{
			delay.SampleDelay = delaySamples;
		}

		void SetFeedback(float feedb)
		{
			feedback = feedb;
		}

		void SetDiffuserDelay(int delaySamples)
		{
			diffuser.SetDelay(delaySamples);
		}

		void SetDiffuserFeedback(float feedb)
		{
			diffuser.SetFeedback(feedb);
		}

		void SetDiffuserStages(int stages)
		{
			diffuser.Stages = stages;
		}

		void SetLowShelfGain(float gain)
		{
			lowShelf.SetGain(gain);
			lowShelf.Update();
		}

		void SetLowShelfFrequency(float frequency)
		{
			lowShelf.Frequency = frequency;
			lowShelf.Update();
		}

		void SetHighShelfGain(float gain)
		{
			highShelf.SetGain(gain);
			highShelf.Update();
		}

		void SetHighShelfFrequency(float frequency)
		{
			highShelf.Frequency = frequency;
			highShelf.Update();
		}

		void SetCutoffFrequency(float frequency)
		{
			lowPass.SetCutoffHz(frequency);
		}

		void SetLineModAmount(float amount)
		{
			delay.ModAmount = amount;
		}

		void SetLineModRate(float rate)
		{
			delay.ModRate = rate;
		}

		void SetDiffuserModAmount(float amount)
		{
			diffuser.SetModulationEnabled(amount > 0.0f);
			diffuser.SetModAmount(amount);
		}

		void SetDiffuserModRate(float rate)
		{
			diffuser.SetModRate(rate);
		}

		void SetInterpolationEnabled(bool value)
		{
			diffuser.SetInterpolationEnabled(value);
		}


		float* GetOutput()
		{
			if (LateStageTap)
			{
				if (DiffuserEnabled)
					return diffuser.GetOutput();
				else
					return mixedBuffer;
			}
			else
			{
				return delay.GetOutput();
			}
		}

		void Process(const float* input, int sampleCount)
		{
			for (int i = 0; i < sampleCount; i++)
				mixedBuffer[i] = input[i] + filterOutputBuffer[i] * feedback;

			const float* stage;
			if (LateStageTap)
			{
				if (DiffuserEnabled)
				{
					diffuser.Process(mixedBuffer, sampleCount);
					delay.Process(diffuser.GetOutput(), sampleCount);
				}
				else
				{
					delay.Process(mixedBuffer, sampleCount);
				}

				stage = delay.GetOutput();
			}
			else
			{

				if (DiffuserEnabled)
				{
					delay.Process(mixedBuffer, sampleCount);
					diffuser.Process(delay.GetOutput(), sampleCount);
					stage = diffuser.GetOutput();
				}
				else
				{
					delay.Process(mixedBuffer, sampleCount);
					stage = delay.GetOutput();
				}
			}

			// The feedback filters run from the stage output into filterOutputBuffer
			// (the first one out of place, the rest in place).
			if (LowShelfEnabled)
			{
				lowShelf.Process(stage, filterOutputBuffer, sampleCount);
				stage = filterOutputBuffer;
			}
			if (HighShelfEnabled)
			{
				highShelf.Process(stage, filterOutputBuffer, sampleCount);
				stage = filterOutputBuffer;
			}
			if (CutoffEnabled)
			{
				lowPass.Process(stage, filterOutputBuffer, sampleCount);
				stage = filterOutputBuffer;
			}

			if (stage != filterOutputBuffer)
				Utils::Copy(stage, filterOutputBuffer, sampleCount);
		}

		void ClearDiffuserBuffer()
		{
			diffuser.ClearBuffers();
		}

		void ClearBuffers()
		{
			delay.ClearBuffers();
			diffuser.ClearBuffers();
			lowShelf.ClearBuffers();
			highShelf.ClearBuffers();
			lowPass.ClearBuffers();

			for (int i = 0; i < bufferSize; i++)
				filterOutputBuffer[i] = 0.0;
		}
	};
}

#endif
