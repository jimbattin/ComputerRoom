#include <cmath>
#include "Biquad.h"

namespace AudioLib
{
	Biquad::Biquad(FilterType filterType, float samplerate)
	{
		Type = filterType;
		this->samplerate = samplerate;
		Slope = 1.0f;
		SetGainDb(0.0f);
		Frequency = samplerate / 4;
		ClearBuffers();
		Update();
	}

	void Biquad::SetGainDb(float value) 
	{
		SetGain(std::pow(10.0f, value / 20.0f));
	}

	void Biquad::SetGain(float value) 
	{
		if (value == 0)
			value = 0.001f; // -60dB
		
		gain = value;
	}

	void Biquad::Update()
	{
		constexpr float kTwoPi = 6.28318530717958647692f;
		float omega = kTwoPi * Frequency / samplerate;
		float sinOmega = std::sin(omega);
		float cosOmega = std::cos(omega);
		float alpha = sinOmega / 2 * std::sqrt((gain + 1 / gain) * (1 / Slope - 1) + 2);
		float sqrtGain = std::sqrt(gain);
		float a0;

		if (Type == FilterType::LowShelf)
		{
			b0 = gain * ((gain + 1) - (gain - 1) * cosOmega + 2 * sqrtGain * alpha);
			b1 = 2 * gain * ((gain - 1) - (gain + 1) * cosOmega);
			b2 = gain * ((gain + 1) - (gain - 1) * cosOmega - 2 * sqrtGain * alpha);
			a0 = (gain + 1) + (gain - 1) * cosOmega + 2 * sqrtGain * alpha;
			a1 = -2 * ((gain - 1) + (gain + 1) * cosOmega);
			a2 = (gain + 1) + (gain - 1) * cosOmega - 2 * sqrtGain * alpha;
		}
		else
		{
			b0 = gain * ((gain + 1) + (gain - 1) * cosOmega + 2 * sqrtGain * alpha);
			b1 = -2 * gain * ((gain - 1) + (gain + 1) * cosOmega);
			b2 = gain * ((gain + 1) + (gain - 1) * cosOmega - 2 * sqrtGain * alpha);
			a0 = (gain + 1) - (gain - 1) * cosOmega + 2 * sqrtGain * alpha;
			a1 = 2 * ((gain - 1) - (gain + 1) * cosOmega);
			a2 = (gain + 1) - (gain - 1) * cosOmega - 2 * sqrtGain * alpha;
		}

		float g = 1 / a0;

		b0 = b0 * g;
		b1 = b1 * g;
		b2 = b2 * g;
		a1 = a1 * g;
		a2 = a2 * g;
	}

	void Biquad::ClearBuffers() 
	{
		x2 = 0;
		y2 = 0;
		x1 = 0;
		y1 = 0;
	}

}
