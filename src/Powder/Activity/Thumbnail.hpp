#pragma once
#include <memory>
#include <variant>
#include "common/String.h"

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	struct ThumbnailLoading
	{
	};
	struct ThumbnailData
	{
		std::unique_ptr<Gui::StaticTexture> originalTexture;
		std::unique_ptr<Gui::StaticTexture> smallTexture;
	};
	struct ThumbnailError
	{
		ByteString message;
	};
	using Thumbnail = std::variant<
		ThumbnailLoading,
		ThumbnailData,
		ThumbnailError
	>;
}
