#include "Console.h"

#include "graphics/FontData.h"
#include "activities/game/Game.h"
#include "gui/SDLWindow.h"
#include "gui/Appearance.h"
#include "gui/Button.h"
#include "gui/ScrollPanel.h"
#include "gui/Label.h"
#include "gui/TextBox.h"
#include "gui/Icons.h"
#include "common/tpt-minmax.h"
#include "lua/CommandInterface.h"

namespace activities
{
namespace console
{
	constexpr auto windowHeight = 150;
	constexpr auto dividerLeft = 250;
	constexpr auto historySize = 100;

	Console::Console()
	{
		editing = history.end();

		auto gameSize = game::Game::Ref().Size();

		Size(gui::Point{ gameSize.x, windowHeight });
		Position(gui::Point{ 0, 0 });

		sp = EmplaceChild<gui::ScrollPanel>().get();
		sp->Position(gui::Point{ 0, 0 });
		sp->AlignBottom(true);

		inputBox = EmplaceChild<gui::TextBox>().get();
		inputBox->Size({ gameSize.x - 6, 16 });
		inputBox->DrawBody(false);
		inputBox->PlaceHolder("[command]");
		inputBox->Change([this]() {
			auto &input = inputBox->Text();
			String output;
			Validate(input, output);
			// * TODO-REDO_UI: implement validation
		});
		inputBox->Format(FormatInput);

		UpdateLayout();
		inputBox->Focus();
	}

	void Console::HistoryPush(Console::CommandResult cr, String input, String output, String formattedInput, String formattedOutput)
	{
		auto spi = sp->Interior();
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
		case Console::commandOk:
		case Console::commandOkCloseConsole:
			okStatic->Text(gui::IconString(gui::Icons::settings, gui::Point{ 0, 0 }));
			break;

		case Console::commandError:
			okStatic->Text(gui::CommonColorString(gui::commonLightRed) + gui::IconString(gui::Icons::cross, gui::Point{ 0, 0 }));
			break;

		case Console::commandIncomplete:
			break;
		}
		okStatic->Size(gui::Point{ 13, 17 });
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

	void Console::Draw() const
	{
		auto abs = AbsolutePosition();
		auto size = Size();
		auto &c = gui::Appearance::colors.inactive.idle;
		auto &g = gui::SDLWindow::Ref();
		g.DrawRect(gui::Rect{ abs, size - gui::Point{ 0, 1 } }, gui::Color{ 0x00, 0x00, 0x00, 0xC0 });
		g.DrawLine(abs + gui::Point{ dividerLeft, 0 }, abs + gui::Point{ dividerLeft, size.y - 15 }, c.border);
		g.DrawLine(abs + gui::Point{ 0, size.y - 15 }, abs + gui::Point{ size.x, size.y - 15 }, c.border);
		g.DrawLine(abs + gui::Point{ 0, size.y -  1 }, abs + gui::Point{ size.x, size.y -  1 }, c.border);
		g.DrawLine(abs + gui::Point{ 0, size.y      }, abs + gui::Point{ size.x, size.y      }, c.shadow);
		ModalWindow::Draw();
	}

	void Console::UpdateEntryLayout()
	{
		auto top = 1;
		for (auto &entry : history)
		{
			entry.inputLabel->Size(gui::Point{ dividerLeft - 6, 0 });
			entry.outputLabel->Size(gui::Point{ Size().x - dividerLeft - 27, 0 });
			int lines = std::max(entry.inputLabel->Lines(), entry.outputLabel->Lines());
			auto height = lines * FONT_H + 5;
			entry.inputLabel->Size(gui::Point{ entry.inputLabel->Size().x, height });
			entry.outputLabel->Size(gui::Point{ entry.outputLabel->Size().x, height });
			entry.okStatic->Position(gui::Point{ dividerLeft + 5, top - 1 });
			entry.inputLabel->Position(gui::Point{ 3, top });
			entry.outputLabel->Position(gui::Point{ dividerLeft + 19, top });
			top += height;
		}
		sp->Interior()->Size(gui::Point{ Size().x, top + 3 });
	}

	void Console::ScrollEntryIntoView(HistoryEntry *entry)
	{
		auto *label = entry->inputLabel;
		sp->Offset(gui::Point{ 0, sp->Size().y - (label->Position().y + label->Size().y + 3) });
	}

	void Console::UpdateLayout()
	{
		sp->Size(gui::Point{ Size().x, Size().y - 15 });
		inputBox->Position(gui::Point{ 3, Size().y - 16 });
	}

	Console::CommandResult Console::Validate(const String &input, String &output)
	{
		// * TODO-REDO_UI: implement validation
		return commandOk;
	}

	Console::CommandResult Console::Execute(const String &input, String &output)
	{
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
}
