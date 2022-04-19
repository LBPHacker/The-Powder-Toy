#include "Game.h"

#include "activities/console/Console.h"
#include "activities/login/Login.h"
#include "activities/options/Options.h"
#include "activities/profile/Profile.h"
#include "activities/toolsearch/ToolSearch.h"
#include "activities/browser/OnlineBrowser.h"
#include "activities/browser/LocalBrowser.h"
#include "activities/save/LocalSave.h"
#include "activities/save/OnlineSave.h"
#include "activities/preview/Preview.h"
#include "activities/edittags/EditTags.h"
#include "backend/Backend.h"
#include "backend/ExecVote.h"
#include "brush/EllipseBrush.h"
#include "brush/RectangleBrush.h"
#include "brush/TriangleBrush.h"
#include "brush/BitmapBrush.h"
#include "brush/CellAlignedBrush.h"
#include "common/Line.h"
#include "common/Platform.h"
#include "Format.h"
#include "graphics/SimulationRenderer.h"
#include "graphics/ThumbnailRenderer.h"
#include "graphics/Pix.h"
#include "gui/SDLAssert.h"
#include "gui/SDLWindow.h"
#include "gui/Button.h"
#include "gui/SDLWindow.h"
#include "gui/Texture.h"
#include "gui/Icons.h"
#include "gui/Appearance.h"
#include "graphics/FontData.h"
#include "simulation/GameSave.h"
#include "simulation/Simulation.h"
#include "simulation/SimTool.h"
#include "simulation/ElementGraphics.h"
#include "simulation/Air.h"
#include "simulation/Gravity.h"
#include "simulation/Snapshot.h"
#include "simulation/SnapshotDelta.h"
#include "simulation/ToolClasses.h"
#include "Misc.h"
#include "prefs/GlobalPrefs.h"
#include "ToolPanel.h"
#include "tool/ElementTool.h"
#include "tool/SimulationTool.h"
#include "tool/LifeElementTool.h"
#include "tool/WallTool.h"
#include "language/Language.h"

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

#ifdef DEBUG
# include <iostream>
#endif

void Element_FILT_renderWavelengths(int &colr, int &colg, int &colb, int wl);

namespace activities::game
{
	constexpr auto undoHistoryLimitLimit = 200; // * Yes, limit limit: a limit on how big a limit you can set.
	constexpr auto maxLogEntries         = 20;
	constexpr auto logTimeoutTicks       = 2000;
	constexpr auto logFadeTicks          = 1000;
	constexpr auto windowSize            = gui::Point{ WINDOWW, WINDOWH };
	constexpr auto simulationSize        = gui::Point{ XRES, YRES };
	constexpr auto simulationRect        = gui::Rect{ { 0, 0 }, simulationSize };
	constexpr auto bottomLeftTextPos     = gui::Point{ 20, simulationSize.y - 34 };
	constexpr auto introTextTimeout      = 30.f;

	HistoryEntry::~HistoryEntry()
	{
	}

	static uint64_t actionKeyWithoutMod(ActionSource source, int code)
	{
		return ((uint64_t(source) & UINT64_C(0xFFFF)) << 32) | (uint64_t(code) & UINT64_C(0xFFFFFFFF));
	}

	static constexpr int actionKeyModShift = 48;
	static constexpr uint64_t dofMask = (1U << gui::SDLWindow::modBits) - 1U;
	static constexpr uint64_t actionKeyModMask = dofMask << actionKeyModShift;
	static uint64_t actionKeyWithMod(ActionSource source, int code, int mod)
	{
		return actionKeyWithoutMod(source, code) | ((uint64_t(mod) & dofMask) << actionKeyModShift);
	}

	static bool actionKeyFromString(String str, uint64_t &key)
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
			// else if (sourceSplit.Before() == "sym") // * See commented-out definition of actionSourceSym.
			// {
			// 	source = actionSourceSym;
			// }
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

	auto BUTTON   = [](int code) { return actionKeyWithMod(actionSourceButton, code, 0                                          ); };
	auto BUTTON2  = [](int code) { return actionKeyWithMod(actionSourceButton, code, gui::SDLWindow::mod2                       ); };
	auto  WHEEL   = [](int code) { return actionKeyWithMod(actionSourceWheel , code, 0                                          ); };
	auto   SCAN   = [](int code) { return actionKeyWithMod(actionSourceScan  , code, 0                                          ); };
	auto   SCAN0  = [](int code) { return actionKeyWithMod(actionSourceScan  , code, gui::SDLWindow::mod0                       ); };
	auto   SCAN1  = [](int code) { return actionKeyWithMod(actionSourceScan  , code, gui::SDLWindow::mod1                       ); };
	auto   SCAN01 = [](int code) { return actionKeyWithMod(actionSourceScan  , code, gui::SDLWindow::mod0 | gui::SDLWindow::mod1); };

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
		{ gui::Icons::wall       , "WALL"      },
		{ gui::Icons::electronic , "ELEC"      },
		{ gui::Icons::powered    , "POWERED"   },
		{ gui::Icons::sensor     , "SENSOR"    },
		{ gui::Icons::force      , "FORCE"     },
		{ gui::Icons::explosive  , "EXPLOSIVE" },
		{ gui::Icons::gas        , "GAS"       },
		{ gui::Icons::liquid     , "LIQUID"    },
		{ gui::Icons::powder     , "POWDERS"   },
		{ gui::Icons::solid      , "SOLIDS"    },
		{ gui::Icons::radioactive, "NUCLEAR"   },
		{ gui::Icons::special    , "SPECIAL"   },
		{ gui::Icons::gol        , "LIFE"      },
		{ gui::Icons::tool       , "TOOL"      },
		{ gui::Icons::favorite   , "FAVORITES" },
		{ gui::Icons::paint      , "DECO"      },
	} };

	struct RendererPreset
	{
		String name;
		uint32_t renderMode;
		uint32_t displayMode;
		uint32_t colorMode;
		uint64_t defaultKey;
	};
	std::vector<RendererPreset> rendererPresets = {
		{ "ALTVELOCITY" ,    RENDER_EFFE | RENDER_BASC,    DISPLAY_AIRC,              0, SCAN (SDL_SCANCODE_0) },
		{ "VELOCITY"    ,    RENDER_EFFE | RENDER_BASC,    DISPLAY_AIRV,              0, SCAN (SDL_SCANCODE_1) },
		{ "PRESSURE"    ,    RENDER_EFFE | RENDER_BASC,    DISPLAY_AIRP,              0, SCAN (SDL_SCANCODE_2) },
		{ "PERSISTENT"  ,    RENDER_EFFE | RENDER_BASC,    DISPLAY_PERS,              0, SCAN (SDL_SCANCODE_3) },
		{ "FIRE"        ,    RENDER_EFFE | RENDER_BASC|
                             RENDER_FIRE | RENDER_SPRK,               0,              0, SCAN (SDL_SCANCODE_4) },
		{ "BLOB"        ,    RENDER_EFFE | RENDER_BLOB|
                             RENDER_FIRE | RENDER_SPRK,               0,              0, SCAN (SDL_SCANCODE_5) },
		{ "HEAT"        ,                  RENDER_BASC,    DISPLAY_AIRH,    COLOUR_HEAT, SCAN (SDL_SCANCODE_6) },
		{ "FANCY"       ,    RENDER_EFFE | RENDER_BASC|
                             RENDER_FIRE | RENDER_SPRK|
                             RENDER_BLUR | RENDER_GLOW,    DISPLAY_WARP,              0, SCAN (SDL_SCANCODE_7) },
		{ "NOTHING"     ,                  RENDER_BASC,               0,              0, SCAN (SDL_SCANCODE_8) },
		{ "HEATGRADIENT",                  RENDER_BASC,               0,    COLOUR_GRAD, SCAN (SDL_SCANCODE_9) },
		{ "LIFEGRADIENT",                  RENDER_BASC,               0,    COLOUR_LIFE, SCAN0(SDL_SCANCODE_1) },
	};

	Game::Game()
	{
		ShadedToolTips(true);
		simulation = std::make_unique<Simulation>();
		simulationRenderer = std::make_unique<SimulationRenderer>();
		InitActions();
		InitWindow();
		InitBrushes();
		InitTools();
		LoadPreferences();
		InitCommandInterface();
	}

	Game::~Game()
	{
	}

	void Game::LoadPreferences()
	{
		// * TODO-REDO_UI: Load and save prefs properly. Most importantly, remove pref saving from Options.
		auto &gpref = prefs::GlobalPrefs::Ref();
		prefs::Prefs::DeferWrite dw(gpref);
		RendererPreset(4);
		DrawGravity(false);
		DrawDeco(true);
		StickyCategories(gpref.Get("MouseClickRequired"         , false));
		PerfectCircle   (gpref.Get("PerfectCircleBrush"         , true));
		IncludePressure (gpref.Get("Simulation.IncludePressure" , true));
		NewtonianGravity(gpref.Get("Simulation.NewtonianGravity", 0));
		AmbientHeat     (gpref.Get("Simulation.AmbientHeat"     , 0));
		PrettyPowders   (gpref.Get("Simulation.PrettyPowder"    , 0));
		AmbientAirTemp  (gpref.Get("Simulation.AmbientAirTemp"  , float(R_TEMP) + 273.15f));
		EdgeMode        (gpref.Get("Simulation.EdgeMode"        , simEdgeModeMax, simEdgeModeVoid));
		DecoSpace       (gpref.Get("Simulation.DecoSpace"       , simDecoSpaceMax, simDecoSpaceSRGB));
		HistoryLimit    (gpref.Get("Simulation.UndoHistoryLimit", 5));

		auto ensureRelative = [](String str) {
			Path path;
			try
			{
				path = Path::FromString(str);
			}
			catch (const Path::FormatError &)
			{
				return Path::FromString(".");
			}
			if (!path.Relative() || path.ParentDepth())
			{
				return Path::FromString(".");
			}
			return path;
		};
		SavesPath(ensureRelative(gpref.Get("Simulation.SavesPath", String("."))));
		StampsPath(ensureRelative(gpref.Get("Simulation.StampsPath", String("."))));

		GravityMode(simGravityModeVertical);
		AirMode(simAirModeOn);
		LegacyHeat(false);
		WaterEqualisation(false);
		for (auto &favorite : gpref.Get("Favorites", std::vector<ByteString>{}))
		{
			Favorite(favorite.FromUtf8(), true);
		}

		DecoColor(PixRGBA(
			gpref.Get("Decoration.Red",   255),
			gpref.Get("Decoration.Green",   0),
			gpref.Get("Decoration.Blue",  255),
			gpref.Get("Decoration.Alpha", 255)
		));
	}

	void Game::InitCommandInterface()
	{
#ifdef LUACONSOLE
		commandInterface = std::make_unique<LuaScriptInterface>(this);
		static_cast<LuaScriptInterface *>(commandInterface.get())->Init();
#else
		commandInterface = std::make_unique<TPTScriptInterface>(this);
#endif
	}

	void Game::InitWindow()
	{
		Size(windowSize);

		auto addButtonHorizToolTip = [](gui::Button *button, String toolTip) {
			button->ToolTip(std::make_shared<gui::ToolTipInfo>(gui::ToolTipInfo{ toolTip, { 12, simulationSize.y - 12 } }));
		};
		auto addButtonHorizAny = [addButtonHorizToolTip](bool right, String text, int width, bool alignLeft, int y, gui::Button::ClickCallback cb, std::shared_ptr<gui::Button> button, int &nextXL, int &nextXR, String toolTip) {
			if (right)
			{
				nextXR -= width + 1;
			}
			button->Size({ width, 15 });
			button->Position({ right ? nextXR : nextXL, y });
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
			addButtonHorizToolTip(button.get(), toolTip);
			return button;
		};

		auto abhlNextX = 1;
		auto abhrNextX = windowSize.x;
		auto addButtonHoriz = [this, addButtonHorizAny, &abhlNextX, &abhrNextX](bool right, String text, int width, bool alignLeft, gui::Button::ClickCallback cb, String toolTip) {
			return addButtonHorizAny(right, text, width, alignLeft, windowSize.y - 16, cb, EmplaceChild<gui::Button>(), abhlNextX, abhrNextX, toolTip);
		};

		auto addToolTipVert = [](gui::Button *button, String toolTip) {
			auto pos = gui::Point{ simulationSize.x - gui::SDLWindow::Ref().TextSize(toolTip).x - 11, button->AbsolutePosition().y + 2 };
			if (pos.y > simulationSize.y - 12)
			{
				pos.y = simulationSize.y - 12;
			}
			button->ToolTip(std::make_shared<gui::ToolTipInfo>(gui::ToolTipInfo{ toolTip, pos }));
		};

		pauseButton = addButtonHoriz(true, gui::IconString(gui::Icons::pause, { -1, -2 }), 15, false, [this]() {
			TogglePaused();
		}, "DEFAULT_LS_GAME_HORIZ_PAUSE_TIP"_Ls()).get();
		rendererButton = addButtonHoriz(
			true,
			gui::BlendAddString() + gui::ColorString({ 0xFF, 0x00, 0x00 }) + gui::IconString(gui::Icons::gradient1, { 0, -1 }) +
			gui::StepBackString() + gui::ColorString({ 0x00, 0xFF, 0x00 }) + gui::IconString(gui::Icons::gradient2, { 0, -1 }) +
			gui::StepBackString() + gui::ColorString({ 0x00, 0x00, 0xFF }) + gui::IconString(gui::Icons::gradient3, { 0, -1 }),
			15, false, [this]() {
				rendererButton->Stuck(!rendererButton->Stuck());
				UpdateGroups();
			},
			"DEFAULT_LS_GAME_HORIZ_RENDERER_TIP"_Ls()
		).get();

		auto abhlNextXSave = abhlNextX;
		auto abhrNextXSave = abhrNextX;
		groupSave = EmplaceChild<gui::Component>().get();
		groupSave->Position({ 0, windowSize.y - 16 });
		groupSave->Size({ abhrNextX, 16 });
		auto addButtonHorizSave = [this, addButtonHorizAny, &abhlNextXSave, &abhrNextXSave](bool right, String text, int width, bool alignLeft, gui::Button::ClickCallback cb, String toolTip) {
			return addButtonHorizAny(right, text, width, alignLeft, 0, cb, groupSave->EmplaceChild<gui::Button>(), abhlNextXSave, abhrNextXSave, toolTip);
		};

		// * TODO-REDO_UI: Maybe do the same on right click as on ctrl + left?
		openButton = addButtonHorizSave(false, gui::IconString(gui::Icons::open, { 0, -2 }), 17, false, [this]() {
			switch (GetOpenButtonAction())
			{
			case openButtonOnline:
				if (!onlineBrowser)
				{
					onlineBrowser = std::make_shared<browser::OnlineBrowser>();
					onlineBrowser->Refresh();
				}
				gui::SDLWindow::Ref().PushBack(onlineBrowser);
				break;

			case openButtonLocal:
				if (!localBrowser)
				{
					localBrowser = std::make_shared<browser::LocalBrowser>();
					localBrowser->Refresh();
				}
				gui::SDLWindow::Ref().PushBack(localBrowser);
				break;

			default:
				break;
			}
		}, "0").get(); // * The tooltip needs to be non-empty for addButtonHorizSave to assign a ToolTipInfo to the button, which UpdateSaveControls requires.
		reloadButton = addButtonHorizSave(false, gui::IconString(gui::Icons::reload, { 0, -2 }), 17, false, [this]() {
			switch (GetReloadButtonAction())
			{
			case reloadButtonLocal:
			case reloadButtonOnline:
				ReloadSimulation();
				break;

			case reloadButtonPreview:
				gui::SDLWindow::Ref().EmplaceBack<preview::Preview>(reloadSourceInfo.idv);
				break;

			default:
				break;
			}
		}, "0").get(); // * The tooltip needs to be non-empty for addButtonHorizSave to assign a ToolTipInfo to the button, which UpdateSaveControls requires.

		updateButton = groupSave->EmplaceChild<gui::Button>().get();
		saveButton = addButtonHorizSave(false, "", 150, true, [this]() {
			switch (GetUpdateButtonAction())
			{
			case updateButtonLocal:
				// * TODO-REDO_UI
				break;

			case updateButtonOnline:
				// * TODO-REDO_UI
				break;

			default:
				break;
			}
		}, "0").get(); // * The tooltip needs to be non-empty for addButtonHorizSave to assign a ToolTipInfo to the button, which UpdateSaveControls requires.
		saveButton->Align(gui::Alignment::horizLeft | gui::Alignment::vertCenter);
		addButtonHorizToolTip(updateButton, "0"); // * The tooltip needs to be non-empty for addButtonHorizSave to assign a ToolTipInfo to the button, which UpdateSaveControls requires.
		saveButton->Click([this]() {
			auto mod0 = bool(gui::SDLWindow::Ref().Mod() & gui::SDLWindow::mod0);
			auto saveData = simulation->Save(includePressure != mod0);
			switch (GetSaveButtonAction())
			{
			case saveButtonLocal:
			case saveButtonForceLocal:
				gui::SDLWindow::Ref().EmplaceBack<save::LocalSave>(reloadSourceInfo.name, std::move(saveData));
				break;

			case saveButtonOnline:
			case saveButtonRename:
				gui::SDLWindow::Ref().EmplaceBack<save::OnlineSave>(reloadSourceInfo, std::move(saveData));
				break;

			default:
				break;
			}
		});
		{
			auto diff = gui::Point{ 19, 0 };
			updateButton->Position(saveButton->Position());
			updateButton->Size({ diff.x + 1, saveButton->Size().y });
		}

		likeButton = addButtonHorizSave(false, String::Build(gui::ColorString(gui::Appearance::voteUp), gui::IconString(gui::Icons::voteUp, { 0, 0 }), gui::OffsetString(3), "DEFAULT_LS_GAME_HORIZ_LIKE"_Ls()), 39, false, [this]() {
			ExecVoteStart(1);
		}, "0").get(); // * The tooltip needs to be non-empty for addButtonHorizSave to assign a ToolTipInfo to the button, which UpdateSaveControls requires.
		likeButton->ActiveText(gui::Button::activeTextNone);
		likeButton->DisabledText(gui::Button::disabledTextNone);
		abhlNextXSave -= 2;
		dislikeButton = addButtonHorizSave(false, String::Build(gui::ColorString(gui::Appearance::voteDown), gui::IconString(gui::Icons::voteDown, { 0, -3 })), 15, false, [this]() {
			ExecVoteStart(-1);
		}, "0").get(); // * The tooltip needs to be non-empty for addButtonHorizSave to assign a ToolTipInfo to the button, which UpdateSaveControls requires.
		dislikeButton->ActiveText(gui::Button::activeTextNone);
		dislikeButton->DisabledText(gui::Button::disabledTextNone);

		addButtonHorizSave(true, gui::IconString(gui::Icons::settings, { 0, -1 }), 15, false, []() {
			gui::SDLWindow::Ref().EmplaceBack<options::Options>();
		}, "DEFAULT_LS_GAME_HORIZ_OPTIONS_TIP"_Ls());

		profileButton = groupSave->EmplaceChild<gui::Button>().get();
		profileButton->Click([]() {
			gui::SDLWindow::Ref().EmplaceBack<profile::Profile>(true);
		});
		profileButton->Align(gui::Alignment::horizLeft | gui::Alignment::vertCenter);
		addButtonHorizToolTip(profileButton, "DEFAULT_LS_GAME_HORIZ_PROFILE_TIP"_Ls());
		loginButton = addButtonHorizSave(true, "", 92, true, []() {
			gui::SDLWindow::Ref().EmplaceBack<login::Login>();
		}, "DEFAULT_LS_GAME_HORIZ_LOGIN_TIP"_Ls()).get();
		{
			auto diff = gui::Point{ 19, 0 };
			profileButton->Position(loginButton->Position() + diff);
			profileButton->Size(loginButton->Size() - diff);
		}

		addButtonHorizSave(true, gui::IconString(gui::Icons::empty, { -1, -2 }), 17, false, [this]() {
			ClearSimulation();
		}, "DEFAULT_LS_GAME_HORIZ_CLEAR_TIP"_Ls());

		tagsButton = addButtonHorizSave(false, "", abhrNextXSave - abhlNextXSave - 1, true, [this]() {
			if (reloadSourceInfo.detailLevel)
			{
				auto *editTags = gui::SDLWindow::Ref().EmplaceBack<edittags::EditTags>(reloadSourceInfo).get();
				editTags->Change([this, editTags]() {
					reloadSourceInfo.tags = editTags->Tags();
					UpdateTagControls();
				});
			}
		}, "DEFAULT_LS_GAME_HORIZ_TAGS_TIP"_Ls()).get();
		tagsButton->TextRect(tagsButton->TextRect().Grow({ -4, 0 }));

		auto abvNextY = windowSize.y - 32;
		auto addButtonVert = [this, &abvNextY](String text, gui::Button::ClickCallback cb) {
			auto button = EmplaceChild<gui::Button>();
			button->Size({ 15, 15 });
			button->Position({ windowSize.x - 16, abvNextY });
			button->Text(text);
			button->Click(cb);
			abvNextY -= 16;
			return button;
		};

		currentMenuSection = SC_POWDERS;
		auto *searchButton = addButtonVert(gui::IconString(gui::Icons::search, { 0, -2 }), [this]() {
			OpenToolSearch();
		}).get();
		addToolTipVert(searchButton, "DEFAULT_LS_GAME_VERT_TOOLSEARCH"_Ls());
		
		for (auto i = 0; i < SC_TOTAL; ++i)
		{
			auto &ms = menuSections[i];
			auto button = EmplaceChild<MenuSectionButton>(i);
			button->Size({ 15, 15 });
			button->Position({ windowSize.x - 16, abvNextY - (SC_TOTAL - i - 1) * 16 });
			button->Text(gui::IconString(ms.icon, { 0, -2 }));
			button->Click([this, i]() {
				currentMenuSection = i;
			});
			addToolTipVert(button.get(), language::Template{ "DEFAULT_LS_GAME_SECTION_" + ms.name }());
		}

		auto aboNextY = 1;
		auto addButtonOption = [this, &aboNextY, addToolTipVert](String text, String toolTip, gui::Button::ClickCallback cb) {
			auto button = EmplaceChild<gui::Button>();
			button->Size({ 15, 15 });
			button->Position({ windowSize.x - 16, aboNextY });
			button->Text(text);
			button->Click(cb);
			addToolTipVert(button.get(), toolTip);
			aboNextY += 16;
			return button;
		};

		prettyPowdersButton = addButtonOption("P", "DEFAULT_LS_GAME_OPTION_PRETTYPOWDERS"_Ls(), [this]() {
			TogglePrettyPowders();
		}).get();
		drawGravityButton = addButtonOption("G", "DEFAULT_LS_GAME_OPTION_DRAWGRAVITY"_Ls(), [this]() {
			ToggleDrawGravity();
		}).get();
		drawDecoButton = addButtonOption("D", "DEFAULT_LS_GAME_OPTION_DRAWDECO"_Ls(), [this]() {
			ToggleDrawDeco();
		}).get();
		newtonianGravityButton = addButtonOption("N", "DEFAULT_LS_GAME_OPTION_NEWTONIANGRAVITY"_Ls(), [this]() {
			ToggleNewtonianGravity();
		}).get();
		ambientHeatButton = addButtonOption("A", "DEFAULT_LS_GAME_OPTION_AMBIENTHEAT"_Ls(), [this]() {
			ToggleAmbientHeat();
		}).get();
		addButtonOption("C", "DEFAULT_LS_GAME_VERT_CONSOLE"_Ls(), [this]() {
			OpenConsole();
		});

		auto abhlNextXRender = abhlNextX;
		auto abhrNextXRender = abhrNextX;
		groupRender = EmplaceChild<gui::Component>().get();
		groupRender->Position({ 0, windowSize.y - 16 });
		groupRender->Size({ abhrNextX, 16 });
		auto addButtonHorizRender = [this, addButtonHorizAny, &abhlNextXRender, &abhrNextXRender](bool right, String text, int width, bool alignLeft, gui::Button::ClickCallback cb, String toolTip) {
			return addButtonHorizAny(right, text, width, alignLeft, 0, cb, groupRender->EmplaceChild<gui::Button>(), abhlNextXRender, abhrNextXRender, toolTip);
		};

		auto addRenderModeToggle = [this, addButtonHorizRender](uint32_t flag, String text, String toolTip) {
			auto *button = addButtonHorizRender(false, text, 17, false, nullptr, toolTip).get();
			button->Click([this, button]() {
				button->Stuck(!button->Stuck());
				ApplyRendererSettings();
			});
			button->ActiveText(gui::Button::activeTextDarkened);
			renderModeButtons.push_back(std::make_tuple(flag, button));
		};

		addRenderModeToggle(RENDER_EFFE, gui::ColorString({ 0xFF, 0xA0, 0xFF }) + gui::IconString(gui::Icons::effect     , {  0, -1 }), "DEFAULT_LS_GAME_RENDER_EFFE_TIP"_Ls());
		addRenderModeToggle(RENDER_FIRE, gui::ColorString({ 0xFF, 0x00, 0x00 }) + gui::IconString(gui::Icons::fireBody   , {  0, -1 }) + gui::StepBackString() +
		                                 gui::ColorString({ 0xFF, 0xFF, 0x40 }) + gui::IconString(gui::Icons::fireOutline, {  0, -1 }), "DEFAULT_LS_GAME_RENDER_FIRE_TIP"_Ls());
		addRenderModeToggle(RENDER_GLOW, gui::ColorString({ 0xC8, 0xFF, 0xFF }) + gui::IconString(gui::Icons::glow       , {  0, -1 }), "DEFAULT_LS_GAME_RENDER_GLOW_TIP"_Ls());
		addRenderModeToggle(RENDER_BLUR, gui::ColorString({ 0x64, 0x96, 0xFF }) + gui::IconString(gui::Icons::liquid     , { -1, -2 }), "DEFAULT_LS_GAME_RENDER_BLUR_TIP"_Ls());
		addRenderModeToggle(RENDER_BLOB, gui::ColorString({ 0x37, 0xFF, 0x37 }) + gui::IconString(gui::Icons::blob       , { -1, -2 }), "DEFAULT_LS_GAME_RENDER_BLOB_TIP"_Ls());
		addRenderModeToggle(RENDER_BASC, gui::ColorString({ 0xFF, 0xFF, 0xC8 }) + gui::IconString(gui::Icons::basic      , {  0, -1 }), "DEFAULT_LS_GAME_RENDER_BASC_TIP"_Ls());
		addRenderModeToggle(RENDER_SPRK, gui::ColorString({ 0xFF, 0xFF, 0xA0 }) + gui::IconString(gui::Icons::effect     , {  0, -1 }), "DEFAULT_LS_GAME_RENDER_SPRK_TIP"_Ls());
		abhlNextXRender += 10;

		auto addDisplayModeToggle = [this, addButtonHorizRender](uint32_t flag, String text, String toolTip) {
			auto *button = addButtonHorizRender(false, text, 17, false, nullptr, toolTip).get();
			button->Click([this, button]() {
				button->Stuck(!button->Stuck());
				ApplyRendererSettings();
			});
			button->ActiveText(gui::Button::activeTextDarkened);
			displayModeButtons.push_back(std::make_tuple(flag, button));
		};

		addDisplayModeToggle(DISPLAY_AIRC, gui::ColorString({ 0xFF, 0x37, 0x37 }) + gui::IconString(gui::Icons::altAirBody   , {  0, -1 }) + gui::StepBackString() +
		                                   gui::ColorString({ 0x37, 0xFF, 0x37 }) + gui::IconString(gui::Icons::altAirOutline, {  0, -1 }), "DEFAULT_LS_GAME_DISPLAY_AIRC_TIP"_Ls());
		addDisplayModeToggle(DISPLAY_AIRP, gui::ColorString({ 0xFF, 0xD4, 0x20 }) + gui::IconString(gui::Icons::sensor       , {  0, -1 }), "DEFAULT_LS_GAME_DISPLAY_AIRP_TIP"_Ls());
		addDisplayModeToggle(DISPLAY_AIRV, gui::ColorString({ 0x80, 0xA0, 0xFF }) + gui::IconString(gui::Icons::velocity     , {  0, -1 }), "DEFAULT_LS_GAME_DISPLAY_AIRV_TIP"_Ls());
		addDisplayModeToggle(DISPLAY_AIRH, gui::ColorString({ 0x9F, 0x00, 0xF0 }) + gui::IconString(gui::Icons::heatBody     , {  0, -2 }) + gui::StepBackString() +
		                                   gui::ColorString({ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::heatOutline  , {  0, -2 }), "DEFAULT_LS_GAME_DISPLAY_AIRH_TIP"_Ls());
		addDisplayModeToggle(DISPLAY_WARP, gui::ColorString({ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::warp         , { -1, -2 }), "DEFAULT_LS_GAME_DISPLAY_WARP_TIP"_Ls());
		addDisplayModeToggle(DISPLAY_EFFE, gui::ColorString({ 0xA0, 0xFF, 0xFF }) + gui::IconString(gui::Icons::effect       , {  0, -1 }), "DEFAULT_LS_GAME_DISPLAY_EFFE_TIP"_Ls());
		addDisplayModeToggle(DISPLAY_PERS, gui::ColorString({ 0xD4, 0xD4, 0xD4 }) + gui::IconString(gui::Icons::persistent   , {  0, -1 }), "DEFAULT_LS_GAME_DISPLAY_PERS_TIP"_Ls());
		abhlNextXRender += 10;

		auto addColourModeToggle = [this, addButtonHorizRender](uint32_t flag, String text, String toolTip) {
			auto *button = addButtonHorizRender(false, text, 17, false, nullptr, toolTip).get();
			button->Click([this, button]() {
				button->Stuck(!button->Stuck());
				ApplyRendererSettings();
			});
			button->ActiveText(gui::Button::activeTextDarkened);
			colorModeButtons.push_back(std::make_tuple(flag, button));
		};

		addColourModeToggle(COLOUR_HEAT, gui::ColorString({ 0xFF, 0x00, 0x00 }) + gui::IconString(gui::Icons::heatBody   , { 0, -2 }) + gui::StepBackString() +
		                                 gui::ColorString({ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::heatOutline, { 0, -2 }), "DEFAULT_LS_GAME_COLOUR_HEAT_TIP"_Ls());
		addColourModeToggle(COLOUR_LIFE, gui::ColorString({ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::life       , { 0, -1 }), "DEFAULT_LS_GAME_COLOUR_LIFE_TIP"_Ls());
		addColourModeToggle(COLOUR_GRAD, gui::ColorString({ 0xCD, 0x32, 0xCD }) + gui::IconString(gui::Icons::gradient   , { 0, -1 }), "DEFAULT_LS_GAME_COLOUR_GRAD_TIP");
		addColourModeToggle(COLOUR_BASC, gui::ColorString({ 0xC0, 0xC0, 0xC0 }) + gui::IconString(gui::Icons::basic      , { 0, -1 }), "DEFAULT_LS_GAME_COLOUR_BASC_TIP"_Ls());

		auto addPresetButton = [this, addButtonHorizRender](int preset, String text, String toolTip) {
			auto *button = addButtonHorizRender(true, text, 17, false, [this, preset]() {
				RendererPreset(preset);
			}, toolTip).get();
			button->ActiveText(gui::Button::activeTextDarkened);
			rendererPresetButtons.push_back(std::make_tuple(preset, button));
		};

		addPresetButton(10, gui::ColorString({ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::life         , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_LIFEGRADIENT"_Ls());
		addPresetButton( 0, gui::ColorString({ 0xFF, 0x37, 0x37 }) + gui::IconString(gui::Icons::altAirBody   , {  0, -1 }) + gui::StepBackString() +
		                    gui::ColorString({ 0x37, 0xFF, 0x37 }) + gui::IconString(gui::Icons::altAirOutline, {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_ALTVELOCITY"_Ls());
		addPresetButton( 9, gui::ColorString({ 0xCD, 0x32, 0xCD }) + gui::IconString(gui::Icons::gradient     , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_HEATGRADIENT"_Ls());
		addPresetButton( 8, gui::ColorString({ 0xFF, 0xFF, 0xC8 }) + gui::IconString(gui::Icons::basic        , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_NOTHING"_Ls());
		addPresetButton( 7, gui::ColorString({ 0x64, 0x96, 0xFF }) + gui::IconString(gui::Icons::liquid       , { -1, -2 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_FANCY"_Ls());
		addPresetButton( 6, gui::ColorString({ 0xFF, 0x00, 0x00 }) + gui::IconString(gui::Icons::heatBody     , {  0, -2 }) + gui::StepBackString() +
		                    gui::ColorString({ 0xFF, 0xFF, 0xFF }) + gui::IconString(gui::Icons::heatOutline  , {  0, -2 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_HEAT"_Ls());
		addPresetButton( 5, gui::ColorString({ 0x37, 0xFF, 0x37 }) + gui::IconString(gui::Icons::blob         , { -1, -2 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_BLOB"_Ls());
		addPresetButton( 4, gui::ColorString({ 0xFF, 0x00, 0x00 }) + gui::IconString(gui::Icons::fireBody     , {  0, -1 }) + gui::StepBackString() +
		                    gui::ColorString({ 0xFF, 0xFF, 0x40 }) + gui::IconString(gui::Icons::fireOutline  , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_FIRE"_Ls());
		addPresetButton( 3, gui::ColorString({ 0xD4, 0xD4, 0xD4 }) + gui::IconString(gui::Icons::persistent   , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_PERSISTENT"_Ls());
		addPresetButton( 2, gui::ColorString({ 0xFF, 0xD4, 0x20 }) + gui::IconString(gui::Icons::sensor       , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_PRESSURE"_Ls());
		addPresetButton( 1, gui::ColorString({ 0x80, 0xA0, 0xFF }) + gui::IconString(gui::Icons::velocity     , {  0, -1 }), "DEFAULT_LS_ACTION_DEFAULT_AC_USEPRESET_VELOCITY"_Ls());

		UpdateGroups();
		UpdateOnlineControls();
		UpdateLoginControls();

		actionTipAnim.LowerLimit(0);
		actionTipAnim.UpperLimit(2);
		actionTipAnim.Slope(1);

		introTextAnim.LowerLimit(0);
		introTextAnim.UpperLimit(introTextTimeout);
		introTextAnim.Slope(1);
		introTextAnim.Start(introTextTimeout, false);

		introText = "DEFAULT_LS_GAME_INTRO_TEXT"_Ls(
			"DEFAULT_LS_GAME_INTRO_TITLE"_Ls(), // * TODO-REDO_UI: Fix underline.
			MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION),
#ifdef BETA
			"DEFAULT_LS_GAME_INTRO_ONLINE_BETA"_Ls(),
#else
			"DEFAULT_LS_GAME_INTRO_ONLINE_NORMAL"_Ls(),
#endif
			MTOS(SAVE_VERSION) "." MTOS(MINOR_VERSION) "." MTOS(BUILD_NUM) " " IDENT
#ifdef SNAPSHOT
			" SNAPSHOT " MTOS(SNAPSHOT_ID)
#elif MOD_ID > 0
			" MODVER " MTOS(SNAPSHOT_ID)
#endif
#ifdef X86
			" X86"
#endif
#ifdef X86_SSE
			" X86_SSE"
#endif
#ifdef X86_SSE2
			" X86_SSE2"
#endif
#ifdef X86_SSE3
			" X86_SSE3"
#endif
#ifdef LUACONSOLE
			" LUACONSOLE"
#endif
#ifdef GRAVFFT
			" GRAVFFT"
#endif
#ifdef REALISTIC
			" REALISTIC"
#endif
			" REDO_UI" // * TODO-REDO_UI: Remove.
		);

		pasteTexture = EmplaceChild<gui::Texture>().get();
		pasteTexture->Modulation({ 255, 255, 255, 128 });
		pasteTexture->Blend(gui::Texture::blendNormal);
	}

	void Game::InitTools()
	{
		simulationTools = GetTools();

		// * TODO-REDO_UI: The rest of this function seems to have been designed to be callable again. Not sure why.
		for (auto i = 0; i < int(currentTools.size()); ++i)
		{
			CurrentTool(i, nullptr);
		}
		tools.clear();
		for (auto i = 0; i < int(simulationTools.size()); ++i)
		{
			auto &to = simulationTools[i];
			auto identifier = to.Identifier.FromUtf8();
			tools.insert(std::make_pair(identifier, std::make_unique<tool::SimulationTool>(i, language::Template{ "DEFAULT_LS_TOOLDESC_" + identifier }())));
		}
		for (auto i = 0; i < int(simulation->elements.size()); ++i)
		{
			auto &el = simulation->elements[i];
			if (el.Enabled)
			{
				auto identifier = el.Identifier.FromUtf8();
				tools.insert(std::make_pair(identifier, std::make_unique<tool::ElementTool>(i, language::Template{ "DEFAULT_LS_ELEMDESC_" + identifier }())));
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
			tools.insert(std::make_pair(String("DEFAULT_PT_LIFE_") + gl.identifier, std::make_unique<tool::LifeElementTool>(
				i,
				gl.name,
				language::Template{ "DEFAULT_LS_GOLDESC_" + gl.identifier }(),
				gui::Color{
					PixR(gl.colour),
					PixG(gl.colour),
					PixB(gl.colour),
					255
				}
			)));
		}
		// * TODO-REDO_UI: add custom gol
		CurrentTool(0, tools.at("DEFAULT_PT_DUST").get());
		CurrentTool(1, tools.at("DEFAULT_PT_NONE").get());
	}

	void Game::InitBrushes()
	{
		brushes.resize(BRUSH_NUM);
		brushes[CIRCLE_BRUSH] = std::make_unique<brush::EllipseBrush>();
		brushes[SQUARE_BRUSH] = std::make_unique<brush::RectangleBrush>();
		brushes[TRI_BRUSH] = std::make_unique<brush::TriangleBrush>();
		for (auto &bf : Platform::DirectorySearch(BRUSH_DIR, "", { ".ptb" }))
		{
			auto data = Platform::ReadFile(ByteString(BRUSH_DIR PATH_SEP) + bf);
			if (!data.size())
			{
				std::cerr << "skipping brush " << bf << ": failed to open" << std::endl;
				continue;
			}
			auto size = int(sqrtf(float(data.size())));
			if (size * size != data.size())
			{
				std::cerr << "skipping brush " << bf << ": invalid size" << std::endl;
				continue;
			}
			brushes.push_back(std::make_unique<brush::BitmapBrush>(data, gui::Point{ size, size }));
		}
		cellAlignedBrush = std::make_unique<brush::CellAlignedBrush>();
		BrushRadius({ 4, 4 });
		lastBrush = brushes[0].get();
	}

	void Game::BuildToolPanelIfNeeded()
	{
		if (!shouldBuildToolPanel)
		{
			return;
		}
		shouldBuildToolPanel = false;
		RemoveChild(toolPanel);
		toolPanel = EmplaceChild<ToolPanel>().get();
	}

	void Game::ToolDrawLine(gui::Point from, gui::Point to)
	{
		from = simulationRect.Clamp(from);
		to = simulationRect.Clamp(to);
		auto *tl = currentTools[activeToolIndex];
		if (tl)
		{
			switch (tl->Application())
			{
			case tool::Tool::applicationDraw:
				{
					auto *brush = tl->CellAligned() ? cellAlignedBrush.get() : brushes[currentBrush].get();
					if (lastBrush->CellAligned())
					{
						from = { from.x / CELL * CELL + CELL / 2, from.y / CELL * CELL + CELL / 2 };
						to   = {   to.x / CELL * CELL + CELL / 2,   to.y / CELL * CELL + CELL / 2 };
					}
					brush->Draw(tl, from, to);
				}
				break;

			case tool::Tool::applicationLine:
				tl->Line(from, to);
				break;

			default:
				break;
			}
		}
	}

	void Game::Tick()
	{
		ModalWindow::Tick();
		auto &g = gui::SDLWindow::Ref();

		auto pos = g.MousePosition();
		auto prevSaveControlModState = saveControlModState;
		auto mod1 = bool(g.Mod() & gui::SDLWindow::mod1);
		saveControlModState = mod1;
		if (prevSaveControlModState != saveControlModState)
		{
			UpdateSaveControls();
		}

		auto *tl = currentTools[activeToolIndex];
		switch (activeToolMode)
		{
		case toolModeFree:
			ToolDrawLine(toolOrigin, pos);
			toolOrigin = pos;
			break;

		case toolModeFill:
			if (tl && tl->Application() == tool::Tool::applicationDraw)
			{
				tl->Fill(pos);
			}
			break;

		default:
			break;
		}

		BuildToolPanelIfNeeded();

		simulation->BeforeSim();
		if (!simulation->sys_pause || simulation->framerender)
		{
			simulation->UpdateParticles(0, NPART);
			simulation->AfterSim();
		}

		simulationSample = simulation->GetSample(pos.x, pos.y);

		if (!contextSelect->active && !contextPaste->active)
		{
			actionTipAnim.Down();
		}
		introTextAnim.Down();

		if (pasteRender && pasteRender->complete)
		{
			PasteGetThumbnail();
		}
		if (execVote && execVote->complete)
		{
			ExecVoteFinish();
		}
	}

	void Game::Draw() const
	{
		auto &g = gui::SDLWindow::Ref();
		auto *r = Renderer();

		if (simulation)
		{
			simulationRenderer->Render(*simulation);
			auto *data = simulationRenderer->MainBuffer();
			// * TODO-REDO_UI: Factor texture access like this out to SDLWindow.
			LockedTextureData ltd;
			SDLASSERTZERO(SDL_LockTexture(simulationTexture, NULL, &ltd.pixels, &ltd.pitch));
			for (auto y = 0; y < simulationSize.y; ++y)
			{
				std::copy(data + y * simulationSize.x, data + (y + 1) * simulationSize.x, &ltd({ 0, y }));
			}

			if (contextSelect->active)
			{
				if (selectOriginValid)
				{
					DrawSelectArea(ltd);
				}
			}
			else if (contextPaste->active)
			{
				DrawPasteArea(ltd);
			}
			else
			{
				if (drawHUD)
				{
					if (activeToolMode != toolModeNone || (g.UnderMouse() && simulationRect.Contains(g.MousePosition())))
					{
						DrawBrush(ltd);
					}
				}
			}
			SDL_UnlockTexture(simulationTexture);
			SDL_Rect rect;
			rect.x = 0;
			rect.y = 0;
			rect.w = simulationSize.x;
			rect.h = simulationSize.y;
			auto alpha = introTextAnim.Current();
			if (alpha > 1.f) alpha = 1.f;
			auto simTexAlpha = 255 - int(alpha * 128);
			SDLASSERTZERO(SDL_SetTextureColorMod(simulationTexture, simTexAlpha, simTexAlpha, simTexAlpha));
			SDLASSERTZERO(SDL_RenderCopy(r, simulationTexture, NULL, &rect));
		}

		if (contextSelect->active)
		{
			DrawSelectBlindfold();
		}
		else if (contextPaste->active)
		{
			DrawPasteTexture();
		}
		else
		{
			if (drawHUD)
			{
				DrawStatistics();
				DrawSampleInfo();
				DrawLogEntries();
				DrawIntroText();
			}
		}

		DrawActionTip();

		ModalWindow::Draw();
	}
	
	bool Game::MouseMove(gui::Point current, gui::Point delta)
	{
		auto pos = gui::SDLWindow::Ref().MousePosition();
		auto *tl = currentTools[activeToolIndex];
		switch (activeToolMode)
		{
		case toolModeFree:
			ToolDrawLine(toolOrigin, pos);
			toolOrigin = pos;
			break;

		case toolModeFill:
			if (tl && tl->Application() == tool::Tool::applicationDraw)
			{
				tl->Fill(pos);
			}
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
		// if (HandlePress(actionSourceSym, sym, g.Mod(), 1)) // * See commented-out definition of actionSourceSym.
		// {
		// 	return true;
		// }
		return false;
	}
	
	bool Game::KeyRelease(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt)
	{
		if (HandleRelease(actionSourceScan, scan))
		{
			return true;
		}
		// if (HandleRelease(actionSourceSym, sym)) // * See commented-out definition of actionSourceSym.
		// {
		// 	return true;
		// }
		return false;
	}

	void Game::DrawLogEntries() const
	{
		auto &g = gui::SDLWindow::Ref();
		auto start = bottomLeftTextPos;
		auto now = g.Ticks();
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
			g.DrawText(start, entry.message, { 255, 255, 255, alpha }, gui::SDLWindow::drawTextShaded);
			start.y -= 14;
		}
	}

	void Game::DrawStatistics() const
	{
		auto alpha = introTextAnim.Current();
		if (alpha > 1.f) alpha = 1.f;
		auto alphaInt = 255 - int(alpha * 255);
		if (alphaInt)
		{
			auto &g = gui::SDLWindow::Ref();
			auto fpsStr = String::Build(Format::Precision(g.GetFPS(), 2));
			String fpsInfo;
			if (debugHUD)
			{
				fpsInfo = "DEFAULT_LS_GAME_HUD_DEBUG"_Ls(fpsStr, simulationSample.NumParts);
			}
			else
			{
				fpsInfo = "DEFAULT_LS_GAME_HUD_NORMAL"_Ls(fpsStr);
			}
			g.DrawText({ 12, 10 }, fpsInfo, { 32, 216, 255, alphaInt }, gui::SDLWindow::drawTextShaded);
		}
	}

	void Game::DrawSampleInfo() const
	{
		// We assume that simulationSample doesn't contain nonsense data, so we don't need to
		// do checks here like simulation->IsElement(part.type); checking if part.type is non-zero is enough.
		auto &part = simulationSample.particle;
		std::vector<String> lines;

		// TODO-REDO_UI: Make this configurable.
		auto formatTemperature = [](StringBuilder &sb, float temp) {
			sb << temp - 273.15f << "C";
		};

		{
			StringBuilder sampleInfo;
			sampleInfo << Format::Precision(2);
			String displayType;
			if (part.type)
			{
				displayType = simulation->ElementResolveDeep(part.type, part.ctype, part.tmp4);
			}
			else
			{
				displayType = "DEFAULT_LS_GAME_HUD_EMPTY"_Ls();
			}
			if (displayType.size())
			{
				// TODO-REDO_UI: Might not work in all locales, idk.
				displayType[0] = std::toupper(displayType[0]);
			}
			if (part.type)
			{
				auto hudProp = simulation->elements[part.type].HudProperties;
				auto ctypeBits = hudProp & HUD_CTYPE_BITS;
				auto tmpBits = hudProp & HUD_TMP_BITS;
				auto tmp2Bits = hudProp & HUD_TMP2_BITS;
				if (debugHUD && ctypeBits == HUD_CTYPE_WAVELENGTH && (part.ctype & 0x3FFFFFFF))
				{
					using Ch = String::value_type;
					for (auto i = 29; i >= 0; --i)
					{
						auto color = gui::Color{ 0x40, 0x40, 0x40 };
						if (part.ctype & (1 << i))
						{
							Element_FILT_renderWavelengths(color.r, color.g, color.b, (i > 2) ? (0x1F << (i - 2)) : (0x1F >> (2 - i)));
							if (color.r > 255) color.r = 255;
							if (color.g > 255) color.g = 255;
							if (color.b > 255) color.b = 255;
						}
						sampleInfo << gui::ColorString(color) << gui::AlignString({ 0, -4 }) << Ch(0xE06A);
					}
					sampleInfo << gui::OffsetString(-27) << gui::ResetString();
				}
				sampleInfo << displayType;
				if (debugHUD)
				{
					if (tmpBits == HUD_TMP_FILTMODE)
					{
						String filtMode = "DEFAULT_LS_GAME_HUD_FILT_MODE_UNKNOWN"_Ls();
						if (part.tmp >= 0 && part.tmp <= 11)
						{
							filtMode = language::Template{ String::Build("DEFAULT_LS_GAME_HUD_FILT_MODE_", part.tmp) }();
						}
						sampleInfo << " (" << filtMode << ")";
					}
					if (ctypeBits == HUD_CTYPE_TYPEVINHIGH && simulation->IsElement(part.ctype))
					{
						sampleInfo << " (" << simulation->ElementResolve(TYP(part.ctype), ID(part.ctype)) << ")";
					}
					else if (ctypeBits == HUD_CTYPE_TYPEVINTMP && simulation->IsElement(part.ctype))
					{
						sampleInfo << " (" << simulation->ElementResolve(part.ctype, part.tmp) << ")";
					}
					else if (ctypeBits == HUD_CTYPE_TYPEORNUM && simulation->IsElement(part.ctype))
					{
						sampleInfo << " (" << simulation->ElementResolve(part.ctype, 0) << ")";
					}
					else if ((ctypeBits == HUD_CTYPE_TYPEORNUM || ctypeBits == HUD_CTYPE_NUM) && part.ctype)
					{
						sampleInfo << " (" << part.ctype << ")";
					}
				}
				sampleInfo << ", Temp: ";
				formatTemperature(sampleInfo, part.temp);
				if (debugHUD)
				{
					sampleInfo << ", Life: " << part.life;
					if (tmpBits != HUD_TMP_NOTHING && ctypeBits != HUD_CTYPE_WAVELENGTH)
					{
						sampleInfo << ", Tmp: ";
						if (tmpBits == HUD_TMP_NUM)
						{
							sampleInfo << part.tmp;
						}
						else if (tmpBits == HUD_TMP_TYPEORNUM && simulation->IsElement(TYP(part.tmp)))
						{
							sampleInfo << simulation->ElementResolve(TYP(part.tmp), ID(part.tmp));
						}
						else if (tmpBits == HUD_TMP_TYPEORNUM || tmpBits == HUD_TMP_NUM)
						{
							sampleInfo << part.tmp;
						}
					}
					if (tmp2Bits == HUD_TMP2_NUM)
					{
						sampleInfo << ", Tmp2: " << part.tmp2;
					}
				}
			}
			else if (simulationSample.WallType)
			{
				sampleInfo << simulation->wtypes[simulationSample.WallType].name;
			}
			else
			{
				sampleInfo << displayType;
			}
			if (simulationSample.isMouseInSim)
			{
				sampleInfo << ", Pressure: " << simulationSample.AirPressure;
			}
			lines.push_back(sampleInfo.Build());
		}

		if (debugHUD && simulationSample.isMouseInSim) // Non particle stuff like pressure gravity etc.
		{
			StringBuilder debugSampleInfo;
			debugSampleInfo << Format::Precision(2);
			if (part.type)
			{
				debugSampleInfo << "#" << simulationSample.ParticleID << ", ";
			}
			debugSampleInfo << "X: " << simulationSample.PositionX << ", Y: " << simulationSample.PositionY;
			if (simulationSample.Gravity)
			{
				debugSampleInfo << ", GX: " << simulationSample.GravityVelocityX << ", GY: " << simulationSample.GravityVelocityY;
			}
			if (ambientHeat)
			{
				debugSampleInfo << ", AHeat: ";
				formatTemperature(debugSampleInfo, simulationSample.AirTemperature);
			}
			lines.push_back(debugSampleInfo.Build());
		}

		// TODO-REDO_UI: Custom hud in Lua. We'd basically get lines from a callback.
		auto posY = 10;
		auto &g = gui::SDLWindow::Ref();
		for (auto &line : lines)
		{
			auto size = g.TextSize(line);
			gui::Rect rect{ { simulationSize.x - 11 - size.x, posY }, size };
			g.DrawText(rect.pos, line, { 255, 255, 255, 255 }, gui::SDLWindow::drawTextShaded);
			posY += FONT_H + 3;
		}

		// TODO-REDO_UI: Make the hud move under zoom window if it overlaps, draw filt spectrum.
	}

	void Game::DrawHighlight(const LockedTextureData ltd, gui::Point point) const
	{
		if (simulationRect.Contains(point))
		{
			auto &pix = ltd(point);
			pix = gui::Appearance::darkColor({ PixB(pix), PixG(pix), PixR(pix) }) ? 0xC0C0C0U : 0x404040U;
		}
	}

	void Game::DrawSelectBlindfold() const
	{
		auto &g = gui::SDLWindow::Ref();
		auto pos = simulationRect.Clamp(g.MousePosition());
		auto blindfold = gui::Color{ 0, 0, 0, 100 };

		if (selectOriginValid)
		{
			auto rect = gui::Rect::FromCorners(selectOrigin, pos);
			g.DrawRect({ { 0,                        0 }, { simulationSize.x,                                  rect.pos.y } }, blindfold);
			g.DrawRect({ { 0, rect.size.y + rect.pos.y }, { simulationSize.x, simulationSize.y - rect.size.y - rect.pos.y } }, blindfold);
			g.DrawRect({ {                        0, rect.pos.y }, {                                  rect.pos.x, rect.size.y } }, blindfold);
			g.DrawRect({ { rect.size.x + rect.pos.x, rect.pos.y }, { simulationSize.x - rect.size.x - rect.pos.x, rect.size.y } }, blindfold);
		}
		else
		{
			g.DrawRect(simulationRect, blindfold);
		}
	}

	void Game::DrawActionTip() const
	{
		auto alpha = actionTipAnim.Current();
		if (alpha > 1.f) alpha = 1.f;
		auto alphaInt = int(alpha * 255);
		if (alphaInt)
		{
			gui::SDLWindow::Ref().DrawText(bottomLeftTextPos, actionTipText, { 239, 239, 20, alphaInt }, gui::SDLWindow::drawTextShaded);
		}
	}

	void Game::ToggleIntroText()
	{
		if (introTextState == introTextForcedOff || (introTextState == introTextInitial && introTextAnim.Current() < 0.5f))
		{
			introTextState = introTextForcedOn;
		}
		else
		{
			introTextState = introTextForcedOff;
		}
	}

	void Game::DrawIntroText() const
	{
		auto alpha = introTextAnim.Current();
		if (introTextState == introTextForcedOn)
		{
			alpha = 1.f;
		}
		if (introTextState == introTextForcedOff)
		{
			alpha = 0.f;
		}
		if (alpha > 1.f) alpha = 1.f;
		auto alphaInt = int(alpha * 255);
		if (alphaInt)
		{
			gui::SDLWindow::Ref().DrawText({ 16, 18 }, introText, { 255, 255, 255, alphaInt });
		}
	}

	void Game::DrawHighlightRect(const LockedTextureData ltd, gui::Rect rect, gui::Point origin) const
	{
		auto dottedLine = [this, origin, ltd](gui::Point from, gui::Point to, gui::Point incr) {
			while (from.x <= to.x && from.y <= to.y)
			{
				if (((origin.x + origin.y) & 1) == ((from.x + from.y) & 1))
				{
					DrawHighlight(ltd, from);
				}
				from += incr;
			}
		};

		auto oneX = gui::Point{ 1, 0 };
		auto oneY = gui::Point{ 0, 1 };
		dottedLine(rect.TopLeft(), rect.TopRight(), oneX);
		if (rect.size.y > 1)
		{
			dottedLine(rect.BottomLeft(), rect.BottomRight(), oneX);
		}
		dottedLine(rect.TopLeft() + oneY, rect.BottomLeft() - oneY, oneY);
		if (rect.size.x > 1)
		{
			dottedLine(rect.TopRight() + oneY, rect.BottomRight() - oneY, oneY);
		}
	}

	void Game::DrawSelectArea(const LockedTextureData ltd) const
	{
		DrawHighlightRect(ltd, gui::Rect::FromCorners(selectOrigin, simulationRect.Clamp(gui::SDLWindow::Ref().MousePosition())), selectOrigin);
	}

	void Game::DrawBrush(const LockedTextureData ltd) const
	{
		auto &g = gui::SDLWindow::Ref();
		auto pos = simulationRect.Clamp(g.MousePosition());

		// * TODO-REDO_UI: Do this with a texture instead or something.
		if ((g.Mod() & gui::SDLWindow::mod0) && (g.Mod() & gui::SDLWindow::mod1))
		{
			DrawHighlight(ltd, { pos.x, pos.y });
			for (auto i = 1; i <= 5; ++i)
			{
				DrawHighlight(ltd, { pos.x, pos.y + i });
				DrawHighlight(ltd, { pos.x, pos.y - i });
				DrawHighlight(ltd, { pos.x + i, pos.y });
				DrawHighlight(ltd, { pos.x - i, pos.y });
			}
		}
		else
		{
			auto midpoint = pos;
			if (lastBrush->CellAligned())
			{
				midpoint = { pos.x / CELL * CELL + CELL / 2, pos.y / CELL * CELL + CELL / 2 };
			}
			for (auto relative : lastBrush->Outline())
			{
				auto absolute = relative + midpoint;
				if (simulationRect.Contains(absolute))
				{
					DrawHighlight(ltd, absolute);
				}
			}
		}

		switch (activeToolMode)
		{
		case toolModeLine:
			BrushLine(toolOrigin.x, toolOrigin.y, pos.x, pos.y, [this, ltd](int x, int y) {
				DrawHighlight(ltd, { x, y });
			});
			break;

		case toolModeRect:
			{
				auto rect = gui::Rect::FromCorners(toolOrigin, pos);
				if (rect.size.x == 1)
				{
					for (auto y = rect.pos.y; y < rect.pos.y + rect.size.y; ++y)
					{
						DrawHighlight(ltd, { rect.pos.x, y });
					}
				}
				else if (rect.size.y == 1)
				{
					for (auto x = rect.pos.x; x < rect.pos.x + rect.size.x; ++x)
					{
						DrawHighlight(ltd, { x, rect.pos.y });
					}
				}
				else
				{
					for (auto x = rect.pos.x; x < rect.pos.x + rect.size.x; ++x)
					{
						DrawHighlight(ltd, { x, rect.pos.y });
						DrawHighlight(ltd, { x, rect.pos.y + rect.size.y - 1 });
					}
					for (auto y = rect.pos.y + 1; y < rect.pos.y + rect.size.y - 1; ++y)
					{
						DrawHighlight(ltd, { rect.pos.x, y });
						DrawHighlight(ltd, { rect.pos.x + rect.size.x - 1, y });
					}
				}
			}
			break;

		default:
			break;
		}
	}
	
	void Game::RendererUp()
	{
		// * TODO-REDO_UI: This is getting slightly out of hand, should come up with a smarter solution.
		simulationTexture = SDLASSERTPTR(SDL_CreateTexture(Renderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, simulationSize.x, simulationSize.y));
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
		ambientAirTemp = restrict_flt(newAmbientAirTemp, float(MIN_TEMP), float(MAX_TEMP));
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

	void Game::IncludePressure(bool newIncludePressure)
	{
		includePressure = newIncludePressure;
	}

	void Game::Paused(bool newPaused)
	{
		paused = newPaused;
		pauseButton->Stuck(paused);
		simulation->sys_pause = paused ? 1 : 0;
	}

	void Game::Log(String message)
	{
		// * TODO-REDO_UI: Log to stdout?
		if (log.size() == maxLogEntries)
		{
			log.pop_back();
		}
		log.push_front(LogEntry{ message, gui::SDLWindow::Ref().Ticks() });
	}

	Game::OpenButtonAction Game::GetOpenButtonAction() const
	{
		if (saveControlModState)
		{
			return openButtonLocal;
		}
		return openButtonOnline;
	}

	Game::ReloadButtonAction Game::GetReloadButtonAction() const
	{
		auto canReload = bool(reloadSource);
		auto onlineSave = canReload && reloadSourceInfo.detailLevel;
		if (!reloadSource)
		{
			return reloadButtonNone;
		}
		if (onlineSave)
		{
			if (saveControlModState)
			{
				return reloadButtonPreview;
			}
			return reloadButtonOnline;
		}
		if (saveControlModState)
		{
			return reloadButtonNone;
		}
		return reloadButtonLocal;
	}

	Game::UpdateButtonAction Game::GetUpdateButtonAction() const
	{
		auto canReload = bool(reloadSource);
		auto onlineSave = canReload && reloadSourceInfo.detailLevel;
		auto &b = backend::Backend::Ref();
		auto canManage = onlineSave && b.User().detailLevel && b.User().name == reloadSourceInfo.username;
		if (canReload && !onlineSave && saveControlModState)
		{
			return updateButtonLocal;
		}
		if (!saveControlModState && canManage)
		{
			return updateButtonOnline;
		}
		return updateButtonNone;
	}

	Game::SaveButtonAction Game::GetSaveButtonAction() const
	{
		auto canReload = bool(reloadSource);
		auto onlineSave = canReload && reloadSourceInfo.detailLevel;
		auto &b = backend::Backend::Ref();
		if (saveControlModState)
		{
			return saveButtonLocal;
		}
		if (!b.User().detailLevel)
		{
			return saveButtonForceLocal;
		}
		if (onlineSave)
		{
			return saveButtonRename;
		}
		return saveButtonOnline;
	}

	struct SaveControlsButtonState
	{
		bool enabled;
		bool stuck;
		String toolTipTemplate;
	};
	template<size_t N> // * TODO-REDO_UI: Do this inside UpdateSaveControls with a template lambda once we get C++20.
	void MirrorSaveControlsButtonState(gui::Button *button, const std::array<SaveControlsButtonState, N> &states, int index)
	{
		auto &state = states[index];
		button->Stuck(state.stuck);
		button->Enabled(state.enabled);
		button->ToolTip()->text.clear();
		if (state.toolTipTemplate.size())
		{
			button->ToolTip()->text = language::Template{ state.toolTipTemplate }();
		}
	}

	void Game::UpdateSaveControls()
	{
		static const std::array<SaveControlsButtonState, int(openButtonMax)> openButtonStates = {{
			{  true, false, "DEFAULT_LS_GAME_HORIZ_OPEN_ONLINE_TIP"    },
			{  true,  true, "DEFAULT_LS_GAME_HORIZ_OPEN_LOCAL_TIP"     },
		}};
		MirrorSaveControlsButtonState(openButton, openButtonStates, int(GetOpenButtonAction()));
		static const std::array<SaveControlsButtonState, int(reloadButtonMax)> reloadButtonStates = {{
			{ false, false, "DEFAULT_LS_GAME_HORIZ_RELOAD_LOCAL_TIP"   }, // * Placeholder tooltip, button is disabled.
			{  true, false, "DEFAULT_LS_GAME_HORIZ_RELOAD_LOCAL_TIP"   },
			{  true, false, "DEFAULT_LS_GAME_HORIZ_RELOAD_ONLINE_TIP"  },
			{  true,  true, "DEFAULT_LS_GAME_HORIZ_RELOAD_PREVIEW_TIP" },
		}};
		MirrorSaveControlsButtonState(reloadButton, reloadButtonStates, int(GetReloadButtonAction()));
		static const std::array<SaveControlsButtonState, int(updateButtonMax)> updateButtonStates = {{
			{ false, false, ""                                         }, // * No placeholder needed, button is invisible.
			{  true,  true, "DEFAULT_LS_GAME_HORIZ_UPDATE_LOCAL_TIP"   },
			{  true, false, "DEFAULT_LS_GAME_HORIZ_UPDATE_ONLINE_TIP"  },
		}};
		MirrorSaveControlsButtonState(updateButton, updateButtonStates, int(GetUpdateButtonAction()));
		static const std::array<SaveControlsButtonState, int(saveButtonMax)> saveButtonStates = {{
			{  true,  true, "DEFAULT_LS_GAME_HORIZ_SAVE_LOCAL_TIP"     },
			{  true, false, "DEFAULT_LS_GAME_HORIZ_SAVE_NOAUTH_TIP"    },
			{  true, false, "DEFAULT_LS_GAME_HORIZ_SAVE_ONLINE_TIP"    },
			{  true, false, "DEFAULT_LS_GAME_HORIZ_SAVE_RENAME_TIP"    },
		}};
		MirrorSaveControlsButtonState(saveButton, saveButtonStates, int(GetSaveButtonAction()));

		auto saveRight = saveButton->Position().x + saveButton->Size().x;
		StringBuilder nameBuilder;
		if (updateButton->Enabled())
		{
			updateButton->Visible(true);
			saveButton->Position({ updateButton->Position().x + updateButton->Size().x - 1, updateButton->Position().y });
			saveButton->Size({ saveRight - saveButton->Position().x, updateButton->Size().y });
			updateButton->Text(gui::IconString(gui::Icons::save, { 0, -2 }));
			nameBuilder << gui::OffsetString(1);
		}
		else
		{
			updateButton->Visible(false);
			saveButton->Position({ updateButton->Position().x, updateButton->Position().y });
			saveButton->Size({ saveRight - saveButton->Position().x, updateButton->Size().y });
			nameBuilder << gui::IconString(gui::Icons::save, { 2, -2 }) << gui::OffsetString(7);
		}
		if (reloadSource)
		{
			nameBuilder << reloadSourceInfo.name;
		}
		else
		{
			nameBuilder << gui::CommonColorString(gui::commonLightGray) << "[" << "DEFAULT_LS_GAME_HORIZ_SAVE_UNTITLED"_Ls() << "]";
		}
		saveButton->Text(nameBuilder.Build());
		saveButton->TextRect(saveButton->TextRect().Grow({ -4, 0 }));
	}

	void Game::UpdateTagControls()
	{
		auto canReload = bool(reloadSource);
		auto onlineSave = canReload && reloadSourceInfo.detailLevel;
		tagsButton->Enabled(onlineSave);
		StringBuilder textBuilder;
		textBuilder << gui::IconString(gui::Icons::tag, { 2, -2 }) << gui::OffsetString(7);
		if (onlineSave && !reloadSourceInfo.tags.empty())
		{
			auto first = true;
			for (auto &tag : reloadSourceInfo.tags)
			{
				if (first)
				{
					first = false;
				}
				else
				{
					textBuilder << " ";
				}
				textBuilder << tag;
			}
		}
		else
		{
			textBuilder << gui::CommonColorString(gui::commonLightGray) << "[" << "DEFAULT_LS_GAME_HORIZ_TAGS_EMPTY"_Ls() << "]";
		}
		tagsButton->Text(textBuilder.Build());
	}

	void Game::UpdateVoteControls()
	{
		auto canReload = bool(reloadSource);
		auto onlineSave = canReload && reloadSourceInfo.detailLevel;
		auto &b = backend::Backend::Ref();
		likeButton->Stuck(false);
		likeButton->ToolTip()->text = "";
		likeButton->BodyColor({ 0, 0, 0, 0 });
		likeButton->Enabled(false);
		dislikeButton->Stuck(false);
		dislikeButton->ToolTip()->text = "";
		dislikeButton->BodyColor({ 0, 0, 0, 0 });
		dislikeButton->Enabled(false);
		if (onlineSave)
		{
			likeButton->ToolTip()->text = "DEFAULT_LS_GAME_HORIZ_LIKE_AUTH"_Ls();
			dislikeButton->ToolTip()->text = "DEFAULT_LS_GAME_HORIZ_DISLIKE_AUTH"_Ls();
			if (b.User().detailLevel)
			{
				likeButton->ToolTip()->text = "DEFAULT_LS_GAME_HORIZ_LIKE_TIP"_Ls();
				dislikeButton->ToolTip()->text = "DEFAULT_LS_GAME_HORIZ_DISLIKE_TIP"_Ls();
				auto liked = onlineSave && reloadSourceInfo.scoreMine == 1;
				auto disliked = onlineSave && reloadSourceInfo.scoreMine == -1;
				auto canManage = onlineSave && b.User().name == reloadSourceInfo.username;
				if (liked)
				{
					likeButton->Stuck(true);
					likeButton->ToolTip()->text = canManage ? "DEFAULT_LS_GAME_HORIZ_LIKE_MINE"_Ls() : "DEFAULT_LS_GAME_HORIZ_LIKE_DONE"_Ls();
					likeButton->BodyColor(gui::Appearance::voteUpBackground);
				}
				if (disliked)
				{
					dislikeButton->Stuck(true);
					dislikeButton->ToolTip()->text = "DEFAULT_LS_GAME_HORIZ_DISLIKE_DONE"_Ls();
					dislikeButton->BodyColor(gui::Appearance::voteDownBackground);
				}
				if (!execVote && onlineSave && reloadSourceInfo.scoreMine == 0)
				{
					likeButton->Enabled(true);
					dislikeButton->Enabled(true);
				}
			}
		}
	}

	void Game::ExecVoteStart(int direction)
	{
		auto canReload = bool(reloadSource);
		auto onlineSave = canReload && reloadSourceInfo.detailLevel;
		if (execVote || !onlineSave)
		{
			return;
		}
		execVoteDirection = direction;
		execVote = std::make_shared<backend::ExecVote>(reloadSourceInfo.idv, direction);
		execVote->Start(execVote);
		UpdateVoteControls();
	}

	void Game::ExecVoteFinish()
	{
		if (execVote->status)
		{
			// * No need to do an info request.
			reloadSourceInfo.scoreMine += execVoteDirection;
			if (execVoteDirection == 1)
			{
				reloadSourceInfo.scoreUp += 1;
			}
			else
			{
				reloadSourceInfo.scoreDown += 1;
			}
		}
		else
		{
			// * TODO-REDO_UI: Display error.
		}
		execVote.reset();
		UpdateVoteControls();
	}

	void Game::UpdateOnlineControls()
	{
		UpdateVoteControls();
		UpdateTagControls();
		UpdateSaveControls();
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
		for (auto flagButton : renderModeButtons)
		{
			renderMode |= std::get<1>(flagButton)->Stuck() ? std::get<0>(flagButton) : 0;
		}
		for (auto flagButton : displayModeButtons)
		{
			displayMode |= std::get<1>(flagButton)->Stuck() ? std::get<0>(flagButton) : 0;
		}
		for (auto flagButton : colorModeButtons)
		{
			colorMode |= std::get<1>(flagButton)->Stuck() ? std::get<0>(flagButton) : 0;
		}
		simulationRenderer->RenderMode(renderMode);
		simulationRenderer->DisplayMode(displayMode);
		simulationRenderer->ColorMode(colorMode);
	}

	void Game::RenderMode(uint32_t newRenderMode)
	{
		simulationRenderer->RenderMode(newRenderMode);
		UpdateRendererSettings();
	}

	uint32_t Game::RenderMode() const
	{
		return simulationRenderer->RenderMode();
	}

	void Game::DisplayMode(uint32_t newDisplayMode)
	{
		simulationRenderer->DisplayMode(newDisplayMode);
		UpdateRendererSettings();
	}

	uint32_t Game::DisplayMode() const
	{
		return simulationRenderer->DisplayMode();
	}

	void Game::ColorMode(uint32_t newColorMode)
	{
		simulationRenderer->ColorMode(newColorMode);
		UpdateRendererSettings();
	}

	uint32_t Game::ColorMode() const
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
		for (auto flagButton : renderModeButtons)
		{
			auto flags = std::get<0>(flagButton);
			std::get<1>(flagButton)->Stuck((flags & renderMode) == flags);
		}
		for (auto flagButton : displayModeButtons)
		{
			auto flags = std::get<0>(flagButton);
			std::get<1>(flagButton)->Stuck((flags & displayMode) == flags);
		}
		for (auto flagButton : colorModeButtons)
		{
			auto flags = std::get<0>(flagButton);
			std::get<1>(flagButton)->Stuck((flags & colorMode) == flags);
		}
		for (auto presetButton : rendererPresetButtons)
		{
			std::get<1>(presetButton)->Stuck(std::get<0>(presetButton) == rendererPreset);
		}
	}

	void Game::UpdateLoginControls()
	{
		auto &b = backend::Backend::Ref();
		if (b.User().detailLevel)
		{
			loginButton->Text(gui::IconString(gui::Icons::passOutline, { 2, -1 }));
			loginButton->Size({ profileButton->Position().x - loginButton->Position().x + 1, profileButton->Size().y });
			profileButton->Text(gui::OffsetString(1) + b.User().name);
			profileButton->Visible(true);
		}
		else
		{
			loginButton->Text(String::Build(gui::IconString(gui::Icons::passOutline, { 2, -1 }), gui::OffsetString(8), gui::CommonColorString(gui::commonLightGray), String::Build("[", "DEFAULT_LS_GAME_HORIZ_LOGIN"_Ls(), "]")));
			loginButton->Size({ profileButton->Size().x + profileButton->Position().x - loginButton->Position().x, profileButton->Size().y });
			profileButton->Visible(false);
		}
	}

	void Game::RendererPreset(int newRendererPreset)
	{
		if (newRendererPreset < 0 || newRendererPreset >= int(rendererPresets.size()))
		{
			return;
		}
		rendererPreset = newRendererPreset;
		simulationRenderer->RenderMode(rendererPresets[rendererPreset].renderMode);
		simulationRenderer->DisplayMode(rendererPresets[rendererPreset].displayMode);
		simulationRenderer->ColorMode(rendererPresets[rendererPreset].colorMode);
		UpdateRendererSettings();
	}

	bool Game::HandlePress(ActionSource source, int code, int mod, int multiplier)
	{
		if (introTextAnim.Current() > 1)
		{
			introTextAnim.Start(1, false);
		}
		RehashAvailableActionsIfNeeded();
		actionMultiplier = multiplier;
		auto keyWithoutMod = actionKeyWithoutMod(source, code);
		auto keyWithMod = actionKeyWithMod(source, code, mod);
		auto keyToActionIt = availableActions.find(keyWithMod);
		if (keyToActionIt != availableActions.end())
		{
			auto *action = keyToActionIt->second;
			if (std::find_if(activeActions.begin(), activeActions.end(), [action](std::pair<uint64_t, Action *> &active) {
				return active.second == action;
			}) == activeActions.end())
			{
				activeActions.push_back(std::make_pair(keyWithoutMod, action));
				if (action->begin)
				{
					// * TODO-REDO_UI: Prevent adding and removing actions during this callback.
					action->begin();
				}
				return true;
			}
		}
		return false;
	}

	bool Game::HandleRelease(ActionSource source, int code)
	{
		RehashAvailableActionsIfNeeded();
		auto keyWithoutMod = actionKeyWithoutMod(source, code);
		auto endedSomething = false;
		while (true)
		{
			auto toRemove = std::find_if(activeActions.begin(), activeActions.end(), [keyWithoutMod](std::pair<uint64_t, Action *> &active) {
				return active.first == keyWithoutMod;
			});
			if (toRemove == activeActions.end())
			{
				break;
			}
			endedSomething = true;
			auto last = activeActions.end();
			--last;
			auto *action = toRemove->second;
			*toRemove = *last;
			activeActions.erase(last, activeActions.end());
			if (action->end)
			{
				// * TODO-REDO_UI: Prevent adding and removing actions during this callback.
				action->end(false);
			}
		}
		return endedSomething;
	}

	bool Game::EnterContext(ActionContext *context)
	{
		// * TODO-REDO_UI: Return reason for failure.
		if (inContextCallback)
		{
			return false;
		}
		EndAllActions();
		std::deque<ActionContext *> path;
		auto *up = context;
		while (up)
		{
			path.push_front(up);
			up = up->parent;
		}
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
		if (startFrom < activeContexts.size())
		{
			LeaveContext(activeContexts[startFrom]);
		}
		for (auto i = startFrom; i < path.size(); ++i)
		{
			if (path[i]->canEnter && !path[i]->canEnter())
			{
				return false;
			}
		}
		for (auto i = startFrom; i < path.size(); ++i)
		{
			if (activeContexts.back()->yield)
			{
				inContextCallback = true;
				activeContexts.back()->yield();
				inContextCallback = false;
			}
			activeContexts.push_back(path[i]);
			activeContexts.back()->active = true;
			if (activeContexts.back()->enter)
			{
				inContextCallback = true;
				activeContexts.back()->enter();
				inContextCallback = false;
			}
		}
		shouldRehashAvailableActions = true;
		return true;
	}

	bool Game::LeaveContext(ActionContext *context)
	{
		// * TODO-REDO_UI: Return reason for failure.
		if (inContextCallback)
		{
			return false;
		}
		auto pathSize = 0U;
		auto *up = context;
		while (up)
		{
			// * TODO-REDO_UI: Eww.
			pathSize += 1U;
			up = up->parent;
		}
		if (activeContexts.size() >= pathSize && activeContexts[pathSize - 1] == context)
		{
			EndAllActions();
			while (activeContexts.size() >= pathSize)
			{
				if (activeContexts.back()->leave)
				{
					inContextCallback = true;
					activeContexts.back()->leave();
					inContextCallback = false;
				}
				activeContexts.back()->active = false;
				activeContexts.pop_back();
				if (activeContexts.back()->resume)
				{
					inContextCallback = true;
					activeContexts.back()->resume();
					inContextCallback = false;
				}
			}
			shouldRehashAvailableActions = true;
			return true;
		}
		return false;
	}

	void Game::FocusLose()
	{
		EndAllActions();
	}

	void Game::EndAllActions()
	{
		decltype(activeActions) prevActiveActions;
		std::swap(prevActiveActions, activeActions);
		for (auto &keyToActionPtr : prevActiveActions)
		{
			if (keyToActionPtr.second->end)
			{
				// * TODO-REDO_UI: Prevent adding and removing actions during this callback.
				keyToActionPtr.second->end(true);
			}
		}
	}

	void Game::RehashAvailableActionsIfNeeded()
	{
		if (!shouldRehashAvailableActions)
		{
			return;
		}
		shouldRehashAvailableActions = false;
		availableActions.clear();
		// * TODO-REDO_UI: remove
		// std::cerr << "========= ACTIVE CONTEXTS BEGIN =========" << std::endl;
		// for (auto *context : activeContexts)
		// {
		// 	std::cerr << context->name.ToUtf8() << std::endl;
		// }
		// std::cerr << "========== ACTIVE CONTEXTS END ==========" << std::endl;
		for (auto *context : activeContexts)
		{
			for (auto &keyToAction : context->actions)
			{
				auto it = availableActions.find(keyToAction.first);
				if (it != availableActions.end())
				{
					if (it->second->dof)
					{
						// * There might be a computationally cheaper way to do this, but there are only
						//   8 modifier bits i.e. 256 iterations, and there aren't too many non-zero dof
						//   actions, so this is okay.
						auto dof = uint64_t(it->second->dof) & dofMask;
						auto mod = (keyToAction.first & actionKeyModMask) >> actionKeyModShift;
						auto rest = keyToAction.first & ~actionKeyModMask;
						for (uint64_t i = 0U; i < 1U << gui::SDLWindow::modBits; ++i)
						{
							if ((i & ~dof) == mod)
							{
								auto key = (i << actionKeyModShift) | rest;
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
					if (keyToAction.second->dof)
					{
						// * Again, there might be a computationally cheaper way to do this; see above.
						auto dof = uint64_t(keyToAction.second->dof) & dofMask;
						auto mod = (keyToAction.first & actionKeyModMask) >> actionKeyModShift;
						auto rest = keyToAction.first & ~actionKeyModMask;
						for (uint64_t i = 0U; i < 1U << gui::SDLWindow::modBits; ++i)
						{
							if ((i & ~dof) == mod)
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
		struct DefaultAction
		{
			String name;
			uint32_t dof; // * Degrees of freedom.
			int precedence;
			std::function<void ()> begin;
			std::function<void (bool)> end;
		};
		struct DefaultContext
		{
			String name;
			String parentName;
			int precedence;
		};
		struct DefaultKeyBind
		{
			String actionName;
			String contextName;
			uint64_t key;
		};

		std::vector<DefaultContext> defaultContexts = {
			// * The order in which these are listed is important: the line of
			//   code that defines a context must come after the line of code
			//   that defines its parent.
			// * ROOT's parent is empty string, i.e. nothing.
			{ "ROOT"  ,     "", 1000 },
			{ "SELECT", "ROOT", 1001 },
			{ "PASTE" , "ROOT", 1002 },
		};
		auto brushDof = gui::SDLWindow::mod0 | gui::SDLWindow::mod1 | gui::SDLWindow::mod2;
		auto toolDof = gui::SDLWindow::mod0 | gui::SDLWindow::mod1;
		auto propDof = gui::SDLWindow::mod0;
		std::vector<DefaultAction> defaultActions = {
			{ "NONE"               ,        0,    0,                                        nullptr,                      nullptr },
			{ "TOGGLEPAUSED"       ,        0, 1000, [this]() { TogglePaused();                   },                      nullptr },
			{ "TOGGLEPRETTYPOWDERS",        0, 2000, [this]() { TogglePrettyPowders();            },                      nullptr },
			{ "TOGGLEDRAWGRAV"     ,        0, 2001, [this]() { ToggleDrawGravity();              },                      nullptr },
			{ "TOGGLEDRAWDECO"     ,        0, 2002, [this]() { ToggleDrawDeco();                 },                      nullptr },
			{ "TOGGLENEWTONIAN"    ,        0, 2003, [this]() { ToggleNewtonianGravity();         },                      nullptr },
			{ "TOGGLEAMBHEAT"      ,        0, 2004, [this]() { ToggleAmbientHeat();              },                      nullptr },
			{ "CYCLEAIR"           ,        0,    0, [this]() { CycleAir();                       },                      nullptr },
			{ "CYCLEGRAV"          ,        0,    0, [this]() { CycleGravity();                   },                      nullptr },
			{ "CYCLEBRUSH"         ,        0,    0, [this]() { CycleBrush();                     },                      nullptr },
			{ "QUIT"               ,        0,    0, [this]() { PreQuit();                        },                      nullptr },
			{ "TOGGLEHUD"          ,        0,    0, [this]() { ToggleDrawHUD();                  },                      nullptr },
			{ "TOGGLEDEBUG"        ,        0,    0, [this]() { ToggleDebugHUD();                 },                      nullptr },
			{ "TOOLSEARCH"         ,        0,    0, [this]() { OpenToolSearch();                 },                      nullptr },
			{ "SHOWCONSOLE"        ,        0,    0, [this]() { OpenConsole();                    },                      nullptr },
			{ "COPY"               ,        0,    0, [this]() { SelectStart(selectModeCopy);      },                      nullptr },
			{ "CUT"                ,        0,    0, [this]() { SelectStart(selectModeCut);       },                      nullptr },
			{ "STAMP"              ,        0,    0, [this]() { SelectStart(selectModeStamp);     },                      nullptr },
			{ "LOADPREVSTAMP"      ,        0,    0, [this]() { PasteFromPrevStamp();             },                      nullptr },
			{ "LOADNEXTSTAMP"      ,        0,    0, [this]() { PasteFromNextStamp();             },                      nullptr },
			{ "PASTE"              ,        0,    0, [this]() { PasteFromClipboard();             },                      nullptr },
			{ "SELECTFINISH"       ,        0,    0, [this]() { SelectSetOrigin();                }, [this](bool cancel) { SelectFinish(cancel); } },
			{ "SELECTCANCEL"       ,        0,    0,                                        nullptr, [this](bool)        { SelectCancel();       } },
			{ "PASTEFINISH"        ,        0,    0,                                        nullptr, [this](bool cancel) { PasteFinish (cancel); } },
			{ "PASTECANCEL"        ,        0,    0,                                        nullptr, [this](bool)        { PasteCancel ();       } },
			{ "PASTERIGHT"         ,        0,    0, [this]() { PasteNudge({  1,  0 });           },                      nullptr },
			{ "PASTELEFT"          ,        0,    0, [this]() { PasteNudge({ -1,  0 });           },                      nullptr },
			{ "PASTEUP"            ,        0,    0, [this]() { PasteNudge({  0, -1 });           },                      nullptr },
			{ "PASTEDOWN"          ,        0,    0, [this]() { PasteNudge({  0,  1 });           },                      nullptr },
			{ "PASTEROTATE"        ,        0,    0, [this]() { PasteRotate();                    },                      nullptr },
			{ "PASTEHFLIP"         ,        0,    0, [this]() { PasteHorizontalFlip();            },                      nullptr },
			{ "PASTEVFLIP"         ,        0,    0, [this]() { PasteVerticalFlip();              },                      nullptr },
			{ "UNDO"               ,        0,    0, [this]() { HistoryUndo();                    },                      nullptr },
			{ "REDO"               ,        0,    0, [this]() { HistoryRedo();                    },                      nullptr },
			{ "BRUSHZOOMSIZEUP"    , brushDof, 1200, [this]() { AdjustBrushOrZoomSize( 1);        },                      nullptr },
			{ "BRUSHZOOMSIZEDOWN"  , brushDof, 1201, [this]() { AdjustBrushOrZoomSize(-1);        },                      nullptr },
			{ "TOOL0"              ,  toolDof, 1100, [this]() { ToolStart(0);                     },  [this](bool) { ToolFinish(0); } },
			{ "TOOL1"              ,  toolDof, 1101, [this]() { ToolStart(1);                     },  [this](bool) { ToolFinish(1); } },
			{ "TOOL2"              ,  toolDof, 1102, [this]() { ToolStart(2);                     },  [this](bool) { ToolFinish(2); } },
			{ "RELOADSIM"          ,        0,    0, [this]() { ReloadSimulation();               },                      nullptr },
			{ "TOGGLEINTRO"        ,        0,    0, [this]() { ToggleIntroText();                },                      nullptr },
			{ "PROPTOOL"           ,  propDof,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "OPENSTAMPS"         ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "FRAMESTEP"          ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "GRIDUP"             ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "GRIDDOWN"           ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "TOGGLEDECOTOOL"     ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "RESETAMBHEAT"       ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "RESETSPARK"         ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "RESETPRESSURE"      ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "INVERTPRESSURE"     ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "INSTALL"            ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "TOGGLESDELETE"      ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "TOGGLEREPLACE"      ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
			{ "ZOOM"               ,        0,    0,                                        nullptr,                      nullptr }, // * TODO-REDO_UI
		};
		std::vector<DefaultKeyBind> defaultKeyBinds = {
			{ "TOGGLEPAUSED"     , "ROOT"  ,   SCAN  (SDL_SCANCODE_SPACE       ) },
			{ "TOGGLEDRAWGRAV"   , "ROOT"  ,   SCAN1 (SDL_SCANCODE_G           ) },
			{ "TOGGLEDRAWDECO"   , "ROOT"  ,   SCAN1 (SDL_SCANCODE_B           ) },
			{ "TOGGLENEWTONIAN"  , "ROOT"  ,   SCAN  (SDL_SCANCODE_N           ) },
			{ "TOGGLEAMBHEAT"    , "ROOT"  ,   SCAN  (SDL_SCANCODE_U           ) },
			{ "SHOWCONSOLE"      , "ROOT"  ,   SCAN  (SDL_SCANCODE_GRAVE       ) },
			{ "PROPTOOL"         , "ROOT"  ,   SCAN1 (SDL_SCANCODE_P           ) },
			{ "PROPTOOL"         , "ROOT"  ,   SCAN1 (SDL_SCANCODE_F2          ) },
			{ "TOGGLEDEBUG"      , "ROOT"  ,   SCAN  (SDL_SCANCODE_F3          ) },
			{ "TOGGLEDEBUG"      , "ROOT"  ,   SCAN  (SDL_SCANCODE_D           ) }, // * TODO-REDO_UI: stickmen?
			{ "RELOADSIM"        , "ROOT"  ,   SCAN  (SDL_SCANCODE_F5          ) },
			{ "RELOADSIM"        , "ROOT"  ,   SCAN1 (SDL_SCANCODE_R           ) },
			{ "TOOLSEARCH"       , "ROOT"  ,   SCAN  (SDL_SCANCODE_E           ) },
			{ "FRAMESTEP"        , "ROOT"  ,   SCAN  (SDL_SCANCODE_F           ) },
			{ "GRIDUP"           , "ROOT"  ,   SCAN  (SDL_SCANCODE_G           ) },
			{ "GRIDDOWN"         , "ROOT"  ,   SCAN0 (SDL_SCANCODE_G           ) },
			{ "TOGGLEINTRO"      , "ROOT"  ,   SCAN  (SDL_SCANCODE_F1          ) },
			{ "TOGGLEINTRO"      , "ROOT"  ,   SCAN1 (SDL_SCANCODE_H           ) },
			{ "TOGGLEHUD"        , "ROOT"  ,   SCAN  (SDL_SCANCODE_H           ) },
			{ "TOGGLEDECOTOOL"   , "ROOT"  ,   SCAN  (SDL_SCANCODE_B           ) },
			{ "REDO"             , "ROOT"  ,   SCAN1 (SDL_SCANCODE_Y           ) },
			{ "CYCLEAIR"         , "ROOT"  ,   SCAN  (SDL_SCANCODE_Y           ) },
			{ "CYCLEGRAV"        , "ROOT"  ,   SCAN  (SDL_SCANCODE_W           ) }, // * TODO-REDO_UI: stickmen?
			{ "QUIT"             , "ROOT"  ,   SCAN  (SDL_SCANCODE_Q           ) },
			{ "QUIT"             , "ROOT"  ,   SCAN  (SDL_SCANCODE_ESCAPE      ) },
			{ "RESETAMBHEAT"     , "ROOT"  ,   SCAN1 (SDL_SCANCODE_U           ) },
			{ "RESETSPARK"       , "ROOT"  ,   SCAN1 (SDL_SCANCODE_EQUALS      ) },
			{ "RESETPRESSURE"    , "ROOT"  ,   SCAN  (SDL_SCANCODE_EQUALS      ) },
			{ "COPY"             , "ROOT"  ,   SCAN1 (SDL_SCANCODE_C           ) },
			{ "CUT"              , "ROOT"  ,   SCAN1 (SDL_SCANCODE_X           ) },
			{ "STAMP"            , "ROOT"  ,   SCAN  (SDL_SCANCODE_S           ) }, // * TODO-REDO_UI: stickmen?
			{ "PASTE"            , "ROOT"  ,   SCAN1 (SDL_SCANCODE_V           ) },
			{ "LOADPREVSTAMP"    , "ROOT"  ,   SCAN  (SDL_SCANCODE_L           ) },
			{ "LOADNEXTSTAMP"    , "ROOT"  ,   SCAN0 (SDL_SCANCODE_L           ) },
			{ "OPENSTAMPS"       , "ROOT"  ,   SCAN  (SDL_SCANCODE_K           ) },
			{ "INVERTPRESSURE"   , "ROOT"  ,   SCAN  (SDL_SCANCODE_I           ) },
			{ "INSTALL"          , "ROOT"  ,   SCAN1 (SDL_SCANCODE_I           ) },
			{ "TOGGLESDELETE"    , "ROOT"  ,   SCAN1 (SDL_SCANCODE_SEMICOLON   ) },
			{ "TOGGLEREPLACE"    , "ROOT"  ,   SCAN  (SDL_SCANCODE_SEMICOLON   ) },
			{ "BRUSHZOOMSIZEUP"  , "ROOT"  ,   SCAN  (SDL_SCANCODE_RIGHTBRACKET) },
			{ "BRUSHZOOMSIZEDOWN", "ROOT"  ,   SCAN  (SDL_SCANCODE_LEFTBRACKET ) },
			{ "BRUSHZOOMSIZEUP"  , "ROOT"  ,  WHEEL  (                        1) },
			{ "BRUSHZOOMSIZEDOWN", "ROOT"  ,  WHEEL  (                       -1) },
			{ "ZOOM"             , "ROOT"  ,   SCAN  (SDL_SCANCODE_Z           ) },
			{ "UNDO"             , "ROOT"  ,   SCAN1 (SDL_SCANCODE_Z           ) },
			{ "REDO"             , "ROOT"  ,   SCAN01(SDL_SCANCODE_Z           ) },
			{ "TOGGLESDELETE"    , "ROOT"  ,   SCAN1 (SDL_SCANCODE_INSERT      ) },
			{ "TOGGLESDELETE"    , "ROOT"  ,   SCAN  (SDL_SCANCODE_DELETE      ) },
			{ "TOGGLEREPLACE"    , "ROOT"  ,   SCAN  (SDL_SCANCODE_INSERT      ) },
			{ "CYCLEBRUSH"       , "ROOT"  ,   SCAN  (SDL_SCANCODE_TAB         ) },
			{ "TOOL0"            , "ROOT"  , BUTTON  (SDL_BUTTON_LEFT          ) },
			{ "TOOL1"            , "ROOT"  , BUTTON  (SDL_BUTTON_RIGHT         ) },
			{ "TOOL2"            , "ROOT"  , BUTTON  (SDL_BUTTON_MIDDLE        ) },
			{ "TOOL2"            , "ROOT"  , BUTTON2 (SDL_BUTTON_LEFT          ) },
			{ "SELECTFINISH"     , "SELECT", BUTTON  (SDL_BUTTON_LEFT          ) },
			{ "SELECTCANCEL"     , "SELECT", BUTTON  (SDL_BUTTON_RIGHT         ) },
			{ "NONE"             , "SELECT", BUTTON  (SDL_BUTTON_MIDDLE        ) },
			{ "NONE"             , "SELECT", BUTTON2 (SDL_BUTTON_LEFT          ) },
			{ "PASTEFINISH"      , "PASTE" , BUTTON  (SDL_BUTTON_LEFT          ) },
			{ "PASTECANCEL"      , "PASTE" , BUTTON  (SDL_BUTTON_RIGHT         ) },
			{ "PASTERIGHT"       , "PASTE" ,   SCAN  (SDL_SCANCODE_RIGHT       ) },
			{ "PASTELEFT"        , "PASTE" ,   SCAN  (SDL_SCANCODE_LEFT        ) },
			{ "PASTEUP"          , "PASTE" ,   SCAN  (SDL_SCANCODE_UP          ) },
			{ "PASTEDOWN"        , "PASTE" ,   SCAN  (SDL_SCANCODE_DOWN        ) },
			{ "PASTEROTATE"      , "PASTE" ,   SCAN  (SDL_SCANCODE_R           ) },
			{ "PASTEHFLIP"       , "PASTE" ,   SCAN0 (SDL_SCANCODE_R           ) },
			{ "PASTEVFLIP"       , "PASTE" ,   SCAN01(SDL_SCANCODE_R           ) },
			{ "NONE"             , "PASTE" , BUTTON  (SDL_BUTTON_MIDDLE        ) },
			{ "NONE"             , "PASTE" , BUTTON2 (SDL_BUTTON_LEFT          ) },
		};
		std::vector<Action> gameActions;
		for (auto &action : defaultActions)
		{
			auto name = "DEFAULT_AC_" + action.name;
			gameActions.push_back({
				name,
				language::Template{ "DEFAULT_LS_ACTION_" + name }(),
				action.dof,
				action.precedence,
				action.begin,
				action.end,
			});
		}
		for (auto i = 0; i < int(rendererPresets.size()); ++i)
		{
			auto name = "DEFAULT_AC_USEPRESET_" + rendererPresets[i].name;
			gameActions.push_back({
				name,
				language::Template{ "DEFAULT_LS_ACTION_" + name }(),
				0,
				3000 + i,
				[this, i]() {
					if (i != 10 || debugHUD)
					{
						RendererPreset(i);
					}
				}
			});
			defaultKeyBinds.push_back({ "USEPRESET_" + rendererPresets[i].name, "ROOT", rendererPresets[i].defaultKey });
		}

		for (auto &action : gameActions)
		{
			// * If you're crashing here, make sure your actions are in order (no action is defined twice).
			assert(actions.find(action.name) == actions.end());
			actions.insert(std::make_pair(action.name, std::make_unique<Action>(action)));
		}
		ActionContext *rootContext = nullptr;
		for (auto &context : defaultContexts)
		{
			auto ptr = std::make_unique<ActionContext>();
			ptr->name = "DEFAULT_CX_" + context.name;
			ptr->description = language::Template{ "DEFAULT_LS_ACTION_" + ptr->name }();
			ptr->precedence = context.precedence;
			if (context.parentName == "")
			{
				rootContext = ptr.get();
			}
			else
			{
				auto parentIt = contexts.find("DEFAULT_CX_" + context.parentName);
				// * If you're crashing here, make sure your contexts are in order (the parent of every context exists and is defined earlier).
				assert(parentIt != contexts.end());
				parentIt->second->children.push_back(ptr.get());
				ptr->parent = parentIt->second.get();
			}
			// * If you're crashing here, make sure your contexts are in order (no context is defined twice).
			assert(contexts.find(ptr->name) == contexts.end());
			contexts.insert(std::make_pair(ptr->name, std::move(ptr)));
		}
		std::map<String, std::map<String, std::vector<uint64_t>>> actionToKeyBinds;
		for (auto &bindStr : prefs::GlobalPrefs::Ref().Get("KeyBinds", std::vector<String>{}))
		{
			if (auto contextSplit = bindStr.SplitBy(' '))
			{
				auto contextName = contextSplit.Before();
				auto actionWithKeyStr = contextSplit.After();
				if (auto actionSplit = actionWithKeyStr.SplitBy(' '))
				{
					auto actionName = actionSplit.Before();
					auto keyStr = actionSplit.After();
					uint64_t key;
					if (actionKeyFromString(keyStr, key))
					{
						actionToKeyBinds[contextName][actionName].push_back(key);
					}
				}
			}
		}
		std::map<String, std::map<String, std::vector<uint64_t>>> actionToDefaultKeyBinds;
		for (auto &bind : defaultKeyBinds)
		{
			auto contextName = "DEFAULT_CX_" + bind.contextName;
			auto actionName = "DEFAULT_AC_" + bind.actionName;
			// * If you're crashing here, make sure your keybinds are in order (each keybind references an existing context).
			assert(contexts.find(contextName) != contexts.end());
			// * If you're crashing here, make sure your keybinds are in order (each keybind references an existing action).
			assert(actions.find(actionName) != actions.end());
			actionToDefaultKeyBinds[contextName][actionName].push_back(bind.key);
		}
		for (auto &defaultContextActions : actionToDefaultKeyBinds)
		{
			auto &context = actionToKeyBinds[defaultContextActions.first];
			for (auto &keyBinds : defaultContextActions.second)
			{
				// * insert only inserts if no element with equivalent key exists.
				context.insert(keyBinds);
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

		contextSelect = contexts.at("DEFAULT_CX_SELECT").get();
		contextSelect->canEnter = [this]() {
			return nextSelectMode != selectModeMax;
		};
		contextSelect->enter = [this]() {
			selectMode = nextSelectMode;
			nextSelectMode = selectModeMax;
			selectOriginValid = false;
			switch (selectMode)
			{
			case selectModeCopy:
				ActivateActionTip("DEFAULT_LS_GAME_COPYHELP"_Ls());
				break;

			case selectModeCut:
				ActivateActionTip("DEFAULT_LS_GAME_CUTHELP"_Ls());
				break;

			case selectModeStamp:
				ActivateActionTip("DEFAULT_LS_GAME_STAMPHELP"_Ls());
				break;

			default:
				break;
			}
		};
		contextSelect->leave = [this]() {
			selectMode = selectModeMax;
			DeectivateActionTip();
		};

		contextPaste = contexts.at("DEFAULT_CX_PASTE").get();
		contextPaste->canEnter = [this]() {
			return bool(nextPasteSource);
		};
		contextPaste->enter = [this]() {
			pasteSource = nextPasteSource;
			nextPasteSource.reset();
			pasteRotflip = 0U;
			PasteCenter();
			ActivateActionTip("DEFAULT_LS_GAME_PASTEHELP"_Ls());
		};
		contextPaste->leave = [this]() {
			pasteRender.reset();
			pasteTexture->ImageData(gui::Image());
			pasteSource.reset();
			DeectivateActionTip();
		};
	}

	void Game::ToolStart(int index)
	{
		auto pos = gui::SDLWindow::Ref().MousePosition();
		if (!simulationRect.Contains(pos))
		{
			return;
		}
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
		auto *tl = currentTools[activeToolIndex];
		switch (activeToolMode)
		{
		case toolModeFree:
			if (tl && tl->Application() == tool::Tool::applicationClick)
			{
				tl->Click(pos);
			}
			ToolDrawLine(pos, pos);
			toolOrigin = pos;
			break;

		case toolModeLine:
			toolOrigin = pos;
			break;

		case toolModeRect:
			toolOrigin = pos;
			break;

		case toolModeFill:
			if (tl && tl->Application() == tool::Tool::applicationDraw)
			{
				tl->Fill(pos);
			}
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
		auto *tl = currentTools[activeToolIndex];
		switch (activeToolMode)
		{
		case toolModeLine:
			ToolDrawLine(toolOrigin, pos);
			break;

		case toolModeRect:
			if (tl && tl->Application() == tool::Tool::applicationDraw)
			{
				auto rect = gui::Rect::FromCorners(toolOrigin, pos);
				auto origin = rect.pos + rect.size / 2;
				for (auto y = rect.pos.y; y < rect.pos.y + rect.size.y; ++y)
				{
					for (auto x = rect.pos.x; x < rect.pos.x + rect.size.x; ++x)
					{
						tl->Draw({ x, y }, origin);
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
		if (!cons)
		{
			cons = std::make_shared<console::Console>();
		}
		gui::SDLWindow::Ref().PushBack(cons);
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
		auto *tl = currentTools[toolIndex];
		if (tl && tl->CellAligned())
		{
			lastBrush = cellAlignedBrush.get();
		}
		else
		{
			lastBrush = brushes[currentBrush].get();
		}
	}

	void Game::CurrentTool(int index, tool::Tool *tl)
	{
		if (tl && tl->Select())
		{
			return;
		}
		currentTools[index] = tl;
		auto gz = false;
		for (auto *ctl : currentTools)
		{
			gz |= ctl && ctl->EnablesGravityZones();
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
			std::vector<ByteString> arr;
			for (auto &favorite : favorites)
			{
				arr.push_back(favorite.ToUtf8());
			}
			prefs::GlobalPrefs::Ref().Set("Favorites", arr);
			shouldBuildToolPanel = true;
		}
	}

	bool Game::Favorite(const String &identifier) const
	{
		return favorites.find(identifier) != favorites.end();
	}

	void Game::ActivateActionTip(const String &text)
	{
		actionTipText = text;
		actionTipAnim.Start(2, true);
	}

	void Game::DeectivateActionTip()
	{
		actionTipAnim.Start(0, false);
	}

	void Game::SelectStart(SelectMode newSelectMode)
	{
		nextSelectMode = newSelectMode;
		EnterContext(contextSelect);
	}

	void Game::SelectSetOrigin()
	{
		selectOrigin = simulationRect.Clamp(gui::SDLWindow::Ref().MousePosition());
		selectOriginValid = true;
	}

	void Game::SelectFinish(bool cancel)
	{
		if (cancel)
		{
			SelectCancel();
			return;
		}
		auto &g = gui::SDLWindow::Ref();
		auto mod0 = bool(g.Mod() & gui::SDLWindow::mod0);
		auto rect = gui::Rect::FromCorners(selectOrigin, simulationRect.Clamp(g.MousePosition()));
		auto atl = rect.TopLeft();
		auto abr = rect.BottomRight();
		if (atl.x % CELL) atl.x =  atl.x / CELL      * CELL;
		if (atl.y % CELL) atl.y =  atl.y / CELL      * CELL;
		abr += gui::Point{ 1, 1 };
		if (abr.x % CELL) abr.x = (abr.x / CELL + 1) * CELL;
		if (abr.y % CELL) abr.y = (abr.y / CELL + 1) * CELL;
		abr -= gui::Point{ 1, 1 };
		// * TODO-REDO_UI: Have Simulation::Save return an std::shared_ptr.
		auto selected = std::shared_ptr<GameSave>(simulation->Save(includePressure != mod0, atl.x, atl.y, abr.x, abr.y));
		selected->paused = paused;
		auto prevSelectMode = selectMode;
		LeaveContext(contextSelect);
		switch (prevSelectMode)
		{
		case selectModeCopy:
		case selectModeCut:
			{
				// * TODO-REDO_UI: Blindly copied, factor out?
				auto &be = backend::Backend::Ref();
				Json::Value clipboardInfo;
				clipboardInfo["type"] = "clipboard";
				clipboardInfo["username"] = be.User().name.ToUtf8();
				clipboardInfo["date"] = Json::Value::UInt64(time(NULL));
				// Client::Ref().SaveAuthorInfo(&clipboardInfo); // * TODO-REDO_UI
				selected->authors = clipboardInfo;
				clipboard = selected;
				if (prevSelectMode == selectModeCut)
				{
					HistorySnapshot();
					simulation->clear_area(rect.TopLeft().x, rect.TopLeft().y, rect.size.x, rect.size.y);
					ActivateActionTip("DEFAULT_LS_GAME_CUTDONE"_Ls());
				}
				else
				{
					ActivateActionTip("DEFAULT_LS_GAME_COPYDONE"_Ls());
				}
			}
			break;

		case selectModeStamp:
			{
				// auto stampName = Client::Ref().AddStamp(selected.get());
				// if (stampName.size())
				// {
				// 	ActivateActionTip("DEFAULT_LS_GAME_STAMPDONE"_Ls());
				// }
				// else
				// {
				// 	// * TODO-REDO_UI: Extract some sort of meaningful error message.
				// 	ActivateActionTip("DEFAULT_LS_GAME_STAMPFAILURE"_Ls());
				// } // * TODO-REDO_UI
			}
			break;

		default:
			break;
		}
	}

	void Game::SelectCancel()
	{
		LeaveContext(contextSelect);
		ActivateActionTip("DEFAULT_LS_GAME_SELECTCANCEL"_Ls());
	}

	void Game::PasteDispatchRender()
	{
		if (!pasteSource || !Renderer())
		{
			return;
		}
		pasteTexture->ImageData(gui::Image());
		gui::Point sizeB;
		pasteSource->BlockSizeAfterExtract(pasteRotflip, pasteNudge.x, pasteNudge.y, sizeB.x, sizeB.y);
		pasteRender = graphics::ThumbnailRendererTask::Create({
			pasteSource,
			true,
			true,
			simulationRenderer->RenderMode(),
			simulationRenderer->DisplayMode(),
			simulationRenderer->ColorMode(),
			pasteRotflip,
			pasteNudge,
			{ { 0, 0 }, sizeB },
		});
		pasteRender->Start(pasteRender);
		pasteSize = sizeB * CELL;
	}

	void Game::PasteFromPrevStamp()
	{
	}

	void Game::PasteFromNextStamp()
	{
		// auto stamps = Client::Ref().GetStamps(0, 1);
		// if (stamps.size())
		// {
		// 	auto saveFile = std::unique_ptr<SaveFile>(Client::Ref().GetStamp(stamps[0]));
		// 	if (saveFile && saveFile->GetGameSave())
		// 	{
		// 		auto expanded = std::make_shared<GameSave>(*saveFile->GetGameSave());
		// 		expanded->Expand(); // * TODO-REDO_UI: Verify that this won't throw.
		// 		PasteStart(expanded);
		// 	}
		// 	else
		// 	{
		// 		ActivateActionTip("DEFAULT_LS_GAME_LOADSTAMPFAILURE"_Ls());
		// 	}
		// }
		// else
		// {
		// 	ActivateActionTip("DEFAULT_LS_GAME_LOADSTAMPNOENT"_Ls());
		// } // * TODO-REDO_UI
	}

	void Game::PasteFromClipboard()
	{
		if (clipboard)
		{
			PasteStart(clipboard);
		}
		else
		{
			ActivateActionTip("DEFAULT_LS_GAME_LOADCLIPNOENT"_Ls());
		}
	}

	void Game::PasteGetThumbnail()
	{
		if (!pasteRender->status)
		{
			// * TODO-REDO_UI: Make it so that this is guaranteed to not happen.
			throw std::runtime_error("rendering paste thumbnail failed");
		}
		auto *pixels = pasteRender->output.Pixels();
		for (auto i = 0U; i < pasteRender->output.PixelsSize(); ++i)
		{
			pixels[i] |= UINT32_C(0xFF000000);
		}
		pasteTexture->ImageData(pasteRender->output);
		pasteRender.reset();
	}

	void Game::PasteStart(std::shared_ptr<GameSave> newPasteSource)
	{
		nextPasteSource = newPasteSource;
		EnterContext(contextPaste);
	}

	gui::Point Game::PastePos() const
	{
		auto dest = gui::SDLWindow::Ref().MousePosition() + pasteOffset;
		return {
			(dest.x + CELL / 2) / CELL * CELL,
			(dest.y + CELL / 2) / CELL * CELL,
		};
	}

	void Game::PasteFinish(bool cancel)
	{
		if (cancel)
		{
			PasteCancel();
			return;
		}
		auto mod0 = bool(gui::SDLWindow::Ref().Mod() & gui::SDLWindow::mod0);
		HistorySnapshot();
		auto pasteRect = PasteDestination();
		auto extractPos = pasteRect.pos - PastePos();
		auto save = std::make_unique<GameSave>(*pasteSource);
		save->Extract(pasteRotflip, pasteNudge.x, pasteNudge.y, extractPos.x / CELL, extractPos.y / CELL, pasteRect.size.x / CELL, pasteRect.size.y / CELL);
		auto failed = bool(simulation->Load(save.get(), !mod0, pasteRect.pos.x, pasteRect.pos.y));
		if (!failed)
		{
			Paused(save->paused || paused);
			// Client::Ref().MergeStampAuthorInfo(save->authors); // * TODO-REDO_UI
		}
		LeaveContext(contextPaste);
		ActivateActionTip("DEFAULT_LS_GAME_PASTEDONE"_Ls());
	}

	void Game::PasteCancel()
	{
		LeaveContext(contextPaste);
		ActivateActionTip("DEFAULT_LS_GAME_PASTECANCEL"_Ls());
	}

	void Game::PasteNudge(gui::Point nudge)
	{
		pasteNudge += nudge;
		if (pasteNudge.x <     0) { pasteNudge.x += CELL; pasteOffset.x -= CELL; };
		if (pasteNudge.x >= CELL) { pasteNudge.x -= CELL; pasteOffset.x += CELL; };
		if (pasteNudge.y <     0) { pasteNudge.y += CELL; pasteOffset.y -= CELL; };
		if (pasteNudge.y >= CELL) { pasteNudge.y -= CELL; pasteOffset.y += CELL; };
		PasteDispatchRender();
	}

	void Game::PasteCenter()
	{
		pasteNudge = { 0, 0 };
		PasteDispatchRender();
		pasteOffset = pasteSize / -2;
	}

	void Game::PasteRotate()
	{
		if (pasteRotflip & 4U) pasteRotflip ^= 3U; // * Invert rotation bits if horizontal flip bit is set, this turns +1 below into a -1.
		pasteRotflip = (pasteRotflip & 4U) | ((pasteRotflip + 1U) & 3U);
		if (pasteRotflip & 4U) pasteRotflip ^= 3U; // * Invert rotation bits back. A - 1 = ~(~A + 1).
		PasteCenter();
	}

	void Game::PasteHorizontalFlip()
	{
		pasteRotflip ^= 4U; // * Invert horizontal flip bit.
		PasteCenter();
	}

	void Game::PasteVerticalFlip()
	{
		pasteRotflip ^= 6U; // * Invert horizontal flip bit and rotate twice.
		PasteCenter();
	}

	gui::Rect Game::PasteDestination() const
	{
		return simulationRect.Intersect({ PastePos(), pasteSize });
	}

	void Game::DrawPasteTexture() const
	{
		auto dest = PasteDestination();
		gui::SDLWindow::Ref().DrawTexture(
			*pasteTexture,
			{ dest.pos              + gui::Point{ 1, 1 }, dest.size - gui::Point{ 2, 2 } },
			{ dest.pos - PastePos() + gui::Point{ 1, 1 }, dest.size - gui::Point{ 2, 2 } }
		);
	}

	void Game::DrawPasteArea(const LockedTextureData ltd) const
	{
		DrawHighlightRect(ltd, PasteDestination(), { 0, 0 });
	}

	void Game::HistorySnapshot()
	{
		beforeRestore.reset();
		auto last = simulation->CreateSnapshot();
		Snapshot *rebaseOnto = nullptr;
		if (historyPosition)
		{
			rebaseOnto = history.back().snap.get();
			if (historyPosition < int(history.size()))
			{
				historyCurrent = history[historyPosition - 1].delta->Restore(*historyCurrent);
				rebaseOnto = historyCurrent.get();
			}
		}
		while (historyPosition < int(history.size()))
		{
			history.pop_back();
		}
		if (rebaseOnto)
		{
			auto &prev = history.back();
			prev.delta = SnapshotDelta::FromSnapshots(*rebaseOnto, *last);
			prev.snap.reset();
		}
		history.emplace_back();
		history.back().snap = std::move(last);
		historyPosition += 1;
		historyCurrent.reset();
		while (historyLimit < int(history.size()))
		{
			history.pop_front();
			historyPosition -= 1;
		}
	}

	void Game::HistoryUndo()
	{
		if (!HistoryCanUndo())
		{
			ActivateActionTip("DEFAULT_LS_GAME_UNDONOENT"_Ls());
			return;
		}
		// * When Undoing for the first time since the last call to HistorySnapshot, save the current state.
		//   Redo needs this in order to bring you back to the point right before your last Undo, because
		//   the last history entry is what this Undo brings you back to, not the current state.
		if (!beforeRestore)
		{
			beforeRestore = simulation->CreateSnapshot();
			// beforeRestore->Authors = Client::Ref().GetAuthorInfo(); // * TODO-REDO_UI
		}
		historyPosition -= 1;
		if (history[historyPosition].snap)
		{
			historyCurrent = std::make_unique<Snapshot>(*history[historyPosition].snap);
		}
		else
		{
			historyCurrent = history[historyPosition].delta->Restore(*historyCurrent);
		}
		simulation->Restore(*historyCurrent);
		// Client::Ref().OverwriteAuthorInfo(historyCurrent->Authors); // * TODO-REDO_UI
		ActivateActionTip("DEFAULT_LS_GAME_UNDODONE"_Ls());
	}

	void Game::HistoryRedo()
	{
		if (!HistoryCanRedo())
		{
			ActivateActionTip("DEFAULT_LS_GAME_REDONOENT"_Ls());
			return;
		}
		historyPosition += 1;
		if (historyPosition == int(history.size()))
		{
			historyCurrent = nullptr;
		}
		else if (history[historyPosition].snap)
		{
			historyCurrent = std::make_unique<Snapshot>(*history[historyPosition].snap);
		}
		else
		{
			historyCurrent = history[historyPosition - 1].delta->Forward(*historyCurrent);
		}
		// * If we've Redo'd our way back to the original state, restore it and get rid of it.
		auto &current = historyCurrent ? *historyCurrent : *beforeRestore;
		simulation->Restore(current);
		// Client::Ref().OverwriteAuthorInfo(current.Authors); // * TODO-REDO_UI
		if (&current == beforeRestore.get())
		{
			beforeRestore.reset();
		}
		ActivateActionTip("DEFAULT_LS_GAME_REDODONE"_Ls());
	}

	void Game::HistoryLimit(int newHistoryLimit)
	{
		if (newHistoryLimit < 0)
		{
			newHistoryLimit = 0;
		}
		if (newHistoryLimit > undoHistoryLimitLimit)
		{
			newHistoryLimit = undoHistoryLimitLimit;
		}
		historyLimit = newHistoryLimit;
	}

	void Game::LoadSave(std::shared_ptr<GameSave> save)
	{
		Paused(save->paused);
		GravityMode(SimGravityMode(save->gravityMode));
		AirMode(SimAirMode(save->airMode));
		EdgeMode(SimEdgeMode(save->edgeMode));
		LegacyHeat(save->legacyEnable);
		WaterEqualisation(save->waterEEnabled);
		AmbientHeat(save->aheatEnable);
		NewtonianGravity(save->gravityEnable);
		simulation->clear_sim();
		simulation->Load(save.get(), true);
	}

	void Game::LoadLocalSave(String name, std::shared_ptr<GameSave> save)
	{
		LoadSave(save);
		reloadSource = save;
		reloadSourceInfo = {};
		reloadSourceInfo.name = name;
		execVote.reset();
		UpdateOnlineControls();
	}

	void Game::ReloadSimulation()
	{
		if (reloadSource)
		{
			LoadSave(reloadSource);
			UpdateOnlineControls();
		}
	}

	void Game::ClearSimulation()
	{
		simulation->clear_sim();
		reloadSource.reset();
		reloadSourceInfo = {};
		UpdateOnlineControls();
	}

	void Game::LoadOnlineSave(backend::SaveInfo info, std::shared_ptr<GameSave> save)
	{
		assert(info.detailLevel == backend::SaveInfo::detailLevelExtended);
		LoadSave(save);
		reloadSource = save;
		reloadSourceInfo = info;
		execVote.reset();
		UpdateOnlineControls();
	}
}
