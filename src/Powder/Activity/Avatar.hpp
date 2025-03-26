#pragma once
#include "common/String.h"
#include <optional>
#include <memory>

namespace http
{
	class ImageRequest;
}

namespace Powder::Gui
{
	class Host;
	class StaticTexture;
}

namespace Powder::Activity
{
	class Avatar
	{
	public:
		struct Parameters
		{
			ByteString username;
			int32_t size;
		};

	private:
		std::optional<Parameters> parameters;
		std::unique_ptr<Gui::StaticTexture> texture;
		std::unique_ptr<http::ImageRequest> imageRequest;

		Gui::Host &host;

	public:
		Avatar(Gui::Host &newHost);
		~Avatar();

		void HandleTick();

		void SetParameters(std::optional<Parameters> newParameters);
		Gui::StaticTexture *GetTexture() const;
	};
}
