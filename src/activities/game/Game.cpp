#include "Game.h"

#include "Config.h"

#include "activities/console/Console.h"
#include "activities/login/Login.h"
#include "activities/options/Options.h"
#include "activities/toolsearch/ToolSearch.h"
#include "simulation/Simulation.h"
#include "simulation/ElementGraphics.h"
#include "simulation/Air.h"
#include "simulation/Gravity.h"
#include "graphics/SimulationRenderer.h"
#include "graphics/Pix.h"
#include "gui/SDLWindow.h"
#include "gui/Button.h"
#include "gui/Icons.h"
#include "common/tpt-compat2.h"
#include "common/Line.h"
#include "common/Platform.h"
#include "Format.h"
#include "ToolPanel.h"
#include "brush/EllipseBrush.h"
#include "brush/RectangleBrush.h"
#include "brush/TriangleBrush.h"
#include "brush/BitmapBrush.h"
#include "brush/CellAlignedBrush.h"
#include "tool/ElementTool.h"
#include "tool/LifeElementTool.h"
#include "tool/WallTool.h"
#include "client/Client.h"

#include "client/SaveInfo.h" // for testing with 2585247
#include "client/GameSave.h" // for testing with 2585247
#include "client/SaveFile.h" // for testing with 2585247

#ifdef LUACONSOLE
# include "lua/LuaScriptInterface.h"
# include "lua/LuaEvents.h"
#else
# include "lua/TPTScriptInterface.h"
#endif

#include <iostream>
#include <algorithm>
#include <iomanip>
#include <array>

namespace activities
{
namespace game
{
	constexpr auto maxLogEntries = 20;
	constexpr auto logTimeoutTicks = 2000;
	constexpr auto logFadeTicks = 1000;
	constexpr auto windowSize = gui::Point{ WINDOWW, WINDOWH };
	constexpr auto simulationSize = gui::Point{ XRES, YRES };
	constexpr auto simulationRect = gui::Rect{ gui::Point{ 0, 0 }, simulationSize };

	static ActionKey actionKeyWithoutMod(ActionSource source, int code)
	{
		return ((uint64_t(source) & 0xFFFFU) << 32) | (uint64_t(code) & 0xFFFFFFFFU);
	}

	static constexpr int actionKeyModShift = 48;
	static constexpr uint64_t freedomMask = (1U << gui::SDLWindow::modBits) - 1U;
	static constexpr uint64_t actionKeyModMask = freedomMask << actionKeyModShift;
	static ActionKey actionKeyWithMod(ActionSource source, int code, int mod)
	{
		return actionKeyWithoutMod(source, code) | ((uint64_t(mod) & freedomMask) << actionKeyModShift);
	}

	static bool actionKeyFromString(String str, ActionKey &key)
	{
		if (auto sourceSplit = str.SplitBy(' '))
		{
			ActionSource source = actionSourceMax;
			if (sourceSplit.Before() == "button")
			{
				source = actionSourceButton;
			}
			else if (sourceSplit.Before() == "wheel")
			{
				source = actionSourceWheel;
			}
			else if (sourceSplit.Before() == "sym")
			{
				source = actionSourceSym;
			}
			else if (sourceSplit.Before() == "scan")
			{
				source = actionSourceScan;
			}
			if (source == actionSourceMax)
			{
				return false;
			}
			str = sourceSplit.After();
			int mod = 0;
			std::array<String, gui::SDLWindow::modBits> modNames = { { "mod0", "mod1", "mod2", "mod3", "mod4", "mod5", "mod6", "mod7" } };
			for (auto i = 0; i < int(modNames.size()); ++i)
			{
				if (auto shiftSplit = str.SplitBy(' '))
				{
					if (shiftSplit.Before() == modNames[i])
					{
						mod |= 1 << i;
						str = shiftSplit.After();
					}
				}
			}
			int code;
			try
			{
				code = str.ToNumber<int>();
			}
			catch (...)
			{
				return false;
			}
			key = actionKeyWithMod(source, code, mod);
			return true;
		}
		return false;
	}

	class MenuSectionButton : public gui::Button
	{
		int id;

	public:
		MenuSectionButton(int id) : id(id)
		{
		}

		void Tick() final override
		{
			Stuck(Game::Ref().CurrentMenuSection() == id);
		}

		void MouseEnter(gui::Point current) final override
		{
			if (id != SC_DECO && !Game::Ref().StickyCategories())
			{
				TriggerClick();
			}
		}
	};

	std::array<MenuSection, SC_TOTAL> menuSections = { {
		{ gui::Icons::wall       , "Walls"             },
		{ gui::Icons::electronic , "Electronics"       },
		{ gui::Icons::powered    , "Powered materials" },
		{ gui::Icons::sensor     , "Sensors"           },
		{ gui::Icons::force      , "Force"             },
		{ gui::Icons::explosive  , "Explosives"        },
		{ gui::Icons::gas        , "Gases"             },
		{ gui::Icons::liquid     , "Liquids"           },
		{ gui::Icons::powder     , "Powders"           },
		{ gui::Icons::solid      , "Solids"            },
		{ gui::Icons::radioactive, "Radioactive"       },
		{ gui::Icons::special    , "Special"           },
		{ gui::Icons::gol        , "Game of Life"      },
		{ gui::Icons::tool       , "Tools"             },
		{ gui::Icons::favorite   , "Favorites"         },
		{ gui::Icons::paint      , "Decoration tools"  },
	} };

	struct RendererPreset
	{
		String name;
		int renderMode;
		int displayMode;
		int colorMode;
	};
	std::vector<RendererPreset> rendererPresets = {
		{ "Alternative Velocity Display", int(RENDER_EFFE | RENDER_BASC), int(DISPLAY_AIRC),                0 },
		{             "Velocity Display", int(RENDER_EFFE | RENDER_BASC), int(DISPLAY_AIRV),                0 },
		{             "Pressure Display", int(RENDER_EFFE | RENDER_BASC), int(DISPLAY_AIRP),                0 },
		{           "Persistent Display", int(RENDER_EFFE | RENDER_BASC), int(DISPLAY_PERS),                0 },
		{                 "Fire Display", int(RENDER_EFFE | RENDER_BASC |
                                              RENDER_FIRE | RENDER_SPRK),                 0,                0 },
		{                 "Blob Display", int(RENDER_EFFE | RENDER_BLOB |
                                              RENDER_FIRE | RENDER_SPRK),                 0,                0 },
		{                 "Heat Display", int(              RENDER_BASC), int(DISPLAY_AIRH), int(COLOUR_HEAT) },
		{                "Fancy Display", int(RENDER_EFFE | RENDER_BASC |
                                              RENDER_FIRE | RENDER_SPRK |
                                              RENDER_BLUR | RENDER_GLOW), int(DISPLAY_WARP),                0 },
		{              "Nothing Display", int(              RENDER_BASC),                 0,                0 },
		{        "Heat Gradient Display", int(              RENDER_BASC),                 0, int(COLOUR_GRAD) },
		{        "Life Gradient Display", int(              RENDER_BASC),                 0, int(COLOUR_LIFE) },
	};

	Game::Game()
	{
		simulation = std::make_unique<Simulation>();
		simulationRenderer = std::make_unique<SimulationRenderer>();
		InitActions();
		InitWindow();
		InitTools();
		InitBrushes();
		CurrentTool(0, tools["DEFAULT_PT_DUST"].get()); // * TODO-REDO_UI: do this right
		CurrentTool(1, tools["DEFAULT_PT_NONE"].get()); // * TODO-REDO_UI: do this right
		StickyCategories(Client::Ref().GetPrefBool("MouseClickRequired", false));
		PerfectCircle(Client::Ref().GetPrefBool("PerfectCircleBrush", true));
		for (auto &favorite : Client::Ref().GetPrefByteStringArray("Favorites"))
		{
			Favorite(favorite.FromUtf8(), true);
		}
	}

	Game::~Game()
	{
	}

	void Game::InitWindow()
	{
		Size(windowSize);

		auto addButtonHorizAny = [this](bool right, String text, int width, bool alignLeft, int y, gui::Button::ClickCallback cb, std::shared_ptr<gui::Button> button, int &nextXL, int &nextXR) {
			if (right)
			{
				nextXR -= width + 1;
			}
			button->Size(gui::Point{ width, 15 });
			button->Position(gui::Point{ right ? nextXR : nextXL, y });
			button->Text(text);
			button->Click(cb);
			if (alignLeft)
			{
				button->Align(gui::Alignment::horizLeft | gui::Alignment::vertCenter);
			}
			if (!right)
			{
				nextXL += width + 1;
			}
			return button;
		};

		auto abhlNextX = 1;
		auto abhrNextX = windowSize.x;
		auto addButtonHoriz = [this, addButtonHorizAny, &abhlNextX, &abhrNextX](bool right, String text, int width, bool alignLeft, gui::Button::ClickCallback cb) {
			return addButtonHorizAny(right, text, width, alignLeft, windowSize.y - 16, cb, EmplaceChild<gui::Button>(), abhlNextX, abhrNextX);
		};

		auto addToolTipHoriz = [](gui::Button *button, String toolTip) {
			button->ToolTip(gui::ToolTipInfo{ toolTip, gui::Point{ 12, simulationSize.y - 12 } });
		};

		auto addToolTipVert = [](gui::Button *button, String toolTip) {
			auto pos = gui::Point{ simulationSize.x - gui::SDLWindow::Ref().TextSize(toolTip).x - 9, button->AbsolutePosition().y + 2 };
			if (pos.y > simulationSize.y - 12)
			{
				pos.y = simulationSize.y - 12;
			}
			button->ToolTip(gui::ToolTipInfo{ toolTip, pos });
		};

		pauseButton = addButtonHoriz(true, gui::IconString(gui::Icons::pause, gui::Point{ -1, -2 }), 15, false, [this]() {
			TogglePaused();
		}).get();
		addToolTipHoriz(pauseButton, "Pause/resume simulation");
		rendererButton = addButtonHoriz(true,
			gui::BlendAddString() + gui::ColorString(gui::Color{ 0xFF, 0x00, 0x00 }) + gui::IconString(gui::Icons::gradient1, gui::Point{ 0, -1 }) +
			gui::StepBackString() + gui::ColorString(gui::Color{ 0x00, 0xFF, 0x00 }) + gui::IconString(gui::Icons::gradient2, gui::Point{ 0, -1 }) +
			gui::StepBackString() + gui::ColorString(gui::Color{ 0x00, 0x00, 0xFF }) + gui::IconString(gui::Icons::gradient3, gui::Point{ 0, -1 }),
			15, false, [this]() {
				rendererButton->Stuck(!rendererButton->Stuck());
				UpdateGroups();
			}
		).get(); // * TODO-REDO_UI: reapply add blend mode somehow
		addToolTipHoriz(rendererButton, "Renderer options");

		auto abhlNextXSave = abhlNextX;
		auto abhrNextXSave = abhrNextX;
		groupSave = EmplaceChild<gui::Component>().get();
		groupSave->Position(gui::Point{ 0, windowSize.y - 16 });
		groupSave->Size(gui::Point{ abhrNextX, 16 });
		auto addButtonHorizSave = [this, addButtonHorizAny, &abhlNextXSave, &abhrNextXSave](bool right, String text, int width, bool alignLeft, gui::Button::ClickCallback cb) {
			return addButtonHorizAny(right, text, width, alignLeft, 0, cb, groupSave->EmplaceChild<gui::Button>(), abhlNextXSave, abhrNextXSave);
		};

		addButtonHorizSave(false, gui::IconString(gui::Icons::open, gui::Point{ 0, -2 }), 17, false, []() {
		// * TODO-REDO_UI
		});
		addButtonHorizSave(false, gui::IconString(gui::Icons::reload, gui::Point{ 0, -2 }), 17, false, []() {
		// * TODO-REDO_UI
		})->Enabled(false);
		addButtonHorizSave(false, gui::IconString(gui::Icons::save, gui::Point{ 2, -2 }) + gui::OffsetString(8) + gui::CommonColorString(gui::commonLightGray) + String("[untitled simulation]"), 150, true, []() {
		// * TODO-REDO_UI
		});
		addButtonHorizSave(false, gui::ColorString(gui::Color{ 0x00, 0xBF, 0x00 }) + gui::IconString(gui::Icons::voteUp, gui::Point{ 0, 0 }) + gui::OffsetString(3) + String("Vote"), 39, false, []() {
		// * TODO-REDO_UI
		})->Enabled(false);
		abhlNextX -= 2;
		addButtonHorizSave(false, gui::ColorString(gui::Color{ 0xBF, 0x00, 0x00 }) + gui::IconString(gui::Icons::voteDown, gui::Point{ 0, -3 }), 15, false, []() {
		// * TODO-REDO_UI
		})->Enabled(false);

		auto *optionsButton = addButtonHorizSave(true, gui::IconString(gui::Icons::settings, gui::Point{ 0, -1 }), 15, false, []() {
			gui::SDLWindow::Ref().EmplaceBack<options::Options>();
		}).get();
		addToolTipHoriz(optionsButton, "Simulation options");

		profileButton = groupSave->EmplaceChild<gui::Button>().get();
		profileButton->Align(gui::Alignment::horizLeft | gui::Alignment::vertCenter);
		loginButton = addButtonHorizSave(true, "", 92, true, []() {
			gui::SDLWindow::Ref().EmplaceBack<login::Login>();
		}).get();
		addToolTipHoriz(loginButton, "Sign into simulation server");
		{
			auto diff = gui::Point{ 19, 0 };
			profileButton->Position(loginButton->Position() + diff);
			profileButton->Size(loginButton->Size() - diff);
		}

		auto *clearButton = addButtonHorizSave(true, gui::IconString(gui::Icons::empty, gui::Point{ -1, -2 }), 17, false, [this]() {
			simulation->clear_sim();
		}).get();
		addToolTipHoriz(clearButton, "Clear simulation");

		addButtonHorizSave(false, gui::IconString(gui::Icons::tag, gui::Point{ 2, -2 }) + gui::OffsetString(7) + String("[no tags set]"), abhrNextXSave - abhlNextXSave - 1, true, []() {
		// * TODO-REDO_UI
		})->Enabled(false);

		auto abvNextY = windowSize.y - 32;
		auto addButtonVert = [this, &abvNextY](String text, gui::Button::ClickCallback cb) {
			auto button = EmplaceChild<gui::Button>();
			button->Size(gui::Point{ 15, 15 });
			button->Position(gui::Point{ windowSize.x - 16, abvNextY });
			button->Text(text);
			button->Click(cb);
			abvNextY -= 16;
			return button;
		};

		currentMenuSection = SC_POWDERS;
		auto *searchButton = addButtonVert(gui::IconString(gui::Icons::search, gui::Point{ 0, -2 }), [this]() {
			OpenToolSearch();
		}).get();
		addToolTipVert(searchButton, "Element search");
		for (auto i = 0; i < SC_TOTAL; ++i)
		{
			auto &ms = menuSections[i];
			auto button = EmplaceChild<MenuSectionButton>(i);
			button->Size(gui::Point{ 15, 15 });
			button->Position(gui::Point{ windowSize.x - 16, abvNextY - (SC_TOTAL - i - 1) * 16 });
			button->Text(gui::IconString(ms.icon, gui::Point{ 0, -2 }));
			button->Click([this, i]() {
				currentMenuSection = i;
			});
			addToolTipVert(button.get(), ms.name);
		}

		auto aboNextY = 1;
		auto addButtonOption = [this, &aboNextY, addToolTipVert](String text, String toolTip, gui::Button::ClickCallback cb) {
			auto button = EmplaceChild<gui::Button>();
			button->Size(gui::Point{ 15, 15 });
			button->Position(gui::Point{ windowSize.x - 16, aboNextY });
			button->Text(text);
			button->Click(cb);
			addToolTipVert(button.get(), toolTip);
			aboNextY += 16;
			return button;
		};

		prettyPowdersButton = addButtonOption("P", "Pretty powders", [this]() {
			TogglePrettyPowders();
		}).get();
		drawGravityButton = addButtonOption("G", "Draw gravity field", [this]() {
			ToggleDrawGravity();
		}).get();
		drawDecoButton = addButtonOption("D", "Draw decorations", [this]() {
			ToggleDrawDeco();
		}).get();
		newtonianGravityButton = addButtonOption("N", "Newtonian gravity", [this]() {
			ToggleNewtonianGravity();
		}).get();
		ambientHeatButton = addButtonOption("A", "Ambient heat simulation", [this]() {
			ToggleAmbientHeat();
		}).get();
		addButtonOption("C", "Open console", [this]() {
			OpenConsole();
		});

		auto abhlNextXRender = abhlNextX;
		auto abhrNextXRender = abhrNextX;
		groupRender = EmplaceChild<gui::Component>().get();
		groupRender->Position(gui::Point{ 0, windowSize.y - 16 });
		groupRender->Size(gui::Point{ abhrNextX, 16 });
		auto addButtonHorizRender = [this, addButtonHorizAny, &abhlNextXRender, &abhrNextXRender](bool right, String text, int width, bool alignLeft, gui::Button::ClickCallback cb) {
			return addButtonHorizAny(right, text, width, alignLeft, 0, cb, groupRender->EmplaceChild<gui::Button>(), abhlNextXRender, abhrNextXRender);
		};

		auto addRenderModeCheckbox = [this, addButtonHorizRender, addToolTipHoriz](int flag, String text, String toolTip) {
			auto *button = addButtonHorizRender(false, text, 17, false, nullptr).get();
			button->Click([this, button]() {
				button->Stuck(!button->Stuck());
				ApplyRendererSettings();
			});
			button->ActiveText(gui::Button::activeTextDarkened);
			addToolTipHoriz(button, toolTip);
			renderModeButtons.push_back(std::make_tuple(flag, button));
		};

		abhlNextXRender += 2;

		addRenderModeCheckbox(RENDER_EFFE, gui::ColorString(gui::Color({ 0xFF, 0xA0, 0xFF })) + gui::IconString(gui::Icons::effect, gui::Point{  0, -1 }), "Adds Special flare effects to some elements");
		addRenderModeCheckbox(RENDER_FIRE, gui::ColorString(gui::Color({ 0xFF, 0x00, 0x00 })) + gui::IconString(gui::Icons::firebg, gui::Point{  0, -1 }) + gui::StepBackString() +
		                                   gui::ColorString(gui::Color({ 0xFF, 0xFF, 0x40 })) + gui::IconString(gui::Icons::fire  , gui::Point{  0, -1 }), "Fire effect for gasses");
		addRenderModeCheckbox(RENDER_GLOW, gui::ColorString(gui::Color({ 0xC8, 0xFF, 0xFF })) + gui::IconString(gui::Icons::glow  , gui::Point{  0, -1 }), "Glow effect on some elements");
		addRenderModeCheckbox(RENDER_BLUR, gui::ColorString(gui::Color({ 0x64, 0x96, 0xFF })) + gui::IconString(gui::Icons::liquid, gui::Point{ -1, -2 }), "Blur effect for liquids");
		addRenderModeCheckbox(RENDER_BLOB, gui::ColorString(gui::Color({ 0x37, 0xFF, 0x37 })) + gui::IconString(gui::Icons::blob  , gui::Point{ -1, -2 }), "Makes everything be drawn like a blob");
		addRenderModeCheckbox(RENDER_BASC, gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xC8 })) + gui::IconString(gui::Icons::basic , gui::Point{  0, -1 }), "Basic rendering, without this, most things will be invisible");
		addRenderModeCheckbox(RENDER_SPRK, gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xA0 })) + gui::IconString(gui::Icons::effect, gui::Point{  0, -1 }), "Glow effect on sparks");
		abhlNextXRender += 10;

		auto addDisplayModeCheckbox = [this, addButtonHorizRender, addToolTipHoriz](int flag, String text, String toolTip) {
			auto *button = addButtonHorizRender(false, text, 17, false, nullptr).get();
			button->Click([this, button]() {
				button->Stuck(!button->Stuck());
				ApplyRendererSettings();
			});
			button->ActiveText(gui::Button::activeTextDarkened);
			addToolTipHoriz(button, toolTip);
			displayModeButtons.push_back(std::make_tuple(flag, button));
		};

		addDisplayModeCheckbox(DISPLAY_AIRC, gui::ColorString(gui::Color({ 0xFF, 0x37, 0x37 })) + gui::IconString(gui::Icons::alterairbg, gui::Point{  0, -1 }) + gui::StepBackString() +
		                                     gui::ColorString(gui::Color({ 0x37, 0xFF, 0x37 })) + gui::IconString(gui::Icons::alterair  , gui::Point{  0, -1 }), "Displays pressure as red and blue, and velocity as white");
		addDisplayModeCheckbox(DISPLAY_AIRP, gui::ColorString(gui::Color({ 0xFF, 0xD4, 0x20 })) + gui::IconString(gui::Icons::sensor    , gui::Point{  0, -1 }), "Displays pressure, red is positive and blue is negative");
		addDisplayModeCheckbox(DISPLAY_AIRV, gui::ColorString(gui::Color({ 0x80, 0xA0, 0xFF })) + gui::IconString(gui::Icons::velocity  , gui::Point{  0, -1 }), "Displays velocity and positive pressure: up/down adds blue, right/left adds red, still pressure adds green");
		addDisplayModeCheckbox(DISPLAY_AIRH, gui::ColorString(gui::Color({ 0x9F, 0x00, 0xF0 })) + gui::IconString(gui::Icons::heatbg    , gui::Point{  0, -2 }) + gui::StepBackString() +
		                                     gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xFF })) + gui::IconString(gui::Icons::heat      , gui::Point{  0, -2 }), "Displays the temperature of the air like heat display does");
		addDisplayModeCheckbox(DISPLAY_WARP, gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xFF })) + gui::IconString(gui::Icons::warp      , gui::Point{ -1, -2 }), "Gravity lensing, Newtonian Gravity bends light with this on");
		addDisplayModeCheckbox(DISPLAY_EFFE, gui::ColorString(gui::Color({ 0xA0, 0xFF, 0xFF })) + gui::IconString(gui::Icons::effect    , gui::Point{  0, -1 }), "Enables moving solids, stickmen guns, and premium(tm) graphics");
		addDisplayModeCheckbox(DISPLAY_PERS, gui::ColorString(gui::Color({ 0xD4, 0xD4, 0xD4 })) + gui::IconString(gui::Icons::persistent, gui::Point{  0, -1 }), "Element paths persist on the screen for a while");
		abhlNextXRender += 10;

		auto addColourModeCheckbox = [this, addButtonHorizRender, addToolTipHoriz](int flag, String text, String toolTip) {
			auto *button = addButtonHorizRender(false, text, 17, false, nullptr).get();
			button->Click([this, button]() {
				button->Stuck(!button->Stuck());
				ApplyRendererSettings();
			});
			button->ActiveText(gui::Button::activeTextDarkened);
			addToolTipHoriz(button, toolTip);
			colorModeButtons.push_back(std::make_tuple(flag, button));
		};

		addColourModeCheckbox(COLOUR_HEAT, gui::ColorString(gui::Color({ 0xFF, 0x00, 0x00 })) + gui::IconString(gui::Icons::heatbg  ,  gui::Point{ 0, -2 }) + gui::StepBackString() +
		                                   gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xFF })) + gui::IconString(gui::Icons::heat    ,  gui::Point{ 0, -2 }), "Displays temperatures of the elements, dark blue is coldest, pink is hottest");
		addColourModeCheckbox(COLOUR_LIFE, gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xFF })) + gui::IconString(gui::Icons::life    ,  gui::Point{ 0, -1 }), "Displays the life value of elements in greyscale gradients");
		addColourModeCheckbox(COLOUR_GRAD, gui::ColorString(gui::Color({ 0xCD, 0x32, 0xCD })) + gui::IconString(gui::Icons::gradient,  gui::Point{ 0, -1 }), "Changes colors of elements slightly to show heat diffusing through them");
		addColourModeCheckbox(COLOUR_BASC, gui::ColorString(gui::Color({ 0xC0, 0xC0, 0xC0 })) + gui::IconString(gui::Icons::basic   ,  gui::Point{ 0, -1 }), "No special effects at all for anything, overrides all other options and deco");

		auto addPresetButton = [this, addButtonHorizRender, addToolTipHoriz](int preset, String text, String toolTip) {
			auto *button = addButtonHorizRender(true, text, 17, false, [this, preset]() {
				RendererPreset(preset);
			}).get();
			button->ActiveText(gui::Button::activeTextDarkened);
			addToolTipHoriz(button, toolTip);
			rendererPresetButtons.push_back(std::make_tuple(preset, button));
		};

		addPresetButton(10, gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xFF })) + gui::IconString(gui::Icons::life      , gui::Point{  0, -1 }), "Life display mode preset");
		addPresetButton( 0, gui::ColorString(gui::Color({ 0xFF, 0x37, 0x37 })) + gui::IconString(gui::Icons::alterairbg, gui::Point{  0, -1 }) + gui::StepBackString() +
		                    gui::ColorString(gui::Color({ 0x37, 0xFF, 0x37 })) + gui::IconString(gui::Icons::alterair  , gui::Point{  0, -1 }), "Alternative Velocity display mode preset");
		addPresetButton( 9, gui::ColorString(gui::Color({ 0xCD, 0x32, 0xCD })) + gui::IconString(gui::Icons::gradient  , gui::Point{  0, -1 }), "Heat gradient display mode preset");
		addPresetButton( 8, gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xC8 })) + gui::IconString(gui::Icons::basic     , gui::Point{  0, -1 }), "Nothing display mode preset");
		addPresetButton( 7, gui::ColorString(gui::Color({ 0x64, 0x96, 0xFF })) + gui::IconString(gui::Icons::liquid    , gui::Point{ -1, -2 }), "Fancy display mode preset");
		addPresetButton( 6, gui::ColorString(gui::Color({ 0xFF, 0x00, 0x00 })) + gui::IconString(gui::Icons::heatbg    , gui::Point{  0, -2 }) + gui::StepBackString() +
		                    gui::ColorString(gui::Color({ 0xFF, 0xFF, 0xFF })) + gui::IconString(gui::Icons::heat      , gui::Point{  0, -2 }), "Heat display mode preset");
		addPresetButton( 5, gui::ColorString(gui::Color({ 0x37, 0xFF, 0x37 })) + gui::IconString(gui::Icons::blob      , gui::Point{ -1, -2 }), "Blob display mode preset");
		addPresetButton( 4, gui::ColorString(gui::Color({ 0xFF, 0x00, 0x00 })) + gui::IconString(gui::Icons::firebg    , gui::Point{  0, -1 }) + gui::StepBackString() +
		                    gui::ColorString(gui::Color({ 0xFF, 0xFF, 0x40 })) + gui::IconString(gui::Icons::fire      , gui::Point{  0, -1 }), "Fire display mode preset");
		addPresetButton( 3, gui::ColorString(gui::Color({ 0xD4, 0xD4, 0xD4 })) + gui::IconString(gui::Icons::persistent, gui::Point{  0, -1 }), "Persistent display mode preset");
		addPresetButton( 2, gui::ColorString(gui::Color({ 0xFF, 0xD4, 0x20 })) + gui::IconString(gui::Icons::sensor    , gui::Point{  0, -1 }), "Pressure display mode preset");
		addPresetButton( 1, gui::ColorString(gui::Color({ 0x80, 0xA0, 0xFF })) + gui::IconString(gui::Icons::velocity  , gui::Point{  0, -1 }), "Velocity display mode preset");

		UpdateGroups();
		UpdateLoginButton();
		RendererPreset(4);
		// * TODO-REDO_UI: load prefs
	}

	void Game::InitTools()
	{
		tools.clear();
		for (auto i = 0; i < int(simulation->elements.size()); ++i)
		{
			auto &el = simulation->elements[i];
			if (el.Enabled)
			{
				tools.insert(std::make_pair(el.Identifier.FromUtf8(), std::make_unique<tool::ElementTool>(i)));
			}
		}
		for (auto i = 0; i < int(UI_WALLCOUNT); ++i)
		{
			auto &wl = simulation->wtypes[i];
			tools.insert(std::make_pair(wl.identifier.FromUtf8(), std::make_unique<tool::WallTool>(i)));
		}
		for (auto i = 0; i < int(NGOL); ++i)
		{
			auto &gl = builtinGol[i];
			tools.insert(std::make_pair(String("DEFAULT_PT_LIFE_") + gl.name, std::make_unique<tool::LifeElementTool>(
				i,
				gl.name,
				gl.description,
				gui::Color{
					int(PIXR(gl.colour)),
					int(PIXG(gl.colour)),
					int(PIXB(gl.colour)),
					255
				}
			)));
		}
		// * TODO-REDO_UI: add custom gol
	}

	void Game::InitBrushes()
	{
		brushes.resize(BRUSH_NUM);
		brushes[CIRCLE_BRUSH] = std::make_unique<brush::EllipseBrush>();
		brushes[SQUARE_BRUSH] = std::make_unique<brush::RectangleBrush>();
		brushes[TRI_BRUSH] = std::make_unique<brush::TriangleBrush>();
		for (auto &bf : Platform::DirectorySearch(BRUSH_DIR, "", { ".ptb" }))
		{
			auto data = Client::Ref().ReadFile(ByteString(BRUSH_DIR "/") + bf);
			if (!data.size())
			{
				std::cerr << "Skipping brush " << bf << ": failed to open" << std::endl;
				continue;
			}
			auto size = int(sqrtf(float(data.size())));
			if (size * size != data.size())
			{
				std::cerr << "Skipping brush " << bf << ": invalid size" << std::endl;
				continue;
			}
			brushes.push_back(std::make_unique<brush::BitmapBrush>(data, gui::Point{ size, size }));
		}
		cellAlignedBrush = std::make_unique<brush::CellAlignedBrush>();
	}

	void Game::MaybeBuildToolPanel()
	{
		if (shouldBuildToolPanel)
		{
			shouldBuildToolPanel = false;
			RemoveChild(toolPanel);
			toolPanel = EmplaceChild<ToolPanel>().get();
		}
	}

	void Game::ToolDrawWithBrush(gui::Point from, gui::Point to)
	{
		from = simulationRect.Clamp(from);
		to = simulationRect.Clamp(to);
		auto *tool = currentTools[activeToolIndex];
		auto *brush = tool->CellAligned() ? cellAlignedBrush.get() : brushes[currentBrush].get();
		if (lastBrush->CellAligned())
		{
			from = gui::Point{ from.x / CELL * CELL + CELL / 2, from.y / CELL * CELL + CELL / 2 };
			to   = gui::Point{   to.x / CELL * CELL + CELL / 2,   to.y / CELL * CELL + CELL / 2 };
		}
		brush->Draw(tool, from, to);
	}

	void Game::Tick()
	{
		ModalWindow::Tick();
		auto pos = gui::SDLWindow::Ref().MousePosition();

		switch (activeToolMode)
		{
		case toolModeFree:
			ToolDrawWithBrush(toolStartPos, pos);
			toolStartPos = pos;
			break;

		case toolModeFill:
			currentTools[activeToolIndex]->Fill(pos);
			break;

		default:
			break;
		}

		MaybeBuildToolPanel();

		simulation->BeforeSim();
		if (!simulation->sys_pause || simulation->framerender)
		{
			simulation->UpdateParticles(0, NPART);
			simulation->AfterSim();
		}

		simulationSample = simulation->GetSample(pos.x, pos.y);
	}

	void Game::Draw() const
	{
		auto &g = gui::SDLWindow::Ref();
		auto *r = Renderer();

		if (simulation)
		{
			simulationRenderer->Render(*simulation);
			auto *data = simulationRenderer->MainBuffer();
			void *pixels;
			int pitch;
			SDL_LockTexture(simulationTexture, NULL, &pixels, &pitch);
			if (pitch == int(simulationSize.x * sizeof(uint32_t)))
			{
				std::copy(data, data + simulationSize.y * simulationSize.x, reinterpret_cast<uint32_t *>(pixels));
			}
			else // * Very unlikely to be needed but present for the sake of completeness.
			{
				for (auto y = 0; y < simulationSize.y; ++y)
				{
					std::copy(data + y * simulationSize.x, data + (y + 1) * simulationSize.x, reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(pixels) + y * pitch));
				}
			}
			if (drawHUD && (activeToolMode != toolModeNone || (g.UnderMouse() && simulationRect.Contains(g.MousePosition()))))
			{
				DrawBrush(pixels, pitch);
			}
			SDL_UnlockTexture(simulationTexture);
			SDL_Rect rect;
			rect.x = 0;
			rect.y = 0;
			rect.w = simulationSize.x;
			rect.h = simulationSize.y;
			SDL_RenderCopy(r, simulationTexture, NULL, &rect);
		}

		if (drawHUD)
		{
			DrawStatistics();
			DrawSampleInfo();
			DrawLogEntries();
		}

		ModalWindow::Draw();
	}
	
	bool Game::MouseMove(gui::Point current, gui::Point delta)
	{
		auto pos = gui::SDLWindow::Ref().MousePosition();
		switch (activeToolMode)
		{
		case toolModeFree:
			ToolDrawWithBrush(toolStartPos, pos);
			toolStartPos = pos;
			break;

		case toolModeFill:
			currentTools[activeToolIndex]->Fill(pos);
			break;

		default:
			break;
		}
		return false;
	}
	
	bool Game::MouseDown(gui::Point current, int button)
	{
		auto &g = gui::SDLWindow::Ref();
		return HandlePress(actionSourceButton, button, g.Mod(), 1);
	}
	
	bool Game::MouseUp(gui::Point current, int button)
	{
		return HandleRelease(actionSourceButton, button);
	}

	bool Game::MouseWheel(gui::Point current, int distance, int direction)
	{
		auto &g = gui::SDLWindow::Ref();
		if (distance < 0)
		{
			distance = -distance;
			direction = -direction;
		}
		auto handled = HandlePress(actionSourceWheel, direction, g.Mod(), distance);
		if (handled)
		{
			HandleRelease(actionSourceWheel, direction);
		}
		return handled;
	}
	
	bool Game::KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt)
	{
		auto &g = gui::SDLWindow::Ref();
		if (HandlePress(actionSourceScan, scan, g.Mod(), 1))
		{
			return true;
		}
		if (HandlePress(actionSourceSym, sym, g.Mod(), 1))
		{
			return true;
		}
		return false;
	}
	
	bool Game::KeyRelease(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt)
	{
		if (HandleRelease(actionSourceScan, scan))
		{
			return true;
		}
		if (HandleRelease(actionSourceSym, sym))
		{
			return true;
		}
		return false;
	}

	void Game::DrawLogEntries() const
	{
		auto &g = gui::SDLWindow::Ref();
		auto start = gui::Point{ 20, YRES - 34 };
		auto now = gui::SDLWindow::Ticks();
		for (auto &entry : log)
		{
			int alpha = 255;
			auto starsFadingAt = entry.pushedAt + logTimeoutTicks;
			if (now >= starsFadingAt)
			{
				if (now >= starsFadingAt + logFadeTicks)
				{
					break;
				}
				alpha -= 255 * (now - starsFadingAt) / logFadeTicks;
			}
			g.DrawRect(gui::Rect{ start - gui::Point{ 3, 3 }, gui::Point{ g.TextSize(entry.message).x + 6, 14 } }, gui::Color{ 0, 0, 0, 100 });
			g.DrawText(start, entry.message, gui::Color{ 255, 255, 255, alpha });
			start.y -= 14;
		}
	}

	void Game::DrawStatistics() const
	{
		auto &g = gui::SDLWindow::Ref();
		StringBuilder sb;
		sb << "FPS: " << Format::Precision(2) << g.GetFPS();
		if (debugHUD)
		{
			sb << " Parts: " << simulationSample.NumParts;
		}
		auto stats = sb.Build();
		g.DrawRect(gui::Rect{ gui::Point{ 12, 12 }, gui::Point{ g.TextSize(stats).x + 8, 15 } }, gui::Color{ 0, 0, 0, 128 });
		g.DrawText(gui::Point{ 16, 14 }, stats, gui::Color{ 32, 216, 255, 192 });
	}

	void Game::DrawSampleInfo() const
	{
	}

	void Game::DrawBrush(void *pixels, int pitch) const
	{
		auto brushPixel = [this, pixels, pitch](int x, int y) {
			if (simulationRect.Contains(gui::Point{ x, y }))
			{
				auto &pix = *reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(pixels) + y * pitch + x * sizeof(uint32_t));
				pix = (PixB(pix) + 3 * PixG(pix) + 2 * PixR(pix) < 512) ? 0xC0C0C0U : 0x404040U;
			}
		};

		auto &g = gui::SDLWindow::Ref();
		auto pos = simulationRect.Clamp(g.MousePosition());

		// * TODO-REDO_UI: Do this with a texture instead or something.
		if ((g.Mod() & gui::SDLWindow::mod0) && (g.Mod() & gui::SDLWindow::mod1))
		{
			brushPixel(pos.x, pos.y);
			for (auto i = 1; i <= 5; ++i)
			{
				brushPixel(pos.x, pos.y + i);
				brushPixel(pos.x, pos.y - i);
				brushPixel(pos.x + i, pos.y);
				brushPixel(pos.x - i, pos.y);
			}
		}
		else
		{
			auto pmid = pos;
			if (lastBrush->CellAligned())
			{
				pmid = gui::Point{ pos.x / CELL * CELL + CELL / 2, pos.y / CELL * CELL + CELL / 2 };
			}
			auto rbru = gui::Rect{ pmid - lastBrush->EffectiveRadius(), lastBrush->EffectiveSize() };
			auto rcom = rbru.Intersect(simulationRect);
			auto tl = rcom.pos;
			auto br = rcom.pos + rcom.size;
			for (auto y = tl.y; y < br.y; ++y)
			{
				for (auto x = tl.x; x < br.x; ++x)
				{
					if (lastBrush->Outline(gui::Point{ x, y } - pmid))
					{
						brushPixel(x, y);
					}
				}
			}
		}

		switch (activeToolMode)
		{
		case toolModeLine:
			BrushLine(toolStartPos.x, toolStartPos.y, pos.x, pos.y, brushPixel);
			break;

		case toolModeRect:
			{
				auto rect = gui::Rect::FromCorners(toolStartPos, pos);
				if (rect.size.x == 1)
				{
					for (auto y = rect.pos.y; y < rect.pos.y + rect.size.y; ++y)
					{
						brushPixel(rect.pos.x, y);
					}
				}
				else if (rect.size.y == 1)
				{
					for (auto x = rect.pos.x; x < rect.pos.x + rect.size.x; ++x)
					{
						brushPixel(x, rect.pos.y);
					}
				}
				else
				{
					for (auto x = rect.pos.x; x < rect.pos.x + rect.size.x; ++x)
					{
						brushPixel(x, rect.pos.y);
						brushPixel(x, rect.pos.y + rect.size.y - 1);
					}
					for (auto y = rect.pos.y + 1; y < rect.pos.y + rect.size.y - 1; ++y)
					{
						brushPixel(rect.pos.x, y);
						brushPixel(rect.pos.x + rect.size.x - 1, y);
					}
				}
			}
			break;

		default:
			break;
		}
	}

	void Game::Test()
	{
		BrushRadius(gui::Point{ 4, 4 });
		lastBrush = brushes[0].get();

		std::vector<unsigned char> gsRaw = Client::Ref().GetSaveData(2585247, 0); // * rendering
		if (!gsRaw.size())
		{
			throw std::runtime_error(("Could not load save\n" + Client::Ref().GetLastError()).ToUtf8());
		}
		auto gs = std::make_unique<GameSave>(gsRaw);
		simulation->gravityMode = gs->gravityMode;
		simulation->air->airMode = gs->airMode;
		simulation->edgeMode = gs->edgeMode;
		simulation->legacy_enable = gs->legacyEnable;
		simulation->water_equal_test = gs->waterEEnabled;
		simulation->aheat_enable = gs->aheatEnable;
		if (gs->gravityEnable)
		{
			simulation->grav->start_grav_async();
		}
		else
		{
			simulation->grav->stop_grav_async();
		}
		simulation->clear_sim();
		simulation->Load(gs.get(), true);
		
		RendererPreset(RendererPreset());
#ifdef LUACONSOLE
		commandInterface = std::make_unique<LuaScriptInterface>(this);
		static_cast<LuaScriptInterface *>(commandInterface.get())->Init();
#else
		commandInterface = std::make_unique<TPTScriptInterface>(this);
#endif
	}
	
	void Game::RendererUp()
	{
		simulationTexture = SDL_CreateTexture(Renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, simulationSize.x, simulationSize.y);
		if (!simulationTexture)
		{
			throw std::runtime_error(ByteString("SDL_CreateTexture failed\n") + SDL_GetError());
		}
	}
	
	void Game::RendererDown()
	{
		SDL_DestroyTexture(simulationTexture);
		simulationTexture = NULL;
	}

	void Game::PrettyPowders(bool newPrettyPowders)
	{
		prettyPowders = newPrettyPowders;
		prettyPowdersButton->Stuck(prettyPowders);
		simulation->pretty_powder = prettyPowders ? 1 : 0;
	}

	void Game::DrawGravity(bool newDrawGravity)
	{
		drawGravity = newDrawGravity;
		drawGravityButton->Stuck(drawGravity);
		simulationRenderer->GravityFieldEnabled(drawGravity);
	}

	void Game::DrawDeco(bool newDrawDeco)
	{
		drawDeco = newDrawDeco;
		drawDecoButton->Stuck(drawDeco);
		simulationRenderer->DecorationsEnabled(drawDeco);
	}

	void Game::NewtonianGravity(bool newNewtonianGravity)
	{
		newtonianGravity = newNewtonianGravity;
		newtonianGravityButton->Stuck(newtonianGravity);
		if (newtonianGravity)
		{
			simulation->grav->start_grav_async();
		}
		else
		{
			simulation->grav->stop_grav_async();
		}
	}

	void Game::AmbientHeat(bool newAmbientHeat)
	{
		ambientHeat = newAmbientHeat;
		ambientHeatButton->Stuck(ambientHeat);
		simulation->aheat_enable = ambientHeat ? 1 : 0;
	}

	void Game::LegacyHeat(bool newLegacyHeat)
	{
		legacyHeat = newLegacyHeat;
		simulation->legacy_enable = legacyHeat ? 1 : 0;
	}

	void Game::WaterEqualisation(bool newWaterEqualisation)
	{
		waterEqualisation = newWaterEqualisation;
		simulation->water_equal_test = waterEqualisation ? 1 : 0;
	}

	void Game::AirMode(SimAirMode newAirMode)
	{
		airMode = newAirMode;
		simulation->air->airMode = int(airMode);
	}

	void Game::GravityMode(SimGravityMode newGravityMode)
	{
		gravityMode = newGravityMode;
		simulation->gravityMode = int(gravityMode);
	}

	void Game::EdgeMode(SimEdgeMode newEdgeMode)
	{
		edgeMode = newEdgeMode;
		simulation->SetEdgeMode(int(edgeMode));
	}

	void Game::AmbientAirTemp(float newAmbientAirTemp)
	{
		ambientAirTemp = newAmbientAirTemp;
		simulation->air->ambientAirTemp = ambientAirTemp;
	}

	void Game::DecoSpace(SimDecoSpace newDecoSpace)
	{
		decoSpace = newDecoSpace;
		simulation->SetDecoSpace(int(decoSpace));
	}

	void Game::PerfectCircle(bool newPerfectCircle)
	{
		perfectCircle = newPerfectCircle;
		static_cast<brush::EllipseBrush *>(brushes[CIRCLE_BRUSH].get())->Perfect(perfectCircle);
	}

	void Game::Paused(bool newPaused)
	{
		paused = newPaused;
		pauseButton->Stuck(paused);
		simulation->sys_pause = paused ? 1 : 0;
	}

	void Game::Log(String message)
	{
		if (log.size() == maxLogEntries)
		{
			log.pop_back();
		}
		log.push_front(LogEntry{ message, gui::SDLWindow::Ticks() });
	}

	void Game::UpdateGroups()
	{
		groupSave->Visible(!rendererButton->Stuck());
		groupRender->Visible(rendererButton->Stuck());
		UpdateRendererSettings();
	}

	void Game::ApplyRendererSettings()
	{
		auto renderMode = 0;
		auto displayMode = 0;
		auto colorMode = 0;
		for (auto flag_button : renderModeButtons)
		{
			renderMode |= std::get<1>(flag_button)->Stuck() ? std::get<0>(flag_button) : 0;
		}
		for (auto flag_button : displayModeButtons)
		{
			displayMode |= std::get<1>(flag_button)->Stuck() ? std::get<0>(flag_button) : 0;
		}
		for (auto flag_button : colorModeButtons)
		{
			colorMode |= std::get<1>(flag_button)->Stuck() ? std::get<0>(flag_button) : 0;
		}
		simulationRenderer->RenderMode(renderMode);
		simulationRenderer->DisplayMode(displayMode);
		simulationRenderer->ColorMode(colorMode);
	}

	void Game::RenderMode(int newRenderMode)
	{
		simulationRenderer->RenderMode(newRenderMode);
		UpdateRendererSettings();
	}

	int Game::RenderMode() const
	{
		return simulationRenderer->RenderMode();
	}

	void Game::DisplayMode(int newDisplayMode)
	{
		simulationRenderer->DisplayMode(newDisplayMode);
		UpdateRendererSettings();
	}

	int Game::DisplayMode() const
	{
		return simulationRenderer->DisplayMode();
	}

	void Game::ColorMode(int newColorMode)
	{
		simulationRenderer->ColorMode(newColorMode);
		UpdateRendererSettings();
	}

	int Game::ColorMode() const
	{
		return simulationRenderer->ColorMode();
	}

	void Game::UpdateRendererSettings()
	{
		if (!rendererButton->Stuck())
		{
			return;
		}
		auto renderMode = simulationRenderer->RenderMode();
		auto displayMode = simulationRenderer->DisplayMode();
		auto colorMode = simulationRenderer->ColorMode();
		for (auto flag_button : renderModeButtons)
		{
			auto flags = std::get<0>(flag_button);
			std::get<1>(flag_button)->Stuck((flags & renderMode) == flags);
		}
		for (auto flag_button : displayModeButtons)
		{
			auto flags = std::get<0>(flag_button);
			std::get<1>(flag_button)->Stuck((flags & displayMode) == flags);
		}
		for (auto flag_button : colorModeButtons)
		{
			auto flags = std::get<0>(flag_button);
			std::get<1>(flag_button)->Stuck((flags & colorMode) == flags);
		}
		for (auto preset_button : rendererPresetButtons)
		{
			std::get<1>(preset_button)->Stuck(std::get<0>(preset_button) == rendererPreset);
		}
	}

	void Game::UpdateLoginButton()
	{
		auto user = Client::Ref().GetAuthUser();
		auto profSize = profileButton->Size();
		if (user.UserID)
		{
			loginButton->Text(gui::IconString(gui::Icons::passoutline, gui::Point{ 2, -1 }));
			loginButton->Size(gui::Point{ profileButton->Position().x - loginButton->Position().x + 1, profSize.y });
			profileButton->Text(gui::OffsetString(1) + user.Username.FromUtf8());
			profileButton->Visible(true);
		}
		else
		{
			loginButton->Text(gui::IconString(gui::Icons::passoutline, gui::Point{ 2, -1 }) + gui::OffsetString(8) + String("[sign in]"));
			loginButton->Size(gui::Point{ profSize.x + profileButton->Position().x - loginButton->Position().x, profSize.y });
			profileButton->Visible(false);
		}
	}

	void Game::RendererPreset(int newRendererPreset)
	{
		rendererPreset = newRendererPreset;
		// * TODO-REDO_UI: simulationRenderer should always exist
		if (simulationRenderer && rendererPreset >= 0 && rendererPreset < int(rendererPresets.size()))
		{
			simulationRenderer->RenderMode(rendererPresets[rendererPreset].renderMode);
			simulationRenderer->DisplayMode(rendererPresets[rendererPreset].displayMode);
			simulationRenderer->ColorMode(rendererPresets[rendererPreset].colorMode);
		}
		UpdateRendererSettings();
	}

	bool Game::HandlePress(ActionSource source, int code, int mod, int multiplier)
	{
		MaybeRehashAvailableActions();
		actionMultiplier = multiplier;
		auto keyWithoutMod = actionKeyWithoutMod(source, code);
		auto keyWithMod = actionKeyWithMod(source, code, mod);
		auto keyToActionIt = availableActions.find(keyWithMod);
		auto *action = keyToActionIt->second;
		if (keyToActionIt != availableActions.end() && std::find_if(activeActions.begin(), activeActions.end(), [action](std::pair<ActionKey, Action *> &active) {
			return active.second == action;
		}) == activeActions.end())
		{
			activeActions.push_back(std::make_pair(keyWithoutMod, action));
			if (action->begin)
			{
				action->begin();
			}
			return true;
		}
		return false;
	}

	bool Game::HandleRelease(ActionSource source, int code)
	{
		MaybeRehashAvailableActions();
		auto keyWithoutMod = actionKeyWithoutMod(source, code);
		auto removed = std::remove_if(activeActions.begin(), activeActions.end(), [keyWithoutMod](std::pair<ActionKey, Action *> &active) {
			return active.first == keyWithoutMod;
		});
		if (removed != activeActions.end())
		{
			for (auto it = removed; it != activeActions.end(); ++it)
			{
				if (it->second->end)
				{
					it->second->end();
				}
			}
			activeActions.erase(removed, activeActions.end());
			return true;
		}
		return false;
	}

	bool Game::EnterContext(ActionContext *context)
	{
		EndAllActions();
		std::vector<ActionContext *> path;
		auto *up = context;
		while (up)
		{
			path.push_back(up);
			up = up->parent;
		}
		std::reverse(path.begin(), path.end());
		auto startFrom = 0U;
		for (auto i = 0U; i < activeContexts.size(); ++i)
		{
			if (activeContexts[i] == context)
			{
				return false;
			}
			if (i < path.size() && activeContexts[i] == path[i])
			{
				startFrom = i + 1U;
			}
		}
		LeaveContext(activeContexts[startFrom]);
		for (auto i = startFrom; i < path.size(); ++i)
		{
			if (activeContexts.back()->yield)
			{
				activeContexts.back()->yield();
			}
			activeContexts.push_back(path[i]);
			if (activeContexts.back()->enter)
			{
				activeContexts.back()->enter();
			}
		}
		ShouldRehashAvailableActions();
		return true;
	}

	bool Game::LeaveContext(ActionContext *context)
	{
		auto pathSize = 0U;
		auto *up = context;
		while (up)
		{
			pathSize += 1U;
			up = up->parent;
		}
		if (activeContexts.size() < pathSize)
		{
			return false;
		}
		for (auto i = 0U; i < pathSize; ++i)
		{
			if (activeContexts[i] == context)
			{
				EndAllActions();
				while (activeContexts.size() >= i)
				{
					if (activeContexts.back()->leave)
					{
						activeContexts.back()->leave();
					}
					activeContexts.pop_back();
					if (activeContexts.back()->resume)
					{
						activeContexts.back()->resume();
					}
				}
				ShouldRehashAvailableActions();
				return true;
			}
		}
		return false;
	}

	void Game::FocusLose()
	{
		EndAllActions();
	}

	void Game::EndAllActions()
	{
		for (auto &keyToActionPtr : activeActions)
		{
			if (keyToActionPtr.second->end)
			{
				keyToActionPtr.second->end();
			}
		}
		activeActions.clear();
	}

	void Game::ShouldRehashAvailableActions()
	{
		EndAllActions();
		shouldRehashAvailableActions = true;
	}

	void Game::ShouldBuildToolPanel()
	{
		shouldBuildToolPanel = true;
	}

	void Game::MaybeRehashAvailableActions()
	{
		if (shouldRehashAvailableActions)
		{
			shouldRehashAvailableActions = false;
			availableActions.clear();
			for (auto *context : activeContexts)
			{
				for (auto &keyToAction : context->actions)
				{
					auto it = availableActions.find(keyToAction.first);
					if (it != availableActions.end())
					{
						if (it->second->freedom)
						{
							// * There might be a computationally cheaper way to do this, but there are only
							//   8 modifier bits i.e. 256 iterations, and there aren't too many non-zero freedom
							//   actions, so this is okay.
							auto freedom = uint64_t(keyToAction.second->freedom) & freedomMask;
							auto mod = (keyToAction.first & actionKeyModMask) >> actionKeyModShift;
							auto rest = keyToAction.first & ~actionKeyModMask;
							for (auto i = 0U; i < 1U << gui::SDLWindow::modBits; ++i)
							{
								if ((i & ~freedom) == mod)
								{
									auto key = (i < actionKeyModShift) | rest;
									auto rit = availableActions.find(key);
									if (rit != availableActions.end())
									{
										availableActions.erase(rit);
									}
								}
							}
						}
						else
						{
							availableActions.erase(it);
						}
					}
					if (keyToAction.second)
					{
						if (keyToAction.second->freedom)
						{
							// * Again, there might be a computationally cheaper way to do this; see above.
							auto freedom = uint64_t(keyToAction.second->freedom) & freedomMask;
							auto mod = (keyToAction.first & actionKeyModMask) >> actionKeyModShift;
							auto rest = keyToAction.first & ~actionKeyModMask;
							for (uint64_t i = 0U; i < 1U << gui::SDLWindow::modBits; ++i)
							{
								if ((i & ~freedom) == mod)
								{
									auto key = (i << actionKeyModShift) | rest;
									availableActions.insert(std::make_pair(key, keyToAction.second));
								}
							}
						}
						else
						{
							availableActions.insert(keyToAction);
						}
					}
				}
			}
			// * TODO-REDO_UI: remove
			// std::cerr << "========= AVAILABLE ACTIONS BEGIN =========" << std::endl;
			// for (auto &av : availableActions)
			// {
			// 	std::cerr << std::setw(16) << std::setfill('0') << std::uppercase << std::hex << av.first << " ";
			// 	std::cerr << av.second->name.ToUtf8() << std::endl;
			// }
			// std::cerr << "========== AVAILABLE ACTIONS END ==========" << std::endl;
		}
	}

	void Game::TogglePaused()
	{
		Paused(!Paused());
	}

	void Game::TogglePrettyPowders()
	{
		PrettyPowders(!PrettyPowders());
	}

	void Game::ToggleDrawGravity()
	{
		DrawGravity(!DrawGravity());
	}

	void Game::ToggleDrawDeco()
	{
		DrawDeco(!DrawDeco());
	}

	void Game::ToggleNewtonianGravity()
	{
		NewtonianGravity(!NewtonianGravity());
	}
				
	void Game::ToggleAmbientHeat()
	{
		AmbientHeat(!AmbientHeat());
	}

	void Game::CycleAir()
	{
		AirMode(SimAirMode((int(AirMode()) + 1) % int(simAirModeMax)));
	}

	void Game::CycleGravity()
	{
		GravityMode(SimGravityMode((int(GravityMode()) + 1) % int(simGravityModeMax)));
	}

	void Game::CycleBrush()
	{
		CurrentBrush((CurrentBrush() + 1) % int(brushes.size()));
	}

	void Game::PreQuit()
	{
		Quit();
	}

	void Game::ToggleDrawHUD()
	{
		DrawHUD(!DrawHUD());
	}

	void Game::ToggleDebugHUD()
	{
		DebugHUD(!DebugHUD());
	}

	void Game::InitActions()
	{
		struct DefaultContext
		{
			String name;
			String parentName;
			String description;
			int precedence;
		};
		struct DefaultKeyBind
		{
			String actionName;
			String contextName;
			ActionKey key;
		};
		auto BUTTON   = [](int code) { return actionKeyWithMod(actionSourceButton, code, 0                                          ); };
		auto BUTTON2  = [](int code) { return actionKeyWithMod(actionSourceButton, code, gui::SDLWindow::mod2                       ); };
		auto  WHEEL   = [](int code) { return actionKeyWithMod(actionSourceWheel , code, 0                                          ); };
		auto   SCAN   = [](int code) { return actionKeyWithMod(actionSourceScan  , code, 0                                          ); };
		auto   SCAN0  = [](int code) { return actionKeyWithMod(actionSourceScan  , code, gui::SDLWindow::mod0                       ); };
		auto   SCAN1  = [](int code) { return actionKeyWithMod(actionSourceScan  , code, gui::SDLWindow::mod1                       ); };
		auto   SCAN01 = [](int code) { return actionKeyWithMod(actionSourceScan  , code, gui::SDLWindow::mod0 | gui::SDLWindow::mod1); };
		auto    SYM   = [](int code) { return actionKeyWithMod(actionSourceSym   , code, 0                                          ); };
		auto    SYM0  = [](int code) { return actionKeyWithMod(actionSourceSym   , code, gui::SDLWindow::mod0                       ); };
		auto    SYM1  = [](int code) { return actionKeyWithMod(actionSourceSym   , code, gui::SDLWindow::mod1                       ); };

		std::vector<DefaultContext> defaultContexts = {
			// * The order in which these are listed is important: the line of
			//   code that defines a context must come after the line of code
			//   that defines its parent.
			{ "DEFAULT_CX_ROOT"  ,  /* nothing */ "", "Simulation"                   , 1000 },
			{ "DEFAULT_CX_SELECT", "DEFAULT_CX_ROOT", "Cutting, copying and stamping", 1001 },
			{ "DEFAULT_CX_PASTE" , "DEFAULT_CX_ROOT", "Pasting"                      , 1002 },
		};
		auto brushFreedom = gui::SDLWindow::mod0 | gui::SDLWindow::mod1 | gui::SDLWindow::mod2;
		auto toolFreedom = gui::SDLWindow::mod0 | gui::SDLWindow::mod1;
		auto propFreedom = gui::SDLWindow::mod0;
		std::vector<Action> gameActions = {
			{ "DEFAULT_AC_NONE"             , "No action"              , 0,    0,                                nullptr,  nullptr },
			{ "DEFAULT_AC_TOGGLEPAUSED"     , "Pause/resume simulation", 0, 1000, [this]() { TogglePaused();           },  nullptr },
			{ "DEFAULT_AC_TOGGLEPRETTYPOWDERS", "Pretty powders"         , 0, 2000, [this]() { TogglePrettyPowders();    },  nullptr },
			{ "DEFAULT_AC_TOGGLEDRAWGRAV"   , "Draw gravity field"     , 0, 2001, [this]() { ToggleDrawGravity();      },  nullptr },
			{ "DEFAULT_AC_TOGGLEDRAWDECO"   , "Draw decorations"       , 0, 2002, [this]() { ToggleDrawDeco();         },  nullptr },
			{ "DEFAULT_AC_TOGGLENEWTONIAN"  , "Newtonian gravity"      , 0, 2003, [this]() { ToggleNewtonianGravity(); },  nullptr },
			{ "DEFAULT_AC_TOGGLEAMBHEAT"    , "Ambient heat simulation", 0, 2004, [this]() { ToggleAmbientHeat();      },  nullptr },
			{ "DEFAULT_AC_CYCLEAIR"         , "Cycle air mode"         , 0,    0, [this]() { CycleAir();               },  nullptr },
			{ "DEFAULT_AC_CYCLEGRAV"        , "Cycle gravity mode"     , 0,    0, [this]() { CycleGravity();           },  nullptr },
			{ "DEFAULT_AC_CYCLEBRUSH"       , "Cycle brush"            , 0,    0, [this]() { CycleBrush();             },  nullptr },
			{ "DEFAULT_AC_QUIT"             , "Quit"                   , 0,    0, [this]() { PreQuit();                },  nullptr },
			{ "DEFAULT_AC_TOGGLEHUD"        , "Toggle HUD"             , 0,    0, [this]() { ToggleDrawHUD();          },  nullptr },
			{ "DEFAULT_AC_TOGGLEDEBUG"      , "Toggle Debug HUD"       , 0,    0, [this]() { ToggleDebugHUD();         },  nullptr },
			{ "DEFAULT_AC_TOOLSEARCH"       , "Open tool search"       , 0,    0, [this]() { OpenToolSearch();         },  nullptr },
			{ "DEFAULT_AC_SHOWCONSOLE"      , "Open console"           , 0,    0, [this]() { OpenConsole();            },  nullptr },
			{ "DEFAULT_AC_RELOADSIM"        , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_AUTHORINFO"       , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_TOOLFIND"         , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_FRAMESTEP"        , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_GRIDUP"           , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_GRIDDOWN"         , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_TOGGLEINTRO"      , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_TOGGLEDECOTOOL"   , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_REDO"             , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_RESETAMBHEAT"     , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_RESETSPARK"       , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_RESETPRESSURE"    , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_BEGINCOPY"        , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_BEGINCUT"         , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_BEGINSTAMP"       , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_BEGINPASTE"       , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_LASTSTAMP"        , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_OPENSTAMPS"       , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_INVERTPRESSURE"   , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_INSTALL"          , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_TOGGLESDELETE"    , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_TOGGLEREPLACE"    , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_ZOOM"             , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_UNDO"             , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_RESTORE"          , ""                       , 0,    0,                                nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_PROPTOOL"         , ""                       , propFreedom,    0,                      nullptr,  nullptr }, // * TODO-REDO_UI
			{ "DEFAULT_AC_BRUSHZOOMSIZEUP"  , "Increase brush or zoom window size", brushFreedom, 1200, [this]() { AdjustBrushOrZoomSize( 1); }, nullptr },
			{ "DEFAULT_AC_BRUSHZOOMSIZEDOWN", "Decrease brush or zoom window size", brushFreedom, 1201, [this]() { AdjustBrushOrZoomSize(-1); }, nullptr },
			{ "DEFAULT_AC_TOOL0", "Primary tool"  , toolFreedom, 1100, [this]() { ToolStart(0); }, [this]() { ToolFinish(0); } },
			{ "DEFAULT_AC_TOOL1", "Secondary tool", toolFreedom, 1101, [this]() { ToolStart(1); }, [this]() { ToolFinish(1); } },
			{ "DEFAULT_AC_TOOL2", "Tertiary tool" , toolFreedom, 1102, [this]() { ToolStart(2); }, [this]() { ToolFinish(2); } },
		};
		std::vector<DefaultKeyBind> defaultKeyBinds = {
			{ "DEFAULT_AC_TOGGLEPAUSED"     , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_SPACE       ) },
			{ "DEFAULT_AC_TOGGLEDRAWGRAV"   , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_G           ) },
			{ "DEFAULT_AC_TOGGLEDRAWDECO"   , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_B           ) },
			{ "DEFAULT_AC_TOGGLENEWTONIAN"  , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_N           ) },
			{ "DEFAULT_AC_TOGGLEAMBHEAT"    , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_U           ) },
			{ "DEFAULT_AC_SHOWCONSOLE"      , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_GRAVE       ) },
			{ "DEFAULT_AC_PROPTOOL"         , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_P           ) },
			{ "DEFAULT_AC_PROPTOOL"         , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_F2          ) },
			{ "DEFAULT_AC_TOGGLEDEBUG"      , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_F3          ) },
			{ "DEFAULT_AC_TOGGLEDEBUG"      , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_D           ) },
			{ "DEFAULT_AC_RELOADSIM"        , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_F5          ) },
			{ "DEFAULT_AC_RELOADSIM"        , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_R           ) },
			{ "DEFAULT_AC_AUTHORINFO"       , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_A           ) },
			{ "DEFAULT_AC_TOOLSEARCH"       , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_E           ) },
			{ "DEFAULT_AC_TOOLFIND"         , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_F           ) },
			{ "DEFAULT_AC_FRAMESTEP"        , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_F           ) },
			{ "DEFAULT_AC_GRIDUP"           , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_G           ) },
			{ "DEFAULT_AC_GRIDDOWN"         , "DEFAULT_CX_ROOT"  ,   SCAN0 (SDL_SCANCODE_G           ) },
			{ "DEFAULT_AC_TOGGLEINTRO"      , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_F1          ) },
			{ "DEFAULT_AC_TOGGLEINTRO"      , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_H           ) },
			{ "DEFAULT_AC_TOGGLEHUD"        , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_H           ) },
			{ "DEFAULT_AC_TOGGLEDECOTOOL"   , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_B           ) },
			{ "DEFAULT_AC_REDO"             , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_Y           ) },
			{ "DEFAULT_AC_CYCLEAIR"         , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_Y           ) },
			{ "DEFAULT_AC_CYCLEGRAV"        , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_W           ) },
			{ "DEFAULT_AC_QUIT"             , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_Q           ) },
			{ "DEFAULT_AC_QUIT"             , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_ESCAPE      ) },
			{ "DEFAULT_AC_RESETAMBHEAT"     , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_U           ) },
			{ "DEFAULT_AC_RESETSPARK"       , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_EQUALS      ) },
			{ "DEFAULT_AC_RESETPRESSURE"    , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_EQUALS      ) },
			{ "DEFAULT_AC_BEGINCOPY"        , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_C           ) },
			{ "DEFAULT_AC_BEGINCUT"         , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_X           ) },
			{ "DEFAULT_AC_BEGINSTAMP"       , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_S           ) },
			{ "DEFAULT_AC_BEGINPASTE"       , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_V           ) },
			{ "DEFAULT_AC_LASTSTAMP"        , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_L           ) },
			{ "DEFAULT_AC_OPENSTAMPS"       , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_K           ) },
			{ "DEFAULT_AC_INVERTPRESSURE"   , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_I           ) },
			{ "DEFAULT_AC_INSTALL"          , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_I           ) },
			{ "DEFAULT_AC_TOGGLESDELETE"    , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_SEMICOLON   ) },
			{ "DEFAULT_AC_TOGGLEREPLACE"    , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_SEMICOLON   ) },
			{ "DEFAULT_AC_BRUSHZOOMSIZEUP"  , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_RIGHTBRACKET) },
			{ "DEFAULT_AC_BRUSHZOOMSIZEDOWN", "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_LEFTBRACKET ) },
			{ "DEFAULT_AC_BRUSHZOOMSIZEUP"  , "DEFAULT_CX_ROOT"  ,  WHEEL  (                        1) },
			{ "DEFAULT_AC_BRUSHZOOMSIZEDOWN", "DEFAULT_CX_ROOT"  ,  WHEEL  (                       -1) },
			{ "DEFAULT_AC_ZOOM"             , "DEFAULT_CX_ROOT"  ,   SCAN  (SDL_SCANCODE_Z           ) },
			{ "DEFAULT_AC_UNDO"             , "DEFAULT_CX_ROOT"  ,   SCAN1 (SDL_SCANCODE_Z           ) },
			{ "DEFAULT_AC_RESTORE"          , "DEFAULT_CX_ROOT"  ,   SCAN01(SDL_SCANCODE_Z           ) },
			{ "DEFAULT_AC_TOGGLESDELETE"    , "DEFAULT_CX_ROOT"  ,    SYM1 (SDLK_INSERT              ) },
			{ "DEFAULT_AC_TOGGLESDELETE"    , "DEFAULT_CX_ROOT"  ,    SYM  (SDLK_DELETE              ) },
			{ "DEFAULT_AC_TOGGLEREPLACE"    , "DEFAULT_CX_ROOT"  ,    SYM  (SDLK_INSERT              ) },
			{ "DEFAULT_AC_CYCLEBRUSH"       , "DEFAULT_CX_ROOT"  ,    SYM  (SDLK_TAB                 ) },
			{ "DEFAULT_AC_TOOL0"            , "DEFAULT_CX_ROOT"  , BUTTON  (SDL_BUTTON_LEFT          ) },
			{ "DEFAULT_AC_TOOL1"            , "DEFAULT_CX_ROOT"  , BUTTON  (SDL_BUTTON_RIGHT         ) },
			{ "DEFAULT_AC_TOOL2"            , "DEFAULT_CX_ROOT"  , BUTTON  (SDL_BUTTON_MIDDLE        ) },
			{ "DEFAULT_AC_TOOL2"            , "DEFAULT_CX_ROOT"  , BUTTON2 (SDL_BUTTON_LEFT          ) },
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_SELECT", BUTTON  (SDL_BUTTON_LEFT          ) }, // * Unbind TOOL0.
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_SELECT", BUTTON  (SDL_BUTTON_RIGHT         ) }, // * Unbind TOOL1.
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_SELECT", BUTTON  (SDL_BUTTON_MIDDLE        ) }, // * Unbind TOOL2.
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_SELECT", BUTTON2 (SDL_BUTTON_LEFT          ) }, // * Unbind TOOL2.
			{ "DEFAULT_AC_FINISHSELECT"     , "DEFAULT_CX_SELECT", BUTTON  (SDL_BUTTON_LEFT          ) },
			{ "DEFAULT_AC_CANCELSELECT"     , "DEFAULT_CX_SELECT", BUTTON  (SDL_BUTTON_RIGHT         ) },
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_PASTE" , BUTTON  (SDL_BUTTON_LEFT          ) }, // * Unbind TOOL0.
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_PASTE" , BUTTON  (SDL_BUTTON_RIGHT         ) }, // * Unbind TOOL1.
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_PASTE" , BUTTON  (SDL_BUTTON_MIDDLE        ) }, // * Unbind TOOL2.
			{ "DEFAULT_AC_NONE"             , "DEFAULT_CX_PASTE" , BUTTON2 (SDL_BUTTON_LEFT          ) }, // * Unbind TOOL2.
			{ "DEFAULT_AC_FINISHPASTE"      , "DEFAULT_CX_PASTE" , BUTTON  (SDL_BUTTON_LEFT          ) },
			{ "DEFAULT_AC_CANCELPASTE"      , "DEFAULT_CX_PASTE" , BUTTON  (SDL_BUTTON_RIGHT         ) },
		};
		for (auto i = 0; i < int(rendererPresets.size()); ++i)
		{
			StringBuilder sb;
			sb << "DEFAULT_AC_RENDERERPRESET" << i;
			auto actionName	= sb.Build();
			gameActions.push_back({ actionName, rendererPresets[i].name, 0, 3000 + i, [this, i]() {
				if (i != 10 || debugHUD)
				{
					RendererPreset(i);
				}
			} });
			defaultKeyBinds.push_back({ actionName, "DEFAULT_CX_ROOT", i == 10 ? SYM0('1') : SYM('0' + i) });
		}

		for (auto &action : gameActions)
		{
			actions.insert(std::make_pair(action.name, std::make_unique<Action>(action)));
		}
		ActionContext *rootContext = nullptr;
		for (auto &context : defaultContexts)
		{
			auto ptr = std::make_unique<ActionContext>(ActionContext{ context.name, context.description, context.precedence });
			if (context.parentName == "")
			{
				rootContext = ptr.get();
				ptr->parent = nullptr;
			}
			else
			{
				auto parentIt = contexts.find(context.parentName);
				parentIt->second->children.push_back(ptr.get());
				ptr->parent = parentIt->second.get();
			}
			contexts.insert(std::make_pair(context.name, std::move(ptr)));
		}
		std::map<String, std::map<String, std::vector<ActionKey>>> actionToKeyBinds;
		for (auto &bindStr : Client::Ref().GetPrefStringArray("KeyBinds"))
		{
			if (auto contextSplit = bindStr.SplitBy(' '))
			{
				auto contextName = contextSplit.Before();
				auto actionWithKeyStr = contextSplit.After();
				if (auto actionSplit = actionWithKeyStr.SplitBy(' '))
				{
					auto actionName = actionSplit.Before();
					auto keyStr = actionSplit.After();
					ActionKey key;
					if (actionKeyFromString(keyStr, key))
					{
						actionToKeyBinds[contextName][actionName].push_back(key);
					}
				}
			}
		}
		std::map<String, std::map<String, std::vector<ActionKey>>> actionToDefaultKeyBinds;
		for (auto &bind : defaultKeyBinds)
		{
			actionToDefaultKeyBinds[bind.contextName][bind.actionName].push_back(bind.key);
		}
		for (auto &defaultContextActions : actionToDefaultKeyBinds)
		{
			auto &context = actionToKeyBinds[defaultContextActions.first];
			for (auto &defaultKeyBinds : defaultContextActions.second)
			{
				// * insert only inserts if no element with equivalent key exists.
				context.insert(defaultKeyBinds);
			}
		}
		for (auto &contextActions : actionToKeyBinds)
		{
			auto contextIt = contexts.find(contextActions.first);
			if (contextIt != contexts.end())
			{
				auto *context = contextIt->second.get();
				for (auto &keyBinds : contextActions.second)
				{
					Action *action;
					bool found = false;
					if (keyBinds.first == "DEFAULT_AC_NONE")
					{
						found = true;
						action = nullptr;
					}
					else
					{
						auto actionIt = actions.find(keyBinds.first);
						if (actionIt != actions.end())
						{
							found = true;
							action = actionIt->second.get();
						}
					}
					if (found)
					{
						for (auto &bind : keyBinds.second)
						{
							context->actions.insert(std::make_pair(bind, action));
						}
					}
				}
			}
		}

		activeContexts.push_back(rootContext);
	}

	void Game::ToolStart(int index)
	{
		ToolMode mode;
		switch (gui::SDLWindow::Ref().Mod() & (gui::SDLWindow::mod0 | gui::SDLWindow::mod1))
		{
		case 0:
			mode = toolModeFree;
			break;

		case gui::SDLWindow::mod0:
			mode = toolModeLine;
			break;

		case gui::SDLWindow::mod1:
			mode = toolModeRect;
			break;

		case gui::SDLWindow::mod0 | gui::SDLWindow::mod1:
			mode = toolModeFill;
			break;
		}
		if (index < 0 || index >= 3)
		{
			return;
		}
		if (activeToolMode != toolModeNone)
		{
			ToolCancel(activeToolIndex);
		}
		activeToolIndex = index;
		activeToolMode = mode;
		UpdateLastBrush(activeToolIndex);
		auto pos = gui::SDLWindow::Ref().MousePosition();
		switch (activeToolMode)
		{
		case toolModeFree:
			ToolDrawWithBrush(pos, pos);
			toolStartPos = pos;
			break;

		case toolModeLine:
			toolStartPos = pos;
			break;

		case toolModeRect:
			toolStartPos = pos;
			break;

		case toolModeFill:
			currentTools[activeToolIndex]->Fill(pos);
			break;

		default:
			break;
		}
	}

	void Game::ToolCancel(int index)
	{
		if (activeToolMode == toolModeNone)
		{
			return;
		}
		activeToolMode = toolModeNone;
	}

	void Game::ToolFinish(int index)
	{
		if (activeToolMode == toolModeNone)
		{
			return;
		}
		auto pos = gui::SDLWindow::Ref().MousePosition();
		switch (activeToolMode)
		{
		case toolModeLine:
			ToolDrawWithBrush(toolStartPos, pos);
			break;

		case toolModeRect:
			{
				auto rect = gui::Rect::FromCorners(toolStartPos, pos);
				for (auto y = rect.pos.y; y < rect.pos.y + rect.size.y; ++y)
				{
					for (auto x = rect.pos.x; x < rect.pos.x + rect.size.x; ++x)
					{
						currentTools[activeToolIndex]->Draw(gui::Point{ x, y });
					}
				}
			}
			break;

		default:
			break;
		}
		activeToolMode = toolModeNone;
	}

	void Game::AdjustBrushOrZoomSize(int sign)
	{
		auto adjust = [this, sign](int &v) {
			if (gui::SDLWindow::Ref().Mod() & gui::SDLWindow::mod2)
			{
				v += sign * actionMultiplier;
			}
			else
			{
				auto times = actionMultiplier;
				for (auto i = 0; i < times; ++i)
				{
					constexpr int num = 8;
					constexpr int denom = 7;
					int vv = (sign > 0) ? (v * num / denom) : (v * denom / num);
					if (vv == v)
					{
						vv += sign;
					}
					v = vv;
				}
			}
			if (v <   0) v =   0;
			if (v > 200) v = 200;
		};
		auto radius = BrushRadius();
		if (!(gui::SDLWindow::Ref().Mod() & gui::SDLWindow::mod1))
		{
			adjust(radius.x);
		}
		if (!(gui::SDLWindow::Ref().Mod() & gui::SDLWindow::mod0))
		{
			adjust(radius.y);
		}
		BrushRadius(radius);
		// * TODO-REDO_UI: zoom
	}

	void Game::CurrentBrush(int newCurrentBrush)
	{
		brushes[newCurrentBrush]->RequestedRadius(BrushRadius());
		currentBrush = newCurrentBrush;
		UpdateLastBrush(activeToolIndex);
	}

	void Game::OpenToolSearch()
	{
		gui::SDLWindow::Ref().EmplaceBack<toolsearch::ToolSearch>();
	}

	void Game::OpenConsole()
	{
		gui::SDLWindow::Ref().EmplaceBack<console::Console>();
	}

	void Game::BrushRadius(gui::Point newBrushRadius)
	{
		brushes[currentBrush]->RequestedRadius(newBrushRadius);
		cellAlignedBrush->RequestedRadius(newBrushRadius);
	}

	gui::Point Game::BrushRadius() const
	{
		return brushes[currentBrush]->RequestedRadius();
	}

	void Game::UpdateLastBrush(int toolIndex)
	{
		lastBrush = currentTools[toolIndex]->CellAligned() ? cellAlignedBrush.get() : brushes[currentBrush].get();
	}

	void Game::CurrentTool(int index, tool::Tool *tool)
	{
		currentTools[index] = tool;
		auto gz = false;
		for (auto *tool : currentTools)
		{
			gz |= tool && tool->EnablesGravityZones();
		}
		simulationRenderer->GravityZonesEnabled(gz);
		UpdateLastBrush(index);
	}

	void Game::Favorite(const String &identifier, bool newFavorite)
	{
		if (newFavorite != Favorite(identifier))
		{
			if (newFavorite)
			{
				favorites.insert(identifier);
			}
			else
			{
				favorites.erase(identifier);
			}
			std::vector<Json::Value> arr;
			for (auto &favorite : favorites)
			{
				arr.push_back(favorite.ToUtf8());
			}
			Client::Ref().SetPref("Favorites", arr);
			shouldBuildToolPanel = true;
		}
	}

	bool Game::Favorite(const String &identifier) const
	{
		return favorites.find(identifier) != favorites.end();
	}
}
}
