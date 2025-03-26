#pragma once
#include "Common/NoCopy.hpp"
#include "Common/Plane.hpp"
#include "graphics/Pixel.h"
#include <list>
#include <cstdint>
#include <memory>

typedef struct SDL_Texture SDL_Texture;
class VideoBuffer;

namespace Powder::Gui
{
	class Host;

	class StaticTexture : public NoCopy
	{
		Host &host;
		bool blend;
		std::list<StaticTexture *>::iterator hostIt;
		SDL_Texture *texture = nullptr;
		bool haveTexture = false;
		PlaneAdapter<std::vector<pixel>> data;

	public:
		StaticTexture(Host &newHost, bool newBlend, PlaneAdapter<std::vector<pixel>> newData);
		StaticTexture(Host &newHost, bool newBlend, std::unique_ptr<VideoBuffer> video);
		~StaticTexture();

		Vec2<int> GetSize() const
		{
			return data.Size();
		}

		const PlaneAdapter<std::vector<pixel>> &GetData() const
		{
			return data;
		}

		friend class Host;
	};
}
