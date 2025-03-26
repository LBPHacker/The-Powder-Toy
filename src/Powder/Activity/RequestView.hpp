#pragma once
#include "Gui/View.hpp"
#include "common/String.h"
#include "client/http/Request.h"
#include <future>
#include <memory>
#include <optional>

namespace Powder::Activity
{
	// TODO-REDO_UI: decide if maybe the failure state should be a PushMessage
	template<class Request>
	class RequestView : public Gui::View
	{
		ByteString title;
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
		}

		void Gui() final override
		{
			ProgressdialogState state = ProgressdialogStateProgress{};
			if (error)
			{
				state = ProgressdialogStateDone{ true, *error };
			}
			Progressdialog("dialog", title, state);
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
		RequestView(Gui::Host &newHost, ByteString newTitle, std::unique_ptr<Request> newRequest) : View(newHost), title(newTitle), request(std::move(newRequest))
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
	std::shared_ptr<RequestView<Request>> MakeRequestView(Gui::Host &host, ByteString newTitle, std::unique_ptr<Request> request)
	{
		return std::make_shared<RequestView<Request>>(host, newTitle, std::move(request));
	}
}
