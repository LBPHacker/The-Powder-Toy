#include "ThumbnailRendererTask.h"

#include <cmath>

#include "graphics/VideoBuffer.h"
#include "simulation/SaveRenderer.h"
#include "client/GameSave.h"

int ThumbnailRendererTask::queueSize = 0;

int ThumbnailRendererTask::QueueSize()
{
	return queueSize;
}

ThumbnailRendererTask::ThumbnailRendererTask(GameSave const &originalSave, Vec2<int> size, RendererSettings::DecorationLevel newDecorationLevel, bool fire):
	size(size),
	decorationLevel(newDecorationLevel),
	fire(fire)
{
	save = std::make_unique<GameSave>(originalSave);
	queueSize += 1;
}

ThumbnailRendererTask::~ThumbnailRendererTask()
{
	queueSize -= 1;
}

bool ThumbnailRendererTask::doWork()
{
	RendererSettings rendererSettings;
	rendererSettings.decorationLevel = decorationLevel;
	thumbnail = SaveRenderer::Ref().Render(save.get(), fire, rendererSettings);
	if (thumbnail)
	{
		thumbnail->ResizeToFit(size, true);
		size = thumbnail->Size();
		return true;
	}
	else
	{
		return false;
	}
}

std::unique_ptr<VideoBuffer> ThumbnailRendererTask::Finish()
{
	auto ptr = std::move(thumbnail);
	AbandonableTask::Finish();
	return ptr;
}

