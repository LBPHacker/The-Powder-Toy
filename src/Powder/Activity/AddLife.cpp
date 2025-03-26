#include "AddLife.hpp"
#include "Game.hpp"
#include "ColorPicker.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"
#include "prefs/GlobalPrefs.h"
#include "simulation/GOLString.h"
#include "simulation/SimulationData.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth = 200;
	}

	AddLife::AddLife(Game &newGame, int32_t newToolSelection, int32_t newRule, Rgb8 newColor1, Rgb8 newColor2) :
		View(newGame.GetHost()),
		game(newGame),
		toolSelection(newToolSelection),
		rule(newRule),
		color1(newColor1),
		color2(newColor2)
	{
		if (!rule)
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

	std::optional<CustomGOLData> AddLife::CheckCustomGolToAdd() const
	{
		return Game::CheckCustomGolToAdd(ByteString(ruleString).FromUtf8(), ByteString(name).FromUtf8(), color1, color2);
	}

	AddLife::DispositionFlags AddLife::GetDisposition() const
	{
		if (!CheckCustomGolToAdd())
		{
			return DispositionFlags::okDisabled;
		}
		return DispositionFlags::none;
	}

	void AddLife::Ok()
	{
		auto cgd = CheckCustomGolToAdd();
		auto canonical = SerialiseGOLRule(cgd->rule).ToUtf8();
		{
			auto &prefs = GlobalPrefs::Ref();
			Prefs::DeferWrite dw(prefs);
			prefs.Set("CustomGOL.Name", ByteString(name));
			prefs.Set("CustomGOL.Rule", ByteString(canonical));
		}
		auto *tool = game.AddCustomGol(*cgd);
		game.SelectTool(toolSelection, tool);
		Exit();
	}

	void AddLife::Gui()
	{
		auto &g = GetHost();
		auto dialog = ScopedDialog("addlife", "Edit custom GOL type", dialogWidth); // TODO-REDO_UI-TRANSLATE
		{
			BeginTextbox("name", name, "[name]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
			SetSize(Common{});
			EndTextbox();
			BeginTextbox("ruleString", ruleString, "[ruleset]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
			SetSize(Common{});
			EndTextbox();
			auto colors = ScopedHPanel("colors");
			SetSize(Common{});
			SetSpacing(Common{});
			BeginColorButton("color1", color1);
			SetSize(Common{});
			if (EndColorButton())
			{
				PushAboveThis(std::make_shared<ColorPicker>(GetHost(), false, color1.WithAlpha(0), [this](Rgba8 newColor1) {
					color1 = newColor1.NoAlpha();
				}));
			}
			{
				auto gradient = ScopedComponent("gradient");
				auto rr = GetRect();
				if (rr.size.X > 1)
				{
					for (auto x = 0; x <= rr.size.X - 1; ++x)
					{
						g.DrawLine(rr.pos + Point{ x, 0 }, rr.pos + Point{ x, rr.size.Y - 1 }, color1.Blend(color2.WithAlpha(x * 255 / (rr.size.X - 1))).WithAlpha(255));
					}
				}
			}
			BeginColorButton("color2", color2);
			SetSize(Common{});
			if (EndColorButton())
			{
				PushAboveThis(std::make_shared<ColorPicker>(GetHost(), false, color2.WithAlpha(0), [this](Rgba8 newColor2) {
					color2 = newColor2.NoAlpha();
				}));
			}
		}
	}
}
