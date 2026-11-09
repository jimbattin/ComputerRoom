#pragma once

#include <cmath>
#include "MathDefs.h"

namespace AudioLib
{
	// One-pole low-pass. Denormals are handled by the FPU's flush-to-zero mode
	// (src/cloudseed.cpp main()), so there is no zero-snap.
	class Lp1
	{
	public:
		Lp1(float fs)
		{
			this->fs = fs;
			state = 0.0f;
		}

		void SetCutoffHz(float hz)
		{
			cutoffHz = hz;
			Update();
		}

		void Update()
		{
			// Prevent going over the Nyquist frequency
			if (cutoffHz >= fs * 0.5f)
				cutoffHz = fs * 0.499f;

			// alpha = nn - sqrt(nn^2 - 1), nn = 2 - cos(w), written with 1 - cos(w) =
			// 2 sin^2(w/2) = c so that float keeps its precision at a 20 Hz cutoff.
			const float s = std::sin((float)M_PI * cutoffHz / fs);
			const float c = 2.0f * s * s;
			const float root = std::sqrt(c * (2.0f + c));
			b0 = root - c;
			a1 = 1.0f + c - root;
		}

		void ClearBuffers()
		{
			state = 0.0f;
		}

		// In place is allowed (input == output).
		void Process(const float* input, float* output, int len)
		{
			const float b0 = this->b0, a1 = this->a1;
			float s = state;
			for (int i = 0; i < len; i++)
			{
				s = b0 * input[i] + a1 * s;
				output[i] = s;
			}
			state = s;
		}

	protected:
		float fs;
		float b0, a1;
		float cutoffHz;
		float state;
	};
}
