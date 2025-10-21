#include "GetCommentsRequest.h"
#include "client/Client.h"
#include "Config.h"

namespace http
{
	GetCommentsRequest::GetCommentsRequest(int saveID, int start, int count) :
		APIRequest({ ByteString::Build(SERVER, "/Browse/Comments.json"), {
			{ "ID", ByteString::Build(saveID) },
			{ "Start", ByteString::Build(start) },
			{ "Count", ByteString::Build(count) },
		} }, authOmit, false)
	{
	}

	std::vector<Comment> GetCommentsRequest::Finish()
	{
		auto result = APIRequest::Finish();
		std::vector<Comment> comments;
		auto user = Client::Ref().GetAuthUser();
		try
		{
			for (auto &comment : result)
			{
				comments.push_back({
					comment["Username"].asString(),
					User::ElevationFromString(comment["Elevation"].asString()),
					user ? comment["Username"].asString() == user->Username : false,
					comment["IsBanned"].asBool(),
					ByteString(comment["Text"].asString()).FromUtf8(),
					ByteString(comment["Timestamp"].asString()).ToNumber<time_t>(),
				});
			}
		}
		catch (const std::exception &ex)
		{
			throw RequestError("Could not read response: " + ByteString(ex.what()));
		}
		return comments;
	}
}
