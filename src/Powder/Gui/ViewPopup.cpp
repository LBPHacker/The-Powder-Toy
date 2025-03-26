#include "View.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	bool View::MaybeBeginPopup(ComponentKey key, bool open)
	{
		BeginComponent(key);
		SetImmaterial(true);
		auto &context = SetContext<PopupContext>(*GetCurrentComponent());
		if (open)
		{
			context.open = true;
			if (auto m = GetMousePos())
			{
				context.rootRect.pos = *m - Pos2{ 1, 1 };
			}
		}
		if (!context.open)
		{
			EndComponent();
			return false;
		}
		BeginModal(key, context.rootRect);
		return true;
	}

	void View::EndPopup()
	{
		auto r = GetRect();
		auto layoutRootIndex = *GetCurrentComponentIndex(); // BeginModal pushes 2 components
		EndModal();
		auto popupIndex = *GetCurrentComponentIndex();
		auto &context = GetContext<PopupContext>(componentStore[popupIndex]);
		{
			auto vrect = GetHost().GetSize().OriginRect();
			vrect.size -= r.size - Size2{ 1, 1 };
			auto oldRect = context.rootRect;
			context.rootRect = Rect{ RobustClamp(context.rootRect.pos, vrect), r.size };
			if (context.rootRect != oldRect)
			{
				RequestRepeatFrame();
			}
		}
		context.layoutRootIndex = layoutRootIndex;
		popupIndices.push_back(popupIndex);
		EndComponent();
	}

	void View::ClosePopupInternal(Component &component)
	{
		auto &context = GetContext<PopupContext>(component);
		context.open = false;
	}

	void View::PopupClose()
	{
		ClosePopupInternal(componentStore[(componentStack.end() - 3)->index]); // BeginModal pushes 2 components
	}

	void View::PopupSetAnchor(Pos2 newAnchor)
	{
		auto &context = GetContext<PopupContext>(componentStore[(componentStack.end() - 3)->index]); // BeginModal pushes 2 components;
		context.rootRect.pos = newAnchor;
	}
}
