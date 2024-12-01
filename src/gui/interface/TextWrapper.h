#pragma once

#include "Point.h"
#include "common/String.h"

#include <vector>

namespace ui
{
class TextWrapper
{
public:
	struct Index
	{
		int raw_index;
		int wrapped_index;
		int clear_index;
	};

private:
	int raw_text_size;
	int clear_text_size;
	String wrapped_text;

	struct clickmap_region
	{
		int pos_x, pos_y, width, pos_line;
		Index index;
	};

	int wrapped_lines;
	int wrappedWidth;
	std::vector<clickmap_region> regions;

public:
	int Update(const String &text, bool do_wrapping, int max_width);
	Index Clear2Index(int clear_index) const;
	Index Point2Index(int x, int y) const;
	int Index2Point(Index index, int &x, int &y) const;

	const String &WrappedText() const
	{
		return wrapped_text;
	}

	int WrappedLines() const
	{
		return wrapped_lines;
	}

	Index IndexBegin() const
	{
		return Index{ 0, 0, 0 };
	}

	Index IndexEnd() const
	{
		return Index{ raw_text_size, (int)wrapped_text.size(), clear_text_size };
	}

	int WrappedWidth() const
	{
		return wrappedWidth;
	}
};
}
