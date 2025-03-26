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
		StringView title = "New sign"; // TODO-REDO_UI-TRANSLATE
		if (signIndex)
		{
			title = "Edit sign"; // TODO-REDO_UI-TRANSLATE
		}
		auto signtool = ScopedDialog("signtool", title, dialogWidth);
		BeginTextbox("signtext", signText, "[text]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
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
		DropdownItem(ByteString::Build(Gui::iconSignAlignLeftOutline  , " Left"  )); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconSignAlignMiddleOutline, " Middle")); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconSignAlignRightOutline , " Right" )); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconSignAlignNoneOutline  , " None"  )); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			focusSignText = true;
		}
		if (Button("move", "Move", SpanAll{})) // TODO-REDO_UI-TRANSLATE
		{
			movingSign = true;
		}
		BeginButton("delete", "Delete", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
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
