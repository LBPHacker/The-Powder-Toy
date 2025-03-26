#include "Preview.hpp"
#include "Main.hpp"
#include "Avatar.hpp"
#include "Profile.hpp"
#include "BrowserCommon.hpp"
#include "Config.h"
#include "Game.hpp"
#include "RequestView.hpp"
#include "Votebars.hpp"
#include "Common/Format.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"
#include "Gui/ViewUtil.hpp"
#include "SimulationConfig.h"
#include "Simulation/RenderThumbnail.hpp"
#include "graphics/VideoBuffer.h"
#include "client/http/AddCommentRequest.h"
#include "client/http/FavouriteSaveRequest.h"
#include "client/http/GetSaveRequest.h"
#include "client/http/GetSaveDataRequest.h"
#include "client/http/GetCommentsRequest.h"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "common/platform/Platform.h"
#include "common/tpt-rand.h"

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
		constexpr Gui::View::Size commentInputMaxHeight       = 107;

		Rgba8 GetAuthorColor(ByteString authorName, User::Elevation authorElevation, bool authorIsBanned, const SaveInfo *saveInfo)
		{
			auto user = Client::Ref().GetAuthUser();
			auto authorColor = Gui::colorWhite;
			if      (authorElevation != User::ElevationNone      ) authorColor = Gui::colorTeal    ;
			else if (authorIsBanned                              ) authorColor = Gui::colorGray    ;
			else if (user && user->Username == authorName        ) authorColor = Gui::colorYellow  ;
			else if (saveInfo && saveInfo->userName == authorName) authorColor = Gui::colorLightRed;
			return authorColor.WithAlpha(255);
		}
	}

	Preview::Preview(
		Game &newGame,
		int32_t newId,
		int32_t newVersion,
		bool newQuickOpen,
		std::optional<PlaneAdapter<std::vector<pixel>>> earlyThumbnail
	) :
		View(newGame.GetHost()),
		game(newGame),
		threadPool(newGame.GetThreadPool()),
		id(newId),
		version(newVersion),
		quickOpen(newQuickOpen),
		openedAt(time(nullptr)),
		showAvatars(GetHost().GetShowAvatars())
	{
		if (earlyThumbnail && !quickOpen)
		{
			thumbnail = ThumbnailData{ std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(*earlyThumbnail)) };
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

	Preview::DispositionFlags Preview::GetDisposition() const
	{
		if (quickOpen)
		{
			if (HasQuickOpenFailed())
			{
				return DispositionFlags::cancelMissing;
			}
			return DispositionFlags::okMissing;
		}
		auto *infoData = std::get_if<InfoData>(&info);
		if (infoData && save)
		{
			return DispositionFlags::none;
		}
		return DispositionFlags::okDisabled;
	}

	void Preview::Ok()
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
		game.SetSave(std::move(saveInfo), game.GetIncludePressure());
		stacks->SelectStack(gameStack);
		Exit();
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

	void Preview::UpdateCommentAdvice()
	{
		auto lower = ByteString(commentInput).ToLower();
		struct AdviceSet
		{
			std::set<uint64_t> triggers;
			std::vector<ByteString> variants;
		};
		static const std::vector<AdviceSet> adviceSets = {
			{ {
				UINT64_C(0x906348C11A5939A6),
				UINT64_C(0x0FDC68E823738905),
			}, {
				"Stolen? Report the save instead",
				"Please report stolen saves",
			} },
			{ {
				UINT64_C(0x312CEC00E33F1AB5),
			}, {
				"Do not ask for votes",
			} },
			{ {
				UINT64_C(0x86585B78DAFE862A),
				UINT64_C(0x6F0B9EC3C2BAC013),
				UINT64_C(0x46F05E18E29ECD13),
				UINT64_C(0x8FC7B5E8E955A6D0),
				UINT64_C(0xCA0DC36757607E74),
				UINT64_C(0x4C6D7290ACC036BF),
				UINT64_C(0x600024F2D4F9DD79),
				UINT64_C(0x67EA3CBD93F2F225),
				UINT64_C(0x561D9EECDE047202),
			}, {
				"Please do not swear",
				"Bad language may be deleted",
			} },
		};
		for (auto &adviceSet : adviceSets)
		{
			for (int32_t i = 1; i <= 7; ++i)
			{
				for (int32_t j = 0; j < int32_t(lower.size()) - i + 1; ++j)
				{
					// http://www.isthe.com/chongo/tech/comp/fnv/
					auto hash = UINT64_C(14695981039346656037);
					for (int32_t k = 0U; k < i; ++k)
					{
						hash ^= uint64_t(lower[j + k]);
						hash *= UINT64_C(1099511628211);
					}
					if (adviceSet.triggers.find(hash) != adviceSet.triggers.end())
					{
						auto &pool = adviceSet.variants;
						if (!(commentAdvice && commentAdvice->variants == &pool))
						{
							commentAdvice = CommentAdvice{ &pool, int32_t(interfaceRng() % pool.size()) };
						}
						return;
					}
				}
			}
		}
		commentAdvice.reset();
	}

	void Preview::HandleTick()
	{
		if (favorFuture.valid() && favorFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				favorFuture.get();
				std::get<InfoData>(info).info->Favourite = requestedFavorState;
			}
			catch (const http::RequestError &)
			{
			}
		}
		if (postCommentFuture.valid() && postCommentFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				postCommentFuture.get();
				commentInput.clear();
				UpdateCommentAdvice();
				BeginGetComments({ 0 }, false);
			}
			catch (const http::RequestError &)
			{
			}
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
					RendererSettings::decorationAntiClickbait,  // TODO-REDO_UI-FUTURE: decide if this is appropriate for online saves
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
			thumbnail = ThumbnailData{ std::make_unique<Gui::StaticTexture>(GetHost(), false, thumbnailFuture.get()) };
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
			Ok();
		}
	}

	Preview::Size2 Preview::GetThumbnailSize() const
	{
		return RES / 2;
	}

	void Preview::GuiInfo()
	{
		auto infoPanel = ScopedVPanel("info");
		if (std::holds_alternative<InfoLoading>(info))
		{
			Spinner("loading", GetHost().GetCommonMetrics().smallSpinner);
		}
		else if (auto *infoData = std::get_if<InfoData>(&info))
		{
			auto &g = GetHost();
			auto &saveInfo = *infoData->info;
			{
				auto head = ScopedHPanel("head");
				SetMaxSizeSecondary(MaxSizeFitParent{});
				auto padding = g.GetCommonMetrics().padding;
				SetPadding(padding + 1);
				SetSpacing(Common{});
				SetParentFillRatio(0);
				if (authorAvatar)
				{
					auto avatar = ScopedComponent("avatar");
					SetHandleButtons(true);
					SetCursor(Cursor::hand);
					if (IsClicked(SDL_BUTTON_LEFT))
					{
						PushAboveThis(std::make_shared<Profile>(GetHost(), saveInfo.GetUserName()));
					}
					{
						SetSize(authorAvatarSize);
						SetSizeSecondary(authorAvatarSize);
						auto r = GetRect();
						if (auto *texture = authorAvatar->GetTexture())
						{
							g.DrawStaticTexture(r, *texture, 0xFFFFFFFF_argb);
						}
					}
				}
				auto rest = ScopedVPanel("rest");
				{
					auto row1 = ScopedHPanel("row1");
					SetSpacing(Common{});
					{
						auto left = ScopedComponent("left");
						SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
						auto title = saveInfo.GetName().ToUtf8();
						SetTitle(title);
						BeginText("title", title, "...", TextFlags::selectable | TextFlags::autoWidth);
						EndText();
					}
					auto right = ScopedVPanel("right");
					SetParentFillRatio(0);
					SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
					auto votebars = ScopedVPanel("votebars");
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
				}
				{
					auto row2 = ScopedHPanel("row2");
					SetSpacing(Common{});
					SetSize(12);
					{
						auto left = ScopedComponent("left");
						SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
						BeginText("author", saveInfo.GetUserName(), "...", TextFlags::autoWidth, GetAuthorColor(
							saveInfo.GetUserName(),
							saveInfo.authorElevation,
							saveInfo.authorIsBanned,
							&saveInfo
						));
						SetHandleButtons(true);
						SetCursor(Cursor::hand);
						if (IsClicked(SDL_BUTTON_LEFT))
						{
							PushAboveThis(std::make_shared<Profile>(GetHost(), saveInfo.GetUserName()));
						}
						EndText();
					}
					auto right = ScopedComponent("right");
					SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
					{
						ByteStringBuilder sb;
						sb << "\bg";
						if (!saveInfo.GetPublished())
						{
							sb << Gui::iconLockOutline << " ";
						}
						sb << "Views: \x0E" << saveInfo.Views;
						BeginText("views", sb.Build(), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
					}
					EndText();
				}
				{
					auto row3 = ScopedHPanel("row3");
					SetSpacing(Common{});
					SetSize(12);
					{
						auto left = ScopedComponent("left");
						SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
						BeginText("timestamp", ByteString::Build(saveInfo.updatedDate == saveInfo.createdDate ? "\bgCreated: \x0E" : "\bgUpdated: \x0E", FormatRelative(saveInfo.updatedDate, openedAt)), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
						EndText();
					}
					auto right = ScopedComponent("right");
					SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
					BeginText("saveid", ByteString::Build("\bgSave ID: \x0E", id), TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
					EndText();
				}
			}
			Separator("descriptionSeparator");
			auto descriptionPanel = ScopedScrollpanel("descriptionPanel");
			SetMaxSize(MaxSizeFitParent{});
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::top);
			BeginText("description", saveInfo.GetDescription().ToUtf8(), TextFlags::multiline | TextFlags::selectable | TextFlags::autoHeight);
			auto commonPadding = g.GetCommonMetrics().padding + 1;
			SetTextPadding(commonPadding, commonPadding, commonPadding, commonPadding);
			EndText();
		}
		else if (auto *infoError = std::get_if<InfoError>(&info))
		{
			BeginText("error", ByteString::Build(infoError->message), TextFlags::multiline);
			EndText();
		}
	}

	void Preview::GuiSave()
	{
		auto save = ScopedVPanel("save");
		{
			auto thumbnailComponent = ScopedComponent("thumbnail");
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
					g.DrawStaticTexture(Rect{ r.pos, thumbnailSize }, *thumbnailData->originalTexture, 0xFFFFFFFF_argb);
				}
				else
				{
					auto &thumbnailError = std::get<ThumbnailError>(thumbnail);
					BeginText("error", ByteString::Build(Gui::iconBrokenImage, "\n", thumbnailError.message), TextFlags::multiline);
					EndText();
				}
			}
		}
		Separator("separator");
		auto info = ScopedVPanel("info");
		SetSize(descriptionHeight);
		GuiInfo();
		Separator("manageSeparator");
		GuiManage();
	}

	void Preview::GuiCommentsItems()
	{
		auto itemsPanel = ScopedVPanel("items");
		int32_t overscroll = 0;
		if (std::holds_alternative<CommentsLoading>(comments))
		{
			Spinner("loading", GetHost().GetCommonMetrics().smallSpinner);
		}
		else if (auto *commentsData = std::get_if<CommentsData>(&comments))
		{
			auto scrollPanel = ScopedScrollpanel("scrollPanel");
			if (scrollCommentsToEnd)
			{
				ScrollpanelSetScroll({ 0, -Gui::maxSize });
			}
			overscroll = ScrollpanelGetOverscroll().Y;
			SetAlignment(Gui::Alignment::top);
			SetMaxSize(MaxSizeFitParent{});
			if (commentsData->items.empty())
			{
				Text("noComments", "No comments found", Gui::colorGray.WithAlpha(255)); // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				int32_t index = 0;
				for (auto &item : commentsData->items)
				{
					{
						auto iPanel = ScopedVPanel(index);
						SetParentFillRatio(0);
						SetAlignment(Gui::Alignment::top);
						SetLayered(true);
						auto padding = GetHost().GetCommonMetrics().padding + 1;
						{
							auto contentPanel = ScopedVPanel("content");
							SetParentFillRatio(0);
							constexpr Size avatarPadding = 5;
							SetPadding(padding);
							SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
							{
								auto head = ScopedHPanel("head");
								SetSpacing(Common{});
								SetSize(14);
								constexpr Size paddingTotal = 2;
								Size paddingAfter = item.makeSpaceForAvatar ? 0 : 1;
								{
									auto left = ScopedComponent("left");
									SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
									BeginText("author", item.authorName, "...", TextFlags::none, item.authorColor);
									SetTextPadding(item.avatar ? (commentAvatarSize + avatarPadding) : 0, 0, paddingTotal - paddingAfter, paddingAfter);
									SetHandleButtons(true);
									SetCursor(Cursor::hand);
									if (IsClicked(SDL_BUTTON_LEFT))
									{
										PushAboveThis(std::make_shared<Profile>(GetHost(), item.authorName));
									}
									EndText();
								}
								auto right = ScopedComponent("right");
								SetTextAlignment(Gui::Alignment::right, Gui::Alignment::center);
								SetParentFillRatio(0);
								BeginText("timestamp", FormatRelative(item.timestamp, openedAt), TextFlags::autoWidth, Gui::colorYellow.WithAlpha(255));
								SetTextPadding(0, 0, paddingTotal - paddingAfter, paddingAfter);
								EndText();
							}
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
						}
						if (item.avatar)
						{
							auto avatar = ScopedVPanel("avatar");
							SetParentFillRatio(0);
							SetPadding(padding);
							SetAlignment(Gui::Alignment::top);
							SetAlignmentSecondary(Gui::Alignment::left);
							auto textureComponent = ScopedComponent("texture");
							SetSize(commentAvatarSize);
							SetSizeSecondary(commentAvatarSize);
							SetHandleButtons(true);
							SetCursor(Cursor::hand);
							if (IsClicked(SDL_BUTTON_LEFT))
							{
								PushAboveThis(std::make_shared<Profile>(GetHost(), item.authorName));
							}
							auto &g = GetHost();
							auto r = GetRect();
							if (auto *texture = item.avatar->GetTexture())
							{
								g.DrawStaticTexture(r, *texture, 0xFFFFFFFF_argb);
							}
						}
					}
					Separator(index + 1);
					index += 2;
				}
			}
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

	void Preview::GuiCommentsPagination()
	{
		auto pagination = ScopedHPanel("pagination");
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
	}

	void Preview::GuiManage()
	{
		auto manage = ScopedHPanel("manage");
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetPadding(Common{});
		SetSpacing(Common{});
		BeginButton("open", ByteString::Build(Gui::iconOpen, " Open"), ButtonFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		SetEnabled(std::holds_alternative<InfoData>(info));
		if (EndButton() && GetDisposition() == DispositionFlags::none)
		{
			Ok();
		}
		auto *infoData = std::get_if<InfoData>(&info);
		{
			auto favorButtonFlags = ButtonFlags::autoWidth;
			if (infoData && infoData->info->Favourite)
			{
				favorButtonFlags = favorButtonFlags | ButtonFlags::stuck;
			}
			BeginButton("favor", ByteString::Build(Gui::iconFavorite, " Favor"), favorButtonFlags); // TODO-REDO_UI-TRANSLATE
		}
		SetEnabled(infoData);
		if (EndButton())
		{
			auto request = std::make_unique<http::FavouriteSaveRequest>(id, !infoData->info->Favourite);
			requestedFavorState = request->Favourite();
			auto requestView = MakeRequestView(GetHost(), "Saving favorite", std::move(request)); // TODO-REDO_UI-TRANSLATE
			favorFuture = requestView->GetFuture();
			requestView->Start();
			PushAboveThis(requestView);
		}
		BeginButton("report", ByteString::Build(Gui::iconWarning, " Report"), ButtonFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		if (EndButton())
		{
			// TODO
		}
		BeginButton("openinBrowser", ByteString::Build(Gui::iconOpen, " Open in browser"), ButtonFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
		if (EndButton())
		{
			format::Url url{ ByteString::Build(SERVER, "/Browse/View.html") };
			url.params["ID"] = ByteString::Build(id);
			if (version)
			{
				url.params["Date"] = ByteString::Build(version);
			}
			Platform::OpenURI(url.ToByteString());
		}
	}

	bool Preview::CanPostComment() const
	{
		return Client::Ref().GetAuthUser() && !commentInput.empty();
	}

	void Preview::GuiCommentsPost()
	{
		BeginHPanel("post");
		SetMinSize(GetHost().GetCommonMetrics().size);
		SetSpacing(Common{});
		SetAlignmentSecondary(Gui::Alignment::bottom);
		auto user = Client::Ref().GetAuthUser();
		if (user)
		{
			auto flags = TextboxFlags::none;
			if (commentInputAboveButton)
			{
				flags = TextboxFlags::multiline;
			}
			BeginTextbox("input", commentInput, "[comment]", flags); // TODO-REDO_UI-TRANSLATE
			SetMaxSizeSecondary(commentInputMaxHeight);
			if (GetOverflow())
			{
				commentInputAboveButton = true;
			}
			if (commentInput.empty())
			{
				commentInputAboveButton = false;
			}
			if (EndTextbox())
			{
				UpdateCommentAdvice();
			}
			if (commentInputAboveButton)
			{
				EndPanel();
				BeginHPanel("submitPanel");
				SetAlignment(Gui::Alignment::right);
			}
			BeginButton("submit", "Submit", ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
			SetEnabled(CanPostComment());
			SetSize(GetHost().GetCommonMetrics().smallButton);
			SetSizeSecondary(GetHost().GetCommonMetrics().size);
			if (EndButton())
			{
				auto request = std::make_unique<http::AddCommentRequest>(id, ByteString(commentInput).FromUtf8());
				auto requestView = MakeRequestView(GetHost(), "Submitting comment", std::move(request)); // TODO-REDO_UI-TRANSLATE
				postCommentFuture = requestView->GetFuture();
				requestView->Start();
				PushAboveThis(requestView);
			}
		}
		else
		{
			Text("nouser", "Log in to submit comments", Gui::colorGray.WithAlpha(255)); // TODO-REDO_UI-TRANSLATE
		}
		EndPanel();
	}

	void Preview::GuiComments()
	{
		auto comments = ScopedVPanel("comments");
		SetSize(commentsWidth);
		GuiCommentsItems();
		Separator("itemsSeparator");
		auto bottom = ScopedVPanel("bottom");
		SetParentFillRatio(0);
		SetPadding(Common{});
		SetSpacing(Common{});
		GuiCommentsPagination();
		if (commentAdvice)
		{
			BeginText("advice", (*commentAdvice->variants)[commentAdvice->index], TextFlags::multiline | TextFlags::autoHeight, Gui::colorLightRed.WithAlpha(255));
			EndText();
		}
		GuiCommentsPost();
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
		ProgressdialogState state = ProgressdialogStateProgress{};
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
			state = ProgressdialogStateDone{ true, message };
		}
		Progressdialog("dialog", "Opening save", state); // TODO-REDO_UI-TRANSLATE
	}

	void Preview::Gui()
	{
		if (quickOpen)
		{
			GuiQuickOpen();
			return;
		}
		auto preview = ScopedModal("preview", GetHost().GetSize().OriginRect());
		SetCancelWhenRootMouseDown(true);
		SetPrimaryAxis(Axis::horizontal);
		GuiSave();
		Separator("separator");
		GuiComments();
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
