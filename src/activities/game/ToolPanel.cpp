#include "ToolPanel.h"

#include "Config.h"
#include "Game.h"
#include "tool/Tool.h"
#include "ToolButton.h"
#include "gui/Appearance.h"
#include "gui/SDLWindow.h"

namespace activities::game
{
	constexpr auto simulationSize = gui::Point{ XRES, YRES };
	constexpr auto panelSize = gui::Point{ simulationSize.x - 1, 22 };

	constexpr std::array<int, 9> buttonShade = { { // * Gamma-corrected.
		255, 195, 145, 103, 69, 42, 23, 10, 3
	} };

	class ToolSubpanel : public gui::Component
	{
		int index;
		int thumbSize = 0;
		int thumbPosition = 0;

	public:
		ToolSubpanel(int index) : index(index)
		{
		}

		void Tick() final override
		{
			if (Game::Ref().CurrentMenuSection() == index)
			{
				Visible(true);
				auto scroll = (gui::SDLWindow::Ref().MousePosition() - Parent()->AbsolutePosition()).x;
				if (scroll < 0)
				{
					scroll = 0;
				}
				if (scroll > panelSize.x)
				{
					scroll = panelSize.x;
				}
				auto scrollDistance = Size().x - panelSize.x;
				auto panelPosition = -scrollDistance;
				if (scrollDistance > 0)
				{
					thumbSize = panelSize.x * panelSize.x / Size().x;
					thumbPosition = scroll * (panelSize.x - thumbSize) / panelSize.x;
					panelPosition = -int(scrollDistance * scroll / panelSize.x);
				}
				else
				{
					thumbSize = panelSize.x;
					thumbPosition = 0;
				}
				Position({ panelPosition, 0 });
			}
			else
			{
				Visible(false);
			}
		}

		void DrawAfterChildren() const final override
		{
			auto abs = Parent()->AbsolutePosition();
			auto &g = gui::SDLWindow::Ref();
			g.DrawRect(gui::Rect { abs + gui::Point{ thumbPosition, 20 }, { thumbSize, 2 } }, gui::Appearance::colors.inactive.idle.border);
			auto tl = abs;
			auto tr = abs + gui::Point{ panelSize.x - 1, 0 };
			auto bl = abs + gui::Point{ 0, panelSize.y - 5 };
			auto br = abs + gui::Point{ panelSize.x - 1, panelSize.y - 5 };
			for (auto i = 0; i < int(buttonShade.size()); ++i)
			{
				auto p = gui::Point{ i, 0 };
				auto c = gui::Color{ 0, 0, 0, buttonShade[i] };
				g.DrawLine(tl + p, bl + p, c);
				g.DrawLine(tr - p, br - p, c);
			}
			Component::DrawAfterChildren();
		}
	};

	ToolPanel::ToolPanel()
	{
		ClipChildren(true);
		Position({ 1, simulationSize.y + 1 });
		Size({ simulationSize.x - 1, panelSize.y });
		auto &gm = game::Game::Ref();

		std::array<std::vector<std::pair<String, tool::Tool *>>, SC_TOTAL> sections;
		for (const auto &identifierToTool : gm.Tools())
		{
			auto &identifier = identifierToTool.first;
			auto *tool = identifierToTool.second.get();
			auto ent = std::make_pair(identifier, tool);
			if (tool->MenuVisible() && tool->MenuSection() >= 0 && tool->MenuSection() < SC_TOTAL)
			{
				sections[tool->MenuSection()].push_back(ent);
			}
			if (gm.Favorite(identifier))
			{
				sections[SC_FAVORITES].push_back(ent);
			}
		}
		for (auto &buttons : sections)
		{
			std::sort(buttons.begin(), buttons.end(), [](const std::pair<String, tool::Tool *> &lhs, const std::pair<String, tool::Tool *> &rhs) {
				auto lhsp = lhs.second->MenuPrecedence();
				auto rhsp = rhs.second->MenuPrecedence();
				return std::tie(lhsp, lhs.first) < std::tie(rhsp, rhs.first);
			});
		}

		for (auto i = 0U; i < SC_TOTAL; ++i)
		{
			auto panel = EmplaceChild<ToolSubpanel>(i);
			auto width = 2 * int(buttonShade.size()) - 1;
			for (auto j = sections[i].size(); j > 0U; --j)
			{
				auto &name = sections[i][j - 1].first;
				auto *tool = sections[i][j - 1].second;
				auto button = panel->EmplaceChild<ToolButton>(name, tool);
				button->ToolTipPosition({ simulationSize.x - gui::SDLWindow::Ref().TextSize(tool->Description()).x - 11, simulationSize.y - 12 });
				button->Position({ width - int(buttonShade.size()) + 1, 0 });
				width += button->Size().x + 1;
			}
			if (width < 0)
			{
				width = 0;
			}
			panel->Size({ width, panelSize.y });
		}
	}
}
