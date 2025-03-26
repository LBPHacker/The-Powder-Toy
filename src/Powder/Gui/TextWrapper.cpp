#include "TextWrapper.hpp"
#include "Host.hpp"
#include <algorithm>
#include <iterator>

namespace Powder::Gui
{
	namespace
	{
		constexpr TextWrapper::CharCount maxCharacters   = 100'000;
		constexpr TextWrapper::LineCount maxWrappedLines = 1000;
		constexpr TextWrapper::CharCount minMaxWidth     = 0;
		constexpr TextWrapper::CharCount maxMaxWidth     = 10'000;
	}

	void TextWrapper::Update(Host &host, ShapeTextParameters stp)
	{
		auto text = host.GetInternedText(stp.text);
		if (stp.maxWidth)
		{
			stp.maxWidth->width = std::clamp(stp.maxWidth->width, minMaxWidth, maxMaxWidth);
		}
		auto effectiveMaxWidth = stp.maxWidth ? stp.maxWidth->width : maxMaxWidth;
		enum class State
		{
			none,
			word,
			space,
		};
		State state = State::none;
		bool pastFirstWordStart = false;
		PixelCount lineWidth = 0;
		wrappedLines = 0;
		wrappedWidth = 0;
		auto it = Utf8Begin(text);
		auto end = Utf8End(text);
		std::optional<PixelCount> spaceWidth;
		std::optional<PixelCount> wordWidth;
		ClearIndex wordBeginsAt = 0;
		ClearIndex spaceBeginsAt = 0;
		auto beginSpace = [&]() {
			if (state != State::space)
			{
				state = State::space;
				spaceBeginsAt = ClearIndex(regions.size());
				spaceWidth = 0;
			}
		};
		auto endSpace = [&]() {
			if (state == State::space)
			{
				state = State::none;
			}
		};
		auto beginWord = [&]() {
			if (state != State::word)
			{
				state = State::word;
				wordBeginsAt = ClearIndex(regions.size());
				wordWidth = 0;
			}
		};
		auto commitSpace = [&]() {
			if (spaceWidth)
			{
				lineWidth += *spaceWidth;
				spaceWidth.reset();
			}
		};
		auto endWord = [&]() {
			if (state == State::word)
			{
				commitSpace();
				lineWidth += *wordWidth;
				wordWidth.reset();
				state = State::none;
			}
		};
		PixelCount lastLineWidth = 0;
		auto wrap = [&]() {
			commitSpace();
			wrappedWidth = std::max(wrappedWidth, lineWidth);
			lastLineWidth = lineWidth;
			lineWidth = 0;
			wrappedLines += 1;
		};
		auto endLine = [&]() {
			endSpace();
			endWord();
			pastFirstWordStart = false;
			wrap();
		};
		Utf8Iterator::Character ch;
		PixelCount charWidth = 0;
		regions.clear();
		CharIndex charIndex = 0;
		auto consume = [&]() {
			if (state == State::word)
			{
				if (lineWidth + *wordWidth + charWidth > effectiveMaxWidth)
				{
					endWord();
					wrap();
					beginWord();
				}
				else
				{
					auto effectiveLineWidth = lineWidth + (spaceWidth ? *spaceWidth : PixelCount(0));
					if (*wordWidth + effectiveLineWidth + charWidth > effectiveMaxWidth)
					{
						if (spaceWidth)
						{
							for (auto i = spaceBeginsAt; i < wordBeginsAt; ++i)
							{
								regions[i].material = false;
							}
						}
						for (auto i = wordBeginsAt; i < ClearIndex(regions.size()); ++i)
						{
							regions[i].left -= effectiveLineWidth;
							regions[i].line += 1;
						}
						wrap();
					}
				}
				auto left = std::clamp(lineWidth + *wordWidth + (spaceWidth ? *spaceWidth : PixelCount(0)), 0, effectiveMaxWidth);
				auto width = std::min(charWidth, effectiveMaxWidth - left);
				regions.push_back({
					true,
					wrappedLines,
					left,
					width,
					RawIndex(it.pos),
				});
			}
			if (state == State::space)
			{
				auto left = std::clamp(lineWidth + *spaceWidth, 0, effectiveMaxWidth);
				auto width = std::min(charWidth, effectiveMaxWidth - left);
				regions.push_back({
					true,
					wrappedLines,
					left,
					width,
					RawIndex(it.pos),
				});
			}
			++it;
			charIndex += 1;
			if (state == State::word)
			{
				*wordWidth += charWidth;
			}
			if (state == State::space)
			{
				*spaceWidth += charWidth;
			}
		};
		while (it != end && charIndex < maxCharacters && wrappedLines < maxWrappedLines)
		{
			ch = *it;
			if (ch.codePoint == '\x11')
			{
				++it;
				if (it == end)
				{
					break;
				}
				auto delta = int32_t((*it).codePoint);
				if (delta >= 0x80)
				{
					delta -= 0x100;
				}
				if (state == State::word)
				{
					*wordWidth = std::clamp(*wordWidth + delta, 0, effectiveMaxWidth);
				}
				++it;
				continue;
			}
			std::optional<CharCount> sequenceLength;
			switch (ch.codePoint)
			{
			case '\x01': sequenceLength = 1; break;
			case   '\b': sequenceLength = 2; break;
			case '\x0E': sequenceLength = 1; break;
			case '\x0F': sequenceLength = 4; break;
			case '\x10': sequenceLength = 3; break;
			}
			if (sequenceLength)
			{
				bool bad = false;
				for (CharCount i = 0; i < *sequenceLength; ++i)
				{
					if (it == end)
					{
						bad = true;
						break;
					}
					++it;
				}
				if (bad)
				{
					break;
				}
				continue;
			}
			charWidth = host.CharWidth(ch.codePoint);
			if (charWidth > effectiveMaxWidth)
			{
				break;
			}
			switch (ch.codePoint)
			{
			case '\n':
				endWord();
				beginSpace();
				consume();
				endLine();
				break;

			case '?':
			case ';':
			case ',':
			case ':':
			case '.':
			case '-':
			case '!':
				endSpace();
				endWord();
				beginWord();
				consume();
				endWord();
				break;

			case '\t':
			case ' ':
				if (pastFirstWordStart)
				{
					endWord();
					beginSpace();
					consume();
					break;
				}
				[[fallthrough]];

			default:
				pastFirstWordStart = true;
				endSpace();
				beginWord();
				consume();
				break;
			}
		}
		endLine();
		rawSize = RawIndex(it.pos);
		posAfterLast = { lastLineWidth, wrappedLines - 1 };
		if (stp.maxWidth && stp.maxWidth->fill)
		{
			wrappedWidth = stp.maxWidth->width;
		}
		LineIndex alignLine = 0;
		auto alignIt = regions.begin();
		auto alignEnd = regions.begin();
		Region *firstMaterial = nullptr;
		Region *lastMaterial = nullptr;
		auto align = [&]() {
			auto alignWidth = 0;
			if (firstMaterial)
			{
				alignWidth = lastMaterial->left + lastMaterial->width - firstMaterial->left;
			}
			auto alignOffset = PixelCount(stp.alignment) * (wrappedWidth - alignWidth) / 2;
			while (alignIt != alignEnd)
			{
				alignIt->left += alignOffset;
				++alignIt;
			}
			firstMaterial = nullptr;
			lastMaterial = nullptr;
		};
		while (alignEnd != regions.end())
		{
			if (alignEnd->line != alignLine)
			{
				align();
				alignLine = alignEnd->line;
			}
			if (alignEnd->material)
			{
				if (!firstMaterial)
				{
					firstMaterial = &*alignEnd;
				}
				lastMaterial = &*alignEnd;
			}
			++alignEnd;
		}
		align();
		posAfterLast.left += PixelCount(stp.alignment) * (wrappedWidth - posAfterLast.left) / 2;
	}

	TextWrapper::Index TextWrapper::ConvertPointToIndex(Point point) const
	{
		if (point.Y < 0)
		{
			return GetIndexBegin();
		}
		std::optional<Index> index;
		std::optional<PixelCount> prevWidth;
		for (ClearIndex i = 0; i < ClearIndex(regions.size()); ++i)
		{
			auto &region = regions[i];
			if (!( region.line      * fontTypeSize <= point.Y &&
			      (region.line + 1) * fontTypeSize >  point.Y))
			{
				continue;
			}
			if (!prevWidth || region.left - *prevWidth / 2 <= point.X)
			{
				index = { region.raw, i };
			}
			prevWidth = region.width;
		}
		if (index && regions[index->clear].line == posAfterLast.line && posAfterLast.left - *prevWidth / 2 < point.X)
		{
			index.reset();
		}
		return index ? *index : GetIndexEnd();
	}

	TextWrapper::PointAndLine TextWrapper::ConvertClearToPointAndLine(ClearIndex clearIndex) const
	{
		if (clearIndex < 0 || clearIndex >= ClearIndex(regions.size()))
		{
			return { { posAfterLast.left, posAfterLast.line * fontTypeSize }, posAfterLast.line };
		}
		return { { regions[clearIndex].left, regions[clearIndex].line * fontTypeSize }, regions[clearIndex].line };
	}

	TextWrapper::Index TextWrapper::ConvertClearToIndex(ClearIndex clearIndex) const
	{
		if (clearIndex < 0 || regions.empty())
		{
			return GetIndexBegin();
		}
		if (clearIndex >= ClearIndex(regions.size()))
		{
			return GetIndexEnd();
		}
		return { regions[clearIndex].raw, clearIndex };
	}
}
