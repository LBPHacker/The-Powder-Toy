#include "Avatar.hpp"
#include "Gui/StaticTexture.hpp"
#include "client/http/ImageRequest.h"
#include "graphics/VideoBuffer.h"
#include "Config.h"

namespace Powder::Activity
{
	Avatar::Avatar(Gui::Host &newHost) : host(newHost)
	{
	}

	Avatar::~Avatar() = default;

	void Avatar::SetParameters(std::optional<Parameters> newParameters)
	{
		parameters = newParameters;
		texture.reset();
		if (parameters)
		{
			int32_t avatarSize = 40;
			ByteStringBuilder urlBuilder;
			urlBuilder << STATICSERVER << "/avatars/" << parameters->username;
			if (parameters->size >= 48)
			{
				avatarSize = 48;
			}
			if (parameters->size >= 72)
			{
				avatarSize = 256;
			}
			if (avatarSize)
			{
				urlBuilder << "." << avatarSize;
			}
			urlBuilder << ".png";
			imageRequest = std::make_unique<http::ImageRequest>(urlBuilder.Build(), Vec2<int>{ parameters->size, parameters->size });
			imageRequest->Start();
		}
	}

	Gui::StaticTexture *Avatar::GetTexture() const
	{
		return texture.get();
	}

	void Avatar::HandleTick()
	{
		if (imageRequest && imageRequest->CheckDone())
		{
			try
			{
				texture = std::make_unique<Gui::StaticTexture>(host, false, imageRequest->Finish());
			}
			catch (const http::RequestError &ex)
			{
				// Nothing, oh well.
			}
			imageRequest.reset();
		}
	}
}
