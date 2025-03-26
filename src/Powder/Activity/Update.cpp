#include "Update.hpp"
#include "Gui/Host.hpp"
#include "Gui/ViewUtil.hpp"
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
		auto dialog = ScopedDialog("update", "DEFAULT_LANG_UPDATE_TITLE"_St, dialogSize.X);
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
		message = BuildString(message, "\n\n", "DEFAULT_LANG_UPDATE_TRYMANUALLY"_St);
		if constexpr (!USE_UPDATESERVER)
		{
			message = BuildString(message, " ", "DEFAULT_LANG_UPDATE_GOTOWEBSITE"_St);
		}
		PushMessage(title, message, true, [this]() {
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
			Die("DEFAULT_LANG_UPDATE_FAILEDTOAPPLY"_St(), message);
		};
		if (data.size() < 8 || !(data[0] == 0x42 &&
		                         data[1] == 0x75 &&
		                         data[2] == 0x54 &&
		                         data[3] == 0x54))
		{
			die("Invalid header.");
			return;
		}
		auto uncompressedSize = uint32_t(data[4]      ) |
		                        uint32_t(data[5] <<  8) |
		                        uint32_t(data[6] << 16) |
		                        uint32_t(data[7] << 24);
		std::vector<char> uncompressedData;
		if (BZ2WDecompress(uncompressedData, std::span<const char>(data).subspan(8), uncompressedSize) != BZ2WDecompressOk)
		{
			die("Corrupted compressed data");
			return;
		}
		auto &prefs = GlobalPrefs::Ref();
		prefs.Set("version.update", true);
		// TODO: better error handling
		if (!Platform::UpdateStart(uncompressedData))
		{
			prefs.Set("version.update", false);
			Platform::UpdateCleanup();
			die("DEFAULT_LANG_UPDATE_POSSIBLYFAILEDTOAPPLY"_St());
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
					throw http::RequestError(ByteString::Build("Server responded with status ", status));
				}
				ApplyUpdate(std::move(data));
			}
			catch (const http::RequestError &ex)
			{
				Die("DEFAULT_LANG_UPDATE_FAILEDTODOWNLOAD"_St(), ex.what());
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
			updateMessage << "DEFAULT_LANG_UPDATE_CONFIRM_CANUPDATE"_St();
		}
		else
		{
			updateMessage << "DEFAULT_LANG_UPDATE_CONFIRM_CANNOTUPDATE"_St();
			okText = "DEFAULT_LANG_UPDATE_CONFIRM_CANNOTUPDATE_CONTINUE"_St();
		}
		updateMessage << "\n\n";
		{
			ByteStringBuilder sb;
			sb << "\bt";
			if constexpr (MOD)
			{
				sb << "DEFAULT_LANG_UPDATE_MOD"_St(MOD_ID);
			}
			if constexpr (SNAPSHOT)
			{
				sb << "DEFAULT_LANG_UPDATE_SNAPSHOT"_St(APP_VERSION.build);
			}
			else if constexpr (BETA)
			{
				sb << "DEFAULT_LANG_UPDATE_BETA"_St(DISPLAY_VERSION[0], DISPLAY_VERSION[1], APP_VERSION.build);
			}
			else
			{
				sb << "DEFAULT_LANG_UPDATE_STABLE"_St(DISPLAY_VERSION[0], DISPLAY_VERSION[1], APP_VERSION.build);
			}
			updateMessage << "DEFAULT_LANG_UPDATE_CURRENTVERSION"_St(sb.Build());
		}
		updateMessage << "\x0E\n";
		{
			ByteStringBuilder sb;
			sb << "\bt";
			if (info.channel == UpdateInfo::channelBeta)
			{
				sb << "DEFAULT_LANG_UPDATE_BETA"_St(info.major, info.minor, info.build);
			}
			else if (info.channel == UpdateInfo::channelSnapshot)
			{
				if constexpr (MOD)
				{
					sb << "DEFAULT_LANG_UPDATE_MODVERSION"_St(info.build);
				}
				else
				{
					sb << "DEFAULT_LANG_UPDATE_SNAPSHOT"_St(info.build);
				}
			}
			else if (info.channel == UpdateInfo::channelStable)
			{
				sb << "DEFAULT_LANG_UPDATE_STABLE"_St(info.major, info.minor, info.build);
			}
			updateMessage << "DEFAULT_LANG_UPDATE_NEWVERSION"_St(sb.Build());
		}
		if (info.changeLog.length())
		{
			updateMessage << "\x0E\n\n" << "DEFAULT_LANG_UPDATE_CHANGELOG"_St() << "\n" << info.changeLog.ToUtf8();
		}
		auto built = updateMessage.Build();
		while (!built.empty() && built.back() == '\n')
		{
			built.pop_back();
		}
		view.PushConfirm("DEFAULT_LANG_UPDATE_CONFIRM"_St(), built, okText, [&view, info](bool yes) {
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
