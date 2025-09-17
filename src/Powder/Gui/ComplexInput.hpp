#pragma once
#include "Colors.hpp"
#include "View.hpp"
#include <optional>
#include <string>

namespace Powder::Gui
{
	inline int32_t GetDigitCount(int32_t value)
	{
		auto digitCount = 0;
		while (value)
		{
			value /= 10;
			digitCount += 1;
		}
		return digitCount;
	}

	inline std::optional<int32_t> ParsePage(const std::string &str, int32_t pageCount)
	{
		try
		{
			auto newPage = ByteString(str).ToNumber<int32_t>() - 1;
			if (newPage >= 0 && newPage < pageCount)
			{
				return newPage;
			}
		}
		catch (const std::runtime_error &)
		{
		}
		return std::nullopt;
	}

	template<class Number>
	class NumberInputContext
	{
		std::optional<Number> number;
		std::string str;
		bool wantRead = true;
		bool wantParse = false;

	public:
		void ForceParse()
		{
			wantParse = true;
		}

		void ForceRead()
		{
			wantRead = true;
		}

		const std::optional<Number> &GetNumber() const
		{
			return number;
		}

		std::string &GetString()
		{
			return str;
		}

		template<class ReadWriteParseBuild>
		void BeginTextbox(View::ComponentKey key, ReadWriteParseBuild &&rwpb)
		{
			auto &view = rwpb.view;
			view.BeginTextbox(key, str, View::TextboxFlags::none, (number ? Gui::colorWhite : Gui::colorRed).WithAlpha(255));
		}

		template<class ReadWriteParseBuild>
		bool EndTextbox(ReadWriteParseBuild &&rwpb)
		{
			auto &view = rwpb.view;
			auto inputFocus = view.HasInputFocus();
			auto changed = view.EndTextbox();
			bool wantWrite = false;
			if (changed)
			{
				wantParse = true;
			}
			if (wantParse)
			{
				wantParse = false;
				wantRead = true;
				number = rwpb.Parse(str);
				if (changed)
				{
					wantWrite = true;
				}
			}
			if (wantWrite)
			{
				if (number)
				{
					rwpb.Write(*number);
				}
			}
			if (!inputFocus && wantRead)
			{
				wantRead = false;
				number = rwpb.Read();
				str = rwpb.Build(*number);
			}
			return changed;
		}
	};

	class PaginationContext
	{
		Gui::NumberInputContext<int32_t> pageInput;
		int32_t pageCount = 1;

	public:
		void SetPageCount(int32_t newPageCount)
		{
			pageCount = newPageCount;
			pageInput.ForceParse();
		}

		int32_t GetPageCount() const
		{
			return pageCount;
		}

		void ForceRead()
		{
			pageInput.ForceRead();
		}

		template<class ReadWrite>
		void Gui(View::ComponentKey key, ReadWrite &&rw)
		{
			auto &view = rw.view;
			view.BeginHPanel(key);
			view.SetSpacing(5);
			view.BeginText("before", "Page", View::TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
			view.SetParentFillRatio(0);
			view.EndText();
			struct Rwpb
			{
				View &view;
				ReadWrite &rw;
				PaginationContext &pc;
				int32_t Read() { return rw.Read(); }
				void Write(int32_t value) { rw.Write(value); }
				std::optional<int32_t> Parse(const std::string &str) { return ParsePage(str, pc.pageCount); }
				std::string Build(int32_t value) { return ByteString::Build(value + 1); }
			};
			Rwpb rwpb{ view, rw, *this };
			pageInput.BeginTextbox("input", rwpb);
			view.SetSize(GetDigitCount(pageCount) * 6 + 10);
			view.SetTextAlignment(Gui::Alignment::center, Gui::Alignment::center);
			pageInput.EndTextbox(rwpb);
			view.BeginText("after", ByteString::Build("of ", pageCount), View::TextFlags::autoWidth); // TODO-REDO_UI-TRANSLATE
			view.SetParentFillRatio(0);
			view.EndText();
			view.EndComponent();
		}
	};
}
