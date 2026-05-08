#pragma once
#include <new>

template<class Item, std::align_val_t Alignment = std::align_val_t(64)>
struct AlignedAllocator
{
	typedef Item value_type;

	AlignedAllocator() = default;

	template<class OtherItem>
	constexpr AlignedAllocator(const AlignedAllocator<OtherItem, Alignment> &) noexcept
	{
	}

	template<class OtherItem>
	struct rebind
	{
		using other = AlignedAllocator<OtherItem, Alignment>;
	};

	[[nodiscard]] Item *allocate(std::size_t n)
	{
		return reinterpret_cast<Item *>(::operator new[](n * sizeof(Item), Alignment));
	}

	void deallocate(Item *p, std::size_t n) noexcept
	{
		::operator delete[](p, Alignment);
	}
};

template<class Item, std::align_val_t Alignment>
bool operator ==(const AlignedAllocator<Item, Alignment> &, const AlignedAllocator<Item, Alignment> &) { return true; }

template<class Item, std::align_val_t Alignment>
bool operator !=(const AlignedAllocator<Item, Alignment> &, const AlignedAllocator<Item, Alignment> &) { return false; }
