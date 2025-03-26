#pragma once
#include "Gui/InputMapper.hpp"
#include "Gui/View.hpp"

typedef struct SDL_Texture SDL_Texture;
class Tool;

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

		SDL_Texture *toolAtlasTexture = nullptr;
		void UpdateToolAtlasTexture(bool rendererUp);

		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;
		void SetRendererUp(bool newRendererUp) final override;
		void SetOnTop(bool newOnTop) final override;

		std::vector<const GameToolInfo *> matchingTools;
		void Update();

		void InitActions();

	public:
		ElementSearch(Game &newGame);

		bool CanTryOk() const;
		void Ok();
	};
}
