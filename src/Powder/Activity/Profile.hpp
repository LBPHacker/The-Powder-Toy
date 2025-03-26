#pragma once
#include "Gui/View.hpp"
#include "client/UserInfo.h"
#include "Avatar.hpp"
#include <ctime>
#include <future>
#include <memory>

namespace http
{
	class GetUserInfoRequest;
}

namespace Powder::Activity
{
	class Avatar;

	class Profile : public Gui::View
	{
		ByteString username;
		time_t openedAt;
		std::unique_ptr<http::GetUserInfoRequest> getUserInfoRequest;
		std::future<void> logoutFuture;

		struct InfoLoading
		{
		};
		struct InfoData
		{
			UserInfo info;
			ByteString biography;
		};
		struct InfoError
		{
			ByteString message;
		};
		using Info = std::variant<
			InfoLoading,
			InfoData,
			InfoError
		>;
		Info info;
		std::unique_ptr<Avatar> avatar;

		void Gui() final override;
		void HandleTick() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;

	public:
		Profile(Gui::Host &newHost, ByteString newUsername);
		~Profile();
	};
}
