#include "View.hpp"
#include "View.hpp"
#include "Common/Div.hpp"
#include "Host.hpp"
#include "ViewStack.hpp"
#include "SdlAssert.hpp"
#include "graphics/VideoBuffer.h"
#include <algorithm>

#include "Common/Log.hpp"

#if DebugGuiView
# include "Common/Log.hpp"
#endif
#define DebugGuiViewStats 0
#define DebugGuiViewDump 0
#define DebugGuiViewChildren 0
#define DebugGuiViewRects 1

namespace Powder::Gui
{
	namespace
	{
		constexpr View::Pos maxPos             = 100'000;
		constexpr View::Size maxSize           = 100'000;
		constexpr int32_t maxInternedItemSize  = 1000;
		constexpr int32_t maxInternedTotalSize = 100'000;
		constexpr int32_t selectionOverhang    = 1;

		View::Pos &operator %(View::Pos2 &point, View::AxisBase index)
		{
			return index ? point.Y : point.X;
		}

		template<class Item>
		Item &operator %(View::ExtendedSize2<Item> &point, View::AxisBase index)
		{
			return index ? point.Y : point.X;
		}

		void ClampSize(View::Size &size)
		{
			size = std::clamp(size, 0, maxSize);
		}

		void ClampPos(View::Pos &pos)
		{
			pos = std::clamp(pos, -maxPos, maxPos);
		}

		void ClampSize(View::Size2 &size)
		{
			ClampSize(size.X);
			ClampSize(size.Y);
		}

		void ClampPos(View::Size2 &size)
		{
			ClampPos(size.X);
			ClampPos(size.Y);
		}

		template<class SizeHolder>
		void ClampSize(SizeHolder &sizeHolder)
		{
			if (auto *size = std::get_if<View::Size>(&sizeHolder))
			{
				ClampSize(*size);
			}
		}

		template<class>
		struct DependentFalse : std::false_type
		{
		};
	}

	std::string_view View::ComponentKey::GetBytes() const
	{
		return std::visit([](auto &key) {
			std::string_view view;
			using Key = std::remove_cvref_t<decltype(key)>;
			if      constexpr (std::is_same_v<Key, NumericKey      >) view = std::string_view(reinterpret_cast<const char *>(&key), sizeof(key));
			else if constexpr (std::is_same_v<Key, std::string_view>) view = key;
			else static_assert(DependentFalse<Key>::value);
			return view;
		}, key);
	}

	View::ComponentKeyStoreSpan View::ComponentStore::InternKey(ComponentKey key)
	{
		auto bytes = key.GetBytes();
		auto oldKeyStoreSize = ComponentKeyStoreSize(keyData.size());
		auto bytesSize = std::min(std::min(ComponentKeyStoreSize(bytes.size()), maxInternedItemSize), maxInternedTotalSize - oldKeyStoreSize);
		keyData.resize(oldKeyStoreSize + bytesSize);
		std::copy_n(bytes.data(), bytesSize, keyData.data() + oldKeyStoreSize);
		return { oldKeyStoreSize, bytesSize };
	}

	bool View::ComponentStore::CompareKey(ComponentKeyStoreSpan interned, ComponentKey key) const
	{
		return DereferenceSpan(interned) == key.GetBytes();
	}

	View::ComponentIndex View::FindOrAllocComponent(ComponentKey key)
	{
		if (prevSiblingIndex)
		{
			auto &prevSibling = componentStore[*prevSiblingIndex];
			if (prevSibling.nextSiblingIndex)
			{
				auto &nextSibling = componentStore[*prevSibling.nextSiblingIndex];
				if (componentStore.CompareKey(nextSibling.key, key))
				{
					return *prevSibling.nextSiblingIndex;
				}
			}
		}
		auto *currentComponent = GetCurrentComponent();
		if (currentComponent)
		{
			auto childIndex = currentComponent->firstChildIndex;
			while (childIndex)
			{
				auto &child = componentStore[*childIndex];
				if (componentStore.CompareKey(child.key, key))
				{
					return *childIndex;
				}
				childIndex = child.nextSiblingIndex;
				shouldDoLayout = true;
#if DebugGuiView
				debugFindIterationCount += 1;
#endif
			}
		}
		if (rootIndex)
		{
			auto &firstChild = componentStore[*rootIndex];
			if (componentStore.CompareKey(firstChild.key, key))
			{
				return *rootIndex;
			}
		}
		shouldDoLayout = true;
		auto componentIndex = componentStore.Alloc();
		componentStore[componentIndex].key = componentStore.InternKey(key);
		return componentIndex;
	}

	View::Component &View::GetOrAllocComponent(ComponentKey key)
	{
		auto componentIndex = FindOrAllocComponent(key);
		auto &component = componentStore[componentIndex];
		if (component.state == Component::State::unused)
		{
			component.state = Component::State::used;
			component.spacing = 0;
			component.layout = {};
			component.content = {};
			component.handleButtons = false;
			component.handleWheel = false;
			component.handleInput = false;
			component.forwardInputFocusIndex.reset();
#if DebugGuiView
			component.debugMe = false;
#endif
			if (prevSiblingIndex)
			{
				componentStore[*prevSiblingIndex].nextSiblingIndex = componentIndex;
				component.layout.spacingFromParent = componentStore[componentStack.back().index].spacing + extraSpacing;
			}
			else
			{
				(componentStack.empty() ? rootIndex : componentStore[componentStack.back().index].firstChildIndex) = componentIndex;
			}
			extraSpacing = 0;
		}
		prevSiblingIndex.reset();
		auto clipRect = component.rect;
		if (!componentStack.empty())
		{
			clipRect &= componentStack.back().clipRect;
		}
		componentStack.push_back({ componentIndex, clipRect });
		return component;
	}

	bool View::HandleEvent(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_MOUSEMOTION:
			SetMousePos(Pos2{ event.motion.x, event.motion.y });
			break;

		case SDL_MOUSEBUTTONUP:
			SetMousePos(Pos2{ event.button.x, event.button.y });
			if (handleButtonsIndex && componentMouseDownEvent &&
			    componentMouseDownEvent->component == *handleButtonsIndex &&
			    componentMouseDownEvent->button == event.button.button)
			{
				componentMouseClickEvent = componentMouseDownEvent;
			}
			componentMouseDownEvent.reset();
			break;

		case SDL_MOUSEBUTTONDOWN:
			SetMousePos(Pos2{ event.button.x, event.button.y });
			if (inputFocus && inputFocus->component != handleInputIndex)
			{
				SetInputFocus(std::nullopt);
			}
			if (handleInputIndex)
			{
				SetInputFocus(*handleInputIndex);
			}
			if (handleButtonsIndex)
			{
				componentMouseDownEvent = ComponentMouseButtonEvent{ *handleButtonsIndex, event.button.button };
				return true;
			}
			if (exitWhenRootMouseDown && (!underMouseIndex ||
			                              (rootIndex && underMouseIndex && *underMouseIndex == *rootIndex)))
			{
				Exit();
			}
			break;

		case SDL_MOUSEWHEEL:
			SetMousePos(Pos2{ event.wheel.mouseX, event.wheel.mouseY });
			if (handleWheelIndex)
			{
				int32_t sign = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
				int32_t dx = event.wheel.x * sign;
				int32_t dy = event.wheel.y * sign;
				if (!componentMouseScrollEvent)
				{
					componentMouseScrollEvent = ComponentMouseWheelEvent{ *handleWheelIndex, { 0, 0 } };
				}
				componentMouseScrollEvent->distance.X += dx;
				componentMouseScrollEvent->distance.Y += dy;
				return true;
			}
			break;

		case SDL_TEXTINPUT:
			if (SDL_GetModState() & KMOD_GUI)
			{
				break;
			}
			if (inputFocus)
			{
				inputFocus->events.push_back(InputFocus::TextInputEvent{ event.text.text });
				return true;
			}
			break;

		case SDL_KEYDOWN:
			if (SDL_GetModState() & KMOD_GUI)
			{
				break;
			}
			if (inputFocus)
			{
				inputFocus->events.push_back(InputFocus::KeyDownEvent{
					event.key.keysym.sym,
					bool(event.key.repeat),
					bool(event.key.keysym.mod & KMOD_CTRL),
					bool(event.key.keysym.mod & KMOD_SHIFT),
					bool(event.key.keysym.mod & KMOD_ALT),
				});
				return true;
			}
			break;

		case SDL_WINDOWEVENT:
			switch (event.window.event)
			{
			case SDL_WINDOWEVENT_ENTER:
				HandleMouseEnter();
				break;

			case SDL_WINDOWEVENT_LEAVE:
				HandleMouseLeave();
				break;

			case SDL_WINDOWEVENT_SHOWN:
				HandleWindowShown();
				break;

			case SDL_WINDOWEVENT_HIDDEN:
				HandleWindowHidden();
				break;
			}
			break;
		}
		return false;
	}

	void View::SetPrimaryAxis(Axis newPrimaryAxis)
	{
		GetCurrentComponent()->layout.primaryAxis = newPrimaryAxis;
	}

	void View::SetLayered(bool newLayered)
	{
		GetCurrentComponent()->layout.layered = newLayered;
	}

	void View::SetMinSize(MinSize newMinSize)
	{
		ClampSize(newMinSize);
		GetCurrentComponent()->layout.minSize.X = newMinSize;
	}

	void View::SetMinSizeSecondary(MinSize newMinSizeSecondary)
	{
		ClampSize(newMinSizeSecondary);
		GetCurrentComponent()->layout.minSize.Y = newMinSizeSecondary;
	}

	void View::SetMaxSize(MaxSize newMaxSize)
	{
		ClampSize(newMaxSize);
		GetCurrentComponent()->layout.maxSize.X = newMaxSize;
	}

	void View::SetMaxSizeSecondary(MaxSize newMaxSizeSecondary)
	{
		ClampSize(newMaxSizeSecondary);
		GetCurrentComponent()->layout.maxSize.Y = newMaxSizeSecondary;
	}

	void View::SetSize(SetSizeSize newSize)
	{
		ClampSize(newSize);
		if (auto *size = std::get_if<Size>(&newSize))
		{
			SetMinSize(*size);
			SetMaxSize(*size);
		}
		else
		{
			SetMinSize(MinSizeFitChildren{});
			SetMaxSize(MaxSizeInfinite{});
		}
	}

	void View::SetSizeSecondary(SetSizeSize newSizeSecondary)
	{
		ClampSize(newSizeSecondary);
		if (auto *size = std::get_if<Size>(&newSizeSecondary))
		{
			SetMinSizeSecondary(*size);
			SetMaxSizeSecondary(*size);
		}
		else
		{
			SetMinSizeSecondary(MinSizeFitChildren{});
			SetMaxSizeSecondary(MaxSizeInfinite{});
		}
	}

	void View::SetScroll(Size newScroll)
	{
		ClampPos(newScroll);
		GetCurrentComponent()->layout.scroll.X = newScroll;
	}

	void View::SetScrollSecondary(Size newScrollSecondary)
	{
		ClampPos(newScrollSecondary);
		GetCurrentComponent()->layout.scroll.Y = newScrollSecondary;
	}

	void View::SetAlignment(Alignment newAlignment)
	{
		GetCurrentComponent()->layout.alignment = newAlignment;
	}

	void View::SetOrder(Order newOrder)
	{
		GetCurrentComponent()->layout.order.X = newOrder;
	}

	void View::SetOrderSecondary(Order newOrderSecondary)
	{
		GetCurrentComponent()->layout.order.Y = newOrderSecondary;
	}

	void View::SetSpacing(Size newSpacing)
	{
		ClampPos(newSpacing);
		GetCurrentComponent()->spacing = newSpacing;
	}

	void View::AddSpacing(Size addSpacing)
	{
		extraSpacing += addSpacing;
		ClampPos(extraSpacing);
	}

	void View::SetPadding(Size newPadding)
	{
		SetPadding(newPadding, newPadding, newPadding, newPadding);
	}

	void View::SetPadding(Size newPadding, Size newPaddingSecondary)
	{
		SetPadding(newPadding, newPadding, newPaddingSecondary, newPaddingSecondary);
	}

	void View::SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondary)
	{
		SetPadding(newPaddingBefore, newPaddingAfter, newPaddingSecondary, newPaddingSecondary);
	}

	void View::SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondaryBefore, Size newPaddingSecondaryAfter)
	{
		auto &layout = GetCurrentComponent()->layout;
		layout.paddingBefore.X = newPaddingBefore;
		layout.paddingAfter .X = newPaddingAfter;
		layout.paddingBefore.Y = newPaddingSecondaryBefore;
		layout.paddingAfter .Y = newPaddingSecondaryAfter;
		ClampSize(layout.paddingBefore);
		ClampSize(layout.paddingAfter);
	}

	void View::SetTextPadding(Size newHorizontalPadding)
	{
		SetTextPadding(newHorizontalPadding, newHorizontalPadding, newHorizontalPadding, newHorizontalPadding);
	}

	void View::SetTextPadding(Size newHorizontalPadding, Size newVerticalPadding)
	{
		SetTextPadding(newHorizontalPadding, newHorizontalPadding, newVerticalPadding, newVerticalPadding);
	}

	void View::SetTextPadding(Size newHorizontalPaddingBefore, Size newHorizontalPaddingAfter, Size newVerticalPaddingBefore, Size newVerticalPaddingAfter)
	{
		auto &content = GetCurrentComponent()->content;
		content.textPaddingBefore.X = newHorizontalPaddingBefore;
		content.textPaddingAfter .X = newHorizontalPaddingAfter;
		content.textPaddingBefore.Y = newVerticalPaddingBefore;
		content.textPaddingAfter .Y = newVerticalPaddingAfter;
		ClampSize(content.textPaddingBefore);
		ClampSize(content.textPaddingAfter);
	}

	void View::SetEnabled(bool newEnabled)
	{
		auto &content = GetCurrentComponent()->content;
		content.enabled = newEnabled;
	}

	void View::SetHandleButtons(bool newHandleButtons)
	{
		GetCurrentComponent()->handleButtons = newHandleButtons;
	}

	void View::SetHandleWheel(bool newHandleWheel)
	{
		GetCurrentComponent()->handleWheel = newHandleWheel;
	}

	void View::SetHandleInput(bool newHandleInput)
	{
		GetCurrentComponent()->handleInput = newHandleInput;
	}

#if DebugGuiView
	void View::SetDebugMe(bool newDebugMe)
	{
		GetCurrentComponent()->debugMe = newDebugMe;
	}
#endif

	void View::SetTextAlignment(Alignment newHorizontalTextAlignment, Alignment newVerticalTextAlignment)
	{
		GetCurrentComponent()->content.horizontalTextAlignment = newHorizontalTextAlignment;
		GetCurrentComponent()->content.verticalTextAlignment = newVerticalTextAlignment;
	}

	void View::SetRootRect(std::optional<Rect> newRootRect)
	{
		if (newRootRect)
		{
			ClampPos(newRootRect->pos);
			ClampSize(newRootRect->size);
		}
		if (rootRect != newRootRect)
		{
			rootRect = newRootRect;
			shouldDoLayout = true;
		}
	}

	void View::SetParentFillRatio(Size newParentFillRatio)
	{
		ClampSize(newParentFillRatio);
		GetCurrentComponent()->layout.parentFillRatio.X = newParentFillRatio;
	}

	void View::SetParentFillRatioSecondary(Size newParentFillRatioSecondary)
	{
		ClampSize(newParentFillRatioSecondary);
		GetCurrentComponent()->layout.parentFillRatio.Y = newParentFillRatioSecondary;
	}

	void View::RequestRepeatFrame()
	{
		shouldRepeatFrame = true;
	}

	void View::BeginComponent(ComponentKey key)
	{
		// TODO-REDO_UI: limit depth
		GetOrAllocComponent(key);
		auto &g = Host::Ref();
		auto &clipRect = componentStack.back().clipRect;
		// TODO-REDO_UI: separate self and child clip rect
		g.SetClipRect(clipRect);
#if DebugGuiView
# if DebugGuiViewRects
		auto &component = *GetCurrentComponent();
		g.DrawRect(component.rect, 0x80FF0000_argb);
# endif
#endif
	}

	void View::EndComponent()
	{
		Assert(!componentStack.empty());
		if (prevSiblingIndex)
		{
			componentStore[*prevSiblingIndex].nextSiblingIndex.reset();
		}
		else
		{
			componentStore[componentStack.back().index].firstChildIndex.reset();
		}
#if DebugGuiView
# if DebugGuiViewRects
		{
			auto &component = *GetCurrentComponent();
			if (component.debugMe)
			{
				Log(component.rect.pos.X, " ", component.rect.pos.Y, " ", component.rect.size.X, " ", component.rect.size.Y);
			}
		}
# endif
#endif
		prevSiblingIndex = componentStack.back().index;
		componentStack.pop_back();
		for (auto &child : componentStore.GetChildRange(componentStore[*prevSiblingIndex]))
		{
			UpdateShouldDoLayout(child);
			UpdateShouldRepeatFrame(child);
		}
		if (!componentStack.empty())
		{
			auto &g = Host::Ref();
			auto &clipRect = componentStack.back().clipRect;
			g.SetClipRect(clipRect);
		}
	}

#if DebugGuiView
	void View::DebugDump(int32_t depth, Component &component)
	{
		std::ostringstream keySs;
		// for (auto ch : componentStore.DereferenceSpan(component.key))
		// {
		// 	keySs << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int(ch) & 0xFF) << ' ';
		// }
		keySs << componentStore.DereferenceSpan(component.key);
		Log(std::string(depth, ' '), "component: ", keySs.str(), ", size: ", component.rect.size.X, " ", component.rect.size.Y);
		for (auto &child : componentStore.GetChildRange(component))
		{
			DebugDump(depth + 1, child);
		}
	}
#endif

	void View::UpdateUnderMouse(Component &component)
	{
		if (component.rect.Contains(*mousePos)) // TODO-REDO_UI: clip to parent
		{
			auto index = GetComponentIndex(component);
			underMouseIndex = index;
			component.hovered = true;
			if (component.handleButtons)
			{
				handleButtonsIndex = index;
			}
			if (component.handleWheel)
			{
				handleWheelIndex = index;
			}
			if (component.handleInput)
			{
				handleInputIndex = index;
			}
			for (auto &child : componentStore.GetChildRange(component))
			{
				UpdateUnderMouse(child);
				if (child.hovered)
				{
					break;
				}
			}
		}
	}

	void View::ComponentStore::PruneKeys()
	{
		// TODO-REDO_UI: maybe do this only when wasted space ratio exceeds 2 or something
		ComponentKeyStoreSize newEnd = 0;
		std::vector<char> newKeyData(keyData.size());
		for (auto &item : *this)
		{
			std::copy_n(keyData.data() + item.key.base, item.key.size, newKeyData.data() + newEnd);
			item.key.base = newEnd;
			newEnd += item.key.size;
		}
		newKeyData.resize(newEnd);
		std::swap(keyData, newKeyData);
	}

	void View::SetRendererUpOuter(bool newRendererUp)
	{
		if (rendererUp == newRendererUp)
		{
			return;
		}
		rendererUp = newRendererUp;
		SetRendererUp(newRendererUp);
	}

	void View::SetRendererUp(bool /* newRendererUp */)
	{
	}

	void View::HandleBeforeFrame()
	{
	}

	void View::HandleBeforeGui()
	{
	}

	void View::HandleAfterGui()
	{
	}

	void View::HandleAfterFrame()
	{
	}

	void View::HandleTick()
	{
	}

	bool View::HandleGui()
	{
#if DebugGuiView
		debugFindIterationCount = 0;
#endif
		componentStore.BeginFrame();
		Assert(componentStack.empty());
		exitWhenRootMouseDown = false;
		underMouseIndex.reset();
		handleButtonsIndex.reset();
		handleWheelIndex.reset();
		handleInputIndex.reset();
		if (mousePos && rootIndex)
		{
			UpdateUnderMouse(componentStore[*rootIndex]);
		}
		Gui();
		Assert(componentStack.empty());
		prevSiblingIndex.reset();
		componentMouseClickEvent.reset();
		componentMouseScrollEvent.reset();
		for (auto &component : componentStore)
		{
			component.hovered = false;
		}
		if (componentStore.EndFrame())
		{
			shouldDoLayout = true;
		}
		if (rootIndex)
		{
			auto &root = componentStore[*rootIndex];
			if (rootRect)
			{
				root.layout.minSize = { rootRect->size.X, rootRect->size.Y };
				root.layout.maxSize = { rootRect->size.X, rootRect->size.Y };
			}
			else
			{
				root.layout.minSize = { MinSizeFitChildren{}, MinSizeFitChildren{} };
				root.layout.maxSize = { MaxSizeInfinite{}, MaxSizeInfinite{} };
			}
			UpdateShouldDoLayout(root);
			UpdateShouldRepeatFrame(root);
		}
#if DebugGuiView
# if DebugGuiViewStats
		Log("stats: ", componentStore.debugAllocCount, " ", componentStore.debugFreeCount, " ", debugFindIterationCount, " ", componentStore.keyData.size());
# endif
# if DebugGuiViewDump
		if (shouldDoDebugDump)
		{
			if (rootIndex)
			{
				DebugDump(0, componentStore[*rootIndex]);
			}
			else
			{
				Log("(no components)");
			}
			shouldDoDebugDump = false;
		}
# endif
#endif
		if (shouldDoLayout)
		{
			// TODO-REDO_UI: do layout only on the subset of the component tree that needs it
			DoLayout();
			shouldDoLayout = false;
			RequestRepeatFrame();
		}
		bool result = false;
		if (shouldRepeatFrame)
		{
			result = true;
			shouldRepeatFrame = false;
		}
		return result;
	}

	void View::DoLayoutContentSize(Axis parentPrimaryAxis, Component &component)
	{
		component.rect.size = { 0, 0 };
		for (auto &child : componentStore.GetChildRange(component))
		{
			DoLayoutContentSize(component.layout.primaryAxis, child);
			for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
			{
				auto xyAxis = AxisBase(component.layout.primaryAxis) ^ psAxis;
				if (psAxis == 0 && !component.layout.layered)
				{
					component.rect.size % xyAxis += child.rect.size % xyAxis + child.layout.spacingFromParent;
				}
				else
				{
					component.rect.size % xyAxis = std::max(component.rect.size % xyAxis, child.rect.size % xyAxis);
				}
			}
		}
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.layout.primaryAxis) ^ psAxis;
			auto parentPsAxis = AxisBase(parentPrimaryAxis) ^ xyAxis;
			auto &effectiveSizeC = component.rect.size % xyAxis;
			effectiveSizeC += component.layout.paddingBefore % psAxis + component.layout.paddingAfter % psAxis;
			if (!std::holds_alternative<MaxSizeInfinite>(component.layout.maxSize % parentPsAxis))
			{
				effectiveSizeC = 0;
			}
			if (auto *size = std::get_if<Size>(&(component.layout.minSize % parentPsAxis)))
			{
				effectiveSizeC = std::max(effectiveSizeC, *size);
			}
			if (auto *size = std::get_if<Size>(&(component.layout.maxSize % parentPsAxis)))
			{
				effectiveSizeC = std::min(effectiveSizeC, *size);
			}
		}
		ClampSize(component.rect.size);
	}

	void View::DoLayoutFillSpace(Component &component)
	{
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			int32_t childPosition = 0;
			auto xyAxis = AxisBase(component.layout.primaryAxis) ^ psAxis;
			auto space = component.rect.size % xyAxis - component.layout.paddingBefore % psAxis - component.layout.paddingAfter % psAxis;
			if (psAxis == 0 && !component.layout.layered)
			{
				Size spaceUsed = 0;
				Size fillRatioSum = 0;
				for (auto &child : componentStore.GetChildRange(component))
				{
					spaceUsed += child.rect.size % xyAxis + child.layout.spacingFromParent;
					fillRatioSum += child.layout.parentFillRatio % psAxis;
					child.fillSatisfied = child.layout.parentFillRatio % psAxis == 0;
				}
				auto spaceLeft = std::max(0, space - spaceUsed);
#if DebugGuiView
# if DebugGuiViewChildren
				if (component.debugMe)
				{
					Log(space, " ", spaceUsed);
					std::ostringstream ss;
					for (auto i = component.firstChildIndex; i; i = componentStore[*i].nextSiblingIndex)
					{
						ss << i->value << " ";
					}
					Log("  ", ss.str());
				}
# endif
#endif
				if (fillRatioSum && spaceLeft)
				{
					bool lastRound = false;
					while (true)
					{
						Size partialFillRatioSum = 0;
						Size primarySpaceFilled = 0;
						Size fillRatioHandled = 0;
						bool roundLimitedByMaxSize = false;
						for (auto &child : componentStore.GetChildRange(component))
						{
							if (!child.fillSatisfied)
							{
								auto prevPartialFillRatioSum = partialFillRatioSum;
								partialFillRatioSum += child.layout.parentFillRatio % psAxis;
								auto growLow = spaceLeft * prevPartialFillRatioSum / fillRatioSum;
								auto growHigh = spaceLeft * partialFillRatioSum / fillRatioSum;
								auto willGrow = growHigh - growLow;
								bool limitedByMaxSize = false;
								if (auto *size = std::get_if<Size>(&(child.layout.maxSize % psAxis)))
								{
									auto canGrow = *size - child.rect.size % xyAxis;
									if (willGrow > canGrow)
									{
										willGrow = canGrow;
										limitedByMaxSize = true;
										roundLimitedByMaxSize = true;
									}
								}
								if (lastRound || limitedByMaxSize)
								{
									child.rect.size % xyAxis += willGrow;
									primarySpaceFilled += willGrow;
									fillRatioHandled += child.layout.parentFillRatio % psAxis;
									child.fillSatisfied = true;
								}
							}
						}
						spaceLeft -= primarySpaceFilled;
						fillRatioSum -= fillRatioHandled;
						if (lastRound || spaceLeft == 0)
						{
							break;
						}
						if (!roundLimitedByMaxSize)
						{
							lastRound = true;
						}
					}
				}
			}
			else
			{
				for (auto &child : componentStore.GetChildRange(component))
				{
					if (child.layout.parentFillRatio % psAxis > 0)
					{
						child.rect.size % xyAxis = std::max(space, child.rect.size % xyAxis);
						if (auto *size = std::get_if<Size>(&(child.layout.maxSize % psAxis)))
						{
							child.rect.size % xyAxis = std::min(*size, child.rect.size % xyAxis);
						}
					}
				}
			}
			Size spaceUsed = 0;
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (psAxis == 0 && !component.layout.layered)
				{
					spaceUsed += child.rect.size % xyAxis + child.layout.spacingFromParent;
				}
				else
				{
					spaceUsed = std::max(spaceUsed, child.rect.size % xyAxis);
				}
			}
			auto minScroll = std::min(space - spaceUsed, 0);
			auto maxScroll = 0;
			auto spaceLeft = std::max(0, space - spaceUsed);
			auto alignmentOffset = spaceLeft * Size(component.layout.alignment) / 2;
			auto effectiveScroll = std::clamp(component.layout.scroll % psAxis, minScroll, maxScroll) + alignmentOffset;
			component.contentSpace % psAxis = space;
			component.contentSize % psAxis = spaceUsed;
			component.overflow % psAxis = -minScroll;
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (psAxis == 0 && !component.layout.layered)
				{
					childPosition += child.layout.spacingFromParent;
				}
				child.rect.pos % xyAxis = childPosition;
				if (psAxis == 0 && !component.layout.layered)
				{
					childPosition += child.rect.size % xyAxis;
				}
			}
			auto effectivePaddingBefore = (component.layout.order % psAxis == Order::highToLow ? component.layout.paddingAfter : component.layout.paddingBefore) % psAxis;
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (component.layout.order % psAxis == Order::highToLow)
				{
					child.rect.pos % xyAxis = childPosition - child.rect.size % xyAxis - child.rect.pos % xyAxis;
				}
				child.rect.pos % xyAxis += component.rect.pos % xyAxis + effectiveScroll + effectivePaddingBefore;
				ClampPos(child.rect.pos);
			}
		}
		for (auto &child : componentStore.GetChildRange(component))
		{
			DoLayoutFillSpace(child);
		}
	}

	void View::DoLayout()
	{
#if DebugGuiView
		Log("DoLayout");
#endif
		if (rootIndex)
		{
			auto &root = componentStore[*rootIndex];
			DoLayoutContentSize(Axis::horizontal, root);
			if (rootRect)
			{
				root.rect.pos = rootRect->pos;
			}
			else
			{
				root.rect.pos = { 0, 0 };
			}
			DoLayoutFillSpace(root);
			rootEffectiveSize = root.rect.size;
		}
		else
		{
			rootEffectiveSize = { 0, 0 };
		}
	}

	void View::UpdateShouldDoLayout(Component &component)
	{
		if (component.prevLayout != component.layout)
		{
			component.prevLayout = component.layout;
			shouldDoLayout = true;
		}
	}

	void View::UpdateShouldRepeatFrame(Component &component)
	{
		if (component.prevContent != component.content)
		{
			component.prevContent = component.content;
			RequestRepeatFrame();
		}
	}

	bool View::IsHovered() const
	{
		return GetCurrentComponent()->hovered;
	}

	View::Size View::GetOverflow() const
	{
		return GetCurrentComponent()->overflow.X;
	}

	View::Size View::GetOverflowSecondary() const
	{
		return GetCurrentComponent()->overflow.Y;
	}

	View::Rect View::GetRect() const
	{
		return GetCurrentComponent()->rect;
	}

	bool View::IsMouseDown() const
	{
		return IsMouseDown(SDL_BUTTON_LEFT);
	}

	bool View::IsMouseDown(MouseButtonIndex button) const
	{
		return componentMouseDownEvent && GetCurrentComponentIndex() == componentMouseDownEvent->component && componentMouseDownEvent->button == button;
	}

	bool View::IsClicked() const
	{
		return IsClicked(SDL_BUTTON_LEFT);
	}

	bool View::IsClicked(MouseButtonIndex button) const
	{
		return componentMouseClickEvent && GetCurrentComponentIndex() == componentMouseClickEvent->component && componentMouseClickEvent->button == button;
	}

	std::optional<View::Pos2> View::GetScrollDistance() const
	{
		if (componentMouseScrollEvent && GetCurrentComponentIndex() == componentMouseScrollEvent->component)
		{
			return componentMouseScrollEvent->distance;
		}
		return std::nullopt;
	}

	bool View::HasInputFocus() const
	{
		if (!inputFocus)
		{
			return false;
		}
		auto componentIndex = GetCurrentComponentIndex();
		while (auto nextComponentIndex = componentStore[*componentIndex].forwardInputFocusIndex)
		{
			componentIndex = *nextComponentIndex;
		}
		return componentIndex == inputFocus->component;
	}

	void View::GiveInputFocus()
	{
		SetInputFocus(GetCurrentComponentIndex());
	}

	std::string_view View::ComponentStore::DereferenceSpan(ComponentKeyStoreSpan span) const
	{
		return std::string_view(keyData.data() + span.base, span.size);
	}

	bool View::ComponentStore::ChildIterator::operator !=(const ChildIterator &other) const
	{
		return index != other.index;
	}

	View::Component &View::ComponentStore::ChildIterator::operator *()
	{
		return store[*index];
	}

	View::ComponentStore::ChildIterator &View::ComponentStore::ChildIterator::operator ++()
	{
		index = store[*index].nextSiblingIndex;
		return *this;
	}

	View::ComponentIndex View::ComponentStore::Alloc()
	{
		return { CrossFrameItemStore::Alloc() };
	}

	void View::ComponentStore::Free(CrossFrameItem::Index index)
	{
		auto componentIndex = ComponentIndex{ index };
		if (view.underMouseIndex && *view.underMouseIndex == componentIndex)
		{
			view.underMouseIndex.reset();
		}
		if (view.handleButtonsIndex && *view.handleButtonsIndex == componentIndex)
		{
			view.handleButtonsIndex.reset();
		}
		if (view.handleWheelIndex && *view.handleWheelIndex == componentIndex)
		{
			view.handleWheelIndex.reset();
		}
		if (view.componentMouseDownEvent && view.componentMouseDownEvent->component == componentIndex)
		{
			view.componentMouseDownEvent.reset();
		}
		if (view.componentMouseClickEvent && view.componentMouseClickEvent->component == componentIndex)
		{
			view.componentMouseClickEvent.reset();
		}
		if (view.componentMouseScrollEvent && view.componentMouseScrollEvent->component == componentIndex)
		{
			view.componentMouseScrollEvent.reset();
		}
		if (view.handleInputIndex && *view.handleInputIndex == componentIndex)
		{
			view.handleInputIndex.reset();
		}
		if (view.inputFocus && view.inputFocus->component == componentIndex)
		{
			view.SetInputFocus(std::nullopt);
		}
		auto &component = (*this)[{ index }];
		component.context.reset();
	}

	bool View::ComponentStore::EndFrame()
	{
		auto removedSome = CrossFrameItemStore::EndFrame();
		if (removedSome)
		{
			PruneKeys();
		}
		return removedSome;
	}

	std::optional<View::ComponentIndex> View::GetCurrentComponentIndex() const
	{
		if (!componentStack.empty())
		{
			return (componentStack.end() - 1)->index;
		}
		return std::nullopt;
	}

	View::Component *View::GetCurrentComponent()
	{
		auto index = GetCurrentComponentIndex();
		if (index)
		{
			return &componentStore[*index];
		}
		return nullptr;
	}

	const View::Component *View::GetCurrentComponent() const
	{
		auto index = GetCurrentComponentIndex();
		if (index)
		{
			return &componentStore[*index];
		}
		return nullptr;
	}

	std::optional<View::ComponentIndex> View::GetParentComponentIndex() const
	{
		if (componentStack.size() > 1)
		{
			return (componentStack.end() - 2)->index;
		}
		return std::nullopt;
	}

	View::Component *View::GetParentComponent()
	{
		auto index = GetParentComponentIndex();
		if (index)
		{
			return &componentStore[*index];
		}
		return nullptr;
	}

	const View::Component *View::GetParentComponent() const
	{
		auto index = GetParentComponentIndex();
		if (index)
		{
			return &componentStore[*index];
		}
		return nullptr;
	}

	View::ComponentIndex View::GetComponentIndex(const Component &component) const
	{
		return ComponentIndex{ componentStore.GetItemIndex(component) };
	}

	View::Size2 View::GetRootEffectiveSize() const
	{
		return rootEffectiveSize;
	}

	void View::SetMousePos(std::optional<Pos2> newMousePos)
	{
		mousePos = newMousePos;
	}

	std::optional<View::Pos2> View::GetMousePos() const
	{
		return mousePos;
	}

	void View::SetExitWhenRootMouseDown(bool newExitWhenRootMouseDown)
	{
		exitWhenRootMouseDown = newExitWhenRootMouseDown;
	}

	void View::SetInputFocus(std::optional<ComponentIndex> componentIndex)
	{
		if (inputFocus && !componentIndex)
		{
			Host::Ref().StopTextInput();
			inputFocus.reset();
		}
		if (componentIndex)
		{
			while (auto nextComponentIndex = componentStore[*componentIndex].forwardInputFocusIndex)
			{
				componentIndex = *nextComponentIndex;
			}
			if (!inputFocus)
			{
				Host::Ref().StartTextInput();
			}
			inputFocus = InputFocus{ *componentIndex };
		}
	}

	void View::HandleWindowHidden()
	{
		HandleMouseLeave();
		SetInputFocus(std::nullopt);
	}

	void View::HandleWindowShown()
	{
	}

	void View::HandleMouseEnter()
	{
		int x = 0;
		int y = 0;
		SDL_GetMouseState(&x, &y);
		float rx = 0;
		float ry = 0;
		// TODO-REDO_UI: this seems fishy, verify
		SDL_RenderWindowToLogical(Host::Ref().GetRenderer(), x, y, &rx, &ry);
		SetMousePos(Pos2{ int32_t(rx), int32_t(ry) });
	}

	void View::HandleMouseLeave()
	{
		SetMousePos(std::nullopt);
		componentMouseClickEvent.reset();
		componentMouseDownEvent.reset();
		componentMouseScrollEvent.reset();
	}

	void View::SetOnTop(bool /* newOnTop */)
	{
	}

	void View::SetOnTopOuter(bool newOnTop)
	{
		if (onTop == newOnTop)
		{
			return;
		}
		if (onTop && !newOnTop)
		{
			HandleMouseLeave();
			HandleWindowHidden();
		}
		if (!onTop && newOnTop)
		{
			HandleWindowShown();
			HandleMouseEnter();
		}
		onTop = newOnTop;
		SetOnTop(newOnTop);
	}

	void View::BeginPanel(ComponentKey key)
	{
		BeginComponent(key);
	}

	void View::BeginHPanel(ComponentKey key)
	{
		BeginPanel(key);
		SetPrimaryAxis(Axis::horizontal);
	}

	void View::BeginVPanel(ComponentKey key)
	{
		BeginPanel(key);
		SetPrimaryAxis(Axis::vertical);
	}

	void View::EndPanel()
	{
		EndComponent();
	}

	void View::BeginScrollpanel(ComponentKey key)
	{
		BeginScrollpanel(key, {});
	}

	void View::BeginScrollpanel(ComponentKey key, ScrollpanelDimension primary)
	{
		BeginScrollpanel(key, primary, { ScrollpanelDimension::Visibility::never });
	}

	void View::BeginScrollpanel(ComponentKey key, ScrollpanelDimension primary, ScrollpanelDimension secondary)
	{
		BeginPanel(key);
		SetHandleButtons(true);
		SetHandleWheel(true);
		auto &parentComponent = *GetParentComponent();
		auto &component = *GetCurrentComponent();
		auto &context = SetContext<ScrollpanelContext>(component);
		context.dimension = { primary, secondary };
		SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
	}

	void View::EndScrollpanel()
	{
		static constexpr auto scrollbarTrackColor = 0x80FFFFFF_argb;
		static constexpr auto scrollbarThumbColor = 0xFFFFFFFF_argb;
		static constexpr auto wheelFactor         = 120.f;
		static constexpr auto maxMomentum         = 420.f;
		static constexpr auto wheelDirectFactor   = 20.f;
		static constexpr Size maxThickness        = 6;
		static constexpr Size minLength           = 6;
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<ScrollpanelContext>(component);
		auto thickness = maxThickness;
		struct Side
		{
			bool show = false;
			bool reverseSide = false;
			Size pos = 0;
			Size size = 0;
		};
		auto r = GetRect();
		ExtendedSize2<Side> showSide;
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.prevLayout.primaryAxis) ^ psAxis;
			auto &dimension = context.dimension % psAxis;
			auto show = (dimension.visibility == ScrollpanelDimension::Visibility::always) ||
		                (dimension.visibility == ScrollpanelDimension::Visibility::ifUseful && (component.overflow % psAxis));
			auto &side = showSide % xyAxis;
			side.show = show;
			side.size = r.size % xyAxis;
		}
		{
			auto &primarySide   = showSide %  AxisBase(component.prevLayout.primaryAxis)     ;
			auto &secondarySide = showSide % (AxisBase(component.prevLayout.primaryAxis) ^ 1);
			if (primarySide.show)
			{
				secondarySide.size -= thickness;
				if (primarySide.reverseSide)
				{
					secondarySide.pos += thickness;
				}
			}
		}
		bool clearMouseDownPos = true;
		auto scrollDistance = GetScrollDistance();
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.prevLayout.primaryAxis) ^ psAxis;
			auto &side = showSide % xyAxis;
			if (side.show)
			{
				auto contentSpace = component.contentSpace      % psAxis;
				auto contentSize  = component.contentSize       % psAxis;
				auto overflow     = component.overflow          % psAxis;
				auto scroll       = component.prevLayout.scroll % psAxis;
				Rect trackRect{ r.TopLeft(), { thickness, thickness } };
				trackRect.pos  % xyAxis += side.pos;
				trackRect.size % xyAxis =  side.size;
				if (side.reverseSide)
				{
					component.layout.paddingBefore % (psAxis ^ 1) += thickness;
				}
				else
				{
					trackRect.pos % (xyAxis ^ 1) += r.size % (xyAxis ^ 1) - thickness;
					component.layout.paddingAfter % (psAxis ^ 1) += thickness;
				}
				auto &g = Host::Ref();
				g.FillRect(trackRect, scrollbarTrackColor);
				Rect thumbRect = trackRect;
				auto &thumbLength = thumbRect.size % xyAxis;
				auto &trackLength = trackRect.size % xyAxis;
				thumbLength = std::min(trackLength, std::max(minLength, contentSpace * trackLength / contentSize));
				thumbRect.pos % xyAxis = (trackRect.pos % xyAxis) + scroll * (trackLength - thumbLength) / -overflow;
				g.FillRect(thumbRect, scrollbarThumbColor);
				auto &stickyScroll = context.stickyScroll % xyAxis;
				if (IsMouseDown(SDL_BUTTON_LEFT))
				{
					auto m = *GetMousePos();
					clearMouseDownPos = false;
					if (context.mouseDownInThumbPos)
					{
						auto wantThumbPos = m - *context.mouseDownInThumbPos;
						stickyScroll.SetMomentum(0.f);
						stickyScroll.SetValue(float(((wantThumbPos % xyAxis) - (trackRect.pos % xyAxis)) * -overflow / (trackLength - thumbLength)));
					}
					else
					{
						if (thumbRect.Contains(m))
						{
							context.mouseDownInThumbPos = m - thumbRect.TopLeft();
						}
						else if (trackRect.Contains(m))
						{
							context.mouseDownInThumbPos = thumbRect.size / 2;
						}
					}
				}
				if (scrollDistance)
				{
					auto distance = ((*scrollDistance) % xyAxis);
					if (g.GetMomentumScroll())
					{
						stickyScroll.SetMomentum(stickyScroll.GetMomentum() + distance * wheelFactor);
					}
					else
					{
						stickyScroll.SetValue(stickyScroll.GetValue() + distance * wheelDirectFactor);
					}
				}
				{
					auto momentum = stickyScroll.GetMomentum();
					if (std::abs(momentum) > maxMomentum)
					{
						stickyScroll.SetMomentum(std::copysign(maxMomentum, momentum));
					}
				}
				{
					auto value = stickyScroll.GetValue();
					auto valuePos = Pos(value);
					auto clampedValuePos = std::clamp(valuePos, -overflow, 0);
					if (valuePos != clampedValuePos)
					{
						stickyScroll.SetMomentum(0.f);
						stickyScroll.SetValue(float(clampedValuePos));
					}
				}
				component.layout.scroll % psAxis = Pos(stickyScroll.GetValue());
			}
		}
		if (clearMouseDownPos)
		{
			context.mouseDownInThumbPos.reset();
		}
		EndPanel();
	}

	void View::BeginText(ComponentKey key, StringView text, TextFlags textFlags, Rgba8 color)
	{
		BeginComponent(key);
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::selectable))
		{
			SetHandleButtons(true);
			SetHandleInput(true);
		}
		auto enabled = GetCurrentComponent()->prevContent.enabled;
		if (!enabled)
		{
			color.Alpha /= 2;
		}
		auto &g = Host::Ref();
		auto &parentComponent = *GetParentComponent();
		auto &component = *GetCurrentComponent();
		ShapeTextParameters stp{ g.InternText(text), std::nullopt, parentComponent.prevContent.horizontalTextAlignment };
		auto alignmentRect = component.rect;
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			// Using prevLayout so we have access to parameters set after this function returns.
			alignmentRect.pos % psAxis += component.prevContent.textPaddingBefore % psAxis;
			alignmentRect.size % psAxis -= component.prevContent.textPaddingBefore % psAxis + component.prevContent.textPaddingAfter % psAxis;
		}
		ClampSize(alignmentRect.size);
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::multiline))
		{
			stp.maxWidth = ShapeTextParameters::MaxWidth{ alignmentRect.size.X, true };
		}
		auto st = g.ShapeText(stp);
		auto &stw = g.GetShapedTextWrapper(st);
		auto wrappedSize = stw.GetWrappedSize();
		if (!stp.maxWidth)
		{
			wrappedSize.X -= letterSpacing;
		}
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::autoWidth))
		{
			if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
			{
				SetMinSize(wrappedSize.X);
			}
			else
			{
				SetMinSizeSecondary(wrappedSize.X);
			}
		}
		if (TextFlagBase(textFlags) & TextFlagBase(TextFlags::autoHeight))
		{
			if (parentComponent.prevLayout.primaryAxis == Axis::vertical)
			{
				SetMinSize(wrappedSize.Y);
			}
			else
			{
				SetMinSizeSecondary(wrappedSize.Y);
			}
		}
		auto contentSize = stw.GetContentSize();
		auto diff = alignmentRect.size - contentSize;
		// Using prevContent so we have access to parameters set after this function returns.
		// Using TruncateDiv causes some extremely weird results here and there, most notable in
		// the case of tool buttons, but this has been grandfathered in so whatever.
		auto offset = Pos2{
			TruncateDiv(diff.X * Size(parentComponent.prevContent.horizontalTextAlignment), 2),
			TruncateDiv(diff.Y * Size(parentComponent.prevContent.verticalTextAlignment), 2),
		};
		auto textPos = alignmentRect.pos + offset;
		auto wrapperPos = textPos - Pos2(0, fontCapLine);
		g.DrawText(textPos, st, color);
		auto &context = SetContext<TextContext>(component);
		context.selectionMax = stw.GetIndexEnd().clear;
		bool clearMouseDownPos = true;
		if (HasInputFocus())
		{
			context.stp = stp;
			{
				context.selectionBegin = std::clamp(context.selectionBegin, 0, context.selectionMax);
				context.selectionEnd = std::clamp(context.selectionEnd, 0, context.selectionMax);
			}
			auto cursorColor = 0xFFFFFFFF_argb;
			if (IsMouseDown(SDL_BUTTON_LEFT))
			{
				auto m = *GetMousePos();
				clearMouseDownPos = false;
				if (context.mouseDownPos)
				{
					context.selectionEnd = stw.ConvertPointToIndex(m - wrapperPos).clear;
				}
				else
				{
					context.mouseDownPos = m;
					context.selectionEnd = stw.ConvertPointToIndex(*context.mouseDownPos - wrapperPos).clear;
					context.selectionBegin = context.selectionEnd;
				}
			}
			{
				auto curPos = stw.ConvertClearToPoint(context.selectionEnd);
				Rect rect{ wrapperPos + curPos, { 1, fontTypeSize } };
				g.FillRect(rect, cursorColor);
			}
			if (context.selectionBegin != context.selectionEnd)
			{
				auto lo = std::min(context.selectionBegin, context.selectionEnd);
				auto hi = std::max(context.selectionBegin, context.selectionEnd);
				auto [ loPos, loLine ] = stw.ConvertClearToPointAndLine(lo);
				auto [ hiPos, hiLine ] = stw.ConvertClearToPointAndLine(hi);
				auto oldClipRect = g.GetClipRect();
				for (auto i = loLine; i <= hiLine; ++i)
				{
					Rect rect{ wrapperPos + Pos2(-selectionOverhang, i * fontTypeSize), { wrappedSize.X + 2 * selectionOverhang, fontTypeSize } };
					if (i == loLine)
					{
						rect.pos.X += loPos.X;
						rect.size.X -= loPos.X;
					}
					if (i == hiLine)
					{
						rect.size.X -= wrappedSize.X - hiPos.X + selectionOverhang;
					}
					g.SetClipRect(rect & oldClipRect);
					g.FillRect({ wrapperPos - Pos2(selectionOverhang, 0), wrappedSize + Pos2(2 * selectionOverhang, 0) }, cursorColor);
					g.DrawText(textPos, st, InvertColor(color.NoAlpha()).WithAlpha(255));
				}
				g.SetClipRect(oldClipRect);
			}
		}
		if (clearMouseDownPos)
		{
			context.mouseDownPos.reset();
		}
	}

	void View::EndText()
	{
		EndComponent();
	}

	void View::Text(StringView text, Rgba8 color)
	{
		Text(text, text, color);
	}

	void View::Text(ComponentKey key, StringView text, Rgba8 color)
	{
		BeginText(key, text, TextFlags::none, color);
		EndText();
	}

	void View::BeginButton(ComponentKey key, StringView text, ButtonFlags buttonFlags, Rgba8 color)
	{
		BeginComponent(key);
		auto &g = Host::Ref();
		auto edgeColor = 0xFFFFFFFF_argb;
		{
			auto &component = *GetCurrentComponent();
			if (component.prevContent.enabled)
			{
				SetHandleButtons(true);
			}
			if (!component.prevContent.enabled)
			{
				edgeColor.Alpha /= 2;
				color.Alpha /= 2;
			}
			else if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::stuck))
			{
				g.FillRect(component.rect, edgeColor);
				color = InvertColor(color.NoAlpha()).WithAlpha(color.Alpha);
			}
			else if (IsHovered())
			{
				auto backgroundColor = edgeColor;
				if (IsMouseDown(SDL_BUTTON_LEFT) || IsClicked(SDL_BUTTON_LEFT))
				{
					color = InvertColor(color.NoAlpha()).WithAlpha(color.Alpha);
				}
				else
				{
					backgroundColor.Alpha /= 2;
				}
				g.FillRect(component.rect, backgroundColor);
			}
		}
		if (!text.view.empty())
		{
			auto textFlags = TextFlags::none;
			if (ButtonFlagBase(buttonFlags) & ButtonFlagBase(ButtonFlags::autoWidth))
			{
				textFlags = textFlags | TextFlags::autoWidth;
			}
			BeginText(key, text, textFlags, color);
			auto &component = *GetParentComponent();
			SetTextPadding(
				component.prevContent.textPaddingBefore.X,
				component.prevContent.textPaddingAfter.X,
				component.prevContent.textPaddingBefore.Y,
				component.prevContent.textPaddingAfter.Y
			);
			EndText();
		}
		g.DrawRect(GetRect(), edgeColor);
	}

	bool View::EndButton()
	{
		bool clicked = IsClicked(SDL_BUTTON_LEFT);
		EndComponent();
		return clicked;
	}

	bool View::Button(StringView text, SetSizeSize size, ButtonFlags buttonFlags)
	{
		return Button(text, text, size, buttonFlags);
	}

	bool View::Button(ComponentKey key, StringView text, SetSizeSize size, ButtonFlags buttonFlags)
	{
		BeginButton(key, text, buttonFlags);
		SetSize(size);
		return EndButton();
	}

	void View::BeginCheckbox(ComponentKey key, StringView text, bool &checked, CheckboxFlags checkboxFlags, Rgba8 color)
	{
		constexpr Size boxSize    = 12;
		constexpr Size boxSpacing = 4;
		auto &g = Host::Ref();
		auto edgeColor = 0xFFFFFFFF_argb;
		BeginComponent(key);
		SetPrimaryAxis(Axis::vertical);
		auto enabled = GetCurrentComponent()->prevContent.enabled;
		if (enabled)
		{
			SetHandleButtons(true);
		}
		if (!enabled)
		{
			edgeColor.Alpha /= 2;
			color.Alpha /= 2;
		}
		if (!text.view.empty())
		{
			auto textFlags = TextFlags::none;
			if (CheckboxFlagBase(checkboxFlags) & CheckboxFlagBase(CheckboxFlags::multiline))
			{
				textFlags = textFlags | TextFlags::autoHeight | TextFlags::multiline;
			}
			else
			{
				if (CheckboxFlagBase(checkboxFlags) & CheckboxFlagBase(CheckboxFlags::autoWidth))
				{
					textFlags = textFlags | TextFlags::autoWidth;
				}
			}
			SetTextAlignment(Alignment::left, Alignment::center);
			BeginText(key, text, textFlags, color);
			auto &component = *GetParentComponent();
			SetTextPadding(
				component.prevContent.textPaddingBefore.X,
				component.prevContent.textPaddingAfter.X,
				component.prevContent.textPaddingBefore.Y,
				component.prevContent.textPaddingAfter.Y
			);
			EndText();
		}
		auto r = GetRect();
		SetPadding(0, 0, boxSize + boxSpacing, 0);
		Rect rr{ r.TopLeft() + Pos2{ 0, 0 }, { boxSize, boxSize } };
		auto round = bool(CheckboxFlagBase(checkboxFlags) & CheckboxFlagBase(CheckboxFlags::round));
		if (round)
		{
			g.DrawLine(rr.pos + Pos2{  4,  0 }, rr.pos + Pos2{  7,  0 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  8,  1 }, rr.pos + Pos2{  9,  1 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{ 10,  2 }, rr.pos + Pos2{ 10,  3 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{ 11,  4 }, rr.pos + Pos2{ 11,  7 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{ 10,  8 }, rr.pos + Pos2{ 10,  9 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  8, 10 }, rr.pos + Pos2{  9, 10 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  4, 11 }, rr.pos + Pos2{  7, 11 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  2, 10 }, rr.pos + Pos2{  3, 10 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  1,  8 }, rr.pos + Pos2{  1,  9 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  0,  4 }, rr.pos + Pos2{  0,  7 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  1,  2 }, rr.pos + Pos2{  1,  3 }, edgeColor);
			g.DrawLine(rr.pos + Pos2{  2,  1 }, rr.pos + Pos2{  3,  1 }, edgeColor);
		}
		else
		{
			g.DrawRect(rr, edgeColor);
		}
		if (IsClicked(SDL_BUTTON_LEFT))
		{
			checked = !checked;
		}
		if (checked)
		{
			if (round)
			{
				g.DrawLine(rr.pos + Pos2{ 3, 4 }, rr.pos + Pos2{ 3, 7 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 4, 3 }, rr.pos + Pos2{ 4, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 5, 3 }, rr.pos + Pos2{ 5, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 6, 3 }, rr.pos + Pos2{ 6, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 7, 3 }, rr.pos + Pos2{ 7, 8 }, edgeColor);
				g.DrawLine(rr.pos + Pos2{ 8, 4 }, rr.pos + Pos2{ 8, 7 }, edgeColor);
			}
			else
			{
				g.FillRect(rr.Inset(3), edgeColor);
			}
		}
	}

	bool View::EndCheckbox()
	{
		bool clicked = IsClicked(SDL_BUTTON_LEFT);
		EndComponent();
		return clicked;
	}

	bool View::Checkbox(StringView text, bool &checked, SetSizeSize size, CheckboxFlags checkboxFlags)
	{
		return Checkbox(text, text, checked, size, checkboxFlags);
	}

	bool View::Checkbox(ComponentKey key, StringView text, bool &checked, SetSizeSize size, CheckboxFlags checkboxFlags)
	{
		BeginCheckbox(key, text, checked, checkboxFlags);
		SetSize(size);
		return EndCheckbox();
	}

	bool View::MayBeHandledExclusively(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEWHEEL:
			return true;
			break;
		}
		return false;
	}

	void View::BeginModal(ComponentKey key)
	{
		BeginComponent(key);
		BeginVPanel("inner");
		SetParentFillRatio(0);
		SetParentFillRatioSecondary(0);
		auto &component = *GetCurrentComponent();
		auto &g = Host::Ref();
		auto rh = component.rect.Inset(1); // TODO-REDO_UI: child clip rect
		g.FillRect(component.rect, 0xFF000000_argb);
		g.DrawRect(rh, 0xFFFFFFFF_argb);
		SetPadding(2);
	}

	void View::EndModal()
	{
		EndPanel();
		EndComponent();
	}

	void View::Separator(ComponentKey key, int32_t size)
	{
		BeginComponent(key);
		auto &component = *GetCurrentComponent();
		auto &g = Host::Ref();
		g.FillRect(component.rect, 0xFF808080_argb);
		SetSize(size);
		EndComponent();
	}

	void View::TextSeparator(ComponentKey key, StringView text)
	{
		BeginHPanel(key);
		auto &parentComponent = *GetParentComponent();
		if (parentComponent.prevLayout.primaryAxis == Axis::vertical)
		{
			SetParentFillRatio(0);
			SetSize(17);
		}
		else
		{
			SetParentFillRatioSecondary(0);
			SetSizeSecondary(17);
		}
		SetSpacing(5);
		BeginVPanel("left");
		Separator("separator");
		EndPanel();
		BeginText("text", text, TextFlags::autoWidth);
		SetParentFillRatio(0);
		EndText();
		BeginVPanel("right");
		Separator("separator");
		EndPanel();
		EndComponent();
	}

	void View::Exit()
	{
		viewStack->Pop(this);
	}

	void View::PushAboveThis(std::shared_ptr<View> view)
	{
		viewStack->Push(view);
	}

	void View::BeginTextbox(ComponentKey key, std::string &str, TextboxFlags textboxFlags, Rgba8 color)
	{
		BeginComponent(key);
		auto backgroundColor = 0xFFFFFFFF_argb;
		auto &g = Host::Ref();
		{
			auto &component = *GetCurrentComponent();
			g.DrawRect(component.rect, backgroundColor);
		}
		auto &parentComponent = *GetParentComponent();
		if (parentComponent.prevLayout.primaryAxis == Axis::horizontal)
		{
			SetMaxSize(MaxSizeFitParent{});
		}
		else
		{
			SetMaxSizeSecondary(MaxSizeFitParent{});
		}
		auto textFlags = TextFlags::selectable;
		if (TextboxFlagBase(textboxFlags) & TextboxFlagBase(TextboxFlags::multiline))
		{
			textFlags = textFlags | TextFlags::autoHeight | TextFlags::multiline;
			SetTextAlignment(Alignment::left, Alignment::top);
		}
		else
		{
			textFlags = textFlags | TextFlags::autoWidth;
			SetTextAlignment(Alignment::left, Alignment::center);
		}
		BeginText("text", str, textFlags, color);
		auto &component = *GetParentComponent();
		auto &context = SetContext<TextboxContext>(component);
		context.changed = false;
		context.textComponentIndex = *GetCurrentComponentIndex();
		component.forwardInputFocusIndex = context.textComponentIndex;
		auto &textContext = GetContext<TextContext>(*GetCurrentComponent());
		if (HasInputFocus())
		{
			// TODO-REDO_UI: scrolling
			// TODO-REDO_UI: word selection with double click
			// TODO-REDO_UI: paragraph selection with double click
			auto stp = *textContext.stp;
			bool strChanged = true;
			ShapedTextIndex st;
			const TextWrapper *stw = nullptr;
			auto getSelectionMax = [&]() {
				return stw->GetIndexEnd().clear;
			};
			while (!inputFocus->events.empty())
			{
				if (strChanged)
				{
					stp.text = g.InternText(str); // just in case in the first iteration; TODO-REDO_UI: figure out when this should matter
					st = g.ShapeText(stp);
					stw = &g.GetShapedTextWrapper(st);
					strChanged = false;
					textContext.selectionBegin = std::clamp(textContext.selectionBegin, 0, getSelectionMax());
					textContext.selectionEnd = std::clamp(textContext.selectionEnd, 0, getSelectionMax());
				}
				auto getLo = [&]() {
					return std::min(textContext.selectionBegin, textContext.selectionEnd);
				};
				auto getHi = [&]() {
					return std::max(textContext.selectionBegin, textContext.selectionEnd);
				};
				auto remove = [&](TextWrapper::ClearIndex begin, TextWrapper::ClearIndex end) {
					auto beginRaw = stw->ConvertClearToIndex(begin).raw;
					auto endRaw = stw->ConvertClearToIndex(end).raw;
					str.erase(beginRaw, endRaw - beginRaw);
					textContext.selectionEnd = begin; // this may be incorrect so don't rely on it anymore in this iteration
					textContext.selectionBegin = textContext.selectionEnd;
					strChanged = true;
				};
				auto replace = [&](TextWrapper::ClearIndex begin, TextWrapper::ClearIndex end, std::string_view istr) {
					remove(begin, end);
					// TODO-REDO_UI: remove disallowed code points from istr
					auto beginRaw = stw->ConvertClearToIndex(begin).raw;
					str.insert(beginRaw, istr);
					for (auto iit = Utf8Begin(istr), iend = Utf8End(istr); iit != iend; ++iit)
					{
						begin += 1;
					}
					textContext.selectionEnd = begin; // this may be incorrect so don't rely on it anymore in this iteration
					textContext.selectionBegin = textContext.selectionEnd;
					strChanged = true;
				};
				auto copy = [&]() {
					Host::Ref().CopyTextToClipboard(str.substr(getLo(), getHi() - getLo()));
				};
				auto cut = [&]() {
					copy();
					remove(getLo(), getHi());
				};
				auto paste = [&]() {
					if (auto text = Host::Ref().PasteTextFromClipboard())
					{
						replace(getLo(), getHi(), *text);
					}
				};
				auto nextClear = [&](int32_t direction) {
					return std::clamp(textContext.selectionEnd + direction, 0, getSelectionMax());
				};
				auto adjustSelection = [&textContext](bool shift) {
					if (!shift)
					{
						textContext.selectionBegin = textContext.selectionEnd;
					}
				};
				auto &event = inputFocus->events.front();
				auto *keyDown = std::get_if<InputFocus::KeyDownEvent>(&event);
				auto upDown = false;
				if (keyDown && !keyDown->alt)
				{
					switch (keyDown->sym)
					{
					case SDLK_UP:
					case SDLK_DOWN:
						upDown = true;
						if (!context.upDownPrev)
						{
							context.upDownPrev = stw->ConvertClearToPoint(textContext.selectionEnd);
						}
						{
							context.upDownPrev->Y += (keyDown->sym == SDLK_DOWN ? 1 : -1) * fontTypeSize;
							textContext.selectionEnd = stw->ConvertPointToIndex(*context.upDownPrev).clear;
							adjustSelection(keyDown->shift);
							if (textContext.selectionEnd == 0 || textContext.selectionEnd == getSelectionMax())
							{
								context.upDownPrev.reset();
							}
						}
						break;

					case SDLK_LEFT:
						if (keyDown->ctrl)
						{
							Log("nyi"); // TODO-REDO_UI: word border
							break;
						}
						else
						{
							textContext.selectionEnd = nextClear(-1);
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_RIGHT:
						if (keyDown->ctrl)
						{
							Log("nyi"); // TODO-REDO_UI: word border
							break;
						}
						else
						{
							textContext.selectionEnd = nextClear(1);
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_HOME:
						if (keyDown->ctrl)
						{
							textContext.selectionEnd = 0;
						}
						else
						{
							Log("nyi"); // TODO-REDO_UI: line edges (visually, so not just \n)
							break;
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_END:
						if (keyDown->ctrl)
						{
							textContext.selectionEnd = getSelectionMax();
						}
						else
						{
							Log("nyi"); // TODO-REDO_UI: line edges (visually, so not just \n)
							break;
						}
						adjustSelection(keyDown->shift);
						break;

					case SDLK_INSERT:
						if (keyDown->ctrl && !keyDown->shift)
						{
							if (!keyDown->repeat)
							{
								copy();
							}
						}
						if (!keyDown->ctrl && keyDown->shift)
						{
							paste();
						}
						break;

					case SDLK_DELETE:
						if (keyDown->shift)
						{
							if (!keyDown->repeat)
							{
								cut();
							}
							break;
						}
						if (getLo() != getHi())
						{
							remove(getLo(), getHi());
							break;
						}
						if (keyDown->ctrl)
						{
							Log("nyi"); // TODO-REDO_UI: word border
							break;
						}
						remove(getLo(), nextClear(1));
						break;

					case SDLK_BACKSPACE:
						if (getLo() != getHi())
						{
							remove(getLo(), getHi());
							break;
						}
						if (keyDown->ctrl)
						{
							Log("nyi"); // TODO-REDO_UI: word border
							break;
						}
						remove(nextClear(-1), getLo());
						break;

					case SDLK_RETURN:
					case SDLK_KP_ENTER:
						if (TextboxFlagBase(textboxFlags) & TextboxFlagBase(TextboxFlags::multiline))
						{
							replace(getLo(), getHi(), "\n");
						}
						break;

					case SDLK_a:
						if (keyDown->ctrl && !keyDown->repeat)
						{
							textContext.selectionEnd = getSelectionMax();
							textContext.selectionBegin = 0;
						}
						break;

					case SDLK_c:
						if (keyDown->ctrl && !keyDown->repeat)
						{
							copy();
						}
						break;

					case SDLK_v:
						if (keyDown->ctrl)
						{
							paste();
						}
						break;

					case SDLK_x:
						if (keyDown->ctrl && !keyDown->repeat)
						{
							cut();
						}
						break;
					}
				}
				if (auto *textInput = std::get_if<InputFocus::TextInputEvent>(&event))
				{
					replace(getLo(), getHi(), textInput->input);
				}
				if (!upDown)
				{
					context.upDownPrev.reset();
				}
				if (strChanged)
				{
					context.changed = true;
				}
				inputFocus->events.pop_front();
			}
		}
		SetTextPadding(4);
		EndText();
	}

	void View::TextboxSelectAll()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextboxContext>(component);
		auto &textContext = GetContext<TextContext>(componentStore[context.textComponentIndex]);
		textContext.selectionEnd = textContext.selectionMax;
		textContext.selectionBegin = 0;
	}

	bool View::EndTextbox()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<TextboxContext>(component);
		bool changed = false;
		if (HasInputFocus())
		{
			changed = context.changed;
		}
		EndComponent();
		return changed;
	}

	bool View::Textbox(ComponentKey key, std::string &str, SetSizeSize size)
	{
		BeginTextbox(key, str, TextboxFlags::none);
		SetSize(size);
		return EndTextbox();
	}

	struct DropdownView : public View
	{
		static constexpr Size buttonSize = 17;

		Rect parentRect{ { 0, 0 }, { 0, 0 } };
		Alignment horizontalTextAlignment;
		Alignment verticalTextAlignment;
		// these two are (horizontal, vertical)
		Pos2 textPaddingBefore = { 0, 0 };
		Pos2 textPaddingAfter = { 0, 0 };
		int32_t selected;
		std::vector<std::string> items;
		enum class State
		{
			initial,
			active,
			done,
		};
		State state = State::initial;

		Rect GetRootRect() const
		{
			Size2 size{ parentRect.size.X, (parentRect.size.Y - 1) * int32_t(items.size()) + 5 };
			auto vrect = Host::Ref().GetSize().OriginRect();
			vrect.size -= size;
			return Rect{ RobustClamp(parentRect.pos, vrect), size };
		}

		void Gui() final override
		{
			SetRootRect(GetRootRect());
			SetExitWhenRootMouseDown(true);
			BeginModal("dropdown");
			SetSize(parentRect.size.X);
			SetPadding(1);
			SetSpacing(-1);
			int32_t buttonIndex = 0;
			for (auto &item : items)
			{
				BeginButton(buttonIndex, item, ButtonFlags::autoWidth);
				SetTextAlignment(horizontalTextAlignment, verticalTextAlignment);
				SetTextPadding(textPaddingBefore.X, textPaddingAfter.X, textPaddingBefore.Y, textPaddingAfter.Y);
				SetSize(parentRect.size.Y);
				if (EndButton())
				{
					selected = buttonIndex;
					Exit();
				}
				buttonIndex += 1;
			}
			EndModal();
		}

		bool HandleEvent(const SDL_Event &event) final override
		{
			auto handledByView = View::HandleEvent(event);
			if (HandleExitEvent(event))
			{
				return true;
			}
			if (MayBeHandledExclusively(event) && handledByView)
			{
				return true;
			}
			return false;
		}

		void Exit()
		{
			state = DropdownView::State::done;
			View::Exit();
		}
	};

	void View::BeginDropdown(ComponentKey key, int32_t &selected)
	{
		BeginComponent(key);
		StringView str = "";
		{
			auto &component = *GetCurrentComponent();
			auto &context = SetContext<DropdownContext>(component);
			auto &parentComponent = *GetParentComponent();
			SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
			if (selected >= 0 && selected < int32_t(context.items.size()))
			{
				str = context.items[selected];
			}
		}
		BeginButton("button", str, ButtonFlags::none);
		auto &component = *GetParentComponent();
		auto &context = SetContext<DropdownContext>(component);
		auto enabled = component.prevContent.enabled;
		SetEnabled(enabled);
		context.changed = false;
		context.lastSelected = selected;
		if (context.dropdownView && context.dropdownView->state == DropdownView::State::done)
		{
			selected = context.dropdownView->selected;
			context.changed = true;
			context.dropdownView.reset();
		}
		if (context.items != context.lastItems)
		{
			RequestRepeatFrame();
		}
		std::swap(context.items, context.lastItems);
		context.items.clear();
		context.clicked = EndButton();
	}

	bool View::EndDropdown()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		if (context.clicked)
		{
			context.dropdownView = std::make_shared<DropdownView>();
			auto r = GetRect();
			context.dropdownView->parentRect = Rect{
				Pos2{ r.pos - Pos2(1, 2 + context.lastSelected * (r.size.Y - 1)) },
				Size2{ r.size.X + 2, r.size.Y },
			};
			context.dropdownView->horizontalTextAlignment = component.prevContent.horizontalTextAlignment;
			context.dropdownView->verticalTextAlignment = component.prevContent.verticalTextAlignment;
			context.dropdownView->textPaddingBefore = component.prevContent.textPaddingBefore;
			context.dropdownView->textPaddingAfter = component.prevContent.textPaddingAfter;
			context.dropdownView->selected = context.lastSelected;
			for (auto &item : context.lastItems)
			{
				context.dropdownView->items.push_back(item);
			}
			PushAboveThis(context.dropdownView);
			context.dropdownView->state = DropdownView::State::active;
		}
		EndComponent();
		return context.changed;
	}

	void View::DropdownItem(StringView item)
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		context.items.push_back(std::string(item));
	}

	bool View::DropdownGetChanged() const
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		return context.changed;
	}

	bool View::Dropdown(ComponentKey key, std::span<StringView> items, int32_t &selected, SetSizeSize size)
	{
		BeginDropdown(key, selected);
		SetSize(size);
		for (auto item : items)
		{
			DropdownItem(item);
		}
		return EndDropdown();
	}

	void View::BeginColorButton(ComponentKey key, Rgb8 color)
	{
		BeginButton(key, "", ButtonFlags::none);
		auto rr = GetRect();
		auto &g = Host::Ref();
		g.FillRect(rr.Inset(1), color.WithAlpha(255));
	}

	bool View::EndColorButton()
	{
		return EndButton();
	}

	View::ExitEventType View::ClassifyExitEvent(const SDL_Event &event)
	{
		switch (event.type)
		{
		case SDL_KEYDOWN:
			if (!event.key.repeat)
			{
				switch (event.key.keysym.sym)
				{
				case SDLK_RETURN:
				case SDLK_RETURN2:
				case SDLK_KP_ENTER:
					return ExitEventType::tryOk;

				case SDLK_ESCAPE:
					return ExitEventType::exit;
				}
			}
			break;
		}
		return ExitEventType::none;
	}

	void View::BeginDialog(ComponentKey key, StringView title, std::optional<int32_t> size)
	{
		SetExitWhenRootMouseDown(true);
		BeginModal(key);
		if (size)
		{
			SetSize(*size);
		}
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		BeginText("title", title, Gui::View::TextFlags::none);
		SetSize(21);
		SetTextPadding(7);
		EndText();
		Separator("dialogbeginsep");
		BeginVPanel("content");
		SetPadding(6);
		SetSpacing(4);
	}

	bool View::EndDialog(bool allowOk)
	{
		bool tryOk = false;
		EndPanel();
		Separator("dialogendsep");
		BeginHPanel("dialogend");
		{
			SetSize(27);
			SetPadding(6);
			SetSpacing(4);
			if (Button("Cancel", 50)) // TODO-REDO_UI-TRANSLATE
			{
				Exit();
			}
			if (Button("\boOK", SpanAll{}, ButtonFlags::none)) // TODO-REDO_UI-TRANSLATE
			{
				SetEnabled(allowOk);
				if (allowOk)
				{
					tryOk = true;
					Exit();
				}
			}
		}
		EndPanel();
		EndModal();
		return tryOk;
	}
}
