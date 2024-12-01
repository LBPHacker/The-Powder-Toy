#include "X86KillDenormals.h"
#include <pmmintrin.h>
#include <xmmintrin.h>

void X86KillDenormals()
{
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
}
