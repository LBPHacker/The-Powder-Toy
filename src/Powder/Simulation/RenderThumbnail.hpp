#pragma once
#include <future>
#include "graphics/RendererSettings.h"

class VideoBuffer;
class GameSave;

namespace Powder
{
	class ThreadPool;
}

namespace Powder::Simulation
{
	std::future<std::unique_ptr<VideoBuffer>> RenderThumbnail(
		ThreadPool &threadPool,
		std::unique_ptr<GameSave> save,
		Vec2<int> size,
		RendererSettings::DecorationLevel decorationLevel,
		bool fire
	);
	std::future<std::unique_ptr<VideoBuffer>> RenderThumbnail(
		ThreadPool &threadPool,
		std::unique_ptr<GameSave> save,
		Vec2<int> size,
		RendererSettings rendererSettings,
		bool fire
	);
}
