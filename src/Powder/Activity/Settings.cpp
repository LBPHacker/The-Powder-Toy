#include "Settings.hpp"
#include "Activity/Game.hpp"
#include "Common/Log.hpp"
#include "graphics/Renderer.h"
#include "Gui/Host.hpp"
#include "Format.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size rightColumnWidth = 80;

		template<class Mode>
		struct ArrayItem
		{
			Mode mode;
			Gui::View::StringView name;
		};
	}

	template<class Getter, class Setter>
	void Settings::Checkbox(ComponentKey key, int indentation, StringView title, Getter getter, Setter setter)
	{
		BeginVPanel(key);
		SetSize(23);
		auto value = (game.*getter)();
		if (View::Checkbox("checkbox", title, value))
		{
			(game.*setter)(value);
		}
		EndPanel();
	}

	template<class Getter, class Setter, class Items>
	void Settings::Dropdown(ComponentKey key, StringView title, Getter getter, Setter setter, Items &&items)
	{
		BeginHPanel(key);
		SetSize(17);
		auto value = int32_t((game.*getter)());
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		Text("title", title);
		BeginDropdown("dropdown", value);
		SetSize(rightColumnWidth);
		for (auto &item : items)
		{
			DropdownItem(item.name);
		}
		if (EndDropdown())
		{
			(game.*setter)(static_cast<decltype(items[0].mode)>(value));
		}
		EndPanel();
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

	Settings::Settings(Game &newGame) : game(newGame)
	{
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

	void Settings::Gui()
	{
		SetRootRect(GetHost()->GetSize().OriginRect());
		BeginDialog("settings", "\biSettings", 320); // TODO-REDO_UI-TRANSLATE
		{
			SetSize(300);
			SetPadding(0);
			SetSpacing(0);
			BeginScrollpanel("scroll");
			SetMaxSize(MaxSizeFitParent{});
			SetPadding(6);
			SetSpacing(4);
			SetAlignment(Gui::Alignment::top);
			// auto button = [this](StringView title, StringView explanation) {
			// };
			Checkbox("heat", 0,
			         "Heat simulation\n\bgCan cause odd behaviour when disabled", // TODO-REDO_UI-TRANSLATE
			         &Game::GetHeat, &Game::SetHeat);
			Checkbox("newtoniangravity", 0,
			         "Newtonian gravity\n\bgMay cause poor performance on older computers", // TODO-REDO_UI-TRANSLATE
			         &Game::GetNewtonianGravity, &Game::SetNewtonianGravity);
			Checkbox("ambientheat", 0,
			         "Ambient heat simulation\n\bgCan cause odd / broken behaviour with many saves", // TODO-REDO_UI-TRANSLATE
			         &Game::GetAmbientHeat, &Game::SetAmbientHeat);
			Checkbox("waterequalization", 0,
			         "Water equalization\n\bgMay cause poor performance with a lot of water", // TODO-REDO_UI-TRANSLATE
			         &Game::GetWaterEqualization, &Game::SetWaterEqualization);
			static const std::array airModes = {
				ArrayItem{ AIR_ON         , "On"           }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ AIR_PRESSUREOFF, "Pressure off" }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ AIR_VELOCITYOFF, "Velocity off" }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ AIR_OFF        , "Off"          }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ AIR_NOUPDATE   , "No update"    }, // TODO-REDO_UI-TRANSLATE
			};
			Dropdown("airmode",
			         "Air simulation mode", // TODO-REDO_UI-TRANSLATE
			         &Game::GetAirMode, &Game::SetAirMode,
			         airModes);
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
			static const std::array gravityModes = {
				ArrayItem{ GRAV_VERTICAL, "Vertical" }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ GRAV_OFF     , "Off"      }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ GRAV_RADIAL  , "Radial"   }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ GRAV_CUSTOM  , "Custom"   }, // TODO-REDO_UI-TRANSLATE
			};
			Dropdown("gravitymode",
			         "Gravity simulation mode", // TODO-REDO_UI-TRANSLATE
			         &Game::GetGravityMode, &Game::SetGravityMode,
			         gravityModes);
			static const std::array edgeModes = {
				ArrayItem{ EDGE_VOID , "Void"  }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ EDGE_SOLID, "Solid" }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ EDGE_LOOP , "Loop"  }, // TODO-REDO_UI-TRANSLATE
			};
			Dropdown("edgemode",
			         "Edge mode", // TODO-REDO_UI-TRANSLATE
			         &Game::GetEdgeMode, &Game::SetEdgeMode,
			         edgeModes);
			static const std::array temperatureScales = { // TODO-REDO_UI: move this from the sim section
				ArrayItem{ TEMPSCALE_KELVIN    , "Kelvin"     }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ TEMPSCALE_CELSIUS   , "Celsius"    }, // TODO-REDO_UI-TRANSLATE
				ArrayItem{ TEMPSCALE_FAHRENHEIT, "Fahrenheit" }, // TODO-REDO_UI-TRANSLATE
			};
			Dropdown("temperaturescale",
			         "Temperature scale", // TODO-REDO_UI-TRANSLATE
			         &Game::GetTemperatureScale, &Game::SetTemperatureScale,
			         temperatureScales);
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
