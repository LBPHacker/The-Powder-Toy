#include "Tags.hpp"
#include "RequestView.hpp"
#include "Gui/Colors.hpp"
#include "Gui/Host.hpp"
#include "client/Client.h"
#include "client/SaveInfo.h"
#include "client/http/AddTagRequest.h"
#include "client/http/RemoveTagRequest.h"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth = 200;
		constexpr Gui::View::Size tagsHeight  = 200;
	}

	void Tags::Gui()
	{
		auto user = Client::Ref().GetAuthUser();
		auto &g = GetHost();
		auto tagsdialog = ScopedDialog("tags", "DEFAULT_LANG_TAGS_TITLE"_St, dialogWidth);
		{
			auto warning = ScopedComponent("warning");
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			SetPadding(4);
			BeginText("text", "DEFAULT_LANG_TAGS_ADVICE", TextFlags::multiline | TextFlags::autoHeight, Gui::colorYellow.WithAlpha(255));
			EndText();
		}
		{
			auto existing = ScopedVPanel("existing");
			SetSpacing(Common{});
			SetAlignment(Gui::Alignment::top);
			SetSize(tagsHeight);
			int32_t tagIndex = 0;
			for (auto &tag : saveInfo.tags)
			{
				auto hPanel = ScopedHPanel(tagIndex);
				SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
				SetSize(Common{});
				SetSpacing(Common{});
				BeginText("text", tag, TextFlags::none);
				SetTextPadding(4);
				EndText();
				if (saveInfo.CanManage() && Button("button", "DEFAULT_LANG_TAGS_REMOVE"_St, g.GetCommonMetrics().smallButton))
				{
					auto request = std::make_unique<http::RemoveTagRequest>(saveInfo.GetID(), tag);
					auto requestView = MakeRequestView(GetHost(), "DEFAULT_LANG_TAGS_REMOVEPROGRESS"_St, "DEFAULT_LANG_TAGS_REMOVEFAILED"_St, std::move(request));
					removeTagFuture = requestView->GetFuture();
					requestView->Start();
					PushAboveThis(requestView);
					// TODO-REDO_UI: focus input
				}
				tagIndex += 1;
			}
			if (saveInfo.tags.empty())
			{
				Text("notags", "DEFAULT_LANG_TAGS_NOTAGS"_St, Gui::colorGray.WithAlpha(255));
			}
		}
		auto newtag = ScopedHPanel("newtag");
		SetSize(Common{});
		SetSpacing(Common{});
		if (user)
		{
			Textbox("input", newTag, BuildString("[", "DEFAULT_LANG_TAGS_NEWTAG"_St, "]"));
			BeginButton("button", "DEFAULT_LANG_TAGS_ADD"_St, ButtonFlags::none);
			SetSize(g.GetCommonMetrics().smallButton);
			SetEnabled(CanAddTag());
			if (EndButton())
			{
				auto request = std::make_unique<http::AddTagRequest>(saveInfo.GetID(), newTag);
				auto requestView = MakeRequestView(GetHost(), "DEFAULT_LANG_TAGS_ADDPROGRESS"_St, "DEFAULT_LANG_TAGS_ADDFAILED"_St, std::move(request));
				addTagFuture = requestView->GetFuture();
				requestView->Start();
				PushAboveThis(requestView);
				// TODO-REDO_UI: focus input
			}
		}
		else
		{
			Text("noUser", "DEFAULT_LANG_TAGS_NOUSER"_St, Gui::colorGray.WithAlpha(255));
		}
	}

	bool Tags::CanAddTag() const
	{
		return Client::Ref().GetAuthUser() && !newTag.empty();
	}

	Tags::DispositionFlags Tags::GetDisposition() const
	{
		return DispositionFlags::cancelMissing;
	}

	void Tags::Ok()
	{
		Exit();
	}

	void Tags::HandleTick()
	{
		if (addTagFuture.valid() && addTagFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				saveInfo.SetTags(addTagFuture.get());
				newTag.clear();
			}
			catch (const http::RequestError &ex)
			{
			}
		}
		if (removeTagFuture.valid() && removeTagFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			try
			{
				saveInfo.SetTags(removeTagFuture.get());
			}
			catch (const http::RequestError &ex)
			{
			}
		}
	}
}
