#include "Preview.hpp"
#include "Main.hpp"
#include "BrowserCommon.hpp"
#include "Config.h"
#include "Game.hpp"
#include "RequestView.hpp"
#include "Votebars.hpp"
#include "Common/Format.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/StaticTexture.hpp"
#include "Gui/ViewUtil.hpp"
#include "SimulationConfig.h"
#include "Simulation/RenderThumbnail.hpp"
#include "graphics/VideoBuffer.h"
#include "client/http/FavouriteSaveRequest.h"
#include "client/http/GetSaveRequest.h"
#include "client/http/GetSaveDataRequest.h"
#include "client/http/GetCommentsRequest.h"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "common/platform/Platform.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::Ticks queryDebounce                    = 600;
		constexpr int32_t commentsPageSize                    =  20;
		constexpr Gui::View::Size authorAvatarSize            =  39;
		constexpr Gui::View::Size commentAvatarSize           =  26;
		constexpr Gui::View::Size commentPaginationArrowWidth =  20;
		constexpr Gui::View::Size descriptionHeight           = 159;
		constexpr Gui::View::Size commentsWidth               = 210;

		Rgba8 GetAuthorColor(ByteString authorName, User::Elevation authorElevation, bool authorIsBanned, const SaveInfo *saveInfo)
		{
			auto user = Client::Ref().GetAuthUser();
			auto authorColor = Gui::colorWhite;
			if      (authorElevation != User::ElevationNone || authorName == "jacobot") authorColor = Gui::colorTeal    ;
			else if (authorIsBanned                                                   ) authorColor = Gui::colorGray    ;
			else if (user.UserID && user.Username == authorName                       ) authorColor = Gui::colorYellow  ;
			else if (saveInfo && saveInfo->GetUserName() == authorName                ) authorColor = Gui::colorLightRed;
			return authorColor.WithAlpha(255);
		}
	}

	Preview::Preview(Gui::Host &newHost, int newId, int newVersion, bool newQuickOpen, std::optional<PlaneAdapter<std::vector<uint32_t>>> smallThumbnail) :
		View(newHost),
		id(newId),
		version(newVersion),
		quickOpen(newQuickOpen),
		openedAt(time(nullptr)),
		showAvatars(newHost.GetShowAvatars())
	{
		if (smallThumbnail && !quickOpen)
		{
			thumbnail = ThumbnailData{ std::make_unique<Gui::StaticTexture>(GetHost(), std::move(*smallThumbnail)) };
		}
		getSaveRequest = std::make_unique<http::GetSaveRequest>(id, version);
		getSaveRequest->Start();
		getSaveDataRequest = std::make_unique<http::GetSaveDataRequest>(id, version);
		getSaveDataRequest->Start();
		if (!quickOpen)
		{
			BeginGetComments({ 0 }, false);
		}
	}

	Preview::~Preview() = default;

	Preview::CanApplyKind Preview::CanApply() const
	{
		if (quickOpen)
		{
			if (HasQuickOpenFailed())
			{
				return CanApplyKind::onlyApply;
			}
			return CanApplyKind::onlyCancel;
		}
		auto *infoData = std::get_if<InfoData>(&info);
		if (infoData && save)
		{
			return CanApplyKind::yes;
		}
		return CanApplyKind::no;
	}

	void Preview::Apply()
	{
		if (quickOpen && HasQuickOpenFailed())
		{
			Exit();
			return;
		}
		auto &infoData = std::get<InfoData>(info);
		auto saveInfo = std::move(infoData.info);
		saveInfo->SetGameSave(std::move(save));
		info = InfoExtracted{};
		game->SetSave(std::move(saveInfo), game->GetLoadWithoutPressure());
		stacks->SetHostStack(gameStack);
		Exit();
	}

	bool Preview::Cancel()
	{
		Exit();
		return true;
	}

	void Preview::FormatCommentAuthorNames()
	{
		SaveInfo *saveInfo = nullptr;
		if (auto *infoData = std::get_if<InfoData>(&info))
		{
			saveInfo = infoData->info.get();
		}
		if (auto *commentsData = std::get_if<CommentsData>(&comments))
		{
			for (auto &item : commentsData->items)
			{
				item.authorColor = GetAuthorColor(item.authorName, item.authorElevation, item.authorIsBanned, saveInfo);
			}
		}
	}

	void Preview::HandleTick()
	{
		if (favorFuture.valid())
		{
			try
			{
				favorFuture.get();
				std::get<InfoData>(info).info->Favourite = requestedFavorState;
			}
			catch (const http::RequestError &)
			{
			}
			favorFuture = {};
		}
		if (getSaveRequest && getSaveRequest->CheckDone())
		{
			try
			{
				auto saveInfo = getSaveRequest->Finish();
				// This is a workaround for a bug on the TPT server where the wrong 404 save is returned
				// Redownload the .cps file for a fixed version of the 404 save
				if (saveInfo->GetID() == 404 && id != 404)
				{
					save.reset();
					getSaveDataRequest = std::make_unique<http::GetSaveDataRequest>(2157797, 0);
					getSaveDataRequest->Start();
					thumbnail = ThumbnailLoading{};
					getCommentsRequest.reset();
					comments = CommentsError{ "Save not found" };
					commentsPaginationContext.SetPageCount(1);
					saveNotFoundHack = true;
					info = InfoError{ "Save not found" };
				}
				else
				{
					if (showAvatars && !quickOpen)
					{
						authorAvatar = std::make_unique<Avatar>(GetHost());
						authorAvatar->SetParameters(Avatar::Parameters{ saveInfo->GetUserName(), authorAvatarSize });
					}
					info = InfoData{ std::move(saveInfo) };
				}
			}
			catch (const http::RequestError &ex)
			{
				info = InfoError{ ex.what() };
			}
			getSaveRequest.reset();
			FormatCommentAuthorNames();
		}
		if (getSaveDataRequest && getSaveDataRequest->CheckDone())
		{
			try
			{
				save = std::make_unique<GameSave>(getSaveDataRequest->Finish());
			}
			catch (const ParseException &ex)
			{
				thumbnail = ThumbnailError{ ex.what() };
			}
			catch (const http::RequestError &ex)
			{
				thumbnail = ThumbnailError{ ex.what() };
			}
			getSaveDataRequest.reset();
			if (save && !quickOpen)
			{
				thumbnailFuture = Simulation::RenderThumbnail(
					threadPool,
					std::make_unique<GameSave>(*save),
					GetThumbnailSize(),
					RendererSettings::decorationAntiClickbait,
					true
				);
			}
		}
		if (getCommentsRequest && getCommentsRequest->CheckDone())
		{
			try
			{
				std::vector<Comment> items;
				for (auto &item : getCommentsRequest->Finish())
				{
					auto &previewItem = items.emplace_back();
					if (showAvatars)
					{
						previewItem.avatar = std::make_unique<Avatar>(GetHost());
						previewItem.avatar->SetParameters(Avatar::Parameters{ item.authorName, commentAvatarSize });
					}
					static_cast<::Comment &>(previewItem) = item;
				}
				comments = CommentsData{ std::move(items) };
				FormatCommentAuthorNames();
			}
			catch (const http::RequestError &ex)
			{
				comments = CommentsError{ ex.what() };
			}
			scrollCommentsToEnd = queuedScrollCommentsToEnd;
			queuedScrollCommentsToEnd = false;
			getCommentsRequest.reset();
		}
		if (thumbnailFuture.valid() && thumbnailFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			thumbnail = ThumbnailData{ std::make_unique<Gui::StaticTexture>(GetHost(), thumbnailFuture.get(), true) };
		}
		if (queuedCommentsQuery &&
		    queuedCommentsQuery->queuedAt + queryDebounce <= GetHost().GetLastTick() &&
		    !std::holds_alternative<CommentsLoading>(comments))
		{
			BeginGetComments(queuedCommentsQuery->commentsQuery, false);
			queuedCommentsQuery.reset();
		}
		if (auto *infoData = std::get_if<InfoData>(&info); infoData && commentsPageCountStale)
		{
			commentsPageCountStale = false;
			int32_t pageCount = 1;
			if (!saveNotFoundHack)
			{
				pageCount = std::max(1, (infoData->info->Comments + commentsPageSize - 1) / commentsPageSize);
			}
			commentsPaginationContext.SetPageCount(pageCount);
		}
		if (authorAvatar)
		{
			authorAvatar->HandleTick();
		}
		if (auto *commentsData = std::get_if<CommentsData>(&comments))
		{
			for (auto &item : commentsData->items)
			{
				if (item.avatar)
				{
					item.avatar->HandleTick();
				}
			}
		}
		if (quickOpen && save)
		{
			Apply();
		}
	}

	Preview::Size2 Preview::GetThumbnailSize() const
	{
		return RES / 2;
	}

	void Preview::GuiInfo()
	{
		BeginVPanel("info");
		if (std::holds_alternative<InfoLoading>(info))
		{
			Spinner("loading", GetHost().GetCommonMetrics().smallSpinner);
		}
		else if (auto *infoData = std::get_if<InfoData>(&info))
		{
			auto &g = GetHost();
			auto &saveInfo = *infoData->info;
			BeginHPanel("head");
			SetMaxSizeSecondary(MaxSizeFitParent{});
			auto padding = g.GetCommonMetrics().padding;
			SetPadding(padding + 1);
			SetSpacing(padding + 1);
			SetParentFillRatio(0);
			if (authorAvatar)
			{
				BeginComponent("avatar");
				{
					SetSize(authorAvatarSize);
					SetSizeSecondary(authorAvatarSize);
					auto r = GetRect();
					if (auto *texture = authorAvatar->GetTexture())
					{
						g.DrawStaticTexture(r, *texture, 0xFFFFFFFF_argb);
					}
				}
				EndComponent();
			}
			BeginVPanel("rest");
			{
				BeginHPanel("row1");
				{
					BeginComponent("left");
					SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
					BeginText("title", saveInfo.GetName().ToUtf8(), "...", TextFlags::selectable | TextFlags::autoWidth);
					EndText();
					EndComponent();
					BeginVPanel("right");
					SetParentFillRatio(0);
					SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
					BeginVPanel("votebars");
					SetSize(15);
					SetSizeSecondary(60);
					{
						auto rUp = GetRect();
						rUp.size.Y = 7;
						auto rDown = rUp;
						rDown.pos.Y += 6;
						g.DrawRect(rUp, 0xFF808080_argb);
						g.DrawRect(rDown, 0xFF808080_argb);
						rUp = rUp.Inset(1);
						rDown = rDown.Inset(1);
						g.FillRect(rUp, voteUpBackground);
						g.FillRect(rDown, voteDownBackground);
						rUp = rUp.Inset(1);
						rDown = rDown.Inset(1);
						auto [ sizeUp, sizeDown ] = ScaleVoteBars(rUp.size.X, rDown.size.X, saveInfo.GetVotesUp(), saveInfo.GetVotesDown());
						rUp.pos.X += rUp.size.X - sizeUp;
						rUp.size.X = sizeUp;
						rDown.pos.X += rDown.size.X - sizeDown;
						rDown.size.X = sizeDown;
						g.FillRect(rUp, voteUpBar);
						g.FillRect(rDown, voteDownBar);
					}
					EndPanel();
					EndPanel();
				}
				EndPanel();
				BeginHPanel("row2");
				{
					SetSize(12);
					BeginComponent("left");
					SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
					BeginText("author", saveInfo.GetUserName(), "...", TextFlags::selectable | TextFlags::autoWidth, GetAuthorColor(
						saveInfo.GetUserName(),
						saveInfo.authorElevation,
						saveInfo.authorIsBanned,
						&saveInfo
					));
					EndText();
					EndComponent();
					BeginComponent("right");
					SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
					BeginText("views", ByteString::Build("\bgViews: \x0E", saveInfo.Views), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
					EndText();
					EndComponent();
				}
				EndPanel();
				BeginHPanel("row3");
				{
					SetSize(12);
					BeginComponent("left");
					SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
					BeginText("timestamp", ByteString::Build(saveInfo.updatedDate == saveInfo.createdDate ? "\bgCreated: \x0E" : "\bgUpdated: \x0E", FormatRelative(saveInfo.updatedDate, openedAt)), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
					EndText();
					EndComponent();
					BeginComponent("right");
					SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
					BeginText("saveid", ByteString::Build("\bgSave ID: \x0E", id), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
					EndText();
					EndComponent();
				}
				EndPanel();
			}
			EndPanel();
			EndPanel();
			Separator("descriptionseparator");
			BeginScrollpanel("descriptionpanel");
			SetMaxSize(MaxSizeFitParent{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::top);
			BeginText("description", saveInfo.GetDescription().ToUtf8(), TextFlags::multiline | TextFlags::selectable | TextFlags::autoHeight);
			auto commonPadding = g.GetCommonMetrics().padding + 1;
			SetTextPadding(commonPadding, commonPadding, commonPadding, commonPadding);
			EndText();
			EndScrollpanel();
		}
		else if (auto *infoError = std::get_if<InfoError>(&info))
		{
			BeginText("error", ByteString::Build(infoError->message), TextFlags::multiline);
			EndText();
		}
		EndPanel();
	}

	void Preview::GuiSave()
	{
		BeginVPanel("save");
		BeginComponent("thumbnail");
		auto thumbnailSize = GetThumbnailSize();
		SetSize(thumbnailSize.Y);
		SetSizeSecondary(thumbnailSize.X);
		auto &g = GetHost();
		{
			auto r = GetRect();
			if (std::holds_alternative<ThumbnailLoading>(thumbnail))
			{
				Spinner("loading", GetHost().GetCommonMetrics().bigSpinner);
			}
			else if (auto *thumbnailData = std::get_if<ThumbnailData>(&thumbnail))
			{
				g.DrawStaticTexture(Rect{ r.pos, thumbnailSize }, *thumbnailData->texture, 0xFFFFFFFF_argb);
			}
			else
			{
				auto &thumbnailError = std::get<ThumbnailError>(thumbnail);
				BeginText("error", ByteString::Build(Gui::iconBrokenImage, "\n", thumbnailError.message), TextFlags::multiline);
				EndText();
			}
		}
		EndComponent();
		Separator("separator");
		BeginVPanel("info");
		SetSize(descriptionHeight);
		GuiInfo();
		Separator("manageseparator");
		GuiManage();
		EndPanel();
		EndPanel();
	}

	void Preview::GuiCommentsItems()
	{
		BeginVPanel("items");
		{
			int32_t overscroll = 0;
			if (std::holds_alternative<CommentsLoading>(comments))
			{
				Spinner("loading", GetHost().GetCommonMetrics().smallSpinner);
			}
			else if (auto *commentsData = std::get_if<CommentsData>(&comments))
			{
				BeginScrollpanel("scrollpanel");
				if (scrollCommentsToEnd)
				{
					ScrollpanelSetScroll({ 0, -Gui::maxSize });
				}
				overscroll = ScrollpanelGetOverscroll().Y;
				SetAlignment(Gui::Alignment::top);
				SetMaxSize(MaxSizeFitParent{});
				if (commentsData->items.empty())
				{
					Text("No comments found");
				}
				else
				{
					int32_t index = 0;
					for (auto &item : commentsData->items)
					{
						BeginVPanel(index);
						SetParentFillRatio(0);
						auto padding = GetHost().GetCommonMetrics().padding + 1;
						constexpr Size avatarPadding = 5;
						SetPadding(padding);
						SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
						BeginHPanel("head");
						SetSize(14);
						{
							constexpr Size paddingTotal = 2;
							Size paddingAfter = item.makeSpaceForAvatar ? 0 : 1;
							BeginComponent("left");
							SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
							BeginText("author", item.authorName, "...", TextFlags::selectable, item.authorColor);
							SetTextPadding(item.avatar ? (commentAvatarSize + avatarPadding) : 0, 0, paddingTotal - paddingAfter, paddingAfter);
							EndText();
							EndComponent();
							BeginComponent("right");
							SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
							SetParentFillRatio(0);
							BeginText("timestamp", FormatRelative(item.timestamp, openedAt), TextFlags::autoWidth, Gui::colorYellow.WithAlpha(255));
							SetTextPadding(0, 0, paddingTotal - paddingAfter, paddingAfter);
							EndText();
							EndComponent();
						}
						EndPanel();
						BeginText("text", item.content.ToUtf8(), TextFlags::multiline | TextFlags::autoHeight | TextFlags::selectable, 0xFFB4B4B4_argb);
						if (item.avatar)
						{
							SetTextFirstLineIndent(commentAvatarSize + avatarPadding);
						}
						item.makeSpaceForAvatar = item.avatar && TextGetWrappedLines() > 1;
						if (item.makeSpaceForAvatar)
						{
							SetTextPadding(0, 0, avatarPadding - 2, 0);
						}
						EndText();
						if (item.avatar)
						{
							auto &g = GetHost();
							auto r = GetRect();
							r.pos += Pos2{ padding, padding };
							r.size = { commentAvatarSize, commentAvatarSize };
							if (auto *texture = item.avatar->GetTexture())
							{
								// TODO-REDO_UI: use a layered component to prevent cursor confusion
								g.DrawStaticTexture(r, *texture, 0xFFFFFFFF_argb);
							}
						}
						EndPanel();
						Separator(index + 1);
						index += 2;
					}
				}
				EndScrollpanel();
			}
			else
			{
				auto &commentsError = std::get<CommentsError>(comments);
				BeginText("error", commentsError.message, TextFlags::multiline);
				EndText();
			}
			if (overscroll < 0)
			{
				LoadNextCommentsPage();
			}
			if (overscroll > 0)
			{
				LoadPrevCommentsPage();
			}
		}
		EndPanel();
	}

	void Preview::GuiCommentsPagination()
	{
		BeginHPanel("pagination");
		SetSize(Common{});
		{
			BeginButton("prev", Gui::iconLeft, ButtonFlags::none);
			SetEnabled(CanLoadPrevCommentsPage());
			SetSize(commentPaginationArrowWidth);
			if (EndButton())
			{
				LoadPrevCommentsPage();
			}
			struct Rw
			{
				Preview &view;
				int32_t Read() { return view.commentsQuery.page; }
				void Write(int32_t value) { view.QueueCommentsQuery({ value }); }
			};
			commentsPaginationContext.Gui("input", Rw{ *this });
			BeginButton("next", Gui::iconRight, ButtonFlags::none);
			SetEnabled(CanLoadNextCommentsPage());
			SetSize(commentPaginationArrowWidth);
			if (EndButton())
			{
				LoadNextCommentsPage();
			}
		}
		EndPanel();
	}

	void Preview::GuiManage()
	{
		BeginHPanel("manage");
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetPadding(Common{});
		SetSpacing(Common{});
		auto *infoData = std::get_if<InfoData>(&info);
		BeginButton("open", ByteString::Build(Gui::iconOpen, " Open"), ButtonFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		SetEnabled(infoData);
		if (EndButton() && CanApply() == CanApplyKind::yes)
		{
			Apply();
		}
		{
			auto favorButtonFlags = ButtonFlags::autoWidth;
			if (infoData && infoData->info->Favourite)
			{
				favorButtonFlags = favorButtonFlags | ButtonFlags::stuck;
			}
			BeginButton("favor", ByteString::Build(Gui::iconFavorite, " Favor"), favorButtonFlags); // TODO-REDO_UI-TRANSLATE
		}
		SetEnabled(infoData);
		if (EndButton() && infoData)
		{
			auto request = std::make_unique<http::FavouriteSaveRequest>(infoData->info->GetID(), !infoData->info->Favourite);
			requestedFavorState = request->Favourite();
			auto requestView = MakeRequestView(GetHost(), "\biSaving favorite", std::move(request)); // TODO-REDO_UI-TRANSLATE
			favorFuture = requestView->GetFuture();
			requestView->Start();
			PushAboveThis(requestView);
		}
		BeginButton("report", ByteString::Build(Gui::iconWarning, " Report"), ButtonFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		SetEnabled(infoData);
		if (EndButton() && infoData)
		{
			// TODO
		}
		BeginButton("openinbrowser", ByteString::Build(Gui::iconOpen, " Open in browser"), ButtonFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		SetEnabled(infoData);
		if (EndButton() && infoData)
		{
			ByteStringBuilder sb;
			sb << SERVER << "/Browse/View.html?ID=" << infoData->info->GetID();
			if (auto version = infoData->info->Version)
			{
				sb << "&Date=" << version;
			}
			Platform::OpenURI(sb.Build());
		}
		EndPanel();
	}

	void Preview::GuiCommentsPost()
	{
		BeginHPanel("post");
		SetSize(Common{});
		SetSpacing(Common{});
		Textbox("input", commentInput, "[comment]");
		if (Button("submit", "Submit", GetHost().GetCommonMetrics().smallButton)) // TODO-REDO_UI-TRANSLATE
		{
			commentInput.clear();
		}
		EndPanel();
	}

	void Preview::GuiComments()
	{
		BeginVPanel("comments");
		SetSize(commentsWidth);
		GuiCommentsItems();
		Separator("itemsseparator");
		BeginVPanel("bottom");
		SetParentFillRatio(0);
		SetPadding(Common{});
		SetSpacing(Common{});
		GuiCommentsPagination();
		GuiCommentsPost();
		EndPanel();
		EndPanel();
	}

	bool Preview::CanLoadNextCommentsPage() const
	{
		return commentsQuery.page < commentsPaginationContext.GetPageCount() - 1;
	}

	bool Preview::CanLoadPrevCommentsPage() const
	{
		return commentsQuery.page > 0;
	}

	void Preview::LoadNextCommentsPage()
	{
		if (CanLoadNextCommentsPage())
		{
			BeginGetComments({ commentsQuery.page + 1 }, false);
		}
	}

	void Preview::LoadPrevCommentsPage()
	{
		if (CanLoadPrevCommentsPage())
		{
			BeginGetComments({ commentsQuery.page - 1 }, true);
		}
	}

	void Preview::QueueCommentsQuery(CommentsQuery newCommentsQuery)
	{
		queuedCommentsQuery = { newCommentsQuery, GetHost().GetLastTick() };
	}

	bool Preview::HasQuickOpenFailed() const
	{
		return std::holds_alternative<ThumbnailError>(thumbnail) ||
			   std::holds_alternative<InfoError>(info);
	}

	void Preview::GuiQuickOpen()
	{
		CommondialogState state = CommondialogStateProgress{};
		if (HasQuickOpenFailed())
		{
			StringView message;
			if (auto *infoError = std::get_if<InfoError>(&info))
			{
				message = infoError->message;
			}
			else
			{
				message = std::get<ThumbnailError>(thumbnail).message;
			}
			state = CommondialogStateFailed{ message };
		}
		Commondialog("dialog", "\biOpening save", state); // TODO-REDO_UI-TRANSLATE
	}

	void Preview::Gui()
	{
		if (quickOpen)
		{
			GuiQuickOpen();
			return;
		}
		BeginModal("preview", GetHost().GetSize().OriginRect());
		SetExitWhenRootMouseDown(true);
		SetPrimaryAxis(Axis::horizontal);
		GuiSave();
		Separator("separator");
		GuiComments();
		EndModal();
	}

	void Preview::BeginGetComments(CommentsQuery newCommentsQuery, bool newQueuedScrollCommentsToEnd)
	{
		queuedScrollCommentsToEnd = newQueuedScrollCommentsToEnd;
		commentsPageCountStale = true;
		commentsQuery = newCommentsQuery;
		commentsPaginationContext.ForceRead();
		comments = CommentsLoading{};
		getCommentsRequest = std::make_unique<http::GetCommentsRequest>(id, commentsQuery.page * commentsPageSize, commentsPageSize);
		getCommentsRequest->Start();
	}

	void Preview::HandleAfterFrame()
	{
		scrollCommentsToEnd = false;
	}
}
