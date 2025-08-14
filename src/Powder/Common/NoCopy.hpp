#pragma once

namespace Powder
{
	class NoCopy
	{
	public:
		NoCopy() = default;
		NoCopy(const NoCopy &) = delete;
		NoCopy &operator =(const NoCopy &) = delete;
	};
}
