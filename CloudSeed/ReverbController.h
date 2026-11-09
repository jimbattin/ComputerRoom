//see https://github.com/ValdemarOrn/CloudSeed
//port to Daisy and App_Dekrispator by Erwin Coumans

#ifndef REVERBCONTROLLER
#define REVERBCONTROLLER

#include <cmath>
#include "Parameter.h"
#include "ReverbChannel.h"
#include "AudioLib/ValueTables.h"
#include "AllpassDiffuser.h"
#include "MultitapDiffuser.h"

namespace CloudSeed
{
	class ReverbController
	{
	private:
		static const int bufferSize = 48; // Engine block size; src/cloudseed.cpp AUDIO_BUFFER_SIZE must match

		ReverbChannel channel;
		float parameters[(int)Parameter::Count];

	public:
		ReverbController(int samplerate)
			: channel(bufferSize, samplerate)
		{
			for (int i = 0; i < (int)Parameter::Count; i++)
				parameters[i] = 0.0f;
			// No preset is applied here: main() parses presets.toml and calls
			// LoadPreset() before audio starts.
		}

		// Applies a preset parsed from presets.toml.
		void LoadPreset(const float* values)
		{
			for (int i = 0; i < (int)Parameter::Count; i++)
			{
				// LineCount comes from the delay-line toggle target at audio rate;
				// every other slot, Bloom included, is preset data.
				if (i == (int)Parameter::LineCount)
					continue;
				parameters[i] = values[i];
			}

			for (int i = 0; i < (int)Parameter::Count; i++)
				SetParameter((Parameter)i, parameters[i]);
		}

		float* GetAllParameters()
		{
			return parameters;
		}

		// Continuous values are float. Integer results (stage counts, seeds, the line
		// delay) keep upstream's double arithmetic, written as explicit casts, so the
		// integers they produce are unchanged.
		float GetScaledParameter(Parameter param)
		{
			using AudioLib::ValueTables;
			switch (param)
			{
				// Input
			case Parameter::InputMix:                  return P(Parameter::InputMix);
			case Parameter::PreDelay:                  return (int)(P(Parameter::PreDelay) * 1000);

			case Parameter::HighPass:                  return 20 + ValueTables::Get(P(Parameter::HighPass), ValueTables::Response4Oct) * 980;
			case Parameter::LowPass:                   return 400 + ValueTables::Get(P(Parameter::LowPass), ValueTables::Response4Oct) * 19600;

				// Early
			case Parameter::TapCount:                  return 1 + (int)(P(Parameter::TapCount) * (MultitapDiffuser::MaxTaps - 1));
			case Parameter::TapLength:                 return (int)(P(Parameter::TapLength) * 500);
			case Parameter::TapGain:                   return ValueTables::Get(P(Parameter::TapGain), ValueTables::Response2Dec);
			case Parameter::TapDecay:                  return P(Parameter::TapDecay);
			case Parameter::Bloom:                     return P(Parameter::Bloom) < 0.5f ? 0.0f : 1.0f;

				// Diffusion

			case Parameter::DiffusionEnabled:          return P(Parameter::DiffusionEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::DiffusionStages:           return 1 + (int)((double)P(Parameter::DiffusionStages) * (AllpassDiffuser::MaxStageCount - 0.001));
			case Parameter::DiffusionDelay:            return (int)(10 + P(Parameter::DiffusionDelay) * (AllpassDiffuser::MaxDelayMs - 10));
			case Parameter::DiffusionFeedback:         return P(Parameter::DiffusionFeedback);

				// Late
			case Parameter::LineCount:                 return (int)(P(Parameter::LineCount));
			case Parameter::LineDelay:                 return (int)(20.0 + (double)(ValueTables::Get(P(Parameter::LineDelay), ValueTables::Response2Dec) * 980));
			case Parameter::LineDecay:                 return 0.05f + ValueTables::Get(P(Parameter::LineDecay), ValueTables::Response3Dec) * 59.95f;

			case Parameter::LateDiffusionEnabled:      return P(Parameter::LateDiffusionEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::LateDiffusionStages:       return 1 + (int)((double)P(Parameter::LateDiffusionStages) * (AllpassDiffuser::MaxStageCount - 0.001));
			case Parameter::LateDiffusionDelay:        return (int)(10 + P(Parameter::LateDiffusionDelay) * (AllpassDiffuser::MaxDelayMs - 10));
			case Parameter::LateDiffusionFeedback:     return P(Parameter::LateDiffusionFeedback);

				// Frequency Response
			case Parameter::PostLowShelfGain:          return ValueTables::Get(P(Parameter::PostLowShelfGain), ValueTables::Response2Dec);
			case Parameter::PostLowShelfFrequency:     return 20 + ValueTables::Get(P(Parameter::PostLowShelfFrequency), ValueTables::Response4Oct) * 980;
			case Parameter::PostHighShelfGain:         return ValueTables::Get(P(Parameter::PostHighShelfGain), ValueTables::Response2Dec);
			case Parameter::PostHighShelfFrequency:    return 400 + ValueTables::Get(P(Parameter::PostHighShelfFrequency), ValueTables::Response4Oct) * 19600;
			case Parameter::PostCutoffFrequency:       return 400 + ValueTables::Get(P(Parameter::PostCutoffFrequency), ValueTables::Response4Oct) * 19600;

				// Modulation
			case Parameter::EarlyDiffusionModAmount:   return P(Parameter::EarlyDiffusionModAmount) * 2.5f;
			case Parameter::EarlyDiffusionModRate:     return ValueTables::Get(P(Parameter::EarlyDiffusionModRate), ValueTables::Response2Dec) * 5;
			case Parameter::LineModAmount:             return P(Parameter::LineModAmount) * 2.5f;
			case Parameter::LineModRate:               return ValueTables::Get(P(Parameter::LineModRate), ValueTables::Response2Dec) * 5;
			case Parameter::LateDiffusionModAmount:    return P(Parameter::LateDiffusionModAmount) * 2.5f;
			case Parameter::LateDiffusionModRate:      return ValueTables::Get(P(Parameter::LateDiffusionModRate), ValueTables::Response2Dec) * 5;

				// Seeds
			case Parameter::TapSeed:                   return (int)std::floor((double)(P(Parameter::TapSeed) * 1000000) + 0.001);
			case Parameter::DiffusionSeed:             return (int)std::floor((double)(P(Parameter::DiffusionSeed) * 1000000) + 0.001);
			case Parameter::DelaySeed:                 return (int)std::floor((double)(P(Parameter::DelaySeed) * 1000000) + 0.001);
			case Parameter::PostDiffusionSeed:         return (int)std::floor((double)(P(Parameter::PostDiffusionSeed) * 1000000) + 0.001);

				// Output
			case Parameter::CrossSeed:                 return P(Parameter::CrossSeed);

			case Parameter::DryOut:                    return ValueTables::Get(P(Parameter::DryOut), ValueTables::Response2Dec);
			case Parameter::PredelayOut:               return ValueTables::Get(P(Parameter::PredelayOut), ValueTables::Response2Dec);
			case Parameter::EarlyOut:                  return ValueTables::Get(P(Parameter::EarlyOut), ValueTables::Response2Dec);
			case Parameter::MainOut:                   return ValueTables::Get(P(Parameter::MainOut), ValueTables::Response2Dec);

				// Switches
			case Parameter::HiPassEnabled:             return P(Parameter::HiPassEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::LowPassEnabled:            return P(Parameter::LowPassEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::LowShelfEnabled:           return P(Parameter::LowShelfEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::HighShelfEnabled:          return P(Parameter::HighShelfEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::CutoffEnabled:             return P(Parameter::CutoffEnabled) < 0.5f ? 0.0f : 1.0f;
			case Parameter::LateStageTap:			   return P(Parameter::LateStageTap) < 0.5f ? 0.0f : 1.0f;

				// Effects
			case Parameter::Interpolation:			   return P(Parameter::Interpolation) < 0.5f ? 0.0f : 1.0f;

				// Early tap animation
			case Parameter::Scatter:                   return P(Parameter::Scatter) < 0.5f ? 0.0f : 1.0f;
			case Parameter::ScatterRate:               return 0.1f + ValueTables::Get(P(Parameter::ScatterRate), ValueTables::Response2Dec) * 4.9f;
			case Parameter::ScatterDepth:              return P(Parameter::ScatterDepth);

			default: return 0.0f;
			}
		}

		void SetParameter(Parameter param, float value)
		{
			parameters[(int)param] = value;
			auto scaled = GetScaledParameter(param);
			
			channel.SetParameter(param, scaled);
		}

		void ClearBuffers()
		{
			channel.ClearBuffers();
		}

		void Process(const float* input, float* output, int len)
		{
			channel.Process(input, output, len);
		}
		
	private:
		float P(Parameter para)
		{
			return parameters[(int)para];
		}
	};
}
#endif
