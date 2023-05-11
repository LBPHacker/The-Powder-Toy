#include "Detail.h"
#include "Dynamic.h"
#include "client/GameSave.h"
#include "PowderToySDL.h"
#include <SDL_syswm.h>

namespace Clipboard
{
#define CLIPBOARD_IMPLS_DECLARE
#include "ClipboardImpls.h"
#undef CLIPBOARD_IMPLS_DECLARE

	struct ClipboardImplEntry
	{
		SDL_SYSWM_TYPE subsystem;
		std::unique_ptr<ClipboardImpl> (*factory)();
	} clipboardImpls[] = {
#define CLIPBOARD_IMPLS_DEFINE
#include "ClipboardImpls.h"
#undef CLIPBOARD_IMPLS_DEFINE
		{ SDL_SYSWM_UNKNOWN, nullptr },
	};

	std::unique_ptr<GameSave> clipboardData;
	std::unique_ptr<ClipboardImpl> clipboard;

	void SetClipboardData(std::unique_ptr<GameSave> data)
	{
		clipboardData = std::move(data);
		if (clipboard)
		{
			clipboard->SetClipboardData(); // this either works or it doesn't, we don't care
		}
	}

	const GameSave *GetClipboardData()
	{
		if (clipboard)
		{
			clipboard->GetClipboardData(); // this either works or it doesn't, we don't care
		}
		return clipboardData.get();
	}

	void Init()
	{
	}

	int currentSubsystem;

	void RecreateWindow()
	{
		// old window is gone (or doesn't exist), associate clipboard data with the new one
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		SDL_GetWindowWMInfo(sdl_window, &info);
		clipboard.reset();
		currentSubsystem = info.subsystem;
		for (auto *impl = clipboardImpls; impl->factory; ++impl)
		{
			if (impl->subsystem == currentSubsystem)
			{
				clipboard = impl->factory();
				break;
			}
		}
		if (clipboard)
		{
			clipboard->SetClipboardData();
		}
	}
}
