#include "Stack.hpp"
#include "Config.h"
#include "Host.hpp"
#include "SdlAssert.hpp"
#include "Common/Assert.hpp"
#include "Common/Log.hpp"
#include "graphics/VideoBuffer.h"
#include "View.hpp"
#include <algorithm>

namespace Powder::Gui
{
	namespace
	{
		constexpr int32_t maxFrameRepeatCount = 10; // TODO-REDO_UI: make this a Stack property?
	}

	Stack::ViewHolder::ViewHolder(Stack &newStack, std::shared_ptr<View> newView) :
		stack(newStack),
		view(newView),
		backgroundShadeAlpha{ view->GetHost(), TargetAnimation<int32_t>::LinearProfile{ 500 }, 100, 0 }
	{
	}

	Stack::ViewHolder::~ViewHolder()
	{
		SetOnTop(false);
		SetRendererUp(false);
		SetStack(nullptr);
	}

	void Stack::ViewHolder::SetStack(Stack *newStack)
	{
		if (view->GetStack() != newStack)
		{
			view->SetStack(newStack);
		}
	}

	void Stack::ViewHolder::SetRendererUp(bool newRendererUp)
	{
		if (view->GetRendererUp() != newRendererUp)
		{
			view->SetRendererUpOuter(newRendererUp);
		}
	}

	void Stack::ViewHolder::SetOnTop(bool newOnTop)
	{
		if (view->GetOnTop() != newOnTop)
		{
			view->SetOnTopOuter(newOnTop);
		}
	}

	Stack::~Stack()
	{
		Exit();
	}

	void Stack::Push(std::shared_ptr<View> view)
	{
		Assert(TryPush(view));
	}

	void Stack::Pop(View *view)
	{
		Assert(TryPop(view));
	}

	bool Stack::TryPush(std::shared_ptr<View> view)
	{
		Assert(view.get());
		auto it = std::find_if(viewHolders.begin(), viewHolders.end(), [view](auto &ptr) {
			return ptr->view == view;
		});
		if (it == viewHolders.end())
		{
			viewHolders.push_back(std::make_shared<ViewHolder>(*this, view));
			return true;
		}
		return false;
	}

	bool Stack::TryPop(View *view)
	{
		if (!viewHolders.empty() && viewHolders.back()->view.get() == view)
		{
			viewHolders.pop_back();
			return true;
		}
		return false;
	}

	void Stack::ExitUntilCount(int32_t count)
	{
		while (int32_t(viewHolders.size()) > count)
		{
			Pop(viewHolders.back()->view.get());
		}
	}

	void Stack::ExitModals()
	{
		ExitUntilCount(1);
	}

	void Stack::Exit()
	{
		ExitUntilCount(0);
	}

	void Stack::SetRendererUp(bool newRendererUp)
	{
		if (rendererUp != newRendererUp)
		{
			rendererUp = newRendererUp;
			if (!rendererUp)
			{
				// TODO-REDO_UI: notify views of the renderer going away from Host so they don't throw away their textures for no reason
				for (auto &viewHolder : viewHolders)
				{
					viewHolder->SetRendererUp(false);
					SDL_DestroyTexture(viewHolder->background);
					viewHolder->background = nullptr;
					viewHolder->haveBackground = false;
				}
			}
		}
	}

	void Stack::SetOnTop(bool newOnTop)
	{
		if (onTop != newOnTop)
		{
			onTop = newOnTop;
			if (onTop)
			{
				shouldUpdateTitle = true;
			}
			else
			{
				for (auto &viewHolder : viewHolders)
				{
					viewHolder->SetOnTop(false);
				}
			}
		}
	}

	void Stack::HandleFrame(SDL_Texture *renderTarget)
	{
		auto &g = GetHost();
		auto *sdlRenderer = g.GetRenderer();
		for (int32_t i = 0; i < int32_t(viewHolders.size()); ++i)
		{
			auto &currViewHolder = viewHolders[i];
			if (!currViewHolder->haveBackground)
			{
				currViewHolder->haveBackground = true;
				if (i)
				{
					currViewHolder->background = SdlAssertPtr(SdlAssertPtr(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, g.GetSize().X, g.GetSize().Y)));
					auto &prevViewHolder = viewHolders[i - 1];
					HandleFrameInternal(prevViewHolder, false, currViewHolder->background);
				}
			}
		}
		if (viewHolders.empty())
		{
			return;
		}
		auto currentViewHolder = viewHolders.back();
		HandleFrameInternal(currentViewHolder, true, renderTarget);
		if (shouldUpdateTitle)
		{
			std::optional<ByteString> topTitle;
			for (auto &viewHolder : viewHolders)
			{
				auto title = viewHolder->view->GetTitle();
				if (title)
				{
					topTitle = title;
				}
			}
			ByteStringBuilder sb;
			if (topTitle)
			{
				sb << *topTitle << " - ";
			}
			sb << APPNAME;
			SDL_SetWindowTitle(g.GetWindow(), sb.Build().c_str());
			shouldUpdateTitle = false;
		}
		if (!(!viewHolders.empty() && viewHolders.back() == currentViewHolder))
		{
			currentViewHolder->SetOnTop(false);
		}
	}

	void Stack::HandleFrameInternal(std::shared_ptr<ViewHolder> currentViewHolder, bool handleEvents, SDL_Texture *renderTarget)
	{
		auto &g = GetHost();
		currentViewHolder->SetStack(this);
		currentViewHolder->SetRendererUp(true);
		if (handleEvents && onTop)
		{
			currentViewHolder->SetOnTop(true);
		}
		auto *currentView = currentViewHolder->view.get();
		currentView->HandleBeforeFrame();
		if (handleEvents)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				currentView->HandleEvent(event);
				switch (event.type)
				{
				case SDL_QUIT:
					if (g.GetFastQuit())
					{
						g.Stop();
					}
					break;

				case SDL_DROPFILE:
					SDL_free(event.drop.file);
					break;
				}
			}
			currentView->HandleTick();
		}
		auto *sdlRenderer = g.GetRenderer();
		auto prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
		SdlAssertZero(SDL_SetRenderTarget(sdlRenderer, renderTarget));
		currentView->HandleBeforeGui();
		int32_t repeatCount = 0;
		while (repeatCount < maxFrameRepeatCount)
		{
			repeatCount += 1;
			g.SetClipRect(g.GetSize().OriginRect());
			SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255));
			SdlAssertZero(SDL_RenderClear(sdlRenderer));
			if (currentViewHolder->background)
			{
				SdlAssertZero(SDL_RenderCopy(sdlRenderer, currentViewHolder->background, nullptr, nullptr));
				g.FillRect(g.GetSize().OriginRect(), 0x000000_rgb .WithAlpha(currentViewHolder->backgroundShadeAlpha.GetValue()));
			}
			if (!currentView->HandleGui())
			{
				break;
			}
		}
#if DebugGuiStack
		if (repeatCount > 1)
		{
			Log("Stack::HandleFrameInternal repeatCount ", repeatCount);
		}
#endif
		currentView->HandleAfterGui();
		SdlAssertZero(SDL_SetRenderTarget(sdlRenderer, prevRenderTarget));
		currentView->HandleAfterFrame();
		// currentViewHolder goes out of scope here and potentially calls ~ViewHolder
	}

	void Stack::RequestUpdateTitle()
	{
		shouldUpdateTitle = true;
	}
}
