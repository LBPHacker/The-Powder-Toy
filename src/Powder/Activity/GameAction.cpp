#include "Game.hpp"
#include "Gui/SdlAssert.hpp"
#include "Common/Log.hpp"
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "client/GameSave.h"
#include "gui/game/tool/Tool.h"

namespace Powder::Activity
{
	namespace
	{
		const std::string actionNamePrefix        = "DEFAULT_ACTION_";
		const std::string actionGroupNamePrefix   = "DEFAULT_ACTIONGROUP_";
		const std::string actionContextNamePrefix = "DEFAULT_ACTIONCONTEXT_";

		struct ModifierNotation
		{
			std::set<Game::Input> modifiers;
		};

		struct ScancodeWithModifiers
		{
			int32_t scancode;
			std::set<Game::Input> modifiers;

			ScancodeWithModifiers(int32_t newScancode) : scancode(newScancode)
			{
			}
		};

		ScancodeWithModifiers operator +(const ModifierNotation &mn, ScancodeWithModifiers swm)
		{
			swm.modifiers.insert(mn.modifiers.begin(), mn.modifiers.end());
			return swm;
		}

		ModifierNotation operator +(const ModifierNotation &mnl, ModifierNotation mnr)
		{
			mnr.modifiers.insert(mnl.modifiers.begin(), mnl.modifiers.end());
			return mnr;
		}

		const auto shift = ModifierNotation{ { Game::KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } } }; // TODO-REDO_UI: somehow allow right side key too
		const auto ctrl  = ModifierNotation{ { Game::KeyboardKeyInput{ SDL_SCANCODE_LCTRL  } } };
		const auto alt   = ModifierNotation{ { Game::KeyboardKeyInput{ SDL_SCANCODE_LALT   } } };
	}

	void Game::InitActions()
	{
		auto makeActionGroup = [&](std::string name) {
			auto group = std::make_shared<ActionGroup>();
			group->name = actionGroupNamePrefix + name;
			group->displayIndex = int32_t(shortcutMapperInfo.groups.size());
			shortcutMapperInfo.groups.push_back(group);
			return group;
		};
		auto makeAction = [&](std::string name, std::shared_ptr<ActionGroup> group, std::vector<std::shared_ptr<Action>> modifierFor = {}) {
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + name;
			action->displayIndex = int32_t(shortcutMapperInfo.actions.size());
			action->group = group;
			action->modifierFor = modifierFor;
			shortcutMapperInfo.actions.push_back(action);
			return action;
		};
		auto makeActionContext = [&](std::string name, std::shared_ptr<ActionContext> parentContext) {
			auto context = std::make_shared<ActionContext>();
			context->name = actionContextNamePrefix + name;
			context->parentContext = parentContext;
			shortcutMapperInfo.contexts.push_back(context);
			return context;
		};
		rootContext      = makeActionContext("ROOT"     , nullptr    );
		placeZoomContext = makeActionContext("PLACEZOOM", rootContext);
		selectContext    = makeActionContext("SELECT"   , rootContext);
		pasteContext     = makeActionContext("PASTE"    , rootContext);
		auto drawingGroup        = makeActionGroup("DRAWING");
		auto brushGroup          = makeActionGroup("BRUSH");
		auto simGroup            = makeActionGroup("SIM");
		auto rendererpresetGroup = makeActionGroup("RENDERERPRESET");
		auto historyGroup        = makeActionGroup("HISTORY");
		auto stampGroup          = makeActionGroup("STAMP");
		auto clipboardGroup      = makeActionGroup("CLIPBOARD");
		auto renderGroup         = makeActionGroup("RENDER");
		auto toolGroup           = makeActionGroup("TOOL");
		auto miscGroup           = makeActionGroup("MISC");
		auto placezoomctxGroup   = makeActionGroup("PLACEZOOMCTX");
		auto selectctxGroup      = makeActionGroup("SELECTCTX");
		auto pastectxGroup       = makeActionGroup("PASTECTX");

		auto initRootToggle = [&](std::string name, std::shared_ptr<ActionGroup> group, auto member, std::set<Input> modifiers, std::vector<std::shared_ptr<Action>> modifierFor) {
			auto action = makeAction(name, group, modifierFor);
			action->begin = [this, member]() {
				this->*member = true;
				return InputDisposition::shared;
			};
			action->end = [this, member]() {
				this->*member = false;
			};
			rootContext->inputToActions.push_back({ ModifierOnly{}, modifiers, action });
		};

		{
			auto initOne = [&](std::string name, std::shared_ptr<ActionGroup> group, auto member, ScancodeWithModifiers swm) {
				auto [ scancode, modifiers ] = swm;
				auto action = makeAction(name, group);
				action->begin = [this, member]() {
					(this->*member)();
					return InputDisposition::exclusive;
				};
				rootContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
				struct InitOther
				{
					Game &game;
					std::shared_ptr<Action> action;

					InitOther AddAlternative(ScancodeWithModifiers swm)
					{
						auto [ scancode, modifiers ] = swm;
						game.rootContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
						return *this;
					}
				};
				return InitOther{ *this, action };
			};
			initOne("CYCLEBRUSH"       , brushGroup    , &Game::CycleBrushAction       , SDL_SCANCODE_TAB             );
			initOne("TOGGLEAMBIENTHEAT", simGroup      , &Game::ToggleAmbientHeatAction, SDL_SCANCODE_U               );
			initOne("TOGGLEDRAWGRAVITY", renderGroup   , &Game::ToggleDrawGravityAction, ctrl + SDL_SCANCODE_G        );
			initOne("TOGGLEPAUSE"      , simGroup      , &Game::ToggleSimPausedAction  , SDL_SCANCODE_SPACE           );
			initOne("TOGGLEDEBUGHUD"   , renderGroup   , &Game::ToggleDebugHudAction   , SDL_SCANCODE_D               )
			.AddAlternative(                                                             SDL_SCANCODE_F3              );
			initOne("UNDOHISTORYENTRY" , historyGroup  , &Game::UndoHistoryEntryAction , ctrl + SDL_SCANCODE_Z        );
			initOne("REDOHISTORYENTRY" , historyGroup  , &Game::RedoHistoryEntryAction , ctrl + shift + SDL_SCANCODE_Z)
			.AddAlternative(                                                             ctrl + SDL_SCANCODE_Y        );
			initOne("RELOADSIM"        , simGroup      , &Game::ReloadSimAction        , SDL_SCANCODE_F5              )
			.AddAlternative(                                                             ctrl + SDL_SCANCODE_R        );
			initOne("CYCLEEDGEMODE"    , simGroup      , &Game::CycleEdgeModeAction    , ctrl + SDL_SCANCODE_E        );
			initOne("ELEMENTSEARCH"    , toolGroup     , &Game::OpenElementSearch      , SDL_SCANCODE_E               );
			initOne("FRAMESTEP"        , simGroup      , &Game::DoSimFrameStep         , SDL_SCANCODE_F               );
			initOne("TOGGLEFIND"       , renderGroup   , &Game::ToggleFindAction       , ctrl + SDL_SCANCODE_F        );
			initOne("GROWGRID"         , renderGroup   , &Game::GrowGridAction         , SDL_SCANCODE_G               );
			initOne("SHRINKGRID"       , renderGroup   , &Game::ShrinkGridAction       , shift + SDL_SCANCODE_G       );
			initOne("TOGGLEGINTROTEXT" , renderGroup   , &Game::ToggleIntroTextAction  , SDL_SCANCODE_F1              )
			.AddAlternative(                                                             ctrl + SDL_SCANCODE_H        );
			initOne("TOGGLEHUD"        , renderGroup   , &Game::ToggleHudAction        , SDL_SCANCODE_H               );
			initOne("TOGGLEDECO"       , renderGroup   , &Game::ToggleDrawDecoAction   , ctrl + SDL_SCANCODE_B        );
			initOne("TOGGLEDECOMENU"   , toolGroup     , &Game::ToggleDecoMenuAction   , SDL_SCANCODE_B               );
			initOne("CYCLEAIRMODE"     , simGroup      , &Game::CycleAirModeAction     , SDL_SCANCODE_Y               );
			initOne("RESETAMBIENTHEAT" , simGroup      , &Game::ResetAmbientHeatAction , ctrl + SDL_SCANCODE_U        );
			initOne("TOGGLENEWTONIAN"  , simGroup      , &Game::ToggleNewtonianAction  , SDL_SCANCODE_N               );
			initOne("RESETAIR"         , simGroup      , &Game::ResetAirAction         , SDL_SCANCODE_EQUALS          );
			initOne("RESETSPARK"       , simGroup      , &Game::ResetSparkAction       , ctrl + SDL_SCANCODE_EQUALS   );
			initOne("INVERTAIR"        , simGroup      , &Game::InvertAirAction        , SDL_SCANCODE_I               );
			initOne("TOGGLEREPLACE"    , toolGroup     , &Game::ToggleReplaceAction    , SDL_SCANCODE_SEMICOLON       )
			.AddAlternative(                                                             SDL_SCANCODE_INSERT          );
			initOne("TOGGLESDELETE"    , toolGroup, &Game::ToggleSdeleteAction         , ctrl + SDL_SCANCODE_SEMICOLON)
			.AddAlternative(                                                             ctrl + SDL_SCANCODE_INSERT   )
			.AddAlternative(                                                             SDL_SCANCODE_DELETE          );
			initOne("OPENPROPERTY"     , toolGroup, &Game::OpenProperty                , ctrl + SDL_SCANCODE_P        )
			.AddAlternative(                                                             ctrl + SDL_SCANCODE_F2       );
			initOne("PICKSTAMP"        , stampGroup    , &Game::OpenStampBrowser       , SDL_SCANCODE_K               );
			initOne("OPENCONSOLE"      , miscGroup     , &Game::OpenConsoleAction      , SDL_SCANCODE_GRAVE           );
			initOne("INSTALL"          , miscGroup     , &Game::InstallAction          , ctrl + SDL_SCANCODE_I        );
			initOne("PASTE"            , clipboardGroup, &Game::PasteAction            , ctrl + SDL_SCANCODE_V        );
			initOne("COPY"             , clipboardGroup, &Game::CopyAction             , ctrl + SDL_SCANCODE_C        );
			initOne("CUT"              , clipboardGroup, &Game::CutAction              , ctrl + SDL_SCANCODE_X        );
			initOne("STAMP"            , stampGroup    , &Game::StampAction            , SDL_SCANCODE_S               );
			initOne("LOADLASTSTAMP"    , stampGroup    , &Game::LoadLastStampAction    , SDL_SCANCODE_L               );
			initOne("TOGGLEFULLSCREEN" , miscGroup     , &Game::ToggleFullscreenAction , SDL_SCANCODE_F11             );
			initOne("SCREENSHOT"       , miscGroup     , &Game::TakeScreenshotAction   , SDL_SCANCODE_P               )
			.AddAlternative(                                                             SDL_SCANCODE_F2              );
		}

		auto initRendererPreset = [&](int32_t index, ScancodeWithModifiers swm) {
			auto [ scancode, modifiers ] = swm;
			auto action = makeAction(ByteString::Build("RENDERERPRESET", index), rendererpresetGroup);
			action->begin = [this, index]() {
				UseRendererPreset(index);
				QueueInfoTip(Renderer::renderModePresets[index].Name.ToUtf8());
				return InputDisposition::exclusive;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
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
		initRendererPreset(10, shift + SDL_SCANCODE_1);
		initRendererPreset(11, shift + SDL_SCANCODE_6);

		{
			auto action = makeAction("SAVEBUTTONSALTFUNCTION", miscGroup);
			action->begin = [this]() {
				saveButtonsAltFunction = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				saveButtonsAltFunction = false;
			};
			rootContext->inputToActions.push_back({ ModifierOnly{}, { KeyboardKeyInput{ SDL_SCANCODE_LCTRL } }, action });
		}
		{
			auto action = makeAction("INVERTINCLUDEPRESSURE", miscGroup);
			action->begin = [this]() {
				invertIncludePressure = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				invertIncludePressure = false;
			};
			rootContext->inputToActions.push_back({ ModifierOnly{}, { KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } }, action });
		}
		{
			auto action = makeAction("SAMPLEPROPERTY", toolGroup);
			action->begin = [this]() {
				sampleProperty = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				sampleProperty = false;
			};
			rootContext->inputToActions.push_back({ ModifierOnly{}, { KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } }, action });
		}
		{
			auto action = makeAction("TOOLSTRENGTHMUL10", toolGroup);
			action->begin = [this]() {
				toolStrengthMul10 = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				toolStrengthMul10 = false;
			};
			rootContext->inputToActions.push_back({ ModifierOnly{}, { KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } }, action });
		}
		{
			auto action = makeAction("TOOLSTRENGTHDIV10", toolGroup);
			action->begin = [this]() {
				toolStrengthDiv10 = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				toolStrengthDiv10 = false;
			};
			rootContext->inputToActions.push_back({ ModifierOnly{}, { KeyboardKeyInput{ SDL_SCANCODE_LCTRL } }, action });
		}

		std::vector<std::shared_ptr<Action>> toolsizeActions;
		auto initToolsize = [&](std::string name, int32_t scancode, MouseWheelInput::Direction direction, int32_t diff) {
			auto action = makeAction(name, brushGroup);
			toolsizeActions.push_back(action);
			action->begin = [this, diff]() {
				auto diffX = toolSizeConstrainX ? 0 : diff;
				auto diffY = toolSizeConstrainY ? 0 : diff;
				if (GetCurrentActionContext() == placeZoomContext.get())
				{
					AdjustZoomSize(diffX, diffY, !toolSizeLinear);
				}
				AdjustBrushSize(diffX, diffY, !toolSizeLinear);
				return InputDisposition::exclusive;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, {}, action });
			rootContext->inputToActions.push_back({ MouseWheelInput{ direction }, {}, action });
			return action;
		};
		initToolsize("TOOLSIZEUP", SDL_SCANCODE_RIGHTBRACKET, MouseWheelInput::Direction::positiveY, 1);
		initToolsize("TOOLSIZEDOWN", SDL_SCANCODE_LEFTBRACKET, MouseWheelInput::Direction::negativeY, -1);
		initRootToggle("TOOLSIZELINEAR", brushGroup, &Game::toolSizeLinear, { KeyboardKeyInput{ SDL_SCANCODE_LALT } }, toolsizeActions);
		initRootToggle("TOOLSIZECONSTRAINX", brushGroup, &Game::toolSizeConstrainX, { KeyboardKeyInput{ SDL_SCANCODE_LCTRL } }, toolsizeActions);
		initRootToggle("TOOLSIZECONSTRAINY", brushGroup, &Game::toolSizeConstrainY, { KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } }, toolsizeActions);

		std::vector<std::shared_ptr<Action>> drawActions;
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			auto action = makeAction(ByteString::Build("DRAW", i), drawingGroup);
			drawActions.push_back(action);
			action->begin = [this, i]() {
				if (lastHoveredTool)
				{
					SelectTool(i, lastHoveredTool);
					return InputDisposition::exclusive;
				}
				if (ClickSign(i))
				{
					return InputDisposition::exclusive;
				}
				if (!GetSimMousePos())
				{
					return InputDisposition::unhandled;
				}
				if (brushModeFill)
				{
					BeginBrush(i, DrawMode::fill);
				}
				else if (brushModeRect)
				{
					BeginBrush(i, DrawMode::rect);
				}
				else if (brushModeLine)
				{
					BeginBrush(i, DrawMode::line);
				}
				else
				{
					BeginBrush(i, DrawMode::free);
				}
				return InputDisposition::exclusive;
			};
			action->end = [this, i]() {
				EndBrush(i);
			};
			std::optional<int32_t> button;
			switch (i)
			{
			case 0: button = SDL_BUTTON_LEFT  ; break;
			case 1: button = SDL_BUTTON_RIGHT ; break;
			case 2: button = SDL_BUTTON_MIDDLE; break;
			}
			if (!button)
			{
				continue;
			}
			rootContext->inputToActions.push_back({ MouseButtonInput{ *button }, {}, action });
		}
		initRootToggle("DRAWLINE", drawingGroup, &Game::brushModeLine, { KeyboardKeyInput{ SDL_SCANCODE_LSHIFT } }, drawActions);
		initRootToggle("DRAWRECT", drawingGroup, &Game::brushModeRect, { KeyboardKeyInput{ SDL_SCANCODE_LCTRL } }, drawActions);
		initRootToggle("DRAWFILL", drawingGroup, &Game::brushModeFill, { KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, KeyboardKeyInput{ SDL_SCANCODE_LCTRL } }, drawActions);

		{
			auto action = makeAction("TOGGLEFAVORITE", toolGroup);
			action->begin = [this]() {
				if (lastHoveredTool)
				{
					ToggleFavorite(lastHoveredTool);
					return InputDisposition::exclusive;
				}
				return InputDisposition::unhandled;
			};
			rootContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_LEFT }, (shift + ctrl).modifiers, action });
		}
		{
			auto action = makeAction("REMOVECUSTOMGOL", toolGroup);
			action->begin = [this]() {
				if (lastHoveredTool && lastHoveredTool->Identifier.BeginsWith("DEFAULT_PT_LIFECUST_"))
				{
					RemoveCustomGol(lastHoveredTool->Identifier);
					return InputDisposition::exclusive;
				}
				return InputDisposition::unhandled;
			};
			rootContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_RIGHT }, (shift + ctrl).modifiers, action });
		}

		{
			auto placeZoomAction = makeAction("PLACEZOOM", drawingGroup);
			placeZoomAction->begin = [this]() {
				zoomShown = true;
				zoomMetrics = MakeZoomMetrics(zoomMetrics.from.size);
				SetCurrentActionContext(placeZoomContext);
				return InputDisposition::exclusive;
			};
			placeZoomAction->end = [this]() {
				if (GetCurrentActionContext() == placeZoomContext.get())
				{
					SetCurrentActionContext(rootContext);
					zoomShown = false;
				}
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_Z }, {}, placeZoomAction });
			auto finalizeZoomAction = makeAction("FINALIZEZOOM", placezoomctxGroup);
			finalizeZoomAction->begin = [this, placeZoomAction]() {
				zoomMetrics = MakeZoomMetrics(zoomMetrics.from.size);
				if (GetCurrentActionContext() == placeZoomContext.get())
				{
					SetCurrentActionContext(rootContext);
				}
				EndAction(placeZoomAction.get());
				return InputDisposition::exclusive;
			};
			placeZoomContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_LEFT }, {}, finalizeZoomAction });
		}

		{
			selectFinish = makeAction("SELECTFINISH", selectctxGroup);
			selectFinish->begin = [this]() {
				if (auto p = GetSimMousePos())
				{
					selectFirstPos = *p;
					return InputDisposition::exclusive;
				}
				return InputDisposition::unhandled;
			};
			selectFinish->end = [this]() {
				if (GetCurrentActionContext() == selectContext.get())
				{
					if (auto p = GetSimMousePos())
					{
						EndSelectAction(*selectFirstPos, *p, *selectMode, GetIncludePressure());
					}
					SetCurrentActionContext(rootContext);
				}
			};
			auto cancelAction = makeAction("SELECTCANCEL", selectctxGroup);
			cancelAction->begin = [this]() {
				if (GetCurrentActionContext() == selectContext.get())
				{
					SetCurrentActionContext(rootContext);
				}
				return InputDisposition::exclusive;
			};
			selectContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_LEFT }, {}, selectFinish });
			selectContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_RIGHT }, {}, cancelAction });
			selectContext->end = [this]() {
				selectFirstPos.reset();
				selectMode.reset();
			};
		}

		{
			pasteFinish = makeAction("PASTEFINISH", pastectxGroup);
			pasteFinish->end = [this]() {
				if (GetCurrentActionContext() == pasteContext.get())
				{
					if (auto p = GetSimMousePos())
					{
						EndPaste(*p, GetIncludePressure());
					}
					SetCurrentActionContext(rootContext);
				}
			};
			auto cancelAction = makeAction("PASTECANCEL", pastectxGroup);
			cancelAction->begin = [this]() {
				if (GetCurrentActionContext() == pasteContext.get())
				{
					SetCurrentActionContext(rootContext);
				}
				return InputDisposition::exclusive;
			};
			pasteContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_LEFT }, {}, pasteFinish });
			pasteContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_RIGHT }, {}, cancelAction });
			auto initOne = [&](std::string name, ScancodeWithModifiers swm, auto func) {
				auto [ scancode, modifiers ] = swm;
				auto action = makeAction(name, pastectxGroup);
				action->begin = [func]() {
					func();
					return InputDisposition::exclusive;
				};
				pasteContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
			};
			initOne("PASTEROTATE", SDL_SCANCODE_R               , [this]() { TransformPasteSave(Mat2x2::CCW    ); });
			initOne("PASTEHFLIP" , shift + SDL_SCANCODE_R       , [this]() { TransformPasteSave(Mat2x2::MirrorX); });
			initOne("PASTEVFLIP" , shift + ctrl + SDL_SCANCODE_R, [this]() { TransformPasteSave(Mat2x2::MirrorY); });
			initOne("PASTERIGHT" , SDL_SCANCODE_RIGHT           , [this]() { TranslatePasteSave({       1,  0 }); });
			initOne("PASTELEFT"  , SDL_SCANCODE_LEFT            , [this]() { TranslatePasteSave({      -1,  0 }); });
			initOne("PASTEUP"    , SDL_SCANCODE_UP              , [this]() { TranslatePasteSave({       0, -1 }); });
			initOne("PASTEDOWN"  , SDL_SCANCODE_DOWN            , [this]() { TranslatePasteSave({       0,  1 }); });
			pasteContext->end = [this]() {
				pasteSave.reset();
			};
		}

		SetCurrentActionContext(rootContext);
	}
}
