#include "Console.h"

#include "graphics/FontData.h"
#include "activities/game/Game.h"
#include "gui/SDLWindow.h"
#include "gui/Dragger.h"
#include "gui/Appearance.h"
#include "gui/Button.h"
#include "gui/ScrollPanel.h"
#include "gui/Separator.h"
#include "gui/Label.h"
#include "gui/TextBox.h"
#include "gui/Icons.h"
#include "common/tpt-minmax.h"
#include "lua/CommandInterface.h"
#include "language/Language.h"

#include <SDL.h>

namespace activities::console
{
	constexpr auto windowHeightDefault = 157;
	constexpr auto windowMinHeight     = 41;
	constexpr auto inputWidthDefault   = 0; // * This will likely result in the input half being as short as possible.
	constexpr auto inputMinWidth       = 150;
	constexpr auto outputMinWidth      = 150;
	constexpr auto windowSize          = gui::Point{ WINDOWW, WINDOWH };
	constexpr auto historySize         = 100;

	Console::Console()
	{
		editing = history.end();

		Position({ 0, 0 });

		sp = EmplaceChild<gui::ScrollPanel>().get();
		sp->Position({ 0, 1 });
		sp->AlignBottom(true);
		sp->ToFrontUnderMouse(false);

		inputBox = EmplaceChild<gui::TextBox>().get();
		inputBox->Size({ windowSize.x - 6, 18 });
		inputBox->TextRect(inputBox->TextRect().Offset({ 0, 1 }));
		inputBox->DrawBody(false);
		inputBox->DrawBorder(false);
		inputBox->Change([this]() {
			auto &input = inputBox->Text();
			String output;
			Validate(input, output);
			// * TODO-REDO_UI: implement validation
		});
		inputBox->Format(FormatInput);

		inputBoxBorder = EmplaceChild<gui::Separator>().get();
		inputBoxBorder->Size({ windowSize.x - 2, 1 });

		// * TODO-REDO_UI: load history

		splitDragger = EmplaceChild<gui::Dragger>().get();
		splitDragger->Drag([this]() {
			UpdateEntryLayout();
			// * TODO-REDO_UI: Update sp->Offset, it'll jump around like crazy otherwise.
		});

		bottomDragger = EmplaceChild<gui::Dragger>().get();
		bottomDragger->Drag([this]() {
			UpdateLayout();
		});
		bottomDragger->AllowedRect({ { 0, windowMinHeight }, { windowSize.x, windowSize.y - windowMinHeight } });
		bottomDragger->Position({ 0, windowHeightDefault });
		bottomDragger->Size({ windowSize.x, 4 });

		splitDragger->Position({ 1 + inputWidthDefault, 1 });

		UpdatePlaceHolder(commandOk);
		UpdateEntryLayout();
		UpdateLayout();
		inputBox->Focus();
	}

	void Console::UpdatePlaceHolder(CommandResult cr)
	{
		if (cr == commandIncomplete)
		{
			inputBox->PlaceHolder(String::Build("[", "DEFAULT_LS_CONSOLE_CONTINUE"_Ls(), "]"));
		}
		else
		{
			inputBox->PlaceHolder(String::Build("[", "DEFAULT_LS_CONSOLE_COMMAND"_Ls(), "]"));
		}
	}

	void Console::HistoryPush(CommandResult cr, String input, String output, String formattedInput, String formattedOutput)
	{
		auto *spi = sp->Interior();
		while (history.size() >= historySize)
		{
			spi->RemoveChild(history.front().okStatic);
			spi->RemoveChild(history.front().inputLabel);
			spi->RemoveChild(history.front().outputLabel);
			history.pop_front();
		}
		auto *okStatic = spi->EmplaceChild<gui::Static>().get();
		switch (cr)
		{
		case commandOk:
		case commandOkCloseConsole:
			okStatic->Text(gui::IconString(gui::Icons::settings, { 0, 0 }));
			break;

		case commandError:
			okStatic->Text(gui::CommonColorString(gui::commonLightRed) + gui::IconString(gui::Icons::cross, { 0, 0 }));
			break;

		case commandIncomplete:
			break;
		}
		UpdatePlaceHolder(cr);
		okStatic->Size({ 13, 17 });
		auto *inputLabel = spi->EmplaceChild<gui::Label>().get();
		inputLabel->Multiline(true);
		inputLabel->Format([formattedInput](const String &) {
			return formattedInput;
		});
		inputLabel->Text(input);
		auto *outputLabel = spi->EmplaceChild<gui::Label>().get();
		outputLabel->Multiline(true);
		outputLabel->Format([formattedOutput](const String &) {
			return formattedOutput;
		});
		outputLabel->Text(output);
		history.push_back(HistoryEntry{ cr, input, output, okStatic, inputLabel, outputLabel });
		editing = history.end();
	}

	String Console::FormatInput(const String &str)
	{
		return activities::game::Game::Ref().GetCommandInterface()->FormatCommand(str);
	}

	String Console::FormatOutput(const String &str)
	{
		return str;
	}

	bool Console::KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt)
	{
		switch (sym)
		{
		case SDLK_UP:
			if (editing != history.begin())
			{
				(editing == history.end() ? inputEditingBackup : editing->input) = inputBox->Text();
				--editing;
				inputBox->Text(editing->input);
				inputBox->Cursor(int(inputBox->Text().size()));
			}
			return true;

		case SDLK_DOWN:
			if (editing != history.end())
			{
				editing->input = inputBox->Text();
				++editing;
				inputBox->Text(editing == history.end() ? inputEditingBackup : editing->input);
				inputBox->Cursor(int(inputBox->Text().size()));
			}
			return true;

		case SDLK_RETURN:
			if (inputBox->Text().size())
			{
				auto input = inputBox->Text();
				String output;
				auto cr = Execute(input, output);
				HistoryPush(cr, input, output, FormatInput(input), FormatOutput(output));
				UpdateEntryLayout();
				ScrollEntryIntoView(&history.back());
				inputBox->Text("");
				if (cr == commandOkCloseConsole)
				{
					Quit();
				}
			}
			return true;

		default:
			break;
		}
		return ModalWindow::KeyPress(sym, scan, repeat, shift, ctrl, alt);
	}

	void Console::UpdateLayout()
	{
		Size({ windowSize.x, bottomDragger->Position().y });
		MouseForwardRect({ { 0, 0 }, Size() + gui::Point{ 0, 4 } });
		sp->Size({ Size().x, Size().y - 18 });
		splitDragger->Size({ 4, Size().y - 18 });
		splitDragger->AllowedRect({ { 1 + inputMinWidth, 1 }, { windowSize.x - 2 - inputMinWidth - outputMinWidth, Size().y - 18 } });
		inputBox->Position({ 3, Size().y - 18 });
		inputBoxBorder->Position({ 1, Size().y - 17 });
	}

	void Console::UpdateEntryLayout()
	{
		auto top = 1;
		auto x = splitDragger->Position().x;
		for (auto &entry : history)
		{
			entry.inputLabel->Size({ x - 6, 0 });
			entry.outputLabel->Size({ Size().x - x - 29, 0 });
			int lines = std::max(entry.inputLabel->Lines(), entry.outputLabel->Lines());
			auto height = lines * FONT_H + 5;
			entry.inputLabel->Size({ entry.inputLabel->Size().x, height });
			entry.outputLabel->Size({ entry.outputLabel->Size().x, height });
			entry.okStatic->Position({ x + 7, top - 1 });
			entry.inputLabel->Position({ 3, top });
			entry.outputLabel->Position({ x + 21, top });
			top += height;
		}
		sp->Interior()->Size({ Size().x, top + 3 });
	}

	void Console::ScrollEntryIntoView(HistoryEntry *entry)
	{
		auto *label = entry->inputLabel;
		sp->Offset({ 0, sp->Size().y - (label->Position().y + label->Size().y + 3) });
	}

	Console::CommandResult Console::Validate(const String &input, String &output)
	{
		// * TODO-REDO_UI: implement validation
		return commandOk;
	}

	Console::CommandResult Console::Execute(const String &input, String &output)
	{
		// * TODO-REDO_UI: "more input expected, press Enter to cancel"
		auto *cmd = activities::game::Game::Ref().GetCommandInterface();
		auto ret = cmd->Execute(prevLines + input);
		output = cmd->GetLastError();
		if (ret == CommandInterface::commandWantMore)
		{
			prevLines += input + String("\n");
			return commandIncomplete;
		}
		prevLines = "";
		switch (ret)
		{
		case CommandInterface::commandOk:
			return commandOk;

		case CommandInterface::commandOkCloseConsole:
			return commandOkCloseConsole;

		default:
			break;
		}
		return commandError;
	}
}
