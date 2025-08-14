#pragma once
#include "Gui/InputMapper.hpp"
#include "Gui/View.hpp"

typedef struct SDL_Texture SDL_Texture;
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
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;

		std::vector<const GameToolInfo *> matchingTools;
		void Update();

		void InitActions();

	public:
		ElementSearch(Game &newGame);
	};
}
