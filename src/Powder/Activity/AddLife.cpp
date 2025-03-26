#include "AddLife.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"

namespace Powder::Activity
{
	AddLife::AddLife(Game &newGame, int newToolSelection, int newRule, Rgb8 newColor1, Rgb8 newColor2) :
		game(newGame),
		toolSelection(newToolSelection),
		rule(newRule),
		color1(newColor1),
		color2(newColor2)
	{
		if (!rule) // TODO-REDO_UI: verify that this is a correct emptiness check
		{
			auto &rng = Gui::Host::Ref().GetRng();
			color1.Red   = rng.between(0x80, 0xFF);
			color1.Green = rng.between(0x80, 0xFF);
			color1.Blue  = rng.between(0x80, 0xFF);
			color2.Red   = rng.between(0x80, 0xFF);
			color2.Green = rng.between(0x80, 0xFF);
			color2.Blue  = rng.between(0x80, 0xFF);
		}
	}

	void AddLife::Gui()
	{
		SetRootRect(GetHost()->GetSize().OriginRect());
		BeginModal("addlife");
		SetSize(200);
		{
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("title", "\biEdit custom GOL type", Gui::View::TextFlags::none); // TODO-REDO_UI-TRANSLATE
			SetSize(21);
			SetTextPadding(4);
			EndText();
			Separator("titlesep");
			BeginVPanel("content");
			{
				SetPadding(3);
				SetSpacing(3);
				BeginTextbox("name", name, TextboxFlags::none);
				SetSize(17);
				EndTextbox();
				BeginTextbox("ruleString", ruleString, TextboxFlags::none);
				SetSize(17);
				EndTextbox();
				BeginHPanel("colors");
				{
					SetSize(17);
					SetSpacing(3);
					BeginColorButton("color1", color1);
					SetSize(17);
					if (EndColorButton())
					{
						// TODO: open color picker
					}
					BeginComponent("gradient");
					{
						auto rr = GetRect();
						auto &g = *GetHost();
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
					SetSize(17);
					if (EndColorButton())
					{
						// TODO: open color picker
					}
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
				if (Button("\boOK", SpanAll{}, ButtonFlags::none)) // TODO-REDO_UI-TRANSLATE
				{
					// TODO
					Exit();
				}
			}
			EndPanel();
		}
		EndModal();
	}

	bool AddLife::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (!event.key.repeat)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_ESCAPE:
					Exit();
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
