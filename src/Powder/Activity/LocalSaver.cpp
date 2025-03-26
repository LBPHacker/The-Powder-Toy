#include "LocalSaver.hpp"
#include "Game.hpp"
#include "Config.h"
#include "Gui/StaticTexture.hpp"
#include "client/GameSave.h"
#include "client/SaveFile.h"
#include "client/SaveInfo.h"
#include "save_local_png.h"
#include "Format.h"
#include "common/platform/Platform.h"

namespace Powder::Activity
{
	LocalSaver::LocalSaver(Game &newGame, std::unique_ptr<SaveFile> newSaveFile, bool newOverwrite) :
		Saver(newGame, *newSaveFile->GetGameSave(), newOverwrite),
		saveFile(std::move(newSaveFile))
	{
		backgroundImage = std::make_unique<Gui::StaticTexture>(GetHost(), true, std::move(*format::PixelsFromPNG(save_local_png.AsCharSpan())));
		title = saveFile->GetDisplayName().ToUtf8();
		UpdateItemTitle();
	}

	ByteString LocalSaver::GuiTitle()
	{
		return "Save local simulation"; // TODO-REDO_UI-TRANSLATE
	}

	void LocalSaver::Save()
	{
		// TODO-REDO_UI: better error handling
		auto data = saveFile->GetGameSave()->Serialise().second;
		if (data.empty())
		{
			PushMessage("Error saving local simulation", "Unable to serialize game data.", true, nullptr);
			return;
		}
		Platform::MakeDirectory(LOCAL_SAVE_DIR);
		auto path = ByteString::Build(LOCAL_SAVE_DIR, PATH_SEP_CHAR, title, ".cps");
		if (!Platform::WriteFile(data, path))
		{
			PushMessage("Error saving local simulation", "Unable to write save file.", true, nullptr);
			return;
		}
		saveFile->SetDisplayName(ByteString(title).FromUtf8());
		saveFile->SetFileName(path);
		game.SetSave(std::move(saveFile), game.GetIncludePressure());
		Exit();
	}
}
