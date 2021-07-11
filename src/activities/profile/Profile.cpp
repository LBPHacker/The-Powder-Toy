#include "Profile.h"

#include "common/Platform.h"
#include "gui/Appearance.h"
#include "gui/Button.h"
#include "gui/ImageButton.h"
#include "gui/Label.h"
#include "gui/TextBox.h"
#include "gui/ScrollPanel.h"
#include "gui/Separator.h"

namespace activities
{
namespace profile
{
	constexpr auto parentSize = gui::Point{ WINDOWW, WINDOWH };
	constexpr auto windowSize = gui::Point{ 236, 300 };

	Profile::Profile(bool editable)
	{
		Size(windowSize);
		Position((parentSize - windowSize) / 2);

		{
			auto title = EmplaceChild<gui::Static>();
			title->Position(gui::Point{ 6, 5 });
			title->Size(gui::Point{ windowSize.x - 20, 14 });
			title->Text(editable ? String("Edit profile") : String("View profile"));
			title->TextColor(gui::Appearance::informationTitle);
			auto sep = EmplaceChild<gui::Separator>();
			sep->Position(gui::Point{ 1, 22 });
			sep->Size(gui::Point{ windowSize.x - 2, 1 });
			auto ok = EmplaceChild<gui::Button>();
			if (editable)
			{
				auto floorW2 = windowSize.x / 2;
				auto ceilW2 = windowSize.x - floorW2;
				ok->Text("\boSave");
				ok->Position(gui::Point{ ceilW2 - 1, windowSize.y - 16 });
				ok->Size(gui::Point{ floorW2 + 1, 16 });
				ok->Click([this]() {
					// * TODO-REDO_UI: save
					Quit();
				});
				auto cancel = EmplaceChild<gui::Button>();
				cancel->Text("Cancel");
				cancel->Position(gui::Point{ 0, windowSize.y - 16 });
				cancel->Size(gui::Point{ ceilW2, 16 });
				cancel->Click([this]() {
					Quit();
				});
				CancelButton(cancel);
				OkayButton(ok);
			}
			else
			{
				ok->Text("\boOK");
				ok->Position(gui::Point{ 0, windowSize.y - 16 });
				ok->Size(gui::Point{ windowSize.x, 16 });
				ok->Click([this]() {
					Quit();
				});
				CancelButton(ok);
				OkayButton(ok);
			}
		}

		auto ailNextY = 31;
		auto addInfoLine = [this, &ailNextY](int labelSize, String labelText, String infoText) {
			auto *label = EmplaceChild<gui::Static>().get();
			label->Text(labelText);
			label->Position(gui::Point{ 8, ailNextY });
			label->Size(gui::Point{ labelSize, 15 });
			label->TextColor(gui::Appearance::commonLightGray);
			auto *info = EmplaceChild<gui::Label>().get();
			info->Text(infoText);
			info->Position(gui::Point{ 8 + labelSize, ailNextY });
			info->Size(gui::Point{ windowSize.x - 56 - labelSize, 15 });
			ailNextY += 15;
		};

		addInfoLine(50, "Username:", "LBPHacker");
		addInfoLine(50, "Location:", "Budapest, Hungary");
		addInfoLine(50, "Website:", "https://trigraph.net/");
		addInfoLine(75, "Save count:", "47");
		addInfoLine(75, "Average score:", "70.1277");
		addInfoLine(75, "Highest score:", "439");


		{
			auto *label = EmplaceChild<gui::Static>().get();
			label->Text("Biography:");
			label->Position(gui::Point{ 8, ailNextY });
			label->Size(gui::Point{ 50, 15 });
			label->TextColor(gui::Appearance::commonLightGray);
			auto *button = EmplaceChild<gui::Button>().get();
			button->Text("Edit other details");
			button->Size(gui::Point{ 100, 15 });
			button->Position(gui::Point{ windowSize.x - 8 - button->Size().x, ailNextY });
			button->Click([]() {
				Platform::OpenURI(SCHEME SERVER "/Profile.html");
			});
			ailNextY += 15;
		}

		{
			auto *image = EmplaceChild<gui::ImageButton>().get();
			image->Size(gui::Point{ 40, 40 });
			image->Position(gui::Point{ windowSize.x - 8 - image->Size().x, 31 });
			auto *button = EmplaceChild<gui::Button>().get();
			button->Text("Change");
			button->Size(gui::Point{ 40, 15 });
			button->Position(gui::Point{ windowSize.x - 8 - button->Size().x, 75 });
			button->Click([]() {
				Platform::OpenURI(SCHEME SERVER "/Profile/Avatar.html");
			});
		}
	}
}
}
