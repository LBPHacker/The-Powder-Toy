#include "SearchTags.h"

#include "Backend.h"
#include "network/Request.h"
#include "Format.h"

namespace backend
{
	SearchTags::SearchTags(int start, int count)
	{
		request->uri = ByteString::Build(SCHEME SERVER "/Browse/Tags.json?Start=", start, "&Count=", count);
		Backend::Ref().AddAuthHeaders(*request);
	}

	bool SearchTags::Process()
	{
		if (!PreprocessResponse(responseCheckJson))
		{
			return false;
		}
		try
		{
			tagCount = root["TagTotal"].asInt();
			for (auto &sub : root["Tags"])
			{
				tags.push_back(TagInfo{
					ByteString(sub["Tag"].asString()).FromUtf8(),
					sub["Count"].asInt(),
				});
			}
		}
		catch (const std::exception &e) // * TODO-REDO_UI: Stupid, should be an exception specific to our json lib.
		{
			error = "Could not read response: " + ByteString(e.what()).FromUtf8();
			shortError = "invalid JSON";
			return false;
		}
		return true;
	}
}