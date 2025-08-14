#include "OnlineBrowser.hpp"
#include "BrowserCommon.hpp"
#include "Preview.hpp"
#include "Votebars.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"
#include "client/http/SearchSavesRequest.h"
#include "client/http/ThumbnailRequest.h"
#include "client/http/SearchTagsRequest.h"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveInfo.h"
#include "common/platform/Platform.h"
#include "graphics/VideoBuffer.h"
#include "Common/Log.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size titleHeight         = 11;
		constexpr Gui::View::Size voteBarsWidth       =  6;
		constexpr Gui::View::Size dropdownTextPadding =  6;

		std::pair<ByteString, std::optional<ByteString>> ParseMotd(ByteString raw)
		{
			auto openAt = raw.find("{a:");
			if (openAt != raw.npos)
			{
				auto textAt = raw.find("|", openAt + 1);
				if (textAt != raw.npos)
				{
					auto closeAt = raw.find("}", textAt + 1);
					if (closeAt)
					{
						return {
							raw.substr(0, openAt) + raw.substr(textAt + 1, closeAt - textAt - 1) + raw.substr(closeAt + 1),
							raw.substr(openAt + 3, textAt - openAt - 3)
						};
					}
				}
			}
			return { raw, std::nullopt };
		}
	}

	OnlineBrowser::OnlineBrowser(Gui::Host &newHost) : Browser(newHost)
	{
	}

	OnlineBrowser::~OnlineBrowser() = default;

	void OnlineBrowser::BeginSearch()
	{
		auto pageSize = GetPageSize();
		auto pageSizeLinear = pageSize.X * pageSize.Y;
		items = ItemsLoading{};
		searchSaves = std::make_unique<http::SearchSavesRequest>(query.page * pageSizeLinear, pageSizeLinear, query.str, period, sort, category);
		searchSaves->Start();
	}

	void OnlineBrowser::HandleTick()
	{
		if (searchTags && searchTags->CheckDone())
		{
			try
			{
				tags = TagsData{ searchTags->Finish() };
			}
			catch (const http::RequestError &ex)
			{
				tags = TagsError{ ex.what() };
			}
			searchTags.reset();
		}
		if (searchSaves && searchSaves->CheckDone())
		{
			try
			{
				auto [ itemCountOnAllPages, saves ] = searchSaves->Finish();
				ItemsData itemsData;
				itemsData.items = std::make_shared<decltype(itemsData.items)::element_type>();
				itemsData.itemCountOnAllPages = itemCountOnAllPages;
				itemsData.includesFeatured = searchSaves->GetIncludesFp();
				for (auto &save : saves)
				{
					auto item = std::make_shared<OnlineItem>();
					item->title = save->GetName().ToUtf8();
					item->info = std::move(save);
					itemsData.items->push_back(std::move(item));
				}
				items = std::move(itemsData);
			}
			catch (const http::RequestError &ex)
			{
				items = ItemsError{ ex.what() };
			}
			searchSaves.reset();
		}
		if (auto *itemsData = std::get_if<ItemsData>(&items))
		{
			for (auto &item : *itemsData->items)
			{
				auto &onlineItem = static_cast<OnlineItem &>(*item);
				if (onlineItem.thumbnailRequest && onlineItem.thumbnailRequest->CheckDone())
				{
					try
					{
						onlineItem.thumbnail = ThumbnailData{ std::make_unique<Gui::StaticTexture>(GetHost(), onlineItem.thumbnailRequest->Finish(), true) };
					}
					catch (const http::RequestError &ex)
					{
						onlineItem.thumbnail = ThumbnailError{ ex.what() };
					}
					onlineItem.thumbnailRequest.reset();
				}
			}
		}
		if (auto *itemsData = std::get_if<ItemsData>(&items); itemsData && pageCountStale)
		{
			showingFeatured = itemsData->includesFeatured && !query.page;
		}
		Browser::HandleTick();
	}

	void OnlineBrowser::GuiSearchLeft()
	{
		BeginDropdown("category", category); // TODO-REDO_UI: reset to none when the user goes away
		SetEnabled(Client::Ref().GetAuthUser().UserID != 0);
		SetSize(GetHost().GetCommonMetrics().bigButton);
		SetTextPadding(dropdownTextPadding, 0);
		DropdownItem(ByteString::Build(Gui::iconPowder    , " All"      )); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconFolderBody, " My own"   )); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconFavorite  , " Favorites")); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			SetQuery({ 0, query.str });
			FocusQuery();
		}
	}

	void OnlineBrowser::GuiSearchRight()
	{
		BeginDropdown("period", period);
		SetSize(GetHost().GetCommonMetrics().smallButton);
		SetTextPadding(dropdownTextPadding, 0);
		DropdownItem("All"  ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Today"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Week" ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Month"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Year" ); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			SetQuery({ 0, query.str });
			FocusQuery();
		}
		BeginDropdown("sort", sort);
		SetSize(GetHost().GetCommonMetrics().bigButton);
		SetTextPadding(dropdownTextPadding, 0);
		DropdownItem(ByteString::Build(Gui::iconUnusedScoreOutline, " By votes")); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconDateOutline       , " By date" )); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			SetQuery({ 0, query.str });
			FocusQuery();
		}
	}

	void OnlineBrowser::GuiPaginationLeft()
	{
		if (showingFeatured)
		{
			BeginButton("tags", ByteString::Build(Gui::iconTag, "\u0011\u0009Tags"), showTagsWithFeatured ? ButtonFlags::stuck : ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
			SetSize(GetHost().GetCommonMetrics().bigButton);
			if (EndButton())
			{
				showTagsWithFeatured = !showTagsWithFeatured;
				FocusQuery();
			}
		}
		else
		{
			Browser::GuiPaginationLeft();
		}
	}

	void OnlineBrowser::GuiQuickNavTags()
	{
		BeginVPanel("tags");
		if (std::holds_alternative<TagsLoading>(tags))
		{
			if (!searchTags)
			{
				auto pageSize = GetTagPageSize();
				auto pageSizeLinear = pageSize.X * pageSize.Y;
				searchTags = std::make_unique<http::SearchTagsRequest>(0, pageSizeLinear, "");
				searchTags->Start();
			}
			Text("loading", "Loading tags..."); // TODO-REDO_UI-TRANSLATE
		}
		else if (auto *tagsData = std::get_if<TagsData>(&tags))
		{
			auto &items = tagsData->tags;
			auto pageSize = GetTagPageSize();
			SetSpacing(2);
			SetPadding(0, 3, 20);
			BeginHPanel("populartags");
			{
				SetParentFillRatio(0);
				BeginComponent("before");
				EndComponent();
				BeginTextSeparator("separator", "Popular tags", 15, Gui::colorYellow.WithAlpha(255)); // TODO-REDO_UI-TRANSLATE
				SetParentFillRatio(4);
				EndTextSeparator();
				BeginComponent("after");
				EndComponent();
			}
			EndPanel();
			for (auto y = 0; y < pageSize.Y; ++y)
			{
				BeginHPanel(y);
				SetSpacing(4);
				SetAlignment(Gui::Alignment::left);
				for (auto x = 0; x < pageSize.X; ++x)
				{
					BeginComponent(x);
					SetCursor(Cursor::hand);
					auto index = y * pageSize.X + x;
					if (index < int32_t(items.size()))
					{
						auto maxVotes = items[0].second;
						auto alpha = maxVotes ? (127 + (128 * items[index].second / maxVotes)) : 192;
						auto color = Rgba8(alpha, alpha, alpha, 255);
						if (IsHovered())
						{
							color.Red   = color.Red   * 5 / 6;
							color.Green = color.Green * 5 / 6;
						}
						SetHandleButtons(true);
						if (IsClicked(SDL_BUTTON_LEFT))
						{
							SetQuery({ 0, items[index].first });
						}
						Text(index, items[index].first, color);
					}
					EndComponent();
				}
				EndPanel();
			}
		}
		else
		{
			auto &tagsError = std::get<TagsError>(tags);
			BeginText("error", tagsError.message, TextFlags::multiline);
			EndText();
		}
		EndPanel();
	}

	bool OnlineBrowser::GuiQuickNav()
	{
		if (!(showingFeatured && showTagsWithFeatured))
		{
			return false;
		}
		BeginHPanel("quicknav");
		BeginVPanel("cell"); // for sizing purposes
		SetSizeSecondary(GetSaveButtonSize().Y);
		{
			auto [ text, uri ] = ParseMotd(Client::Ref().GetMessageOfTheDay().ToUtf8());
			BeginText("motd", text, TextFlags::autoWidth);
			SetSize(15);
			SetTextPadding(6);
			SetParentFillRatioSecondary(0);
			if (uri)
			{
				SetCursor(Cursor::hand);
				SetHandleButtons(true);
				if (IsClicked(SDL_BUTTON_LEFT))
				{
					Platform::OpenURI(*uri);
				}
			}
			EndText();
		}
		GuiQuickNavTags();
		EndPanel();
		EndPanel();
		return true;
	}

	OnlineBrowser::Size2 OnlineBrowser::GetTagPageSize() const
	{
		return { 10, 4 };
	}

	void OnlineBrowser::PreviewItem(Item &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		std::optional<PlaneAdapter<std::vector<uint32_t>>> smallThumbnail;
		if (auto *thumbnailData = std::get_if<ThumbnailData>(&item.thumbnail))
		{
			smallThumbnail = thumbnailData->texture->GetData();
		}
		OpenItemInternal(onlineItem, false, smallThumbnail);
	}

	void OnlineBrowser::OpenItemInternal(OnlineItem &onlineItem, bool quickOpen, std::optional<PlaneAdapter<std::vector<uint32_t>>> smallThumbnail)
	{
		auto preview = std::make_shared<Preview>(GetHost(), onlineItem.info->id, onlineItem.info->Version, quickOpen, std::move(smallThumbnail));
		preview->SetGame(game);
		preview->SetGameStack(gameStack);
		preview->SetStacks(stacks);
		preview->SetThreadPool(threadPool);
		PushAboveThis(preview);
	}

	void OnlineBrowser::AcquireThumbnail(Item &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		if (!onlineItem.thumbnailRequest)
		{
			onlineItem.thumbnailRequest = std::make_unique<http::ThumbnailRequest>(onlineItem.info->GetID(), onlineItem.info->GetVersion(), GetThumbnailSize());
			onlineItem.thumbnailRequest->Start();
		}
	}

	void OnlineBrowser::OpenItem(Item &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		OpenItemInternal(onlineItem, true, std::nullopt);
	}

	void OnlineBrowser::GuiManage()
	{
		auto bigButton = GetHost().GetCommonMetrics().bigButton;
		Button("delete", "Delete", bigButton); // TODO-REDO_UI-TRANSLATE
		Button("unpublish", "Unpublish", bigButton); // TODO-REDO_UI-TRANSLATE
		Button("favourite", "Favourite", bigButton); // TODO-REDO_UI-TRANSLATE
	}

	bool OnlineBrowser::GuiSaveButtonSubtitle(int32_t index, Item &item, bool saveButtonHovered)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
		bool authorHovered = false;
		BeginText("author", onlineItem.info->GetUserName(), "...", TextFlags::none, colors.author);
		SetHandleButtons(true);
		SetSize(titleHeight);
		authorHovered = IsHovered();
		if (IsClicked(SDL_BUTTON_LEFT))
		{
			SetQueryMoreByThisUser(onlineItem);
		}
		EndText();
		return authorHovered;
	}

	void OnlineBrowser::GuiVoteBars(OnlineItem &onlineItem, bool saveButtonHovered)
	{
		auto &g = GetHost();
		auto colors = saveButtonHovered ? saveButtonColorsHovered : saveButtonColorsIdle;
		BeginComponent("votebars");
		SetSize(voteBarsWidth);
		auto r = GetRect();
		r.pos.X -= 1;
		r.size.X += 1;
		g.DrawRect(r, colors.edge);
		r = r.Inset(1);
		auto rUp = r;
		rUp.size.Y /= 2;
		auto rDown = r;
		rDown.pos.Y += rUp.size.Y;
		rDown.size.Y -= rUp.size.Y;
		g.FillRect(rUp, voteUpBackground);
		g.FillRect(rDown, voteDownBackground);
		rUp = rUp.Inset(1);
		rDown = rDown.Inset(1);
		rUp.size.Y += 1;
		rDown.pos.Y -= 1;
		rDown.size.Y += 1;
		auto [ sizeUp, sizeDown ] = ScaleVoteBars(rUp.size.Y, rDown.size.Y, onlineItem.info->GetVotesUp(), onlineItem.info->GetVotesDown());
		rDown.size.Y = sizeDown;
		rUp.pos.Y += rUp.size.Y - sizeUp;
		rUp.size.Y = sizeUp;
		g.FillRect(rUp, voteUpBar);
		g.FillRect(rDown, voteDownBar);
		EndComponent();
	}

	void OnlineBrowser::GuiScore(OnlineItem &onlineItem)
	{
		auto &g = GetHost();
		// at this point Host's clipRect is the entire parent component
		auto r = GetRect();
		auto score = onlineItem.info->GetVotesUp() - onlineItem.info->GetVotesDown();
		auto scoreString = ByteString::Build(score);
		std::string bodyString;
		std::string numberString;
		std::string outlineString;
		int32_t width = 0;
		auto appendBody = [&](int32_t ch) {
			width += g.CharWidth(ch);
			Utf8Append(bodyString, ch);
		};
		appendBody(Gui::iconBubbleInitialBody);
		Utf8Append(outlineString, Gui::iconBubbleInitialOutline);
		for (int32_t i = 0; i < int32_t(scoreString.size()); ++i)
		{
			auto ch = scoreString[i];
			appendBody(i == 0 ? Gui::iconBubbleMedialFirstBody : Gui::iconBubbleMedialRestBody);
			Utf8Append(outlineString, i == 0 ? Gui::iconBubbleMedialFirstOutline : Gui::iconBubbleMedialRestOutline);
			Utf8Append(numberString, ch == '-' ? ch : (ch - '0' + Gui::iconBubble0));
		}
		appendBody(Gui::iconBubbleFinalBody);
		Utf8Append(outlineString, Gui::iconBubbleFinalOutline);
		auto p = r.BottomRight() - Pos2{ width + voteBarsWidth + 2, 12 };
		g.DrawText(p               , bodyString, score >= 1 ? voteUpBackground : voteDownBackground);
		g.DrawText(p + Pos2{ 3, 0 }, numberString, 0xFFFFFFFF_argb);
		g.DrawText(p               , outlineString, 0xFFFFFFFF_argb);
	}

	void OnlineBrowser::GuiSaveButtonThumbnailRight(int32_t index, Item &item, bool saveButtonHovered)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		GuiVoteBars(onlineItem, saveButtonHovered);
		GuiScore(onlineItem);
	}

	void OnlineBrowser::SetQueryMoreByThisUser(OnlineItem &onlineItem)
	{
		SetQuery({ 0, ByteString::Build("user:", onlineItem.info->GetUserName()) });
	}

	void OnlineBrowser::GuiSaveButtonContextmenu(int32_t index, Item &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		if (ContextmenuButton("preview", "Preview")) // TODO-REDO_UI-TRANSLATE
		{
			PreviewItem(item);
		}
		if (ContextmenuButton("history", "View history")) // TODO-REDO_UI-TRANSLATE
		{
			SetQuery({ 0, ByteString::Build("history:", onlineItem.info->GetID()) });
		}
		if (ContextmenuButton("morebyauthor", "More by this user")) // TODO-REDO_UI-TRANSLATE
		{
			SetQueryMoreByThisUser(onlineItem);
		}
	}

	void OnlineBrowser::GuiHandleClicks(Item &item)
	{
		if (IsClicked(SDL_BUTTON_LEFT))
		{
			if (SDL_GetModState() & KMOD_CTRL)
			{
				OpenItem(item);
			}
			else
			{
				PreviewItem(item);
			}
		}
	}

	bool OnlineBrowser::HandleEvent(const SDL_Event &event)
	{
		auto handledByView = View::HandleEvent(event);
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (!event.key.repeat)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_F1:
					Log("nyi"); // TODO-REDO_UI
					break;
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
