#include "ElementSearch.hpp"
#include "Common/Log.hpp"
#include "Game.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"
#include "simulation/SimulationData.h"
#include "gui/game/tool/Tool.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr int32_t toolButtonsPerRow         =  7;
		constexpr int32_t maxRows                   = 12; // and an extra half row is shown to indicate that scrolling is possible
		constexpr Gui::View::Size toolButtonPadding =  2;
		constexpr Gui::View::Size toolButtonSpacing =  1;
	}

	ElementSearch::ElementSearch(Game &newGame) :
		View(newGame.GetHost()),
		game(newGame)
	{
		toolAtlasTexture = game.MakeToolAtlasTexture(*this);
		InitActions();
		Update();
	}

	void ElementSearch::InitActions()
	{
		// ActionContext root;
		// auto actionKeyPrefix = "DEFAULT_ACTION_";
		// for (int32_t i = 0; i < Game::toolSlotCount; ++i)
		// {
		// 	ActionKey actionKey = ByteString::Build(actionKeyPrefix, "SELECT", i);
		// 	AddAction(actionKey, Action{ [this, i]() {
		// 		if (lastHoveredTool)
		// 		{
		// 			game.SelectTool(i, lastHoveredTool);
		// 			Exit();
		// 			return InputDisposition::exclusive;
		// 		}
		// 		return InputDisposition::unhandled;
		// 	} });
		// 	std::optional<int32_t> button;
		// 	switch (i)
		// 	{
		// 	case 0: button = SDL_BUTTON_LEFT  ; break;
		// 	case 1: button = SDL_BUTTON_RIGHT ; break;
		// 	case 2: button = SDL_BUTTON_MIDDLE; break;
		// 	}
		// 	if (button)
		// 	{
		// 		// TODO-REDO_UI: somehow sync with Game
		// 		root.inputToActions.push_back({ MouseButtonInput{ *button }, {}, actionKey });
		// 	}
		// }
		// AddActionContext("DEFAULT_ACTIONCONTEXT_ROOT", std::move(root));
		// SetCurrentActionContext("DEFAULT_ACTIONCONTEXT_ROOT");
	}

	void ElementSearch::Update()
	{
		auto toLower = [](const std::string &str) {
			return std::string(ByteString(str).ToLower()); // TODO-REDO_UI
		};
		auto queryLower = toLower(query);

		enum class MatchingProperty
		{
			name,
			description,
			menuDescription,
		};
		enum class MatchKind
		{
			equals,
			beginsWith,
			contains,
		};
		struct Match
		{
			MatchingProperty matchingProperty;
			MatchKind matchKind;
			int32_t toolIndex;

			auto operator <=>(const Match &) const = default;
		};

		auto &tools = game.GetTools();
		std::vector<std::optional<Match>> matches(tools.size());
		auto found = [&](Match newMatch) {
			auto &match = matches[newMatch.toolIndex];
			if (!match)
			{
				match = newMatch;
			}
			*match = std::min(*match, newMatch);
		};

		auto matchProperty = [&](std::string infoLower, MatchingProperty matchingProperty, int32_t toolIndex) {
			if (infoLower == queryLower)
			{
				found({ matchingProperty, MatchKind::equals, toolIndex });
			}
			auto pos = infoLower.find(queryLower);
			if (pos == 0)
			{
				found({ matchingProperty, MatchKind::beginsWith, toolIndex });
			}
			if (pos != infoLower.npos)
			{
				found({ matchingProperty, MatchKind::contains, toolIndex });
			}
		};

		auto &sd = SimulationData::CRef();
		for (int32_t toolIndex = 0; toolIndex < int32_t(tools.size()); ++toolIndex)
		{
			auto &info = tools[toolIndex];
			if (!info)
			{
				continue;
			}
			matchProperty(toLower(info->tool->Name.ToUtf8()), MatchingProperty::name, toolIndex);
			matchProperty(toLower(info->tool->Description.ToUtf8()), MatchingProperty::description, toolIndex);
			auto menuSection = info->tool->MenuSection;
			if (menuSection >= 0 && menuSection < int32_t(sd.msections.size()))
			{
				matchProperty(toLower(sd.msections[menuSection].name.ToUtf8()), MatchingProperty::menuDescription, toolIndex);
			}
		}

		std::erase_if(matches, [](auto &item) {
			return !item;
		});
		std::sort(matches.begin(), matches.end());
		matchingTools.clear();
		std::transform(matches.begin(), matches.end(), std::back_inserter(matchingTools), [&](auto &match) {
			return &*tools[match->toolIndex];
		});
	}

	ElementSearch::DispositionFlags ElementSearch::GetDisposition() const
	{
		if (!matchingTools.empty())
		{
			return DispositionFlags::none;
		}
		return DispositionFlags::okDisabled;
	}

	void ElementSearch::Ok()
	{
		game.SelectTool(0, matchingTools[0]->tool.get());
		Exit();
	}

	void ElementSearch::Gui()
	{
		auto elementSearch = ScopedDialog("elementSearch", "Element search"); // TODO-REDO_UI-TRANSLATE
		BeginTextbox("query", query, "[name, description, or category]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
		SetSize(Common{});
		if (focusQuery)
		{
			GiveInputFocus();
			focusQuery = false;
		}
		if (EndTextbox())
		{
			Update();
		}
		auto tools = ScopedScrollpanel("tools");
		SetAlignment(Gui::Alignment::top);
		SetMaxSizeSecondary(MaxSizeFitParent{});
		SetParentFillRatio(0);
		SetSize((Game::toolTextureDataSize.Y + 2 * toolButtonPadding + toolButtonSpacing) * (2 * maxRows + 1) / 2 + toolButtonSpacing + 2);
		SetSizeSecondary((Game::toolTextureDataSize.X + 2 * toolButtonPadding + toolButtonSpacing) * toolButtonsPerRow + toolButtonSpacing + 2);
		SetPadding(toolButtonPadding);
		SetSpacing(toolButtonSpacing);
		auto &g = GetHost();
		auto r = GetRect();
		lastHoveredTool = nullptr;
		int32_t buttonIndex = 0;
		while (buttonIndex < int32_t(matchingTools.size()))
		{
			auto bPanel = ScopedHPanel(buttonIndex);
			SetAlignment(Gui::Alignment::left);
			SetSpacing(1);
			SetParentFillRatio(0);
			for (int32_t i = 0; i < toolButtonsPerRow; ++i)
			{
				auto cell = ScopedComponent(buttonIndex);
				SetSize(Game::toolTextureDataSize.X + 2 * toolButtonPadding);
				auto *info = (buttonIndex < int32_t(matchingTools.size())) ? matchingTools[buttonIndex] : nullptr;
				if (info && game.GuiToolButton(*this, *info, *toolAtlasTexture, true))
				{
					// QueueToolTip(info->tool->Description.ToUtf8(), ...) // TODO-REDO_UI
					lastHoveredTool = info->tool.get();
				}
				buttonIndex += 1;
			}
		}
		g.DrawRect(r, 0xFFFFFFFF_argb);
	}

	bool ElementSearch::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		if (MayBeHandledExclusively(event) && handledByView && !lastHoveredTool)
		{
			return true;
		}
		auto handledByInputMapper = InputMapper::HandleEvent(event);
		if (MayBeHandledExclusively(event) && handledByInputMapper)
		{
			return true;
		}
		return false;
	}

	void ElementSearch::SetOnTop(bool newOnTop)
	{
		if (!newOnTop)
		{
			EndAllInputs();
		}
	}
}
