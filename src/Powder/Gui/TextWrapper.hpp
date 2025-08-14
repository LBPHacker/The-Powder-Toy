#pragma once
#include "Common/Point.hpp"
#include "Common/Utf8.hpp"
#include "Alignment.hpp"
#include "Text.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Powder::Gui
{
	class Host;

	class TextWrapper
	{
	public:
		using PixelCount = int32_t;
		using ClearIndex = int32_t;
		using ClearCount = int32_t;
		using CharIndex  = int32_t;
		using CharCount  = int32_t;
		using RawIndex   = int32_t;
		using RawCount   = int32_t;
		using LineIndex  = int32_t;
		using LineCount  = int32_t;
		struct PointAndLine
		{
			Point pos;
			LineIndex line;
		};
		struct Index
		{
			RawIndex raw;
			ClearIndex clear;
		};

	private:
		struct Region
		{
			bool material;
			LineIndex line;
			PixelCount left;
			PixelCount width;
			RawIndex raw;
		};
		std::vector<Region> regions;
		LineCount wrappedLines = 0;
		PixelCount wrappedWidth = 0;
		struct LeftAndLine
		{
			PixelCount left;
			LineIndex line;
		};
		LeftAndLine posAfterLast{ 0, 0 };
		RawCount rawSize = 0;

	public:
		void Update(Host &host, ShapeTextParameters stp);

		Index ConvertPointToIndex(Point point) const;
		PointAndLine ConvertClearToPointAndLine(ClearIndex clearIndex) const;
		Index ConvertClearToIndex(ClearIndex clearIndex) const;

		Point ConvertClearToPoint(ClearIndex clearIndex) const
		{
			return ConvertClearToPointAndLine(clearIndex).pos;
		}

		TextWrapper::LineCount GetWrappedLines() const
		{
			return wrappedLines;
		}

		TextWrapper::Index GetIndexBegin() const
		{
			return { 0, 0 };
		}

		TextWrapper::Index GetIndexEnd() const
		{
			return { rawSize, ClearIndex(regions.size()) };
		}

		Point GetWrappedSize() const
		{
			return { wrappedWidth, wrappedLines * fontTypeSize };
		}

		Point GetContentSize() const
		{
			return GetWrappedSize() - Point(letterSpacing, fontTypeSize - fontCapHeight);
		}

		const std::vector<Region> &GetRegions() const
		{
			return regions;
		}
	};
}
