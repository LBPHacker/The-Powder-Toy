#include "Settings.hpp"
#include "Config.h"
#include "Activity/Game.hpp"
#include "Common/Log.hpp"
#include "graphics/Renderer.h"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
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
		SetMaxSize(MaxSizeFitParent{});
		SetPadding(GetHost().GetCommonMetrics().padding + 1);
		SetSpacing(Common{});
		SetAlignment(Gui::Alignment::top);
		(this->*categories[currentCategory].gui)();
	}
}
