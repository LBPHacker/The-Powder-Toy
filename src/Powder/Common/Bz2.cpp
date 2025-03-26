#include "Bz2.hpp"
#include "Defer.hpp"
#include <bzlib.h>

namespace Powder
{
	namespace
	{
		constexpr Bz2DataSize outputSizeIncrement    = 0x100000U;
		constexpr Bz2DataSize maxMaxCompressedSize   = 1'000'000'000;
		constexpr Bz2DataSize maxMaxUncompressedSize = 1'000'000'000;
	}

	std::vector<char> Bz2Compress(std::span<const char> uncompressed, std::optional<Bz2DataSize> maxCompressedSize)
	{
		if (uncompressed.size() > maxMaxUncompressedSize)
		{
			throw Bz2CompressError("oversize input");
		}
		if (maxCompressedSize && *maxCompressedSize < 0)
		{
			throw Bz2CompressError("invalid max size");
		}
		bz_stream stream;
		stream.bzalloc = nullptr;
		stream.bzfree = nullptr;
		stream.opaque = nullptr;
		if (BZ2_bzCompressInit(&stream, 9, 0, 0) != BZ_OK)
		{
			throw std::bad_alloc();
		}
		Defer compressEnd([&stream]() {
			BZ2_bzCompressEnd(&stream);
		});
		stream.next_in = const_cast<char *>(uncompressed.data()); // I hope bz2 doesn't actually write anything here...
		stream.avail_in = uncompressed.size();
		std::vector<char> compressed;
		auto effectiveMaxCompressedSize = maxMaxCompressedSize;
		if (maxCompressedSize)
		{
			effectiveMaxCompressedSize = std::min(effectiveMaxCompressedSize, *maxCompressedSize);
		}
		while (true)
		{
			auto thisOutputSizeIncrement = outputSizeIncrement;
			if (compressed.empty() && maxCompressedSize)
			{
				thisOutputSizeIncrement = effectiveMaxCompressedSize;
			}
			compressed.resize(std::min(Bz2DataSize(compressed.size()) + thisOutputSizeIncrement, effectiveMaxCompressedSize));
			stream.next_out = compressed.data() + stream.total_out_lo32;
			stream.avail_out = compressed.size() - stream.total_out_lo32;
			if (BZ2_bzCompress(&stream, BZ_FINISH) == BZ_STREAM_END)
			{
				break;
			}
			if (Bz2DataSize(compressed.size()) >= effectiveMaxCompressedSize)
			{
				throw Bz2CompressError("reached max size");
			}
		}
		compressed.resize(stream.total_out_lo32);
		return compressed;
	}

	std::vector<char> Bz2Decompress(std::span<const char> compressed, std::optional<Bz2DataSize> maxUncompressedSize)
	{
		if (compressed.size() > maxMaxCompressedSize)
		{
			throw Bz2DecompressError("oversize input");
		}
		if (maxUncompressedSize && *maxUncompressedSize < 0)
		{
			throw Bz2DecompressError("invalid max size");
		}
		bz_stream stream;
		stream.bzalloc = nullptr;
		stream.bzfree = nullptr;
		stream.opaque = nullptr;
		if (BZ2_bzDecompressInit(&stream, 0, 0) != BZ_OK)
		{
			throw std::bad_alloc();
		}
		Defer decompressEnd([&stream]() {
			BZ2_bzDecompressEnd(&stream);
		});
		stream.next_in = const_cast<char *>(compressed.data()); // I hope bz2 doesn't actually write anything here...
		stream.avail_in = compressed.size();
		std::vector<char> uncompressed;
		auto effectiveMaxUncompressedSize = maxMaxUncompressedSize;
		if (maxUncompressedSize)
		{
			effectiveMaxUncompressedSize = std::min(effectiveMaxUncompressedSize, *maxUncompressedSize);
		}
		while (true)
		{
			auto thisOutputSizeIncrement = outputSizeIncrement;
			if (uncompressed.empty() && maxUncompressedSize)
			{
				thisOutputSizeIncrement = effectiveMaxUncompressedSize;
			}
			uncompressed.resize(std::min(Bz2DataSize(uncompressed.size()) + thisOutputSizeIncrement, effectiveMaxUncompressedSize));
			stream.next_out = uncompressed.data() + stream.total_out_lo32;
			stream.avail_out = uncompressed.size() - stream.total_out_lo32;
			auto result = BZ2_bzDecompress(&stream);
			if (result == BZ_STREAM_END)
			{
				break;
			}
			if (result == BZ_MEM_ERROR)
			{
				throw std::bad_alloc();
			}
			if (result != BZ_OK || (!stream.avail_in && stream.avail_out))
			{
				throw Bz2DecompressError("corrupted stream");
			}
			if (Bz2DataSize(uncompressed.size()) >= effectiveMaxUncompressedSize)
			{
				throw Bz2DecompressError("reached max size");
			}
		}
		uncompressed.resize(stream.total_out_lo32);
		return uncompressed;
	}
}
