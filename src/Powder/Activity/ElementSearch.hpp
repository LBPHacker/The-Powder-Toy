#pragma once
#include "Gui/InputMapper.hpp"
#include "Gui/View.hpp"

class Tool;

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	class Game;
	struct GameToolInfo;

	class ElementSearch : public Gui::View, public Gui::InputMapper
	{
		Game &game;
		std::string query;
		Tool *lastHoveredTool = nullptr;
		bool focusQuery = true;

		std::unique_ptr<Gui::StaticTexture> toolAtlasTexture;

		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;
		void SetOnTop(bool newOnTop) final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;

		std::vector<const GameToolInfo *> matchingTools;
		void Update();

		void InitActions();

	public:
		ElementSearch(Game &newGame);
	};
}
