#include "Game.hpp"
#include "Gui/SdlAssert.hpp"
#include "Common/Log.hpp"
#include "graphics/Renderer.h"
#include "graphics/VideoBuffer.h"
#include "client/GameSave.h"
#include "gui/game/tool/Tool.h"

namespace Powder::Activity
{
	void Game::InitActions()
	{
		std::string actionContextNamePrefix = "DEFAULT_ACTIONCONTEXT_";
		auto makeActionContext = [actionContextNamePrefix](std::string name, std::shared_ptr<ActionContext> parentContext) {
			auto context = std::make_shared<ActionContext>();
			context->name = actionContextNamePrefix + name;
			context->parentContext = parentContext;
			return context;
		};
		rootContext      = makeActionContext("ROOT"     , nullptr    );
		placeZoomContext = makeActionContext("PLACEZOOM", rootContext);
		selectContext    = makeActionContext("SELECT"   , rootContext);
		pasteContext     = makeActionContext("PASTE"    , rootContext);

		std::string actionNamePrefix = "DEFAULT_ACTION_";
		auto shiftInput = KeyboardKeyInput{ SDL_SCANCODE_LSHIFT };
		auto ctrlInput  = KeyboardKeyInput{ SDL_SCANCODE_LCTRL };
		auto altInput   = KeyboardKeyInput{ SDL_SCANCODE_LALT };

		{
			auto initSimple = [this, actionNamePrefix](std::string name, auto member, int32_t scancode, std::set<Input> modifiers = {}) {
				auto action = std::make_shared<Action>();
				action->name = actionNamePrefix + name;
				action->begin = [this, member]() {
					(this->*member)();
					return InputDisposition::exclusive;
				};
				rootContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
				return action;
			};
			auto initSimpleAlt = [this, actionNamePrefix](std::shared_ptr<Action> action, int32_t scancode, std::set<Input> modifiers = {}) {
				rootContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
			};
			                        initSimple("CYCLEBRUSH"            , &Game::CycleBrushAction            , SDL_SCANCODE_TAB      );
			                        initSimple("TOGGLEAMBIENTHEAT"     , &Game::ToggleAmbientHeatAction     , SDL_SCANCODE_U        );
			                        initSimple("TOGGLEDRAWDECO"        , &Game::ToggleDrawDecoAction        , SDL_SCANCODE_B        , { ctrlInput });
			                        initSimple("TOGGLEDRAWGRAVITY"     , &Game::ToggleDrawGravityAction     , SDL_SCANCODE_G        , { ctrlInput });
			                        initSimple("TOGGLEPAUSE"           , &Game::ToggleSimPausedAction       , SDL_SCANCODE_SPACE    );
			auto toggleDebugHud   = initSimple("TOGGLEDEBUGHUD"        , &Game::ToggleDebugHudAction        , SDL_SCANCODE_D        );
			                        initSimpleAlt(toggleDebugHud                                            , SDL_SCANCODE_F3       );
			                        initSimple("UNDOHISTORYENTRY"      , &Game::UndoHistoryEntryAction      , SDL_SCANCODE_Z        , { ctrlInput });
			auto redoHistoryEntry = initSimple("REDOHISTORYENTRY"      , &Game::RedoHistoryEntryAction      , SDL_SCANCODE_Z        , { ctrlInput, shiftInput });
			                        initSimpleAlt(redoHistoryEntry                                          , SDL_SCANCODE_Y        , { ctrlInput });
			auto reloadSim        = initSimple("RELOADSIM"             , &Game::ReloadSimAction             , SDL_SCANCODE_F5       );
			                        initSimpleAlt(reloadSim                                                 , SDL_SCANCODE_R        , { ctrlInput });
			                        initSimple("CYCLEEDGEMODE"         , &Game::CycleEdgeModeAction         , SDL_SCANCODE_E        , { ctrlInput });
			                        initSimple("ELEMENTSEARCH"         , &Game::OpenElementSearch           , SDL_SCANCODE_E        );
			                        initSimple("FRAMESTEP"             , &Game::DoSimFrameStep              , SDL_SCANCODE_F        );
			                        initSimple("TOGGLEFIND"            , &Game::ToggleFindAction            , SDL_SCANCODE_F        , { ctrlInput });
			                        initSimple("GROWGRID"              , &Game::GrowGridAction              , SDL_SCANCODE_G        );
			                        initSimple("SHRINKGRID"            , &Game::ShrinkGridAction            , SDL_SCANCODE_G        , { shiftInput });
			                        initSimple("TOGGLEGRAVITYGRID"     , &Game::ToggleGravityGridAction     , SDL_SCANCODE_G        , { ctrlInput });
			auto toggleIntroText  = initSimple("TOGGLEGINTROTEXT"      , &Game::ToggleIntroTextAction       , SDL_SCANCODE_F1       );
			                        initSimpleAlt(toggleIntroText                                           , SDL_SCANCODE_H        , { ctrlInput });
			                        initSimple("TOGGLEHUD"             , &Game::ToggleHudAction             , SDL_SCANCODE_H        );
			                        initSimple("TOGGLEDECO"            , &Game::ToggleDecoAction            , SDL_SCANCODE_B        , { ctrlInput });
			                        initSimple("TOGGLEDECOMENU"        , &Game::ToggleDecoMenuAction        , SDL_SCANCODE_B        );
			                        initSimple("CYCLEAIRMODE"          , &Game::CycleAirModeAction          , SDL_SCANCODE_Y        );
			                        initSimple("RESETAMBIENTHEAT"      , &Game::ResetAmbientHeatAction      , SDL_SCANCODE_U        , { ctrlInput });
			                        initSimple("TOGGLENEWTONIANGRAVITY", &Game::ToggleNewtonianGravityAction, SDL_SCANCODE_N        );
			                        initSimple("RESETAIR"              , &Game::ResetAirAction              , SDL_SCANCODE_EQUALS   );
			                        initSimple("RESETSPARK"            , &Game::ResetSparkAction            , SDL_SCANCODE_EQUALS   , { ctrlInput });
			                        initSimple("INVERTAIR"             , &Game::InvertAirAction             , SDL_SCANCODE_I        );
			auto toggleReplace    = initSimple("TOGGLEREPLACE"         , &Game::ToggleReplaceAction         , SDL_SCANCODE_SEMICOLON);
			                        initSimpleAlt(toggleReplace                                             , SDL_SCANCODE_INSERT   );
			auto toggleSdelete    = initSimple("TOGGLESDELETE"         , &Game::ToggleSdeleteAction         , SDL_SCANCODE_SEMICOLON, { ctrlInput });
			                        initSimpleAlt(toggleSdelete                                             , SDL_SCANCODE_INSERT   , { ctrlInput });
			                        initSimpleAlt(toggleSdelete                                             , SDL_SCANCODE_DELETE   );
			auto openProperty     = initSimple("OPENPROPERTY"          , &Game::OpenProperty                , SDL_SCANCODE_P        , { ctrlInput });
			                        initSimpleAlt(openProperty                                              , SDL_SCANCODE_F2       , { ctrlInput });
			                        initSimple("PICKSTAMP"             , &Game::OpenStampBrowser            , SDL_SCANCODE_K        );
			                        initSimple("OPENCONSOLE"           , &Game::OpenConsoleAction           , SDL_SCANCODE_GRAVE    );
			                        initSimple("INSTALL"               , &Game::InstallAction               , SDL_SCANCODE_I        , { ctrlInput });
			                        initSimple("PASTE"                 , &Game::PasteAction                 , SDL_SCANCODE_V        , { ctrlInput });
			                        initSimple("COPY"                  , &Game::CopyAction                  , SDL_SCANCODE_C        , { ctrlInput });
			                        initSimple("CUT"                   , &Game::CutAction                   , SDL_SCANCODE_X        , { ctrlInput });
			                        initSimple("STAMP"                 , &Game::StampAction                 , SDL_SCANCODE_S        );
			                        initSimple("LOADLASTSTAMP"         , &Game::LoadLastStampAction         , SDL_SCANCODE_L        );
			                        initSimple("TOGGLEFULLSCREEN"      , &Game::ToggleFullscreenAction      , SDL_SCANCODE_F11        );
			auto screenshot       = initSimple("SCREENSHOT"            , &Game::TakeScreenshotAction        , SDL_SCANCODE_P        );
			                        initSimpleAlt(screenshot                                                , SDL_SCANCODE_F2       );
		}

		auto initRendererPreset = [this, actionNamePrefix](int32_t index, int32_t scancode, std::set<Input> modifiers = {}) {
			auto action = std::make_shared<Action>();
			action->name = ByteString::Build(actionNamePrefix, "RENDERERPRESET", index);
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
		initRendererPreset(10, SDL_SCANCODE_1, { shiftInput });
		initRendererPreset(11, SDL_SCANCODE_6, { shiftInput });

		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "SAVEBUTTONSALTFUNCTION";
			action->begin = [this]() {
				saveButtonsAltFunction = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				saveButtonsAltFunction = false;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LCTRL }, {}, action });
		}
		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "INVERTINCLUDEPRESSURE";
			action->begin = [this]() {
				invertIncludePressure = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				invertIncludePressure = false;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, {}, action });
		}
		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "SAMPLEPROPERTY";
			action->begin = [this]() {
				sampleProperty = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				sampleProperty = false;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, {}, action });
		}
		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "TOOLSTRENGTHMUL10";
			action->begin = [this]() {
				toolStrengthMul10 = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				toolStrengthMul10 = false;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LSHIFT }, {}, action });
		}
		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "TOOLSTRENGTHDIV10";
			action->begin = [this]() {
				toolStrengthDiv10 = true;
				return InputDisposition::shared;
			};
			action->end = [this]() {
				toolStrengthDiv10 = false;
			};
			rootContext->inputToActions.push_back({ KeyboardKeyInput{ SDL_SCANCODE_LCTRL }, {}, action });
		}

		auto initGrowShrink = [actionNamePrefix, shiftInput, ctrlInput, altInput](ByteString actionNameStub, ActionContext *context, auto func) {
			for (int32_t i = 0; i < 12; ++i)
			{
				int32_t growShrink =  i       & 1;
				int32_t linLog     = (i >> 1) & 1;
				int32_t bothXY     =  i >> 2     ;
				int32_t diff;
				int32_t scancode;
				MouseWheelInput::Direction direction;
				auto action = std::make_shared<Action>();
				action->name = actionNamePrefix;
				switch (growShrink)
				{
				case 0: action->name += "GROW"  ; scancode = SDL_SCANCODE_RIGHTBRACKET; direction = MouseWheelInput::Direction::positiveY; diff =  1; break;
				case 1: action->name += "SHRINK"; scancode = SDL_SCANCODE_LEFTBRACKET ; direction = MouseWheelInput::Direction::negativeY; diff = -1; break;
				}
				action->name += actionNameStub;
				int32_t diffX = 0;
				int32_t diffY = 0;
				std::set<Input> modifiers;
				switch (bothXY)
				{
				case 0: diffX = diff; diffY = diff;                                                    break;
				case 1: diffX = diff;               action->name += "X"; modifiers.insert(shiftInput); break;
				case 2:               diffY = diff; action->name += "Y"; modifiers.insert(ctrlInput ); break;
				}
				bool logarithmic = false;
				switch (linLog)
				{
				case 0: modifiers.insert(altInput);                 break;
				case 1: logarithmic = true ; action->name += "LOG"; break;
				}
				action->begin = [func, diffX, diffY, logarithmic]() {
					func(diffX, diffY, logarithmic);
					return InputDisposition::exclusive;
				};
				context->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
				context->inputToActions.push_back({ MouseWheelInput{ direction }, modifiers, action });
			}
		};
		initGrowShrink("BRUSH", rootContext.get(), [this](int32_t diffX, int32_t diffY, bool logarithmic) {
			AdjustBrushSize(diffX, diffY, logarithmic);
		});
		initGrowShrink("ZOOM", placeZoomContext.get(), [this](int32_t diffX, int32_t diffY, bool logarithmic) {
			AdjustZoomSize(diffX, diffY, logarithmic);
		});

		auto initDraw = [this, actionNamePrefix, shiftInput, ctrlInput](ByteString actionNameStub, DrawMode drawMode, bool shift, bool ctrl, bool select) {
			for (int32_t i = 0; i < toolSlotCount; ++i)
			{
				auto action = std::make_shared<Action>();
				action->name = ByteString::Build(actionNamePrefix, actionNameStub, i);
				action->begin = [this, i, drawMode, select]() {
					if (select && ClickSign(i))
					{
						return InputDisposition::exclusive;
					}
					if (!GetSimMousePos())
					{
						return InputDisposition::unhandled;
					}
					BeginBrush(i, drawMode);
					return InputDisposition::exclusive;
				};
				action->end = [this, i]() {
					EndBrush(i);
				};
				std::shared_ptr<Action> selectAction;
				if (select)
				{
					selectAction = std::make_shared<Action>();
					selectAction->name = ByteString::Build(actionNamePrefix, "SELECT", i);
					selectAction->begin = [this, i]() {
						if (lastHoveredTool)
						{
							SelectTool(i, lastHoveredTool);
							return InputDisposition::exclusive;
						}
						return InputDisposition::unhandled;
					};
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
						rootContext->inputToActions.push_back({ MouseButtonInput{ *button }, modifiers, selectAction });
					}
					rootContext->inputToActions.push_back({ MouseButtonInput{ *button }, modifiers, action });
				}
			}
		};
		initDraw("DRAWFREE", DrawMode::free, false, false,  true);
		initDraw("DRAWLINE", DrawMode::line,  true, false, false);
		initDraw("DRAWRECT", DrawMode::rect, false,  true, false);
		initDraw("DRAWFILL", DrawMode::fill,  true,  true, false);
		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "TOGGLEFAVORITE";
			action->begin = [this]() {
				if (lastHoveredTool)
				{
					ToggleFavorite(lastHoveredTool);
					return InputDisposition::exclusive;
				}
				return InputDisposition::unhandled;
			};
			rootContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_LEFT }, { shiftInput, ctrlInput }, action });
		}
		{
			auto action = std::make_shared<Action>();
			action->name = actionNamePrefix + "REMOVECUSTOMGOL";
			action->begin = [this]() {
				if (lastHoveredTool && lastHoveredTool->Identifier.BeginsWith("DEFAULT_PT_LIFECUST_"))
				{
					RemoveCustomGol(lastHoveredTool->Identifier);
					return InputDisposition::exclusive;
				}
				return InputDisposition::unhandled;
			};
			rootContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_RIGHT }, { shiftInput, ctrlInput }, action });
		}

		{
			auto placeZoomAction = std::make_shared<Action>();
			placeZoomAction->name = actionNamePrefix + "PLACEZOOM";
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
			auto finalizeZoomAction = std::make_shared<Action>();
			finalizeZoomAction->name = actionNamePrefix + "FINALIZEZOOM";
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
			selectFinish = std::make_shared<Action>();
			selectFinish->name = actionNamePrefix + "SELECTFINISH";
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
			auto cancelAction = std::make_shared<Action>();
			cancelAction->name = actionNamePrefix + "SELECTCANCEL";
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
			pasteFinish = std::make_shared<Action>();
			pasteFinish->name = actionNamePrefix + "PASTEFINISH";
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
			auto cancelAction = std::make_shared<Action>();
			cancelAction->name = actionNamePrefix + "PASTECANCEL";
			cancelAction->begin = [this]() {
				if (GetCurrentActionContext() == pasteContext.get())
				{
					SetCurrentActionContext(rootContext);
				}
				return InputDisposition::exclusive;
			};
			pasteContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_LEFT }, {}, pasteFinish });
			pasteContext->inputToActions.push_back({ MouseButtonInput{ SDL_BUTTON_RIGHT }, {}, cancelAction });
			auto initSimple = [this, actionNamePrefix](std::string name, int32_t scancode, std::set<Input> modifiers, auto func) {
				auto action = std::make_shared<Action>();
				action->name = actionNamePrefix + name;
				action->begin = [func]() {
					func();
					return InputDisposition::exclusive;
				};
				pasteContext->inputToActions.push_back({ KeyboardKeyInput{ scancode }, modifiers, action });
			};
			initSimple("PASTEROTATE", SDL_SCANCODE_R    , {                       }, [this]() { TransformPasteSave(Mat2x2::CCW    ); });
			initSimple("PASTEHFLIP" , SDL_SCANCODE_R    , { shiftInput            }, [this]() { TransformPasteSave(Mat2x2::MirrorX); });
			initSimple("PASTEVFLIP" , SDL_SCANCODE_R    , { shiftInput, ctrlInput }, [this]() { TransformPasteSave(Mat2x2::MirrorY); });
			initSimple("PASTERIGHT" , SDL_SCANCODE_RIGHT, {                       }, [this]() { TranslatePasteSave({       1,  0 }); });
			initSimple("PASTELEFT"  , SDL_SCANCODE_LEFT , {                       }, [this]() { TranslatePasteSave({      -1,  0 }); });
			initSimple("PASTEUP"    , SDL_SCANCODE_UP   , {                       }, [this]() { TranslatePasteSave({       0, -1 }); });
			initSimple("PASTEDOWN"  , SDL_SCANCODE_DOWN , {                       }, [this]() { TranslatePasteSave({       0,  1 }); });
			pasteContext->end = [this]() {
				pasteSave.reset();
			};
		}

		SetCurrentActionContext(rootContext);
	}
}
