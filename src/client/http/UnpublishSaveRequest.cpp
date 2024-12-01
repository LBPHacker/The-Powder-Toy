#include "UnpublishSaveRequest.h"
#include "Config.h"
#include "client/Client.h"

namespace http
{
UnpublishSaveRequest::UnpublishSaveRequest(int saveID) :
	APIRequest(
		ByteString::Build(
			SERVER, "/Browse/Delete.json?ID=", saveID, "&Mode=Unpublish&Key=", Client::Ref().GetAuthUser().SessionKey
		),
		authRequire,
		true
	)
{
}

void UnpublishSaveRequest::Finish()
{
	APIRequest::Finish();
}
}
