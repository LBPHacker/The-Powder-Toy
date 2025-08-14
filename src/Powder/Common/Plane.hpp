#pragma once
#include "common/Plane.h"

namespace Powder
{
	// TODO-REDO_UI: move definition here
	template<typename T, size_t Width = DynamicExtent, size_t Height = DynamicExtent>
	using PlaneAdapter = ::PlaneAdapter<T, Width, Height>;
}
