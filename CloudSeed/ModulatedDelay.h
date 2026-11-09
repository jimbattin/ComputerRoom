#ifndef MODULATEDDELAY
#define MODULATEDDELAY

#include "Utils.h"
#include "FastSin.h"

extern void* custom_pool_allocate(size_t size);

namespace CloudSeed
{
	class ModulatedDelay
	{
	private:

		static const int ModulationUpdateRate = 8;

		float* delayBuffer;
		float* output;
		int bufferSize;
		int writeIndex;
		int readIndexA;
		int readIndexB;
		int samplesProcessed;
		int delayBufferSizeSamples;

		float modPhase;
		float gainA;
		float gainB;

	public:
		int SampleDelay;
		
		float ModAmount;
		float ModRate;

		ModulatedDelay(int bufferSize, int delayBufferSizeSamples, int sampleDelay)
		{
			this->delayBufferSizeSamples = delayBufferSizeSamples;
			this->bufferSize = bufferSize;
			this->delayBuffer = new (custom_pool_allocate(sizeof(float) * delayBufferSizeSamples)) float[delayBufferSizeSamples];
			this->output = new (custom_pool_allocate(sizeof(float) * bufferSize)) float[bufferSize];
			this->SampleDelay = sampleDelay;
			writeIndex = 0;
			modPhase = FastSin::RandomPhase();
			ModRate = 0.0f;
			ModAmount = 0.0f;
			Update();
		}

		~ModulatedDelay()
		{
			// delayBuffer/output are placement-new'd into the SDRAM bump-allocator
			// pool (custom_pool_allocate) and are never freed; nothing to delete here.
		}

		float* GetOutput()
		{
			return output;
		}

		void Process(const float* input, int sampleCount)
		{
			// Locals in each 8-sample segment between Update()s: the float stores to
			// buffer/output may alias the members, which otherwise reload every sample.
			int i = 0;
			while (i < sampleCount)
			{
				if (samplesProcessed == ModulationUpdateRate)
					Update();
				int end = i + (ModulationUpdateRate - samplesProcessed);
				if (end > sampleCount)
					end = sampleCount;
				samplesProcessed += end - i;

				float* const buf = delayBuffer;
				const int size = delayBufferSizeSamples;
				const float gA = gainA, gB = gainB;
				int w = writeIndex, a = readIndexA, b = readIndexB;
				for (; i < end; i++)
				{
					buf[w] = input[i];
					output[i] = buf[a] * gA + buf[b] * gB;
					if (++w >= size) w -= size;
					if (++a >= size) a -= size;
					if (++b >= size) b -= size;
				}
				writeIndex = w;
				readIndexA = a;
				readIndexB = b;
			}
		}

		void ClearBuffers()
		{
			Utils::ZeroBuffer(delayBuffer, delayBufferSizeSamples);
			Utils::ZeroBuffer(output, bufferSize);
		}


	private:
		void Update()
		{
			auto mod = FastSin::Step(modPhase, ModRate * ModulationUpdateRate);
			auto totalDelay = SampleDelay + ModAmount * mod;

			auto delayA = (int)totalDelay;
			auto delayB = (int)totalDelay + 1;

			auto partial = totalDelay - delayA;

			gainA = 1 - partial;
			gainB = partial;

			readIndexA = writeIndex - delayA;
			readIndexB = writeIndex - delayB;
			if (readIndexA < 0) readIndexA += delayBufferSizeSamples;
			if (readIndexB < 0) readIndexB += delayBufferSizeSamples;

			samplesProcessed = 0;
		}
	};
}

#endif