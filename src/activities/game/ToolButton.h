#pragma once

#include "gui/Button.h"

namespace activities
{
namespace game
{
namespace tool
{
	class Tool;
}

	class ToolButton : public gui::Button
	{
		String identifier;
		tool::Tool *tool;
		bool allowReplace;
		bool favorite;
		gui::Color bg;
		gui::Color fg;

		SDL_Texture *texture = nullptr;
		void UpdateTexture();
		void UpdateFavorite();

	public:
		ToolButton(String identifier, tool::Tool *tool);

		void HandleClick(int button) final override;
		void UpdateText() final override;
		void Draw() const final override;
		void RendererUp() final override;
		void RendererDown() final override;

		void ToolTipPosition(gui::Point toolTipPosition);
		virtual void Select(int toolIndex);
	};
}
}
