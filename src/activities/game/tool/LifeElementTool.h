#pragma once

#include "ElementTool.h"

namespace activities
{
namespace game
{
namespace tool
{
	class LifeElementTool : public ElementTool
	{
		String name;
		String description;
		gui::Color color;

	public:
		LifeElementTool(int kind, String name, String description, gui::Color color);

		bool MenuVisible() const final override
		{
			return true;
		}

		const String &Name() const final override
		{
			return name;
		}

		const String &Description() const final override
		{
			return description;
		}

		gui::Color Color() const final override
		{
			return color;
		}
	};
}
}
}
