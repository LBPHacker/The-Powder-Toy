#include "Detail.h"
#include "Dynamic.h"
#include "prefs/GlobalPrefs.h"
#include "client/GameSave.h"
#include "PowderToySDL.h"
#include <iostream>
#include <stdio.h>
#include <optional>
#include <algorithm>
#include <map>
#include <SDL_syswm.h>

namespace Clipboard
{
	struct Preset
	{
		ByteString inCommand;
		ByteString formatsCommand;
		ByteString outCommand;
		std::optional<int> defaultForSubsystem;
	};
	std::map<ByteString, Preset> builtInPresets = {
		{ "xclip", {
			"xclip -selection clipboard -target %s",
			"xclip -out -selection clipboard -target TARGETS",
			"xclip -out -selection clipboard -target %s",
			SDL_SYSWM_X11,
		} },
		{ "wl-clipboard", {
			"wl-copy --type %s",
			"wl-paste --list-types",
			"wl-paste --type %s",
			SDL_SYSWM_WAYLAND,
		} },
	};

	static ByteString SubstFormat(ByteString str)
	{
		if (auto split = str.SplitBy("%s"))
		{
			str = split.Before() + clipboardFormatName + split.After();
		}
		return str;
	}

	static std::optional<Preset> GetPreset()
	{
		std::optional<ByteString> name = GlobalPrefs::Ref().Get("ExternalClipboardPreset", ByteString("auto"));
		if (name == "custom")
		{
			auto getCommand = [](ByteString key) -> std::optional<ByteString> {
				auto fullKey = "ExternalClipboardPreset" + key;
				auto value = GlobalPrefs::Ref().Get<ByteString>(fullKey);
				if (!value)
				{
					std::cerr << "custom external clipboard command preset: missing " << fullKey << std::endl;
					return std::nullopt;
				}
				return *value;
			};
			auto inCommand      = getCommand("In");
			auto formatsCommand = getCommand("Formats");
			auto outCommand     = getCommand("Out");
			if (!inCommand || !formatsCommand || !outCommand)
			{
				return std::nullopt;
			}
			return Preset{
				SubstFormat(*inCommand),
				SubstFormat(*formatsCommand),
				SubstFormat(*outCommand),
			};
		}
		if (name == "auto")
		{
			name.reset();
			for (auto &[ presetName, preset ] : builtInPresets)
			{
				if (preset.defaultForSubsystem && *preset.defaultForSubsystem == currentSubsystem)
				{
					name = presetName;
				}
			}
			if (!name)
			{
				std::cerr << "no built-in external clipboard command preset for SDL window subsystem " << currentSubsystem << std::endl;
				return std::nullopt;
			}
		}
		auto it = builtInPresets.find(*name);
		if (it == builtInPresets.end())
		{
			std::cerr << "no built-in external clipboard command preset with name " << *name << std::endl;
			return std::nullopt;
		}
		return Preset{
			SubstFormat(it->second.inCommand),
			SubstFormat(it->second.formatsCommand),
			SubstFormat(it->second.outCommand),
		};
	}

	class ExternalClipboardImpl : public ClipboardImpl
	{
	public:
		void SetClipboardData() final override
		{
			if (!clipboardData)
			{
				std::cerr << "cannot announce save on clipboard: no data to transfer" << std::endl;
				return;
			}
			auto preset = GetPreset();
			if (!preset)
			{
				return;
			}
			auto handle = popen(preset->inCommand.c_str(), "we");
			if (!handle)
			{
				std::cerr << "failed to set clipboard data: popen: " << strerror(errno) << std::endl;
				return;
			}
			std::vector<char> saveData;
			std::tie(std::ignore, saveData) = clipboardData->Serialise();
			auto bail = false;
			if (fwrite(&saveData[0], 1, saveData.size(), handle) != saveData.size())
			{
				std::cerr << "failed to set clipboard data: fwrite: " << strerror(errno) << std::endl;
				bail = true;
			}
			auto status = pclose(handle);
			if (bail)
			{
				return;
			}
			if (status == -1)
			{
				std::cerr << "failed to set clipboard data: pclose: " << strerror(errno) << std::endl;
				return;
			}
			if (status)
			{
				std::cerr << "failed to set clipboard data: " << preset->inCommand << ": wait4 status code " << status << std::endl;
				return;
			}
			std::cerr << "announced save on clipboard" << std::endl;
		}

		void GetClipboardData() final override
		{
			auto getTarget = [](ByteString command) -> std::optional<std::vector<char>> {
				if (!command.size())
				{
					return std::nullopt;
				}
				auto handle = popen(command.c_str(), "re");
				if (!handle)
				{
					std::cerr << "failed to get clipboard data: popen: " << strerror(errno) << std::endl;
					return std::nullopt;
				}
				constexpr auto blockSize = 0x10000;
				std::vector<char> data;
				auto bail = false;
				while (true)
				{
					auto pos = data.size();
					data.resize(pos + blockSize);
					auto got = fread(&data[pos], 1, blockSize, handle);
					if (got != blockSize)
					{
						if (ferror(handle))
						{
							std::cerr << "failed to get clipboard data: fread: " << strerror(errno) << std::endl;
							bail = true;
							break;
						}
						if (feof(handle))
						{
							data.resize(data.size() - blockSize + got);
							break;
						}
					}
				}
				auto status = pclose(handle);
				if (bail)
				{
					return std::nullopt;
				}
				if (status == -1)
				{
					std::cerr << "failed to get clipboard data: pclose: " << strerror(errno) << std::endl;
					return std::nullopt;
				}
				if (status)
				{
					std::cerr << "failed to get clipboard data: " << command << ": wait4 status code " << status << std::endl;
					return std::nullopt;
				}
				return data;
			};
			auto preset = GetPreset();
			if (!preset)
			{
				return;
			}
			auto formatsOpt = getTarget(preset->formatsCommand);
			if (!formatsOpt)
			{
				return;
			}
			// If we fail after this point, either there was no save on the system clipboard, or there was one
			// but we failed to load it. In either case, our local clipboard should be empty.
			clipboardData.reset();
			auto formats = ByteString(formatsOpt->begin(), formatsOpt->end()).PartitionBy('\n');
			if (std::find(formats.begin(), formats.end(), clipboardFormatName) == formats.end())
			{
				std::cerr << "not getting save from clipboard: no data" << std::endl;
				return;
			}
			auto saveDataOpt = getTarget(preset->outCommand);
			if (!saveDataOpt)
			{
				return;
			}
			try
			{
				clipboardData = std::make_unique<GameSave>(*saveDataOpt);
			}
			catch (const ParseException &e)
			{
				std::cerr << "got bad save from clipboard: " << e.what() << std::endl;
				return;
			}
			std::cerr << "got save from clipboard" << std::endl;
		}
	};

	std::unique_ptr<ClipboardImpl> ExternalClipboardFactory()
	{
		return std::make_unique<ExternalClipboardImpl>();
	}
}
