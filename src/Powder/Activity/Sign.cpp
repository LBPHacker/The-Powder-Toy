#include "Sign.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/ViewUtil.hpp"
#include "Activity/Game.hpp"
#include "simulation/Simulation.h"
#include "simulation/SignDraw.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth         = 200;
		constexpr Gui::View::Size dropdownTextPadding =   6;
	}

	Sign::Sign(Game &newGame, std::optional<int32_t> newSignIndex, Pos2 newSignPos) :
		View(newGame.GetHost()),
		game(newGame),
		signIndex(newSignIndex),
		signPos(newSignPos)
	{
		if (signIndex)
		{
			auto &sim = game.GetSimulation();
			auto &sign = sim.signs[*signIndex];
			signText = sign.text.ToUtf8();
			signJustification = sign.ju;
			signPos = { sign.x, sign.y };
		}
	}

	sign Sign::GetSignInfo() const
	{
		return sign(ByteString(signText).FromUtf8(), signPos.X, signPos.Y, sign::Justification(signJustification));
	}

	Sign::DispositionFlags Sign::GetDisposition() const
	{
		if (!signText.empty())
		{
			return DispositionFlags::none;
		}
		return DispositionFlags::okDisabled;
	}

	void Sign::Ok()
	{
		auto &sim = game.GetSimulation();
		auto newSignInfo = GetSignInfo();
		if (signIndex)
		{
			sim.signs[*signIndex] = newSignInfo;
		}
		else
		{
			sim.signs.push_back(newSignInfo);
		}
		Exit();
	}

	void Sign::Gui()
	{
		auto &g = GetHost();
		struct Proxy
		{
			Gui::Host &g;
			void DrawPixel(Vec2<int> pos, RGB color) { g.DrawPoint(pos, color.WithAlpha(255)); }
			void DrawRect(::Rect<int> rect, RGB color) { g.DrawRect(rect, color.WithAlpha(255)); }
			void DrawFilledRect(::Rect<int> rect, RGB color) { g.FillRect(rect, color.WithAlpha(255)); }
			void BlendText(Vec2<int> pos, const String &str, RGBA color) { g.DrawText(pos, str.ToUtf8(), color); }
		};
		GetSignInfo().Draw(game.GetSimulation(), Proxy{ g });
		if (movingSign)
		{
			return;
		}
		Lang::Translation *titleTl = &"DEFAULT_LANG_SIGN_NEWTITLE"_St;
		if (signIndex)
		{
			titleTl = &"DEFAULT_LANG_SIGN_EDITTITLE"_St;
		}
		auto signtool = ScopedDialog("signtool", *titleTl, dialogWidth);
		BeginTextbox("signtext", signText, BuildString("[", "DEFAULT_LANG_SIGN_TEXT"_St, "]"), TextboxFlags::none);
		SetSize(Common{});
		if (focusSignText)
		{
			GiveInputFocus();
			TextboxSelectAll();
			focusSignText = false;
		}
		EndTextbox();
		auto buttons = ScopedHPanel("buttons");
		SetSize(Common{});
		SetSpacing(Common{});
		BeginDropdown("alignment", signJustification);
		SetTextPadding(dropdownTextPadding, 0);
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		DropdownItem(BuildString(Gui::iconSignAlignLeftOutline  , " ", "DEFAULT_LANG_SIGN_ALIGNLEFT"_St  ));
		DropdownItem(BuildString(Gui::iconSignAlignMiddleOutline, " ", "DEFAULT_LANG_SIGN_ALIGNMIDDLE"_St));
		DropdownItem(BuildString(Gui::iconSignAlignRightOutline , " ", "DEFAULT_LANG_SIGN_ALIGNRIGHT"_St ));
		DropdownItem(BuildString(Gui::iconSignAlignNoneOutline  , " ", "DEFAULT_LANG_SIGN_ALIGNNONE"_St  ));
		if (EndDropdown())
		{
			focusSignText = true;
		}
		if (Button("move", "DEFAULT_LANG_SIGN_MOVE"_St, SpanAll{}))
		{
			movingSign = true;
		}
		BeginButton("delete", "DEFAULT_LANG_SIGN_DELETE"_St, ButtonFlags::none);
		SetEnabled(bool(signIndex));
		if (EndButton())
		{
			auto &sim = game.GetSimulation();
			sim.signs.erase(sim.signs.begin() + *signIndex);
			Exit();
		}
	}

	void Sign::MoveSign()
	{
		signPos = game.ResolveZoom(*GetMousePos());
	}

	bool Sign::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		switch (event.type)
		{
		case SDL_MOUSEMOTION:
			if (movingSign)
			{
				MoveSign();
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (movingSign)
			{
				MoveSign();
				movingSign = false;
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
