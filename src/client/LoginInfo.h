#pragma once
#include "ServerNotification.h"
#include "User.h"
#include <vector>

struct LoginInfo
{
	User user;
	std::vector<ServerNotification> notifications;
};
