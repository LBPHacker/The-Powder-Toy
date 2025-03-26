#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr int32_t maxInternedItemSize  = 1000;
		constexpr int32_t maxInternedTotalSize = 100'000;
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

#if DebugGuiView
	void View::ComponentStore::DumpKey(ComponentKeyStoreSpan interned) const
	{
		ByteStringBuilder sb;
		for (auto ch : DereferenceSpan(interned))
		{
			if (ch >= 0x20 && ch <= 0x7F)
			{
				sb << ch;
			}
			else
			{
				sb << Format::Hex() << Format::Width(2) << Format::Fill('0') << uint32_t(uint8_t(ch));
			}
		}
		Log("dumpkey ", sb.Build());
	}
#endif

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
				shouldUpdateLayout = true;
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
		shouldUpdateLayout = true;
		auto componentIndex = componentStore.Alloc();
#if DebugGuiView
		componentStore[componentIndex].generation = componentGeneration;
		componentGeneration += 1;
#endif
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
			component.cursor = std::nullopt;
			component.handleButtons = false;
			component.handleWheel = false;
			component.handleInput = false;
			component.forwardInputFocusIndex.reset();
#if DebugGuiView
			component.debugMe = false;
#endif
			std::optional<ComponentIndex> expectedNextSiblingIndex = componentIndex;
			if (prevSiblingIndex)
			{
				std::swap(componentStore[*prevSiblingIndex].nextSiblingIndex, expectedNextSiblingIndex);
				component.layout.spacingFromParent = componentStore[componentStack.back().index].spacing + extraSpacing;
			}
			else
			{
				std::swap((componentStack.empty() ? rootIndex : componentStore[componentStack.back().index].firstChildIndex), expectedNextSiblingIndex);
			}
			if (expectedNextSiblingIndex != componentIndex)
			{
				for (auto childIndex = expectedNextSiblingIndex; childIndex; )
				{
#if DebugGuiView
					debugRelinkIterationCount += 1;
#endif
					auto &nextSiblingIndex = componentStore[*childIndex].nextSiblingIndex;
					if (nextSiblingIndex == componentIndex)
					{
						nextSiblingIndex = component.nextSiblingIndex;
						break;
					}
					childIndex = nextSiblingIndex;
				}
				component.nextSiblingIndex = expectedNextSiblingIndex;
			}
			extraSpacing = 0;
		}
		prevSiblingIndex.reset();
		auto clipRect = component.rect;
		SDL_Texture *prevRenderTarget = nullptr;
		if (!componentStack.empty())
		{
			auto &item = componentStack.back();
			clipRect &= item.clipRect;
			prevRenderTarget = item.prevRenderTarget;
		}
		if (component.prevLayout.rootRect)
		{
			auto &g = GetHost();
			auto *sdlRenderer = g.GetRenderer();
			auto windowSize = g.GetSize();
			clipRect = *component.prevLayout.rootRect;
			while (true)
			{
				auto layoutRootTextureIndex = int32_t(layoutRootTextures.size());
				if (nextLayoutRootTextureIndex < layoutRootTextureIndex)
				{
					break;
				}
				SDL_Texture *texture = nullptr;
				if (layoutRootTextureIndex != 0)
				{
					texture = SdlAssertPtr(SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, windowSize.X, windowSize.Y));
					SdlAssertZero(SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND));
				}
				layoutRootTextures.push_back(texture);
			}
			prevRenderTarget = SDL_GetRenderTarget(sdlRenderer);
			if (nextLayoutRootTextureIndex != 0)
			{
				SdlAssertZero(SDL_SetRenderTarget(sdlRenderer, layoutRootTextures[nextLayoutRootTextureIndex]));
				SdlAssertZero(SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, 0));
				SdlAssertZero(SDL_RenderClear(sdlRenderer));
			}
			nextLayoutRootTextureIndex += 1;
		}
		componentStack.push_back({ componentIndex, clipRect, prevRenderTarget });
		return component;
	}

	void View::ShrinkLayoutRootTextures(int32_t newSize)
	{
		auto oldSize = int32_t(layoutRootTextures.size());
		newSize = std::min(newSize, oldSize);
		for (auto i = newSize; i < oldSize; ++i)
		{
			if (layoutRootTextures[i])
			{
				SDL_DestroyTexture(layoutRootTextures[i]);
			}
		}
		layoutRootTextures.resize(newSize);
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

	void View::BeginComponent(ComponentKey key)
	{
		// TODO-REDO_UI: limit depth
		GetOrAllocComponent(key);
		auto &g = GetHost();
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
		if (componentStore[*prevSiblingIndex].prevLayout.rootRect)
		{
			SdlAssertZero(SDL_SetRenderTarget(GetHost().GetRenderer(), componentStack.back().prevRenderTarget));
		}
		if (componentStore[*prevSiblingIndex].layout.rootRect)
		{
			layoutRootIndices.push_back(*prevSiblingIndex);
		}
		componentStack.pop_back();
		if (!componentStack.empty())
		{
			auto &g = GetHost();
			auto &clipRect = componentStack.back().clipRect;
			g.SetClipRect(clipRect);
		}
	}
}
