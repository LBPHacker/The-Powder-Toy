#pragma once
#include "Saver.hpp"
#include <future>

class SaveInfo;

namespace Powder::Activity
{
	class Game;

	class OnlineSaver : public Saver
	{
		std::unique_ptr<SaveInfo> saveInfo;
		std::future<int> saveIDFuture;
		std::string description;

		ByteString GuiTitle() final override;
		void Save() final override;
		void HandleTick() final override;
		void GuiRight() final override;

	public:
		OnlineSaver(Game &newGame, std::unique_ptr<SaveInfo> newSaveInfo, bool newOverwrite);
	};
}
