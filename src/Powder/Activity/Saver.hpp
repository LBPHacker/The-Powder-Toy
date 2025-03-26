#pragma once
#include "Gui/View.hpp"
#include "SaveButton.hpp"
#include <future>

class GameSave;
class VideoBuffer;

namespace Powder
{
	class ThreadPool;
}

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	class Game;

	class Saver : public Gui::View
	{
	protected:
		Game &game;
		ThreadPool &threadPool;
		const GameSave &gameSave;

		std::string title;
		void UpdateItemTitle();

		BrowserItem item;
		std::unique_ptr<Gui::StaticTexture> backgroundImage;

		void HandleTick() override; // Call this at the end of overriding functions.
		bool focusTitle = true;

	private:
		std::future<std::unique_ptr<VideoBuffer>> thumbnailFuture;
		bool interactiveThumbnail;

		void Gui() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;
		virtual ByteString GuiTitle() = 0;
		virtual void Save() = 0;
		virtual void GuiRight();

	public:
		Saver(Game &newGame, const GameSave &newGameSave, bool newOverwrite);
		virtual ~Saver();
	};
}
