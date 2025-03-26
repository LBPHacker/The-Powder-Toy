#include "View.hpp"
#include "Host.hpp"
#include "Gui/Colors.hpp"

#include "Common/Log.hpp"

namespace Powder::Gui
{
	namespace
	{
		constexpr View::Size commondialogWidth          = 250;
		constexpr View::Size commondialogLoadingHeight  =  80;
		constexpr View::Size commondialogErrorMinHeight =  50;
		constexpr View::Size commondialogErrorMaxHeight = 300;
		constexpr View::Size titlePadding               =   7;
	}

	void View::BeginDialog(ComponentKey key, StringView title, std::optional<Size> size, bool error)
	{
		SetTitle(ByteString(title.view.begin(), title.view.end()));
		auto dispositionFlags = GetDisposition();
		auto cancelEffort = DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::cancelEffort);
		if (!cancelEffort)
		{
			SetCancelWhenRootMouseDown(true);
		}
		BeginModal(key, GetHost().GetSize().OriginRect());
		if (size)
		{
			SetSize(*size);
		}
		SetTextAlignment(Alignment::left, Alignment::center);
		BeginText("title", title, TextFlags::none, (error ? Gui::colorLightRed : Gui::colorTitlePurple).WithAlpha(255));
		SetSize(GetHost().GetCommonMetrics().heightToFitSize);
		SetTextPadding(titlePadding);
		EndText();
		Separator("dialogBeginSeparator");
		BeginVPanel("content");
		SetPadding(Common{});
		SetSpacing(Common{});
	}

	void View::EndDialog()
	{
		auto dispositionFlags = GetDisposition();
		auto wordingBits = DispositionFlags(DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::wordingBits));
		auto applyCancel = wordingBits == DispositionFlags::applyCancel;
		auto yesNo       = wordingBits == DispositionFlags::yesNo      ;
		auto okBits = DispositionFlags(DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::okBits));
		auto okDisabled = okBits == DispositionFlags::okDisabled;
		auto okMissing  = okBits == DispositionFlags::okMissing ;
		auto cancelMissing = DispositionFlagsBase(dispositionFlags) & DispositionFlagsBase(DispositionFlags::cancelMissing);
		EndPanel();
		Separator("dialogEndSeparator");
		BeginHPanel("dialogEnd");
		{
			SetSize(GetHost().GetCommonMetrics().heightToFitSize);
			SetPadding(Common{});
			SetSpacing(Common{});
			if (!cancelMissing)
			{
				ByteString wording = "Cancel"; // TODO-REDO_UI-TRANSLATE
				if (yesNo)
				{
					wording = "No"; // TODO-REDO_UI-TRANSLATE
				}
				BeginButton("cancel", wording, ButtonFlags::none);
				if (!okMissing)
				{
					SetSize(GetHost().GetCommonMetrics().smallButton);
				}
				if (EndButton())
				{
					Cancel();
				}
			}
			if (!okMissing)
			{
				ByteString wording = "OK"; // TODO-REDO_UI-TRANSLATE
				if (yesNo)
				{
					wording = "Yes"; // TODO-REDO_UI-TRANSLATE
				}
				if (applyCancel)
				{
					wording = "Apply"; // TODO-REDO_UI-TRANSLATE
				}
				if (auto okText = GetOkText())
				{
					wording = *okText;
				}
				BeginButton("ok", wording, ButtonFlags::none, Gui::colorYellow.WithAlpha(255));
				SetSize(SpanAll{});
				SetEnabled(!okDisabled);
				if (EndButton())
				{
					Ok();
				}
			}
		}
		EndPanel();
		EndModal();
	}

	void View::BeginProgressdialog(ComponentKey key, StringView title, ProgressdialogState state)
	{
		auto *done = std::get_if<ProgressdialogStateDone>(&state);
		BeginDialog(key, title, commondialogWidth, done ? done->error : false);
		SetSpacing(0);
		if (done)
		{
			SetPadding(0);
			BeginScrollpanel("messagePanel");
			SetPadding(titlePadding);
			SetMinSize(commondialogErrorMinHeight);
			SetMaxSize(commondialogErrorMaxHeight);
			SetTextAlignment(Gui::Alignment::left, Gui::Alignment::center);
			BeginText("message", done->message, TextFlags::multiline | TextFlags::selectable | TextFlags::autoHeight);
			EndText();
			EndScrollpanel();
		}
		else
		{
			SetSize(commondialogLoadingHeight);
			Spinner("progress", GetHost().GetCommonMetrics().bigSpinner);
		}
	}

	void View::EndProgressdialog()
	{
		EndDialog();
	}

	void View::Progressdialog(ComponentKey key, StringView title, ProgressdialogState state)
	{
		BeginProgressdialog(key, title, state);
		EndProgressdialog();
	}

	void View::PushMessage(std::string title, std::string message, bool error, std::function<void ()> done)
	{
		class MessageView : public View
		{
			std::string title;
			std::string message;
			bool error;
			std::function<void ()> done;

		public:
			void Gui() final override
			{
				BeginProgressdialog("dialog", title, ProgressdialogStateDone{ error, message });
				EndProgressdialog();
			}

			DispositionFlags GetDisposition() const final override
			{
				return DispositionFlags::cancelMissing;
			}

			void Ok() final override
			{
				Exit();
				if (done)
				{
					done();
				}
			}

			MessageView(Host &newHost, std::string newTitle, std::string newMessage, bool newError, std::function<void ()> newDone) :
				View(newHost),
				title(newTitle),
				message(newMessage),
				error(newError),
				done(newDone)
			{
			}
		};
		PushAboveThis(std::make_shared<MessageView>(GetHost(), title, message, error, done));
	}

	void View::PushConfirm(std::string title, std::string message, std::optional<std::string> okText, std::function<void (bool)> done)
	{
		class ConfirmView : public View
		{
			std::string title;
			std::string message;
			std::optional<std::string> okText;
			std::function<void (bool)> done;

		public:
			void Gui() final override
			{
				BeginProgressdialog("dialog", title, ProgressdialogStateDone{ false, message });
				EndProgressdialog();
			}

			DispositionFlags GetDisposition() const final override
			{
				return DispositionFlags::yesNo;
			}

			void Ok() final override
			{
				Exit();
				if (done)
				{
					done(true);
				}
			}

			bool Cancel() final override
			{
				Exit();
				if (done)
				{
					done(false);
				}
				return true;
			}

			std::optional<std::string> GetOkText() const final override
			{
				return okText;
			}

			ConfirmView(Host &newHost, std::string newTitle, std::string newMessage, std::optional<std::string> newOkText, std::function<void (bool)> newDone) :
				View(newHost),
				title(newTitle),
				message(newMessage),
				okText(newOkText),
				done(newDone)
			{
			}
		};
		PushAboveThis(std::make_shared<ConfirmView>(GetHost(), title, message, okText, done));
	}

	void View::PushInput(std::string title, std::string message, std::string initial, std::optional<std::string> placeholder, std::function<bool (std::optional<std::string>)> done)
	{
		class InputView : public View
		{
			std::string title;
			std::string message;
			std::string text;
			std::optional<std::string> placeholder;
			std::function<bool (std::optional<std::string>)> done;
			bool focusInputNext = false;
			bool focusInput = true;

		public:
			void Gui() final override
			{
				BeginProgressdialog("dialog", title, ProgressdialogStateDone{ false, message });
				Separator("inputSeparator");
				BeginVPanel("inputPanel");
				SetSize(GetHost().GetCommonMetrics().heightToFitSize);
				SetPadding(Common{});
				if (placeholder)
				{
					BeginTextbox("input", text, *placeholder, TextboxFlags::none);
				}
				else
				{
					BeginTextbox("input", text, TextboxFlags::none);
				}
				if (focusInput)
				{
					Log("focusInput");
					GiveInputFocus();
					TextboxSelectAll();
					focusInput = false;
				}
				focusInput = focusInputNext;
				focusInputNext = false;
				EndTextbox();
				EndPanel();
				EndProgressdialog();
			}

			DispositionFlags GetDisposition() const final override
			{
				return DispositionFlags::none;
			}

			void Ok() final override
			{
				bool ok = true;
				if (done)
				{
					ok = done(text);
				}
				if (ok)
				{
					Exit();
				}
				else
				{
					focusInputNext = true;
				}
			}

			bool Cancel() final override
			{
				if (done)
				{
					done(std::nullopt);
				}
				Exit();
				return true;
			}

			InputView(
				Host &newHost,
				std::string newTitle,
				std::string newMessage,
				std::string newInitial,
				std::optional<std::string> newPlaceholder,
				std::function<bool (std::optional<std::string>)> newDone
			) :
				View(newHost),
				title(newTitle),
				message(newMessage),
				text(newInitial),
				placeholder(newPlaceholder),
				done(newDone)
			{
			}
		};
		PushAboveThis(std::make_shared<InputView>(GetHost(), title, message, initial, placeholder, done));
	}
}
