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
#include "Lang/Language.hpp"
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
		using ActionCategory = Gui::InputMapper::ActionCategory;

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

		Settings::InputMappings GetInputMappings(const Game::ShortcutMapperInfo &smi, const ActionCategory *category)
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
			for (auto &context : smi.contexts)
			{
				for (auto &toAdd : context->inputToActions)
				{
					if (toAdd.action->group->category.get() != category)
					{
						continue;
					}
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
					scaleOptions.push_back({ "DEFAULT_LANG_SETTINGS_SCALING_FACTORVALUE"_St, scale });
					scale += 1;
				}
				while (desktopSize.X >= windowParameters.windowSize.X * scale && desktopSize.Y >= windowParameters.windowSize.Y * scale);
			}
			if (!currentScaleValid)
			{
				scaleIndex = int32_t(scaleOptions.size());
				scaleOptions.push_back({ "DEFAULT_LANG_SETTINGS_SCALING_FACTORCURRENT"_St, windowParameters.fixedScale });
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
		Text("title", "DEFAULT_LANG_SETTINGS_ENV_AIRTEMP"_St);
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
			auto color = ambientAirTempInput.GetNumber() ? HeatToColour(*ambientAirTempInput.GetNumber(), MIN_TEMP, MAX_TEMP) : 0x000000_rgb;
			BeginColorButton("color", color);
			SetSize(Common{});
			EndColorButton();
		}
	}

	void Settings::GuiSimulation()
	{
		auto gameCheckbox = [this](
			ComponentKey key,
			Lang::Translation title,
			Lang::Translation details,
			auto getter,
			auto setter
		) {
			auto vPanel = ScopedVPanel(key);
			SetParentFillRatio(0);
			auto value = (game.*getter)();
			BeginCheckbox("checkbox", BuildString(title, "\n\bg", details), value, CheckboxFlags::multiline);
			if (EndCheckbox())
			{
				(game.*setter)(value);
			}
		};

		TextSeparator("features", "DEFAULT_LANG_SETTINGS_FEATURE"_St);

		gameCheckbox(
			"heat",
			"DEFAULT_LANG_SETTINGS_FEATURE_HEATSIM"_St,
			"DEFAULT_LANG_SETTINGS_FEATURE_HEATSIMDETAIL"_St,
			&Game::GetHeat,
			&Game::SetHeat
		);
		gameCheckbox(
			"newtonianGravity",
			"DEFAULT_LANG_SETTINGS_FEATURE_NEWTONIAN"_St,
			"DEFAULT_LANG_SETTINGS_FEATURE_NEWTONIANDETAIL"_St,
			&Game::GetNewtonianGravity,
			&Game::SetNewtonianGravity
		);
		gameCheckbox(
			"ambientHeat",
			"DEFAULT_LANG_SETTINGS_FEATURE_AMBHEAT"_St,
			"DEFAULT_LANG_SETTINGS_FEATURE_AMBHEATDETAIL"_St,
			&Game::GetAmbientHeat,
			&Game::SetAmbientHeat
		);
		gameCheckbox(
			"waterEqualization",
			"DEFAULT_LANG_SETTINGS_FEATURE_WATEREQ"_St,
			"DEFAULT_LANG_SETTINGS_FEATURE_WATEREQDETAIL"_St,
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

		TextSeparator("environment", "DEFAULT_LANG_SETTINGS_ENV"_St);

		beginGameDropdown(
			"airMode",
			"DEFAULT_LANG_SETTINGS_ENV_AIRMODE"_St,
			&Game::GetAirMode,
			&Game::SetAirMode
		);
		DropdownItem("DEFAULT_LANG_SIMSTATE_AIR_ON"_St         );
		DropdownItem("DEFAULT_LANG_SIMSTATE_AIR_PRESSUREOFF"_St);
		DropdownItem("DEFAULT_LANG_SIMSTATE_AIR_VELOCITYOFF"_St);
		DropdownItem("DEFAULT_LANG_SIMSTATE_AIR_OFF"_St        );
		DropdownItem("DEFAULT_LANG_SIMSTATE_AIR_NOUPDATE"_St   );
		endGameDropdown();

		GuiAmbientAirTemp();

		beginGameDropdown(
			"gravityMode",
			"DEFAULT_LANG_SETTINGS_ENV_GRAVMODE"_St,
			&Game::GetGravityMode,
			&Game::SetGravityMode
		);
		DropdownItem("DEFAULT_LANG_SIMSTATE_GRAV_VERTICAL"_St);
		DropdownItem("DEFAULT_LANG_SIMSTATE_GRAV_OFF"_St     );
		DropdownItem("DEFAULT_LANG_SIMSTATE_GRAV_RADIAL"_St  );
		DropdownItem("DEFAULT_LANG_SIMSTATE_GRAV_CUSTOM"_St  );
		endGameDropdown();

		beginGameDropdown(
			"edgeMode",
			"DEFAULT_LANG_SETTINGS_ENV_EDGEMODE"_St,
			&Game::GetEdgeMode,
			&Game::SetEdgeMode
		);
		DropdownItem("DEFAULT_LANG_SIMSTATE_EDGE_VOID"_St );
		DropdownItem("DEFAULT_LANG_SIMSTATE_EDGE_SOLID"_St);
		DropdownItem("DEFAULT_LANG_SIMSTATE_EDGE_LOOP"_St );
		endGameDropdown();

		beginGameDropdown(
			"temperatureScale",
			"DEFAULT_LANG_SETTINGS_ENV_TEMPSCALE"_St,
			&Game::GetTemperatureScale,
			&Game::SetTemperatureScale
		);
		DropdownItem("DEFAULT_LANG_SIMSTATE_TEMPSCALE_KELVIN"_St    );
		DropdownItem("DEFAULT_LANG_SIMSTATE_TEMPSCALE_CELSIUS"_St   );
		DropdownItem("DEFAULT_LANG_SIMSTATE_TEMPSCALE_FAHRENHEIT"_St);
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
			BeginDropdown("select", selectedCategory);
			SetSize(Common{});
			for (auto &item : game.shortcutMapperInfo.categories)
			{
				DropdownItem(Lang::GetFormatterHolder(item->name.c_str()).formatter->Format({}));
			}
			if (EndDropdown())
			{
				inputMappingsForCategory.reset();
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
			if (!inputMappingsForCategory)
			{
				inputMappingsForCategory = GetInputMappings(game.shortcutMapperInfo, game.shortcutMapperInfo.categories[selectedCategory].get());
			}
			int32_t groupIndex = 0;
			for (auto &[ group, groupItems ] : *inputMappingsForCategory)
			{
				auto groupPanel = ScopedVPanel(groupIndex);
				SetAlignment(Gui::Alignment::top);
				SetPadding(Common{});
				SetParentFillRatio(0);
				groupIndex += 1;
				TextSeparator("groupName", Lang::GetFormatterHolder(group->name.c_str()).formatter->Format({}));
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
						BeginText("name", Lang::GetFormatterHolder(action->name.c_str()).formatter->Format({}), TextFlags::multiline | TextFlags::autoHeight);
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
						if (Button("change", inputToAction.humanReadableInput, 100))
						{
							Log("change");
						}
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

	void Settings::GuiInterface()
	{
		{
			TextSeparator("language", "DEFAULT_LANG_SETTINGS_LANG"_St);
			auto windowScale = ScopedHPanel("windowScale");
			SetSize(Common{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("title", "DEFAULT_LANG_SETTINGS_LANG_SELECT"_St, TextFlags::none);
			EndText();
			auto loadedIndex = Lang::Language::Ref().GetLoadedIndex();
			BeginDropdown("dropdown", loadedIndex);
			SetSize(150);
			for (auto &item : Lang::Language::GetAvailable())
			{
				DropdownItem(item.friendlyName);
			}
			if (EndDropdown())
			{
				Lang::Language::Ref().Load(loadedIndex);
			}
		}
	}

	void Settings::GuiVideo()
	{
		auto &g = GetHost();
		auto windowParameters = g.GetWindowParameters();

		auto checkbox = [this](
			ComponentKey key,
			Lang::Translation &title,
			Lang::Translation *details,
			bool &value,
			bool enabled,
			bool round,
			Size indent
		) {
			auto hPanel = ScopedHPanel(key);
			SetPadding(indent * indentSize, 0, 0, 0);
			SetParentFillRatio(0);
			auto checkboxFlags = CheckboxFlags::multiline;
			if (round)
			{
				checkboxFlags = checkboxFlags | CheckboxFlags::round;
			}
			if (details)
			{
				BeginCheckbox("checkbox", BuildString(title, "\n\bg", *details), value, checkboxFlags);
			}
			else
			{
				BeginCheckbox("checkbox", title, value, checkboxFlags);
			}
			SetEnabled(enabled);
			auto changed = EndCheckbox();
			return changed;
		};

		bool changed = false;
		bool isFixed = true;
		if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
		{
			TextSeparator("displayMode", "DEFAULT_LANG_SETTINGS_DISP"_St);

			isFixed = windowParameters.frameType == Gui::WindowParameters::FrameType::fixed;
			changed |= checkbox(
				"fixedFrame",
				"DEFAULT_LANG_SETTINGS_SCALING_FIXED"_St,
				{},
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
				"DEFAULT_LANG_SETTINGS_SCALING_RESIZABLE"_St,
				{},
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
				"DEFAULT_LANG_SETTINGS_SCALING_FULLSCREEN"_St,
				{},
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
				"DEFAULT_LANG_SETTINGS_SCALING_SETRES"_St,
				{},
				windowParameters.fullscreenChangeResolution,
				isFullscreen,
				false,
				1
			);
			changed |= checkbox(
				"forceIntegerScaling",
				"DEFAULT_LANG_SETTINGS_SCALING_FORCEINTEGER"_St,
				&"DEFAULT_LANG_SETTINGS_SCALING_FORCEINTEGERDETAIL"_St,
				windowParameters.fullscreenForceIntegerScale,
				isFullscreen,
				false,
				1
			);
		}

		TextSeparator("scaling", "DEFAULT_LANG_SETTINGS_SCALING"_St);

		if (FORCE_WINDOW_FRAME_OPS != forceWindowFrameOpsHandheld)
		{
			auto windowScale = ScopedHPanel("windowScale");
			SetSize(Common{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("title", "DEFAULT_LANG_SETTINGS_SCALING_FACTOR"_St, TextFlags::none);
			SetEnabled(isFixed);
			EndText();
			BeginDropdown("dropdown", scaleIndex);
			SetEnabled(isFixed);
			SetSize(rightColumnWidth);
			for (auto &item : scaleOptions)
			{
				DropdownItem(item.name(item.scale));
			}
			changed |= EndDropdown();
		}
		changed |= checkbox(
			"blurryScaling",
			"DEFAULT_LANG_SETTINGS_SCALING_LINEAR"_St,
			&"DEFAULT_LANG_SETTINGS_SCALING_LINEARDETAIL"_St,
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
		auto settings = ScopedDialog("settings", "DEFAULT_LANG_SETTINGS_TITLE"_St, viewWidth);
		SetPrimaryAxis(Axis::horizontal);
		SetSize(viewHeight);
		SetPadding(0);
		SetSpacing(0);
		struct Category
		{
			std::string name;
			void (Settings::*gui)();
		};
		const std::array categories = {
			Category{ BuildString(Gui::iconPowder, " ", "DEFAULT_LANG_SETTINGS_TAB_SIMULATION"_St), &Settings::GuiSimulation },
			Category{ BuildString(Gui::iconSolid , " ", "DEFAULT_LANG_SETTINGS_TAB_VIDEO"_St     ), &Settings::GuiVideo      },
			Category{ BuildString(Gui::iconSolid , " ", "DEFAULT_LANG_SETTINGS_TAB_INTERFACE"_St ), &Settings::GuiInterface  },
			Category{ BuildString(Gui::iconSolid , " ", "DEFAULT_LANG_SETTINGS_TAB_SHORTCUTS"_St ), &Settings::GuiShortcuts  },
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
			inputMappingsForCategory.reset();
		}
		return View::HandleEvent(event);
	}
}
