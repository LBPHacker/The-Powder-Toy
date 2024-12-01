#pragma once
#include "common/String.h"
#include <memory>
#include <optional>

class GameSave;

namespace Clipboard
{
const ByteString clipboardFormatName = "application/vnd.powdertoy.save";
void SetClipboardData(std::unique_ptr<GameSave> data);
const GameSave *GetClipboardData();
void Init();
bool GetEnabled();
void SetEnabled(bool newEnabled);
void RecreateWindow();
std::optional<String> Explanation();
}
