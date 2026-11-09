#ifndef BIQUAD
#define BIQUAD

namespace AudioLib
{
	// RBJ shelving filter: the late lines' low and high shelves (DelayLine).
	class Biquad
	{
	public:
		enum class FilterType
		{
			LowShelf,
			HighShelf
		};

	private:
		float samplerate;
		float a1, a2, b0, b1, b2;
		float x1, x2, y1, y2;
		float gain;

	public:
		FilterType Type;
		float Frequency;
		float Slope;

		Biquad(FilterType filterType, float samplerate);

		void SetGainDb(float value);
		void SetGain(float value);

		void Update();
		
		// In place is allowed (input == output). State and coefficients are copied to
		// locals: the output stores may alias the members.
		void inline Process(const float* input, float* output, int len)
		{
			const float b0 = this->b0, b1 = this->b1, b2 = this->b2, a1 = this->a1, a2 = this->a2;
			float x1 = this->x1, x2 = this->x2, y1 = this->y1, y2 = this->y2;
			for (int i = 0; i < len; i++)
			{
				const float x = input[i];
				const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
				x2 = x1;
				y2 = y1;
				x1 = x;
				y1 = y;
				output[i] = y;
			}
			this->x1 = x1;
			this->x2 = x2;
			this->y1 = y1;
			this->y2 = y2;
		}

		void ClearBuffers();
	};
}

#endif
