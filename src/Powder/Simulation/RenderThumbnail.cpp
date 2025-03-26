#include "RenderThumbnail.hpp"
#include "Common/ThreadPool.hpp"
#include "client/GameSave.h"
#include "graphics/VideoBuffer.h"
#include "graphics/Renderer.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationData.h"

namespace Powder::Simulation
{
	std::future<std::unique_ptr<VideoBuffer>> RenderThumbnail(
		ThreadPool &threadPool,
		std::unique_ptr<GameSave> save,
		Vec2<int> size,
		RendererSettings::DecorationLevel decorationLevel,
		bool fire
	)
	{
		RendererSettings rendererSettings;
		rendererSettings.decorationLevel = decorationLevel;
		return RenderThumbnail(threadPool, std::move(save), size, rendererSettings, fire);
	}

	std::future<std::unique_ptr<VideoBuffer>> RenderThumbnail(
		ThreadPool &threadPool,
		std::unique_ptr<GameSave> save,
		Vec2<int> size,
		RendererSettings rendererSettings,
		bool fire
	)
	{
		std::promise<std::unique_ptr<VideoBuffer>> promise;
		auto future = promise.get_future();
		threadPool.PushWorkItem([
			promiseInner = std::move(promise),
			saveInner = std::move(save),
			rendererSettings,
			fire,
			size
		]() mutable {
			auto thumbnail = std::make_unique<VideoBuffer>(saveInner->blockSize * CELL);
			{
				auto &sd = SimulationData::CRef();
				std::shared_lock lk(sd.elementGraphicsMx);
				auto sim = std::make_unique<::Simulation>();
				auto ren = std::make_unique<Renderer>();
				ren->sim = sim.get();
				ren->ApplySettings(rendererSettings);
				sim->clear_sim();
				sim->Load(saveInner.get(), true, { 0, 0 });
				ren->ClearAccumulation();
				ren->Clear();
				if (fire)
				{
					ren->ApproximateAccumulation();
				}
				ren->RenderSimulation();
				auto &video = ren->GetVideo();
				thumbnail->BlendImage(video.data(), 0xFF, video.Size().OriginRect());
			}
			thumbnail->ResizeToFit(size, true);
			promiseInner.set_value(std::move(thumbnail));
		});
		return future;
	}
}

