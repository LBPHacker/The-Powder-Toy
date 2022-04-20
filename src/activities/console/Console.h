#pragma once

#include "gui/PopupWindow.h"

namespace gui
{
	class Label;
	class Static;
	class TextBox;
	class Button;
	class ScrollPanel;
	class Dragger;
	class Separator;
}

namespace activities::console
{
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
			CommandResult cr;
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
		gui::Dragger *bottomDragger = nullptr;
		gui::Dragger *splitDragger = nullptr;
		gui::Separator *inputBoxBorder = nullptr;

		void ScrollEntryIntoView(HistoryEntry *entry);
		void UpdateLayout();
		void UpdateEntryLayout();
		CommandResult Validate(const String &input, String &output);
		CommandResult Execute(const String &input, String &output);
		static String FormatInput(const String &str);
		static String FormatOutput(const String &str);
		void HistoryPush(CommandResult cr, String input, String output, String formattedInput, String formattedOutput);
		void UpdatePlaceHolder(CommandResult cr);

	public:
		Console();

		bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) final override;
	};
}
