#pragma once
#include "Common/NoCopy.hpp"
#include "TargetAnimation.hpp"
#include <deque>
#include <memory>
#include <optional>

typedef union SDL_Event SDL_Event;
typedef struct SDL_Texture SDL_Texture;

class VideoBuffer;

#define DebugGuiStack 0

namespace Powder::Gui
{
	class Host;
	class View;

	class Stack : public NoCopy
	{
		struct ViewHolder
		{
			Stack &stack;
			std::shared_ptr<View> view;
			TargetAnimation<int32_t> backgroundShadeAlpha;
			bool haveBackground = false;
			SDL_Texture *background = nullptr;

			ViewHolder(Stack &newStack, std::shared_ptr<View> newView);
			~ViewHolder();

			void SetStack(Stack *newStack);
			void SetRendererUp(bool newRendererUp);
			void SetOnTop(bool newRendererUp);
		};
		std::deque<std::shared_ptr<ViewHolder>> viewHolders;

		Host &host;
		bool rendererUp = false;
		bool onTop = false;

		bool shouldUpdateTitle = false;

		void ExitUntilCount(int32_t count);
		void HandleFrameInternal(std::shared_ptr<ViewHolder> currentViewHolder, bool handleEvents, SDL_Texture *renderTarget);

		Host &GetHost() const
		{
			return host;
		}

	public:
		Stack(Host &newHost) : host(newHost)
		{
		}
		~Stack();

		void Push(std::shared_ptr<View> view);
		void Pop(View *view);
		bool TryPush(std::shared_ptr<View> view);
		bool TryPop(View *view);
		void ExitModals();
		void Exit();
		void HandleFrame(SDL_Texture *renderTarget);
		void RequestUpdateTitle();

		bool GetRendererUp() const
		{
			return rendererUp;
		}
		void SetRendererUp(bool newRendererUp);

		bool GetOnTop() const
		{
			return onTop;
		}
		void SetOnTop(bool newOnTop);

		bool IsEmpty() const
		{
			return viewHolders.empty();
		}
	};
}
