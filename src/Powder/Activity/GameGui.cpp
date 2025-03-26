#include "Game.hpp"
#include "Config.h"
#include "Main.hpp"
#include "ColorPicker.hpp"
#include "ElementSearch.hpp"
#include "LocalBrowser.hpp"
#include "LocalSaver.hpp"
#include "Login.hpp"
#include "OnlineBrowser.hpp"
#include "OnlineSaver.hpp"
#include "Preview.hpp"
#include "Profile.hpp"
#include "Settings.hpp"
#include "StampBrowser.hpp"
#include "Tags.hpp"
#include "Votebars.hpp"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "client/http/ExecVoteRequest.h"
#include "common/platform/Platform.h"
#include "Gui/Host.hpp"
#include "Gui/Icons.hpp"
#include "Gui/SdlAssert.hpp"
#include "Gui/StaticTexture.hpp"
#include "gui/game/tool/Tool.h"
#include "graphics/Renderer.h"
#include "simulation/Simulation.h"
#include "simulation/SimulationData.h"
#include "simulation/Sample.h"
#include "simulation/ElementClasses.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr auto toolSlotColors = std::array<Rgb8, Game::toolSlotCount>({
			0xFF0000_rgb,
			0x0000FF_rgb,
			0x00FF00_rgb,
			0x00FFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
			0xFFFFFF_rgb,
		});
		constexpr auto decoSlotColor = 0xFF0000_rgb;
	}

	void Game::Gui()
	{
		auto game = ScopedVPanel("game");
		SetRootRect(GetHost().GetSize().OriginRect());
		{
			auto top = ScopedHPanel("top");
			{
				auto simTools = ScopedVPanel("simTools");
				{
					auto simLayers = ScopedVPanel("simLayers");
					SetLayered(true);
					GuiSim();
					GuiNotifications();
				}
				GuiTools();
			}
			GuiRight();
		}
		GuiBottom();
	}

	void Game::GuiHud()
	{
		if (!hud)
		{
			return;
		}
		// TODO-REDO_UI: remove vectors
		std::vector<std::string> sampleLines;
		std::vector<std::string> fpsLines;
		std::optional<int32_t> wavelengthGfx;
		auto m = GetMousePos();
		auto p = ResolveZoom(m ? *m : Pos2(-1, -1)); // TODO-REDO_UI-POSTCLEANUP: pass std::optional to GetSample
		auto sample = simulation->GetSample(p.X, p.Y);
		GetBasicSampleText(sampleLines, wavelengthGfx, sample);
		GetDebugSampleText(sampleLines, sample);
		GetFpsLines(fpsLines, sample);
		DrawHudLines(sampleLines, wavelengthGfx, Gui::Alignment::right, 0xFFFFFF_rgb);
		DrawHudLines(fpsLines, std::nullopt, Gui::Alignment::left, 0x20D8FF_rgb);
		DrawLogLines();
		DrawToolTip();
		DrawInfoTip();
		DrawIntroText();
		if (selectContext->active)
		{
			std::string tip;
			switch (*selectMode)
			{
			case SelectMode::copy : tip = "\boClick-and-drag to specify an area to copy then cut (right click = cancel)" ; break; // TODO-REDO_UI-TRANSLATE
			case SelectMode::cut  : tip = "\boClick-and-drag to specify an area to copy (right click = cancel)"          ; break; // TODO-REDO_UI-TRANSLATE
			case SelectMode::stamp: tip = "\boClick-and-drag to specify an area to create a stamp (right click = cancel)"; break; // TODO-REDO_UI-TRANSLATE
			}
			QueueToolTip(tip, { 0, YRES }, Gui::Alignment::left, Gui::Alignment::bottom);
		}
	}

	void Game::GuiZoom()
	{
		if (!zoomShown)
		{
			return;
		}
		auto &g = GetHost();
		auto prevClipRect = g.GetClipRect();
		g.SetClipRect(prevClipRect & RES.OriginRect());
		auto r = Rect{ zoomMetrics.to - Size2{ 1, 1 }, zoomMetrics.from.size * zoomMetrics.scale + Size2{ 1, 1 } };
		g.DrawRect(r          , 0xFF000000_argb);
		g.DrawRect(r.Inset(-1), 0xFFFFFFFF_argb);
		g.DrawRect(r.Inset(-2), 0xFF000000_argb);
		SDL_Rect toRect;
		toRect.x = r.pos.X;
		toRect.y = r.pos.Y;
		toRect.w = r.size.X - 1;
		toRect.h = r.size.Y - 1;
		SDL_Rect fromRect;
		fromRect.x = zoomMetrics.from.pos.X;
		fromRect.y = zoomMetrics.from.pos.Y;
		fromRect.w = zoomMetrics.from.size.X;
		fromRect.h = zoomMetrics.from.size.Y;
		SdlAssertZero(SDL_RenderCopy(g.GetRenderer(), simTexture, &fromRect, &toRect));
		for (int32_t x = 0; x < zoomMetrics.from.size.X; ++x)
		{
			g.DrawLine({ r.pos.X + x * zoomMetrics.scale, r.pos.Y }, { r.pos.X + x * zoomMetrics.scale, r.pos.Y + r.size.Y - 1 }, 0xFF000000_argb);
		}
		for (int32_t y = 0; y < zoomMetrics.from.size.Y; ++y)
		{
			g.DrawLine({ r.pos.X, r.pos.Y + y * zoomMetrics.scale }, { r.pos.X + r.size.X - 1, r.pos.Y + y * zoomMetrics.scale }, 0xFF000000_argb);
		}
		g.SetClipRect(prevClipRect);
	}

	void Game::GuiSimSigns()
	{
		SetCursor(std::nullopt);
		lastHoveredSign.reset();
		auto p = GetSimMousePos();
		if (!p)
		{
			return;
		}
		for (auto i = int32_t(simulation->signs.size()) - 1; i >= 0; i--)
		{
			auto &sign = simulation->signs[i];
			Rect rect{ { 0, 0 }, { 0, 0 } };
			sign.getDisplayText(simulation.get(), rect.pos.X, rect.pos.Y, rect.size.X, rect.size.Y);
			rect.size.X += 1; // TODO-REDO_UI-POSTCLEANUP: figure out why this is needed
			if (!rect.Contains(*p))
			{
				continue;
			}
			lastHoveredSign = i;
			auto split = sign.split();
			std::optional<ByteString> tip;
			switch (split.second)
			{
			case sign::Type::Save:
				SetCursor(Cursor::hand);
				tip = ByteString::Build("Go to save ID:", sign.text.Substr(3, split.first - 3).ToUtf8()); // TODO-REDO_UI-TRANSLATE
				break;

			case sign::Type::Thread:
				SetCursor(Cursor::hand);
				tip = ByteString::Build("Open forum thread ", sign.text.Substr(3, split.first - 3).ToUtf8(), " in browser"); // TODO-REDO_UI-TRANSLATE
				break;

			case sign::Type::Search:
				SetCursor(Cursor::hand);
				tip = ByteString::Build("Search for ", sign.text.Substr(3, split.first - 3).ToUtf8()); // TODO-REDO_UI-TRANSLATE
				break;

			case sign::Type::Button:
				SetCursor(Cursor::hand);
				break;

			default:
				break;
			}
			if (tip)
			{
				QueueToolTip(*tip, Pos2{ 0, RES.Y }, Gui::Alignment::left, Gui::Alignment::bottom);
			}
			return;
		}
	}

	void Game::GuiSim()
	{
		auto simPanel = ScopedPanel("sim");
		SetSize(RES.Y);
		SetSizeSecondary(RES.X);
		if (true) // TODO-REDO_UI: decide whether we should render
		{
			rendererSettings.gravityZonesEnabled = false;
			for (int32_t i = 0; i < toolSlotCount; ++i)
			{
				if (toolSlots[i] && toolSlots[i]->Identifier == "DEFAULT_WL_GRVTY")
				{
					rendererSettings.gravityZonesEnabled = true;
				}
			}
			if (GetRendererThreadEnabled())
			{
				StartRendererThread();
				WaitForRendererThread();
				rendererStats = renderer->GetStats();
				*rendererThreadResult = renderer->GetVideo();
				rendererFrame = rendererThreadResult.get();
				DispatchRendererThread();
			}
			else
			{
				PauseRendererThread();
				renderer->ApplySettings(rendererSettings);
				RenderSimulation(*simulation, true);
				rendererStats = renderer->GetStats();
				rendererFrame = &renderer->GetVideo();
			}
		}
		auto &g = GetHost();
		SDL_Rect rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = RES.X;
		rect.h = RES.Y;
		SdlAssertZero(SDL_RenderCopy(g.GetRenderer(), simTexture, nullptr, &rect));
		GuiZoom();
		GuiHud();
		GuiSimSigns();
	}

	bool Game::ClickSign(ToolSlotIndex toolSlotIndex)
	{
		if (!lastHoveredSign ||
		    *lastHoveredSign >= int32_t(simulation->signs.size()) || // lastHoveredSign may be one frame late
		    (toolSlots[toolSlotIndex] && toolSlots[toolSlotIndex]->Identifier == "DEFAULT_UI_SIGN"))
		{
			return false;
		}
		auto &sign = simulation->signs[*lastHoveredSign];
		auto split = sign.split();
		switch (split.second)
		{
		case sign::Type::Save:
			try
			{
				if (int id = sign.text.Substr(3, split.first - 3).ToNumber<int>())
				{
					OpenOnlinePreview(id, 0, false);
				}
			}
			catch (const std::runtime_error &)
			{
			}
			break;

		case sign::Type::Thread:
			Platform::OpenURI(ByteString::Build(SERVER, "/Discussions/Thread/View.html?Thread=", sign.text.Substr(3, split.first - 3).ToUtf8()));
			break;

		case sign::Type::Search:
			onlineBrowser->SetQuery({ 0, sign.text.Substr(3, split.first - 3).ToUtf8() });
			OpenOnlineBrowser();
			break;

		case sign::Type::Button:
			simulation->create_part(-1, sign.x, sign.y, PT_SPRK);
			break;

		default:
			break;
		}
		return true;
	}

	void Game::GuiTools()
	{
		if (shouldUpdateDecoTools)
		{
			UpdateDecoTools();
			shouldUpdateDecoTools = false;
		}
		if (shouldUpdateToolAtlas)
		{
			UpdateToolAtlas();
			shouldUpdateToolAtlas = false;
		}
		auto &g = GetHost();
		auto toolButtonsPanel = ScopedHPanel("toolButtons");
		SetMaxSizeSecondary(MaxSizeFitParent{});
		SetSize(23);
		if (GetShowingDecoTools())
		{
			auto decoRect = [&](Rect rect, Rgba8 color) {
				auto leftRect = rect;
				auto rightRect = rect;
				leftRect.size.X /= 2;
				rightRect.pos.X += leftRect.size.X;
				rightRect.size.X -= leftRect.size.X;
				auto &decoSlotBackground = extraToolTextures[int32_t(ExtraToolTexture::decoSlotBackground)];
				g.DrawStaticTexture(Rect{ decoSlotBackground.toolAtlasPos, rect.size }, rect, *toolAtlasTexture, 0xFFFFFFFF_argb);
				g.FillRect(leftRect, color.NoAlpha().WithAlpha(255));
				g.FillRect(rightRect, color);
			};
			{
				auto decoSlotsPanel = ScopedHPanel("decoSlots");
				SetMaxSize(MaxSizeFitParent{});
				SetPadding(10, 9, 1, 4);
				SetSpacing(1);
				SetAlignment(Gui::Alignment::left);
				SetOrder(Order::leftToRight);
				int32_t buttonIndex = 0;
				for (auto &decoSlot : decoSlots)
				{
					auto buttonComponent = ScopedComponent(buttonIndex);
					SetHandleButtons(true);
					SetSize(toolTextureDataSize.X + 4);
					SetSizeSecondary(toolTextureDataSize.Y + 4);
					auto rr = GetRect();
					auto rh = rr.Inset(2);
					decoRect(rh, decoSlot);
					g.DrawRect(rh, 0xFFFFFFFF_argb);
					if (decoSlot == decoColor || IsHovered())
					{
						g.DrawRect(rr, decoSlotColor.WithAlpha(255));
					}
					if (IsClicked(SDL_BUTTON_LEFT))
					{
						SetDecoColor(decoSlot);
					}
					buttonIndex += 1;
				}
			}
			auto colorPicker = ScopedHPanel("colorPicker");
			SetPadding(9, 9, 2, 5);
			SetParentFillRatio(0);
			auto button = ScopedComponent("button");
			SetHandleButtons(true);
			SetCursor(Cursor::hand);
			SetSize(toolTextureDataSize.Y + 2);
			// .Y below is correct, do not fix
			SetSizeSecondary(toolTextureDataSize.Y + 2);
			auto rr = GetRect();
			g.DrawRect(rr, 0xFFFFFFFF_argb);
			decoRect(rr.Inset(1), decoColor);
			if (IsClicked(SDL_BUTTON_LEFT))
			{
				PushAboveThis(std::make_shared<ColorPicker>(GetHost(), true, GetDecoColor(), [this](Rgba8 newDecoColor) {
					SetDecoColor(newDecoColor);
				}));
			}
		}
		auto toolsPanel = ScopedHPanel("tools");
		SetMaxSize(MaxSizeFitParent{});
		SetPadding(9, 10, 1, 4);
		SetSpacing(1);
		auto r = GetRect();
		auto overflow = GetOverflow();
		// r.size.X - 1 below is correct, do not fix
		auto scrollWidth = std::max(1, r.size.X - 1);
		auto scrollLeft = r.pos.X + 1;
		if (auto m = GetMousePos())
		{
			toolScroll = m->X;
		}
		auto scrollPosition = std::clamp(toolScroll - scrollLeft, 0, scrollWidth);
		SetScroll(-scrollPosition * overflow / scrollWidth);
		SetAlignment(Gui::Alignment::right);
		SetOrder(Order::rightToLeft);
		int32_t buttonIndex = 0;
		lastHoveredTool = nullptr;
		for (auto &info : tools)
		{
			if (!info)
			{
				continue;
			}
			auto inThisMenuSection = info->tool->MenuVisible && info->tool->MenuSection == activeMenuSection;
			auto shownAsFavorite = info->favorite && activeMenuSection == SC_FAVORITES;
			if (!(inThisMenuSection || shownAsFavorite))
			{
				continue;
			}
			auto buttonComponent = ScopedComponent(buttonIndex);
			if (GuiToolButton(*this, *info, *toolAtlasTexture, activeMenuSection != SC_FAVORITES))
			{
				QueueToolTip(info->tool->Description.ToUtf8(), r.pos + r.size, Gui::Alignment::right, Gui::Alignment::bottom); // TODO-REDO_UI-TRANSLATE
				lastHoveredTool = info->tool.get();
			}
			buttonIndex += 1;
		}
		{
			constexpr std::array<int32_t, 9> fadeout = {{ // * Gamma-corrected.
				255, 195, 145, 103, 69, 42, 23, 10, 3
			}};
			for (int32_t i = 0; i < int32_t(fadeout.size()); ++i)
			{
				// r.size.X below is correct, do not fix
				g.DrawLine(r.pos + Pos2(r.size.X - i, 0), r.pos + Pos2(r.size.X - i, r.size.Y), 0x000000_rgb .WithAlpha(fadeout[i]));
				g.DrawLine(r.pos + Pos2(           i, 0), r.pos + Pos2(           i, r.size.Y), 0x000000_rgb .WithAlpha(fadeout[i]));
			}
		}
		if (overflow)
		{
			auto contentWidth = overflow + scrollWidth;
			auto barSize = scrollWidth * scrollWidth / contentWidth;
			auto barMaxOffset = scrollWidth - barSize;
			auto barOffset = scrollPosition * barMaxOffset / scrollWidth;
			g.FillRect({ Pos2(scrollLeft + barOffset, r.pos.Y + r.size.Y - 2), Pos2(barSize, 2) }, 0xFFC8C8C8_argb);
		}
	}

	void Game::GuiQuickOptions()
	{
		auto quickOptions = ScopedVPanel("quickOptions");
		SetPadding(1);
		SetSpacing(1);
		SetAlignment(Gui::Alignment::top);
		auto quickOption = [this](ComponentKey key, StringView text, ButtonFlags buttonFlags, StringView toolTip) {
			BeginButton(key, text, buttonFlags);
			SetSize(15);
			if (IsHovered())
			{
				auto r = GetRect();
				QueueToolTip(std::string(toolTip.view), r.pos + Pos2(0, r.size.Y / 2), Gui::Alignment::right, Gui::Alignment::center);
			}
			return EndButton();
		};
		if (quickOption("prettyPowder", "P", GetSandEffect() ? ButtonFlags::stuck : ButtonFlags::none, "Enable pretty powders")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleSandEffectAction();
		}
		if (quickOption("drawGravity", "G", GetDrawGravity() ? ButtonFlags::stuck : ButtonFlags::none, "Draw gravity field")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleDrawGravityAction();
		}
		if (quickOption("decorations", "D", GetDrawDeco() ? ButtonFlags::stuck : ButtonFlags::none, "Draw decorations")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleDrawDecoAction();
		}
		if (quickOption("newtonianGravity", "N", GetNewtonianGravity() ? ButtonFlags::stuck : ButtonFlags::none, "Enable newtonian gravity")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleNewtonianGravityAction();
		}
		if (quickOption("ambientHeat", "A", GetAmbientHeat() ? ButtonFlags::stuck : ButtonFlags::none, "Enable ambient heat")) // TODO-REDO_UI-TRANSLATE
		{
			ToggleAmbientHeatAction();
		}
		if (quickOption("openConsole", "C", ButtonFlags::none, "Open console")) // TODO-REDO_UI-TRANSLATE
		{
			OpenConsoleAction();
		}
	}

	void Game::GuiMenuSections()
	{
		auto toolTip = [this](std::string &&message) {
			if (IsHovered())
			{
				auto r = GetRect();
				QueueToolTip(std::forward<std::string>(message), r.pos + Pos2(0, r.size.Y / 2), Gui::Alignment::right, Gui::Alignment::center);
			}
		};
		auto menuSections = ScopedVPanel("menuSections");
		SetParentFillRatio(0);
		SetPadding(1, 0, 1, 1);
		SetSpacing(1);
		auto &sd = SimulationData::CRef();
		static const std::array<StringView, 16> iconOverrides = {{ // TODO-REDO_UI-POSTCLEANUP: move this where menu sections are defined
			StringView(Gui::iconWalls),
			StringView(Gui::iconElectronic),
			StringView(Gui::iconPowered),
			StringView(Gui::iconSensor),
			StringView(Gui::iconForce),
			StringView(Gui::iconExplosive),
			StringView(Gui::iconGas),
			StringView(Gui::iconLiquid),
			StringView(Gui::iconPowder),
			StringView(Gui::iconSolid),
			StringView(Gui::iconRadioactive),
			StringView(Gui::iconStar),
			StringView(Gui::iconGol),
			StringView(Gui::iconTool),
			StringView(Gui::iconFavorite),
			StringView(Gui::iconDeco),
		}};
		Assert(sd.msections.size() == iconOverrides.size()); // TODO-REDO_UI-POSTCLEANUP: constexpr, but see previous TODO
		for (int32_t i = 0; i < int32_t(sd.msections.size()); ++i)
		{
			BeginButton(i, iconOverrides[i], activeMenuSection == i ? ButtonFlags::stuck : ButtonFlags::none);
			SetSize(15);
			SetTextPadding(0);
			toolTip(sd.msections[i].name.ToUtf8()); // TODO-REDO_UI-TRANSLATE
			auto hovered = IsHovered();
			auto activate = EndButton();
			if (!(i == SC_DECO || menuSectionsNeedClick))
			{
				activate = hovered;
			}
			if (activate && activeMenuSection != i)
			{
				activeMenuSectionBeforeDeco = activeMenuSection;
				activeMenuSection = i;
				RequestRepeatFrame();
			}
		}
		{
			BeginButton("search", Gui::iconSearch2, ButtonFlags::none);
			SetSize(15);
			toolTip("Search for elements"); // TODO-REDO_UI-TRANSLATE
			if (EndButton())
			{
				OpenElementSearch();
			}
		}
	}

	void Game::GuiRight()
	{
		auto right = ScopedVPanel("right");
		SetSize(17);
		GuiQuickOptions();
		GuiMenuSections();
	}

	void Game::GuiSave()
	{
		auto user = Client::Ref().GetAuthUser();
		BeginButton("openSim", Gui::iconOpen, saveButtonsAltFunction ? ButtonFlags::stuck : ButtonFlags::none);
		SetSize(17);
		if (EndButton())
		{
			if (saveButtonsAltFunction)
			{
				OpenLocalBrowser();
			}
			else
			{
				OpenOnlineBrowser();
			}
		}
		auto *onlineSave = (save && std::holds_alternative<OnlineSave>(*save)) ? std::get<OnlineSave>(*save).get() : nullptr;
		auto *localSave  = (save && std::holds_alternative<LocalSave >(*save)) ? std::get<LocalSave >(*save).get() : nullptr;
		auto ownSave = user && onlineSave && onlineSave->IsOwn();
		BeginButton("reloadSim", Gui::iconReload, (saveButtonsAltFunction && onlineSave) ? ButtonFlags::stuck : ButtonFlags::none);
		SetEnabled(bool(save));
		SetSize(17);
		if (IsClicked(SDL_BUTTON_RIGHT))
		{
			OpenOnlinePreview();
		}
		if (EndButton())
		{
			if (saveButtonsAltFunction)
			{
				OpenOnlinePreview();
			}
			else
			{
				ReloadSimAction();
			}
		}
		{
			constexpr Size overwriteWidth =  19;
			constexpr Size renameWidth    = 132;
			ByteString buttonText = "[untitled simulation]"; // TODO-REDO_UI-POSTCLEANUP: StringView // TODO-REDO_UI-TRANSLATE
			ByteString title;
			bool merge = false;
			if (onlineSave)
			{
				title = onlineSave->GetName().ToUtf8();
				buttonText = title;
				SetTitle(title);
				if (!ownSave && !saveButtonsAltFunction)
				{
					merge = true;
				}
			}
			else if (localSave)
			{
				title = localSave->GetDisplayName().ToUtf8();
				buttonText = title;
				SetTitle(title);
			}
			else
			{
				SetTitle(buttonText);
				buttonText = "\bg" + buttonText;
				merge = true;
			}
			auto saveButtonFlags = saveButtonsAltFunction ? ButtonFlags::stuck : ButtonFlags::none;
			struct OpenInfo
			{
				bool overwrite;
			};
			std::optional<OpenInfo> openInfo;
			if (merge)
			{
				BeginButton("saveSim", ByteString::Build(Gui::iconSave, "\u0011\u0005", buttonText), saveButtonFlags | ButtonFlags::truncateText); // TODO-REDO_UI-TRANSLATE
				SetSize(overwriteWidth + renameWidth - 1);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				if (EndButton())
				{
					openInfo = OpenInfo{ false };
				}
			}
			else
			{
				BeginButton("overwriteSim", Gui::iconSave, saveButtonFlags);
				SetSize(overwriteWidth);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				if (EndButton())
				{
					openInfo = OpenInfo{ true };
				}
				AddSpacing(-2);
				BeginButton("renameSim", title, saveButtonFlags | ButtonFlags::truncateText);
				SetSize(renameWidth);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				if (EndButton())
				{
					openInfo = OpenInfo{ false };
				}
			}
			if (openInfo)
			{
				auto gameSave = simulation->Save(GetIncludePressure(), RES.OriginRect());
				gameSave->paused = GetSimPaused();
				if (saveButtonsAltFunction || !user)
				{
					auto saveFile = std::make_unique<SaveFile>(title);
					if (localSave)
					{
						saveFile->SetFileName(localSave->GetName());
						saveFile->SetDisplayName(localSave->GetDisplayName());
					}
					saveFile->SetGameSave(std::move(gameSave));
					PushAboveThis(std::make_shared<LocalSaver>(*this, std::move(saveFile), openInfo->overwrite));
				}
				else
				{
					std::unique_ptr<SaveInfo> saveInfo;
					if (onlineSave)
					{
						saveInfo = onlineSave->CloneInfo();
					}
					else
					{
						saveInfo = std::make_unique<SaveInfo>(0, 0, 0, 0, 0, user->Username, title.FromUtf8());
					}
					saveInfo->SetGameSave(std::move(gameSave));
					PushAboveThis(std::make_shared<OnlineSaver>(*this, std::move(saveInfo), openInfo->overwrite));
				}
			}
		}
		{
			constexpr Size upvoteWidth   = 39;
			constexpr Size downvoteWidth = 15;
			if (execVoteRequest)
			{
				BeginButton("cancelVote", "", ButtonFlags::none);
				SetSize(upvoteWidth + downvoteWidth - 1);
				Spinner("spinner", 11);
				if (EndButton())
				{
					execVoteRequest.reset(); // TODO-REDO_UI: notification
				}
			}
			else
			{
				auto upvoted = onlineSave && onlineSave->GetVote() == 1;
				BeginButton("upvote", ByteString::Build(Gui::iconUpvote, "\u0011\u0003Vote"), ButtonFlags::none, voteUpBar, upvoted ? voteUpBackground : 0x00000000_argb); // TODO-REDO_UI-TRANSLATE
				SetEnabled(bool(onlineSave) && !ownSave);
				SetSize(upvoteWidth);
				if (EndButton())
				{
					BeginVote(upvoted ? 0 : 1);
				}
				AddSpacing(-2);
				auto downvoted = onlineSave && onlineSave->GetVote() == -1;
				BeginButton("downvote", Gui::iconDownvote, ButtonFlags::none, voteDownBar, downvoted ? voteDownBackground : 0x00000000_argb);
				SetEnabled(bool(onlineSave) && !ownSave);
				SetSize(downvoteWidth);
				if (EndButton())
				{
					BeginVote(downvoted ? 0 : -1);
				}
			}
		}
		{
			ByteStringBuilder sb;
			sb << Gui::iconTag << "\u0011\u0005";
			auto tags = onlineSave ? &onlineSave->GetTags() : nullptr;
			if (!tags || tags->empty())
			{
				sb << "\bg[no tags set]"; // TODO-REDO_UI-TRANSLATE
			}
			else
			{
				bool first = true;
				for (auto &tag : *tags)
				{
					if (first)
					{
						first = false;
					}
					else
					{
						sb << " ";
					}
					sb << tag;
				}
			}
			BeginButton("tags", sb.Build(), ButtonFlags::truncateText);
		}
		SetEnabled(bool(onlineSave));
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		if (EndButton())
		{
			PushAboveThis(std::make_shared<Tags>(GetHost(), *onlineSave));
		}
		if (Button("clear", ByteString::Build(" ", Gui::iconNew, " "), 17)) // TODO-REDO_UI-POSTCLEANUP: remove padding when icon alignment has been fixed
		{
			ClearSim();
		}
		ByteString username = "\bg[sign in]"; // TODO-REDO_UI-TRANSLATE
		if (user)
		{
			username = user->Username;
		}
		BeginButton("user", ByteString::Build(Gui::iconKeyOutline, "\u0011\u0005", username), ButtonFlags::truncateText);
		SetSize(92);
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		if (EndButton())
		{
			if (user)
			{
				PushAboveThis(std::make_shared<Profile>(GetHost(), user->Username));
			}
			else
			{
				PushAboveThis(std::make_shared<Login>(*this));
			}
		}
		BeginButton("settings", Gui::iconCheckmark, ButtonFlags::none);
		SetSize(15);
		SetTextPadding(0);
		if (EndButton())
		{
			PushAboveThis(std::make_shared<Settings>(*this));
		}
	}

	void Game::GuiRenderer()
	{
		auto rendererSettingsPanel = ScopedHPanel("rendererSettings");
		SetSpacing(1);
		enum class BitPolicy
		{
			independent,
			exclusive,
			subset,
		};
		auto r = GetRect();
		auto group = [this, r](uint32_t componentKeyBase, auto &&groupItems, auto &mode, BitPolicy bitPolicy) {
			uint32_t newMode = mode;
			if (bitPolicy == BitPolicy::subset)
			{
				newMode = 0;
			}
			for (int32_t i = 0; i < int32_t(groupItems.size()); ++i)
			{
				auto &info = groupItems[i];
				bool stuck = false;
				switch (bitPolicy)
				{
				case BitPolicy::exclusive:
					stuck = mode == info.mode;
					break;

				case BitPolicy::subset:
					stuck = (mode & info.mode) == info.mode;
					break;

				case BitPolicy::independent:
					stuck = mode & info.mode;
					break;
				}
				BeginButton(componentKeyBase + i, info.icon, stuck ? ButtonFlags::stuck : ButtonFlags::none);
				if (IsHovered())
				{
					QueueToolTip(std::string(info.toolTip), r.pos + r.size, Gui::Alignment::right, Gui::Alignment::bottom);
				}
				if (EndButton())
				{
					switch (bitPolicy)
					{
					case BitPolicy::exclusive:
						newMode = stuck ? 0 : info.mode;
						break;

					case BitPolicy::subset:
						stuck = !stuck;
						break;

					case BitPolicy::independent:
						newMode ^= info.mode;
						break;
					}
				}
				if (bitPolicy == BitPolicy::subset && stuck)
				{
					newMode |= info.mode;
				}
			}
			Separator(componentKeyBase + 100, 3);
			mode = newMode;
		};
		struct GroupItem
		{
			uint32_t mode;
			StringView icon;
			StringView toolTip;
		};
		static const std::array<GroupItem, 7> renderModes = {{
			{ RENDER_EFFE, Gui::iconFlare      , "Adds Special flare effects to some elements"                  }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_FIRE, Gui::iconFireOutline, "Fire effect for gasses"                                       }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_GLOW, Gui::iconGlow       , "Glow effect on some elements"                                 }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_BLUR, Gui::iconLiquid     , "Blur effect for liquids"                                      }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_BLOB, Gui::iconBlob       , "Makes everything be drawn like a blob"                        }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_BASC, Gui::iconBasic      , "Basic rendering, without this, most things will be invisible" }, // TODO-REDO_UI-TRANSLATE
			{ RENDER_SPRK, Gui::iconFlare      , "Glow effect on sparks"                                        }, // TODO-REDO_UI-TRANSLATE
		}};
		group(1000, renderModes, rendererSettings.renderMode, BitPolicy::subset);
		static const std::array<GroupItem, 4> airDisplayModes = {{
			{ DISPLAY_AIRC, Gui::iconFanBlades      , "Displays pressure as red and blue, and velocity as white"                                                   }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_AIRP, Gui::iconSensor         , "Displays pressure, red is positive and blue is negative"                                                    }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_AIRV, Gui::iconWind           , "Displays velocity and positive pressure: up/down adds blue, right/left adds red, still pressure adds green" }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_AIRH, Gui::iconTemperatureBody, "Displays the temperature of the air like heat display does"                                                 }, // TODO-REDO_UI-TRANSLATE
		}};
		group(2000, airDisplayModes, rendererSettings.displayMode, BitPolicy::exclusive);
		static const std::array<GroupItem, 3> displayModes = {{
			{ DISPLAY_WARP, Gui::iconLensing, "Gravity lensing, Newtonian Gravity bends light with this on"    }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_EFFE, Gui::iconFlare  , "Enables moving solids, stickmen guns, and premium(tm) graphics" }, // TODO-REDO_UI-TRANSLATE
			{ DISPLAY_PERS, Gui::iconPaths  , "Element paths persist on the screen for a while"                }, // TODO-REDO_UI-TRANSLATE
		}};
		group(3000, displayModes, rendererSettings.displayMode, BitPolicy::independent);
		static const std::array<GroupItem, 4> colorModes = {{
			{ COLOUR_HEAT, Gui::iconTemperatureBody, "Displays temperatures of the elements, dark blue is coldest, pink is hottest" }, // TODO-REDO_UI-TRANSLATE
			{ COLOUR_LIFE, Gui::iconScale          , "Displays the life value of elements in greyscale gradients"                   }, // TODO-REDO_UI-TRANSLATE
			{ COLOUR_GRAD, Gui::iconGradient       , "Changes colors of elements slightly to show heat diffusing through them"      }, // TODO-REDO_UI-TRANSLATE
			{ COLOUR_BASC, Gui::iconBasic          , "No special effects at all for anything, overrides all other options and deco" }, // TODO-REDO_UI-TRANSLATE
		}};
		group(4000, colorModes, rendererSettings.colorMode, BitPolicy::exclusive);
		static const std::array<StringView, 12> rendererPresetIcons = {{
			Gui::iconFanBlades,
			Gui::iconWind,
			Gui::iconSensor,
			Gui::iconPaths,
			Gui::iconFireOutline,
			Gui::iconBlob,
			Gui::iconTemperatureBody,
			Gui::iconLiquid,
			Gui::iconBasic,
			Gui::iconGradient,
			Gui::iconScale,
			Gui::iconTemperatureBody,
		}};
		{
			uint32_t componentKeyBase = 5000;
			for (int32_t i = 0; i < int32_t(rendererPresetIcons.size()); ++i)
			{
				BeginButton(componentKeyBase + i, rendererPresetIcons[i], ButtonFlags::none);
				if (IsHovered())
				{
					QueueToolTip(Renderer::renderModePresets[i].Name.ToUtf8(), r.pos + r.size, Gui::Alignment::right, Gui::Alignment::bottom);
				}
				if (EndButton())
				{
					UseRendererPreset(i);
				}
			}
			Separator(componentKeyBase + 100, 3);
		}
	}

	void Game::GuiBottom()
	{
		auto bottom = ScopedHPanel("bottom");
		SetSize(17);
		SetPadding(1);
		SetSpacing(1);
		{
			if (showRenderer)
			{
				GuiRenderer();
			}
			else
			{
				GuiSave();
			}
			BeginButton("toggleShowRenderer", ByteString::Build(
				"\u0012\u000F\u00FF\u0001\u0001", // TODO-REDO_UI-POSTCLEANUP: figure out nulls
				Gui::iconColors1,
				"\u0011\u00FB\u000F\u0001\u00FF\u0001",
				Gui::iconColors2,
				"\u0011\u00F8\u000F\u0001\u0001\u00FF",
				Gui::iconColors3
			), showRenderer ? ButtonFlags::stuck : ButtonFlags::none);
			SetSize(15);
			SetTextPadding(0);
			if (EndButton())
			{
				showRenderer = !showRenderer;
			}
			if (Button("togglePause", Gui::iconPause, 15, GetSimPaused() ? ButtonFlags::stuck : ButtonFlags::none))
			{
				ToggleSimPausedAction();
			}
		}
	}

	bool Game::GuiToolButton(Gui::View &view, const GameToolInfo &info, Gui::StaticTexture &externalToolAtlasTexture, bool indicateFavorite) const
	{
		auto &g = view.GetHost();
		view.SetSize(toolTextureDataSize.X + 4);
		view.SetSizeSecondary(toolTextureDataSize.Y + 4);
		auto rr = view.GetRect();
		if (info.texture)
		{
			g.DrawStaticTexture(Rect{
				info.texture->toolAtlasPos,
				toolTextureDataSize,
			}, Rect{
				rr.pos + Pos2{ 2, 2 },
				toolTextureDataSize,
			}, externalToolAtlasTexture, 0xFFFFFFFF_argb);
		}
		else
		{
			g.FillRect(rr.Inset(2), info.tool->Colour.WithAlpha(255));
			view.BeginText(0, info.tool->Name.ToUtf8(), TextFlags::none, GetContrastColor2(info.tool->Colour).WithAlpha(255)); // TODO-REDO_UI-TRANSLATE: maybe
			view.SetTextPadding(4, 0); // Triggers the weird negative horizontal alignment behaviour in View::BeginText, see TruncateDiv explanation there.
			view.EndText();
		}
		bool selected = false;
		for (int32_t i = 0; i < toolSlotCount; ++i)
		{
			if (toolSlots[i] == info.tool.get())
			{
				g.DrawRect(rr, toolSlotColors[i].WithAlpha(255));
				selected = true;
				break;
			}
		}
		auto hovered = view.IsHovered();
		if (hovered)
		{
			if (!selected)
			{
				g.DrawRect(rr, toolSlotColors[0].WithAlpha(255));
			}
		}
		if (indicateFavorite && info.favorite)
		{
			g.DrawText(rr.pos - Pos2{ 2, 0 }, Gui::iconFavorite, Gui::colorYellow.WithAlpha(255));
		}
		return hovered;
	}

	void Game::BeginVote(int32_t vote)
	{
		auto *onlineSave = save ? std::get_if<OnlineSave>(&*save) : nullptr;
		if (!onlineSave)
		{
			return;
		}
		execVoteRequest = std::make_unique<http::ExecVoteRequest>((*onlineSave)->GetID(), vote);
		execVoteRequest->Start();
	}

	void Game::OpenOnlinePreview()
	{
		auto *onlineSave = save ? std::get_if<OnlineSave>(&*save) : nullptr;
		if (!onlineSave)
		{
			VisualLog("No online preview to show"); // TODO-REDO_UI-TRANSLATE
			return;
		}
		auto &saveInfo = *onlineSave;
		OpenOnlinePreview(saveInfo->GetID(), saveInfo->Version, false);
	}

	void Game::OpenOnlinePreview(int32_t id, int32_t version, bool quickOpen)
	{
		auto preview = std::make_shared<Preview>(*this, id, version, quickOpen, std::nullopt);
		preview->SetGameStack(gameStack);
		preview->SetStacks(stacks);
		PushAboveThis(preview);
	}

	void Game::OpenElementSearch()
	{
		PushAboveThis(std::make_shared<ElementSearch>(*this));
	}

	void Game::OpenOnlineBrowser()
	{
		stacks->SelectStack(onlineBrowserStack);
		onlineBrowser->Open();
	}

	void Game::OpenLocalBrowser()
	{
		stacks->SelectStack(localBrowserStack);
		localBrowser->Open();
	}

	void Game::OpenStampBrowser()
	{
		stacks->SelectStack(stampBrowserStack);
		stampBrowser->Open();
	}

	void Game::SetRendererUp(bool newRendererUp)
	{
		if (newRendererUp)
		{
			auto &g = GetHost();
			simTexture = SdlAssertPtr(SDL_CreateTexture(g.GetRenderer(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RES.X, RES.Y));
			DrawSim();
		}
		else
		{
			SDL_DestroyTexture(simTexture);
			simTexture = nullptr;
		}
	}
}
