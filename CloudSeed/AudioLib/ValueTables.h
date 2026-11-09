#ifndef AUDIOLIB_VALUETABLES
#define AUDIOLIB_VALUETABLES

#include "MathDefs.h"


namespace AudioLib
{
	class ValueTables
	{
	public:
		// Get() reads indices 0..4000 (index * 4000.999 for index 0..1).
		static const int TableSize = 4001;


		// octave response: the value doubles every step (3 or 4 octaves)
		static DSY_SDRAM_BSS float Response3Oct[TableSize];
		static DSY_SDRAM_BSS float Response4Oct[TableSize];

		// decade response, value multiplies by 10 every step
		static DSY_SDRAM_BSS float Response2Dec[TableSize];
		static DSY_SDRAM_BSS float Response3Dec[TableSize];

		static void Init();
		static float Get(float index, float* table);
	};
}

#endif
