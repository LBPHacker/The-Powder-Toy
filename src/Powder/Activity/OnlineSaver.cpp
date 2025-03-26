#include "OnlineSaver.hpp"
#include "Game.hpp"
#include "Gui/Colors.hpp"
#include "Gui/StaticTexture.hpp"
#include "RequestView.hpp"
#include "client/Client.h"
#include "client/GameSave.h"
#include "client/SaveInfo.h"
#include "client/SaveFile.h"
#include "client/http/UploadSaveRequest.h"
#include "save_online_png.h"
#include "Format.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size rightWidth = 230;
	}

	OnlineSaver::OnlineSaver(Game &newGame, std::unique_ptr<SaveInfo> newSaveInfo, bool newOverwrite) :
		Saver(newGame, *newSaveInfo->GetGameSave(), newOverwrite),
		saveInfo(std::move(newSaveInfo)),
		description(saveInfo->GetDescription().ToUtf8())
	{
		backgroundImage = std::make_unique<Gui::StaticTexture>(GetHost(), true, std::move(*format::PixelsFromPNG(save_online_png.AsCharSpan())));
		item.onlineInfo = BrowserItem::OnlineInfo{
			Client::Ref().GetAuthUser()->Username,
			1,
			0,
			saveInfo->GetPublished(),
		};
		title = saveInfo->GetName().ToUtf8();
		UpdateItemTitle();
	}

	ByteString OnlineSaver::GuiTitle()
	{
		return "DEFAULT_LANG_ONLINESAVER_TITLE"_St();
	}

	void OnlineSaver::Save()
	{
		// TODO-REDO_UI: better error handling
		auto data = saveInfo->GetGameSave()->Serialise().second;
		if (data.empty())
		{
			PushMessage("Error saving online simulation", "Unable to serialize game data.", true, nullptr);
			return;
		}
		saveInfo->SetName(ByteString(title).FromUtf8());
		saveInfo->SetDescription(ByteString(description).FromUtf8());
		auto request = std::make_unique<http::UploadSaveRequest>(*saveInfo);
		auto requestView = MakeRequestView(GetHost(), "DEFAULT_LANG_ONLINESAVER_PROGRESS"_St, "DEFAULT_LANG_ONLINESAVER_FAILED"_St, std::move(request));
		saveIDFuture = requestView->GetFuture();
		requestView->Start();
		PushAboveThis(requestView);
	}

	void OnlineSaver::HandleTick()
	{
		if (saveIDFuture.valid() && saveIDFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				saveInfo->SetID(saveIDFuture.get());
				game.SetSave(std::move(saveInfo), game.GetIncludePressure());
				Exit();
			}
			catch (const http::RequestError &ex)
			{
			}
		}
		Saver::HandleTick();
	}

	void OnlineSaver::GuiRight()
	{
		if (!saveInfo)
		{
			return;
		}
		Separator("rightSeparator");
		auto right = ScopedVPanel("right");
		SetSize(rightWidth);
		SetPadding(Common{});
		SetSpacing(Common{});
		BeginTextbox("description", description, BuildString("[", "DEFAULT_LANG_ONLINESAVER_DESC"_St, "]"), TextboxFlags::multiline);
		SetMaxSize(MaxSizeFitParent{});
		TextboxSetLimit(253);
		EndTextbox();
		{
			auto row = ScopedHPanel("row1");
			SetSize(Common{});
			{
				auto publish = saveInfo->GetPublished();
				BeginCheckbox("publish", "DEFAULT_LANG_ONLINESAVER_PUBLISH"_St, publish, CheckboxFlags::none);
				SetParentFillRatio(2);
				SetSizeSecondary(13);
				if (EndCheckbox())
				{
					saveInfo->SetPublished(publish);
					item.onlineInfo->published = publish;
					focusTitle = true;
				}
			}
			BeginButton("publishingInfo", "DEFAULT_LANG_ONLINESAVER_INFO"_St, ButtonFlags::none, Gui::colorTitlePurple.WithAlpha(255));
			SetParentFillRatio(3);
			if (EndButton())
			{
				// TODO
			}
		}
		{
			auto row = ScopedHPanel("row2");
			SetSize(Common{});
			{
				auto paused = saveInfo->GetGameSave()->paused;
				BeginCheckbox("paused", "DEFAULT_LANG_ONLINESAVER_PAUSED"_St, paused, CheckboxFlags::none);
				SetSizeSecondary(13);
				SetParentFillRatio(2);
				if (EndCheckbox())
				{
					auto gameSave = saveInfo->TakeGameSave();
					gameSave->paused = paused;
					saveInfo->SetGameSave(std::move(gameSave));
					focusTitle = true;
				}
			}
			BeginButton("uploadingRules", "DEFAULT_LANG_ONLINESAVER_RULES"_St, ButtonFlags::none, Gui::colorTitlePurple.WithAlpha(255));
			SetParentFillRatio(3);
			if (EndButton())
			{
				// TODO
			}
		}
	}
}
