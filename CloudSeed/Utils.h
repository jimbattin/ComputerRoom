#ifndef UTILS
#define UTILS

#include <cstring>
#include <cmath>

namespace CloudSeed
{
	class Utils
	{
	public:

		static inline void ZeroBuffer(float* buffer, int len)
		{
			for (int i = 0; i < len; i++)
				buffer[i] = 0.0f;
		}

		static inline void Copy(const float* source, float* dest, int len)
		{
			std::memcpy(dest, source, len * sizeof(float));
		}

		static float DB2gain(float db)
		{
			return std::pow(10.0f, db / 20.0f);
		}
	};
}

#endif
