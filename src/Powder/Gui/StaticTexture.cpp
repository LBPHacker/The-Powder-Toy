#include "StaticTexture.hpp"
#include "Host.hpp"
#include "graphics/VideoBuffer.h"

namespace Powder::Gui
{
	namespace
	{
		PlaneAdapter<std::vector<uint32_t>> VideoBufferToPlane(const VideoBuffer &video, bool makeOpaque)
		{
			auto size = video.Size();
			PlaneAdapter<std::vector<uint32_t>> plane(size);
			std::copy_n(video.Data(), size.X * size.Y, plane.data());
			if (makeOpaque)
			{
				for (auto p : plane.Size().OriginRect())
				{
					plane[p] |= UINT32_C(0xFF000000);
				}
			}
			return plane;
		}
	}

	StaticTexture::StaticTexture(Host &newHost, PlaneAdapter<std::vector<uint32_t>> newData) : host(newHost), data(std::move(newData))
	{
		host.AddStaticTexture(*this);
	}

	StaticTexture::StaticTexture(Host &newHost, const VideoBuffer &video, bool makeOpaque) : StaticTexture(newHost, VideoBufferToPlane(video, makeOpaque))
	{
	}

	StaticTexture::~StaticTexture()
	{
		host.RemoveStaticTexture(*this);
	}
}
