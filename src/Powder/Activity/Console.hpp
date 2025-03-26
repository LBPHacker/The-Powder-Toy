#pragma once
#include "Gui/View.hpp"
#include <deque>

namespace Powder::Activity
{
	class Game;

	class Console : public Gui::View
	{
		Game &game;

		struct HistoryEntry
		{
			std::string input;
			int32_t returnCode;
			std::string output;
		};
		std::deque<HistoryEntry> history;
		int32_t firstHistoryEntryIndex = 0;

		Pos inputColumnWidthExtra = 50;
		Pos viewHeightExtra = 0;

		std::string input;
		std::string inputFormatted;
		void UpdateInput();
		bool focusInput = false;
		bool scrollToBottom = false;
		bool scrollToBottomNext = true;
		bool CanSubmit() const;
		void Submit();

		void Gui() final override;
		void HandleBeforeGui() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;

		void PushHistoryEntry(HistoryEntry historyEntry);

	public:
		Console(Game &newGame);

		void Open();
	};
}
