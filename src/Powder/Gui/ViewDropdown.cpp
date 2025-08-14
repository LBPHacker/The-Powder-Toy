#include "View.hpp"
#include "SdlAssert.hpp"

namespace Powder::Gui
{
	void View::BeginDropdown(ComponentKey key, int32_t &selected)
	{
		StringView str = "";
		BeginComponent(key);
		auto dropdownIndex = *GetCurrentComponentIndex();
		{
			auto &component = componentStore[dropdownIndex];
			auto &context = SetContext<DropdownContext>(component);
			auto &parentComponent = *GetParentComponent();
			SetPrimaryAxis(parentComponent.prevLayout.primaryAxis);
			if (selected >= 0 && selected < int32_t(context.items.size()))
			{
				str = context.items[selected];
			}
		}
		Rect parentRect{ { 0, 0 }, { 0, 0 } };
		Rect popupRect{ { 0, 0 }, { 0, 0 } };
		Alignment horizontalTextAlignment;
		Alignment verticalTextAlignment;
		Pos2 textPaddingBefore = { 0, 0 };
		Pos2 textPaddingAfter = { 0, 0 };
		bool clicked = false;
		auto changed = false;
		{
			BeginButton("button", str, ButtonFlags::none);
			auto &component = componentStore[dropdownIndex];
			auto &context = GetContext<DropdownContext>(component);
			SetEnabled(component.prevContent.enabled);
			horizontalTextAlignment = component.prevContent.horizontalTextAlignment;
			verticalTextAlignment   = component.prevContent.verticalTextAlignment  ;
			textPaddingBefore       = component.prevContent.textPaddingBefore      ;
			textPaddingAfter        = component.prevContent.textPaddingAfter       ;
			if (context.items != context.lastItems)
			{
				RequestRepeatFrame();
			}
			std::swap(context.items, context.lastItems);
			context.items.clear();
			if (EndButton())
			{
				clicked = true;
			}
			auto r = GetRect();
			parentRect = Rect{
				Pos2{ r.pos - Pos2(1, 1 + selected * (r.size.Y - 1)) },
				Size2{ r.size.X + 2, r.size.Y + 2 },
			};
			popupRect = { parentRect.pos, { parentRect.size.X, (parentRect.size.Y - 1) * int32_t(context.lastItems.size()) + 3 } };
		}
		if (MaybeBeginPopup("popup", clicked))
		{
			PopupSetAnchor(popupRect.pos);
			SetPrimaryAxis(Axis::vertical);
			auto closePopup = false;
			SetSize(parentRect.size.X);
			SetPadding(1);
			SetSpacing(-1);
			int32_t lastItemsSize;
			{
				auto &component = componentStore[dropdownIndex];
				auto &context = GetContext<DropdownContext>(component);
				lastItemsSize = int32_t(context.lastItems.size());
			}
			for (int32_t i = 0; i < lastItemsSize; ++i)
			{
				{
					auto &component = componentStore[dropdownIndex];
					auto &context = GetContext<DropdownContext>(component);
					BeginButton(i, context.lastItems[i], ButtonFlags::autoWidth);
				}
				SetTextAlignment(horizontalTextAlignment, verticalTextAlignment);
				SetTextPadding(textPaddingBefore.X, textPaddingAfter.X, textPaddingBefore.Y, textPaddingAfter.Y);
				SetSize(parentRect.size.Y - 2);
				if (EndButton())
				{
					changed = selected != i;
					selected = i;
					closePopup = true;
				}
			}
			if (closePopup)
			{
				PopupClose();
			}
			EndPopup();
		}
		{
			auto &component = componentStore[dropdownIndex];
			auto &context = GetContext<DropdownContext>(component);
			context.changed = changed;
		}
	}

	bool View::EndDropdown()
	{
		auto &component = *GetCurrentComponent();
		auto &context = GetContext<DropdownContext>(component);
		EndComponent();
		// Slight hack: see View::EndButton for explanation.
		return context.changed && component.content.enabled;
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
