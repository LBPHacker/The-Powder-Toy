#pragma once
#include "Assert.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#define DebugCrossFrameItemStore 0

namespace Powder
{
	struct CrossFrameItem
	{
		using Index = int32_t;

		template<class DerivedItem>
		struct DerivedIndex
		{
			Index value;

			auto operator <=>(const DerivedIndex &) const = default;
		};

		std::optional<Index> nextSameStateIndex;
		enum class State
		{
			used,
			unused,
			free,
		};
		State state = State::unused;
	};

	template<class Derived, class DerivedItem, CrossFrameItem::Index maxItems = 100000>
	class CrossFrameItemStore
	{
		std::vector<DerivedItem> items;

		std::optional<CrossFrameItem::Index> firstUsedOrUnusedIndex;
		std::optional<CrossFrameItem::Index> firstFreeIndex;

	public:
#if DebugCrossFrameItemStore
		CrossFrameItem::Index debugAllocCount = 0;
		CrossFrameItem::Index debugFreeCount = 0;
		CrossFrameItem::Index debugAliveCount = 0;
		CrossFrameItem::Index debugItemsSize = 0;
#endif

		using DerivedItemIndex = CrossFrameItem::DerivedIndex<DerivedItem>;

		CrossFrameItem::Index Alloc()
		{
#if DebugCrossFrameItemStore
			debugAllocCount += 1;
#endif
			CrossFrameItem::Index itemIndex;
			if (firstFreeIndex)
			{
				itemIndex = *firstFreeIndex;
				auto &item = items[itemIndex];
				firstFreeIndex = item.nextSameStateIndex;
				item = {};
			}
			else
			{
				Assert(items.size() <= maxItems);
				itemIndex = CrossFrameItem::Index(items.size());
				items.emplace_back();
			}
			items[itemIndex].nextSameStateIndex = firstUsedOrUnusedIndex;
			firstUsedOrUnusedIndex = itemIndex;
			return itemIndex;
		}

		void Free(CrossFrameItem::Index)
		{
		}

		void BeginFrame()
		{
#if DebugCrossFrameItemStore
			debugAllocCount = 0;
			debugFreeCount = 0;
			debugAliveCount = 0;
#endif
		}

		bool EndFrame()
		{
			bool removedSome = false;
			auto itemIndex = firstUsedOrUnusedIndex;
			auto *nextUsedOrUnusedIndex = &firstUsedOrUnusedIndex;
			while (itemIndex)
			{
				nextUsedOrUnusedIndex->reset();
				auto &item = items[*itemIndex];
				auto newItemIndex = item.nextSameStateIndex;
				if (item.state == CrossFrameItem::State::unused)
				{
					removedSome = true;
					static_cast<Derived &>(*this).Free(*itemIndex);
					item.state = CrossFrameItem::State::free;
					item.nextSameStateIndex = firstFreeIndex;
					firstFreeIndex = itemIndex;
#if DebugCrossFrameItemStore
					debugFreeCount += 1;
#endif
				}
				else
				{
					item.state = CrossFrameItem::State::unused;
					*nextUsedOrUnusedIndex = itemIndex;
					nextUsedOrUnusedIndex = &item.nextSameStateIndex;
#if DebugCrossFrameItemStore
					debugAliveCount += 1;
#endif
				}
				itemIndex = newItemIndex;
			}
#if DebugCrossFrameItemStore
			debugItemsSize = int32_t(items.size());
#endif
			return removedSome;
		}

		DerivedItem &operator [](DerivedItemIndex index)
		{
			return items[index.value];
		}

		const DerivedItem &operator [](DerivedItemIndex index) const
		{
			return items[index.value];
		}

		struct UsedOrUnusedIterator
		{
			CrossFrameItemStore &store;
			std::optional<CrossFrameItem::Index> index;

			bool operator !=(const UsedOrUnusedIterator &other) const
			{
				return index != other.index;
			}

			DerivedItem &operator *()
			{
				return store.items[*index];
			}

			UsedOrUnusedIterator &operator ++()
			{
				index = store.items[*index].nextSameStateIndex;
				return *this;
			}
		};
		UsedOrUnusedIterator begin()
		{
			return { *this, firstUsedOrUnusedIndex };
		}
		UsedOrUnusedIterator end()
		{
			return { *this, std::nullopt };
		}

		CrossFrameItem::Index GetItemIndex(const DerivedItem &item) const
		{
			return &item - items.data();
		}
	};
}
