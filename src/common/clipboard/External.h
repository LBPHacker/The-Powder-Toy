#pragma once
#include "Detail.h"

namespace Clipboard
{
	class ExternalClipboardImpl : public ClipboardImpl
	{
	public:
		void SetClipboardData() final override;
		void GetClipboardData() final override;
	};
}
