#pragma once

#include <cstdint>

constexpr uint8_t PixA(uint32_t pix)
{
	return uint8_t(pix >> 24);
}

constexpr uint8_t PixR(uint32_t pix)
{
	return uint8_t(pix >> 16);
}

constexpr uint8_t PixG(uint32_t pix)
{
	return uint8_t(pix >> 8);
}

constexpr uint8_t PixB(uint32_t pix)
{
	return uint8_t(pix);
}

constexpr uint32_t PixRGB(uint8_t r, uint8_t g, uint8_t b)
{
	return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

constexpr uint32_t PixRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

constexpr uint32_t PixPack(uint32_t pack)
{
	return pack;
}
