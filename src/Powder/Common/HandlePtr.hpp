#pragma once
#include <memory>

namespace Powder
{
	template<class Handle, Handle Empty>
	struct HandlePtrWrapper
	{
		Handle handle;

		HandlePtrWrapper(Handle newHandle = Empty) : handle(newHandle)
		{
		}

		HandlePtrWrapper(std::nullptr_t) : HandlePtrWrapper()
		{
		}

		bool operator ==(const HandlePtrWrapper &other) const
		{
			return handle == other.handle;
		}

		bool operator !=(const HandlePtrWrapper &other) const
		{
			return !(*this == other);
		}

		bool operator ==(std::nullptr_t) const
		{
			return !bool(*this);
		}

		bool operator !=(std::nullptr_t) const
		{
			return !(*this == nullptr);
		}

		explicit operator bool() const
		{
			return handle != Empty;
		}

		operator Handle() const
		{
			return handle;
		}
	};

	template<class Wrapper, auto Free>
	struct HandlePtrDeleter
	{
		using pointer = Wrapper;

		void operator ()(Wrapper wrapper)
		{
			Free(wrapper.handle);
		}
	};

	template<class Handle, auto Free, Handle Empty = Handle(0)>
	using HandlePtr = std::unique_ptr<HandlePtrWrapper<Handle, Empty>, HandlePtrDeleter<HandlePtrWrapper<Handle, Empty>, Free>>;
}
