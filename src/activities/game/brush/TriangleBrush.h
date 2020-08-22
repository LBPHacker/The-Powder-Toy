#pragma once

#include "Brush.h"

namespace activities
{
namespace game
{
namespace brush
{
	class TriangleBrush : public Brush
	{
	public:
		TriangleBrush()
		{
			RequestedRadius(gui::Point{ 0, 0 });
		}

		void GenerateBody() final override;
	};
}
}
}
