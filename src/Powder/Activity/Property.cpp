#include "Property.hpp"
#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"
#include "simulation/Particle.h"
#include "prefs/GlobalPrefs.h"
#include "Format.h"

namespace Powder::Activity
{
	Property::Property(PropertyTool &newTool, Game &newGame, std::optional<int> takePropertyFrom) :
		View(newGame.GetHost()),
		tool(newTool),
		game(newGame)
	{
		auto &prefs = GlobalPrefs::Ref();
		propIndex = PropertyIndex(prefs.Get("Prop.Type", 0));
		propValueStr = prefs.Get("Prop.Value", ByteString(""));
		auto taken = TakePropertyFrom(game.GetSimulation(), takePropertyFrom);
		if (taken)
		{
			std::tie(propIndex, propValueStr) = *taken;
		}
		Update();
	}

	std::optional<std::pair<Property::PropertyIndex, std::string>> Property::TakePropertyFrom(const Simulation &sim, std::optional<int> i) const
	{
		auto toolConfiguration = tool.GetConfiguration();
		if (!toolConfiguration || !i)
		{
			return std::nullopt;
		}
		auto value = toolConfiguration->changeProperty.Get(&sim, *i);
		auto &prop = Particle::GetProperties()[toolConfiguration->changeProperty.propertyIndex];
		String valueString;
		if (prop.Name == "temp")
		{
			StringBuilder sb;
			format::RenderTemperature(sb, std::get<float>(value), game.GetTemperatureScale());
			valueString = sb.Build();
		}
		else
		{
			valueString = prop.ToString(value);
		}
		return std::pair{ toolConfiguration->changeProperty.propertyIndex, valueString.ToUtf8() };
	}

	void Property::Update()
	{
		configuration.reset();
		try
		{
			auto str = ByteString(propValueStr).FromUtf8();
			configuration = PropertyTool::Configuration{ AccessProperty::Parse(propIndex, str, game.GetTemperatureScale()), str };
		}
		catch (const AccessProperty::ParseError &ex)
		{
		}
	}

	bool Property::CanTryOk() const
	{
		return bool(configuration);
	}

	void Property::Ok()
	{
		tool.SetConfiguration(configuration);
	}

	void Property::Gui()
	{
		SetRootRect(GetHost().GetSize().OriginRect());
		BeginDialog("propertytool", "\biEdit property", 200); // TODO-REDO_UI-TRANSLATE
		{
			BeginDropdown("propname", propIndex);
			SetSize(17);
			auto &properties = Particle::GetProperties();
			for (auto &prop : properties)
			{
				DropdownItem(prop.Name);
			}
			if (EndDropdown())
			{
				Update();
				focusPropValue = true;
			}
			BeginTextbox("propvalue", propValueStr, TextboxFlags::none, (configuration ? Gui::colorWhite : Gui::colorRed).WithAlpha(255));
			SetSize(17);
			if (focusPropValue)
			{
				GiveInputFocus();
				TextboxSelectAll();
				focusPropValue = false;
			}
			if (EndTextbox())
			{
				Update();
			}
		}
		EndDialog(*this);
	}

	bool Property::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		if (HandleExitEvent(event, *this))
		{
			return true;
		}
		auto &properties = Particle::GetProperties();
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (!event.key.repeat)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_UP:
					if (propIndex > 0)
					{
						propIndex -= 1;
						Update();
					}
					return true;

				case SDLK_DOWN:
					if (propIndex < PropertyIndex(properties.size()) - 1)
					{
						propIndex += 1;
						Update();
					}
					return true;
				}
			}
			break;
		}
		if (MayBeHandledExclusively(event) && handledByView)
		{
			return true;
		}
		return false;
	}
}
