#pragma once
#include "Clipboard.h"

namespace Clipboard
{
	class ClipboardImpl
	{
	public:
		virtual ~ClipboardImpl() = default;

		virtual void SetClipboardData() = 0;
		virtual void GetClipboardData() = 0;
	};

	extern std::unique_ptr<GameSave> clipboardData;
}
