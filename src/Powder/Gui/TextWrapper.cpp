#include "TextWrapper.hpp"
#include "Host.hpp"
#include "Gui/Icons.hpp"
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
		struct Overflow
		{
			std::string_view text;
			PixelCount textWidth = 0;
		};
		std::optional<Overflow> overflow;
		if (stp.maxWidth)
		{
			stp.maxWidth->width = std::clamp(stp.maxWidth->width, minMaxWidth, maxMaxWidth);
		}
		auto effectiveMaxWidth = stp.maxWidth ? stp.maxWidth->width : maxMaxWidth;
		if (stp.overflowText && stp.maxWidth)
		{
			auto probeStp = stp;
			probeStp.overflowText = std::nullopt;
			Update(host, probeStp);
			if (wrappedLines < 2)
			{
				return;
			}
			overflow = { host.GetInternedText(*stp.overflowText) };
			for (auto ch : Utf8Range{ overflow->text })
			{
				if (!ch.valid)
				{
					continue;
				}
				auto charWidth = host.CharWidth(ch.codePoint);
				if (overflow->textWidth + charWidth > effectiveMaxWidth)
				{
					break;
				}
				overflow->textWidth += charWidth;
			}
		}
		enum class State
		{
			none,
			word,
			space,
		};
		State state = State::none;
		bool pastFirstWordStart = false;
		PixelCount indent = std::min(stp.firstLineIndent, effectiveMaxWidth);
		PixelCount lineWidth = indent;
		wrappedLines = 0;
		wrappedWidth = 0;
		auto range = Utf8Range{ text };
		auto it = range.begin();
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
			indent = 0;
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
				if (indent + *wordWidth + charWidth > effectiveMaxWidth)
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
					RawIndex(it - range.begin()),
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
					RawIndex(it - range.begin()),
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
		while (it != range.end() && charIndex < maxCharacters && wrappedLines < maxWrappedLines)
		{
			ch = *it;
			if (!ch.valid)
			{
				++it;
				charIndex += 1;
				continue;
			}
			if (ch.codePoint == '\x11')
			{
				++it;
				if (it == range.end())
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
				else
				{
					lineWidth = std::clamp(lineWidth + delta, 0, effectiveMaxWidth);
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
			case '\x12': sequenceLength = 1; break;
			}
			if (sequenceLength)
			{
				bool bad = false;
				for (CharCount i = 0; i < *sequenceLength; ++i)
				{
					if (it == range.end())
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
			if (stp.replaceWithDots)
			{
				ch.codePoint = Gui::iconBullet;
			}
			charWidth = host.CharWidth(ch.codePoint);
			if (charWidth > effectiveMaxWidth)
			{
				break;
			}
			if (overflow && charWidth + lineWidth + overflow->textWidth > effectiveMaxWidth)
			{
				endSpace();
				endWord();
				for (auto och : Utf8Range{ overflow->text })
				{
					if (!och.valid)
					{
						continue;
					}
					auto charWidth = host.CharWidth(och.codePoint);
					overflowRegions.push_back({
						och.codePoint,
						wrappedLines,
						lineWidth,
						charWidth,
					});
					lineWidth += charWidth;
				}
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
				// TODO-REDO_UI: attach character to the previous word somehow
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
					if (overflow)
					{
						commitSpace();
						endSpace();
					}
					break;
				}
				[[fallthrough]];

			default:
				pastFirstWordStart = true;
				endSpace();
				beginWord();
				consume();
				if (overflow)
				{
					endWord();
				}
				break;
			}
		}
		endLine();
		rawSize = RawIndex(it - range.begin());
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
		PixelCount overflowAlignOffset = 0;
		auto align = [&]() {
			auto alignWidth = 0;
			if (firstMaterial)
			{
				alignWidth = lastMaterial->left + lastMaterial->width - firstMaterial->left;
			}
			if (overflowRegions.size())
			{
				auto &back = overflowRegions.back();
				alignWidth = back.left + back.width - firstMaterial->left;
			}
			auto alignOffset = PixelCount(stp.alignment) * (wrappedWidth - alignWidth) / 2;
			overflowAlignOffset = alignOffset;
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
		for (auto &overflowRegion : overflowRegions)
		{
			overflowRegion.left += overflowAlignOffset;
		}
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
