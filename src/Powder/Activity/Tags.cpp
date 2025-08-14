#include "Tags.hpp"
#include "RequestView.hpp"
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
		BeginDialog("tags", "Manage tags", g.GetSize().OriginRect(), dialogWidth); // TODO-REDO_UI-TRANSLATE
		BeginComponent("warning");
		SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
		SetPadding(4);
		BeginText("text", "\boTags are only to be used to improve search results.", TextFlags::multiline | TextFlags::autoHeight); // TODO-REDO_UI-TRANSLATE
		EndText();
		EndComponent();
		BeginVPanel("existing");
		SetSpacing(Common{});
		SetAlignment(Gui::Alignment::top);
		SetSize(tagsHeight);
		int32_t tagIndex = 0;
		for (auto &tag : saveInfo.tags)
		{
			BeginHPanel(tagIndex);
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			SetSize(Common{});
			SetSpacing(Common{});
			BeginText("text", tag, TextFlags::none);
			SetTextPadding(4);
			EndText();
			if (saveInfo.CanManage() && Button("button", "Remove", g.GetCommonMetrics().smallButton)) // TODO-REDO_UI-TRANSLATE
			{
				auto request = std::make_unique<http::RemoveTagRequest>(saveInfo.GetID(), tag);
				auto requestView = MakeRequestView(GetHost(), "Removing tag", std::move(request)); // TODO-REDO_UI-TRANSLATE
				removeTagFuture = requestView->GetFuture();
				requestView->Start();
				PushAboveThis(requestView);
			}
			EndPanel();
			tagIndex += 1;
		}
		if (saveInfo.tags.empty())
		{
			Text("notags", "\bgNo tags set"); // TODO-REDO_UI-TRANSLATE
		}
		EndPanel();
		BeginHPanel("newtag");
		SetSize(Common{});
		SetSpacing(Common{});
		if (user)
		{
			Textbox("input", newTag, "[new tag]"); // TODO-REDO_UI-TRANSLATE
			BeginButton("button", "Add", ButtonFlags::none);  // TODO-REDO_UI-TRANSLATE
			SetSize(g.GetCommonMetrics().smallButton);
			SetEnabled(CanAddTag());
			if (EndButton())
			{
				auto request = std::make_unique<http::AddTagRequest>(saveInfo.GetID(), newTag);
				auto requestView = MakeRequestView(GetHost(), "Adding tag", std::move(request)); // TODO-REDO_UI-TRANSLATE
				addTagFuture = requestView->GetFuture();
				requestView->Start();
				PushAboveThis(requestView);
			}
		}
		else
		{
			Text("nouser", "\bgLog in to add tags"); // TODO-REDO_UI-TRANSLATE
		}
		EndPanel();
		EndDialog();
	}

	bool Tags::CanAddTag() const
	{
		return Client::Ref().GetAuthUser() && !newTag.empty();
	}

	Tags::CanApplyKind Tags::CanApply() const
	{
		return CanApplyKind::onlyApply;
	}

	void Tags::Apply()
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
