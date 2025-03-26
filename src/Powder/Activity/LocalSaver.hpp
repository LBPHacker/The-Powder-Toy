#pragma once
#include "Saver.hpp"

class SaveFile;

namespace Powder::Activity
{
	class Game;

	class LocalSaver : public Saver
	{
		std::unique_ptr<SaveFile> saveFile;

		ByteString GuiTitle() final override;
		void Save() final override;

	public:
		LocalSaver(Game &newGame, std::unique_ptr<SaveFile> newSaveFile, bool newOverwrite);
	};
}
