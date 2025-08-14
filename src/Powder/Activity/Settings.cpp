#include "Settings.hpp"
#include "Config.h"
#include "Activity/Game.hpp"
#include "Common/Log.hpp"
#include "graphics/Renderer.h"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "common/VariantIndex.h"
#include "prefs/GlobalPrefs.h"
#include "Format.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size indentSize       = 16;
		constexpr Gui::View::Size rightColumnWidth = 80;
		constexpr Gui::View::Size viewWidth        = 400;
	}

	std::optional<float> Settings::ParseTemperature(const std::string &str) const
	{
		std::optional<float> value;
		try
		{
			value = format::StringToTemperature(ByteString(str).FromUtf8(), game.GetTemperatureScale());
		}
		catch (const std::exception &ex) // TODO-REDO_UI: catch only sensible exceptions
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

	bool Settings::CanTryOk() const
	{
		// TODO-REDO_UI
		return true;
	}

	void Settings::Ok()
	{
		// TODO-REDO_UI
	}

	void Settings::GuiAmbientAirTemp()
	{
		BeginHPanel("ambientairtemp");
		SetSize(17);
		auto value = game.GetAmbientAirTemp();
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		Text("title", "Ambient air temperature"); // TODO-REDO_UI-TRANSLATE
		{
			BeginHPanel("textandcolor");
			SetSpacing(4);
			SetSize(rightColumnWidth);
			BeginTextbox("text", ambientAirTempInput.str, Gui::View::TextboxFlags::none);
			SetTextAlignment(Gui::Alignment::center, Gui::Alignment::center);
			auto inputFocus = HasInputFocus();
			if (EndTextbox())
			{
				ambientAirTempInput.wantReset = true;
				auto newValue = ParseTemperature(ambientAirTempInput.str);
				ambientAirTempInput.editedNumber = newValue;
				if (newValue)
				{
					game.SetAmbientAirTemp(*newValue);
				}
			}
			auto color = ambientAirTempInput.editedNumber ? RGB::Unpack(HeatToColour(*ambientAirTempInput.editedNumber, MIN_TEMP, MAX_TEMP)) : 0x000000_rgb;
			BeginColorButton("color", color);
			SetSize(17);
			EndColorButton();
			EndPanel();
			if (!inputFocus && ambientAirTempInput.wantReset)
			{
				StringBuilder sb;
				sb << Format::Precision(2);
				format::RenderTemperature(sb, value, game.GetTemperatureScale());
				ambientAirTempInput.str = sb.Build().ToUtf8();
				ambientAirTempInput.wantReset = false;
				ambientAirTempInput.editedNumber = value;
			}
		}
		EndPanel();
	}

	void Settings::GuiSimulation()
	{
		auto gameCheckbox = [this](ComponentKey key, StringView title, auto getter, auto setter) {
			BeginVPanel(key);
			SetParentFillRatio(0);
			auto value = (game.*getter)();
			BeginCheckbox("checkbox", title, value, CheckboxFlags::multiline);
			if (EndCheckbox())
			{
				(game.*setter)(value);
			}
			EndPanel();
		};

		TextSeparator("features", "Features"); // TODO-REDO_UI-TRANSLATE

		gameCheckbox(
			"heat",
			"Heat simulation\n\bgCan cause odd behaviour when disabled", // TODO-REDO_UI-TRANSLATE
			&Game::GetHeat,
			&Game::SetHeat
		);
		gameCheckbox(
			"newtoniangravity",
			"Newtonian gravity\n\bgMay cause poor performance on older computers", // TODO-REDO_UI-TRANSLATE
			&Game::GetNewtonianGravity,
			&Game::SetNewtonianGravity
		);
		gameCheckbox(
			"ambientheat",
			"Ambient heat simulation\n\bgCan cause odd / broken behaviour with many saves", // TODO-REDO_UI-TRANSLATE
			&Game::GetAmbientHeat,
			&Game::SetAmbientHeat
		);
		gameCheckbox(
			"waterequalization",
			"Water equalization\n\bgMay cause poor performance with a lot of water", // TODO-REDO_UI-TRANSLATE
			&Game::GetWaterEqualization,
			&Game::SetWaterEqualization
		);

		auto beginGameDropdown = [this](ComponentKey key, StringView title, auto getter, auto setter) {
			auto originalValue = (game.*getter)();
			auto value = int32_t(originalValue);
			BeginHPanel(key);
			SetSize(17);
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
			"airmode",
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
			"gravitymode",
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
			"edgemode",
			"Edge mode", // TODO-REDO_UI-TRANSLATE
			&Game::GetEdgeMode,
			&Game::SetEdgeMode
		);
		DropdownItem("Void" ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Solid"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Loop" ); // TODO-REDO_UI-TRANSLATE
		endGameDropdown();

		beginGameDropdown(
			"temperaturescale",
			"Temperature scale", // TODO-REDO_UI-TRANSLATE
			&Game::GetTemperatureScale,
			&Game::SetTemperatureScale
		);
		DropdownItem("Kelvin"    ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Celsius"   ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Fahrenheit"); // TODO-REDO_UI-TRANSLATE
		endGameDropdown();
	}

	void Settings::GuiVideo()
	{
		auto &g = GetHost();
		auto windowParameters = g.GetWindowParameters();

		auto checkbox = [this](ComponentKey key, StringView title, bool &value, bool enabled, bool round, int indent) {
			BeginHPanel(key);
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
			EndPanel();
			return changed;
		};

		bool changed = false;
		bool isFixed = true;
		if (FORCE_WINDOW_FRAME_OPS == forceWindowFrameOpsNone)
		{
			TextSeparator("displaymode", "Display mode"); // TODO-REDO_UI-TRANSLATE

			isFixed = windowParameters.frameType == Gui::WindowParameters::FrameType::fixed;
			changed |= checkbox(
				"fixedframe",
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
				"resizableframe",
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
				"fullscreenframe",
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
				"changeresolution",
				"Set optimal screen resolution", // TODO-REDO_UI-TRANSLATE
				windowParameters.fullscreenChangeResolution,
				isFullscreen,
				false,
				1
			);
			changed |= checkbox(
				"forceintegerscaling",
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
			BeginHPanel("windowscale");
			SetSize(17);
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("title", "Window scale factor for larger screens", Gui::View::TextFlags::none);
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
			EndPanel();
		}
		changed |= checkbox(
			"blurryscaling",
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
			{
				auto &prefs = GlobalPrefs::Ref();
				Prefs::DeferWrite dw(prefs);
				prefs.Set("Resizable"          , windowParameters.frameType == Gui::WindowParameters::FrameType::resizable );
				prefs.Set("Fullscreen"         , windowParameters.frameType == Gui::WindowParameters::FrameType::fullscreen);
				prefs.Set("AltFullscreen"      , windowParameters.fullscreenChangeResolution                               );
				prefs.Set("ForceIntegerScaling", windowParameters.fullscreenForceIntegerScale                              );
				prefs.Set("Scale"              , windowParameters.fixedScale                                               );
				prefs.Set("BlurryScaling"      , windowParameters.blurryScaling                                            );
			}
		}
	}

	void Settings::Gui()
	{
		SetRootRect(GetHost().GetSize().OriginRect());
		BeginDialog("settings", "\biSettings", viewWidth); // TODO-REDO_UI-TRANSLATE
		{
			SetPrimaryAxis(Gui::View::Axis::horizontal);
			SetSize(300);
			SetPadding(0);
			SetSpacing(0);
			BeginScrollpanel("categories");
			SetPrimaryAxis(Gui::View::Axis::vertical);
			SetPadding(6);
			SetSpacing(4);
			SetAlignment(Gui::Alignment::top);
			SetSize(100);
			struct Category
			{
				std::string name;
				void (Settings::*gui)();
			};
			static const std::array categories = {
				Category{ std::string(Gui::iconPowder, sizeof(Gui::iconPowder) - 1) + " Simulation", &Settings::GuiSimulation }, // TODO-REDO_UI-TRANSLATE
				Category{ std::string(Gui::iconSolid , sizeof(Gui::iconSolid ) - 1) + " Video"     , &Settings::GuiVideo      }, // TODO-REDO_UI-TRANSLATE
			};
			for (int32_t i = 0; i < int32_t(categories.size()); ++i)
			{
				BeginButton(i, categories[i].name, currentCategory == i ? ButtonFlags::stuck : ButtonFlags::none);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				SetTextPadding(6, 0, 0, 0);
				SetSize(17);
				if (EndButton())
				{
					currentCategory = i;
				}
			}
			EndScrollpanel();
			Separator("separator");
			BeginScrollpanel("scroll");
			SetPrimaryAxis(Gui::View::Axis::vertical);
			SetMaxSize(MaxSizeFitParent{});
			SetPadding(6);
			SetSpacing(4);
			SetAlignment(Gui::Alignment::top);
			(this->*categories[currentCategory].gui)();
			EndScrollpanel();
		}
		EndDialog(*this);
	}

	bool Settings::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		if (HandleExitEvent(event, *this))
		{
			return true;
		}
		if (MayBeHandledExclusively(event) && handledByView)
		{
			return true;
		}
		return false;
	}
}
