#pragma once
#include "Common/NoCopy.hpp"
#include "Common/Plane.hpp"
#include <list>
#include <cstdint>

typedef struct SDL_Texture SDL_Texture;
class VideoBuffer;

namespace Powder::Gui
{
	class Host;

	class StaticTexture : public NoCopy
	{
		Host &host;
		std::list<StaticTexture *>::iterator hostIt;
		SDL_Texture *texture = nullptr;
		bool haveTexture = false;
		PlaneAdapter<std::vector<uint32_t>> data;

	public:
		StaticTexture(Host &newHost, PlaneAdapter<std::vector<uint32_t>> newData);
		StaticTexture(Host &newHost, const VideoBuffer &video, bool makeOpaque);
		~StaticTexture();

		Vec2<int> Size() const
		{
			return data.Size();
		}

		friend class Host;
	};
}
