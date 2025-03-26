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
#include "common/platform/Platform.h"
#include "graphics/VideoBuffer.h"
#include "Common/Log.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dropdownTextPadding = 6;

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

	OnlineBrowser::OnlineBrowser(Gui::Host &newHost, ThreadPool &newThreadPool) : Browser(newHost, newThreadPool)
	{
		// only exists so stuff doesn't have to be complete in OnlineBrowser.hpp
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
					item->onlineInfo = BrowserItem::OnlineInfo{
						save->GetUserName(),
						save->GetVotesUp(),
						save->GetVotesDown(),
						save->GetPublished(),
					};
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
						auto original = onlineItem.thumbnailRequest->Finish();
						auto small = std::make_unique<VideoBuffer>(*original);
						small->Resize(GetSmallThumbnailSize(), true);
						onlineItem.thumbnail = ThumbnailData{
							std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(original)),
							std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(small)),
						};
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
		auto user = Client::Ref().GetAuthUser();
		if (!user)
		{
			category = http::categoryNone;
		}
		BeginDropdown("category", category);
		SetEnabled(bool(user));
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
		auto tagsPanel = ScopedVPanel("tags");
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
			{
				auto populartags = ScopedHPanel("popularTags");
				SetParentFillRatio(0);
				ScopedComponent("before");
				BeginTextSeparator("separator", "Popular tags", 15, Gui::colorYellow.WithAlpha(255)); // TODO-REDO_UI-TRANSLATE
				SetParentFillRatio(4);
				EndTextSeparator();
				ScopedComponent("after");
			}
			for (auto y = 0; y < pageSize.Y; ++y)
			{
				auto yPanel = ScopedHPanel(y);
				SetSpacing(4);
				SetAlignment(Gui::Alignment::left);
				for (auto x = 0; x < pageSize.X; ++x)
				{
					auto xComponent = ScopedComponent(x);
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
						SetCursor(Cursor::hand);
						SetHandleButtons(true);
						if (IsClicked(SDL_BUTTON_LEFT))
						{
							SetQuery({ 0, items[index].first });
							FocusQuery();
						}
						Text(index, items[index].first, color);
					}
				}
			}
		}
		else
		{
			auto &tagsError = std::get<TagsError>(tags);
			BeginText("error", tagsError.message, TextFlags::multiline);
			EndText();
		}
	}

	bool OnlineBrowser::GuiQuickNav()
	{
		if (!(showingFeatured && showTagsWithFeatured))
		{
			return false;
		}
		auto quickNav = ScopedHPanel("quickNav");
		auto cell = ScopedVPanel("cell"); // for sizing purposes
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
		return true;
	}

	OnlineBrowser::Size2 OnlineBrowser::GetTagPageSize() const
	{
		return { 10, 4 };
	}

	void OnlineBrowser::PreviewItem(BrowserItem &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		std::optional<PlaneAdapter<std::vector<pixel>>> smallThumbnail;
		if (auto *thumbnailData = std::get_if<ThumbnailData>(&item.thumbnail))
		{
			smallThumbnail = thumbnailData->originalTexture->GetData();
		}
		OpenItemInternal(onlineItem, false, smallThumbnail);
	}

	void OnlineBrowser::OpenItemInternal(OnlineItem &onlineItem, bool quickOpen, std::optional<PlaneAdapter<std::vector<pixel>>> smallThumbnail)
	{
		auto preview = std::make_shared<Preview>(*game, onlineItem.info->id, onlineItem.info->Version, quickOpen, std::move(smallThumbnail));
		preview->SetGameStack(gameStack);
		preview->SetStacks(stacks);
		PushAboveThis(preview);
	}

	void OnlineBrowser::AcquireItemThumbnail(BrowserItem &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		if (!onlineItem.thumbnailRequest)
		{
			onlineItem.thumbnailRequest = std::make_unique<http::ThumbnailRequest>(onlineItem.info->GetID(), onlineItem.info->GetVersion(), GetOriginalThumbnailSize());
			onlineItem.thumbnailRequest->Start();
		}
	}

	void OnlineBrowser::OpenItem(BrowserItem &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		OpenItemInternal(onlineItem, true, std::nullopt);
	}

	void OnlineBrowser::GuiManage()
	{
		auto &itemsData = std::get<ItemsData>(items);
		auto bigButton = GetHost().GetCommonMetrics().bigButton;
		if (Button("delete", "Delete", bigButton)) // TODO-REDO_UI-TRANSLATE
		{
			for (auto &item : *itemsData.items)
			{
				if (item->selected)
				{
					Log("nyi"); // TODO-REDO_UI-EASY
				}
			}
		}
		if (Button("unpublish", "Unpublish", bigButton)) // TODO-REDO_UI-TRANSLATE
		{
			for (auto &item : *itemsData.items)
			{
				if (item->selected)
				{
					Log("nyi"); // TODO-REDO_UI-EASY
				}
			}
		}
		if (Button("favourite", "Favourite", bigButton)) // TODO-REDO_UI-TRANSLATE
		{
			for (auto &item : *itemsData.items)
			{
				if (item->selected)
				{
					Log("nyi"); // TODO-REDO_UI-EASY
				}
			}
		}
	}

	void OnlineBrowser::SetQueryMoreByThisUser(OnlineItem &onlineItem)
	{
		SetQuery({ 0, ByteString::Build("user:", onlineItem.info->GetUserName()) });
	}

	void OnlineBrowser::GuiSaveButtonContextmenu(BrowserItem &item)
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
		if (ContextmenuButton("moreByAuthor", "More by this user")) // TODO-REDO_UI-TRANSLATE
		{
			SetQueryMoreByThisUser(onlineItem);
		}
	}

	void OnlineBrowser::GuiSaveButtonHandleClicks(BrowserItem &item)
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

	void OnlineBrowser::GuiTitle()
	{
		SetTitle("Open online simulation"); // TODO-REDO_UI-TRANSLATE
	}

	void OnlineBrowser::GuiNoResults()
	{
		Text("empty", "No online saves found", Gui::colorGray.WithAlpha(255)); // TODO-REDO_UI-TRANSLATE
	}

	void OnlineBrowser::ClickItemSubtitle(BrowserItem &item)
	{
		auto &onlineItem = static_cast<OnlineItem &>(item);
		SetQueryMoreByThisUser(onlineItem);
	}
}
