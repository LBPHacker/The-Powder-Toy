#include "OnlineBrowser.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Icons.hpp"
#include "Gui/StaticTexture.hpp"
#include "client/http/SearchSavesRequest.h"
#include "client/http/ThumbnailRequest.h"
#include "client/http/SearchTagsRequest.h"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveInfo.h"
#include "common/platform/Platform.h"
#include "graphics/VideoBuffer.h"

namespace Powder::Activity
{
	namespace
	{
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
				itemsData.itemCountOnAllPages = itemCountOnAllPages;
				itemsData.includesFeatured = searchSaves->GetIncludesFp();
				for (auto &save : saves)
				{
					auto item = std::make_shared<OnlineItem>();
					item->thumbnailRequest = std::make_unique<http::ThumbnailRequest>(save->GetID(), save->GetVersion(), GetThumbnailSize());
					item->thumbnailRequest->Start();
					item->title = save->GetName().ToUtf8();
					item->onlineInfo = Item::OnlineInfo{
						save->GetUserName(),
						save->GetVotesUp(),
						save->GetVotesDown(),
						save->GetPublished(),
					};
					item->info = std::move(save);
					itemsData.items.push_back(std::move(item));
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
			for (auto &item : itemsData->items)
			{
				auto &onlineItem = static_cast<OnlineItem &>(*item);
				if (onlineItem.thumbnailRequest && onlineItem.thumbnailRequest->CheckDone())
				{
					try
					{
						auto thumbnail = std::make_shared<ThumbnailData>();
						thumbnail->texture = std::make_unique<Gui::StaticTexture>(GetHost(), *onlineItem.thumbnailRequest->Finish(), true);
						onlineItem.thumbnail = thumbnail;
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
		BeginDropdown("category", category);
		SetEnabled(Client::Ref().GetAuthUser().UserID != 0);
		SetSize(70);
		SetTextPadding(6, 0);
		// SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		DropdownItem(ByteString::Build(Gui::iconPowder    , " All"      )); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconFolderBody, " My own"   )); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconFavorite  , " Favorites")); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			BeginSearchInternal({ 0, query.str });
			FocusQuery();
		}
	}

	void OnlineBrowser::GuiSearchRight()
	{
		BeginDropdown("period", period);
		SetSize(50);
		SetTextPadding(6, 0);
		// SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		DropdownItem("All"  ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Today"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Week" ); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Month"); // TODO-REDO_UI-TRANSLATE
		DropdownItem("Year" ); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			BeginSearchInternal({ 0, query.str });
			FocusQuery();
		}
		BeginDropdown("sort", sort);
		SetSize(70);
		SetTextPadding(6, 0);
		// SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		DropdownItem(ByteString::Build(Gui::iconUnusedScoreOutline, " By votes")); // TODO-REDO_UI-TRANSLATE
		DropdownItem(ByteString::Build(Gui::iconDateOutline       , " By date" )); // TODO-REDO_UI-TRANSLATE
		if (EndDropdown())
		{
			BeginSearchInternal({ 0, query.str });
			FocusQuery();
		}
	}

	void OnlineBrowser::GuiPaginationLeft()
	{
		if (showingFeatured)
		{
			BeginButton("tags", ByteString::Build(Gui::iconTag, "\u0011\u0009Tags"), showTagsWithFeatured ? ButtonFlags::stuck : ButtonFlags::none); // TODO-REDO_UI-TRANSLATE
			SetSize(70);
			if (EndButton())
			{
				showTagsWithFeatured = !showTagsWithFeatured;
				FocusQuery();
			}
			return;
		}
		Browser::GuiPaginationLeft();
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
						// BeginButton(index, items[index].first, ButtonFlags::none, color);
						// EndButton();
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
			BeginText("error", tagsError.message, Gui::View::TextFlags::multiline);
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
			BeginText("motd", text, TextFlags::none);
			SetSize(15);
			SetHandleButtons(true);
			if (IsClicked() && uri)
			{
				Platform::OpenURI(*uri);
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
}
