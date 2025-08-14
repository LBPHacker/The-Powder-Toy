#include "Sign.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
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

	Sign::CanApplyKind Sign::CanApply() const
	{
		if (!signText.empty())
		{
			return CanApplyKind::yes;
		}
		return CanApplyKind::no;
	}

	void Sign::Apply()
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

	bool Sign::Cancel()
	{
		Exit();
		return true;
	}

	void Sign::Gui()
	{
		auto &g = GetHost();
		{
			struct Proxy
			{
				Gui::Host &g;
				void DrawPixel(Vec2<int> pos, RGB color) { g.DrawPoint(pos, color.WithAlpha(255)); }
				void DrawRect(::Rect<int> rect, RGB color) { g.DrawRect(rect, color.WithAlpha(255)); }
				void DrawFilledRect(::Rect<int> rect, RGB color) { g.FillRect(rect, color.WithAlpha(255)); }
				void BlendText(Vec2<int> pos, const String &str, RGBA color) { g.DrawText(pos, str.ToUtf8(), color); }
			};
			GetSignInfo().Draw(game.GetSimulation(), Proxy{ g });
		}
		if (movingSign)
		{
			return;
		}
		StringView title = "\biNew sign"; // TODO-REDO_UI-TRANSLATE
		if (signIndex)
		{
			title = "\biEdit sign"; // TODO-REDO_UI-TRANSLATE
		}
		BeginDialog("signtool", title, g.GetSize().OriginRect(), dialogWidth);
		{
			BeginTextbox("signtext", signText, "[text]", TextboxFlags::none);
			SetSize(Common{});
			if (focusSignText)
			{
				GiveInputFocus();
				TextboxSelectAll();
				focusSignText = false;
			}
			EndTextbox();
			BeginHPanel("buttons");
			SetSize(Common{});
			SetSpacing(Common{});
			BeginDropdown("alignment", signJustification);
			SetTextPadding(dropdownTextPadding, 0);
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			DropdownItem(ByteString::Build(Gui::iconSignAlignLeftOutline  , " Left"  )); // TODO-REDO_UI-TRANSLATE
			DropdownItem(ByteString::Build(Gui::iconSignAlignMiddleOutline, " Middle")); // TODO-REDO_UI-TRANSLATE
			DropdownItem(ByteString::Build(Gui::iconSignAlignRightOutline , " Right" )); // TODO-REDO_UI-TRANSLATE
			DropdownItem(ByteString::Build(Gui::iconSignAlignNoneOutline  , " None"  )); // TODO-REDO_UI-TRANSLATE
			if (EndDropdown())
			{
				focusSignText = true;
			}
			if (Button("Move", SpanAll{})) // TODO-REDO_UI-TRANSLATE
			{
				movingSign = true;
			}
			if (Button("Delete", SpanAll{}, ButtonFlags::none)) // TODO-REDO_UI-TRANSLATE
			{
				SetEnabled(bool(signIndex));
				auto &sim = game.GetSimulation();
				sim.signs.erase(sim.signs.begin() + *signIndex);
				Exit();
			}
			EndPanel();
		}
		EndDialog();
	}

	void Sign::MoveSign()
	{
		auto m = *GetMousePos();
		// TODO-REDO_UI: take zoom into account
		signPos = m;
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
