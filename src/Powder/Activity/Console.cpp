#include "Console.hpp"
#include "Game.hpp"
#include "Gui/Host.hpp"
#include "lua/CommandInterface.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr int32_t maxHistoryEntryCount              =  20;
		constexpr Gui::View::Size minInputOutputColumnWidth = 150;
		constexpr Gui::View::Size columnDraggerWidth        =   4;
		constexpr Gui::View::Size viewDraggerHeight         =   4;
		constexpr Gui::View::Size columnPadding             =   5;
		constexpr Gui::View::Size minViewHeight             = 150;
		constexpr Gui::View::Size inputMaxHeight            =  60;
	}

	Console::Console(Game &newGame) : View(newGame.GetHost()), game(newGame)
	{
	}

	void Console::PushHistoryEntry(HistoryEntry historyEntry)
	{
		history.push_back(std::move(historyEntry));
		while (int32_t(history.size()) > maxHistoryEntryCount)
		{
			history.pop_front();
			firstHistoryEntryIndex += 1;
		}
	}

	void Console::UpdateInput()
	{
		inputFormatted = game.GetCommandInterface().FormatCommand(ByteString(input).FromUtf8()).ToUtf8();
	}

	void Console::Gui()
	{
		auto console = ScopedVPanel("console");
		auto &g = GetHost();
		SetTitle("Console"); // TODO-REDO_UI-TRANSLATE
		SetRootRect(g.GetSize().OriginRect());
		SetLayered(true);
		auto padding = g.GetCommonMetrics().padding;
		{
			SetAlignment(Gui::Alignment::top);
			auto inner = ScopedVPanel("inner");
			SetSize(viewHeightExtra + minViewHeight + viewDraggerHeight);
			SetPadding(padding, viewDraggerHeight, padding, padding);
			SetSpacing(Common{});
			auto r = GetRect();
			g.FillRect(r, 0xA0000000_argb);
			{
				auto historyBorder = ScopedVPanel("historyBorder");
				auto historyRect = GetRect();
				auto columnSpacing = columnPadding * 2 + columnDraggerWidth + 2;
				auto inputColumnWidth = inputColumnWidthExtra + minInputOutputColumnWidth;
				auto inputWidth = inputColumnWidth - columnPadding * 2;
				SetPadding(1);
				{
					auto entriesPanel = ScopedScrollpanel("history");
					if (scrollToBottom)
					{
						ScrollpanelSetScroll({ 0, -100000 });
					}
					SetMaxSize(MaxSizeFitParent{});
					auto inner = ScopedVPanel("inner");
					SetLayered(true);
					{
						auto historyPanel = ScopedVPanel("entries");
						SetAlignment(Gui::Alignment::bottom);
						int32_t index = 0;
						auto pushEntry = [&](StringView input, StringView output) {
							{
								auto entryPanel = ScopedHPanel(firstHistoryEntryIndex + index * 2);
								SetTextAlignment(Gui::Alignment::left, Gui::Alignment::top);
								SetParentFillRatio(0);
								SetPadding(columnPadding);
								SetSpacing(columnSpacing);
								BeginText("input", input, TextFlags::autoHeight | TextFlags::multiline | TextFlags::selectable);
								SetSize(inputWidth);
								EndText();
								BeginText("output", output, TextFlags::autoHeight | TextFlags::multiline | TextFlags::selectable);
								EndText();
							}
							if (index + 1 < int32_t(history.size()))
							{
								Separator(firstHistoryEntryIndex + index * 2 + 1);
							}
							index += 1;
						};
						for (auto &entry : history)
						{
							pushEntry(entry.input, entry.output);
						}
						if (history.empty())
						{
							pushEntry("\bgInput will appear here", "\bgOutput will appear here"); // TODO-REDO_UI-TRANSLATE
						}
					}
					auto draggerPanel = ScopedHPanel("draggerPanel");
					SetPadding(minInputOutputColumnWidth, 0);
					auto draggerInner = ScopedHPanel("draggerInner");
					auto dragger = ScopedDragger("dragger", inputColumnWidthExtra);
					SetSize(columnDraggerWidth);
					auto draggerRect = GetRect();
					g.FillRect({ draggerRect.pos + Pos2{ 0, 0 }, draggerRect.size - Size2{ 0, 0 } }, 0xFF000000_argb);
					g.FillRect({ draggerRect.pos + Pos2{ 1, 0 }, draggerRect.size - Size2{ 2, 0 } }, 0xFF808080_argb);
				}
				g.DrawRect({ historyRect.pos, { inputColumnWidth + 1, historyRect.size.Y } }, 0xFFFFFFFF_argb);
				g.DrawRect({ historyRect.pos + Pos2{ inputColumnWidth + columnDraggerWidth + 1, 0 }, { historyRect.size.X - inputColumnWidth - columnDraggerWidth - 1, historyRect.size.Y } }, 0xFFFFFFFF_argb);
			}
			{
				auto inputPanel = ScopedHPanel("inputPanel");
				SetSpacing(Common{});
				SetAlignmentSecondary(Gui::Alignment::bottom);
				SetParentFillRatio(0);
				BeginTextbox("input", input, inputFormatted, "[command]", TextboxFlags::multiline); // TODO-REDO_UI-TRANSLATE
				SetTextPadding(columnPadding, 2);
				SetMaxSize(MaxSizeFitParent{});
				SetMaxSizeSecondary(inputMaxHeight);
				if (focusInput)
				{
					GiveInputFocus();
					focusInput = false;
				}
				if (EndTextbox())
				{
					UpdateInput();
				}
				BeginButton("submit", "Submit", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
				SetSizeSecondary(Common{});
				SetSize(g.GetCommonMetrics().smallButton);
				SetEnabled(GetDisposition() == DispositionFlags::none);
				if (EndButton())
				{
					Submit();
				}
			}
		}
		Rect draggerRect{ { 0, 0 }, { 0, 0 } };
		{
			auto draggerPanel = ScopedVPanel("draggerPanel");
			SetPadding(minViewHeight, 0, 0, 0);
			auto draggerInner = ScopedVPanel("draggerInner");
			auto dragger = ScopedDragger("dragger", viewHeightExtra);
			SetSize(viewDraggerHeight);
			draggerRect = GetRect();
			g.FillRect({ draggerRect.pos + Pos2{ 1 + padding, 1 }, draggerRect.size - Size2{ 2 + 2 * padding, 2 } }, 0xFF808080_argb);
		}
		g.FillRect({ draggerRect.pos + Pos2{ 0, viewDraggerHeight     }, { draggerRect.size.X, 1 } }, 0xFFFFFFFF_argb);
		g.FillRect({ draggerRect.pos + Pos2{ 0, viewDraggerHeight + 1 }, { draggerRect.size.X, 1 } }, 0xFF000000_argb);
	}

	bool Console::CanSubmit() const
	{
		return !input.empty();
	}

	void Console::Submit()
	{
		auto &commandInterface = game.GetCommandInterface();
		auto returnCode = commandInterface.Command(ByteString(input).FromUtf8());
		PushHistoryEntry({ inputFormatted, returnCode, commandInterface.GetLastError().ToUtf8() });
		input.clear();
		UpdateInput();
		focusInput = true;
		scrollToBottomNext = true;
	}

	void Console::Ok()
	{
		Submit();
	}

	Console::DispositionFlags Console::GetDisposition() const
	{
		if (!CanSubmit())
		{
			return DispositionFlags::okDisabled;
		}
		return DispositionFlags::none;
	}

	void Console::Open()
	{
		focusInput = true;
	}

	void Console::HandleBeforeGui()
	{
		scrollToBottom = scrollToBottomNext;
		scrollToBottomNext = false;
	}
}
