
#include <cmath>
#include "ValueTables.h"
#include <assert.h>

namespace AudioLib
{
	// octave response. value float every step (3 or 4 steps)
	float DSY_SDRAM_BSS ValueTables::Response3Oct[TableSize];
	float DSY_SDRAM_BSS ValueTables::Response4Oct[TableSize];

	// decade response, value multiplies by 10 every step
	float DSY_SDRAM_BSS ValueTables::Response2Dec[TableSize];
	float DSY_SDRAM_BSS ValueTables::Response3Dec[TableSize];

	void ValueTables::Init()
	{
		// Initialize tables

		for (int i = 0; i <= 4000; i++)
		{
			const double x = (float)(i / 4000.0);

			Response3Oct[i] = (std::pow(8.0, x) - 1.0) / 8.0 + 0.125;
			Response4Oct[i] = (std::pow(16.0, x) - 1.0) / 16.0 + 0.125 / 2.0;

			Response2Dec[i] = std::pow(100.0, x) / 100.0;
			Response3Dec[i] = std::pow(1000.0, x) / 1000.0;
		}

		for (int i = 1; i <= 4000; i++)
		{
			Response3Oct[i] = (Response3Oct[i] - Response3Oct[0]) / (1 - Response3Oct[0]);
			Response4Oct[i] = (Response4Oct[i] - Response4Oct[0]) / (1 - Response4Oct[0]);
			Response2Dec[i] = (Response2Dec[i] - Response2Dec[0]) / (1 - Response2Dec[0]);
			Response3Dec[i] = (Response3Dec[i] - Response3Dec[0]) / (1 - Response3Dec[0]);
		}

		Response3Oct[0] = 0;
		Response4Oct[0] = 0;
		Response2Dec[0] = 0;
		Response3Dec[0] = 0;
	}

	float ValueTables::Get(float index, float* table)
	{
		int idx = (int)((double)index * 4000.999);

		assert(idx >= 0);
		assert(idx < AudioLib::ValueTables::TableSize);
		

		return table[idx];
	}
}
