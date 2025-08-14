#include "AddLife.hpp"
#include "Game.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth = 200;
	}

	AddLife::AddLife(Game &newGame, int newToolSelection, int newRule, Rgb8 newColor1, Rgb8 newColor2) :
		View(newGame.GetHost()),
		game(newGame),
		toolSelection(newToolSelection),
		rule(newRule),
		color1(newColor1),
		color2(newColor2)
	{
		if (!rule) // TODO-REDO_UI: verify that this is a correct emptiness check
		{
			auto &rng = GetHost().GetRng();
			color1.Red   = rng.between(0x80, 0xFF);
			color1.Green = rng.between(0x80, 0xFF);
			color1.Blue  = rng.between(0x80, 0xFF);
			color2.Red   = rng.between(0x80, 0xFF);
			color2.Green = rng.between(0x80, 0xFF);
			color2.Blue  = rng.between(0x80, 0xFF);
		}
	}

	AddLife::CanApplyKind AddLife::CanApply() const
	{
		// TODO-REDO_UI: check if rule can be saved
		return CanApplyKind::yes;
	}

	void AddLife::Apply()
	{
		// TODO-REDO_UI: check if rule can be saved
		Exit();
	}

	bool AddLife::Cancel()
	{
		// TODO-REDO_UI: save rule
		Exit();
		return true;
	}

	void AddLife::Gui()
	{
		auto &g = GetHost();
		BeginDialog("addlife", "\biEdit custom GOL type", g.GetSize().OriginRect(), dialogWidth); // TODO-REDO_UI-TRANSLATE
		{
			BeginTextbox("name", name, "[name]", TextboxFlags::none);
			SetSize(Common{});
			EndTextbox();
			BeginTextbox("ruleString", ruleString, "[ruleset]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
			SetSize(Common{});
			EndTextbox();
			BeginHPanel("colors");
			{
				SetSize(Common{});
				SetSpacing(Common{});
				BeginColorButton("color1", color1);
				SetSize(Common{});
				if (EndColorButton())
				{
					// TODO: open color picker
				}
				BeginComponent("gradient");
				{
					auto rr = GetRect();
					if (rr.size.X > 1)
					{
						for (auto x = 0; x <= rr.size.X - 1; ++x)
						{
							g.DrawLine(rr.pos + Point{ x, 0 }, rr.pos + Point{ x, rr.size.Y - 1 }, color1.Blend(color2.WithAlpha(x * 255 / (rr.size.X - 1))).WithAlpha(255));
						}
					}
				}
				EndComponent();
				BeginColorButton("color2", color2);
				SetSize(Common{});
				if (EndColorButton())
				{
					// TODO: open color picker
				}
			}
			EndPanel();
		}
		EndDialog();
	}
}
