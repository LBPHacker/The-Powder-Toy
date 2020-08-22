#pragma once

#include "common/String.h"
#include "Point.h"

#include <vector>

namespace gui
{
	class TextWrapper
	{
	public:
		struct Index
		{
			// * Index of the character this Index corresponds to in the String
			//   S passed to Update, let its size be #S.
			// * Usually goes from 0 to #S - 1, but is #S in IndexEnd() and -1
			//   if the Index doesn't correspond to a character in S (e.g. it's
			//   a newline inserted by the wrapper).
			int rawIndex;
			// * Index of the character this Index corresponds to in the String
			//   W returned by WrappedText, let its size be #W.
			// * Usually goes from 0 to #W - 1, but is #W in IndexEnd().
			int wrappedIndex;
			// * Index of the character this Index corresponds to in the String
			//   C passed to Update with formatting removed, let its size be #C.
			// * Usually goes from 0 to #C - 1, but is #C in IndexEnd() and -1
			//   if the Index doesn't correspond to a character in C.
			int clearIndex;
		};

	private:
		int rawTextSize = 0;
		int clearTextSize = 0;
		String wrappedText;
		struct ClickmapRegion
		{
			int width;
			Point pos0, pos1;
			int line0, line1;
			Index index;
		};
		int wrappedLines = 1;
		std::vector<ClickmapRegion> regions;

	public:
		int Update(const String &text, bool doWrapping, int maxWidth);
		Index Clear2Index(int clearIndex) const;
		Index Point2Index(Point point) const;
		struct PointLine
		{
			Point point;
			int line;
		};
		PointLine Index2PointLine(Index index) const;

		const String &WrappedText() const
		{
			return wrappedText;
		}

		int WrappedLines() const
		{
			return wrappedLines;
		}

		Index IndexBegin() const
		{
			return Index{ 0, 0, 0 };
		}

		Index IndexEnd() const
		{
			return Index{ rawTextSize, int(wrappedText.size()), clearTextSize };
		}

		static bool WordBreak(int ch);
	};
}
