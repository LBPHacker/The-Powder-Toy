#include "LogoutRequest.h"
#include "Config.h"
#include "client/Client.h"

namespace http
{
LogoutRequest::LogoutRequest() :
	APIRequest(
		ByteString::Build(SERVER, "/Logout.json?Key=" + Client::Ref().GetAuthUser().SessionKey), authRequire, true
	)
{
}

void LogoutRequest::Finish()
{
	APIRequest::Finish();
}
}
