#include "ViewStack.hpp"
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
		constexpr int32_t maxFrameRepeatCount = 10; // TODO-REDO_UI: make this a ViewStack property?
	}

	ViewStack::ViewHolder::ViewHolder(ViewStack &newViewStack, std::shared_ptr<View> newView) : viewStack(newViewStack), view(newView)
	{
	}

	ViewStack::ViewHolder::~ViewHolder()
	{
		SetOnTop(false);
		SetRendererUp(false);
		SetViewStack(nullptr);
	}

	void ViewStack::ViewHolder::SetViewStack(ViewStack *newViewStack)
	{
		if (view->GetViewStack() != newViewStack)
		{
			view->SetViewStack(newViewStack);
		}
	}

	void ViewStack::ViewHolder::SetRendererUp(bool newRendererUp)
	{
		if (view->GetRendererUp() != newRendererUp)
		{
			view->SetRendererUpOuter(newRendererUp);
		}
	}

	void ViewStack::ViewHolder::SetOnTop(bool newOnTop)
	{
		if (view->GetOnTop() != newOnTop)
		{
			view->SetOnTopOuter(newOnTop);
		}
	}

	ViewStack::~ViewStack()
	{
		Exit();
	}

	void ViewStack::Push(std::shared_ptr<View> view)
	{
		Assert(TryPush(view));
	}

	void ViewStack::Pop(View *view)
	{
		Assert(TryPop(view));
	}

	bool ViewStack::TryPush(std::shared_ptr<View> view)
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

	bool ViewStack::TryPop(View *view)
	{
		if (!viewHolders.empty() && viewHolders.back()->view.get() == view)
		{
			viewHolders.pop_back();
			return true;
		}
		return false;
	}

	void ViewStack::ExitUntilCount(int32_t count)
	{
		while (int32_t(viewHolders.size()) > count)
		{
			Pop(viewHolders.back()->view.get());
		}
	}

	void ViewStack::ExitModals()
	{
		ExitUntilCount(1);
	}

	void ViewStack::Exit()
	{
		ExitUntilCount(0);
	}

	void ViewStack::SetRendererUp(bool newRendererUp)
	{
		if (rendererUp != newRendererUp)
		{
			rendererUp = newRendererUp;
			if (!rendererUp)
			{
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

	void ViewStack::SetHost(Host *newHost)
	{
		host = newHost;
	}

	void ViewStack::HandleFrame()
	{
		auto *sdlRenderer = host->GetRenderer();
		for (int32_t i = 0; i < int32_t(viewHolders.size()); ++i)
		{
			auto &currViewHolder = viewHolders[i];
			if (!currViewHolder->haveBackground)
			{
				currViewHolder->haveBackground = true;
				if (i)
				{
					currViewHolder->background = SdlAssertPtr(SdlAssertPtr(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, host->GetSize().X, host->GetSize().Y)));
					SdlAssertZero(SDL_SetRenderTarget(sdlRenderer, currViewHolder->background));
					auto &prevViewHolder = viewHolders[i - 1];
					HandleFrameInternal(prevViewHolder, false);
					SdlAssertZero(SDL_SetRenderTarget(sdlRenderer, nullptr));
				}
			}
		}
		if (viewHolders.empty())
		{
			return;
		}
		auto currentViewHolder = viewHolders.back();
		HandleFrameInternal(currentViewHolder, true);
		if (!(!viewHolders.empty() && viewHolders.back() == currentViewHolder))
		{
			currentViewHolder->SetOnTop(false);
		}
	}

	void ViewStack::HandleFrameInternal(std::shared_ptr<ViewHolder> currentViewHolder, bool handleEvents)
	{
		currentViewHolder->SetViewStack(this);
		currentViewHolder->SetRendererUp(true);
		if (handleEvents)
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
					host->Stop();
					break;
				}
			}
			currentView->HandleTick();
		}
		auto *sdlRenderer = host->GetRenderer();
		currentView->HandleBeforeGui();
		int32_t repeatCount = 0;
		while (repeatCount < maxFrameRepeatCount)
		{
			repeatCount += 1;
			host->SetClipRect(host->GetSize().OriginRect());
			SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 255));
			SdlAssertZero(SDL_RenderClear(sdlRenderer));
			if (currentViewHolder->background)
			{
				SdlAssertZero(SDL_RenderCopy(sdlRenderer, currentViewHolder->background, nullptr, nullptr));
				host->FillRect(host->GetSize().OriginRect(), 0x000000_rgb .WithAlpha(currentViewHolder->backgroundShadeAlpha.GetValue()));
			}
			if (!currentView->HandleGui())
			{
				break;
			}
		}
#if DebugGuiViewStack
		if (repeatCount > 1)
		{
			Log("ViewStack::HandleFrameInternal repeatCount ", repeatCount);
		}
#endif
		currentView->HandleAfterGui();
		currentView->HandleAfterFrame();
		// currentViewHolder goes out of scope here and potentially calls ~ViewHolder
	}
}
