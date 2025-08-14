#pragma once
#include "Gui/ComplexInput.hpp"
#include "Gui/View.hpp"
#include "Common/Plane.hpp"
#include "client/Comment.h"
#include "Avatar.hpp"
#include <future>
#include <ctime>

class VideoBuffer;
class GameSave;
class SaveInfo;

namespace http
{
	class GetSaveDataRequest;
	class GetSaveRequest;
	class GetCommentsRequest;
}

namespace Powder
{
	class ThreadPool;
	class Stacks;
}

namespace Powder::Gui
{
	class Host;
	class StaticTexture;
}

namespace Powder::Activity
{
	class Game;

	class Preview : public Gui::View
	{
		Game *game = nullptr;
		Gui::Stack *gameStack = nullptr;
		ThreadPool *threadPool = nullptr;
		Stacks *stacks = nullptr;
		int id;
		int version;
		bool quickOpen;

		bool requestedFavorState = false;
		std::future<void> favorFuture;

		struct InfoLoading
		{
		};
		struct InfoData
		{
			std::unique_ptr<SaveInfo> info;
		};
		struct InfoError
		{
			ByteString message;
		};
		struct InfoExtracted
		{
		};
		using Info = std::variant<
			InfoLoading,
			InfoData,
			InfoError,
			InfoExtracted
		>;
		Info info;
		bool saveNotFoundHack = false;
		std::unique_ptr<http::GetSaveRequest> getSaveRequest;

		std::unique_ptr<GameSave> save;
		std::unique_ptr<http::GetSaveDataRequest> getSaveDataRequest;

		struct ThumbnailLoading
		{
		};
		struct ThumbnailData
		{
			std::unique_ptr<Gui::StaticTexture> texture;
		};
		struct ThumbnailError
		{
			ByteString message;
		};
		using Thumbnail = std::variant<
			ThumbnailLoading,
			ThumbnailData,
			ThumbnailError
		>;
		Thumbnail thumbnail;
		std::future<std::unique_ptr<VideoBuffer>> thumbnailFuture;

		struct Comment : public ::Comment
		{
			Rgba8 authorColor;
			std::unique_ptr<Avatar> avatar;
			bool makeSpaceForAvatar = false;
		};
		void FormatCommentAuthorNames();

		struct CommentsLoading
		{
		};
		struct CommentsData
		{
			std::vector<Comment> items;
		};
		struct CommentsError
		{
			ByteString message;
		};
		using Comments = std::variant<
			CommentsLoading,
			CommentsData,
			CommentsError
		>;
		Comments comments;
		Gui::PaginationContext commentsPaginationContext;
		bool commentsPageCountStale = false;
		bool CanLoadNextCommentsPage() const;
		bool CanLoadPrevCommentsPage() const;
		void LoadNextCommentsPage();
		void LoadPrevCommentsPage();
		std::unique_ptr<http::GetCommentsRequest> getCommentsRequest;
		struct CommentsQuery
		{
			int32_t page = 0;
		};
		CommentsQuery commentsQuery;
		struct QueuedCommentsQuery
		{
			CommentsQuery commentsQuery;
			Gui::Ticks queuedAt;
		};
		std::optional<QueuedCommentsQuery> queuedCommentsQuery;
		bool scrollCommentsToEnd = false;
		bool queuedScrollCommentsToEnd = false;
		void QueueCommentsQuery(CommentsQuery newCommentsQuery);
		void BeginGetComments(CommentsQuery newCommentsQuery, bool newQueuedScrollCommentsToEnd);

		void HandleTick() final override;
		void Gui() final override;
		void HandleAfterFrame() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;

		void GuiSave();
		void GuiManage();

		bool HasQuickOpenFailed() const;
		void GuiQuickOpen();

		std::unique_ptr<Avatar> authorAvatar;
		void GuiInfo();

		std::string commentInput;
		void GuiCommentsPost();
		void GuiComments();
		void GuiCommentsItems();
		void GuiCommentsPagination();

		Size2 GetThumbnailSize() const;

		time_t openedAt;
		bool showAvatars;

	public:
		Preview(Gui::Host &newHost, int newId, int newVersion, bool newQuickOpen, std::optional<PlaneAdapter<std::vector<uint32_t>>> smallThumbnail);
		~Preview();

		void SetStacks(Stacks *newStacks)
		{
			stacks = newStacks;
		}

		void SetGameStack(Gui::Stack *newGameStack)
		{
			gameStack = newGameStack;
		}

		void SetGame(Game *newGame)
		{
			game = newGame;
		}

		void SetThreadPool(ThreadPool *newThreadPool)
		{
			threadPool = newThreadPool;
		}
	};
}
