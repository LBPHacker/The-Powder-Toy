#pragma once
#include "Gui/View.hpp"
#include "Gui/ViewUtil.hpp"
#include "Gui/Host.hpp"
#include "Lang/Language.hpp"
#include "common/String.h"
#include "client/http/Request.h"
#include <future>
#include <memory>
#include <optional>

namespace Powder::Activity
{
	// TODO-REDO_UI: factor out common parts into some base
	template<class Request>
	class RequestView : public Gui::View
	{
		Lang::Translation &title;
		Lang::Translation &failureTitle;
		std::unique_ptr<Request> request;
		using Result = decltype(request->Finish());
		std::promise<Result> result;
		std::optional<ByteString> error;

		void HandleTick() final override
		{
			if (request && request->CheckDone())
			{
				try
				{
					if constexpr (std::is_same_v<Result, void>)
					{
						request->Finish();
						result.set_value();
					}
					else
					{
						result.set_value(request->Finish());
					}
					request.reset();
					Exit();
				}
				catch (const http::RequestError &ex)
				{
					error = ex.what();
					result.set_exception(std::current_exception());
				}
				request.reset();
			}
			if (error)
			{
				Exit();
				PushMessage(BuildString(failureTitle), *error, true, nullptr);
			}
		}

		void Gui() final override
		{
			Progressdialog("dialog", title);
		}

		DispositionFlags GetDisposition() const final override
		{
			if (error)
			{
				return DispositionFlags::cancelMissing;
			}
			return DispositionFlags::okMissing | DispositionFlags::cancelEffort;
		}

		void Ok() final override
		{
			Exit();
		}

		bool Cancel() final override
		{
			Exit();
			return true;
		}

		void Exit() final override
		{
			if (request)
			{
				error = "cancelled";
				result.set_exception(std::make_exception_ptr(http::RequestError(*error)));
				request.reset();
			}
			View::Exit();
		}

	public:
		RequestView(
			Gui::Host &newHost,
			Lang::Translation &newTitle,
			Lang::Translation &newFailureTitle,
			std::unique_ptr<Request> newRequest
		) :
			View(newHost),
			title(newTitle),
			failureTitle(newFailureTitle),
			request(std::move(newRequest))
		{
		}

		std::future<Result> GetFuture()
		{
			return result.get_future();
		}

		void Start()
		{
			request->Start();
		}
	};

	template<class Request>
	std::shared_ptr<RequestView<Request>> MakeRequestView(Gui::Host &host, Lang::Translation &newTitle, Lang::Translation &newFailureTitle, std::unique_ptr<Request> request)
	{
		return std::make_shared<RequestView<Request>>(host, newTitle, newFailureTitle, std::move(request));
	}
}
