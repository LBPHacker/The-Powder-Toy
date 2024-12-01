#pragma once
#include "common/Plane.h"
#include "common/String.h"
#include "graphics/Pixel.h"
#include <memory>
#include <vector>

class VideoBuffer;

namespace format
{
ByteString URLEncode(ByteString value);
ByteString URLDecode(ByteString value);
ByteString UnixtimeToDate(time_t unixtime, ByteString dateFomat = ByteString("%d %b %Y"), bool local = true);
ByteString UnixtimeToDateMini(time_t unixtime);
String CleanString(String dirtyString, bool ascii, bool color, bool newlines, bool numeric = false);
std::vector<char> PixelsToPPM(const PlaneAdapter<std::vector<pixel>> &);
std::unique_ptr<std::vector<char>> PixelsToPNG(const PlaneAdapter<std::vector<pixel>> &);
std::unique_ptr<PlaneAdapter<std::vector<pixel_rgba>>> PixelsFromPNG(const std::vector<char> &);
std::unique_ptr<PlaneAdapter<std::vector<pixel>>> PixelsFromPNG(const std::vector<char> &, RGB<uint8_t> background);
void RenderTemperature(StringBuilder &sb, float temp, int scale);
float StringToTemperature(String str, int defaultScale);
}
