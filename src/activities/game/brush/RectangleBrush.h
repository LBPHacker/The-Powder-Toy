#pragma once

#include "Brush.h"

namespace activities
{
namespace game
{
namespace brush
{
	class RectangleBrush : public Brush
	{
	public:
		RectangleBrush()
		{
			RequestedRadius(gui::Point{ 0, 0 });
		}

		void GenerateBody() final override;
	};
}
}
}
