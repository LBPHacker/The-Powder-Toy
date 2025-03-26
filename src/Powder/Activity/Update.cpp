#include "Update.hpp"
#include "Gui/Host.hpp"
#include "Config.h"
#include "bzip2/bz2wrap.h"
#include "common/platform/Platform.h"
#include "prefs/GlobalPrefs.h"
#include "client/http/Request.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size2 dialogSize = { 200, 70 };
	}

	Update::Update(Gui::Host &newHost, ByteString url) : View(newHost)
	{
		request = std::make_unique<http::Request>(url);
		request->Start();
	}

	void Update::Gui()
	{
		auto &g = GetHost();
		auto dialog = ScopedDialog("update", "Downloading update", dialogSize.X); // TODO-REDO_UI-TRANSLATE
		auto panel = ScopedVPanel("panel");
		SetSize(dialogSize.Y);
		std::optional<Progress> progress;
		if (request)
		{
			auto [ total, done ] = request->CheckProgress();
			progress = Progress{ Size(done), Size(total) };
		}
		Progressbar("progress", progress, g.GetCommonMetrics().size);
	}

	void Update::Die(ByteString title, ByteString message)
	{
		message = ByteString::Build(message, "\n\n", "Try downloading the new version manually."); // TODO-REDO_UI-TRANSLATE
		if constexpr (!USE_UPDATESERVER)
		{
			message = ByteString::Build(message, " Click \"OK\" to go to the website."); // TODO-REDO_UI-TRANSLATE
		}
		PushMessage(title, message, true, [this]() { // TODO-REDO_UI-TRANSLATE
			if constexpr (!USE_UPDATESERVER)
			{
				Platform::OpenURI(ByteString::Build(SERVER, "/Download.html"));
			}
			Exit();
		});
	}

	void Update::ApplyUpdate(ByteString data)
	{
		auto die = [this](ByteString message) {
			Die("Failed to apply update", message); // TODO-REDO_UI-TRANSLATE
		};
		if (data.size() < 8 || !(data[0] == 0x42 &&
		                         data[1] == 0x75 &&
		                         data[2] == 0x54 &&
		                         data[3] == 0x54))
		{
			die("Invalid header."); // TODO-REDO_UI-TRANSLATE
			return;
		}
		auto uncompressedSize = uint32_t(data[4]      ) |
		                        uint32_t(data[5] <<  8) |
		                        uint32_t(data[6] << 16) |
		                        uint32_t(data[7] << 24);
		std::vector<char> uncompressedData;
		if (BZ2WDecompress(uncompressedData, std::span<const char>(data).subspan(8), uncompressedSize) != BZ2WDecompressOk)
		{
			die("Corrupted compressed data"); // TODO-REDO_UI-TRANSLATE
			return;
		}
		auto &prefs = GlobalPrefs::Ref();
		prefs.Set("version.update", true);
		// TODO: better error handling
		if (!Platform::UpdateStart(uncompressedData))
		{
			prefs.Set("version.update", false);
			Platform::UpdateCleanup();
			die("Possibly failed to write update. The update may have been applied successfully; try restarting the game to check."); // TODO-REDO_UI-TRANSLATE
		}
	}

	void Update::HandleTick()
	{
		if (request && request->CheckDone())
		{
			try
			{
				auto [ status, data ] = request->Finish();
				if (status != 200)
				{
					throw http::RequestError(ByteString::Build("Server responded with status ", status)); // TODO-REDO_UI-TRANSLATE
				}
				ApplyUpdate(std::move(data));
			}
			catch (const http::RequestError &ex)
			{
				Die("Failed to download update", ex.what()); // TODO-REDO_UI-TRANSLATE
			}
			request.reset();
		}
	}

	void Update::PushUpdateConfirm(Gui::View &view, UpdateInfo info)
	{
		ByteStringBuilder updateMessage;
		std::optional<std::string> okText;
		if (Platform::CanUpdate())
		{
			updateMessage << "Are you sure you want to run the updater? The game restarts at the end of a successful update, please save any changes before updating.\n\n"; // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			updateMessage << "Click \"Continue\" to download the latest version from our website.\n\n"; // TODO-REDO_UI-TRANSLATE
			okText = "Continue"; // TODO-REDO_UI-TRANSLATE
		}
		updateMessage << "Current version: \bt";
		if constexpr (MOD)
		{
			updateMessage << "Mod " << MOD_ID << " "; // TODO-REDO_UI-TRANSLATE
		}
		if constexpr (SNAPSHOT)
		{
			updateMessage << "Snapshot " << APP_VERSION.build; // TODO-REDO_UI-TRANSLATE
		}
		else if constexpr (BETA)
		{
			updateMessage << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1] << " Beta, Build " << APP_VERSION.build; // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			updateMessage << DISPLAY_VERSION[0] << "." << DISPLAY_VERSION[1] << " Stable, Build " << APP_VERSION.build; // TODO-REDO_UI-TRANSLATE
		}
		updateMessage << "\x0E\nNew version: \bt"; // TODO-REDO_UI-TRANSLATE
		if (info.channel == UpdateInfo::channelBeta)
		{
			updateMessage << info.major << "." << info.minor << " Beta, Build " << info.build; // TODO-REDO_UI-TRANSLATE
		}
		else if (info.channel == UpdateInfo::channelSnapshot)
		{
			if constexpr (MOD)
			{
				updateMessage << "Mod version " << info.build; // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				updateMessage << "Snapshot " << info.build; // TODO-REDO_UI-TRANSLATE
			}
		}
		else if (info.channel == UpdateInfo::channelStable)
		{
			updateMessage << info.major << "." << info.minor << " Stable, Build " << info.build; // TODO-REDO_UI-TRANSLATE
		}
		if (info.changeLog.length())
		{
			updateMessage << "\x0E\n\nChangelog:\n" << info.changeLog.ToUtf8(); // TODO-REDO_UI-TRANSLATE
		}
		auto built = updateMessage.Build();
		while (!built.empty() && built.back() == '\n')
		{
			built.pop_back();
		}
		view.PushConfirm("Update available", built, okText, [&view, info](bool yes) { // TODO-REDO_UI-TRANSLATE
			if (yes)
			{
				if (Platform::CanUpdate())
				{
					view.PushAboveThis(std::make_shared<Update>(view.GetHost(), info.file));
				}
				else
				{
					Platform::OpenURI(info.file);
				}
			}
		});
	}
}
