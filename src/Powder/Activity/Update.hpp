#pragma once
#include "Gui/View.hpp"
#include "client/StartupInfo.h"

namespace http
{
	class Request;
}

namespace Powder::Gui
{
	class Host;
}

namespace Powder::Activity
{
	class Update : public Gui::View
	{
		std::unique_ptr<http::Request> request;

		void Gui() final override;
		void HandleTick() final override;
		void ApplyUpdate(ByteString data);
		void Die(ByteString title, ByteString message);

	public:
		Update(Gui::Host &newHost, ByteString url);

		static void PushUpdateConfirm(Gui::View &view, UpdateInfo info);
	};
}
