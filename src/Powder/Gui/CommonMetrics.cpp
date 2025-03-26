#include "CommonMetrics.hpp"

namespace Powder::Gui
{
	CommonMetrics::CommonMetrics()
	{
		size            =  17;
		padding         =   3;
		spacing         =   3;
		smallButton     =  50;
		bigButton       =  70;
		hugeButton      = 100;
		smallSpinner    =  30;
		bigSpinner      =  45;
		hugeSpinner     =  60;
		heightToFitSize = size + 2 * padding;
	};
}
