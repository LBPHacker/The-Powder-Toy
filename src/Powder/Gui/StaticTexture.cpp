#include "StaticTexture.hpp"
#include "Host.hpp"
#include "graphics/VideoBuffer.h"

namespace Powder::Gui
{
	StaticTexture::StaticTexture(Host &newHost, bool newBlend, PlaneAdapter<std::vector<pixel>> newData) : host(newHost), blend(newBlend), data(std::move(newData))
	{
		host.AddStaticTexture(*this);
	}

	StaticTexture::StaticTexture(Host &newHost, bool newBlend, std::unique_ptr<VideoBuffer> video) : StaticTexture(newHost, newBlend, video->ExtractVideo())
	{
	}

	StaticTexture::~StaticTexture()
	{
		host.RemoveStaticTexture(*this);
	}
}
