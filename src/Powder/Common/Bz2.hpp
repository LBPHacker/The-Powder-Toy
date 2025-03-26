#include "Assert.hpp"
#include "Common/Log.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace Powder
{
	using Bz2DataSize = int32_t;

	struct Bz2CompressError : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	std::vector<char> Bz2Compress(std::span<const char> uncompressed, std::optional<Bz2DataSize> maxCompressedSize);

	struct Bz2DecompressError : public std::runtime_error
	{
		using runtime_error::runtime_error;
	};
	std::vector<char> Bz2Decompress(std::span<const char> compressed, std::optional<Bz2DataSize> maxUncompressedSize);
}
