#pragma once
#include "Common/CrossFrameItemStore.hpp"
#include "Alignment.hpp"
#include <cstdint>
#include <optional>

namespace Powder::Gui
{
	struct InternedText;
	using InternedTextIndex = CrossFrameItem::DerivedIndex<InternedText>;

	struct ShapedText;
	using ShapedTextIndex = CrossFrameItem::DerivedIndex<ShapedText>;

	struct ShapeTextParameters
	{
		InternedTextIndex text;
		struct MaxWidth
		{
			int32_t width;
			bool fill;

			bool operator ==(const MaxWidth &) const = default;
		};
		std::optional<MaxWidth> maxWidth;
		Alignment alignment = Alignment::center;

		bool operator ==(const ShapeTextParameters &) const = default;
	};

	constexpr int32_t fontTypeSize  = 12;
	constexpr int32_t fontCapHeight = 7;
	constexpr int32_t fontCapLine   = 2;
	constexpr int32_t letterSpacing = 1;
}
