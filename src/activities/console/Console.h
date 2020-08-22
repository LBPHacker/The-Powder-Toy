#pragma once

#include "gui/PopupWindow.h"

namespace gui
{
	class Label;
	class Static;
	class TextBox;
	class Button;
	class ScrollPanel;
}

namespace activities
{
namespace console
{
	class Resizer;

	class Console : public gui::PopupWindow
	{
		enum CommandResult
		{
			commandOk,
			commandOkCloseConsole,
			commandError,
			commandIncomplete,
		};

		struct HistoryEntry
		{
			Console::CommandResult cr;
			String input;
			String output;
			gui::Static *okStatic;
			gui::Label *inputLabel;
			gui::Label *outputLabel;
		};
		std::list<HistoryEntry> history;
		std::list<HistoryEntry>::iterator editing;
		String inputEditingBackup;
		String prevLines;

		gui::ScrollPanel *sp = nullptr;
		gui::TextBox *inputBox = nullptr;
		Resizer *resizer = nullptr;

		void ScrollEntryIntoView(HistoryEntry *entry);
		void UpdateEntryLayout();
		void UpdateLayout();
		Console::CommandResult Validate(const String &input, String &output);
		Console::CommandResult Execute(const String &input, String &output);
		static String FormatInput(const String &str);
		static String FormatOutput(const String &str);
		void HistoryPush(Console::CommandResult cr, String input, String output, String formattedInput, String formattedOutput);

	public:
		Console();

		void Draw() const final override;
		bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;
	};
}
}
