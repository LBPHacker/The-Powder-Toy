#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "simulation/elements/FILT.h"
#include "simulation/ElementClasses.h"
#include "simulation/Simulation.h"
#include "simulation/Sample.h"
#include "gui/game/tool/Tool.h"
#include "gui/game/Brush.h"
#include "graphics/RasterDrawMethodsImpl.h"
#include "common/RasterGeometry.h"
#include "Format.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size hudMargin      =              12;
		constexpr Gui::View::Size backdropMargin =               4;
		constexpr auto copyCutShadow             = 0x64000000_argb;

		Point CellFloor(Point point)
		{
			return point / CELL * CELL;
		}
	}

	void Game::GuiNotifications()
	{
		auto notifications = ScopedVPanel("notifications");
		SetPadding(hudMargin);
		SetSpacing(Common{});
		SetOrder(Gui::View::Order::bottomToTop);
		SetAlignment(Gui::Alignment::bottom);
		SetAlignmentSecondary(Gui::Alignment::right);
		int32_t notificationIndex = 0;
		for (auto &notification : notificationEntries)
		{
			auto panel = ScopedHPanel(notificationIndex);
			SetParentFillRatioSecondary(0);
			SetSize(Common{});
			SetSpacing(Common{});
			BeginButton("message", notification.message, Gui::View::ButtonFlags::autoWidth, Gui::colorYellow.WithAlpha(255), 0xFF000000_argb);
			SetParentFillRatio(0);
			if (EndButton() && notification.action)
			{
				notification.action();
			}
			BeginButton("dismiss", Gui::iconCross, Gui::View::ButtonFlags::none, Gui::colorYellow.WithAlpha(255), 0xFF000000_argb);
			SetSize(Common{});
			if (EndButton())
			{
				notificationEntries.erase(notificationEntries.begin() + notificationIndex);
				RequestRepeatFrame();
				break;
			}
			notificationIndex += 1;
		}
	}

	void Game::DrawHudLines(const std::vector<std::string> &lines, std::optional<int32_t> wavelengthGfx, Gui::Alignment alignment, Rgb8 color)
	{
		auto &g = GetHost();
		auto r = GetRect();
		int32_t y = hudMargin;
		int32_t alpha = 255;
		auto backdropAlpha = alpha * 170 / 255;
		int32_t lastBackdropWidth = 0;
		for (int32_t i = 0; i < int32_t(lines.size()); ++i)
		{
			auto it = g.InternText(lines[i]);
			auto st = g.ShapeText(Gui::ShapeTextParameters{ it, std::nullopt, alignment });
			auto &stw = g.GetShapedTextWrapper(st);
			auto contentSize = stw.GetContentSize();
			auto x = hudMargin;
			auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
			if (alignment == Gui::Alignment::right)
			{
				x = r.size.X - backdropSize.X - x;
			}
			if (backdropSize.X > lastBackdropWidth)
			{
				auto p = r.pos + Pos2(x, y);
				g.DrawLine(p, p + Size2(backdropSize.X - lastBackdropWidth - 1, 0), 0x000000_rgb .WithAlpha(backdropAlpha));
			}
			g.FillRect({ r.pos + Pos2(x, y + 1), backdropSize - Size2(0, 1) }, 0x000000_rgb .WithAlpha(backdropAlpha));
			lastBackdropWidth = backdropSize.X;
			g.DrawText(r.pos + Pos2(x + backdropMargin, y + backdropMargin), st, color.WithAlpha(alpha));
			if (i == 0 && wavelengthGfx)
			{
				for (int32_t b = 0; b < 30; ++b)
				{
					auto bit = 1 << b;
					auto color = 0x404040_rgb;
					if (*wavelengthGfx & bit)
					{
						auto spreadBit = (bit >> 2) |
						                 (bit >> 1) |
						                  bit       |
						                 (bit << 1) |
						                 (bit << 2);
						auto [ fr, fg, fb ] = Element_FILT_wavelengthsToColor(spreadBit);
						color = Rgb8(
							std::clamp(fr, 0, 255),
							std::clamp(fg, 0, 255),
							std::clamp(fb, 0, 255)
						);
					}
					auto p = r.pos + Pos2(r.size.X - contentSize.X + 10 - b, y - 2);
					g.DrawLine(p, p + Pos2(0, 2), color.WithAlpha(alpha));
				}
			}
			y += backdropSize.Y - 1;
		}
	}

	void Game::DrawLogLines()
	{
		auto &g = GetHost();
		auto r = GetRect();
		auto y = r.size.Y - hudMargin;
		for (auto i = int32_t(logEntries.size()) - 1; i >= 0; --i)
		{
			auto lineAlpha = std::min(logEntries[i].alpha.GetValue(), 255);
			auto it = g.InternText(logEntries[i].message);
			auto st = g.ShapeText(Gui::ShapeTextParameters{ it, std::nullopt, Gui::Alignment::left });
			auto &stw = g.GetShapedTextWrapper(st);
			auto contentSize = stw.GetContentSize();
			auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
			y -= backdropSize.Y;
			auto backdropAlpha = lineAlpha * 170 / 255;
			g.FillRect({ r.pos + Pos2(hudMargin, y), backdropSize }, 0x000000_rgb .WithAlpha(backdropAlpha));
			g.DrawText(r.pos + Pos2(hudMargin + backdropMargin, y + backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(lineAlpha));
			y -= 2;
		}
	}

	void Game::DrawToolTip()
	{
		if (!toolTipInfo)
		{
			return;
		}
		auto &g = GetHost();
		auto rh = GetRect().Inset(hudMargin);
		auto alpha = std::min(toolTipAlpha.GetValue(), 255);
		auto backdropAlpha = alpha * 170 / 255;
		auto it = g.InternText(toolTipInfo->message);
		auto st = g.ShapeText(Gui::ShapeTextParameters{ it, Gui::ShapeTextParameters::MaxWidth{ rh.size.X, false }, toolTipInfo->horizontalAlignment });
		auto &stw = g.GetShapedTextWrapper(st);
		auto contentSize = stw.GetContentSize();
		auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
		auto pr = rh;
		pr.size -= backdropSize - Size2{ 1, 1 };
		auto pos = (toolTipInfo->pos - Pos2(
			int32_t(toolTipInfo->horizontalAlignment) * backdropSize.X / 2,
			int32_t(toolTipInfo->verticalAlignment  ) * backdropSize.Y / 2
		));
		pos = RobustClamp(pos, pr);
		g.FillRect({ pos, backdropSize }, 0x000000_rgb .WithAlpha(backdropAlpha));
		g.DrawText(pos + Pos2(backdropMargin, backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(alpha));
	}

	void Game::DrawInfoTip()
	{
		if (!infoTipInfo)
		{
			return;
		}
		auto &g = GetHost();
		auto r = GetRect();
		auto alpha = std::min(infoTipAlpha.GetValue(), 255);
		auto backdropAlpha = alpha * 170 / 255;
		auto it = g.InternText(infoTipInfo->message);
		auto st = g.ShapeText(Gui::ShapeTextParameters{ it, std::nullopt, Gui::Alignment::left });
		auto &stw = g.GetShapedTextWrapper(st);
		auto contentSize = stw.GetContentSize();
		auto backdropSize = contentSize + Size2(2 * backdropMargin, 2 * backdropMargin);
		auto pos = r.pos + (r.size - backdropSize) / 2;
		g.FillRect({ pos, backdropSize }, 0x000000_rgb .WithAlpha(backdropAlpha));
		g.DrawText(pos + Pos2(backdropMargin, backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(alpha));
	}

	void Game::DrawIntroText()
	{
		auto alpha = std::min(introTextAlpha.GetValue(), 255);
		if (!alpha)
		{
			return;
		}
		auto &g = GetHost();
		auto r = GetRect();
		auto rh = r.Inset(hudMargin);
		auto backdropAlpha = alpha * 170 / 255;
		auto it = g.InternText(introText);
		auto st = g.ShapeText(Gui::ShapeTextParameters{ it, Gui::ShapeTextParameters::MaxWidth{ rh.size.X, true }, Gui::Alignment::left });
		g.FillRect(rh, 0x000000_rgb .WithAlpha(backdropAlpha));
		g.DrawText(rh.pos + Pos2(backdropMargin, backdropMargin), st, 0xFFFFFF_rgb .WithAlpha(alpha));
	}

	void Game::DrawBrush()
	{
		auto m = GetMousePos();
		if (!(m && lastSelectedTool))
		{
			return;
		}
		auto mCurr = ResolveZoom(*m);
		if (lastSelectedTool->Blocky)
		{
			mCurr = CellFloor(mCurr);
		}
		bool drawBrush = true;
		if (drawState)
		{
			auto mPrev = drawState->initialMousePos;
			if (lastSelectedTool->Blocky)
			{
				mPrev = CellFloor(mPrev);
			}
			switch (drawState->mode)
			{
			case DrawMode::line:
				// TODO-REDO_UI-POSTCLEANUP: still doesn't quite match Tool::DrawLine
				RasterizeLine<true>(mPrev, mCurr, [this](Pos2 p) {
					drawSimFrame.XorPixel(p);
				});
				break;

			case DrawMode::rect:
				{
					auto rect = ::RectBetween(mPrev, mCurr);
					if (lastSelectedTool->Blocky)
					{
						rect.size += Point(CELL - 1, CELL - 1);
					}
					RasterizeRect(rect, [this](Pos2 p) {
						drawSimFrame.XorPixel(p);
					});
					drawBrush = false;
				}
				break;

			default:
				break;
			}
		}
		auto *brush = GetCurrentBrush();
		if (!(drawBrush && brush))
		{
			return;
		}
		if (lastSelectedTool->Blocky)
		{
			auto cellBrushRadius = CellFloor(brushRadius);
			auto rect = ::RectBetween(mCurr - cellBrushRadius, mCurr + cellBrushRadius);
			rect.size += Point(CELL - 1, CELL - 1);
			RasterizeRect(rect, [this](Pos2 p) {
				drawSimFrame.XorPixel(p);
			});
		}
		else
		{
			for (auto p : brush->outline.Size().OriginRect())
			{
				if (brush->outline[p])
				{
					drawSimFrame.XorPixel(mCurr - brush->GetRadius() + p);
				}
			}
		}
	}

	void Game::DrawSelect()
	{
		if (!selectContext->active)
		{
			return;
		}
		auto p = GetSimMousePos();
		if (!p || !selectFirstPos)
		{
			drawSimFrame.BlendFilledRect(RES.OriginRect(), copyCutShadow);
			return;
		}
		auto rect = RectBetween(*selectFirstPos, *p);
		auto tl = rect.TopLeft();
		auto br = rect.BottomRight();
		drawSimFrame.BlendFilledRect(RectBetween(Pos2{        0,        0 }, Pos2{ XRES - 1, tl.Y - 1 }), copyCutShadow);
		drawSimFrame.BlendFilledRect(RectBetween(Pos2{        0, br.Y + 1 }, Pos2{ XRES    ,     YRES }), copyCutShadow);
		drawSimFrame.BlendFilledRect(RectBetween(Pos2{        0,     tl.Y }, Pos2{ tl.X - 1,     br.Y }), copyCutShadow);
		drawSimFrame.BlendFilledRect(RectBetween(Pos2{ br.X + 1,     tl.Y }, Pos2{ XRES - 1,     br.Y }), copyCutShadow);
		drawSimFrame.XorDottedRect(rect);
	}

	void Game::DrawZoomRect()
	{
		if (!zoomShown)
		{
			return;
		}
		RasterizeRect(zoomMetrics.from.Inset(-1), [this](Pos2 p) {
			drawSimFrame.XorPixel(p);
		});
	}

	void Game::GetBasicSampleText(std::vector<std::string> &lines, std::optional<int32_t> &wavelengthGfx, const SimulationSample &sample)
	{
		auto &sd = SimulationData::CRef();
		ByteStringBuilder sampleInfo;
		sampleInfo << Format::Precision(2);
		auto type = sample.particle.type;
		if (type)
		{
			auto ctype = sample.particle.ctype;
			if (type == PT_PHOT || type == PT_BIZR || type == PT_BIZRG || type == PT_BIZRS || type == PT_FILT || type == PT_BRAY || type == PT_C5)
			{
				wavelengthGfx = ctype & 0x3FFFFFFF;
			}
			if (debugHud)
			{
				if (type == PT_LAVA && sd.IsElement(ctype))
				{
					sampleInfo << "Molten " << sd.ElementResolve(ctype, 0).ToUtf8(); // TODO-REDO_UI-TRANSLATE
				}
				else if ((type == PT_PIPE || type == PT_PPIP) && sd.IsElement(ctype))
				{
					if (ctype == PT_LAVA && sd.IsElement(sample.particle.tmp4))
					{
						sampleInfo << sd.ElementResolve(type, 0).ToUtf8() << " with molten " << sd.ElementResolve(sample.particle.tmp4, -1).ToUtf8(); // TODO-REDO_UI-TRANSLATE
					}
					else
					{
						sampleInfo << sd.ElementResolve(type, 0).ToUtf8() << " with " << sd.ElementResolve(ctype, sample.particle.tmp4).ToUtf8(); // TODO-REDO_UI-TRANSLATE
					}
				}
				else if (type == PT_LIFE)
				{
					sampleInfo << sd.ElementResolve(type, ctype).ToUtf8();
				}
				else if (type == PT_FILT)
				{
					sampleInfo << sd.ElementResolve(type, ctype).ToUtf8();
					static const std::array<const char *, 12> filtModes = {{ // TODO-REDO_UI-TRANSLATE
						"set colour",
						"AND",
						"OR",
						"subtract colour",
						"red shift",
						"blue shift",
						"no effect",
						"XOR",
						"NOT",
						"old QRTZ scattering",
						"variable red shift",
						"variable blue shift",
					}};
					if (sample.particle.tmp >= 0 && sample.particle.tmp < int32_t(filtModes.size()))
					{
						sampleInfo << " (" << filtModes[sample.particle.tmp] << ")";
					}
					else
					{
						sampleInfo << " (unknown mode)"; // TODO-REDO_UI-TRANSLATE
					}
				}
				else
				{
					sampleInfo << sd.ElementResolve(type, ctype).ToUtf8();
					if (wavelengthGfx || type == PT_EMBR || type == PT_PRTI || type == PT_PRTO)
					{
						// Do nothing, ctype is meaningless for these elements
					}
					// Some elements store extra LIFE info in upper bits of ctype, instead of tmp/tmp2
					else if (type == PT_CRAY || type == PT_DRAY || type == PT_CONV || type == PT_LDTC)
					{
						sampleInfo << " (" << sd.ElementResolve(TYP(ctype), ID(ctype)).ToUtf8() << ")";
					}
					else if (type == PT_CLNE || type == PT_BCLN || type == PT_PCLN || type == PT_PBCN || type == PT_DTEC)
					{
						sampleInfo << " (" << sd.ElementResolve(ctype, sample.particle.tmp).ToUtf8() << ")";
					}
					else if (sd.IsElement(ctype) && type != PT_GLOW && type != PT_WIRE && type != PT_SOAP && type != PT_LITH)
					{
						sampleInfo << " (" << sd.ElementResolve(ctype, 0).ToUtf8() << ")";
					}
					else if (ctype)
					{
						sampleInfo << " (" << ctype << ")";
					}
				}
				sampleInfo << ", Temp: "; // TODO-REDO_UI-TRANSLATE
				StringBuilder sb;
				sb << Format::Precision(2);
				format::RenderTemperature(sb, sample.particle.temp, temperatureScale);
				sampleInfo << sb.Build().ToUtf8();
				sampleInfo << ", Life: " << sample.particle.life; // TODO-REDO_UI-TRANSLATE
				if (sample.particle.type != PT_RFRG && sample.particle.type != PT_RFGL && sample.particle.type != PT_LIFE)
				{
					if (sample.particle.type == PT_CONV)
					{
						if (sd.IsElement(TYP(sample.particle.tmp)))
						{
							sampleInfo << ", Tmp: " << sd.ElementResolve(TYP(sample.particle.tmp), ID(sample.particle.tmp)).ToUtf8(); // TODO-REDO_UI-TRANSLATE
						}
						else
						{
							sampleInfo << ", Tmp: " << sample.particle.tmp; // TODO-REDO_UI-TRANSLATE
						}
					}
					else
					{
						sampleInfo << ", Tmp: " << sample.particle.tmp; // TODO-REDO_UI-TRANSLATE
					}
				}
				// only elements that use .tmp2 show it in the debug HUD
				if (type == PT_CRAY || type == PT_DRAY || type == PT_EXOT || type == PT_LIGH || type == PT_SOAP || type == PT_TRON ||
				    type == PT_VIBR || type == PT_VIRS || type == PT_WARP || type == PT_LCRY || type == PT_CBNW || type == PT_TSNS ||
				    type == PT_DTEC || type == PT_LSNS || type == PT_PSTN || type == PT_LDTC || type == PT_VSNS || type == PT_LITH ||
				    type == PT_CONV || type == PT_ETRD)
				{
					sampleInfo << ", Tmp2: " << sample.particle.tmp2; // TODO-REDO_UI-TRANSLATE
				}
				sampleInfo << ", Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				sampleInfo << sd.BasicParticleInfo(sample.particle).ToUtf8();
				sampleInfo << ", Temp: "; // TODO-REDO_UI-TRANSLATE
				StringBuilder sb;
				sb << Format::Precision(2);
				format::RenderTemperature(sb, sample.particle.temp, temperatureScale);
				sampleInfo << sb.Build().ToUtf8();
				sampleInfo << ", Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
			}
		}
		else if (sample.WallType)
		{
			if (sample.WallType >= 0 && sample.WallType < int32_t(sd.wtypes.size()))
			{
				sampleInfo << sd.wtypes[sample.WallType].name.ToUtf8();
			}
			sampleInfo << ", Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
		}
		else if (sample.isMouseInSim)
		{
			sampleInfo << "Empty, Pressure: " << sample.AirPressure; // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			sampleInfo << "Empty"; // TODO-REDO_UI-TRANSLATE
		}
		lines.push_back(sampleInfo.Build());
	}

	void Game::GetDebugSampleText(std::vector<std::string> &lines, const SimulationSample &sample)
	{
		if (!debugHud)
		{
			return;
		}
		ByteStringBuilder sampleInfo;
		sampleInfo << Format::Precision(2);
		auto type = sample.particle.type;
		if (type)
		{
			sampleInfo << "#" << sample.ParticleID << ", ";
		}
		sampleInfo << "X:" << sample.PositionX << " Y:" << sample.PositionY;
		auto gravtot = std::abs(sample.GravityVelocityX) +
		               std::abs(sample.GravityVelocityY);
		if (gravtot)
		{
			sampleInfo << ", GX: " << sample.GravityVelocityX << " GY: " << sample.GravityVelocityY; // TODO-REDO_UI-TRANSLATE
		}
		if (GetAmbientHeat())
		{
			sampleInfo << ", AHeat: "; // TODO-REDO_UI-TRANSLATE
			StringBuilder sb;
			sb << Format::Precision(2);
			format::RenderTemperature(sb, sample.AirTemperature, temperatureScale);
			sampleInfo << sb.Build().ToUtf8();
		}
		lines.push_back(sampleInfo.Build());
	}

	void Game::GetFpsLines(std::vector<std::string> &lines, const SimulationSample &sample)
	{
		ByteStringBuilder fpsInfo;
		auto fps = 0; // TODO-REDO_UI
		fpsInfo << Format::Precision(2) << "FPS: " << fps; // TODO-REDO_UI-TRANSLATE
		if (debugHud)
		{
			if (rendererSettings.findingElement)
			{
				fpsInfo << " Parts: " << rendererStats.foundParticles << "/" << sample.NumParts; // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				fpsInfo << " Parts: " << sample.NumParts; // TODO-REDO_UI-TRANSLATE
			}
		}
		if ((std::holds_alternative<HdispLimitAuto>(rendererSettings.wantHdispLimitMin) ||
		     std::holds_alternative<HdispLimitAuto>(rendererSettings.wantHdispLimitMax)) && rendererStats.hdispLimitValid)
		{
			StringBuilder sb;
			sb << Format::Precision(2);
			sb << " [TEMP L:";
			format::RenderTemperature(sb, rendererStats.hdispLimitMin, temperatureScale);
			sb << " H:";
			format::RenderTemperature(sb, rendererStats.hdispLimitMax, temperatureScale);
			sb << "]";
			fpsInfo << sb.Build().ToUtf8();
		}
		if (simulation->replaceModeFlags & REPLACE_MODE)
		{
			fpsInfo << " [REPLACE MODE]"; // TODO-REDO_UI-TRANSLATE
		}
		if (simulation->replaceModeFlags & SPECIFIC_DELETE)
		{
			fpsInfo << " [SPECIFIC DELETE]"; // TODO-REDO_UI-TRANSLATE
		}
		if (rendererSettings.gridSize)
		{
			fpsInfo << " [GRID: " << rendererSettings.gridSize << "]"; // TODO-REDO_UI-TRANSLATE
		}
		if (rendererSettings.findingElement)
		{
			fpsInfo << " [FIND]"; // TODO-REDO_UI-TRANSLATE
		}
		// if (c->GetDebugFlags() & DEBUG_SIMHUD) // TODO-REDO_UI
		lines.push_back(fpsInfo.Build());
	}

	void Game::QueueToolTip(std::string message, Pos2 pos, Gui::Alignment horizontalAlignment, Gui::Alignment verticalAlignment)
	{
		toolTipInfo = { message, pos, horizontalAlignment, verticalAlignment };
		toolTipQueued = true;
	}

	void Game::QueueInfoTip(std::string message)
	{
		if (!infoTipsEnabled) // TODO-REDO_UI: wire up
		{
			return;
		}
		infoTipInfo = { message };
		infoTipAlpha.SetValue(400);
	}

	void Game::DismissIntroText()
	{
		if (introTextAlpha.GetValue() > 255)
		{
			introTextAlpha.SetValue(255);
		}
	}
}

template struct RasterDrawMethods<Powder::Activity::Game::DrawSimFrame>;
