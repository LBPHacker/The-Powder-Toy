#include "Settings.hpp"
#include "Config.h"
#include "Activity/Game.hpp"
#include "Common/Log.hpp"
#include "graphics/Renderer.h"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/ViewUtil.hpp"
#include "common/VariantIndex.h"
#include "prefs/GlobalPrefs.h"
#include "Format.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size indentSize          =  16;
		constexpr Gui::View::Size rightColumnWidth    =  80;
		constexpr Gui::View::Size viewWidth           = 400;
		constexpr Gui::View::Size viewHeight          = 300;
		constexpr Gui::View::Size categoryWidth       = 100;
		constexpr Gui::View::Size categoryTextPadding =   6;

		using InputOrModifierOnly = Gui::InputMapper::InputOrModifierOnly;
		using Input = Gui::InputMapper::Input;
		using InputToAction = Gui::InputMapper::InputToAction;
		using ActionContext = Gui::InputMapper::ActionContext;

		std::string GetHumanReadable(const InputToAction &inputToAction)
		{
			std::ostringstream humanReadable;
			auto append = [&](const InputOrModifierOnly &inputOrModifierOnly) {
				auto *realInput = std::get_if<Input>(&inputOrModifierOnly);
				if (!realInput)
				{
					humanReadable << "...";
					return;
				}
				auto &input = *realInput;
				if (auto *keyboardKeyInput = std::get_if<Gui::InputMapper::KeyboardKeyInput>(&input))
				{
					std::string name(Gui::iconBrokenImage);
					switch (keyboardKeyInput->scancode)
					{
					case SDL_SCANCODE_LSHIFT: name = "Shift"; break; // TODO-REDO_UI: somehow allow right side key too
					case SDL_SCANCODE_LCTRL : name = "Ctrl" ; break;
					case SDL_SCANCODE_LALT  : name = "Alt"  ; break;
					default:
						auto key = SDL_GetKeyFromScancode(SDL_Scancode(keyboardKeyInput->scancode));
						auto *sdlName = SDL_GetKeyName(key);
						auto bad = !sdlName || strlen(sdlName) == 0;
						if (!bad)
						{
							name = sdlName;
						}
						break;
					}
					// TODO-REDO_UI: remove \bK from DrawText if it turns out to be as useless as it feels
					// humanReadable << Gui::iconKeyboardInitial << "\bK" << name <<"\bK" << Gui::iconKeyboardFinal;
					humanReadable << name;
				}
				else if (auto *mouseButtonInput = std::get_if<Gui::InputMapper::MouseButtonInput>(&input))
				{
					switch (mouseButtonInput->button)
					{
					case SDL_BUTTON_LEFT  : humanReadable << "LMB"; break;
					case SDL_BUTTON_MIDDLE: humanReadable << "MMB"; break;
					case SDL_BUTTON_RIGHT : humanReadable << "RMB"; break;
					default:
						humanReadable << "MOUSE" << mouseButtonInput->button;
						break;
					}
				}
				else if (auto *mouseWheelInput = std::get_if<Gui::InputMapper::MouseWheelInput>(&input))
				{
					using Direction = Gui::InputMapper::MouseWheelInput::Direction;
					switch (mouseWheelInput->direction)
					{
					case Direction::positiveX: humanReadable << "XWHEELUP"  ; break;
					case Direction::negativeX: humanReadable << "XWHEELDOWN"; break;
					case Direction::positiveY: humanReadable << "WHEELUP"   ; break;
					case Direction::negativeY: humanReadable << "WHEELDOWN" ; break;
					}
				}
				else
				{
					humanReadable << Gui::iconBrokenImage;
				}
			};
			for (auto &modifier : inputToAction.modifiers)
			{
				append(modifier);
				humanReadable << "+";
			}
			append(inputToAction.input);
			return humanReadable.str();
		}

		Settings::InputMappings GetInputMappings(const ActionContext &actionContext)
		{
			using ActionGroup                = Gui::InputMapper::ActionGroup;
			using Action                     = Gui::InputMapper::Action;
			using HumanReadableInputToAction = Settings::HumanReadableInputToAction;
			struct ByAction
			{
				std::shared_ptr<Gui::InputMapper::Action> action;
				std::vector<HumanReadableInputToAction> inputToActions;
			};
			struct ByActionGroup
			{
				std::shared_ptr<Gui::InputMapper::ActionGroup> actionGroup;
				std::map<Action::Name, ByAction> inputToActions;
			};
			std::map<ActionGroup::Name, ByActionGroup> imEarly;
			for (auto &toAdd : actionContext.inputToActions)
			{
				auto byActionGroupIt = imEarly.find(toAdd.action->group->name);
				if (byActionGroupIt == imEarly.end())
				{
					byActionGroupIt = imEarly.insert({ toAdd.action->group->name, { toAdd.action->group, {} } }).first;
				}
				auto &byActionGroup = byActionGroupIt->second.inputToActions;
				auto byActionIt = byActionGroup.find(toAdd.action->group->name);
				if (byActionIt == byActionGroup.end())
				{
					byActionIt = byActionGroup.insert({ toAdd.action->name, { toAdd.action, {} } }).first;
				}
				auto &byAction = byActionIt->second.inputToActions;
				byAction.push_back({ toAdd, GetHumanReadable(toAdd) });
			}
			Settings::InputMappings im;
			for (auto &[ _, byActionGroup ] : imEarly)
			{
				auto &byActionGroupFlat = im.emplace_back();
				byActionGroupFlat.actionGroup = byActionGroup.actionGroup;
				for (auto &[ _, byAction ] : byActionGroup.inputToActions)
				{
					auto &byActionFlat = byActionGroupFlat.inputMappings.emplace_back();
					byActionFlat.action = byAction.action;
					for (auto &inputToAction : byAction.inputToActions)
					{
						byActionFlat.inputToActions.push_back(inputToAction);
					}
					// sort byActionFlat.inputToActions?
				}
				std::sort(byActionGroupFlat.inputMappings.begin(), byActionGroupFlat.inputMappings.end(), [](auto &lhs, auto &rhs) {
					return lhs.action->displayIndex < rhs.action->displayIndex;
				});
			}
			std::sort(im.begin(), im.end(), [](auto &lhs, auto &rhs) {
				return lhs.actionGroup->displayIndex < rhs.actionGroup->displayIndex;
			});
			return im;
		}
	}

	std::optional<float> Settings::ParseTemperature(const std::string &str) const
	{
		std::optional<float> value;
		try
		{
			value = format::StringToTemperature(ByteString(str).FromUtf8(), game.GetTemperatureScale());
		}
		catch (const std::exception &ex) // TODO-REDO_UI-POSTCLEANUP: catch only sensible exceptions
		{
		}
		if (value && *value < MIN_TEMP)
		{
			return MIN_TEMP;
		}
		if (value && *value > MAX_TEMP)
		{
			return MAX_TEMP;
		}
		if (str.empty())
		{
			return R_TEMP + 273.15f;
		}
		return value;
	}

	Settings::Settings(Game &newGame) :
		View(newGame.GetHost()),
		game(newGame)
	{
		auto &g = GetHost();
		auto &windowParameters = g.GetWindowParameters();
		if (FORCE_WINDOW_FRAME_OPS != forceWindowFrameOpsHandheld)
		{
			auto desktopSize = g.GetDesktopSize();
			bool currentScaleValid = false;
			{
				int32_t scale = 1;
				do
				{
					if (windowParameters.fixedScale == scale)
					{
						scaleIndex = int32_t(scaleOptions.size());
						currentScaleValid = true;
					}
					scaleOptions.push_back({ ByteString::Build(scale), scale });
					scale += 1;
				}
				while (desktopSize.X >= windowParameters.windowSize.X * scale && desktopSize.Y >= windowParameters.windowSize.Y * scale);
			}
			if (!currentScaleValid)
			{
				scaleIndex = int32_t(scaleOptions.size());
				scaleOptions.push_back({ "current", windowParameters.fixedScale });
			}
		}
		// if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
		// {
		// 	displayMode = int32_t(windowParameters.frameType);
		// 	changeResolution = windowParameters.fullscreenChangeResolution;
		// 	forceIntegerScale = windowParameters.fullscreenForceIntegerScale;
		// }
		// blurryScaling = windowParameters.blurryScaling;
	}

	Settings::DispositionFlags Settings::GetDisposition() const
	{
		// TODO-REDO_UI
		return DispositionFlags::none;
	}

	void Settings::Ok()
	{
		// TODO-REDO_UI
		Exit();
	}

	bool Settings::Cancel()
	{
		// TODO-REDO_UI
		Exit();
		return true;
	}

	void Settings::GuiAmbientAirTemp()
	{
		auto ambientAirTemp = ScopedHPanel("ambientAirTemp");
		SetSize(Common{});
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		Text("title", "Ambient air temperature"); // TODO-REDO_UI-TRANSLATE
		{
			auto textAndColor = ScopedHPanel("textAndColor");
			SetSpacing(Common{});
			SetSize(rightColumnWidth);
			struct Rwpb
			{
				Settings &view;
				float Read() { return view.game.GetAmbientAirTemp(); }
				void Write(float value) { view.game.SetAmbientAirTemp(value); }
				std::optional<float> Parse(const std::string &str) { return view.ParseTemperature(str); }
				std::string Build(float value)
				{
					StringBuilder sb;
					sb << Format::Precision(2);
					format::RenderTemperature(sb, value, view.game.GetTemperatureScale());
					return sb.Build().ToUtf8();
				}
			};
			ambientAirTempInput.BeginTextbox("text", Rwpb{ *this });
			SetTextAlignment(Gui::Alignment::center, Gui::Alignment::center);
			ambientAirTempInput.EndTextbox(Rwpb{ *this });
			auto color = ambientAirTempInput.GetNumber() ? RGB::Unpack(HeatToColour(*ambientAirTempInput.GetNumber(), MIN_TEMP, MAX_TEMP)) : 0x000000_rgb;
			BeginColorButton("color", color);
			SetSize(Common{});
			EndColorButton();
		}
	}

	void Settings::GuiSimulation()
	{
		auto gameCheckbox = [this](ComponentKey key, StringView title, auto getter, auto setter) {
			auto vPanel = ScopedVPanel(key);
			SetParentFillRatio(0);
			auto value = (game.*getter)();
			BeginCheckbox("checkbox", title, value, CheckboxFlags::multiline);
			if (EndCheckbox())
			{
				(game.*setter)(value);
			}
		};

		TextSeparator("features", "Features"); // TODO-REDO_UI-TRANSLATE

		gameCheckbox(
			"heat",
			"Heat simulation\n\bgCan cause odd behaviour when disabled", // TODO-REDO_UI-TRANSLATE
			&Game::GetHeat,
			&Game::SetHeat
		);
		gameCheckbox(
			"newtonianGravity",
			"Newtonian gravity\n\bgMay cause poor performance on older computers", // TODO-REDO_UI-TRANSLATE
			&Game::GetNewtonianGravity,
			&Game::SetNewtonianGravity
		);
		gameCheckbox(
			"ambientHeat",
			"Ambient heat simulation\n\bgCan cause odd / broken behaviour with many saves", // TODO-REDO_UI-TRANSLATE
			&Game::GetAmbientHeat,
			&Game::SetAmbientHeat
		);
		gameCheckbox(
			"waterEqualization",
			"Water equalization\n\bgMay cause poor performance with a lot of water", // TODO-REDO_UI-TRANSLATE
			&Game::GetWaterEqualization,
			&Game::SetWaterEqualization
		);

		auto beginGameDropdown = [this](ComponentKey key, StringView title, auto getter, auto setter) {
			auto originalValue = (game.*getter)();
			auto value = int32_t(originalValue);
			BeginHPanel(key);
			SetSize(Common{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			Text("title", title);
			BeginDropdown("dropdown", value);
			SetSize(rightColumnWidth);
			if (DropdownGetChanged())
			{
				(game.*setter)(static_cast<decltype(originalValue)>(value));
			}
		};
		auto endGameDropdown = [this]() {
			EndDropdown();
			EndPanel();
		};

		TextSeparator("environment", "Environment"); // TODO-REDO_UI-TRANSLATE

		beginGameDropdown(
			"airMode",
			"Air simulation mode", // TODO-REDO_UI-TRANSLATE
			&Game::GetAirMode,
			&Game::SetAirMode
		);
		DropdownItem("On"          ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Pressure off"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Velocity off"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Off"         ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("No update"   ); // TODO-REDO_UI-TRANSLATE
		endGameDropdown();

		GuiAmbientAirTemp();

		beginGameDropdown(
			"gravityMode",
			"Gravity simulation mode", // TODO-REDO_UI-TRANSLATE
			&Game::GetGravityMode,
			&Game::SetGravityMode
		);
		DropdownItem("Vertical"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Off"     ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Radial"  ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Custom"  ); // TODO-REDO_UI-TRANSLATE
		endGameDropdown();

		beginGameDropdown(
			"edgeMode",
			"Edge mode", // TODO-REDO_UI-TRANSLATE
			&Game::GetEdgeMode,
			&Game::SetEdgeMode
		);
		DropdownItem("Void" ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Solid"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Loop" ); // TODO-REDO_UI-TRANSLATE
		endGameDropdown();

		beginGameDropdown(
			"temperatureScale",
			"Temperature scale", // TODO-REDO_UI-TRANSLATE
			&Game::GetTemperatureScale,
			&Game::SetTemperatureScale
		);
		DropdownItem("Kelvin"    ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Celsius"   ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Fahrenheit"); // TODO-REDO_UI-TRANSLATE
		endGameDropdown();
	}

	void Settings::GuiShortcuts()
	{
		SetPadding(0);
		auto shortcutsPanel = ScopedVPanel("shortcuts");
		bool scrollCategoryToBegin = false;
		SetMaxSize(MaxSizeFitParent{}); // neutralizes the parent scrollpanel
		{
			auto contextPanel = ScopedVPanel("context");
			SetPadding(6);
			SetParentFillRatio(0);
			BeginDropdown("select", selectedContext);
			SetSize(Common{});
			for (auto &item : game.shortcutMapperInfo.contexts)
			{
				DropdownItem(item->name);
			}
			if (EndDropdown())
			{
				inputMappingsForContext.reset();
				scrollCategoryToBegin = true;
			}
		}
		Separator("afterContext");
		auto scroll = ScopedScrollpanel("scroll");
		if (scrollCategoryToBegin)
		{
			ScrollpanelSetScroll({ 0, 0 });
		}
		auto padding = GetHost().GetCommonMetrics().padding;
		SetPrimaryAxis(Axis::vertical);
		SetMaxSize(MaxSizeFitParent{});
		SetAlignment(Gui::Alignment::top);
		{
			auto mappingsPanel = ScopedVPanel("mappings");
			SetAlignment(Gui::Alignment::top);
			SetSpacing(Common{});
			auto context = game.shortcutMapperInfo.contexts[selectedContext];
			if (!inputMappingsForContext)
			{
				inputMappingsForContext = GetInputMappings(*context);
			}
			int32_t groupIndex = 0;
			for (auto &[ group, groupItems ] : *inputMappingsForContext)
			{
				auto groupPanel = ScopedVPanel(groupIndex);
				SetAlignment(Gui::Alignment::top);
				SetPadding(Common{});
				SetParentFillRatio(0);
				groupIndex += 1;
				TextSeparator("groupName", group->name); // TODO-REDO_UI-TRANSLATE
				auto itemsPanel = ScopedVPanel("items");
				SetAlignment(Gui::Alignment::top);
				SetPadding(Common{});
				SetSpacing(Common{});
				int32_t index = 0;
				for (auto &[ action, actionItems ] : groupItems)
				{
					if (index > 0)
					{
						Separator(index - 1);
					}
					auto mappingPanel = ScopedHPanel(index);
					SetPadding(padding, padding, 0, 0);
					SetSpacing(Common{});
					SetParentFillRatio(0);
					{
						auto namePanel = ScopedVPanel("namePanel");
						SetPadding(padding, padding, 0, 0);
						SetTextAlignment(Gui::Alignment::left, Gui::Alignment::top);
						BeginText("name", action->name, TextFlags::multiline | TextFlags::autoHeight); // TODO-REDO_UI-TRANSLATE
						EndText();
					}
					auto inputsPanel = ScopedVPanel("inputsPanel");
					SetAlignment(Gui::Alignment::top);
					SetParentFillRatio(0);
					SetSpacing(Common{});
					auto itemCount = int32_t(actionItems.size());
					auto addRemoveSize = GetHost().GetCommonMetrics().size;
					for (int32_t i = 0; i < itemCount; ++i)
					{
						auto &inputToAction = actionItems[i];
						auto itemPanel = ScopedHPanel(i);
						SetSize(Common{});
						SetSpacing(Common{});
						Button("change", inputToAction.humanReadableInput, 100);
						if (i == itemCount - 1)
						{
							Button("add", Gui::iconAddOutline, addRemoveSize);
						}
						else
						{
							Button("remove", Gui::iconRemoveOutline, addRemoveSize);
						}
					}
					index += 2;
				}
			}
		}
	}

	void Settings::GuiVideo()
	{
		auto &g = GetHost();
		auto windowParameters = g.GetWindowParameters();

		auto checkbox = [this](ComponentKey key, StringView title, bool &value, bool enabled, bool round, Size indent) {
			auto hPanel = ScopedHPanel(key);
			SetPadding(indent * indentSize, 0, 0, 0);
			SetParentFillRatio(0);
			auto checkboxFlags = CheckboxFlags::multiline;
			if (round)
			{
				checkboxFlags = checkboxFlags | CheckboxFlags::round;
			}
			BeginCheckbox("checkbox", title, value, checkboxFlags);
			SetEnabled(enabled);
			auto changed = EndCheckbox();
			return changed;
		};

		bool changed = false;
		bool isFixed = true;
		if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
		{
			TextSeparator("displayMode", "Display mode"); // TODO-REDO_UI-TRANSLATE

			isFixed = windowParameters.frameType == Gui::WindowParameters::FrameType::fixed;
			changed |= checkbox(
				"fixedFrame",
				"Fixed-size window", // TODO-REDO_UI-TRANSLATE
				isFixed,
				true,
				true,
				0
			);
			if (isFixed)
			{
				windowParameters.frameType = Gui::WindowParameters::FrameType::fixed;
			}

			auto isResizable = windowParameters.frameType == Gui::WindowParameters::FrameType::resizable;
			changed |= checkbox(
				"resizableFrame",
				"Resizable window", // TODO-REDO_UI-TRANSLATE
				isResizable,
				true,
				true,
				0
			);
			if (isResizable)
			{
				windowParameters.frameType = Gui::WindowParameters::FrameType::resizable;
			}

			auto isFullscreen = windowParameters.frameType == Gui::WindowParameters::FrameType::fullscreen;
			changed |= checkbox(
				"fullscreenFrame",
				"Fullscreen", // TODO-REDO_UI-TRANSLATE
				isFullscreen,
				true,
				true,
				0
			);
			if (isFullscreen)
			{
				windowParameters.frameType = Gui::WindowParameters::FrameType::fullscreen;
			}
			changed |= checkbox(
				"changeResolution",
				"Set optimal screen resolution", // TODO-REDO_UI-TRANSLATE
				windowParameters.fullscreenChangeResolution,
				isFullscreen,
				false,
				1
			);
			changed |= checkbox(
				"forceIntegerScaling",
				"Force integer scaling\n\bgLess blurry", // TODO-REDO_UI-TRANSLATE
				windowParameters.fullscreenForceIntegerScale,
				isFullscreen,
				false,
				1
			);
		}

		TextSeparator("scaling", "Scaling"); // TODO-REDO_UI-TRANSLATE

		if (FORCE_WINDOW_FRAME_OPS != forceWindowFrameOpsHandheld)
		{
			auto windowScale = ScopedHPanel("windowScale");
			SetSize(Common{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("title", "Window scale factor for larger screens", TextFlags::none);
			SetEnabled(isFixed);
			EndText();
			BeginDropdown("dropdown", scaleIndex);
			SetEnabled(isFixed);
			SetSize(rightColumnWidth);
			for (auto &item : scaleOptions)
			{
				DropdownItem(item.name);
			}
			changed |= EndDropdown();
		}
		changed |= checkbox(
			"blurryScaling",
			"Blurry scaling\n\bgMore blurry, better on very big screens", // TODO-REDO_UI-TRANSLATE
			windowParameters.blurryScaling,
			true,
			false,
			0
		);

		if (changed)
		{
			windowParameters.fixedScale = scaleOptions[scaleIndex].scale;
			g.SetWindowParameters(windowParameters);
			windowParameters.SetPrefs();
		}
	}

	void Settings::Gui()
	{
		auto settings = ScopedDialog("settings", "Settings", viewWidth); // TODO-REDO_UI-TRANSLATE
		SetPrimaryAxis(Axis::horizontal);
		SetSize(viewHeight);
		SetPadding(0);
		SetSpacing(0);
		struct Category
		{
			std::string name;
			void (Settings::*gui)();
		};
		static const std::array categories = {
			Category{ std::string(Gui::iconPowder, sizeof(Gui::iconPowder) - 1) + " Simulation", &Settings::GuiSimulation }, // TODO-REDO_UI-TRANSLATE
			Category{ std::string(Gui::iconSolid , sizeof(Gui::iconSolid ) - 1) + " Video"     , &Settings::GuiVideo      }, // TODO-REDO_UI-TRANSLATE
			Category{ std::string(Gui::iconSolid , sizeof(Gui::iconSolid ) - 1) + " Shortcuts" , &Settings::GuiShortcuts  }, // TODO-REDO_UI-TRANSLATE
		};
		{
			auto categoriesPanel = ScopedScrollpanel("categories");
			SetPrimaryAxis(Axis::vertical);
			SetPadding(Common{});
			SetSpacing(Common{});
			SetAlignment(Gui::Alignment::top);
			SetSize(categoryWidth);
			for (int32_t i = 0; i < int32_t(categories.size()); ++i)
			{
				BeginButton(i, categories[i].name, currentCategory == i ? ButtonFlags::stuck : ButtonFlags::none);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				SetTextPadding(categoryTextPadding, 0, 0, 0);
				SetSize(Common{});
				if (EndButton())
				{
					currentCategory = i;
				}
			}
		}
		Separator("separator");
		auto scroll = ScopedScrollpanel("scroll");
		SetPrimaryAxis(Axis::vertical);
		SetMaxSizeSecondary(MaxSizeFitParent{});
		SetPadding(GetHost().GetCommonMetrics().padding + 1);
		SetSpacing(Common{});
		SetAlignment(Gui::Alignment::top);
		(this->*categories[currentCategory].gui)();
	}

	bool Settings::HandleEvent(const SDL_Event &event)
	{
		if (event.type == SDL_KEYMAPCHANGED)
		{
			// TODO-REDO_UI: deliver SDL_KEYMAPCHANGED as a global event to all Views/Stacks, possibly from Host
			//               probably in the form of a monotonously increasing keymap generation number in Host
			inputMappingsForContext.reset();
		}
		return View::HandleEvent(event);
	}
}
