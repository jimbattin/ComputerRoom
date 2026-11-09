#pragma once

#include "Lp1.h"

namespace AudioLib
{
	// One-pole high-pass: the input minus Lp1's low-pass output.
	class Hp1 : public Lp1
	{
	public:
		using Lp1::Lp1;

		// In place is allowed (input == output).
		void Process(const float* input, float* output, int len)
		{
			const float b0 = this->b0, a1 = this->a1;
			float s = state;
			for (int i = 0; i < len; i++)
			{
				const float x = input[i];
				s = b0 * x + a1 * s;
				output[i] = x - s;
			}
			state = s;
		}
	};
}
