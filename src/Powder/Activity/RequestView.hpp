#pragma once
#include "Gui/View.hpp"
#include "common/String.h"
#include "client/http/Request.h"
#include <future>
#include <memory>
#include <optional>

namespace Powder::Activity
{
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
			CommondialogState state = CommondialogStateProgress{};
			if (error)
			{
				state = CommondialogStateFailed{ *error };
			}
			Commondialog("dialog", title, state);
		}

		CanApplyKind CanApply() const final override
		{
			return error ? CanApplyKind::onlyApply : CanApplyKind::onlyCancel;
		}

		void Apply() final override
		{
			Exit();
		}

		bool Cancel() final override
		{
			Exit();
			return true;
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
