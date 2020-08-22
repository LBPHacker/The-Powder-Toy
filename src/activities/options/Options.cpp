#include "Options.h"

#include "common/Platform.h"
#include "gui/SDLWindow.h"
#include "gui/Appearance.h"
#include "gui/Button.h"
#include "gui/CheckBox.h"
#include "gui/DropDown.h"
#include "gui/TextBox.h"
#include "gui/ScrollPanel.h"
#include "gui/Separator.h"
#include "gui/Static.h"
#include "graphics/Pix.h"
#include "simulation/ElementDefs.h"
#include "client/Client.h"
#include "activities/game/Game.h"

float ParseFloatProperty(String value);
int HeatToColour(float temp);

namespace activities
{
namespace options
{
	constexpr auto windowSize = gui::Point{ 320, 340 };
	constexpr auto simDropWidth = 80;

	Options::Options()
	{
		// * TODO-REDO_UI: Figure out where and when to save preferences.
		// * TODO-REDO_UI: data direction migration

		auto &gm = game::Game::Ref();
		Size(windowSize);
		Position((gm.Size() - windowSize) / 2);

		{
			auto title = EmplaceChild<gui::Static>();
			title->Position(gui::Point{ 6, 5 });
			title->Size(gui::Point{ windowSize.x - 20, 14 });
			title->Text("Simulation options");
			title->TextColor(gui::Appearance::informationTitle);
			auto sep = EmplaceChild<gui::Separator>();
			sep->Position(gui::Point{ 1, 22 });
			sep->Size(gui::Point{ windowSize.x - 2, 1 });
			auto ok = EmplaceChild<gui::Button>();
			ok->Position(gui::Point{ 0, windowSize.y - 16 });
			ok->Size(gui::Point{ windowSize.x, 16 });
			ok->Text("\boOK");
			ok->Click([this]() {
				Quit();
			});
			CancelButton(ok);
			OkayButton(ok);
		}

		auto sp = EmplaceChild<gui::ScrollPanel>();
		sp->Position(gui::Point{ 1, 23 });
		sp->Size(windowSize - sp->Position() - gui::Point{ 1, 16 });

		auto spi = sp->Interior(); // * It's bigger on the inside.
		auto interorHeight = 8;

		auto simCheck = [&interorHeight, &spi](String name, String introduced, String description, bool value, gui::CheckBox::ChangeCallback cb) {
			auto checkbox = spi->EmplaceChild<gui::CheckBox>();
			checkbox->Position(gui::Point{ 10, interorHeight });
			checkbox->Size(gui::Point{ windowSize.x - 20, 14 });
			checkbox->Text(name + " \bg" + introduced);
			checkbox->Change(cb);
			checkbox->Value(value);
			auto label = spi->EmplaceChild<gui::Static>();
			label->Position(gui::Point{ 26, interorHeight + 14 });
			label->Size(gui::Point{ windowSize.x - 36, 14 });
			label->Text("\bg" + description);
			interorHeight += 30;
		};

		auto simDrop = [&interorHeight, &spi](String name, std::vector<String> options, int value, gui::DropDown::ChangeCallback cb) {
			auto label = spi->EmplaceChild<gui::Static>();
			label->Position(gui::Point{ 10, interorHeight });
			label->Size(gui::Point{ windowSize.x - 29 - simDropWidth, 14 });
			label->Text(name);
			auto dropdown = spi->EmplaceChild<gui::DropDown>();
			dropdown->Position(gui::Point{ windowSize.x - simDropWidth - 15, interorHeight - 1 });
			dropdown->Size(gui::Point{ simDropWidth, 16 });
			dropdown->Options(options);
			dropdown->Change(cb);
			dropdown->Value(value);
			interorHeight += 20;
		};

		auto separator = [&interorHeight, &spi]() {
			auto sep = spi->EmplaceChild<gui::Separator>();
			sep->Position(gui::Point{ 0, interorHeight - 4 });
			sep->Size(gui::Point{ windowSize.x - 2, 1 });
			interorHeight += 8;
		};

		auto otherCheck = [&interorHeight, &spi](int indent, String name, String description, bool value, gui::CheckBox::ChangeCallback cb) {
			indent *= 15;
			auto checkbox = spi->EmplaceChild<gui::CheckBox>();
			checkbox->Position(gui::Point{ 10 + indent, interorHeight });
			checkbox->Size(gui::Point{ windowSize.x - 20, 14 });
			checkbox->Text(name + " \bg- " + description);
			checkbox->Change(cb);
			checkbox->Value(value);
			interorHeight += 20;
		};

		auto otherDrop = [&interorHeight, &spi](int indent, int width, String description, std::vector<String> options, int value, gui::DropDown::ChangeCallback cb) {
			auto label = spi->EmplaceChild<gui::Static>();
			label->Position(gui::Point{ 15 + width, interorHeight });
			label->Size(gui::Point{ windowSize.x - 30 - width, 14 });
			label->Text("\bg- " + description);
			auto dropdown = spi->EmplaceChild<gui::DropDown>();
			dropdown->Position(gui::Point{ 10, interorHeight - 1 });
			dropdown->Size(gui::Point{ width, 16 });
			dropdown->Options(options);
			dropdown->Change(cb);
			dropdown->Value(value);
			interorHeight += 20;
		};

		auto otherButton = [&interorHeight, &spi](int indent, int width, String title, String description, gui::Button::ClickCallback cb) {
			auto label = spi->EmplaceChild<gui::Static>();
			label->Position(gui::Point{ 15 + width, interorHeight });
			label->Size(gui::Point{ windowSize.x - 30 - width, 14 });
			label->Text("\bg- " + description);
			auto button = spi->EmplaceChild<gui::Button>();
			button->Position(gui::Point{ 10, interorHeight - 1 });
			button->Size(gui::Point{ width, 16 });
			button->Text(title);
			button->Click(cb);
			interorHeight += 20;
		};

		simCheck("Heat simulation", "Introduced in version 34", "Can cause odd behaviour when disabled", gm.LegacyHeat(), [](bool value) {
			game::Game::Ref().LegacyHeat(value);
		});
		simCheck("Ambient heat simulation", "Introduced in version 50", "Can cause odd or broken behaviour with many saves", gm.AmbientHeat(), [](bool value) {
			game::Game::Ref().AmbientHeat(value);
		});
		simCheck("Newtonian gravity", "Introduced in version 48", "May cause poor performance on older computers", gm.NewtonianGravity(), [](bool value) {
			game::Game::Ref().NewtonianGravity(value);
		});
		simCheck("Water equalisation", "Introduced in version 61", "May cause poor performance with a lot of water", gm.WaterEqualisation(), [](bool value) {
			game::Game::Ref().WaterEqualisation(value);
		});
		interorHeight += 8;
		simDrop("Air simulation mode", {
			"On",
			"Pressure off",
			"Velocity off",
			"Off",
			"No update",
		}, int(gm.AirMode()), [](int value) {
			game::Game::Ref().AirMode(game::SimAirMode(value));
		});
		{
			auto label = spi->EmplaceChild<gui::Static>();
			label->Position(gui::Point{ 10, interorHeight });
			label->Size(gui::Point{ windowSize.x - 29 - simDropWidth, 14 });
			label->Text("Ambient Air Temperature");
			auto *tb = spi->EmplaceChild<gui::TextBox>().get();
			tb->Position(gui::Point{ windowSize.x - simDropWidth - 15, interorHeight - 1 });
			tb->Size(gui::Point{ simDropWidth - 20, 16 });
			auto *btn = spi->EmplaceChild<gui::Button>().get();
			btn->Position(gui::Point{ windowSize.x - 31, interorHeight - 1 });
			btn->Size(gui::Point{ 16, 16 });
			tb->Change([this, tb, btn]() {
				float value;
				bool valid = false;
				try
				{
					value = ParseFloatProperty(tb->Text());
					valid = true;
				}
				catch (const std::exception &ex)
				{
				}
				if (!(MIN_TEMP <= value && MAX_TEMP >= value))
				{
					valid = false;
				}
				if (valid)
				{
					game::Game::Ref().AmbientAirTemp(value);
					int c = HeatToColour(value);
					btn->BodyColor(gui::Color{ PixR(c), PixG(c), PixB(c), 255 });
					btn->Text("");
				}
				else
				{
					btn->BodyColor(gui::Color{ 0, 0, 0, 255 });
					btn->Text("?");
				}
			});
			auto reset = [this, tb]() {
				StringBuilder sb;
				sb << Format::Precision(2) << game::Game::Ref().AmbientAirTemp();
				tb->Text(sb.Build());
			};
			tb->Validate(reset);
			reset();
			interorHeight += 20;
		}
		simDrop("Gravity simulation mode", {
			"Vertical",
			"Radial",
			"Off",
		}, int(gm.GravityMode()), [](int value) {
			game::Game::Ref().GravityMode(game::SimGravityMode(value));
		});
		simDrop("Edge mode", {
			"Void",
			"Solid",
			"Loop",
		}, int(gm.EdgeMode()), [](int value) {
			game::Game::Ref().EdgeMode(game::SimEdgeMode(value));
		});
		interorHeight += 8;

		separator();

		{
			std::vector<String> options;
			auto max = gui::SDLWindow::Ref().MaxScale();
			auto current = gui::SDLWindow::Ref().Config().scale;
			for (auto i = 1; i <= max; ++i)
			{
				options.push_back(String::Build(i));
			}
			bool currentValid = current <= max;
			if (!currentValid)
			{
				options.push_back("current");
			}
			auto count = int(options.size());
			otherDrop(0, 40, "Window scale factor for larger screens", options, currentValid ? (current - 1) : (count - 1), [currentValid, current, count](int value) {
				auto conf = gui::SDLWindow::Ref().Config();
				if (!currentValid && value == count - 1)
				{
					conf.scale = current;
				}
				else
				{
					conf.scale = value + 1;
				}
				gui::SDLWindow::Ref().Recreate(conf);
				Client::Ref().SetPref("Scale", conf.scale);
			});
		}
		otherCheck(0, "Resizable", "Allow resizing and maximizing window", gui::SDLWindow::Ref().Config().resizable, [](bool value) {
			auto conf = gui::SDLWindow::Ref().Config();
			conf.resizable = value;
			gui::SDLWindow::Ref().Recreate(conf);
		});
		otherCheck(0, "Fullscreen", "Fill the entire screen", gui::SDLWindow::Ref().Config().fullscreen, [](bool value) {
			auto conf = gui::SDLWindow::Ref().Config();
			conf.fullscreen = value;
			gui::SDLWindow::Ref().Recreate(conf);
		});
		otherCheck(1, "Change resolution", "Set optimal screen resolution", gui::SDLWindow::Ref().Config().borderless, [](bool value) {
			auto conf = gui::SDLWindow::Ref().Config();
			conf.borderless = value;
			gui::SDLWindow::Ref().Recreate(conf);
		});
		otherCheck(1, "Force integer scaling", "Less blurry", gui::SDLWindow::Ref().Config().integer, [](bool value) {
			auto conf = gui::SDLWindow::Ref().Config();
			conf.integer = value;
			gui::SDLWindow::Ref().Recreate(conf);
		});
		otherCheck(0, "Fast quit", "Always exit completely when hitting close", gui::SDLWindow::Ref().FastQuit(), [](bool value) {
			gui::SDLWindow::Ref().FastQuit(value);
			Client::Ref().SetPref("FastQuit", value);
		});
		otherCheck(0, "Show avatars", "Disable if you have a slow connection", Client::Ref().GetPrefBool("ShowAvatars", true), [](bool value) {
			Client::Ref().SetPref("ShowAvatars", value);
		});
		otherCheck(0, "Momentum scrolling", "Accelerating instead of step scroll", gui::SDLWindow::Ref().MomentumScroll(), [](bool value) {
			gui::SDLWindow::Ref().MomentumScroll(value);
			Client::Ref().SetPref("MomentumScroll", value);
		});
		otherCheck(0, "Sticky categories", "Switch between categories by clicking", gm.StickyCategories(), [](bool value) {
			game::Game::Ref().StickyCategories(value);
			Client::Ref().SetPref("MouseClickRequired", value);
		});
		otherCheck(0, "Include pressure", "When saving, copying, stamping, etc.", Client::Ref().GetPrefBool("Simulation.IncludePressure", true), [](bool value) {
			Client::Ref().SetPref("Simulation.IncludePressure", value);
		});
		otherCheck(0, "Perfect circle", "Smoother circle brush", gm.PerfectCircle(), [](bool value) {
			game::Game::Ref().PerfectCircle(value);
			Client::Ref().SetPref("PerfectCircleBrush", value);
		});
		otherDrop(0, 60, "Color space used by decoration tools", {
			"sRGB",
			"Linear",
			"Gamma 2.2",
			"Gamma 1.8",
		}, Client::Ref().GetPrefInteger("Simulation.DecoSpace", 0), [](int value) {
			game::Game::Ref().DecoSpace(game::SimDecoSpace(value));
			Client::Ref().SetPref("Simulation.DecoSpace", value);
		});
		otherButton(0, 90, "Open data folder", "Open the data and preferences folder", []() {
			Platform::OpenDataFolder();
		});
		interorHeight += 4;

		spi->Size(gui::Point{ windowSize.x - 2, interorHeight });
	}
}
}
