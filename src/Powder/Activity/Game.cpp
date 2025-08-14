#include "Game.hpp"
#include "ElementSearch.hpp"
#include "client/GameSave.h"
#include "Common/Defer.hpp"
#include "Common/Div.hpp"
#include "Common/Log.hpp"
#include "Activity/OnlineBrowser.hpp"
#include "Activity/Settings.hpp"
#include "common/RasterGeometry.h"
#include "Format.h"
#include "IntroText.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/Stack.hpp"
#include "gui/game/Brush.h"
#include "gui/game/BitmapBrush.h"
#include "gui/game/EllipseBrush.h"
#include "gui/game/RectangleBrush.h"
#include "gui/game/TriangleBrush.h"
#include "gui/game/tool/Tool.h"
#include "gui/game/tool/ElementTool.h"
#include "gui/game/tool/WallTool.h"
#include "gui/game/tool/DecorationTool.h"
#include "gui/game/tool/SignTool.h"
#include "gui/game/tool/SampleTool.h"
#include "gui/game/tool/GOLTool.h"
#include "gui/game/tool/PropertyTool.h"
#include "graphics/Renderer.h"
#include "simulation/Air.h"
#include "simulation/ElementClasses.h"
#include "simulation/GOLString.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationData.h"
#include "simulation/Sample.h"
#include "simulation/SimTool.h"
#include "simulation/Snapshot.h"
#include "simulation/SnapshotDelta.h"
#include "simulation/ToolClasses.h"
#include "simulation/elements/FILT.h"
#include "prefs/GlobalPrefs.h"
#include "Save1844659.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr int32_t initialIntroTextAlpha = 10500;
		constexpr int32_t maxLogEntries         = 20;
		constexpr int32_t hudMargin             = 12;
		constexpr int32_t backdropMargin        = 4;
		constexpr auto toolSlotColors = std::array<Rgb8, Game::toolSlotCount>({
			0xFF0000_rgb,
			0x0000FF_rgb,
			0x00FF00_rgb,
			0x00FFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
		});
		constexpr auto decoSlotColor = 0xFF0000_rgb;

		std::optional<CustomGOLData> CheckCustomGol(String ruleString, String nameString, RGB color1, RGB color2)
		{
			if (!ValidateGOLName(nameString))
			{
				return std::nullopt;
			}
			auto rule = ParseGOLString(ruleString);
			if (rule == -1)
			{
				return std::nullopt;
			}
			auto &sd = SimulationData::CRef();
			for (auto &gd : sd.GetCustomGol())
			{
				if (gd.nameString == nameString)
				{
					return std::nullopt;
				}
			}
			return CustomGOLData{ rule, color1, color2, nameString };
		}

		Point CellFloor(Point point)
		{
			return point / CELL * CELL;
		}

		std::unique_ptr<VideoBuffer> CreateDecoSlotBackground()
		{
			constexpr int32_t patternSize = 2;
			auto texture = std::make_unique<VideoBuffer>(Game::toolTextureDataSize);
			auto *data = texture->Data();
			for (auto p : Game::toolTextureDataSize.OriginRect())
			{
				data[p.Y * Game::toolTextureDataSize.X + p.X] = (((p.X / patternSize + p.Y / patternSize) & 1) ? 0xFFFFFFFF_argb : 0xFFC8C8C8_argb).Pack();
			}
			return texture;
		}
	}

	Game::HistoryEntry::~HistoryEntry() = default;

	Game::Game(Gui::Host &newHost) :
		View(newHost),
		toolTipAlpha{ GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 400, 200 } },
		infoTipAlpha{ GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 400, 200 } },
		introTextAlpha{ GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 300 } }
	{
		introText = GetIntroText();
		decoSlots = {
			0xFFFFFFFF_argb,
			0xFF00FFFF_argb,
			0xFFFF00FF_argb,
			0xFFFFFF00_argb,
			0xFFFF0000_argb,
			0xFF00FF00_argb,
			0xFF0000FF_argb,
			0xFF000000_argb,
		};
		extraToolTextures[int32_t(ExtraToolTexture::decoSlotBackground)].data = CreateDecoSlotBackground();
		defaultEdgeMode = EDGE_VOID;
		defaultAmbientAirTemp = R_TEMP + 273.15f;
		activeMenuSection = SC_POWDERS;
		activeMenuSectionBeforeDeco = SC_POWDERS;
		rendererRemote = std::make_unique<Renderer>();
		renderer = rendererRemote.get();
		renderer->ClearAccumulation();
		simContext->simulation = std::make_unique<Simulation>();
		{
			auto save1844659 = Save1844659.AsCharSpan();
			save = LocalSave{
				"(none)",
				"Save1844659",
				std::nullopt,
				std::make_unique<GameSave>(std::vector(save1844659.begin(), save1844659.end())),
			};
			ReloadSim(true);
			SetSimPaused(false);
		}
		InitActions();
		InitBrushes();
		InitTools();
		introTextAlpha.SetValue(initialIntroTextAlpha);
	}

	Game::~Game()
	{
		StopRendererThread();
	}

	void Game::InitActions()
	{
		ActionContext root;
		auto actionKeyPrefix = "DEFAULT_ACTION_";
		auto initSimple = [this, &root, actionKeyPrefix](ByteString actionKeyStub, auto member, int32_t scancode, std::set<Input> modifiers = {}) {
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, actionKeyStub);
			AddAction(actionKey, Action{ [this, member]() {
				(this->*member)();
				return InputDisposition::exclusive;
			} });
			root.inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, actionKey });
		};
		auto initSimpleAlt = [&root, actionKeyPrefix](ByteString actionKeyStub, int32_t scancode, std::set<Input> modifiers = {}) {
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, actionKeyStub);
			root.inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, actionKey });
		};
		auto shiftInput = KeyboardKeyInput{ SDL_SCANCODE_LSHIFT };
		auto ctrlInput  = KeyboardKeyInput{ SDL_SCANCODE_LCTRL };
		initSimple   ("CYCLEBRUSH"            , &Game::CycleBrushAction            , SDL_SCANCODE_TAB      );
		initSimple   ("TOGGLEAMBIENTHEAT"     , &Game::ToggleAmbientHeatAction     , SDL_SCANCODE_U        );
		initSimple   ("TOGGLEDRAWDECO"        , &Game::ToggleDrawDecoAction        , SDL_SCANCODE_B        , { ctrlInput });
		initSimple   ("TOGGLEDRAWGRAVITY"     , &Game::ToggleDrawGravityAction     , SDL_SCANCODE_G        , { ctrlInput });
		initSimple   ("TOGGLEPAUSE"           , &Game::ToggleSimPausedAction       , SDL_SCANCODE_SPACE    );
		initSimple   ("TOGGLEDEBUGHUD"        , &Game::ToggleDebugHudAction        , SDL_SCANCODE_D        );
		initSimpleAlt("TOGGLEDEBUGHUD"                                             , SDL_SCANCODE_F3       );
		initSimple   ("UNDOHISTORYENTRY"      , &Game::UndoHistoryEntryAction      , SDL_SCANCODE_Z        , { ctrlInput });
		initSimple   ("REDOHISTORYENTRY"      , &Game::RedoHistoryEntryAction      , SDL_SCANCODE_Z        , { ctrlInput, shiftInput });
		initSimpleAlt("REDOHISTORYENTRY"                                           , SDL_SCANCODE_Y        , { ctrlInput });
		initSimple   ("RELOADSIM"             , &Game::ReloadSimAction             , SDL_SCANCODE_F5       );
		initSimpleAlt("RELOADSIM"                                                  , SDL_SCANCODE_R        , { ctrlInput });
		initSimple   ("CYCLEEDGEMODE"         , &Game::CycleEdgeModeAction         , SDL_SCANCODE_E        , { ctrlInput });
		initSimple   ("ELEMENTSEARCH"         , &Game::OpenElementSearch           , SDL_SCANCODE_E        );
		initSimple   ("FRAMESTEP"             , &Game::DoSimFrameStep              , SDL_SCANCODE_F        );
		initSimple   ("TOGGLEFIND"            , &Game::ToggleFindAction            , SDL_SCANCODE_F        , { ctrlInput });
		initSimple   ("GROWGRID"              , &Game::GrowGridAction              , SDL_SCANCODE_G        );
		initSimple   ("SHRINKGRID"            , &Game::ShrinkGridAction            , SDL_SCANCODE_G        , { shiftInput });
		initSimple   ("TOGGLEGRAVITYGRID"     , &Game::ToggleGravityGridAction     , SDL_SCANCODE_G        , { ctrlInput });
		initSimple   ("TOGGLEGINTROTEXT"      , &Game::ToggleIntroTextAction       , SDL_SCANCODE_F1       );
		initSimpleAlt("TOGGLEGINTROTEXT"                                           , SDL_SCANCODE_H        , { ctrlInput });
		initSimple   ("TOGGLEHUD"             , &Game::ToggleHudAction             , SDL_SCANCODE_H        );
		initSimple   ("TOGGLEDECO"            , &Game::ToggleDecoAction            , SDL_SCANCODE_B        , { ctrlInput });
		initSimple   ("TOGGLEDECOMENU"        , &Game::ToggleDecoMenuAction        , SDL_SCANCODE_B        );
		initSimple   ("CYCLEAIRMODE"          , &Game::CycleAirModeAction          , SDL_SCANCODE_Y        );
		initSimple   ("RESETAMBIENTHEAT"      , &Game::ResetAmbientHeatAction      , SDL_SCANCODE_U        , { ctrlInput });
		initSimple   ("TOGGLENEWTONIANGRAVITY", &Game::ToggleNewtonianGravityAction, SDL_SCANCODE_N        );
		initSimple   ("RESETAIR"              , &Game::ResetAirAction              , SDL_SCANCODE_EQUALS   );
		initSimple   ("RESETSPARK"            , &Game::ResetSparkAction            , SDL_SCANCODE_EQUALS   , { ctrlInput });
		initSimple   ("INVERTAIR"             , &Game::InvertAirAction             , SDL_SCANCODE_I        );
		initSimple   ("TOGGLEREPLACE"         , &Game::ToggleReplaceAction         , SDL_SCANCODE_SEMICOLON);
		initSimpleAlt("TOGGLEREPLACE"                                              , SDL_SCANCODE_INSERT   );
		initSimple   ("TOGGLESDELETE"         , &Game::ToggleSdeleteAction         , SDL_SCANCODE_SEMICOLON, { ctrlInput });
		initSimpleAlt("TOGGLESDELETE"                                              , SDL_SCANCODE_INSERT   , { ctrlInput });
		initSimpleAlt("TOGGLESDELETE"                                              , SDL_SCANCODE_DELETE   );
		initSimple   ("OPENPROPERTY"          , &Game::OpenProperty                , SDL_SCANCODE_P        , { ctrlInput });
		initSimpleAlt("OPENPROPERTY"                                               , SDL_SCANCODE_F2       , { ctrlInput });

		// TODO-REDO_UI:
		//  SDL_SCANCODE_GRAVE: TOGGLECONSOLE
		//  SDL_SCANCODE_Z: zoom
		//  SDL_SCANCODE_P: screenshot
		//  SDL_SCANCODE_F2: screenshot
		//  SDL_SCANCODE_A: authors?
		//  SDL_SCANCODE_F11: fullscreen
		//  SDL_SCANCODE_Q: quit
		//  SDL_SCANCODE_ESCAPE: quit
		//  SDL_SCANCODE_AC_BACK: quit
		//  SDL_SCANCODE_I: install
		//  SDL_SCANCODE_K: stamp browser
		//  SDL_SCANCODE_L: last stamp
		//  SDL_SCANCODE_V: paste
		//  SDL_SCANCODE_C: copy
		//  SDL_SCANCODE_X: cut

		auto initRendererPreset = [this, &root, actionKeyPrefix](int index, int32_t scancode, std::set<Input> modifiers = {}) {
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, "RENDERERPRESET", index);
			AddAction(actionKey, Action{ [this, index]() {
				UseRendererPreset(index);
				QueueInfoTip(Renderer::renderModePresets[index].Name.ToUtf8());
				return InputDisposition::exclusive;
			} });
			root.inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, actionKey });
		};
		initRendererPreset( 0, SDL_SCANCODE_0);
		initRendererPreset( 1, SDL_SCANCODE_1);
		initRendererPreset( 2, SDL_SCANCODE_2);
		initRendererPreset( 3, SDL_SCANCODE_3);
		initRendererPreset( 4, SDL_SCANCODE_4);
		initRendererPreset( 5, SDL_SCANCODE_5);
		initRendererPreset( 6, SDL_SCANCODE_6);
		initRendererPreset( 7, SDL_SCANCODE_7);
		initRendererPreset( 8, SDL_SCANCODE_8);
		initRendererPreset( 9, SDL_SCANCODE_9);
		initRendererPreset(10, SDL_SCANCODE_1, { shiftInput });
		initRendererPreset(11, SDL_SCANCODE_6, { shiftInput });

		{
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, "LOADWITHOUTPRESSURE");
			AddAction(actionKey, Action{ [this]() {
				loadWithoutPressure = true;
				return InputDisposition::shared;
			}, [this]() {
				loadWithoutPressure = false;
			} });
			root.inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, {}, actionKey });
		}
		{
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, "SAMPLEPROPERTY");
			AddAction(actionKey, Action{ [this]() {
				sampleProperty = true;
				return InputDisposition::shared;
			}, [this]() {
				sampleProperty = false;
			} });
			root.inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, {}, actionKey });
		}
		{
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, "TOOLSTRENGTHMUL10");
			AddAction(actionKey, Action{ [this]() {
				toolStrengthMul10 = true;
				return InputDisposition::shared;
			}, [this]() {
				toolStrengthMul10 = false;
			} });
			root.inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, {}, actionKey });
		}
		{
			ActionKey actionKey = ByteString::Build(actionKeyPrefix, "TOOLSTRENGTHDIV10");
			AddAction(actionKey, Action{ [this]() {
				toolStrengthDiv10 = true;
				return InputDisposition::shared;
			}, [this]() {
				toolStrengthDiv10 = false;
			} });
			root.inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LCTRL }, {}, actionKey });
		}

		for (int32_t growShrink = 0; growShrink < 2; ++growShrink)
		{
			ByteString growShrinkStr;
			int32_t diff;
			int32_t scancode;
			MouseWheelInput::Direction direction;
			switch (growShrink)
			{
			case 0: growShrinkStr = "GROW"  ; scancode = SDL_SCANCODE_RIGHTBRACKET; direction = MouseWheelInput::Direction::positiveY; diff =  1; break;
			case 1: growShrinkStr = "SHRINK"; scancode = SDL_SCANCODE_LEFTBRACKET ; direction = MouseWheelInput::Direction::negativeY; diff = -1; break;
			}
			for (int32_t bothXY = 0; bothXY < 3; ++bothXY)
			{
				int32_t diffX, diffY;
				ByteString bothXYStr;
				std::set<Input> modifiers;
				switch (bothXY)
				{
				case 0: diffX = diff; diffY = diff;                                                break;
				case 1: diffX = diff; diffY =    0; bothXYStr = "X"; modifiers.insert(shiftInput); break;
				case 2: diffX =    0; diffY = diff; bothXYStr = "Y"; modifiers.insert(ctrlInput ); break;
				}
				for (int32_t linLog = 0; linLog < 2; ++linLog)
				{
					bool logarithmic;
					ByteString linLogStr;
					switch (linLog)
					{
					case 0: logarithmic = false;                    break;
					case 1: logarithmic = true ; linLogStr = "LOG"; break;
					}
					auto actionKey = actionKeyPrefix + growShrinkStr + "BRUSH" + bothXYStr + linLogStr;
					AddAction(actionKey, Action{ [this, diffX, diffY, logarithmic]() {
						AdjustBrushSize(diffX, diffY, logarithmic);
						return InputDisposition::exclusive;
					} });
					root.inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, actionKey });
					root.inputToActions.push_back({ MouseWheelInput{ direction }, modifiers, actionKey });
				}
			}
		}

		auto initDraw = [this, &root, actionKeyPrefix, shiftInput, ctrlInput](ByteString actionKeyStub, DrawMode drawMode, bool shift, bool ctrl, bool select) {
			for (int32_t i = 0; i < toolSlotCount; ++i)
			{
				ActionKey actionKey = ByteString::Build(actionKeyPrefix, actionKeyStub, i);
				AddAction(actionKey, Action{ [this, i, drawMode]() {
					BeginBrush(i, drawMode);
					return InputDisposition::exclusive;
				}, [this, i]() {
					EndBrush(i);
				} });
				ActionKey selectActionKey;
				if (select)
				{
					selectActionKey = ByteString::Build(actionKeyPrefix, "SELECT", i);
					AddAction(selectActionKey, Action{ [this, i]() {
						if (lastHoveredTool)
						{
							SelectTool(i, lastHoveredTool);
							return InputDisposition::exclusive;
						}
						return InputDisposition::unhandled;
					} });
				}
				std::optional<int32_t> button;
				switch (i)
				{
				case 0: button = SDL_BUTTON_LEFT  ; break;
				case 1: button = SDL_BUTTON_RIGHT ; break;
				case 2: button = SDL_BUTTON_MIDDLE; break;
				}
				if (button)
				{
					std::set<Input> modifiers;
					if (shift)
					{
						modifiers.insert(shiftInput);
					}
					if (ctrl)
					{
						modifiers.insert(ctrlInput);
					}
					if (select)
					{
						root.inputToActions.push_back({ MouseButtonInput{ *button }, modifiers, selectActionKey });
					}
					root.inputToActions.push_back({ MouseButtonInput{ *button }, modifiers, actionKey });
				}
			}
		};
		initDraw("DRAWFREE", DrawMode::free, false, false,  true);
		initDraw("DRAWLINE", DrawMode::line,  true, false, false);
		initDraw("DRAWRECT", DrawMode::rect, false,  true, false);
		initDraw("DRAWFILL", DrawMode::fill,  true,  true, false);

		AddActionContext("DEFAULT_ACTIONCONTEXT_ROOT", std::move(root));
		SetCurrentActionContext("DEFAULT_ACTIONCONTEXT_ROOT");
	}

	void Game::InitBrushes()
	{
		brushes.resize(NUM_DEFAULTBRUSHES);
		brushes[BRUSH_CIRCLE] = std::make_unique<EllipseBrush>(true); // TODO-REDO_UI: perfect circle
		brushes[BRUSH_SQUARE] = std::make_unique<RectangleBrush>();
		brushes[BRUSH_TRIANGLE] = std::make_unique<TriangleBrush>();
		SetBrushIndex(BRUSH_CIRCLE);
	}

	void Game::CycleBrushAction()
	{
		auto nextBrushIndex = brushIndex;
		auto tryNext = [this, &nextBrushIndex]() {
			nextBrushIndex += 1;
			if (!(nextBrushIndex >= 0 && nextBrushIndex < int32_t(brushes.size())))
			{
				nextBrushIndex = 0;
			}
		};
		for (int32_t i = 0; i < int32_t(brushes.size()); ++i)
		{
			tryNext();
			if (brushes[nextBrushIndex])
			{
				SetBrushIndex(nextBrushIndex);
				break;
			}
		}
		// TODO-REDO_UI: info tip
	}

	void Game::InitTools()
	{
		auto &sd = SimulationData::CRef();
		auto &elements = sd.elements;
		auto &builtinGol = SimulationData::builtinGol;
		for (int i = 0; i < PT_NUM; ++i)
		{
			if (elements[i].Enabled)
			{
				AllocElementTool(i);
			}
		}
		for (int i = 0; i < NGOL; ++i)
		{
			auto tool = std::make_unique<ElementTool>(PMAP(i, PT_LIFE), builtinGol[i].name, builtinGol[i].description, builtinGol[i].colour, "DEFAULT_PT_LIFE_" + builtinGol[i].name.ToAscii());
			tool->MenuSection = SC_LIFE;
			AllocTool(std::move(tool));
		}
		for (int i = 0; i < UI_WALLCOUNT; ++i)
		{
			auto tool = std::make_unique<WallTool>(i, sd.wtypes[i].descs, sd.wtypes[i].colour, sd.wtypes[i].identifier, sd.wtypes[i].textureGen);
			tool->MenuSection = SC_WALL;
			AllocTool(std::move(tool));
		}
		for (auto &tool : ::GetTools())
		{
			AllocTool(std::make_unique<SimTool>(tool));
		}
		// TODO-REDO_UI: get deco tool textures
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_ADD     , "ADD" , "Colour blending: Add."                         , 0x000000_rgb, "DEFAULT_DECOR_ADD" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_SUBTRACT, "SUB" , "Colour blending: Subtract."                    , 0x000000_rgb, "DEFAULT_DECOR_SUB" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_MULTIPLY, "MUL" , "Colour blending: Multiply."                    , 0x000000_rgb, "DEFAULT_DECOR_MUL" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_DIVIDE  , "DIV" , "Colour blending: Divide."                      , 0x000000_rgb, "DEFAULT_DECOR_DIV" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_SMUDGE  , "SMDG", "Smudge tool, blends surrounding deco together.", 0x000000_rgb, "DEFAULT_DECOR_SMDG"));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_CLEAR   , "CLR" , "Erase any set decoration."                     , 0x000000_rgb, "DEFAULT_DECOR_CLR" ));
		AllocTool(std::make_unique<DecorationTool>(*this, DECO_DRAW    , "SET" , "Draw decoration (No blending)."                , 0x000000_rgb, "DEFAULT_DECOR_SET" ));
		AllocTool(std::make_unique<PropertyTool>(*this));
		AllocTool(std::make_unique<SignTool>(*this));
		AllocTool(std::make_unique<SampleTool>(*this));
		AllocTool(std::make_unique<GOLTool>(*this));
		LoadCustomGol();
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			ResetToolSlot(i);
		}
		lastSelectedTool = toolSlots[0];
	}

	void Game::SaveCustomGol()
	{
		auto &prefs = GlobalPrefs::Ref();
		std::vector<ByteString> newCustomGOLTypes;
		auto &sd = SimulationData::Ref();
		for (auto &gd : sd.GetCustomGol())
		{
			StringBuilder sb;
			sb << gd.nameString << " " << SerialiseGOLRule(gd.rule) << " " << gd.colour1.Pack() << " " << gd.colour2.Pack();
			newCustomGOLTypes.push_back(sb.Build().ToUtf8());
		}
		prefs.Set("CustomGOL.Types", newCustomGOLTypes);
	}

	void Game::LoadCustomGol()
	{
		auto &prefs = GlobalPrefs::Ref();
		auto customGOLTypes = prefs.Get("CustomGOL.Types", std::vector<ByteString>{});
		bool removedAny = false;
		std::vector<CustomGOLData> newCustomGol;
		for (auto gol : customGOLTypes)
		{
			auto parts = gol.FromUtf8().PartitionBy(' ');
			if (parts.size() != 4)
			{
				removedAny = true;
				continue;
			}
			auto nameString = parts[0];
			auto ruleString = parts[1];
			auto &colour1String = parts[2];
			auto &colour2String = parts[3];
			RGB color1;
			RGB color2;
			try
			{
				color1 = RGB::Unpack(colour1String.ToNumber<int>());
				color2 = RGB::Unpack(colour2String.ToNumber<int>());
			}
			catch (std::exception &)
			{
				removedAny = true;
				continue;
			}
			if (auto gd = CheckCustomGol(ruleString, nameString, color1, color2))
			{
				newCustomGol.push_back(*gd);
				AllocCustomGolTool(*gd);
			}
			else
			{
				removedAny = true;
			}
		}
		auto &sd = SimulationData::Ref();
		sd.SetCustomGOL(newCustomGol);
		if (removedAny)
		{
			SaveCustomGol();
		}
	}

	void Game::AllocCustomGolTool(const CustomGOLData &gd)
	{
		auto tool = std::make_unique<ElementTool>(PMAP(gd.rule, PT_LIFE), gd.nameString, "Custom GOL type: " + SerialiseGOLRule(gd.rule), gd.colour1, "DEFAULT_PT_LIFECUST_" + gd.nameString.ToAscii(), nullptr); // TODO-REDO_UI-TRANSLATE
		tool->MenuSection = SC_LIFE;
		AllocTool(std::move(tool));
	}

	void Game::AllocTool(std::unique_ptr<Tool> tool)
	{
		std::optional<int> index;
		for (int i = 0; i < int(tools.size()); ++i)
		{
			if (!tools[i])
			{
				index = i;
				break;
			}
		}
		if (!index)
		{
			index = int(tools.size());
			tools.emplace_back();
		}
		// GameController::Ref().SetToolIndex(tool->Identifier, *index); // TODO-REDO_UI
		tools[*index] = GameToolInfo{};
		tools[*index]->tool = std::move(tool);
		UpdateToolTexture(*index);
	}

	void Game::ResetToolSlot(ToolSlotIndex toolSlotIndex)
	{
		Tool *tool{};
		switch (toolSlotIndex)
		{
		case 0:
			tool = GetToolFromIdentifier("DEFAULT_PT_DUST");
			break;

		case 2:
			tool = GetToolFromIdentifier("DEFAULT_UI_SAMPLE");
			break;

		default:
			tool = GetToolFromIdentifier("DEFAULT_PT_NONE");
			break;
		}
		toolSlots[toolSlotIndex] = tool;
	}

	void Game::SelectTool(ToolSlotIndex toolSlotIndex, Tool *tool)
	{
		toolSlots[toolSlotIndex] = tool;
		lastSelectedTool = tool;
		if (tool)
		{
			tool->Select(toolSlotIndex);
		}
	}

	void Game::FreeTool(Tool *tool)
	{
		auto index = GetToolIndex(tool);
		if (!index)
		{
			return;
		}
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			if (toolSlots[i] == tool)
			{
				ResetToolSlot(i);
			}
		}
		if (lastSelectedTool == tool)
		{
			lastSelectedTool = toolSlots[0];
		}
		if (lastHoveredTool == tool)
		{
			lastHoveredTool = nullptr;
		}
		// GameController::Ref().SetToolIndex(ptr->Identifier, std::nullopt); // TODO-REDO_UI
		auto &info = tools[*index];
		info->tool.reset();
		info->texture.reset();
		shouldUpdateToolAtlas = true;
	}

	void Game::AllocElementTool(ElementIndex elementIndex)
	{
		auto &sd = SimulationData::CRef();
		auto &elements = sd.elements;
		auto &elem = elements[elementIndex];
		switch (elementIndex)
		{
		case PT_LIGH:
			AllocTool(std::make_unique<Element_LIGH_Tool>(elementIndex, elem.Identifier));
			break;

		case PT_TESC:
			AllocTool(std::make_unique<Element_TESC_Tool>(elementIndex, elem.Identifier));
			break;

		case PT_STKM:
		case PT_FIGH:
		case PT_STKM2:
			AllocTool(std::make_unique<PlopTool>(elementIndex, elem.Identifier));
			break;

		default:
			AllocTool(std::make_unique<ElementTool>(elementIndex, elem.Identifier));
			break;
		}
		UpdateElementTool(elementIndex);
	}

	void Game::UpdateElementTool(ElementIndex elementIndex)
	{
		auto &sd = SimulationData::CRef();
		auto &elements = sd.elements;
		auto &elem = elements[elementIndex];
		auto *tool = GetToolFromIdentifier(elem.Identifier);
		tool->Name = elem.Name;
		tool->Description = elem.Description;
		tool->Colour = elem.Colour;
		tool->textureGen = elem.IconGenerator;
		tool->MenuSection = elem.MenuSection;
		tool->MenuVisible = elem.MenuVisible;
		UpdateToolTexture(*GetToolIndex(tool));
	}

	Tool *Game::GetToolFromIdentifier(const ByteString &identifier)
	{
		for (auto &info : tools)
		{
			if (!info)
			{
				continue;
			}
			if (info->tool->Identifier == identifier)
			{
				return info->tool.get();
			}
		}
		return nullptr;
	}

	std::optional<Game::ToolIndex> Game::GetToolIndex(Tool *tool)
	{
		if (tool)
		{
			for (int i = 0; i < int(tools.size()); ++i)
			{
				if (tools[i] && tools[i]->tool.get() == tool)
				{
					return i;
				}
			}
		}
		return std::nullopt;
	}

	bool Game::GetRendererThreadEnabled() const
	{
		return true;
	}

	void Game::RenderSimulation(const RenderableSimulation &sim, bool handleEvents)
	{
		rendererRemote->sim = &sim;
		rendererRemote->Clear();
		rendererRemote->RenderBackground();
		if (handleEvents)
		{
			BeforeSimDraw();
		}
		{
			// we may write graphicscache here
			auto &sd = SimulationData::Ref();
			std::unique_lock lk(sd.elementGraphicsMx);
			rendererRemote->RenderSimulation();
		}
		if (handleEvents)
		{
			AfterSimDraw();
		}
		rendererRemote->sim = nullptr;
	}

	bool Game::GetDrawDeco() const
	{
		return rendererSettings.decorationLevel != RendererSettings::decorationDisabled;
	}

	void Game::SetDrawDeco(bool newDrawDeco)
	{
		auto desiredLevel = newDrawDeco ? RendererSettings::decorationEnabled : RendererSettings::decorationDisabled;
		if (rendererSettings.decorationLevel != desiredLevel)
		{
			rendererSettings.decorationLevel = desiredLevel;
		}
	}

	bool Game::GetDrawGravity() const
	{
		return rendererSettings.gravityFieldEnabled;
	}

	void Game::SetDrawGravity(bool newDrawGravity)
	{
		rendererSettings.gravityFieldEnabled = newDrawGravity;
	}

	void Game::BeforeSimDraw()
	{
		// TODO-REDO_UI
	}

	void Game::AfterSimDraw()
	{
		// TODO-REDO_UI
	}

	void Game::RendererThread()
	{
		while (true)
		{
			{
				std::unique_lock lk(rendererThreadMx);
				rendererThreadOwnsRenderer = false;
				rendererThreadCv.notify_one();
				rendererThreadCv.wait(lk, [this]() {
					return rendererThreadState == RendererThreadState::stopping || rendererThreadOwnsRenderer;
				});
				if (rendererThreadState == RendererThreadState::stopping)
				{
					break;
				}
			}
			RenderSimulation(*rendererThreadSim, false);
		}
	}

	void Game::StartRendererThread()
	{
		bool start = false;
		bool notify = false;
		{
			std::lock_guard lk(rendererThreadMx);
			if (rendererThreadState == RendererThreadState::absent)
			{
				rendererThreadSim = std::make_unique<RenderableSimulation>();
				rendererThreadResult = std::make_unique<RendererFrame>();
				rendererThreadState = RendererThreadState::running;
				start = true;
			}
			else if (rendererThreadState == RendererThreadState::paused)
			{
				rendererThreadState = RendererThreadState::running;
				notify = true;
			}
		}
		if (start)
		{
			rendererThread = std::thread([this]() {
				RendererThread();
			});
			notify = true;
		}
		if (notify)
		{
			DispatchRendererThread();
		}
	}

	void Game::StopRendererThread()
	{
		bool join = false;
		{
			std::lock_guard lk(rendererThreadMx);
			if (rendererThreadState != RendererThreadState::absent)
			{
				rendererThreadState = RendererThreadState::stopping;
				join = true;
			}
		}
		if (join)
		{
			rendererThreadCv.notify_one();
			rendererThread.join();
		}
	}

	void Game::PauseRendererThread()
	{
		bool notify = false;
		{
			std::unique_lock lk(rendererThreadMx);
			if (rendererThreadState == RendererThreadState::running)
			{
				rendererThreadState = RendererThreadState::paused;
				notify = true;
			}
		}
		if (notify)
		{
			rendererThreadCv.notify_one();
		}
		WaitForRendererThread();
	}

	void Game::DispatchRendererThread()
	{
		renderer->ApplySettings(rendererSettings);
		renderer = nullptr;
		*rendererThreadSim = *simContext->simulation;
		rendererThreadSim->useLuaCallbacks = false;
		rendererThreadOwnsRenderer = true;
		{
			std::lock_guard lk(rendererThreadMx);
			rendererThreadOwnsRenderer = true;
		}
		rendererThreadCv.notify_one();
	}

	void Game::WaitForRendererThread()
	{
		std::unique_lock lk(rendererThreadMx);
		rendererThreadCv.wait(lk, [this]() {
			return !rendererThreadOwnsRenderer;
		});
		renderer = rendererRemote.get();
	}

	void Game::DrawBrush(void *pixels, int pitch)
	{
		auto m = GetMousePos();
		if (!(m && lastSelectedTool))
		{
			return;
		}
		auto mCurr = *m;
		if (lastSelectedTool->Blocky)
		{
			mCurr = CellFloor(mCurr);
		}
		auto xorPixel = [pixels, pitch](Point p) {
			if (RES.OriginRect().Contains(p))
			{
				auto &pixel = reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(pixels) + p.Y * pitch)[p.X];
				pixel = GetContrastColor(Rgb8::Unpack(pixel)).Pack();
			}
		};
		bool drawBrush = true;
		if (drawState)
		{
			auto mPrev = drawState->initialMousePos;
			if (lastSelectedTool->Blocky)
			{
				mPrev = CellFloor(mPrev);
			}
			switch (drawState->mode)
			{
			case DrawMode::line:
				// TODO-REDO_UI: still doesn't quite match Tool::DrawLine
				RasterizeLine<true>(mPrev, mCurr, xorPixel);
				break;

			case DrawMode::rect:
				{
					auto rect = ::RectBetween(mPrev, mCurr);
					rect.size += Point(CELL - 1, CELL - 1);
					RasterizeRect(rect, xorPixel);
					drawBrush = false;
				}
				break;

			default:
				break;
			}
		}
		auto *brush = GetCurrentBrush();
		if (!(drawBrush && brush))
		{
			return;
		}
		if (lastSelectedTool->Blocky)
		{
			auto cellBrushRadius = CellFloor(brushRadius);
			auto rect = ::RectBetween(mCurr - cellBrushRadius, mCurr + cellBrushRadius);
			rect.size += Point(CELL - 1, CELL - 1);
			RasterizeRect(rect, xorPixel);
		}
		else
		{
			for (auto p : brush->outline.Size().OriginRect())
			{
				if (brush->outline[p])
				{
					xorPixel(mCurr - brush->GetRadius() + p);
				}
			}
		}
	}

	void Game::GetBasicSampleText(std::vector<std::string> &lines, std::optional<int> &wavelengthGfx, const SimulationSample &sample)
	{
		auto &sd = SimulationData::CRef();
		ByteStringBuilder sampleInfo;
		sampleInfo << Format::Precision(2);
		int type = sample.particle.type;
		if (type)
		{
			int ctype = sample.particle.ctype;
			if (type == PT_PHOT || type == PT_BIZR || type == PT_BIZRG || type == PT_BIZRS || type == PT_FILT || type == PT_BRAY || type == PT_C5)
			{
				wavelengthGfx = ctype & 0x3FFFFFFF;
			}
			if (debugHud)
			{
				if (type == PT_LAVA && sd.IsElement(ctype))
				{
					sampleInfo << "Molten " << sd.ElementResolve(ctype, 0).ToUtf8(); // TODO-REDO_UI-TRANSLATE
				}
				else if ((type == PT_PIPE || type == PT_PPIP) && sd.IsElement(ctype))
				{
					if (ctype == PT_LAVA && sd.IsElement(sample.particle.tmp4))
					{
						sampleInfo << sd.ElementResolve(type, 0).ToUtf8() << " with molten " << sd.ElementResolve(sample.particle.tmp4, -1).ToUtf8(); // TODO-REDO_UI-TRANSLATE
					}
					else
					{
						sampleInfo << sd.ElementResolve(type, 0).ToUtf8() << " with " << sd.ElementResolve(ctype, sample.particle.tmp4).ToUtf8(); // TODO-REDO_UI-TRANSLATE
					}
				}
				else if (type == PT_LIFE)
				{
					sampleInfo << sd.ElementResolve(type, ctype).ToUtf8();
				}
				else if (type == PT_FILT)
				{
					sampleInfo << sd.ElementResolve(type, ctype).ToUtf8();
					static const std::array<const char *, 12> filtModes = {{ // TODO-REDO_UI-TRANSLATE
						"set colour",
						"AND",
						"OR",
						"subtract colour",
						"red shift",
						"blue shift",
						"no effect",
						"XOR",
						"NOT",
						"old QRTZ scattering",
						"variable red shift",
						"variable blue shift",
					}};
					if (sample.particle.tmp >= 0 && sample.particle.tmp < int32_t(filtModes.size()))
					{
						sampleInfo << " (" << filtModes[sample.particle.tmp] << ")";
					}
					else
					{
						sampleInfo << " (unknown mode)"; // TODO-REDO_UI-TRANSLATE
					}
				}
				else
				{
					sampleInfo << sd.ElementResolve(type, ctype).ToUtf8();
					if (wavelengthGfx || type == PT_EMBR || type == PT_PRTI || type == PT_PRTO)
					{
						// Do nothing, ctype is meaningless for these elements
					}
					// Some elements store extra LIFE info in upper bits of ctype, instead of tmp/tmp2
					else if (type == PT_CRAY || type == PT_DRAY || type == PT_CONV || type == PT_LDTC)
					{
						sampleInfo << " (" << sd.ElementResolve(TYP(ctype), ID(ctype)).ToUtf8() << ")";
					}
					else if (type == PT_CLNE || type == PT_BCLN || type == PT_PCLN || type == PT_PBCN || type == PT_DTEC)
					{
						sampleInfo << " (" << sd.ElementResolve(ctype, sample.particle.tmp).ToUtf8() << ")";
					}
					else if (sd.IsElement(ctype) && type != PT_GLOW && type != PT_WIRE && type != PT_SOAP && type != PT_LITH)
					{
						sampleInfo << " (" << sd.ElementResolve(ctype, 0).ToUtf8() << ")";
					}
					else if (ctype)
					{
						sampleInfo << " (" << ctype << ")";
					}
				}
				sampleInfo << ", Temp: "; // TODO-REDO_UI-TRANSLATE
				StringBuilder sb;
				sb << Format::Precision(2);
				format::RenderTemperature(sb, sample.particle.temp, temperatureScale);
				sampleInfo << sb.Build().ToUtf8();
				sampleInfo << ", Life: " << sample.particle.life; // TODO-REDO_UI-TRANSLATE
				if (sample.particle.type != PT_RFRG && sample.particle.type != PT_RFGL && sample.particle.type != PT_LIFE)
				{
					if (sample.particle.type == PT_CONV)
					{
						if (sd.IsElement(TYP(sample.particle.tmp)))
						{
							sampleInfo << ", Tmp: " << sd.ElementResolve(TYP(sample.particle.tmp), ID(sample.particle.tmp)).ToUtf8(); // TODO-REDO_UI-TRANSLATE
						}
						else
						{
							sampleInfo << ", Tmp: " << sample.particle.tmp; // TODO-REDO_UI-TRANSLATE
						}
					}
					else
					{
						sampleInfo << ", Tmp: " << sample.particle.tmp; // TODO-REDO_UI-TRANSLATE
					}
				}
				// only elements that use .tmp2 show it in the debug HUD
				if (type == PT_CRAY || type == PT_DRAY || type == PT_EXOT || type == PT_LIGH || type == PT_SOAP || type == PT_TRON ||
				    type == PT_VIBR || type == PT_VIRS || type == PT_WARP || type == PT_LCRY || type == PT_CBNW || type == PT_TSNS ||
				    type == PT_DTEC || type == PT_LSNS || type == PT_PSTN || type == PT_LDTC || type == PT_VSNS || type == PT_LITH ||
				    type == PT_CONV || type == PT_ETRD)
				{
					sampleInfo << ", Tmp2: " << sample.particle.tmp2; // TODO-REDO_UI-TRANSLATE
				}
				sampleInfo << ", Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				sampleInfo << sd.BasicParticleInfo(sample.particle).ToUtf8();
				sampleInfo << ", Temp: "; // TODO-REDO_UI-TRANSLATE
				StringBuilder sb;
				sb << Format::Precision(2);
				format::RenderTemperature(sb, sample.particle.temp, temperatureScale);
				sampleInfo << sb.Build().ToUtf8();
				sampleInfo << ", Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
			}
		}
		else if (sample.WallType)
		{
			if (sample.WallType >= 0 && sample.WallType < int32_t(sd.wtypes.size()))
			{
				sampleInfo << sd.wtypes[sample.WallType].name.ToUtf8();
			}
			sampleInfo << ", Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
		}
		else if (sample.isMouseInSim)
		{
			sampleInfo << "Empty, Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			sampleInfo << "Empty"; // TODO-REDO_UI-TRANSLATE
		}
		lines.push_back(sampleInfo.Build());
	}

	void Game::GetDebugSampleText(std::vector<std::string> &lines, const SimulationSample &sample)
	{
		if (!debugHud)
		{
			return;
		}
		ByteStringBuilder sampleInfo;
		sampleInfo << Format::Precision(2);
		int type = sample.particle.type;
		if (type)
		{
			sampleInfo << "#" << sample.ParticleID << ", ";
		}
		sampleInfo << "X:" << sample.PositionX << " Y:" << sample.PositionY;
		auto gravtot = std::abs(sample.GravityVelocityX) +
		               std::abs(sample.GravityVelocityY);
		if (gravtot)
		{
			sampleInfo << ", GX: " << sample.GravityVelocityX << " GY: " << sample.GravityVelocityY; // TODO-REDO_UI-TRANSLATE
		}
		if (GetAmbientHeat())
		{
			sampleInfo << ", AHeat: "; // TODO-REDO_UI-TRANSLATE
			StringBuilder sb;
			sb << Format::Precision(2);
			format::RenderTemperature(sb, sample.AirTemperature, temperatureScale);
			sampleInfo << sb.Build().ToUtf8();
		}
		lines.push_back(sampleInfo.Build());
	}

	void Game::GetFpsLines(std::vector<std::string> &lines, const SimulationSample &sample)
	{
		ByteStringBuilder fpsInfo;
		auto fps = 0; // TODO-REDO_UI
		fpsInfo << Format::Precision(2) << "FPS: " << fps; // TODO-REDO_UI-TRANSLATE
		if (debugHud)
		{
			if (rendererSettings.findingElement)
			{
				fpsInfo << " Parts: " << rendererStats.foundParticles << "/" << sample.NumParts; // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				fpsInfo << " Parts: " << sample.NumParts; // TODO-REDO_UI-TRANSLATE
			}
		}
		if ((std::holds_alternative<HdispLimitAuto>(rendererSettings.wantHdispLimitMin) ||
		     std::holds_alternative<HdispLimitAuto>(rendererSettings.wantHdispLimitMax)) && rendererStats.hdispLimitValid)
		{
			StringBuilder sb;
			sb << Format::Precision(2);
			sb << " [TEMP L:";
			format::RenderTemperature(sb, rendererStats.hdispLimitMin, temperatureScale);
			sb << " H:";
			format::RenderTemperature(sb, rendererStats.hdispLimitMax, temperatureScale);
			sb << "]";
			fpsInfo << sb.Build().ToUtf8();
		}
		if (simContext->simulation->replaceModeFlags & REPLACE_MODE)
		{
			fpsInfo << " [REPLACE MODE]"; // TODO-REDO_UI-TRANSLATE
		}
		if (simContext->simulation->replaceModeFlags & SPECIFIC_DELETE)
		{
			fpsInfo << " [SPECIFIC DELETE]"; // TODO-REDO_UI-TRANSLATE
		}
		if (rendererSettings.gridSize)
		{
			fpsInfo << " [GRID: " << rendererSettings.gridSize << "]"; // TODO-REDO_UI-TRANSLATE
		}
		if (rendererSettings.findingElement)
		{
			fpsInfo << " [FIND]"; // TODO-REDO_UI-TRANSLATE
		}
		// if (c->GetDebugFlags() & DEBUG_SIMHUD) // TODO-REDO_UI
		lines.push_back(fpsInfo.Build());
	}

	void Game::DrawHudLines(const std::vector<std::string> &lines, std::optional<int> wavelengthGfx, Gui::Alignment alignment, Rgb8 color)
	{
		auto &g = GetHost();
		auto r = GetRect();
		int32_t y = hudMargin;
		int32_t alpha = 255;
		auto backdropAlpha = alpha * 170 / 255;
		int32_t lastBackdropWidth = 0;
		for (int32_t i = 0; i < int32_t(lines.size()); ++i)
		{
			auto it = g.InternText(lines[i]);
			auto st = g.ShapeText(Gui::ShapeTextParameters{ it, std::nullopt, alignment });
			auto &stw = g.GetShapedTextWrapper(st);
			auto contentSize = stw.GetContentSize();
			auto x = hudMargin;
			auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
			if (alignment == Gui::Alignment::right)
			{
				x = r.size.X - backdropSize.X - x;
			}
			if (backdropSize.X > lastBackdropWidth)
			{
				auto p = r.pos + Pos2(x, y);
				g.DrawLine(p, p + Size2(backdropSize.X - lastBackdropWidth - 1, 0), 0x000000_rgb .WithAlpha(backdropAlpha));
			}
			g.FillRect({ r.pos + Pos2(x, y + 1), backdropSize - Size2(0, 1) }, 0x000000_rgb .WithAlpha(backdropAlpha));
			lastBackdropWidth = backdropSize.X;
			g.DrawText(r.pos + Pos2(x + backdropMargin, y + backdropMargin), st, color.WithAlpha(alpha));
			if (i == 0 && wavelengthGfx)
			{
				for (int b = 0; b < 30; ++b)
				{
					auto bit = 1 << b;
					auto color = 0x404040_rgb;
					if (*wavelengthGfx & bit)
					{
						auto spreadBit = (bit >> 2) |
						                 (bit >> 1) |
						                  bit       |
						                 (bit << 1) |
						                 (bit << 2);
						auto [ fr, fg, fb ] = Element_FILT_wavelengthsToColor(spreadBit);
						color = Rgb8(
							std::min(255, std::max(0, fr)),
							std::min(255, std::max(0, fg)),
							std::min(255, std::max(0, fb))
						);
					}
					auto p = r.pos + Pos2(r.size.X - contentSize.X + 10 - b, y - 2);
					g.DrawLine(p, p + Pos2(0, 2), color.WithAlpha(alpha));
				}
			}
			y += backdropSize.Y - 1;
		}
	}

	void Game::DrawLogLines()
	{
		auto &g = GetHost();
		auto r = GetRect();
		auto y = r.size.Y - hudMargin;
		for (auto i = int32_t(logEntries.size()) - 1; i >= 0; --i)
		{
			auto lineAlpha = std::min(logEntries[i].alpha.GetValue(), 255);
			auto it = g.InternText(logEntries[i].message);
			auto st = g.ShapeText(Gui::ShapeTextParameters{ it, std::nullopt, Gui::Alignment::left });
			auto &stw = g.GetShapedTextWrapper(st);
			auto contentSize = stw.GetContentSize();
			auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
			y -= backdropSize.Y;
			auto backdropAlpha = lineAlpha * 170 / 255;
			g.FillRect({ r.pos + Pos2(hudMargin, y), backdropSize }, 0x000000_rgb .WithAlpha(backdropAlpha));
			g.DrawText(r.pos + Pos2(hudMargin + backdropMargin, y + backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(lineAlpha));
			y -= 2;
		}
	}

	void Game::QueueToolTip(std::string message, Pos2 pos, Gui::Alignment horizontalAlignment, Gui::Alignment verticalAlignment)
	{
		toolTipInfo = { message, pos, horizontalAlignment, verticalAlignment };
		toolTipQueued = true;
	}

	void Game::DrawToolTip()
	{
		if (!toolTipInfo)
		{
			return;
		}
		auto &g = GetHost();
		auto rh = GetRect().Inset(hudMargin);
		auto alpha = std::min(toolTipAlpha.GetValue(), 255);
		auto backdropAlpha = alpha * 170 / 255;
		auto it = g.InternText(toolTipInfo->message);
		auto st = g.ShapeText(Gui::ShapeTextParameters{ it, Gui::ShapeTextParameters::MaxWidth{ rh.size.X, false }, toolTipInfo->horizontalAlignment });
		auto &stw = g.GetShapedTextWrapper(st);
		auto contentSize = stw.GetContentSize();
		auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
		auto pr = rh;
		pr.size -= backdropSize;
		auto pos = (toolTipInfo->pos - Pos2(
			int32_t(toolTipInfo->horizontalAlignment) * backdropSize.X / 2,
			int32_t(toolTipInfo->verticalAlignment  ) * backdropSize.Y / 2
		));
		pos = RobustClamp(pos, pr);
		g.FillRect({ pos, backdropSize }, 0x000000_rgb .WithAlpha(backdropAlpha));
		g.DrawText(pos + Pos2(backdropMargin, backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(alpha));
	}

	void Game::QueueInfoTip(std::string message)
	{
		infoTipInfo = { message };
		infoTipAlpha.SetValue(400);
	}

	void Game::DismissIntroText()
	{
		if (introTextAlpha.GetValue() > 255)
		{
			introTextAlpha.SetValue(255);
		}
	}

	void Game::DrawInfoTip()
	{
		if (!infoTipInfo)
		{
			return;
		}
		auto &g = GetHost();
		auto r = GetRect();
		auto alpha = std::min(infoTipAlpha.GetValue(), 255);
		auto backdropAlpha = alpha * 170 / 255;
		auto it = g.InternText(infoTipInfo->message);
		auto st = g.ShapeText(Gui::ShapeTextParameters{ it, std::nullopt, Gui::Alignment::left });
		auto &stw = g.GetShapedTextWrapper(st);
		auto contentSize = stw.GetContentSize();
		auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
		auto pos = r.pos + (r.size - backdropSize) / 2;
		g.FillRect({ pos, backdropSize }, 0x000000_rgb .WithAlpha(backdropAlpha));
		g.DrawText(pos + Pos2(backdropMargin, backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(alpha));
	}

	void Game::DrawIntroText()
	{
		auto alpha = std::min(introTextAlpha.GetValue(), 255);
		if (!alpha)
		{
			return;
		}
		auto &g = GetHost();
		auto r = GetRect();
		auto rh = r.Inset(hudMargin);
		auto backdropAlpha = alpha * 170 / 255;
		auto it = g.InternText(introText);
		auto st = g.ShapeText(Gui::ShapeTextParameters{ it, Gui::ShapeTextParameters::MaxWidth{ rh.size.X, true }, Gui::Alignment::left });
		g.FillRect(rh, 0x000000_rgb .WithAlpha(backdropAlpha));
		g.DrawText(rh.pos + Pos2(backdropMargin, backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(alpha));
	}

	void Game::GuiHud()
	{
		// TODO-REDO_UI: remove vectors
		std::vector<std::string> sampleLines;
		std::vector<std::string> fpsLines;
		std::optional<int> wavelengthGfx;
		auto m = GetMousePos();
		auto p = m ? *m : Pos2(-1, -1); // TODO-REDO_UI: pass std::optional to GetSample
		auto sample = simContext->simulation->GetSample(p.X, p.Y);
		GetBasicSampleText(sampleLines, wavelengthGfx, sample);
		GetDebugSampleText(sampleLines, sample);
		GetFpsLines(fpsLines, sample);
		DrawHudLines(sampleLines, wavelengthGfx, Gui::Alignment::right, 0xFFFFFF_rgb);
		DrawHudLines(fpsLines, std::nullopt, Gui::Alignment::left, 0x20D8FF_rgb);
		DrawLogLines();
		DrawToolTip();
		DrawInfoTip();
		DrawIntroText();
	}

	void Game::GuiSim()
	{
		BeginPanel("sim");
		SetSize(RES.Y);
		SetSizeSecondary(RES.X);
		if (true) // TODO-REDO_UI: decide whether we should render
		{
			rendererSettings.gravityZonesEnabled = false;
			for (int32_t i = 0; i < toolSlotCount; ++i)
			{
				if (toolSlots[i] && toolSlots[i]->Identifier == "DEFAULT_WL_GRVTY")
				{
					rendererSettings.gravityZonesEnabled = true;
				}
			}
			if (GetRendererThreadEnabled())
			{
				StartRendererThread();
				WaitForRendererThread();
				rendererStats = renderer->GetStats();
				*rendererThreadResult = renderer->GetVideo();
				rendererFrame = rendererThreadResult.get();
				DispatchRendererThread();
			}
			else
			{
				PauseRendererThread();
				renderer->ApplySettings(rendererSettings);
				RenderSimulation(*simContext->simulation, true);
				rendererStats = renderer->GetStats();
				rendererFrame = &renderer->GetVideo();
			}
		}
		auto &g = GetHost();
		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = RES.X;
		rect.h = RES.Y;
		SdlAssertZero(SDL_RenderCopy(g.GetRenderer(), simTexture, nullptr, &rect));
		if (hud)
		{
			GuiHud();
		}
		EndPanel();
	}

	void Game::UpdateToolTexture(ToolIndex index)
	{
		tools[index]->texture.reset();
		if (auto textureData = tools[index]->tool->GetTexture(toolTextureDataSize))
		{
			tools[index]->texture = GameToolInfo::TextureInfo{};
			tools[index]->texture->data = std::move(textureData);
		}
		shouldUpdateToolAtlas = true;
	}

	void Game::UpdateToolAtlas()
	{
		auto texturedTools = int32_t(extraToolTextures.size());
		for (auto &info : tools)
		{
			if (info && info->texture)
			{
				texturedTools += 1;
			}
		}
		int32_t sideLength = 0;
		while (sideLength * sideLength < texturedTools)
		{
			sideLength += 1;
		}
		toolAtlas = PlaneAdapter<std::vector<pixel>>(Size2(sideLength * toolTextureDataSize.X, sideLength * toolTextureDataSize.Y));
		int32_t pos = 0;
		auto addTexture = [&](GameToolInfo::TextureInfo &texture) {
			texture.toolAtlasPos = Pos2(pos % sideLength * toolTextureDataSize.X, pos / sideLength * toolTextureDataSize.Y);
			for (auto p : texture.data->Size().OriginRect())
			{
				toolAtlas[p + texture.toolAtlasPos] = texture.data->Data()[p.Y * toolTextureDataSize.X + p.X];
			}
			pos += 1;
		};
		for (auto &texture : extraToolTextures)
		{
			addTexture(texture);
		}
		for (auto &info : tools)
		{
			if (!(info && info->texture))
			{
				continue;
			}
			addTexture(*info->texture);
		}
		UpdateToolAtlasTexture(*this, true, toolAtlasTexture);
	}

	void Game::UpdateToolAtlasTexture(Gui::View &view, bool rendererUp, SDL_Texture *&externalToolAtlasTexture) const
	{
		auto &g = view.GetHost();
		SDL_DestroyTexture(externalToolAtlasTexture);
		externalToolAtlasTexture = nullptr;
		if (!rendererUp)
		{
			return;
		}
		auto size = toolAtlas.Size();
		if (size.X && size.Y)
		{
			externalToolAtlasTexture = SdlAssertPtr(SDL_CreateTexture(g.GetRenderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size.X, size.Y));
			SdlAssertZero(SDL_UpdateTexture(externalToolAtlasTexture, nullptr, toolAtlas.data(), sizeof(pixel) * toolAtlas.Size().X));
		}
	}

	void Game::GuiTools()
	{
		if (shouldUpdateToolAtlas)
		{
			UpdateToolAtlas();
			shouldUpdateToolAtlas = false;
		}
		auto &g = GetHost();
		BeginHPanel("toolbuttons");
		SetMaxSizeSecondary(MaxSizeFitParent{});
		SetSize(23);
		if (GetShowingDecoTools())
		{
			BeginHPanel("decoslots");
			SetMaxSize(MaxSizeFitParent{});
			SetPadding(10, 9, 1, 4);
			SetSpacing(1);
			SetAlignment(Gui::Alignment::left);
			SetOrder(Order::leftToRight);
			auto decoRect = [&](Rect rect, Rgba8 color) {
				auto leftRect = rect;
				auto rightRect = rect;
				leftRect.size.X /= 2;
				rightRect.pos.X += leftRect.size.X;
				rightRect.size.X -= leftRect.size.X;
				SDL_Rect rectFrom;
				auto &decoSlotBackground = extraToolTextures[int32_t(ExtraToolTexture::decoSlotBackground)];
				rectFrom.x = decoSlotBackground.toolAtlasPos.X;
				rectFrom.y = decoSlotBackground.toolAtlasPos.Y;
				rectFrom.w = rect.size.X;
				rectFrom.h = rect.size.Y;
				SDL_Rect rectTo;
				rectTo.x = rect.pos.X;
				rectTo.y = rect.pos.Y;
				rectTo.w = rect.size.X;
				rectTo.h = rect.size.Y;
				SdlAssertZero(SDL_RenderCopy(g.GetRenderer(), toolAtlasTexture, &rectFrom, &rectTo));
				g.FillRect(leftRect, color.NoAlpha().WithAlpha(255));
				g.FillRect(rightRect, color);
			};
			int32_t buttonIndex = 0;
			for (auto &decoSlot : decoSlots)
			{
				BeginComponent(buttonIndex);
				SetHandleButtons(true);
				SetSize(toolTextureDataSize.X + 4);
				SetSizeSecondary(toolTextureDataSize.Y + 4);
				auto rr = GetRect();
				auto rh = rr.Inset(2);
				decoRect(rh, decoSlot);
				g.DrawRect(rh, 0xFFFFFFFF_argb);
				if (decoSlot == decoColor || IsHovered())
				{
					g.DrawRect(rr, decoSlotColor.WithAlpha(255));
				}
				if (IsClicked(SDL_BUTTON_LEFT))
				{
					decoColor = decoSlot;
				}
				EndComponent();
				buttonIndex += 1;
			}
			EndPanel();
			BeginHPanel("colorpicker");
			{
				SetPadding(9, 9, 2, 5);
				SetParentFillRatio(0);
				BeginButton(0, "", Gui::View::ButtonFlags::none);
				SetSize(toolTextureDataSize.Y + 2);
				// .Y below is correct, do not fix
				SetSizeSecondary(toolTextureDataSize.Y + 2);
				auto rr = GetRect();
				decoRect(rr.Inset(1), decoColor);
				if (EndButton())
				{
					// TODO-REDO_UI: open colour picker
				}
			}
			EndPanel();
		}
		BeginHPanel("tools");
		SetMaxSize(MaxSizeFitParent{});
		SetPadding(9, 10, 1, 4);
		SetSpacing(1);
		auto r = GetRect();
		auto overflow = GetOverflow();
		// r.size.X - 1 below is correct, do not fix
		auto scrollWidth = std::max(1, r.size.X - 1);
		auto scrollLeft = r.pos.X + 1;
		if (auto m = GetMousePos())
		{
			toolScroll = m->X;
		}
		auto scrollPosition = std::clamp(toolScroll - scrollLeft, 0, scrollWidth);
		SetScroll(-scrollPosition * overflow / scrollWidth);
		SetAlignment(Gui::Alignment::right);
		SetOrder(Order::rightToLeft);
		int32_t buttonIndex = 0;
		lastHoveredTool = nullptr;
		for (auto &info : tools)
		{
			if (!(info && info->tool->MenuSection == activeMenuSection && info->tool->MenuVisible))
			{
				continue;
			}
			BeginComponent(buttonIndex);
			if (GuiToolButton(*this, *info, toolAtlasTexture))
			{
				QueueToolTip(info->tool->Description.ToUtf8(), r.pos + r.size, Gui::Alignment::right, Gui::Alignment::bottom); // TODO-REDO_UI-TRANSLATE
				lastHoveredTool = info->tool.get();
			}
			EndComponent();
			buttonIndex += 1;
		}
		{
			constexpr std::array<int, 9> fadeout = {{ // * Gamma-corrected.
				255, 195, 145, 103, 69, 42, 23, 10, 3
			}};
			for (int32_t i = 0; i < int32_t(fadeout.size()); ++i)
			{
				// r.size.X below is correct, do not fix
				g.DrawLine(r.pos + Pos2(r.size.X - i, 0), r.pos + Pos2(r.size.X - i, r.size.Y), 0x000000_rgb .WithAlpha(fadeout[i]));
				g.DrawLine(r.pos + Pos2(           i, 0), r.pos + Pos2(           i, r.size.Y), 0x000000_rgb .WithAlpha(fadeout[i]));
			}
		}
		if (overflow)
		{
			auto contentWidth = overflow + scrollWidth;
			auto barSize = scrollWidth * scrollWidth / contentWidth;
			auto barMaxOffset = scrollWidth - barSize;
			auto barOffset = scrollPosition * barMaxOffset / scrollWidth;
			g.FillRect({ Pos2(scrollLeft + barOffset, r.pos.Y + r.size.Y - 2), Pos2(barSize, 2) }, 0xFFC8C8C8_argb);
		}
		EndPanel();
		EndPanel();
	}

	void Game::GuiQuickOptions()
	{
		BeginVPanel("quickoptions");
		SetPadding(1);
		SetSpacing(1);
		SetAlignment(Gui::Alignment::top);
		auto quickOption = [this](ComponentKey key, StringView text, ButtonFlags buttonFlags, StringView toolTip) {
			BeginButton(key, text, buttonFlags);
			SetSize(15);
			if (IsHovered())
			{
				auto r = GetRect();
				QueueToolTip(std::string(toolTip.view), r.pos + Pos2(0, r.size.Y / 2), Gui::Alignment::right, Gui::Alignment::center);
			}
			return EndButton();
		};
		if (quickOption("prettypowder", "P", GetSandEffect() ? ButtonFlags::stuck : ButtonFlags::none, "Enable pretty powders")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleSandEffectAction();
		}
		if (quickOption("drawgravity", "G", GetDrawGravity() ? ButtonFlags::stuck : ButtonFlags::none, "Draw gravity field")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleDrawGravityAction();
		}
		if (quickOption("decorations", "D", GetDrawDeco() ? ButtonFlags::stuck : ButtonFlags::none, "Draw decorations")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleDrawDecoAction();
		}
		if (quickOption("newtoniangravity", "N", GetNewtonianGravity() ? ButtonFlags::stuck : ButtonFlags::none, "Enable newtonian gravity")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleNewtonianGravityAction();
		}
		if (quickOption("ambientheat", "A", GetAmbientHeat() ? ButtonFlags::stuck : ButtonFlags::none, "Enable ambient heat")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleAmbientHeatAction();
		}
		if (quickOption("openconsole", "C", ButtonFlags::none, "Open console")) // TODO-REDO_UI-TRANSLATE
		{
			// TODO-REDO_UI
		}
		EndPanel();
	}

	void Game::GuiMenuSections()
	{
		auto toolTip = [this](std::string &&message) {
			if (IsHovered())
			{
				auto r = GetRect();
				QueueToolTip(std::forward<std::string>(message), r.pos + Pos2(0, r.size.Y / 2), Gui::Alignment::right, Gui::Alignment::center);
			}
		};
		BeginVPanel("menusections");
		SetParentFillRatio(0);
		SetPadding(1, 0, 1, 1);
		SetSpacing(1);
		auto &sd = SimulationData::CRef();
		static const std::array<StringView, 16> iconOverrides = {{ // TODO-REDO_UI: move this where menu sections are defined
			StringView(Gui::iconWalls),
			StringView(Gui::iconElectronic),
			StringView(Gui::iconPowered),
			StringView(Gui::iconSensor),
			StringView(Gui::iconForce),
			StringView(Gui::iconExplosive),
			StringView(Gui::iconGas),
			StringView(Gui::iconLiquid),
			StringView(Gui::iconPowder),
			StringView(Gui::iconSolid),
			StringView(Gui::iconRadioactive),
			StringView(Gui::iconStar),
			StringView(Gui::iconGol),
			StringView(Gui::iconTool),
			StringView(Gui::iconFavorite),
			StringView(Gui::iconDeco),
		}};
		assert(sd.msections.size() == iconOverrides.size());
		for (int32_t i = 0; i < int32_t(sd.msections.size()); ++i)
		{
			BeginButton(i, iconOverrides[i], activeMenuSection == i ? ButtonFlags::stuck : ButtonFlags::none);
			SetSize(15);
			toolTip(sd.msections[i].name.ToUtf8()); // TODO-REDO_UI-TRANSLATE
			auto hovered = IsHovered();
			auto activate = EndButton();
			if (!(i == SC_DECO || menuSectionsNeedClick))
			{
				activate = hovered;
			}
			if (activate && activeMenuSection != i)
			{
				activeMenuSectionBeforeDeco = activeMenuSection;
				activeMenuSection = i;
				RequestRepeatFrame();
			}
		}
		{
			BeginButton("search", Gui::iconSearch2, ButtonFlags::none);
			SetSize(15);
			toolTip("Search for elements"); // TODO-REDO_UI-TRANSLATE
			if (EndButton())
			{
				OpenElementSearch();
			}
		}
		EndPanel();
	}

	void Game::GuiRight()
	{
		BeginVPanel("right");
		SetSize(17);
		GuiQuickOptions();
		GuiMenuSections();
		EndPanel();
	}

	void Game::GuiSave()
	{
		if (Button(Gui::iconOpen, 17))
		{
			OpenOnlineBrowser();
		}
		if (Button(Gui::iconReload, 17))
		{
			ReloadSimAction();
		}
		BeginButton("savesim", ByteString::Build(Gui::iconSave, "\u0011\u000A[untitled simulation]"), ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
		SetSize(150);
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		EndButton();
		Button(ByteString::Build(Gui::iconUpvote, "\u0011\u0003Vote"), 39); // TODO-REDO_UI-TRANSLATE
		AddSpacing(-2);
		Button(Gui::iconDownvote, 15);
		BeginButton("tags", ByteString::Build(Gui::iconTag, "\u0011\u0009[no tags set]"), ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		EndButton();
		if (Button(ByteString::Build(" ", Gui::iconNew, " "), 17)) // TODO-REDO_UI: remove padding when icon alignment has been fixed
		{
			ClearSim();
		}
		Button(Gui::iconKeyOutline, 20);
		AddSpacing(-2);
		BeginButton("user", "[sign in]", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
		SetSize(73);
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		EndButton();
		if (Button(Gui::iconCheckmark, 15))
		{
			PushAboveThis(std::make_shared<Settings>(*this));
		}
	}

	void Game::GuiRenderer()
	{
		BeginHPanel("renderersettings");
		SetSpacing(1);
		enum class BitPolicy
		{
			independent,
			exclusive,
			subset,
		};
		auto r = GetRect();
		auto group = [this, r](uint32_t componentKeyBase, auto &&groupItems, auto &mode, BitPolicy bitPolicy) {
			uint32_t newMode = mode;
			if (bitPolicy == BitPolicy::subset)
			{
				newMode = 0;
			}
			for (auto i = 0; i < int(groupItems.size()); ++i)
			{
				auto &info = groupItems[i];
				bool stuck = false;
				switch (bitPolicy)
				{
				case BitPolicy::exclusive:
					stuck = mode == info.mode;
					break;

				case BitPolicy::subset:
					stuck = (mode & info.mode) == info.mode;
					break;

				case BitPolicy::independent:
					stuck = mode & info.mode;
					break;
				}
				BeginButton(componentKeyBase + i, info.icon, stuck ? Gui::View::ButtonFlags::stuck : Gui::View::ButtonFlags::none);
				if (IsHovered())
				{
					QueueToolTip(std::string(info.toolTip), r.pos + r.size, Gui::Alignment::right, Gui::Alignment::bottom);
				}
				if (EndButton())
				{
					switch (bitPolicy)
					{
					case BitPolicy::exclusive:
						newMode = stuck ? 0 : info.mode;
						break;

					case BitPolicy::subset:
						stuck = !stuck;
						break;

					case BitPolicy::independent:
						newMode ^= info.mode;
						break;
					}
				}
				if (bitPolicy == BitPolicy::subset && stuck)
				{
					newMode |= info.mode;
				}
			}
			Separator(componentKeyBase + 100, 3);
			mode = newMode;
		};
		struct GroupItem
		{
			uint32_t mode;
			StringView icon;
			StringView toolTip;
		};
		static const std::array<GroupItem, 7> renderModes = {{
			{ RENDER_EFFE, Gui::iconFlare      , "Adds Special flare effects to some elements"                  }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_FIRE, Gui::iconFireOutline, "Fire effect for gasses"                                       }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_GLOW, Gui::iconGlow       , "Glow effect on some elements"                                 }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_BLUR, Gui::iconLiquid     , "Blur effect for liquids"                                      }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_BLOB, Gui::iconBlob       , "Makes everything be drawn like a blob"                        }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_BASC, Gui::iconBasic      , "Basic rendering, without this, most things will be invisible" }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_SPRK, Gui::iconFlare      , "Glow effect on sparks"                                        }, // TODO-REDO_UI-TRANSLATE
		}};
		group(1000, renderModes, rendererSettings.renderMode, BitPolicy::subset);
		static const std::array<GroupItem, 4> airDisplayModes = {{
			{ DISPLAY_AIRC, Gui::iconFanBlades      , "Displays pressure as red and blue, and velocity as white"                                                   }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_AIRP, Gui::iconSensor         , "Displays pressure, red is positive and blue is negative"                                                    }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_AIRV, Gui::iconWind           , "Displays velocity and positive pressure: up/down adds blue, right/left adds red, still pressure adds green" }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_AIRH, Gui::iconTemperatureBody, "Displays the temperature of the air like heat display does"                                                 }, // TODO-REDO_UI-TRANSLATE
		}};
		group(2000, airDisplayModes, rendererSettings.displayMode, BitPolicy::exclusive);
		static const std::array<GroupItem, 3> displayModes = {{
			{ DISPLAY_WARP, Gui::iconLensing, "Gravity lensing, Newtonian Gravity bends light with this on"    }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_EFFE, Gui::iconFlare  , "Enables moving solids, stickmen guns, and premium(tm) graphics" }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_PERS, Gui::iconPaths  , "Element paths persist on the screen for a while"                }, // TODO-REDO_UI-TRANSLATE
		}};
		group(3000, displayModes, rendererSettings.displayMode, BitPolicy::independent);
		static const std::array<GroupItem, 4> colorModes = {{
			{ COLOUR_HEAT, Gui::iconTemperatureBody, "Displays temperatures of the elements, dark blue is coldest, pink is hottest" }, // TODO-REDO_UI-TRANSLATE
			{ COLOUR_LIFE, Gui::iconScale          , "Displays the life value of elements in greyscale gradients"                   }, // TODO-REDO_UI-TRANSLATE
			{ COLOUR_GRAD, Gui::iconGradient       , "Changes colors of elements slightly to show heat diffusing through them"      }, // TODO-REDO_UI-TRANSLATE
			{ COLOUR_BASC, Gui::iconBasic          , "No special effects at all for anything, overrides all other options and deco" }, // TODO-REDO_UI-TRANSLATE
		}};
		group(4000, colorModes, rendererSettings.colorMode, BitPolicy::exclusive);
		static const std::array<StringView, 12> rendererPresetIcons = {{
			Gui::iconFanBlades,
			Gui::iconWind,
			Gui::iconSensor,
			Gui::iconPaths,
			Gui::iconFireOutline,
			Gui::iconBlob,
			Gui::iconTemperatureBody,
			Gui::iconLiquid,
			Gui::iconBasic,
			Gui::iconGradient,
			Gui::iconScale,
			Gui::iconTemperatureBody,
		}};
		{
			uint32_t componentKeyBase = 5000;
			for (auto i = 0; i < int(rendererPresetIcons.size()); ++i)
			{
				BeginButton(componentKeyBase + i, rendererPresetIcons[i], Gui::View::ButtonFlags::none);
				if (IsHovered())
				{
					QueueToolTip(Renderer::renderModePresets[i].Name.ToUtf8(), r.pos + r.size, Gui::Alignment::right, Gui::Alignment::bottom);
				}
				if (EndButton())
				{
					UseRendererPreset(i);
				}
			}
			Separator(componentKeyBase + 100, 3);
		}
		EndPanel();
	}

	void Game::GuiBottom()
	{
		BeginHPanel("bottom");
		SetSize(17);
		SetPadding(1);
		SetSpacing(1);
		{
			if (showRenderer)
			{
				GuiRenderer();
			}
			else
			{
				GuiSave();
			}
			if (Button(Gui::iconColors1, 15, showRenderer ? ButtonFlags::stuck : ButtonFlags::none))
			{
				showRenderer = !showRenderer;
			}
			if (Button(Gui::iconPause, 15, GetSimPaused() ? ButtonFlags::stuck : ButtonFlags::none))
			{
				ToggleSimPausedAction();
			}
		}
		EndPanel();
	}

	void Game::Gui()
	{
		SetRootRect(GetHost().GetSize().OriginRect());
		BeginVPanel("game");
		{
			BeginHPanel("top");
			{
				BeginVPanel("sim+tools");
				GuiSim();
				GuiTools();
				EndPanel();
			}
			GuiRight();
			EndPanel();
		}
		GuiBottom();
		EndPanel();
	}

	void Game::HandleTick()
	{
		if (drawState)
		{
			if (!drawState->mouseMovedSinceLastTick)
			{
				DragBrush(true);
			}
			drawState->mouseMovedSinceLastTick = false;
		}
		if (!simContext->simulation->sys_pause || simContext->simulation->framerender)
		{
			UpdateSimUpTo(NPART);
		}
		else
		{
			BeforeSim();
		}
	}

	void Game::SetRendererUp(bool newRendererUp)
	{
		if (newRendererUp)
		{
			auto &g = GetHost();
			simTexture = SdlAssertPtr(SDL_CreateTexture(g.GetRenderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RES.X, RES.Y));
			DrawSim();
		}
		else
		{
			SDL_DestroyTexture(simTexture);
			simTexture = nullptr;
		}
		UpdateToolAtlasTexture(*this, newRendererUp, toolAtlasTexture);
	}

	void Game::HandleBeforeFrame()
	{
		PauseSimThread();
	}

	void Game::HandleBeforeGui()
	{
		DrawSim();
	}

	void Game::HandleAfterGui()
	{
		while (!logEntries.empty() && !logEntries.front().alpha.GetValue())
		{
			logEntries.pop_front();
		}
		if (toolTipQueued)
		{
			toolTipAlpha.SetTarget(400);
			toolTipQueued = false;
		}
		else
		{
			toolTipAlpha.SetTarget(0);
		}
		if (!toolTipAlpha.GetValue())
		{
			toolTipInfo.reset();
		}
		if (!infoTipAlpha.GetValue())
		{
			infoTipInfo.reset();
		}
	}

	void Game::HandleAfterFrame()
	{
		if (GetSimThreadEnabled())
		{
			StartSimThread();
		}
	}

	void Game::DrawSim()
	{
		if (!simTexture || !rendererFrame)
		{
			return;
		}
		void *pixels;
		int pitch;
		SdlAssertZero(SDL_LockTexture(simTexture, nullptr, &pixels, &pitch));
		Defer unlockTexture([this]() {
			SDL_UnlockTexture(simTexture);
		});
		for (int y = 0; y < RES.Y; ++y)
		{
			std::copy_n(
				rendererFrame->data() + y * rendererFrame->Size().X,
				RES.X,
				reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(pixels) + y * pitch)
			);
		}
		DrawBrush(pixels, pitch);
	}

	float Game::GetToolStrength() const
	{
		if (toolStrengthMul10)
		{
			return 10.f;
		}
		if (toolStrengthDiv10)
		{
			return 0.1f;
		}
		return 1.0f;
	}

	bool Game::DragBrush(bool invokeToolDrag)
	{
		auto m = GetMousePos();
		auto *tool = toolSlots[drawState->toolSlotIndex];
		auto *brush = GetCurrentBrush();
		if (m && tool && brush)
		{
			auto *sim = simContext->simulation.get();
			tool->Strength = GetToolStrength();
			switch (drawState->mode)
			{
			case DrawMode::free:
				tool->DrawLine(sim, *brush, drawState->initialMousePos, *m, true);
				drawState->initialMousePos = *m;
				return true;

			case DrawMode::fill:
				tool->DrawFill(sim, *brush, *m);
				return true;

			case DrawMode::line:
				if (invokeToolDrag)
				{
					tool->Drag(sim, *brush, drawState->initialMousePos, *m);
				}
				break;

			default:
				break;
			}
		}
		return false;
	}

	void Game::AdjustBrushSize(int32_t diffX, int32_t diffY, bool logarithmic)
	{
		auto *brush = GetCurrentBrush();
		if (brush)
		{
			// TODO-REDO_UI: move adjust code here
			brush->AdjustSize(diffX == 0 ? diffY : diffX, logarithmic, diffX == 0, diffY == 0);
			brushRadius = brush->GetRadius();
		}
	}

	bool Game::HandleEvent(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEWHEEL:
		case SDL_KEYDOWN:
			if (event.type == SDL_KEYDOWN && event.key.repeat)
			{
				break;
			}
			DismissIntroText();
			break;
		}
#if DebugGuiView
		if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_KP_MULTIPLY)
		{
			shouldDoDebugDump = true;
		}
#endif
		auto handledByView = View::HandleEvent(event);
		if (MayBeHandledExclusively(event) && handledByView && !lastHoveredTool)
		{
			return true;
		}
		if (ClassifyExitEvent(event) == Gui::View::ExitEventType::exit) // TODO-REDO_UI: maybe unify with exit action from the InputMapper
		{
			if (showRenderer)
			{
				showRenderer = false;
				return true;
			}
			if (GetShowingDecoTools())
			{
				activeMenuSection = activeMenuSectionBeforeDeco;
				return true;
			}
		}
		auto handledByInputMapper = InputMapper::HandleEvent(event);
		if (MayBeHandledExclusively(event) && handledByInputMapper)
		{
			return true;
		}
		switch (event.type)
		{
		case SDL_MOUSEMOTION:
			if (drawState)
			{
				if (DragBrush(false))
				{
					drawState->mouseMovedSinceLastTick = true;
				}
			}
			break;
		}
		return false;
	}

	void Game::PauseSimThread()
	{
		if (!simContext)
		{
			// TODO-REDO_UI: acquire sim thread
			simContext = &simContextRemote;
		}
	}

	void Game::StartSimThread()
	{
		if (!simContext)
		{
			// TODO-REDO_UI: release sim thread
			simContext = nullptr;
		}
	}

	bool Game::GetSimThreadEnabled() const
	{
		// TODO-REDO_UI
		return true;
	}

	void Game::ToggleSandEffectAction()
	{
		SetSandEffect(!GetSandEffect());
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleDrawGravityAction()
	{
		SetDrawGravity(!GetDrawGravity());
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleDrawDecoAction()
	{
		SetDrawDeco(!GetDrawDeco());
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleNewtonianGravityAction()
	{
		SetNewtonianGravity(!GetNewtonianGravity());
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleAmbientHeatAction()
	{
		SetAmbientHeat(!GetAmbientHeat());
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleSimPausedAction()
	{
		SetSimPaused(!GetSimPaused());
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleDebugHudAction()
	{
		debugHud = !debugHud;
		// TODO-REDO_UI: info tip
	}

	void Game::ToggleFindAction()
	{
		auto fp = GetFindParameters();
		if (rendererSettings.findingElement == fp)
		{
			rendererSettings.findingElement = std::nullopt;
		}
		else
		{
			rendererSettings.findingElement = fp;
		}
		// TODO-REDO_UI: info tip
	}

	void Game::SetBrushIndex(BrushIndex newBrushIndex)
	{
		brushIndex = newBrushIndex;
		auto *brush = GetCurrentBrush();
		if (brush)
		{
			brush->SetRadius(brushRadius);
		}
	}

	void Game::BeginBrush(ToolSlotIndex toolSlotIndex, DrawMode drawMode)
	{
		auto m = GetMousePos();
		if (m && RES.OriginRect().Contains(*m))
		{
			CreateHistoryEntry();
			auto *tool = toolSlots[toolSlotIndex];
			if (tool->Identifier == "DEFAULT_UI_SAMPLE")
			{
				// TODO: have tools decide on draw mode
				drawMode = DrawMode::free;
			}
			drawState = DrawState{ drawMode, toolSlotIndex, *m };
			auto *brush = GetCurrentBrush();
			if (tool && brush)
			{
				auto *sim = simContext->simulation.get();
				tool->Strength = GetToolStrength();
				switch (drawMode)
				{
				case DrawMode::free:
					tool->Draw(sim, *brush, *m);
					break;

				case DrawMode::fill:
					tool->DrawFill(sim, *brush, *m);
					break;

				default:
					break;
				}
			}
		}
	}

	void Game::EndBrush(ToolSlotIndex toolSlotIndex)
	{
		if (drawState && drawState->toolSlotIndex == toolSlotIndex)
		{
			auto m = GetMousePos();
			auto *tool = toolSlots[toolSlotIndex];
			auto *brush = GetCurrentBrush();
			if (m && tool && brush)
			{
				auto *sim = simContext->simulation.get();
				tool->Strength = GetToolStrength();
				switch (drawState->mode)
				{
				case DrawMode::free:
					tool->DrawLine(sim, *brush, drawState->initialMousePos, *m, true);
					tool->Click(sim, *brush, *m);
					break;

				case DrawMode::line:
					tool->DrawLine(sim, *brush, drawState->initialMousePos, *m, false);
					break;

				case DrawMode::rect:
					tool->DrawRect(sim, *brush, drawState->initialMousePos, *m);
					break;

				case DrawMode::fill:
					tool->DrawFill(sim, *brush, *m);
					break;

				default:
					break;
				}
			}
			drawState.reset();
		}
	}

	Brush *Game::GetCurrentBrush()
	{
		if (brushIndex >= 0 && brushIndex < int32_t(brushes.size()))
		{
			return brushes[brushIndex].get();
		}
		return nullptr;
	}

	const Brush *Game::GetCurrentBrush() const
	{
		if (brushIndex >= 0 && brushIndex < int32_t(brushes.size()))
		{
			return brushes[brushIndex].get();
		}
		return nullptr;
	}

	void Game::VisualLog(std::string message)
	{
		// TODO-REDO_UI: make safe to call from sim contexts
		logEntries.push_back(LogEntry{ { GetHost(), Gui::TargetAnimation<int32_t>::LinearProfile{ 170 }, 0, 600 }, std::move(message) });
		while (int32_t(logEntries.size()) > maxLogEntries)
		{
			logEntries.pop_front();
		}
	}

	void Game::UndoHistoryEntryAction()
	{
		if (!UndoHistoryEntry())
		{
			VisualLog("Nothing left to undo"); // TODO-REDO_UI-TRANSLATE
		}
		// TODO-REDO_UI: info tip
	}

	void Game::RedoHistoryEntryAction()
	{
		if (!RedoHistoryEntry())
		{
			VisualLog("Nothing left to redo"); // TODO-REDO_UI-TRANSLATE
		}
		// TODO-REDO_UI: info tip
	}

	// * SnapshotDelta d is the difference between the two Snapshots A and B (i.e. d = B - A)
	//   if auto d = SnapshotDelta::FromSnapshots(A, B). In this case, a Snapshot that is
	//   identical to B can be constructed from d and A via d.Forward(A) (i.e. B = A + d)
	//   and a Snapshot that is identical to A can be constructed from d and B via
	//   d.Restore(B) (i.e. A = B - d). SnapshotDeltas often consume less memory than Snapshots,
	//   although pathological cases of pairs of Snapshots exist, the SnapshotDelta constructed
	//   from which actually consumes more than the two snapshots combined.
	// * .history is an N-item deque of HistoryEntry structs, each of which owns either
	//   a SnapshotDelta, except for history[N - 1], which always owns a Snapshot. A logical Snapshot
	//   accompanies each item in .history. This logical Snapshot may or may not be
	//   materialized (present in memory). If an item owns an actual Snapshot, the aforementioned
	//   logical Snapshot is this materialised Snapshot. If, however, an item owns a SnapshotDelta d,
	//   the accompanying logical Snapshot A is the Snapshot obtained via A = d.Restore(B), where B
	//   is the logical Snapshot accompanying the next (at an index that is one higher than the
	//   index of this item) item in history. Slightly more visually:
	//
	//      i   |    history[i]   |  the logical Snapshot   | relationships |
	//          |                 | accompanying history[i] |               |
	//   -------|-----------------|-------------------------|---------------|
	//          |                 |                         |               |
	//    N - 1 |   Snapshot A    |       Snapshot A        |            A  |
	//          |                 |                         |           /   |
	//    N - 2 | SnapshotDelta b |       Snapshot B        |  B+b=A   b-B  |
	//          |                 |                         |           /   |
	//    N - 3 | SnapshotDelta c |       Snapshot C        |  C+c=B   c-C  |
	//          |                 |                         |           /   |
	//    N - 4 | SnapshotDelta d |       Snapshot D        |  D+d=C   d-D  |
	//          |                 |                         |           /   |
	//     ...  |      ...        |          ...            |   ...    ...  |
	//
	// * .historyPosition is an integer in the closed range 0 to N, which is decremented
	//   by UndoHistoryEntry and incremented by RedoHistoryEntry, by 1 at a time.
	//   .historyCurrent "follows" historyPosition such that it always holds a Snapshot
	//   that is identical to the logical Snapshot of history[historyPosition], except when
	//   historyPosition = N, in which case it's empty. This following behaviour is achieved either
	//   by "stepping" historyCurrent by Redoing and Undoing it via the SnapshotDelta in
	//   history[historyPosition], cloning the Snapshot in history[historyPosition] into it if
	//   historyPosition = N - 1, or clearing if it historyPosition = N.
	// * .historyCurrent is lost when a new Snapshot item is pushed into .history.
	//   This item appears wherever historyPosition currently points, and every other item above it
	//   is deleted. If historyPosition is below N, this gets rid of the Snapshot in history[N - 1].
	//   Thus, N is set to historyPosition, after which the new Snapshot is pushed and historyPosition
	//   is incremented to the new N.
	// * Pushing a new Snapshot into the history is a bit involved:
	//   * If there are no history entries yet, the new Snapshot is simply placed into .history.
	//     From now on, we consider cases in which .history is originally not empty.
	//
	//     === after pushing Snapshot A' into the history
	//  
	//        i   |    history[i]   |  the logical Snapshot   | relationships |
	//            |                 | accompanying history[i] |               |
	//     -------|-----------------|-------------------------|---------------|
	//            |                 |                         |               |
	//        0   |   Snapshot A    |       Snapshot A        |            A  |
	//
	//   * If there were discarded history entries (i.e. the user decided to continue from some state
	//     which they arrived to via at least one Ctrl+Z), history[N-2] is a SnapshotDelta that when
	//     Redone with the logical Snapshot of history[N-2] yields the logical Snapshot of history[N-1]
	//     from before the new item was pushed. This is not what we want, so we replace it with a
	//     SnapshotDelta that is the difference between the logical Snapshot of history[N-2] and the
	//     Snapshot freshly placed in history[N-1].
	//
	//     === after pushing Snapshot A' into the history
	//  
	//        i   |    history[i]   |  the logical Snapshot   | relationships |
	//            |                 | accompanying history[i] |               |
	//     -------|-----------------|-------------------------|---------------|
	//            |                 |                         |               |
	//      N - 1 |   Snapshot A'   |       Snapshot A'       |            A' | b needs to be replaced with b',
	//            |                 |                         |           /   | B+b'=A'; otherwise we'd run
	//      N - 2 | SnapshotDelta b |       Snapshot B        |  B+b=A   b-B  | into problems when trying to
	//            |                 |                         |           /   | reconstruct B from A' and b
	//      N - 3 | SnapshotDelta c |       Snapshot C        |  C+c=B   c-C  | in UndoHistoryEntry.
	//            |                 |                         |           /   |
	//      N - 4 | SnapshotDelta d |       Snapshot D        |  D+d=C   d-D  |
	//            |                 |                         |           /   |
	//       ...  |      ...        |          ...            |   ...    ...  |
	//  
	//     === after replacing b with b'
	//  
	//        i   |    history[i]   |  the logical Snapshot   | relationships |
	//            |                 | accompanying history[i] |               |
	//     -------|-----------------|-------------------------|---------------|
	//            |                 |                         |               |
	//      N - 1 |   Snapshot A'   |       Snapshot A'       |            A' |
	//            |                 |                         |           /   |
	//      N - 2 | SnapshotDelta b'|       Snapshot B        | B+b'=A' b'-B  |
	//            |                 |                         |           /   |
	//      N - 3 | SnapshotDelta c |       Snapshot C        |  C+c=B   c-C  |
	//            |                 |                         |           /   |
	//      N - 4 | SnapshotDelta d |       Snapshot D        |  D+d=C   d-D  |
	//            |                 |                         |           /   |
	//       ...  |      ...        |          ...            |   ...    ...  |
	//  
	//   * If there weren't any discarded history entries, history[N-2] is now also a Snapshot. Since
	//     the freshly pushed Snapshot in history[N-1] should be the only Snapshot in history, this is
	//     replaced with the SnapshotDelta that is the difference between history[N-2] and the Snapshot
	//     freshly placed in history[N-1].
	//
	//     === after pushing Snapshot A' into the history
	//
	//        i   |    history[i]   |  the logical Snapshot   | relationships |
	//            |                 | accompanying history[i] |               |
	//     -------|-----------------|-------------------------|---------------|
	//            |                 |                         |               |
	//      N - 1 |   Snapshot A'   |       Snapshot A'       |            A' | A needs to be converted to a,
	//            |                 |                         |               | otherwise Snapshots would litter
	//      N - 1 |   Snapshot A    |       Snapshot A        |            A  | .history, which we
	//            |                 |                         |           /   | want to avoid because they
	//      N - 2 | SnapshotDelta b |       Snapshot B        |  B+b=A   b-B  | waste a ton of memory
	//            |                 |                         |           /   |
	//      N - 3 | SnapshotDelta c |       Snapshot C        |  C+c=B   c-C  |
	//            |                 |                         |           /   |
	//      N - 4 | SnapshotDelta d |       Snapshot D        |  D+d=C   d-D  |
	//            |                 |                         |           /   |
	//       ...  |      ...        |          ...            |   ...    ...  |
	//
	//     === after replacing A with a
	//
	//        i   |    history[i]   |  the logical Snapshot   | relationships |
	//            |                 | accompanying history[i] |               |
	//     -------|-----------------|-------------------------|---------------|
	//            |                 |                         |               |
	//      N - 1 |   Snapshot A'   |       Snapshot A'       |            A' |
	//            |                 |                         |           /   |
	//      N - 1 | SnapshotDelta a |       Snapshot A        |  A+a=A'  a-A  |
	//            |                 |                         |           /   |
	//      N - 2 | SnapshotDelta b |       Snapshot B        |  B+b=A   b-B  |
	//            |                 |                         |           /   |
	//      N - 3 | SnapshotDelta c |       Snapshot C        |  C+c=B   c-C  |
	//            |                 |                         |           /   |
	//      N - 4 | SnapshotDelta d |       Snapshot D        |  D+d=C   d-D  |
	//            |                 |                         |           /   |
	//       ...  |      ...        |          ...            |   ...    ...  |
	//
	//   * After all this, the front of the deque is truncated such that there are no more than
	//     undoHistoryLimit entries left. // TODO: allow specifying a maximum memory usage instead

	void Game::CreateHistoryEntry()
	{
		// * Calling HistorySnapshot means the user decided to use the current state and
		//   forfeit the option to go back to whatever they Ctrl+Z'd their way back from.
		beforeRestore.reset();
		auto last = simContext->simulation->CreateSnapshot();
		Snapshot *rebaseOnto = nullptr;
		if (historyPosition)
		{
			rebaseOnto = history.back().snap.get();
			if (historyPosition < int32_t(history.size()))
			{
				historyCurrent = history[historyPosition - 1].delta->Restore(*historyCurrent);
				rebaseOnto = historyCurrent.get();
			}
		}
		while (historyPosition < int32_t(history.size()))
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
		while (undoHistoryLimit < int32_t(history.size()))
		{
			history.pop_front();
			historyPosition -= 1;
		}
	}

	bool Game::UndoHistoryEntry()
	{
		if (!historyPosition)
		{
			return false;
		}
		// * When undoing for the first time since the last call to HistorySnapshot, save the current state.
		//   Ctrl+Y needs this in order to bring you back to the point right before your last Ctrl+Z, because
		//   the last history entry is what this Ctrl+Z brings you back to, not the current state.
		if (!beforeRestore)
		{
			beforeRestore = simContext->simulation->CreateSnapshot();
			beforeRestore->Authors = authors;
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
		auto &current = *historyCurrent;
		simContext->simulation->Restore(current);
		authors = current.Authors;
		return true;
	}

	bool Game::RedoHistoryEntry()
	{
		if (historyPosition >= int32_t(history.size()))
		{
			return false;
		}
		historyPosition += 1;
		if (historyPosition == int32_t(history.size()))
		{
			historyCurrent.reset();
		}
		else if (history[historyPosition].snap)
		{
			historyCurrent = std::make_unique<Snapshot>(*history[historyPosition].snap);
		}
		else
		{
			historyCurrent = history[historyPosition - 1].delta->Forward(*historyCurrent);
		}
		// * If gameModel has nothing more to give, we've Ctrl+Y'd our way back to the original
		//   state; restore this instead, then get rid of it.
		auto &current = historyCurrent ? *historyCurrent : *beforeRestore;
		simContext->simulation->Restore(current);
		authors = current.Authors;
		if (&current == beforeRestore.get())
		{
			beforeRestore.reset();
		}
		return true;
	}

	void Game::LoadGameSave(const GameSave &gameSave, bool includePressure)
	{
		ApplySaveParametersToSim(gameSave);
		auto *sim = simContext->simulation.get();
		sim->clear_sim();
		PauseRendererThread();
		renderer->ClearAccumulation();
		sim->Load(&gameSave, includePressure, { 0, 0 });
	}

	void Game::ReloadSimAction()
	{
		if (!ReloadSim(!loadWithoutPressure))
		{
			VisualLog("No save to reload from"); // TODO-REDO_UI-TRANSLATE
		}
		// TODO-REDO_UI: info tip
	}

	void Game::ClearSim()
	{
		CreateHistoryEntry();
		SetSave(std::nullopt, false);
	}

	void Game::SetSave(std::optional<Save> newSave, bool includePressure)
	{
		DismissIntroText();
		save = std::move(newSave);
		if (save)
		{
			std::visit([this, includePressure](const auto &save) {
				const auto &gameSave = *save.gameSave;
				LoadGameSave(gameSave, includePressure);
				auto newAuthors = gameSave.authors;
				if constexpr (std::is_same_v<std::remove_cvref_t<decltype(save)>, OnlineSave>)
				{
					// This save was created before logging existed
					// Add in the correct info
					if (newAuthors.empty())
					{
						newAuthors["type"]        = "save";
						newAuthors["id"]          = save.id;
						newAuthors["username"]    = save.author;
						newAuthors["title"]       = save.title;
						newAuthors["description"] = save.description;
						newAuthors["published"]   = int(save.published);
						newAuthors["date"]        = save.updatedDate;
					}
					// This save was probably just created, and we didn't know the ID when creating it
					// Update with the proper ID
					else if (newAuthors.get("id", -1) == 0 || newAuthors.get("id", -1) == -1)
					{
						newAuthors["id"] = save.id;
					}
				}
				authors = newAuthors;
			}, *save);
		}
		else
		{
			auto *sim = simContext->simulation.get();
			sim->gravityMode = GRAV_VERTICAL;
			sim->customGravityX = 0.0f;
			sim->customGravityY = 0.0f;
			sim->air->airMode = AIR_ON;
			sim->legacy_enable = false;
			sim->water_equal_test = false;
			sim->SetEdgeMode(defaultEdgeMode);
			sim->air->ambientAirTemp = defaultAmbientAirTemp;
			sim->clear_sim();
			PauseRendererThread();
			renderer->ClearAccumulation();
			authors.clear();
		}
	}

	bool Game::ReloadSim(bool includePressure)
	{
		if (!save)
		{
			return false;
		}
		CreateHistoryEntry();
		std::optional<Save> newSave;
		std::swap(save, newSave);
		SetSave(std::move(newSave), includePressure);
		return true;
	}

	void Game::ApplySaveParametersToSim(const GameSave &save)
	{
		auto *sim = simContext->simulation.get();
		SetSimPaused(save.paused | GetSimPaused());
		sim->gravityMode         = save.gravityMode;
		sim->customGravityX      = save.customGravityX;
		sim->customGravityY      = save.customGravityY;
		sim->air->airMode        = save.airMode;
		sim->air->ambientAirTemp = save.ambientAirTemp;
		sim->edgeMode            = save.edgeMode;
		sim->legacy_enable       = save.legacyEnable;
		sim->water_equal_test    = save.waterEEnabled;
		sim->aheat_enable        = save.aheatEnable;
		sim->frameCount          = save.frameCount;
		sim->ensureDeterminism   = save.ensureDeterminism;
		sim->EnableNewtonianGravity(save.gravityEnable);
		if (save.hasRngState)
		{
			sim->rng.state(save.rngState);
		}
		else
		{
			sim->rng = RNG();
		}
	}

	bool Game::GetSimPaused() const
	{
		return simContext->simulation->sys_pause;
	}

	void Game::SetSimPaused(bool newPaused)
	{
		auto *sim = simContext->simulation.get();
		if (!newPaused && sim->debug_nextToUpdate > 0)
		{
			auto message = ByteString::Build("Updated particles from #", sim->debug_nextToUpdate, " to end due to unpause");
			UpdateSimUpTo(NPART);
			VisualLog(message);
		}
		sim->sys_pause = newPaused;
	}

	bool Game::GetSandEffect() const
	{
		return simContext->simulation->pretty_powder;
	}

	void Game::SetSandEffect(bool newSandEffect)
	{
		simContext->simulation->pretty_powder = newSandEffect;
	}

	bool Game::GetNewtonianGravity() const
	{
		return bool(simContext->simulation->grav);
	}

	void Game::SetNewtonianGravity(bool newNewtonianGravity)
	{
		simContext->simulation->EnableNewtonianGravity(newNewtonianGravity);
	}

	bool Game::GetAmbientHeat() const
	{
		return simContext->simulation->aheat_enable;
	}

	void Game::SetAmbientHeat(bool newAmbientHeat)
	{
		simContext->simulation->aheat_enable = newAmbientHeat;
	}

	bool Game::GetHeat() const
	{
		return simContext->simulation->legacy_enable;
	}

	void Game::SetHeat(bool newHeat)
	{
		simContext->simulation->legacy_enable = newHeat;
	}

	AirMode Game::GetAirMode() const
	{
		return AirMode(simContext->simulation->air->airMode);
	}

	void Game::SetAirMode(AirMode newAirMode)
	{
		simContext->simulation->air->airMode = int32_t(newAirMode);
	}

	GravityMode Game::GetGravityMode() const
	{
		return GravityMode(simContext->simulation->gravityMode);
	}

	void Game::SetGravityMode(GravityMode newGravityMode)
	{
		simContext->simulation->gravityMode = int32_t(newGravityMode);
	}

	EdgeMode Game::GetEdgeMode() const
	{
		return EdgeMode(simContext->simulation->edgeMode);
	}

	void Game::SetEdgeMode(EdgeMode newEdgeMode)
	{
		simContext->simulation->SetEdgeMode(int32_t(newEdgeMode));
	}

	float Game::GetAmbientAirTemp() const
	{
		return simContext->simulation->air->ambientAirTemp;
	}

	void Game::SetAmbientAirTemp(float newAmbientAirTemp)
	{
		simContext->simulation->air->ambientAirTemp = newAmbientAirTemp;
	}

	bool Game::GetWaterEqualization() const
	{
		return simContext->simulation->water_equal_test;
	}

	void Game::SetWaterEqualization(bool newWaterEqualization)
	{
		simContext->simulation->water_equal_test = newWaterEqualization;
	}

	void Game::BeforeSim()
	{
		auto *sim = simContext->simulation.get();
		if (!sim->sys_pause || sim->framerender)
		{
			// TODO
		}
		sim->BeforeSim();
	}

	void Game::AfterSim()
	{
		simContext->simulation->AfterSim();
	}

	void Game::UpdateSimUpTo(int upToIndex)
	{
		auto *sim = simContext->simulation.get();
		if (upToIndex < sim->debug_nextToUpdate)
		{
			upToIndex = NPART;
		}
		if (sim->debug_nextToUpdate == 0)
		{
			BeforeSim();
		}
		sim->UpdateParticles(sim->debug_nextToUpdate, upToIndex);
		if (upToIndex < NPART)
		{
			sim->debug_nextToUpdate = upToIndex;
		}
		else
		{
			AfterSim();
			sim->debug_nextToUpdate = 0;
		}
	}

	void Game::CycleEdgeModeAction()
	{
		auto *sim = simContext->simulation.get();
		sim->SetEdgeMode(EdgeMode((int32_t(simContext->simulation->edgeMode) + 1) % int32_t(NUM_EDGEMODES)));
		defaultEdgeMode = EdgeMode(sim->edgeMode);
		switch (sim->edgeMode)
		{
		case EDGE_VOID : QueueInfoTip("Edge Mode: Void" ); break; // TODO-REDO_UI-TRANSLATE
		case EDGE_SOLID: QueueInfoTip("Edge Mode: Solid"); break; // TODO-REDO_UI-TRANSLATE
		case EDGE_LOOP : QueueInfoTip("Edge Mode: Loop" ); break; // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::CycleAirModeAction()
	{
		auto *sim = simContext->simulation.get();
		simContext->simulation->air->airMode = AirMode((int32_t(simContext->simulation->air->airMode) + 1) % int32_t(NUM_AIRMODES));
		switch (sim->air->airMode)
		{
		case AIR_ON         : QueueInfoTip("Air: On"          ); break; // TODO-REDO_UI-TRANSLATE
		case AIR_PRESSUREOFF: QueueInfoTip("Air: Pressure Off"); break; // TODO-REDO_UI-TRANSLATE
		case AIR_VELOCITYOFF: QueueInfoTip("Air: Velocity Off"); break; // TODO-REDO_UI-TRANSLATE
		case AIR_OFF        : QueueInfoTip("Air: Off"         ); break; // TODO-REDO_UI-TRANSLATE
		case AIR_NOUPDATE   : QueueInfoTip("Air: No Update"   ); break; // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::OpenElementSearch()
	{
		PushAboveThis(std::make_shared<ElementSearch>(*this));
	}

	void Game::OpenProperty()
	{
		static_cast<PropertyTool *>(GetToolFromIdentifier("DEFAULT_UI_PROPERTY"))->OpenWindow();
	}

	void Game::SetOnTop(bool newOnTop)
	{
		if (!newOnTop)
		{
			EndAllInputs();
			PauseRendererThread();
			PauseSimThread();
		}
	}

	bool Game::GuiToolButton(Gui::View &view, const GameToolInfo &info, SDL_Texture *externalToolAtlasTexture) const
	{
		auto &g = view.GetHost();
		view.SetSize(toolTextureDataSize.X + 4);
		view.SetSizeSecondary(toolTextureDataSize.Y + 4);
		auto rr = view.GetRect();
		if (info.texture)
		{
			SDL_Rect rectFrom;
			rectFrom.x = info.texture->toolAtlasPos.X;
			rectFrom.y = info.texture->toolAtlasPos.Y;
			rectFrom.w = toolTextureDataSize.X;
			rectFrom.h = toolTextureDataSize.Y;
			SDL_Rect rectTo;
			rectTo.x = rr.pos.X + 2;
			rectTo.y = rr.pos.Y + 2;
			rectTo.w = toolTextureDataSize.X;
			rectTo.h = toolTextureDataSize.Y;
			SdlAssertZero(SDL_RenderCopy(g.GetRenderer(), externalToolAtlasTexture, &rectFrom, &rectTo));
		}
		else
		{
			g.FillRect(rr.Inset(2), info.tool->Colour.WithAlpha(255));
			view.BeginText(0, info.tool->Name.ToUtf8(), Gui::View::TextFlags::none, GetContrastColor2(info.tool->Colour).WithAlpha(255)); // TODO-REDO_UI-TRANSLATE: maybe
			view.SetTextPadding(4); // Triggers the weird negative alignment behaviour in View::BeginText, see TruncateDiv explanation there.
			view.EndText();
		}
		bool selected = false;
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			if (toolSlots[i] == info.tool.get())
			{
				g.DrawRect(rr, toolSlotColors[i].WithAlpha(255));
				selected = true;
				break;
			}
		}
		if (view.IsHovered())
		{
			if (!selected)
			{
				g.DrawRect(rr, toolSlotColors[0].WithAlpha(255));
			}
			return true;
		}
		return false;
	}

	void Game::DoSimFrameStep()
	{
		simContext->simulation->framerender += 1;
		SetSimPaused(true);
	}

	std::optional<FindingElement> Game::GetFindParameters() const
	{
		Tool *active = toolSlots[0];
		if (!active)
		{
			return std::nullopt;
		}
		auto &properties = Particle::GetProperties();
		if (active->Identifier.Contains("_PT_"))
		{
			return FindingElement{ properties[FIELD_TYPE], active->ToolID };
		}
		else if (active->Identifier == "DEFAULT_UI_PROPERTY")
		{
			auto configuration = static_cast<PropertyTool *>(active)->GetConfiguration();
			if (configuration)
			{
				return FindingElement{ properties[configuration->changeProperty.propertyIndex], configuration->changeProperty.propertyValue };
			}
		}
		return std::nullopt;
	}

	void Game::GrowGridAction()
	{
		rendererSettings.gridSize = FloorMod(rendererSettings.gridSize + 1, 10);
	}

	void Game::ShrinkGridAction()
	{
		rendererSettings.gridSize = FloorMod(rendererSettings.gridSize - 1, 10);
	}

	void Game::ToggleGravityGridAction()
	{
		rendererSettings.gravityFieldEnabled = !rendererSettings.gravityFieldEnabled;
	}

	void Game::ToggleIntroTextAction()
	{
		introTextAlpha.SetValue(introTextAlpha.GetValue() ? 0 : initialIntroTextAlpha);
	}

	void Game::ToggleHudAction()
	{
		hud = !hud;
	}

	void Game::ToggleDecoAction()
	{
		if (rendererSettings.decorationLevel == RendererSettings::decorationEnabled)
		{
			rendererSettings.decorationLevel = RendererSettings::decorationDisabled;
			QueueInfoTip("Decorations Layer: Off"); // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			rendererSettings.decorationLevel = RendererSettings::decorationEnabled;
			QueueInfoTip("Decorations Layer: On"); // TODO-REDO_UI-TRANSLATE
		}
	}

	void Game::ToggleDecoMenuAction()
	{
		if (GetShowingDecoTools())
		{
			activeMenuSection = activeMenuSectionBeforeDeco;
		}
		else
		{
			activeMenuSection = SC_DECO;
			rendererSettings.decorationLevel = RendererSettings::decorationEnabled;
			SetSimPaused(true);
		}
	}

	void Game::ResetAmbientHeatAction()
	{
		simContext->simulation->air->ClearAirH();
	}

	void Game::ResetAirAction()
	{
		auto *sim = simContext->simulation.get();
		sim->air->Clear();
		for (int i = 0; i < NPART; ++i)
		{
			if (GameSave::PressureInTmp3(sim->parts[i].type))
			{
				sim->parts[i].tmp3 = 0;
			}
		}
	}

	void Game::ResetSparkAction()
	{
		auto &sd = SimulationData::CRef();
		auto *sim = simContext->simulation.get();
		for (int i = 0; i < NPART; ++i)
		{
			if (sim->parts[i].type == PT_SPRK)
			{
				if (sim->parts[i].ctype >= 0 && sim->parts[i].ctype < PT_NUM && sd.elements[sim->parts[i].ctype].Enabled)
				{
					sim->parts[i].type = sim->parts[i].ctype;
					sim->parts[i].ctype = sim->parts[i].life = 0;
				}
				else
				{
					sim->kill_part(i);
				}
			}
		}
		std::memset(sim->wireless, 0, sizeof(sim->wireless));
	}

	void Game::InvertAirAction()
	{
		simContext->simulation->air->Invert();
	}

	void Game::ToggleReplaceModeFlag(int flag)
	{
		auto prev = bool(simContext->simulation->replaceModeFlags & flag);
		simContext->simulation->replaceModeFlags &= ~(REPLACE_MODE | SPECIFIC_DELETE);
		if (!prev)
		{
			simContext->simulation->replaceModeFlags |= flag;
		}
	}

	void Game::ToggleReplaceAction()
	{
		ToggleReplaceModeFlag(REPLACE_MODE);
	}

	void Game::ToggleSdeleteAction()
	{
		ToggleReplaceModeFlag(SPECIFIC_DELETE);
	}

	bool Game::GetShowingDecoTools() const
	{
		return activeMenuSection == SC_DECO;
	}

	std::optional<Rgb8> Game::GetColorUnderMouse() const
	{
		if (auto m = GetMousePos())
		{
			if (rendererFrame->Size().OriginRect().Contains(*m))
			{
				return Rgb8::Unpack((*rendererFrame)[*m]);
			}
		}
		return std::nullopt;
	}

	void Game::UseRendererPreset(int index)
	{
		auto &preset = Renderer::renderModePresets[index];
		rendererSettings.renderMode        = preset.renderMode;
		rendererSettings.displayMode       = preset.displayMode;
		rendererSettings.colorMode         = preset.colorMode;
		rendererSettings.wantHdispLimitMin = preset.wantHdispLimitMin;
		rendererSettings.wantHdispLimitMax = preset.wantHdispLimitMax;
	}

	void Game::OpenOnlineBrowser()
	{
		GetHost().SetStack(onlineBrowserStack);
		onlineBrowser->Open();
	}
}
