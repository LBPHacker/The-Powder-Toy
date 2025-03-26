#include "Profile.hpp"
#include "Config.h"
#include "RequestView.hpp"
#include "Gui/Host.hpp"
#include "Gui/SdlAssert.hpp"
#include "Common/Format.hpp"
#include "client/Client.h"
#include "client/http/GetUserInfoRequest.h"
#include "client/http/LogoutRequest.h"
#include "common/platform/Platform.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size2 dialogSize = { 250, 250 };
		constexpr Gui::View::Size  avatarSize =           48;
	}

	Profile::Profile(Gui::Host &newHost, ByteString newUsername) :
		View(newHost),
		username(newUsername),
		openedAt(time(nullptr))
	{
		getUserInfoRequest = std::make_unique<http::GetUserInfoRequest>(username);
		getUserInfoRequest->Start();
		if (newHost.GetShowAvatars())
		{
			avatar = std::make_unique<Avatar>(GetHost());
			avatar->SetParameters(Avatar::Parameters{ username, avatarSize });
		}
	}

	Profile::~Profile() = default;

	Profile::DispositionFlags Profile::GetDisposition() const
	{
		return DispositionFlags::cancelMissing;
	}

	void Profile::Ok()
	{
		Exit();
	}

	void Profile::HandleTick()
	{
		if (logoutFuture.valid() && logoutFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				logoutFuture.get();
				Client::Ref().SetAuthUser(std::nullopt);
				Exit();
			}
			catch (const http::RequestError &)
			{
			}
		}
		if (getUserInfoRequest && getUserInfoRequest->CheckDone())
		{
			try
			{
				InfoData infoData = { getUserInfoRequest->Finish() };
				infoData.biography = infoData.info.biography.ToUtf8();
				int32_t cutAt = std::min(1000, int32_t(infoData.biography.size())); // TODO-REDO_UI: factor out
				auto range = std::string_view(infoData.biography).substr(0, cutAt);
				while (cutAt)
				{
					auto ch = Utf8Prev(range, cutAt);
					if (ch.valid)
					{
						break;
					}
					cutAt -= 1;
				}
				infoData.biography = infoData.biography.substr(0, cutAt);
				info = std::move(infoData);
			}
			catch (const http::RequestError &ex)
			{
				info = InfoError{ ex.what() };
			}
			getUserInfoRequest.reset();
		}
		if (avatar)
		{
			avatar->HandleTick();
		}
	}

	void Profile::Gui()
	{
		auto &g = GetHost();
		auto profile = ScopedDialog("profile", username, dialogSize.X);
		SetSize(dialogSize.Y);
		SetPadding(0);
		SetSpacing(0);
		if (std::holds_alternative<InfoLoading>(info))
		{
			Spinner("loading", GetHost().GetCommonMetrics().bigSpinner);
		}
		else if (auto *infoData = std::get_if<InfoData>(&info))
		{
			if (auto user = Client::Ref().GetAuthUser(); user && user->Username == username)
			{
				{
					auto manage = ScopedHPanel("manage");
					SetSize(GetHost().GetCommonMetrics().heightToFitSize);
					SetPadding(Common{});
					SetSpacing(Common{});
					if (Button("edit", "Edit profile")) // TODO-REDO_UI-TRANSLATE
					{
						Platform::OpenURI(ByteString::Build(SERVER, "/Profile.html"));
					}
					if (Button("logout", "Log out")) // TODO-REDO_UI-TRANSLATE
					{
						auto requestView = MakeRequestView(GetHost(), "Logging out", std::make_unique<http::LogoutRequest>()); // TODO-REDO_UI-TRANSLATE
						logoutFuture = requestView->GetFuture();
						requestView->Start();
						PushAboveThis(requestView);
					}
				}
				Separator("manageSeparator");
			}
			auto panel = ScopedScrollpanel("panel");
			SetMaxSize(MaxSizeFitParent{});
			SetPadding(GetHost().GetCommonMetrics().padding + 1);
			SetSpacing(Common{});
			SetAlignment(Gui::Alignment::top);
			auto &userInfo = infoData->info;
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			{
				auto head = ScopedHPanel("head");
				SetParentFillRatio(0);
				SetSpacing(Common{});
				{
					auto column = ScopedVPanel("column");
					SetAlignment(Gui::Alignment::top);
					SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
					SetSpacing(Common{});
					BeginText("age", ByteString::Build("\bgAge: \x0E", userInfo.age), TextFlags::autoHeight | TextFlags::selectable); // TODO-REDO_UI-TRANSLATE
					SetParentFillRatio(0);
					EndText();
					BeginText("location", ByteString::Build("\bgLocation: \x0E", userInfo.location.ToUtf8()), TextFlags::multiline | TextFlags::autoHeight | TextFlags::selectable); // TODO-REDO_UI-TRANSLATE
					SetParentFillRatio(0);
					EndText();
					BeginText("website", ByteString::Build("\bgWebsite: \x0E", userInfo.website), TextFlags::multiline | TextFlags::autoHeight | TextFlags::selectable); // TODO-REDO_UI-TRANSLATE
					SetParentFillRatio(0);
					EndText();
					BeginText("registeredAt", ByteString::Build("\bgRegistered: \x0E", userInfo.registeredAt ? FormatRelative(*userInfo.registeredAt, openedAt) : ByteString("a long time ago")), TextFlags::autoHeight | TextFlags::selectable); // TODO-REDO_UI-TRANSLATE
					SetParentFillRatio(0);
					EndText();
				}
				if (avatar)
				{
					auto avatarPanel = ScopedVPanel("avatar");
					SetParentFillRatio(0);
					SetAlignment(Gui::Alignment::top);
					auto textureComponent = ScopedComponent("texture");
					SetSize(avatarSize);
					SetSizeSecondary(avatarSize);
					if (auto texture = avatar->GetTexture())
					{
						auto &g = GetHost();
						auto r = GetRect();
						g.DrawStaticTexture(r, *texture, 0xFFFFFFFF_argb);
					}
					else
					{
						Spinner("loading", g.GetCommonMetrics().smallSpinner);
					}
				}
			}
			BeginText("saves", ByteString::Build(
				"\bgSaves:\n", // TODO-REDO_UI-TRANSLATE
				"  \bgCount: \x0E", userInfo.saveCount, "\n", // TODO-REDO_UI-TRANSLATE
				"  \bgAverage score: \x0E", userInfo.averageScore, "\n", // TODO-REDO_UI-TRANSLATE
				"  \bgHighest score: \x0E", userInfo.highestScore // TODO-REDO_UI-TRANSLATE
			), TextFlags::autoHeight | TextFlags::multiline | TextFlags::selectable);
			SetParentFillRatio(0);
			EndText();
			BeginText("forum", ByteString::Build(
				"\bgForum:\n", // TODO-REDO_UI-TRANSLATE
				"  \bgTopics: \x0E", userInfo.topicCount, "\n" // TODO-REDO_UI-TRANSLATE
				"  \bgReplies: \x0E", userInfo.topicReplies, "\n" // TODO-REDO_UI-TRANSLATE
				"  \bgReputation: \x0E", userInfo.reputation // TODO-REDO_UI-TRANSLATE
			), TextFlags::autoHeight | TextFlags::multiline | TextFlags::selectable);
			SetParentFillRatio(0);
			EndText();
			BeginText("biography", ByteString::Build("\bgBiography: \x0E", infoData->biography), TextFlags::multiline | TextFlags::autoHeight | TextFlags::selectable); // TODO-REDO_UI-TRANSLATE
			SetParentFillRatio(0);
			EndText();
		}
		else
		{
			auto &infoError = std::get<InfoError>(info);
			BeginText("error", infoError.message, TextFlags::multiline);
			EndText();
		}
	}
}
