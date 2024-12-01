#include "DeleteSaveRequest.h"
#include "Config.h"
#include "client/Client.h"

namespace http
{
DeleteSaveRequest::DeleteSaveRequest(int saveID) :
	APIRequest(
		ByteString::Build(
			SERVER, "/Browse/Delete.json?ID=", saveID, "&Mode=Delete&Key=", Client::Ref().GetAuthUser().SessionKey
		),
		authRequire,
		true
	)
{
}

void DeleteSaveRequest::Finish()
{
	APIRequest::Finish();
}
}
