#include "Game.hpp"
#include "gui/game/Brush.h"
#include "gui/game/BitmapBrush.h"
#include "gui/game/EllipseBrush.h"
#include "gui/game/RectangleBrush.h"
#include "gui/game/TriangleBrush.h"
#include "gui/game/tool/Tool.h"

#include "Common/Log.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size zoomViewportMaxSize = 256;

		static ui::Point AdjustBrushSize(int32_t diffX, int32_t diffY, bool logarithmic, ui::Point oldSize, int32_t minSize, int32_t maxSize)
		{
			// TODO-REDO_UI-POSTCLEANUP: remove Brush::AdjustSize
			int delta = diffX == 0 ? diffY : diffX;
			bool keepX = diffX == 0;
			bool keepY = diffY == 0;
			if (keepX && keepY)
			{
				return oldSize;
			}
			ui::Point newSize(0, 0);
			if (logarithmic)
			{
				newSize = oldSize + ui::Point(delta * std::max(oldSize.X / 5, 1), delta * std::max(oldSize.Y / 5, 1));
			}
			else
			{
				newSize = oldSize + ui::Point(delta, delta);
			}
			if (keepY)
			{
				newSize = ui::Point(newSize.X, oldSize.Y);
			}
			else if (keepX)
			{
				newSize = ui::Point(oldSize.X, newSize.Y);
			}
			newSize.X = std::clamp(newSize.X, minSize, maxSize);
			newSize.Y = std::clamp(newSize.Y, minSize, maxSize);
			return newSize;
		}
	}

	void Game::InitBrushes()
	{
		brushes.resize(NUM_DEFAULTBRUSHES);
		brushes[BRUSH_CIRCLE] = std::make_unique<EllipseBrush>(perfectCircleBrush);
		brushes[BRUSH_SQUARE] = std::make_unique<RectangleBrush>();
		brushes[BRUSH_TRIANGLE] = std::make_unique<TriangleBrush>();
	}

	void Game::SetPerfectCircleBrush(bool newPerfectCircleBrush)
	{
		perfectCircleBrush = newPerfectCircleBrush;
		InitBrushes();
	}

	void Game::CycleBrushAction()
	{
		auto nextBrushIndex = brushIndex;
		auto tryNext = [this, &nextBrushIndex]() {
			nextBrushIndex += 1;
			if (!(nextBrushIndex >= 0 && nextBrushIndex < int32_t(brushes.size())))
			{
				nextBrushIndex = 0;
			}
		};
		for (int32_t i = 0; i < int32_t(brushes.size()); ++i)
		{
			tryNext();
			if (brushes[nextBrushIndex])
			{
				SetBrushIndex(nextBrushIndex);
				break;
			}
		}
	}

	void Game::SetBrushRadius(Point newBrushRadius)
	{
		brushRadius = newBrushRadius;
		auto *brush = GetCurrentBrush();
		if (brush)
		{
			brush->SetRadius(brushRadius);
		}
	}

	void Game::AdjustBrushSize(int32_t diffX, int32_t diffY, bool logarithmic)
	{
		SetBrushRadius(Activity::AdjustBrushSize(diffX, diffY, logarithmic, brushRadius, 0, 200));
	}

	void Game::AdjustZoomSize(int32_t diffX, int32_t diffY, bool logarithmic)
	{
		auto oldSize = zoomMetrics.from.size;
		auto oldRadius = (oldSize - Size2{ 1, 1 }) / 2;
		auto newRadius = Activity::AdjustBrushSize(diffX, diffY, logarithmic, oldRadius, 2, 31);
		auto newSize = newRadius * 2 + Size2{ 1, 1 };
		zoomMetrics = MakeZoomMetrics(newSize);
	}

	std::optional<Game::Pos2> Game::ResolveZoomIfAffected(Pos2 point) const
	{
		if (zoomShown)
		{
			auto r = Rect{ zoomMetrics.to, zoomMetrics.from.size * zoomMetrics.scale - Size2{ 1, 1 } };
			if (r.Contains(point))
			{
				return zoomMetrics.from.pos + (point - r.pos) / zoomMetrics.scale;
			}
		}
		return std::nullopt;
	}

	bool Game::CheckZoomMetrics(const ZoomMetrics &newMetrics) const
	{
		auto toRect = RectSized(newMetrics.to, newMetrics.from.size * newMetrics.scale - Size2{ 1, 1 });
		return ((RES.OriginRect() & newMetrics.from) == newMetrics.from &&
		        newMetrics.scale >= 2 &&
		        newMetrics.scale <= 100 &&
		        (RES.OriginRect() & toRect) == toRect);
	}

	Game::ZoomMetrics Game::MakeZoomMetrics(Size2 size) const
	{
		ZoomMetrics metrics;
		metrics.from.size = size;
		if (auto m = GetMousePos())
		{
			auto vrect = RES.OriginRect();
			vrect.size -= metrics.from.size - Size2{ 1, 1 };
			metrics.from.pos = RobustClamp(*m - metrics.from.size / 2, vrect);
		}
		metrics.scale = zoomViewportMaxSize / std::max(metrics.from.size.X, metrics.from.size.Y);
		metrics.to = { 0, 0 };
		if (metrics.from.pos.X < RES.X / 2)
		{
			metrics.to.X = RES.X - (metrics.from.size.X * metrics.scale - 1);
		}
		return metrics;
	}

	bool Game::DragBrush(bool invokeToolDrag)
	{
		auto m = GetMousePos();
		auto *tool = toolSlots[drawState->toolSlotIndex];
		auto *brush = GetCurrentBrush();
		if (m && tool && brush)
		{
			auto p = ResolveZoom(*m);
			auto *sim = simulation.get();
			tool->Strength = GetToolStrength();
			switch (drawState->mode)
			{
			case DrawMode::free:
				tool->DrawLine(sim, *brush, drawState->initialMousePos, p, true);
				drawState->initialMousePos = p;
				return true;

			case DrawMode::fill:
				tool->DrawFill(sim, *brush, p);
				return true;

			case DrawMode::line:
				if (invokeToolDrag)
				{
					tool->Drag(sim, *brush, drawState->initialMousePos, p);
				}
				break;

			default:
				break;
			}
		}
		return false;
	}

	void Game::SetBrushIndex(BrushIndex newBrushIndex)
	{
		brushIndex = newBrushIndex;
		auto *brush = GetCurrentBrush();
		if (brush)
		{
			brush->SetRadius(brushRadius);
		}
	}

	void Game::BeginBrush(ToolSlotIndex toolSlotIndex, DrawMode drawMode)
	{
		auto p = GetSimMousePos();
		if (!p)
		{
			return;
		}
		auto affectedByZoom = bool(ResolveZoomIfAffected(*GetMousePos()));
		CreateHistoryEntry();
		auto *tool = toolSlots[toolSlotIndex];
		if (tool->Identifier == "DEFAULT_UI_SAMPLE")
		{
			// TODO: have tools decide on draw mode
			drawMode = DrawMode::free;
		}
		drawState = DrawState{ drawMode, toolSlotIndex, *p, affectedByZoom };
		auto *brush = GetCurrentBrush();
		if (tool && brush)
		{
			auto *sim = simulation.get();
			tool->Strength = GetToolStrength();
			switch (drawMode)
			{
			case DrawMode::free:
				tool->Draw(sim, *brush, *p);
				break;

			case DrawMode::fill:
				tool->DrawFill(sim, *brush, *p);
				break;

			default:
				break;
			}
		}
	}

	void Game::ResetDrawStateIfZoomChanged()
	{
		if (auto m = GetMousePos(); m && drawState)
		{
			auto affectedByZoom = bool(ResolveZoomIfAffected(*m));
			if (drawState->affectedByZoom != affectedByZoom)
			{
				// TODO-REDO_UI: fake mouseup
				drawState.reset();
			}
		}
	}

	void Game::EndBrush(ToolSlotIndex toolSlotIndex)
	{
		ResetDrawStateIfZoomChanged();
		if (drawState && drawState->toolSlotIndex == toolSlotIndex)
		{
			auto m = GetMousePos();
			auto *tool = toolSlots[toolSlotIndex];
			auto *brush = GetCurrentBrush();
			if (m && tool && brush)
			{
				auto p = ResolveZoom(*m);
				auto *sim = simulation.get();
				tool->Strength = GetToolStrength();
				switch (drawState->mode)
				{
				case DrawMode::free:
					tool->DrawLine(sim, *brush, drawState->initialMousePos, p, true);
					tool->Click(sim, *brush, p);
					break;

				case DrawMode::line:
					tool->DrawLine(sim, *brush, drawState->initialMousePos, p, false);
					break;

				case DrawMode::rect:
					tool->DrawRect(sim, *brush, drawState->initialMousePos, p);
					break;

				case DrawMode::fill:
					tool->DrawFill(sim, *brush, p);
					break;

				default:
					break;
				}
			}
			drawState.reset();
		}
	}

	Brush *Game::GetCurrentBrush()
	{
		if (brushIndex >= 0 && brushIndex < int32_t(brushes.size()))
		{
			return brushes[brushIndex].get();
		}
		return nullptr;
	}

	const Brush *Game::GetCurrentBrush() const
	{
		if (brushIndex >= 0 && brushIndex < int32_t(brushes.size()))
		{
			return brushes[brushIndex].get();
		}
		return nullptr;
	}

	const Brush *Game::GetBrush(BrushIndex index) const
	{
		if (index >= 0 && index < int32_t(brushes.size()))
		{
			return brushes[index].get();
		}
		return nullptr;
	}

	std::optional<Game::BrushIndex> Game::GetIndexFromBrush(const Brush *brush)
	{
		if (brush)
		{
			for (int32_t i = 0; i < int32_t(brushes.size()); ++i)
			{
				if (brushes[i].get() == brush)
				{
					return i;
				}
			}
		}
		return std::nullopt;
	}
}
