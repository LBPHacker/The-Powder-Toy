#include "Language.hpp"
#include "en_US_lang.h"
#include <iomanip>

using namespace Powder;
using namespace Powder::Lang;

namespace
{
	bool IsWhitespace(char ch)
	{
		switch (ch)
		{
		case ' ':
		case '\f':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
			return true;
		}
		return false;
	}

	bool IsAlpha(char ch)
	{
		return (ch >= 'a' && ch <= 'z') ||
		       (ch >= 'A' && ch <= 'Z') ||
		       uint8_t(ch) >= 128 ||
		        ch == '-' ||
		        ch == '_';
	}

	bool IsAlphaNumeric(char ch)
	{
		return (ch >= '0' && ch <= '9') ||
		       (ch >= 'a' && ch <= 'z') ||
		       (ch >= 'A' && ch <= 'Z') ||
		       uint8_t(ch) >= 128 ||
		        ch == '-' ||
		        ch == '_';
	}

	struct Template
	{
		struct Literal
		{
			std::string value;
		};
		struct Parameter
		{
			int32_t index;
		};
		using Item = std::variant<Literal, Parameter>;
		std::vector<Item> items;

		std::string Format(std::span<FormatParam> parameters) const
		{
			std::string s;
			for (auto &item : items)
			{
				if (auto *literal = std::get_if<Literal>(&item))
				{
					s += literal->value;
				}
				else
				{
					auto index = std::get<Parameter>(item).index;
					if (index >= 0 && index < int32_t(parameters.size()))
					{
						if (auto *i64Param = std::get_if<int64_t>(&parameters[index]))
						{
							s += std::to_string(*i64Param);
						}
						else if (auto *f32Param = std::get_if<float>(&parameters[index]))
						{
							std::ostringstream ss;
							ss << std::fixed;
							ss << std::setprecision(2);
							ss << *f32Param;
							s += ss.str();
						}
						else
						{
							s += std::get<std::string_view>(parameters[index]);
						}
					}
					else
					{
						s += "???";
					}
				}
			}
			return s;
		}
	};

	class Parser : public NoCopy
	{
	public:
		using Data = std::span<const char>;
		struct Where
		{
			Data::iterator it;
			int row;
			int col;

			auto operator <(const Data::iterator &other) const { return it < other; }
			auto operator ==(const Data::iterator &other) const { return it == other; }
			auto operator <(const Where &other) const { return it < other.it; }
			auto operator ==(const Where &other) const { return it == other.it; }
			auto &operator *() const { return *it; }

			auto &operator ++()
			{
				col += 1;
				if (*it == '\n')
				{
					row += 1;
					col = 1;
				}
				++it;
				return *this;
			}
		};

	private:
		Data data;
		Where it;

		void SkipWhitespace()
		{
			bool inComment = false;
			while (it < data.end())
			{
				if (inComment)
				{
					if (*it == '\n')
					{
						inComment = false;
					}
					++it;
					continue;
				}
				if (IsWhitespace(*it))
				{
					++it;
					continue;
				}
				if (*it == '#')
				{
					inComment = true;
					++it;
					continue;
				}
				break;
			}
		}

	public:
		Parser(std::span<const char> newData) : data(newData)
		{
			it.it = data.begin();
			it.row = 1;
			it.col = 1;
		}

		Where GetWhere() const
		{
			return it;
		}

		void Die(const char *why)
		{
			Die(why, it);
		}

		void Die(const char *why, Where where)
		{
			throw std::runtime_error(ByteString::Build(why, " at ", where.row, ":", where.col));
		}

		std::optional<std::pair<Where, std::string>> GetName()
		{
			SkipWhitespace();
			if (it == data.end())
			{
				return std::nullopt;
			}
			auto firstIt = it;
			if (it < data.end() && IsAlpha(*it))
			{
				++it;
			}
			while (it < data.end() && IsAlphaNumeric(*it))
			{
				++it;
			}
			if (firstIt == it)
			{
				Die("no name", firstIt);
			}
			return std::make_pair(firstIt, std::string(firstIt.it, it.it));
		}

		std::pair<Where, Template> GetTemplate()
		{
			SkipWhitespace();
			Template templ;
			auto firstIt = it;
			if (it < data.end() && *it == '"')
			{
				++it;
			}
			else
			{
				Die("no template", it);
			}
			bool done = false;
			bool escape = false;
			std::optional<int32_t> paramIndex;
			while (it < data.end() && !done)
			{
				if (paramIndex)
				{
					if (*it == '@')
					{
						templ.items.push_back(Template::Parameter{ *paramIndex });
						paramIndex.reset();
						++it;
						continue;
					}
					if (*it >= '0' && *it <= '9' && *paramIndex < 100)
					{
						*paramIndex = *paramIndex * 10 + int32_t(*it - '0');
						++it;
						continue;
					}
					Die("bad parameter index");
				}
				if (!escape && *it == '"')
				{
					done = true;
					++it;
					continue;
				}
				if (!escape && *it == '\\')
				{
					escape = true;
					++it;
					continue;
				}
				if (!escape && *it == '@')
				{
					paramIndex = 0;
					++it;
					continue;
				}
				if (templ.items.empty() || !std::holds_alternative<Template::Literal>(templ.items.back()))
				{
					templ.items.push_back(Template::Literal{});
				}
				std::get<Template::Literal>(templ.items.back()).value.push_back(*it);
				escape = false;
				++it;
			}
			if (!done)
			{
				Die("unterminated template", firstIt);
			}
			return { firstIt, templ };
		}
	};

	class SimpleFormatter : public Formatter
	{
		Template templ;

	public:
		SimpleFormatter(Template newTempl) : templ(newTempl)
		{
		}

		std::string Format(std::span<FormatParam> params) final override
		{
			return templ.Format(params);
		}
	};

	class Plural2Formatter : public Formatter
	{
		Template templOne;
		Template templMore;

	public:
		Plural2Formatter(Template newTemplOne, Template newTemplMore) : templOne(newTemplOne), templMore(newTemplMore)
		{
		}

		std::string Format(std::span<FormatParam> params) final override
		{
			if (std::get<int64_t>(params[0]) == 1)
			{
				return templOne.Format(params);
			}
			return templMore.Format(params);
		}
	};

	class MissingFormatter : public Formatter
	{
		std::string Format(std::span<FormatParam> params) final override
		{
			return "???????";
		}
	};
}

void Language::Load(std::span<const char> data)
{
	for (auto &[ key, holder ] : formatterHolders)
	{
		holder.formatter = std::make_unique<MissingFormatter>();
	}
	Parser parser{ data };
	while (auto name = parser.GetName())
	{
		auto method = parser.GetName();
		if (!method)
		{
			parser.Die("unexpected EOF");
		}
		std::unique_ptr<Formatter> formatter;
		if (method->second == "simple")
		{
			auto templ = parser.GetTemplate();
			GetFormatterHolder(name->second.c_str()).formatter = std::make_unique<SimpleFormatter>(templ.second);
		}
		else if (method->second == "plural2")
		{
			auto templOne = parser.GetTemplate();
			auto templMore = parser.GetTemplate();
			GetFormatterHolder(name->second.c_str()).formatter = std::make_unique<Plural2Formatter>(templOne.second, templMore.second);
		}
		else
		{
			parser.Die("unknown method", method->first);
		}
	}
}

Language::Language()
{
	Load(en_US_lang.AsString());
}

FormatterHolder &Language::GetFormatterHolder(const char *name)
{
	auto [ it, inserted ] = formatterHolders.emplace(std::make_pair(name, FormatterHolder{}));
	if (inserted)
	{
		it->second.formatter = std::make_unique<MissingFormatter>();
	}
	return it->second;
}

std::string Language::Format(Translation &templ)
{
	return templ.GetFormatterHolder().formatter->Format({});
}
