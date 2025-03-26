#include "Settings.hpp"
#include "Common/Log.hpp"
#include "Gui/Host.hpp"

namespace Powder::Activity
{
	Settings::Settings(Game &newGame) : game(newGame)
	{
	}

	bool Settings::CanTryOk() const
	{
		// TODO-REDO_UI
		return true;
	}

	void Settings::Ok()
	{
		// TODO-REDO_UI
	}

	void Settings::Gui()
	{
		SetRootRect(GetHost()->GetSize().OriginRect());
		BeginDialog("settings", "\biSettings", 320); // TODO-REDO_UI-TRANSLATE
		{
			SetSize(300);
			SetSpacing(0);
			SetPadding(0);
			BeginScrollpanel("scroll");
			SetMaxSize(MaxSizeFitParent{});
			SetSpacing(3);
			SetPadding(3);
			SetAlignment(Gui::Alignment::top);
			static std::array<bool, 30> a{};
			for (auto i = 0; i < 30; ++i)
			{
				Checkbox(ByteString::Build("Move ", i), a[i], 18);
			}
			EndScrollpanel();
		}
		EndDialog(*this);
	}

	bool Settings::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		if (HandleExitEvent(event, *this))
		{
			return true;
		}
		if (MayBeHandledExclusively(event) && handledByView)
		{
			return true;
		}
		return false;
	}
}
