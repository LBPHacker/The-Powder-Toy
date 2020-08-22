#pragma once

#include "Brush.h"

namespace activities
{
namespace game
{
namespace brush
{
	class CellAlignedBrush : public Brush
	{
		void UpdateEffectiveRadius() final override;

	public:
		CellAlignedBrush()
		{
			RequestedRadius(gui::Point{ 0, 0 });
		}

		void GenerateBody() final override;

		bool CellAligned() const final override
		{
			return true;
		}
	};
}
}
}
