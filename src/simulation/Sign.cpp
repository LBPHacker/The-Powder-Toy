#include "Sign.h"

#include "simulation/Simulation.h"
#include "gui/SDLWindow.h"
#include "language/Language.h"

sign::sign(String text_, int x_, int y_, Justification justification_):
	x(x_),
	y(y_),
	ju(justification_),
	text(text_)
{
}

sign::Info sign::GetInfo(const Simulation *sim) const
{
	Info info;
	info.v95 = false;
	auto si = std::make_pair(0, Type::Normal);
	if (text.find('{') == text.npos)
	{
		info.text = text;
	}
	else
	{
		si = split();
		if (si.first)
		{
			info.text = text.Between(si.first + 1, text.size() - 1);
		}
		else
		{
			Particle const *part = nullptr;
			float pressure = 0.0f;
			float aheat = 0.0f;
			if (sim && x >= 0 && x < XRES && y >= 0 && y < YRES)
			{
				if (sim->photons[y][x])
				{
					part = &(sim->parts[ID(sim->photons[y][x])]);
				}
				else if (sim->pmap[y][x])
				{
					part = &(sim->parts[ID(sim->pmap[y][x])]);
				}
				pressure = sim->pv[y/CELL][x/CELL];
				aheat = sim->hv[y/CELL][x/CELL] - 273.15f;
			}

			String remaining_text = text;
			StringBuilder formatted_text;
			while (auto split_left_curly = remaining_text.SplitBy('{'))
			{
				String after_left_curly = split_left_curly.After();
				if (auto split_right_curly = after_left_curly.SplitBy('}'))
				{
					formatted_text << split_left_curly.Before();
					remaining_text = split_right_curly.After();
					String between_curlies = split_right_curly.Before();
					if (between_curlies == "t" || between_curlies == "temp")
					{
						formatted_text << Format::Precision(Format::ShowPoint(part ? part->temp - 273.15f : 0.0f), 2);
						// * We would really only need to do this if the sign used the new
						//   keyword "temp" or if the text was more than just "{t}", but 95.0
						//   upgrades such signs at load time anyway.
						// * The same applies to "{p}" and "{aheat}" signs.
						info.v95 = true;
					}
					else if (between_curlies == "p" || between_curlies == "pres")
					{
						formatted_text << Format::Precision(Format::ShowPoint(pressure), 2);
						info.v95 = true;
					}
					else if (between_curlies == "a" || between_curlies == "aheat")
					{
						formatted_text << Format::Precision(Format::ShowPoint(aheat), 2);
						info.v95 = true;
					}
					else if (between_curlies == "type")
					{
						auto str = part ? sim->ElementResolveDeep(part->type, part->ctype, part->tmp4) : "DEFAULT_LS_GAME_HUD_EMPTY"_Ls();
						if (!formatted_text.Size() && str.size())
						{
							// TODO-REDO_UI: Might not work in all locales, idk.
							str[0] = std::toupper(str[0]);
						}
						formatted_text << str;
						info.v95 = true;
					}
					else if (between_curlies == "ctype")
					{
						auto str = part ? (sim->IsElementOrNone(part->ctype) ? sim->ElementResolve(part->ctype, -1) : String::Build(part->ctype)) : "DEFAULT_LS_GAME_HUD_EMPTY"_Ls();
						if (!formatted_text.Size() && str.size())
						{
							// TODO-REDO_UI: Might not work in all locales, idk.
							str[0] = std::toupper(str[0]);
						}
						formatted_text << str;
						info.v95 = true;
					}
					else if (between_curlies == "life")
					{
						formatted_text << (part ? part->life : 0);
						info.v95 = true;
					}
					else if (between_curlies == "tmp")
					{
						formatted_text << (part ? part->tmp : 0);
						info.v95 = true;
					}
					else if (between_curlies == "tmp2")
					{
						formatted_text << (part ? part->tmp2 : 0);
						info.v95 = true;
					}
					else
					{
						formatted_text << '{' << between_curlies << '}';
					}
				}
				else
				{
					break;
				}
			}
			formatted_text << remaining_text;
			info.text = formatted_text.Build();
		}
	}

	switch (si.second)
	{
	case Normal: info.r = 255; info.g = 255; info.b = 255; break;
	case Save:   info.r =  32; info.g = 170; info.b = 255; break;
	case Thread: info.r = 255; info.g =  75; info.b =  75; break;
	case Button: info.r = 255; info.g = 216; info.b =  32; break;
	case Search: info.r = 147; info.g =  83; info.b = 211; break;
	}

	info.w = gui::SDLWindow::Ref().TextSize(info.text.c_str()).x + 5;
	info.h = 15;
	info.x = (ju == Right) ? (x - info.w) : ((ju == Left) ? x : (x - info.w / 2));
	info.y = (y > 18) ? (y - 18) : (y + 4);

	return info;
}

std::pair<int, sign::Type> sign::split() const
{
	String::size_type pipe = 0;
	if (text.size() >= 4 && text.front() == '{' && text.back() == '}')
	{
		switch (text[1])
		{
		case 'c':
		case 't':
			if (text[2] == ':' && (pipe = text.find('|', 4)) != text.npos)
			{
				for (String::size_type i = 3; i < pipe; ++i)
				{
					if (text[i] < '0' || text[i] > '9')
					{
						return std::make_pair(0, Type::Normal);
					}
				}
				return std::make_pair(int(pipe), text[1] == 'c' ? Type::Save : Type::Thread);
			}
			break;

		case 'b':
			if (text[2] == '|')
			{
				return std::make_pair(2, Type::Button);
			}
			break;

		case 's':
			if (text[2] == ':' && (pipe = text.find('|', 3)) != text.npos)
			{
				return std::make_pair(int(pipe), Type::Search);
			}
			break;
		}
	}
	return std::make_pair(0, Type::Normal);
}
