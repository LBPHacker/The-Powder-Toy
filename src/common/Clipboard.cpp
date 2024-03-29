#include "Clipboard.h"
#include "client/GameSave.h"
#include "simulation/SaveRenderer.h"
#include "graphics/VideoBuffer.h"
#include "gui/game/GameController.h"
#include <SDL3/SDL.h>
#include <iostream>

namespace
{
	struct ClipboardImpl
	{
		virtual ~ClipboardImpl() = default;

		virtual bool Native() const = 0;
		virtual bool HaveData() const = 0;
		virtual void SetData(std::unique_ptr<GameSave> newData) = 0;
		virtual const GameSave *GetData() = 0;
		virtual void Flush() = 0;
	};

	struct InternalClipboard : public ClipboardImpl
	{
		std::unique_ptr<GameSave> data;

		bool Native() const final override
		{
			return false;
		}

		bool HaveData() const final override
		{
			return bool(data);
		}

		void SetData(std::unique_ptr<GameSave> newData) final override
		{
			data = std::move(newData);
		}

		const GameSave *GetData() final override
		{
			return data.get();
		}

		void Flush() final override
		{
		}
	};

	struct SDLClipboard : public ClipboardImpl
	{
		static constexpr const char saveFormatName[] = "application/vnd.powdertoy.save";
		static constexpr const char imageFormatName[] = "image/png";

		std::unique_ptr<GameSave> dataSent;
		std::unique_ptr<GameSave> dataReceived;
		std::vector<char> serialized;

		static const void *SDLCALL ClipboardDataCallback(void *userdata, const char *mimeType, size_t *size)
		{
			auto *self = reinterpret_cast<SDLClipboard *>(userdata);
			self->serialized.clear();
			if (mimeType && self->dataSent)
			{
				if (!strcmp(mimeType, saveFormatName))
				{
					std::tie(std::ignore, self->serialized) = self->dataSent->Serialise();
				}
				if (!strcmp(mimeType, imageFormatName))
				{
					auto image = SaveRenderer::Ref().Render(self->dataSent.get(), true, GameController::Ref().GetRendererSettings());
					auto data = image->ToPNG();
					if (data)
					{
						self->serialized = *data;
					}
				}
			}
			*size = self->serialized.size();
			return reinterpret_cast<const void *>(self->serialized.data());
		}

		static void SDLCALL ClipboardCleanupCallback(void *userdata)
		{
			auto *self = reinterpret_cast<SDLClipboard *>(userdata);
			self->serialized.clear();
		}

		bool Native() const final override
		{
			return true;
		}

		bool HaveData() const final override
		{
			return SDL_HasClipboardData(saveFormatName);
		}

		void SetData(std::unique_ptr<GameSave> newData) final override
		{
			dataSent = std::move(newData);
			if (dataSent)
			{
				static const std::array<const char *, 2> mimeTypes = {{
					saveFormatName,
					imageFormatName,
				}};
				SDL_SetClipboardData(ClipboardDataCallback, ClipboardCleanupCallback, this, const_cast<const char **>(mimeTypes.data()), mimeTypes.size());
			}
			else
			{
				SDL_ClearClipboardData();
			}
		}

		const GameSave *GetData() final override
		{
			Flush();
			size_t size;
			auto clipboardData = reinterpret_cast<const char *>(SDL_GetClipboardData(saveFormatName, &size));
			if (clipboardData && !dataReceived)
			{
				try
				{
					dataReceived = std::make_unique<GameSave>(std::vector<char>(clipboardData, clipboardData + size));
				}
				catch (const ParseException &e)
				{
					std::cerr << "got bad save from clipboard: " << e.what() << std::endl;
				}
			}
			return dataReceived.get();
		}

		void Flush() final override
		{
			dataReceived.reset();
		}
	};

	std::unique_ptr<ClipboardImpl> clipboardImpl = std::make_unique<InternalClipboard>();
}
	
namespace Clipboard
{
	void SetClipboardData(std::unique_ptr<GameSave> newData)
	{
		clipboardImpl->SetData(std::move(newData));
	}

	const GameSave *GetClipboardData()
	{
		return clipboardImpl->GetData();
	}

	bool HaveClipboardData()
	{
		return clipboardImpl->HaveData();
	}

	bool GetNativeClipboard()
	{
		return clipboardImpl->Native();
	}

	void SetNativeClipboard(bool newUseNative)
	{
		if (newUseNative)
		{
			clipboardImpl = std::make_unique<SDLClipboard>();
		}
		else
		{
			clipboardImpl = std::make_unique<InternalClipboard>();
		}
	}

	void FlushNative()
	{
		clipboardImpl->Flush();
	}
}
