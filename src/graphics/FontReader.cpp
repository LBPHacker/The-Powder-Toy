#include "FontReader.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
// #include <iomanip>
#include <sstream>

const unsigned int unicode_max = 0x110000;
const unsigned int invalid = 0x80000000U;
const int default_baseline = 2;

std::vector<unsigned char> merged_font_data;
std::vector<unsigned int> merged_font_ptrs;
std::vector<unsigned int> merged_font_ranges;

bool FontReader::MergeExternal(ByteString path)
{
	std::fstream f(path, f.in | f.binary);
	if (!f)
	{
		std::cerr << __func__ << ": failed to open " << path << std::endl;
		return false;
	}

	// load current font data
	auto last_cp_ptr = 0U;
	auto font_ptrs_it = font_ptrs;
	std::vector<unsigned int> temp_font_sparse_ptrs(unicode_max, invalid);
	for (auto it = font_ranges; (*it)[1]; ++it)
	{
		for (auto cp = (*it)[0]; cp <= (*it)[1]; ++cp)
		{
			temp_font_sparse_ptrs[cp] = *(font_ptrs_it++);
		}
		last_cp_ptr = font_ptrs_it[-1];
	}

	auto data_size = last_cp_ptr + 1;
	auto bits_needed = font_data[last_cp_ptr] * FONT_H * 2;
	data_size += bits_needed / 8;
	if (bits_needed % 8)
	{
		++data_size;
	}
	std::vector<unsigned char> temp_font_data(data_size);
	std::copy(font_data, font_data + data_size, temp_font_data.data());
	// std::cerr << (int)temp_font_data[temp_font_data.size() - 7] << std::endl; // should be 116

	// read bdf
	{
		int baseline = default_baseline;
		std::string line_buf;
		bool in_char = false;
		int want_bitmap_lines;
		bool code_point_ok, dwidth_ok, bbox_ok;
		unsigned int code_point;
		unsigned int dwidth_x, dwidth_y;
		unsigned int bbox_w, bbox_h;
		int bbox_x, bbox_y;
		int right_margin;
		int line_ptr;
		std::array<unsigned int, FONT_H> bitmap_lines;
		int line_cnt = 0;
		while (std::getline(f, line_buf))
		{
			++line_cnt;
			std::transform(line_buf.begin(), line_buf.end(), line_buf.begin(), [](char in) -> char {
				return (in <= 'Z' && in >= 'A') ? (in - ('Z' - 'z')) : in;
			});
			std::istringstream ss(line_buf);
			std::string first;
			if (!(ss >> first))
			{
				std::cerr << "line " << line_cnt << ":something is up with the format of the line" << std::endl;
				continue;
			}

			if (in_char)
			{
				if (first == "encoding")
				{
					if (!(ss >> code_point))
					{
						std::cerr << "line " << line_cnt << ":something is up with the format of encoding" << std::endl;
						continue;
					}
					if (code_point >= unicode_max)
					{
						std::cerr << "line " << line_cnt << ":out of bounds" << std::endl;
						continue;
					}
					code_point_ok = true;
				}
				else if (first == "dwidth")
				{
					if (!(ss >> dwidth_x >> dwidth_y))
					{
						std::cerr << "line " << line_cnt << ":something is up with the format of dwidth" << std::endl;
						continue;
					}
					if (dwidth_y != 0)
					{
						std::cerr << "line " << line_cnt << ":step vector isn't parallel to x axis" << std::endl;
						continue;
					}
					if (dwidth_x > 32)
					{
						std::cerr << "line " << line_cnt << ":dwidth too wide" << std::endl;
						continue;
					}
					dwidth_ok = true;
				}
				else if (first == "bbx")
				{
					if (!(ss >> bbox_w >> bbox_h >> bbox_x >> bbox_y))
					{
						std::cerr << "line " << line_cnt << ":something is up with the format of bbx" << std::endl;
						continue;
					}
					if (bbox_w > 32)
					{
						std::cerr << "line " << line_cnt << ":bbox too wide" << std::endl;
						continue;
					}
					bbox_ok = true;
					line_ptr = FONT_H - bbox_h - bbox_y - baseline;
					right_margin = dwidth_x - bbox_w - bbox_x;
				}
				else if (first == "endchar")
				{
					if (!code_point_ok)
					{
						std::cerr << "line " << line_cnt << ":encoding is bad" << std::endl;
					}
					else if (!dwidth_ok)
					{
						std::cerr << "line " << line_cnt << ":dwidth is bad" << std::endl;
					}
					else if (!bbox_ok)
					{
						std::cerr << "line " << line_cnt << ":bbox is bad" << std::endl;
					}
					else if (true || temp_font_sparse_ptrs[code_point] == invalid)
					{
						temp_font_sparse_ptrs[code_point] = temp_font_data.size();
						temp_font_data.push_back(dwidth_x);
						auto pixels = 0;
						auto push_pixel = [&temp_font_data, &pixels](bool set) {
							if (!pixels)
							{
								pixels = 4;
								temp_font_data.push_back(0);
							}
							--pixels;
							temp_font_data.back() |= (set ? 3 : 0) << ((3 - pixels) * 2);
						};
						for (auto l : bitmap_lines)
						{
							// std::cerr << std::setfill('0') << std::setw(8) << std::uppercase << std::hex << l << std::endl;
							for (auto i = 0U; i < dwidth_x; ++i)
							{
								push_pixel((l & (1 << (dwidth_x - 1 - i))) != 0);
							}
						}
					}
					in_char = false;
				}
				else if (first == "bitmap")
				{
					if (!bbox_ok)
					{
						std::cerr << "line " << line_cnt << ":no bbox yet" << std::endl;
						continue;
					}
					want_bitmap_lines = bbox_h;
				}
				else if (want_bitmap_lines)
				{
					if (line_ptr >= 0 && line_ptr < FONT_H)
					{
						auto line_buf_it = line_buf.begin();
						auto bits = 0U;
						unsigned int bitbuf = 0;
						auto buf = 0U;
						for (auto i = 0U; i < bbox_w; ++i)
						{
							if (!bits)
							{
								bits = 4;
								auto ch = *(line_buf_it++);
								if (ch >= '0' && ch <= '9') bitbuf = ch - '0';
								if (ch >= 'a' && ch <= 'f') bitbuf = ch - 'a' + 10;
							}
							--bits;
							buf = buf * 2 + ((bitbuf >> bits) & 1);
						}
						bitmap_lines[line_ptr] = buf << right_margin;
					}
					++line_ptr;
					--want_bitmap_lines;
				}
			}
			else
			{
				if (first == "startchar")
				{
					in_char = true;
					want_bitmap_lines = 0;
					code_point_ok = false;
					dwidth_ok = false;
					bbox_ok = false;
					std::fill(bitmap_lines.begin(), bitmap_lines.end(), 0);
				}
			}
		}
	}

	// generate compact pointer list from the sparse one
	std::vector<unsigned int> temp_font_ptrs;
	std::vector<unsigned int> temp_font_ranges;
	{
		auto i = 0U;
		bool valid = false;
		for (auto sparse_ptr : temp_font_sparse_ptrs)
		{
			if (valid && sparse_ptr == invalid)
			{
				temp_font_ranges.push_back(i - 1);
				valid = false;
			}
			if (!valid && sparse_ptr != invalid)
			{
				temp_font_ranges.push_back(i);
				valid = true;
			}
			if (valid)
			{
				temp_font_ptrs.push_back(sparse_ptr);
			}
			++i;
		}
		if (valid)
		{
			temp_font_ranges.push_back(unicode_max - 1);
		}
	}
	temp_font_ranges.push_back(0);
	temp_font_ranges.push_back(0);

	// we're done, update pointers
	merged_font_data.clear();
	merged_font_ptrs.clear();
	merged_font_ranges.clear();
	std::swap(merged_font_data, temp_font_data);
	std::swap(merged_font_ptrs, temp_font_ptrs);
	std::swap(merged_font_ranges, temp_font_ranges);
	font_data = merged_font_data.data();
	font_ptrs = merged_font_ptrs.data();
	font_ranges = (unsigned int (*)[2])merged_font_ranges.data();
	std::cerr << __func__ << ": merged " << path << std::endl;
	return true;
}
