#pragma once
#include "Common/NoCopy.hpp"
#include "TargetAnimation.hpp"
#include <deque>
#include <memory>
#include <optional>

typedef union SDL_Event SDL_Event;
typedef struct SDL_Texture SDL_Texture;

class VideoBuffer;

#define DebugGuiViewStack 0

namespace Powder::Gui
{
	class Host;
	class View;

	class ViewStack : public NoCopy
	{
		struct ViewHolder
		{
			ViewStack &viewStack;
			std::shared_ptr<View> view;
			TargetAnimation<int32_t> backgroundShadeAlpha{ TargetAnimation<int32_t>::LinearProfile{ 500 }, 100, 0 };
			bool haveBackground = false;
			SDL_Texture *background = nullptr;

			ViewHolder(ViewStack &newViewStack, std::shared_ptr<View> newView);
			~ViewHolder();

			void SetViewStack(ViewStack *newViewStack);
			void SetRendererUp(bool newRendererUp);
			void SetOnTop(bool newRendererUp);
		};
		std::deque<std::shared_ptr<ViewHolder>> viewHolders;

		bool rendererUp = false;

		void ExitUntilCount(int32_t count);
		void HandleFrameInternal(std::shared_ptr<ViewHolder> currentViewHolder, bool handleEvents);

	public:
		~ViewStack();

		void Push(std::shared_ptr<View> view);
		void Pop(View *view);
		bool TryPush(std::shared_ptr<View> view);
		bool TryPop(View *view);
		void ExitModals();
		void Exit();
		void HandleFrame();

		bool GetRendererUp() const
		{
			return rendererUp;
		}
		void SetRendererUp(bool newRendererUp);
	};
}
