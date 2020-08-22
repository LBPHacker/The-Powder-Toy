#include "ButtonMenu.h"

#include "Button.h"

namespace gui
{
	ButtonMenu::ButtonMenu()
	{
	}

	void ButtonMenu::ResizeButtons()
	{
		auto counter = 0;
		for (const auto &button : buttons)
		{
			button->Size(buttonSize);
			button->Position(Point{ 0, (buttonSize.y - 1) * counter });
			counter += 1;
		}
		Size(Point{ buttonSize.x, (buttonSize.y - 1) * counter + 1 });
	}

	void ButtonMenu::Options(const std::vector<String> &newOptions)
	{
		options = newOptions;
		for (const auto &button : buttons)
		{
			RemoveChild(button);
		}
		buttons.clear();
		auto counter = 0;
		for (const auto &option : options)
		{
			auto *button = EmplaceChild<Button>().get();
			button->Text(option);
			button->Click([this, counter]() {
				if (select)
				{
					select(counter);
				}
				Quit();
			});
			buttons.push_back(button);
			counter += 1;
		}
		ResizeButtons();
	}

	void ButtonMenu::ButtonSize(Point newButtonSize)
	{
		buttonSize = newButtonSize;
		ResizeButtons();
	}

	void ButtonMenu::Cancel()
	{
		Quit();
	}
}
