#pragma once
#include <memory>
#include <optional>
#include "common/String.h"

class GameSave;

namespace Clipboard
{
	void SetClipboardData(std::unique_ptr<GameSave> newData);
	const GameSave *GetClipboardData();
	bool HaveClipboardData();
	bool GetNativeClipboard();
	void SetNativeClipboard(bool newUsePlatform);
	void FlushNative();
}
