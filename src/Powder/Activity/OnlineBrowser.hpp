#pragma once
#include "Browser.hpp"
#include "client/Search.h"
#include <vector>

class SaveInfo;

namespace http
{
	class SearchSavesRequest;
	class ThumbnailRequest;
	class SearchTagsRequest;
}

namespace Powder::Activity
{
	class OnlineBrowser : public Browser
	{
		http::Period period = http::allSaves;
		http::Sort sort = http::sortByVotes;
		http::Category category = http::categoryNone;
		bool showTagsWithFeatured = true;
		bool showingFeatured = false;

		struct TagsLoading
		{
		};
		struct TagsData
		{
			std::vector<std::pair<ByteString, int>> tags;
		};
		struct TagsError
		{
			ByteString message;
		};
		using Tags = std::variant<
			TagsLoading,
			TagsData,
			TagsError
		>;
		Tags tags;
		std::unique_ptr<http::SearchTagsRequest> searchTags;
		Size2 GetTagPageSize() const;

		std::unique_ptr<http::SearchSavesRequest> searchSaves;
		struct OnlineItem : public Item
		{
			std::unique_ptr<SaveInfo> info;
			std::unique_ptr<http::ThumbnailRequest> thumbnailRequest;
		};

		void BeginSearch() final override;
		void HandleTick() final override;
		void GuiPaginationLeft() final override;
		void GuiSearchRight() final override;
		void GuiSearchLeft() final override;
		bool GuiQuickNav() final override;

		void GuiQuickNavTags();

	public:
		OnlineBrowser(Gui::Host &newHost);
		~OnlineBrowser();
	};
}
