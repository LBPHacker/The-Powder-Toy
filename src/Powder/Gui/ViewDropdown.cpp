#include "View.hpp"
#include "Host.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	struct DropdownView : public View
	{
		static constexpr Size buttonSize = 17;

		Rect parentRect{ { 0, 0 }, { 0, 0 } };
		Alignment horizontalTextAlignment;
		Alignment verticalTextAlignment;
		// these two are (horizontal, vertical)
		Pos2 textPaddingBefore = { 0, 0 };
		Pos2 textPaddingAfter = { 0, 0 };
		int32_t selected;
		std::vector<std::string> items;
		enum class State
		{
			initial,
			active,
			done,
		};
		State state = State::initial;

		using View::View;

		Rect GetRootRect() const
		{
			Size2 size{ parentRect.size.X, (parentRect.size.Y - 1) * int32_t(items.size()) + 5 };
			auto vrect = GetHost().GetSize().OriginRect();
			vrect.size -= size;
			return Rect{ RobustClamp(parentRect.pos, vrect), size };
		}

		void Gui() final override
		{
			SetRootRect(GetRootRect());
			SetExitWhenRootMouseDown(true);
			BeginModal("dropdown");
			SetSize(parentRect.size.X);
			SetPadding(1);
			SetSpacing(-1);
			int32_t buttonIndex = 0;
			for (auto &item : items)
			{
				BeginButton(buttonIndex, item, ButtonFlags::autoWidth);
				SetTextAlignment(horizontalTextAlignment, verticalTextAlignment);
				SetTextPadding(textPaddingBefore.X, textPaddingAfter.X, textPaddingBefore.Y, textPaddingAfter.Y);
				SetSize(parentRect.size.Y);
				if (EndButton())
				{
					selected = buttonIndex;
					Exit();
				}
				buttonIndex += 1;
			}
			EndModal();
		}

		bool HandleEvent(const SDL_Event &event) final override
		{
			auto handledByView = View::HandleEvent(event);
			if (HandleExitEvent(event))
			{
				return true;
			}
			if (MayBeHandledExclusively(event) && handledByView)
			{
				return true;
			}
			return false;
		}

		void Exit()
		{
			state = DropdownView::State::done;
			View::Exit();
		}
	};

	void View::BeginDropdown(ComponentKey key, int32_t &selected)
	{
		BeginComponent(key);
		StringView str = "";
		{
			auto &component = *GetCurrentComponent();
			auto &context = SetContext<DropdownContext>(component);
			auto &parentComponent = *GetParentComponent();
			SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
			if (selected >= 0 && selected < int32_t(context.items.size()))
			{
				str = context.items[selected];
			}
		}
		BeginButton("button", str, ButtonFlags::none);
		auto &component = *GetParentComponent();
		auto &context = SetContext<DropdownContext>(component);
		auto enabled = component.prevContent.enabled;
		SetEnabled(enabled);
		context.changed = false;
		context.lastSelected = selected;
		if (context.dropdownView && context.dropdownView->state == DropdownView::State::done)
		{
			selected = context.dropdownView->selected;
			context.changed = true;
			context.dropdownView.reset();
		}
		if (context.items != context.lastItems)
		{
			RequestRepeatFrame();
		}
		std::swap(context.items, context.lastItems);
		context.items.clear();
		context.clicked = EndButton();
	}

	bool View::EndDropdown()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		if (context.clicked)
		{
			context.dropdownView = std::make_shared<DropdownView>(GetHost());
			auto r = GetRect();
			context.dropdownView->parentRect = Rect{
				Pos2{ r.pos - Pos2(1, 2 + context.lastSelected * (r.size.Y - 1)) },
				Size2{ r.size.X + 2, r.size.Y },
			};
			context.dropdownView->horizontalTextAlignment = component.prevContent.horizontalTextAlignment;
			context.dropdownView->verticalTextAlignment = component.prevContent.verticalTextAlignment;
			context.dropdownView->textPaddingBefore = component.prevContent.textPaddingBefore;
			context.dropdownView->textPaddingAfter = component.prevContent.textPaddingAfter;
			context.dropdownView->selected = context.lastSelected;
			for (auto &item : context.lastItems)
			{
				context.dropdownView->items.push_back(item);
			}
			PushAboveThis(context.dropdownView);
			context.dropdownView->state = DropdownView::State::active;
		}
		EndComponent();
		return context.changed;
	}

	void View::DropdownItem(StringView item)
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		context.items.push_back(std::string(item));
	}

	bool View::DropdownGetChanged() const
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		return context.changed;
	}

	bool View::Dropdown(ComponentKey key, std::span<StringView> items, int32_t &selected, SetSizeSize size)
	{
		BeginDropdown(key, selected);
		SetSize(size);
		for (auto item : items)
		{
			DropdownItem(item);
		}
		return EndDropdown();
	}
}
