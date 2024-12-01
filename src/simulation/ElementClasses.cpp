#include "ElementCommon.h"

const std::array<Element, PT_NUM> &GetElements()
{
	struct DoOnce
	{
		std::array<Element, PT_NUM> elements;

		DoOnce()
		{
#define ELEMENT_NUMBERS_CALL
#include "ElementNumbers.h"
#undef ELEMENT_NUMBERS_CALL
		}
	};

	static DoOnce doOnce;
	return doOnce.elements;
}
