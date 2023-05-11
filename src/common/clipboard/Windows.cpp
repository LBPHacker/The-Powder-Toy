#include "Detail.h"
#include "client/GameSave.h"
#include "common/platform/Platform.h"
#include "PowderToySDL.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <iostream>
#include <windows.h>

namespace Clipboard
{
	class WindowsClipboardImpl : public ClipboardImpl
	{
		UINT saveClipboardFormat = 0;
		HWND ourHwnd = NULL;
		DWORD seqNumber = 0; // 0 is invalid

		class ClipboardSession
		{
			bool open = false;

		public:
			ClipboardSession(HWND ourHwnd)
			{
				if (ourHwnd)
				{
					open = ::OpenClipboard(ourHwnd);
				}
			}

			~ClipboardSession()
			{
				if (open)
				{
					::CloseClipboard();
				}
			}

			explicit operator bool() const
			{
				return open;
			}
		};

		void Transfer()
		{
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot transfer save data: save clipboard format not registered" << std::endl;
				return;
			}
			if (!clipboardData)
			{
				std::cerr << "cannot transfer save data: no data to transfer" << std::endl;
				return;
			}
			std::vector<char> saveData;
			std::tie(std::ignore, saveData) = clipboardData->Serialise();
			auto handle = std::unique_ptr<void, decltype(&::GlobalFree)>(::GlobalAlloc(GMEM_MOVEABLE, saveData.size()), GlobalFree);
			if (!handle)
			{
				std::cerr << "cannot transfer save data: GlobalAlloc failed: " << ::GetLastError() << std::endl;
				return;
			}
			{
				auto data = std::unique_ptr<void, decltype(&::GlobalUnlock)>(::GlobalLock(handle.get()), ::GlobalUnlock);
				auto base = reinterpret_cast<char *>(data.get());
				std::copy(saveData.begin(), saveData.end(), base);
			}
			if (!::SetClipboardData(saveClipboardFormat, handle.get()))
			{
				std::cerr << "cannot transfer save data: SetClipboardData failed: " << ::GetLastError() << std::endl;
				return;
			}
			handle.release(); // windows owns it now
			std::cerr << "transferred save data" << std::endl;
		}

		static int TransferWatchWrapper(void *userdata, SDL_Event *event)
		{
			return reinterpret_cast<WindowsClipboardImpl *>(userdata)->TransferWatch(event);
		}

		int TransferWatch(SDL_Event *event)
		{
			// SDL documentation says we have to be very careful with what we do here because
			// the callback can come from any random thread, and we indeed are: WM_RENDERFORMAT
			// and WM_RENDERALLFORMATS are only posted to windows that have announced data on
			// the clipboard, and only our main thread ever owns a window, so we don't touch
			// the WindowsClipboardImpl outside of these events.
			switch (event->type)
			{
			case SDL_SYSWMEVENT:
				switch (event->syswm.msg->msg.win.msg)
				{
				case WM_RENDERFORMAT:
					if (event->syswm.msg->msg.win.wParam == saveClipboardFormat)
					{
						Transfer(); 
					}
					break; 

				case WM_RENDERALLFORMATS:
					{
						ClipboardSession cs(ourHwnd);
						if (cs)
						{
							if (ourHwnd && ::GetClipboardOwner() == ourHwnd)
							{
								Transfer();
							}
						}
						else
						{
							std::cerr << "cannot place save on clipboard: OpenClipboard failed: " << ::GetLastError() << std::endl;
						}
					}
					break;
				}
				break;
			}
			return 0;
		}

	public:
		WindowsClipboardImpl()
		{
			SDL_SysWMinfo info;
			SDL_VERSION(&info.version);
			SDL_GetWindowWMInfo(sdl_window, &info);
			ourHwnd = info.info.win.window;
			saveClipboardFormat = ::RegisterClipboardFormatW(Platform::WinWiden(clipboardFormatName).c_str());
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot register save clipboard format: RegisterClipboardFormatW failed: " << ::GetLastError() << std::endl;
				return;
			}
			std::cerr << "save clipboard format registered" << std::endl;
			SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
			SDL_AddEventWatch(&WindowsClipboardImpl::TransferWatchWrapper, this);
		}

		~WindowsClipboardImpl()
		{
			SDL_DelEventWatch(&WindowsClipboardImpl::TransferWatchWrapper, this);
			SDL_EventState(SDL_SYSWMEVENT, SDL_DISABLE);
		}

		void SetClipboardData() final override
		{
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot announce save on clipboard: save clipboard format not registered" << std::endl;
				return;
			}
			if (!clipboardData)
			{
				std::cerr << "cannot announce save on clipboard: no data to transfer" << std::endl;
				return;
			}
			ClipboardSession cs(ourHwnd);
			if (!cs)
			{
				std::cerr << "cannot announce save on clipboard: OpenClipboard failed: " << ::GetLastError() << std::endl;
				return;
			}
			if (!::EmptyClipboard())
			{
				std::cerr << "cannot announce save on clipboard: EmptyClipboard failed: " << ::GetLastError() << std::endl;
				return;
			}
			::SetClipboardData(saveClipboardFormat, NULL);
			std::cerr << "announced save on clipboard" << std::endl;
		}

		void GetClipboardData() final override
		{
			// Note that the data from the local clipboard is left alone if any error occurs so
			// the local clipboard keeps working even in the worst case.
			if (!saveClipboardFormat)
			{
				std::cerr << "cannot get save from clipboard: save clipboard format not registered" << std::endl;
				return;
			}
			ClipboardSession cs(ourHwnd);
			if (!cs)
			{
				std::cerr << "cannot get save from clipboard: OpenClipboard failed: " << ::GetLastError() << std::endl;
				return;
			}
			auto newSeqNumber = ::GetClipboardSequenceNumber();
			if (seqNumber && newSeqNumber && seqNumber == newSeqNumber)
			{
				std::cerr << "not getting save from clipboard, data unchanged" << std::endl;
				return;
			}
			seqNumber = newSeqNumber;
			// If we fail after this point, either there was no save on the system clipboard, or there was one
			// but we failed to load it. In either case, our local clipboard should be empty.
			clipboardData.reset();
			if (!::IsClipboardFormatAvailable(saveClipboardFormat))
			{
				std::cerr << "not getting save from clipboard: no data" << std::endl;
				return;
			}
			auto handle = ::GetClipboardData(saveClipboardFormat);
			if (!handle)
			{
				std::cerr << "cannot get save from clipboard: GetClipboardData failed: " << ::GetLastError() << std::endl;
				return;
			}
			auto size = ::GlobalSize(handle);
			auto data = std::unique_ptr<void, decltype(&::GlobalUnlock)>(::GlobalLock(handle), ::GlobalUnlock);
			if (!data)
			{
				std::cerr << "cannot get save from clipboard: GlobalLock failed: " << ::GetLastError() << std::endl;
				return;
			}
			auto base = reinterpret_cast<const char *>(data.get());
			try
			{
				clipboardData = std::make_unique<GameSave>(std::vector<char>(base, base + size));
			}
			catch (const ParseException &e)
			{
				std::cerr << "got bad save from clipboard: " << e.what() << std::endl;
				return;
			}
			std::cerr << "got save from clipboard" << std::endl;
		}
	};

	std::unique_ptr<ClipboardImpl> WindowsClipboardFactory()
	{
		return std::make_unique<WindowsClipboardImpl>();
	}
}
