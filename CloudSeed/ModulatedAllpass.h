#ifndef MODULATEDALLPASS
#define MODULATEDALLPASS

#include <cstddef>
#include <new>
#include "FastSin.h"
#include "Utils.h"
extern void* custom_pool_allocate(size_t size);

namespace CloudSeed
{
	class ModulatedAllpass
	{
	private:
		static const int ModulationUpdateRate = 8;

		float* delayBuffer;
		float* output;
		int bufferSize;
		int delayBufferSamples;
		int index;
		unsigned int samplesProcessed;
		int written;  // samples written since the last clear, saturating at delayBufferSamples

		float modPhase;
		int delayA;
		int delayB;
		float gainA;
		float gainB;

	public:

		int SampleDelay;
		float Feedback;
		float ModAmount;
		float ModRate;

		bool InterpolationEnabled;
		bool ModulationEnabled;

		// bufferSize: samples per Process() block. delayBufferSamples: ring length,
		// longer than the longest read (2 * SampleDelay with full modulation).
		ModulatedAllpass(int bufferSize, int delayBufferSamples, int sampleDelay)
		{
			this->InterpolationEnabled = true;
			this->ModulationEnabled = false;
			this->Feedback = 0.0f;
			this->bufferSize = bufferSize;
			this->delayBufferSamples = delayBufferSamples;
			delayBuffer = new (custom_pool_allocate(sizeof(float) * delayBufferSamples)) float[delayBufferSamples];
			output = new (custom_pool_allocate(sizeof(float) * bufferSize)) float[bufferSize];
			SampleDelay = sampleDelay;
			index = delayBufferSamples - 1;
			written = 0;  // nothing is read from the ring before it is written
			modPhase = FastSin::RandomPhase();
			ModRate = 0.0f;
			ModAmount = 0.0f;
			Update();
		}

		~ModulatedAllpass()
		{
			// delayBuffer/output are placement-new'd into the SDRAM bump-allocator
			// pool (custom_pool_allocate) and are never freed; nothing to delete here.
		}


		inline float* GetOutput()
		{
			return output;
		}

		// Constant time: the ring is not swept. Samples written before the clear are
		// never read again: until the whole ring has been rewritten, ProcessFresh()
		// reads every slot not yet written since the clear as 0. A diffusion enable
		// flip calls this from the audio callback, where zeroing the ring cost up to
		// ~1 MB of SDRAM writes.
		void ClearBuffers()
		{
			written = 0;
			Utils::ZeroBuffer(output, bufferSize);
		}

		void Process(const float* input, int sampleCount)
		{
			if (written < delayBufferSamples)
				ProcessFresh(input, sampleCount);
			else if (ModulationEnabled)
				ProcessWithMod(input, sampleCount);
			else
				ProcessNoMod(input, sampleCount);
		}


	private:
		// Ring slot `distance` (>= 1) samples behind the write index, 0 if it has not
		// been written since the last clear.
		float Fresh(int distance) const
		{
			if (distance > written)
				return 0.0f;
			int i = index - distance;
			if (i < 0)
				i += delayBufferSamples;
			return delayBuffer[i];
		}

		// Process() for the first delayBufferSamples samples after a clear (~200 ms):
		// the same allpass as ProcessWithMod() / ProcessNoMod(), reading through
		// Fresh(). Out of line, it is off the steady-state path.
		[[gnu::noinline]] void ProcessFresh(const float* input, int sampleCount)
		{
			for (int i = 0; i < sampleCount; i++)
			{
				float bufOut;
				if (ModulationEnabled)
				{
					if (samplesProcessed >= ModulationUpdateRate)
						Update();
					bufOut = InterpolationEnabled
						? Fresh(delayA) * gainA + Fresh(delayB) * gainB
						: Fresh(delayA);
				}
				else
				{
					bufOut = Fresh(SampleDelay);
				}

				auto inVal = input[i] + bufOut * Feedback;
				delayBuffer[index] = inVal;
				output[i] = bufOut - inVal * Feedback;

				index++;
				if (index >= delayBufferSamples) index -= delayBufferSamples;
				samplesProcessed++;
				written++;
			}
			if (written > delayBufferSamples)
				written = delayBufferSamples;
		}

		void ProcessNoMod(const float* input, int sampleCount)
		{
			float* const buf = delayBuffer;
			float* const out = output;
			const int size = delayBufferSamples;
			const float feedback = Feedback;
			int idx = index;
			int delayedIndex = idx - SampleDelay;
			if (delayedIndex < 0) delayedIndex += size;

			for (int i = 0; i < sampleCount; i++)
			{
				auto bufOut = buf[delayedIndex];
				auto inVal = input[i] + bufOut * feedback;

				buf[idx] = inVal;
				out[i] = bufOut - inVal * feedback;

				idx++;
				delayedIndex++;
				if (idx >= size) idx -= size;
				if (delayedIndex >= size) delayedIndex -= size;
			}
			index = idx;
			samplesProcessed += sampleCount;
		}

		void ProcessWithMod(const float* input, int sampleCount)
		{
			// Locals in each segment between Update()s, as in ModulatedDelay::Process.
			int i = 0;
			while (i < sampleCount)
			{
				if (samplesProcessed >= ModulationUpdateRate)
					Update();
				int end = i + (ModulationUpdateRate - (int)samplesProcessed);
				if (end > sampleCount)
					end = sampleCount;
				samplesProcessed += end - i;

				float* const buf = delayBuffer;
				float* const out = output;
				const int size = delayBufferSamples;
				const int dA = delayA, dB = delayB;
				const float gA = gainA, gB = gainB;
				const float feedback = Feedback;
				int idx = index;

				if (InterpolationEnabled)
				{
					for (; i < end; i++)
					{
						int idxA = idx - dA;
						int idxB = idx - dB;
						idxA += size * (idxA < 0); // modulo
						idxB += size * (idxB < 0); // modulo

						auto bufOut = buf[idxA] * gA + buf[idxB] * gB;
						auto inVal = input[i] + bufOut * feedback;
						buf[idx] = inVal;
						out[i] = bufOut - inVal * feedback;

						idx++;
						if (idx >= size) idx -= size;
					}
				}
				else
				{
					for (; i < end; i++)
					{
						int idxA = idx - dA;
						idxA += size * (idxA < 0); // modulo

						auto bufOut = buf[idxA];
						auto inVal = input[i] + bufOut * feedback;
						buf[idx] = inVal;
						out[i] = bufOut - inVal * feedback;

						idx++;
						if (idx >= size) idx -= size;
					}
				}
				index = idx;
			}
		}

		void Update()
		{
			auto mod = FastSin::Step(modPhase, ModRate * ModulationUpdateRate);

			if (ModAmount >= SampleDelay) // don't modulate to negative value
				ModAmount = SampleDelay - 1;


			auto totalDelay = SampleDelay + ModAmount * mod;
			
			if (totalDelay <= 0) // should no longer be required
				totalDelay = 1;

			delayA = (int)totalDelay;
			delayB = (int)totalDelay + 1;

			auto partial = totalDelay - delayA;

			gainA = 1 - partial;
			gainB = partial;

			samplesProcessed = 0;
		}

	};
}

#endif
