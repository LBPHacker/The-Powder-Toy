#include "Sign.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Activity/Game.hpp"
#include "simulation/Simulation.h"

namespace Powder::Activity
{
	Sign::Sign(Game &newGame, std::optional<int32_t> newSignIndex, Pos2 newSignPos) : game(newGame), signIndex(newSignIndex), signPos(newSignPos)
	{
		if (signIndex)
		{
			auto *sim = game.GetSimulation();
			auto &sign = sim->signs[*signIndex];
			signText = sign.text.ToUtf8();
			signJustification = sign.ju;
			signPos = { sign.x, sign.y };
		}
	}

	void Sign::Gui()
	{
		SetRootRect(GetHost()->GetSize().OriginRect());
		BeginModal("signtool");
		SetSize(200);
		{
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			StringView title = "\biNew sign"; // TODO-REDO_UI-TRANSLATE
			if (signIndex)
			{
				title = "\biEdit sign"; // TODO-REDO_UI-TRANSLATE
			}
			BeginText("title", title, Gui::View::TextFlags::none);
			SetSize(21);
			SetTextPadding(4);
			EndText();
			Separator("titlesep");
			BeginVPanel("content");
			{
				SetPadding(3);
				SetSpacing(3);
				// BeginDropdown("propname", propIndex);
				// SetSize(17);
				// auto &properties = Particle::GetProperties();
				// for (auto &prop : properties)
				// {
				// 	DropdownItem(prop.Name);
				// }
				// if (EndDropdown())
				// {
				// 	Update();
				// 	focusPropValue = true;
				// }
				BeginTextbox("signtext", signText, TextboxFlags::none);
				SetSize(17);
				if (focusSignText)
				{
					GiveInputFocus();
					TextboxSelectAll();
					focusSignText = false;
				}
				EndTextbox();
				BeginHPanel("buttons");
				SetSize(17);
				SetSpacing(3);
				BeginDropdown("alignment", signJustification);
				SetTextPadding(6, 0);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				DropdownItem(ByteString::Build(Gui::iconSignAlignLeftOutline  , " Left"));
				DropdownItem(ByteString::Build(Gui::iconSignAlignMiddleOutline, " Middle"));
				DropdownItem(ByteString::Build(Gui::iconSignAlignRightOutline , " Right"));
				DropdownItem(ByteString::Build(Gui::iconSignAlignNoneOutline  , " None"));
				if (EndDropdown())
				{
					focusSignText = true;
				}
				if (Button("Move", SpanAll{}, signIndex ? ButtonFlags::none : ButtonFlags::disabled))
				{
				}
				if (Button("Delete", SpanAll{}, signIndex ? ButtonFlags::none : ButtonFlags::disabled))
				{
					auto *sim = game.GetSimulation();
					sim->signs.erase(sim->signs.begin() + *signIndex);
					Exit();
				}
				EndPanel();
			}
			EndPanel();
			Separator("canceloksep");
			BeginHPanel("cancelok");
			{
				SetSize(21);
				SetPadding(3);
				SetSpacing(3);
				if (Button("Cancel", 50)) // TODO-REDO_UI-TRANSLATE
				{
					Exit();
				}
				if (Button("\boOK", SpanAll{}, signText.size() ? ButtonFlags::none : ButtonFlags::disabled)) // TODO-REDO_UI-TRANSLATE
				{
					auto *sim = game.GetSimulation();
					auto newSignInfo = sign(ByteString(signText).FromUtf8(), signPos.X, signPos.Y, sign::Justification(signJustification));
					if (signIndex)
					{
						sim->signs[*signIndex] = newSignInfo;
					}
					else
					{
						sim->signs.push_back(newSignInfo);
					}
					Exit();
				}
			}
			EndPanel();
		}
		EndModal();
	}

	bool Sign::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		switch (event.type)
		{
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym)
			{
			case SDLK_ESCAPE:
				Exit();
				return true;
			}
		}
		if (MayBeHandledExclusively(event) && handledByView)
		{
			return true;
		}
		return false;
	}
}
