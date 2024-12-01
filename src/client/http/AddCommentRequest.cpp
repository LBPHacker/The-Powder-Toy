#include "AddCommentRequest.h"
#include "Config.h"
#include "client/Client.h"

namespace http
{
AddCommentRequest::AddCommentRequest(int saveID, String comment) :
	APIRequest(ByteString::Build(SERVER, "/Browse/Comments.json?ID=", saveID), authRequire, true)
{
	auto user = Client::Ref().GetAuthUser();
	AddPostData(FormData{
		{ "Comment", comment.ToUtf8() },
		{     "Key",  user.SessionKey },
	});
}

void AddCommentRequest::Finish()
{
	APIRequest::Finish();
}
}
