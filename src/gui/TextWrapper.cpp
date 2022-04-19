#include "TextWrapper.h"

#include "SDLWindow.h"
#include "graphics/FontData.h"

#include <algorithm>
#include <vector>
#include <iterator>
#include <iostream>

namespace gui
{
	bool TextWrapper::WordBreak(int ch)
	{
		switch (ch)
		{
		// * Add more supported characters here that break words.
		case '\n':
		case ' ':
		case '?':
		case ';':
		case ',':
		case ':':
		case '.':
		case '-':
		case '!':
			return true;
		}
		return false;
	}

	int TextWrapper::Update(const String &text, bool doWrapping, int maxWidth, int initialInset)
	{
		rawTextSize = int(text.size());

		struct WrapRecord
		{
			String::value_type character;
			int width;
			int rawIndex;
			int clearIndex;
			bool wraps;
			bool mayEatSpace;
		};
		int lineWidth = initialInset;
		std::vector<WrapRecord> records;

		int wordBeginsAt = -1; // * This is a pointer into records; we're not currently in a word.
		int wordWidth = 0;
		int lines = 1;
		int charWidth = 0;
		int clearCount = 0;

		for (auto i = 0; i < initialInset; i += 127)
		{
			// * See gui::OffsetString(int offset).
			auto offset = Min(initialInset - i, 127);
			auto offsetChar = String::value_type(0x100 | offset);
			records.push_back(WrapRecord{
				'\b',  // * Character; makes the line wrap when rendered.
				0,     // * Width; fools the clickmap generator into not seeing this newline.
				-1,    // * Position; the clickmap generator is fooled, this can be anything.
				-1,    // * No corresponding character in the unformatted input string.
				false, // * Signal nothing to the clickmap generator.
				false  // * Don't allow record to eat subsequent space.
			});
			records.push_back(WrapRecord{
				offsetChar, // * Character; makes the line wrap when rendered.
				offset,     // * Width; fools the clickmap generator into not seeing this newline.
				-1,         // * Position; the clickmap generator is fooled, this can be anything.
				-1,         // * No corresponding character in the unformatted input string.
				false,      // * Signal nothing to the clickmap generator.
				false       // * Don't allow record to eat subsequent space.
			});
		}

		auto wrapIfNeeded = [doWrapping, maxWidth, &records, &lineWidth, &lines, &charWidth](int widthToConsider) -> bool {
			if (doWrapping && widthToConsider + charWidth > maxWidth)
			{
				records.push_back(WrapRecord{
					'\n', // * Character; makes the line wrap when rendered.
					0,    // * Width; fools the clickmap generator into not seeing this newline.
					-1,   // * Position; the clickmap generator is fooled, this can be anything.
					-1,   // * No corresponding character in the unformatted input string.
					true, // * Signal the end of the line to the clickmap generator.
					true, // * Allow record to eat the subsequent space.
				});
				lineWidth = 0;
				lines += 1;
				return true;
			}
			return false;
		};

		for (auto it = text.begin(); it != text.end(); ++it)
		{
			charWidth = SDLWindow::Ref().CharWidth(*it);

			int sequenceLength = 0;
			// * Set sequenceLength if *it starts a sequence that should be forwarded as-is.
			switch (*it)
			{
			case   '\b': sequenceLength = 2; break;
			// * TODO-REDO_UI: Support monospace (0xA?) and skips (0xB?).
			case '\x0F': sequenceLength = 4; break;
			}
			
			switch (*it)
			{
			// * Add more supported spaces here.
			case ' ':
				wrapIfNeeded(lineWidth);
				if (records.size() && records.back().mayEatSpace)
				{
					records.back().mayEatSpace = false;
				}
				else
				{
					// * This is pushed only if the previous record isn't a wrapping one
					//   to make spaces immediately following newline characters inserted
					//   by the wrapper disappear.
					records.push_back(WrapRecord{
						*it,                    // * Character; make the line wrap with *it.
						charWidth,              // * Width; make it span all the way to the end.
						int(it - text.begin()), // * Position; so the clickmap generator knows where *it is.
						clearCount,             // * Corresponding character in the unformatted input string.
						false,                  // * Signal nothing to the clickmap generator.
						false                   // * Don't allow record to eat subsequent space.
					});
					lineWidth += charWidth;
				}
				wordBeginsAt = -1; // * Reset word state.
				clearCount += 1;
				break;

			// * Add more supported linebreaks here.
			case '\n':
				records.push_back(WrapRecord{
					*it,                    // * Character; makes the line wrap when rendered.
					maxWidth - lineWidth,   // * Width; make it span all the way to the end.
					int(it - text.begin()), // * Position; so the clickmap generator knows where *it is.
					clearCount,             // * Corresponding character in the unformatted input string.
					true,                   // * Signal the end of the line to the clickmap generator.
					false                   // * Don't allow record to eat subsequent space.
				});
				lines += 1;
				lineWidth = 0;
				wordBeginsAt = -1; // * Reset word state.
				clearCount += 1;
				break;

			default:
				if (sequenceLength) // *it starts a sequence such as \b? or \x0f???.
				{
					if (text.end() - it < sequenceLength)
					{
						it = text.end() - 1; // * Move it such that the ++it in the outer loop will trigger the exit condition.
						continue; // * Text is broken, we might as well skip the whole thing.
					}
					for (auto skip = it + sequenceLength; it != skip; ++it)
					{
						records.push_back(WrapRecord{
							*it,   // * Character; forward the sequence to the output.
							0,     // * Width; fools the clickmap generator into not seeing this sequence.
							-1,    // * Position; the clickmap generator is fooled, this can be anything.
							-1,    // * No corresponding character in the unformatted input string.
							false, // * Signal nothing to the clickmap generator.
							false  // * Don't allow record to eat subsequent space.
						});
					}
					--it; // * Needed because the previous loop moved it beyond the sequence, while that's the outer loop's job.
				}
				else
				{
					if (wordBeginsAt == -1)
					{
						wordBeginsAt = records.size();
						wordWidth = 0;
					}

					if (wrapIfNeeded(wordWidth))
					{
						wordBeginsAt = records.size();
						wordWidth = 0;
					}
					auto oldRecordsSize = int(records.size());
					if (wrapIfNeeded(lineWidth))
					{
						// * If we get in here, we skipped the previous block (since lineWidth
						//   would have been set to 0 (unless of course (charWidth > maxWidth) which
						//   is dumb)). Since (wordWidth + charWidth) <= (lineWidth + charWidth) always
						//   holds and we are in this block, we can be sure that wordWidth < lineWidth,
						//   so breaking the line by the preceding space is sure to decrease lineWidth.
						// * Now of course there's this problem that wrapIfNeeded appends the
						//   newline character (and possibly other things) to the end of records, and we
						//   want it before the record at position wordBeginsAt (0-based).
						auto rotateSize = int(records.size()) - oldRecordsSize;
						std::rotate(
							records.begin() + wordBeginsAt,
							records.end() - rotateSize,
							records.end()
						);
						wordBeginsAt += rotateSize;
						lineWidth = wordWidth;
					}

					records.push_back(WrapRecord{
						*it,                    // * Character; make the line wrap with *it.
						charWidth,              // * Width; make it span all the way to the end.
						int(it - text.begin()), // * Position; so the clickmap generator knows where *it is.
						clearCount,             // * Corresponding character in the unformatted input string.
						false,                  // * Signal nothing to the clickmap generator.
						false                   // * Don't allow record to eat subsequent space.
					});
					wordWidth += charWidth;
					lineWidth += charWidth;
					clearCount += 1;

					if (WordBreak(*it))
					{
						wordBeginsAt = -1; // * Reset word state.
						break;
					}
				}
				break;
			}
		}

		regions.clear();
		wrappedText.clear();
		int x = 0;
		int l = 0;
		for (const auto &record : records)
		{
			ClickmapRegion cmr;
			cmr.width = record.width;
			cmr.pos0 = { x, l * FONT_H };
			cmr.line0 = l;
			x += record.width;
			if (record.wraps)
			{
				x = 0;
				l += 1;
			}
			cmr.pos1 = { x, l * FONT_H };
			cmr.line1 = l;
			cmr.index = Index{ record.rawIndex, int(wrappedText.size()), record.clearIndex };
			regions.push_back(cmr);
			wrappedText.append(1, record.character);
		}

		clearTextSize = clearCount;
		wrappedLines = lines;
		return lines;
	}

	TextWrapper::Index TextWrapper::Point2Index(Point point) const
	{
		auto x = point.x;
		auto y = point.y;
		if (y < 0)
		{
			return IndexBegin();
		}
		if (regions.size())
		{
			auto curr = regions.begin();
			auto end = regions.end();

			auto findNextNonempty = [end](decltype(end) it) {
				++it;
				while (it != end && !it->width)
				{
					++it;
				}
				return it;
			};

			while (true)
			{
				if (curr->pos0.y + FONT_H > y)
				{
					if (curr->pos0.x + curr->width / 2 > x)
					{
						// * If x is to the left of the vertical bisector of the
						//   current region, return this one; really we should
						//   have returned 'the next one' in the previous
						//   iteration.
						return curr->index;
					}
				}
				auto next = findNextNonempty(curr);
				if (next == end)
				{
					break;
				}
				if (curr->pos0.y + FONT_H > y)
				{
					if (curr->pos0.x + curr->width / 2 <= x && next->pos0.x + next->width / 2 > x)
					{
						// * If x is to the right of the vertical bisector of
						//   the current region but to the left of the next
						//   one's, return the next one.
						return next->index;
					}
					if (curr->pos0.x + curr->width / 2 <= x && next->pos0.y > curr->pos0.y)
					{
						// * Nominate the next region if x is to the right of
						//   the vertical bisector of the current region and the
						//   next one is on a new line.
						return next->index;
					}
				}
				curr = next;
			}
		}
		return IndexEnd();
	}

	TextWrapper::PointLine TextWrapper::Index2PointLine(Index index) const
	{
		if (index.wrappedIndex < 0 || index.wrappedIndex > int(regions.size()) || !regions.size())
		{
			return PointLine{ { 0, 0 }, 0 };
		}
		if (index.wrappedIndex == int(regions.size()))
		{
			auto &cmr = regions[index.wrappedIndex - 1];
			return PointLine{ cmr.pos1, cmr.line1 };
		}
		auto &cmr = regions[index.wrappedIndex];
		return PointLine{ cmr.pos0, cmr.line0 };
	}

	TextWrapper::Index TextWrapper::Clear2Index(int clearIndex) const
	{
		if (clearIndex < 0)
		{
			return IndexBegin();
		}
		for (const auto &region : regions)
		{
			if (region.index.clearIndex >= clearIndex)
			{
				return region.index;
			}
		}
		return IndexEnd();
	}
}
