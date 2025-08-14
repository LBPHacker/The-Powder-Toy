#pragma once
#include "Gui/View.hpp"
#include "simulation/Sign.h"
#include <optional>

namespace Powder::Activity
{
	class Game;

	class Sign : public Gui::View
	{
		Game &game;
		std::optional<int32_t> signIndex;
		bool focusSignText = true;
		std::string signText;
		int32_t signJustification = sign::Middle;
		Pos2 signPos{ 0, 0 };

		void MoveSign();
		bool movingSign = false;

		bool HandleEvent(const SDL_Event &event) final override;
		void Gui() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;
		sign GetSignInfo() const;

	public:
		Sign(Game &newGame, std::optional<int32_t> newSignIndex, Pos2 newSignPos);
	};
}
