#include "Saver.hpp"
#include "BrowserCommon.hpp"
#include "Game.hpp"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/StaticTexture.hpp"
#include "Simulation/RenderThumbnail.hpp"
#include "prefs/GlobalPrefs.h"
#include "client/GameSave.h"
#include "graphics/VideoBuffer.h"

namespace Powder::Activity
{
	Saver::Saver(Game &newGame, const GameSave &newGameSave, bool newOverwrite) :
		View(newGame.GetHost()),
		game(newGame),
		threadPool(newGame.GetThreadPool()),
		gameSave(newGameSave),
		interactiveThumbnail(GlobalPrefs::Ref().Get("InteractiveSaveThumbnail", false)) // TODO-REDO_UI: think of a better name
	{
		UpdateItemTitle();
		thumbnailFuture = Simulation::RenderThumbnail(
			threadPool,
			std::make_unique<GameSave>(gameSave),
			GetOriginalThumbnailSize(),
			RendererSettings::decorationEnabled, // TODO-REDO_UI-FUTURE: decide if this is appropriate for local saves
			false
		);
	}

	Saver::~Saver() = default;

	void Saver::UpdateItemTitle()
	{
		if (title.empty())
		{
			item.title = "\bg[save title]"; // TODO-REDO_UI-TRANSLATE
		}
		else
		{
			item.title = title;
		}
	}

	void Saver::Gui()
	{
		auto &g = GetHost();
		g.DrawStaticTexture(Point{ 0, 0 }, *backgroundImage, 0xFFFFFFFF_argb);
		auto saver = ScopedDialog("saver", GuiTitle());
		SetPadding(0);
		SetSpacing(0);
		auto row = ScopedHPanel("row");
		{
			auto left = ScopedVPanel("left");
			{
				auto input = ScopedVPanel("input");
				SetPadding(Common{});
				BeginTextbox("title", title, "[save title]", TextboxFlags::none); // TODO-REDO_UI-TRANSLATE
				SetSize(g.GetCommonMetrics().size); // TODO-REDO_UI-TRANSLATE
				if (focusTitle)
				{
					GiveInputFocus();
					TextboxSelectAll();
					focusTitle = false;
				}
				if (EndTextbox()) // TODO-REDO_UI-TRANSLATE
				{
					UpdateItemTitle();
				}
			}
			Separator("buttonSeparator");
			auto thumbnailPanel = ScopedVPanel("thumbnail");
			SetPadding(Common{});
			SetSpacing(Common{});
			if (interactiveThumbnail)
			{
				auto saveButtonLayers = ScopedHPanel("saveButtonLayers");
				SetSize(RES.Y / 3 + 42);
				SetSizeSecondary(RES.X / 3 + 20);
				auto r = GetRect();
				SetLayered(true);
				std::optional<Rect> zoomThumbnailRect;
				{
					auto cell = ScopedComponent("cell");
					auto saveButtonSize = GetSaveButtonSize();
					SetSize(saveButtonSize.X);
					SetSizeSecondary(saveButtonSize.Y);
					auto clickable = ScopedVPanel("clickable");
					SetCursor(Cursor::hand);
					SetParentFillRatio(0);
					SetParentFillRatioSecondary(0);
					SetAlignmentSecondary(Gui::Alignment::center);
					auto savebutton = ScopedVPanel("savebutton");
					zoomThumbnailRect = GuiSaveButton(*this, item, false).first;
				}
				auto saveButtonZoom = ScopedVPanel("saveButtonZoom");
				if (zoomThumbnailRect)
				{
					GuiSaveButtonZoom(*this, r, *zoomThumbnailRect, item);
				}
			}
			else
			{
				auto inner = ScopedComponent("inner");
				auto thumbnailSize = GetOriginalThumbnailSize();
				SetSize(thumbnailSize.Y);
				SetSizeSecondary(thumbnailSize.X);
				auto r = GetRect();
				if (std::holds_alternative<ThumbnailLoading>(item.thumbnail))
				{
					Spinner("loading", g.GetCommonMetrics().bigSpinner);
				}
				else if (auto *thumbnailData = std::get_if<ThumbnailData>(&item.thumbnail))
				{
					g.DrawStaticTexture(r, *thumbnailData->originalTexture, 0xFFFFFFFF_argb);
				}
				else
				{
					auto &thumbnailError = std::get<ThumbnailError>(item.thumbnail);
					BeginText("error", ByteString::Build(Gui::iconBrokenImage, "\n", thumbnailError.message), Gui::View::TextFlags::multiline);
					EndText();
				}
				g.DrawRect(r, 0xFFFFFFFF_argb);
			}
			if (Checkbox("checkbox", "Interactive thumbnail preview", interactiveThumbnail)) // TODO-REDO_UI-TRANSLATE
			{
				GlobalPrefs::Ref().Set("InteractiveSaveThumbnail", interactiveThumbnail);
				focusTitle = true;
			}
		}
		GuiRight();
	}

	Saver::DispositionFlags Saver::GetDisposition() const
	{
		if (title.empty())
		{
			return DispositionFlags::okDisabled;
		}
		return DispositionFlags::none;
	}

	void Saver::Ok()
	{
		Save();
	}

	void Saver::GuiRight()
	{
	}

	void Saver::HandleTick()
	{
		if (thumbnailFuture.valid() && thumbnailFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			auto original = thumbnailFuture.get();
			auto small = std::make_unique<VideoBuffer>(*original);
			original->Resize(GetOriginalThumbnailSize(), true);
			small->Resize(GetSmallThumbnailSize(), true);
			item.thumbnail = ThumbnailData{
				std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(original)),
				std::make_unique<Gui::StaticTexture>(GetHost(), false, std::move(small)),
			};
		}
	}
}
