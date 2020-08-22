#pragma once

#include "Tool.h"

namespace activities
{
namespace game
{
namespace tool
{
	class ElementTool : public Tool
	{
		int elem;

	public:
		ElementTool(int elem) : elem(elem)
		{
		}

		void Draw(gui::Point pos) const final override;
		void Fill(gui::Point pos) const final override;
		const String &Name() const override;
		const String &Description() const override;
		gui::Color Color() const override;
		bool MenuVisible() const override;
		int MenuSection() const final override;
		int MenuPrecedence() const final override;

		bool OptimizeLines() const final override
		{
			return true;
		}

		bool AllowReplace() const final override
		{
			return true;
		}

		bool CanGenerateTexture() const final override;
		void GenerateTexture(gui::Point size, uint32_t *pixels) const final override;
	};
}
}
}
