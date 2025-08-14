#include "View.hpp"
#include "ViewUtil.hpp"
#include "Host.hpp"

namespace Powder::Gui
{
	void View::UpdateLayoutContentSize(Axis parentPrimaryAxis, Component &component)
	{
		component.rect.size = { 0, 0 };
		for (auto &child : componentStore.GetChildRange(component))
		{
			UpdateLayoutContentSize(component.layout.primaryAxis, child);
			for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
			{
				auto spread = psAxis == 0 && !component.layout.layered;
				auto xyAxis = AxisBase(component.layout.primaryAxis) ^ psAxis;
				if (spread)
				{
					component.rect.size % xyAxis += child.rect.size % xyAxis + child.layout.spacingFromParent;
				}
				else
				{
					component.rect.size % xyAxis = std::max(component.rect.size % xyAxis, child.rect.size % xyAxis);
				}
			}
		}
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			auto xyAxis = AxisBase(component.layout.primaryAxis) ^ psAxis;
			auto parentPsAxis = AxisBase(parentPrimaryAxis) ^ xyAxis;
			auto &effectiveSizeC = component.rect.size % xyAxis;
			effectiveSizeC += component.layout.paddingBefore % psAxis + component.layout.paddingAfter % psAxis;
			if (!std::holds_alternative<MaxSizeInfinite>(component.layout.maxSize % parentPsAxis))
			{
				effectiveSizeC = 0;
			}
			if (auto *size = std::get_if<Size>(&(component.layout.minSize % parentPsAxis)))
			{
				effectiveSizeC = std::max(effectiveSizeC, *size);
			}
			if (auto *size = std::get_if<Size>(&(component.layout.maxSize % parentPsAxis)))
			{
				effectiveSizeC = std::min(effectiveSizeC, *size);
			}
		}
		ClampSize(component.rect.size);
	}

	void View::UpdateLayoutFillSpace(Component &component)
	{
		for (AxisBase psAxis = 0; psAxis < 2; ++psAxis)
		{
			int32_t childPosition = 0;
			auto xyAxis = AxisBase(component.layout.primaryAxis) ^ psAxis;
			auto space = component.rect.size % xyAxis - component.layout.paddingBefore % psAxis - component.layout.paddingAfter % psAxis;
			auto spread = psAxis == 0 && !component.layout.layered;
			if (spread)
			{
				Size spaceUsed = 0;
				Size fillRatioSum = 0;
				for (auto &child : componentStore.GetChildRange(component))
				{
					spaceUsed += child.rect.size % xyAxis + child.layout.spacingFromParent;
					fillRatioSum += child.layout.parentFillRatio % psAxis;
					child.fillSatisfied = child.layout.parentFillRatio % psAxis == 0;
				}
				auto spaceLeft = std::max(0, space - spaceUsed);
#if DebugGuiView
# if DebugGuiViewChildren
				if (component.debugMe)
				{
					Log(space, " ", spaceUsed);
					std::ostringstream ss;
					for (auto i = component.firstChildIndex; i; i = componentStore[*i].nextSiblingIndex)
					{
						ss << i->value << " ";
					}
					Log("  ", ss.str());
				}
# endif
#endif
				if (fillRatioSum && spaceLeft)
				{
					bool lastRound = false;
					while (true)
					{
						Size partialFillRatioSum = 0;
						Size primarySpaceFilled = 0;
						Size fillRatioHandled = 0;
						bool roundLimitedByMaxSize = false;
						for (auto &child : componentStore.GetChildRange(component))
						{
							if (!child.fillSatisfied)
							{
								auto prevPartialFillRatioSum = partialFillRatioSum;
								partialFillRatioSum += child.layout.parentFillRatio % psAxis;
								auto growLow = spaceLeft * prevPartialFillRatioSum / fillRatioSum;
								auto growHigh = spaceLeft * partialFillRatioSum / fillRatioSum;
								auto willGrow = growHigh - growLow;
								bool limitedByMaxSize = false;
								if (auto *size = std::get_if<Size>(&(child.layout.maxSize % psAxis)))
								{
									auto canGrow = *size - child.rect.size % xyAxis;
									if (willGrow > canGrow)
									{
										willGrow = canGrow;
										limitedByMaxSize = true;
										roundLimitedByMaxSize = true;
									}
								}
								if (lastRound || limitedByMaxSize)
								{
									child.rect.size % xyAxis += willGrow;
									primarySpaceFilled += willGrow;
									fillRatioHandled += child.layout.parentFillRatio % psAxis;
									child.fillSatisfied = true;
								}
							}
						}
						spaceLeft -= primarySpaceFilled;
						fillRatioSum -= fillRatioHandled;
						if (lastRound || spaceLeft == 0)
						{
							break;
						}
						if (!roundLimitedByMaxSize)
						{
							lastRound = true;
						}
					}
				}
			}
			else
			{
				for (auto &child : componentStore.GetChildRange(component))
				{
					if (child.layout.parentFillRatio % psAxis > 0)
					{
						child.rect.size % xyAxis = std::max(space, child.rect.size % xyAxis);
						if (auto *size = std::get_if<Size>(&(child.layout.maxSize % psAxis)))
						{
							child.rect.size % xyAxis = std::min(*size, child.rect.size % xyAxis);
						}
					}
				}
			}
			Size spaceUsed = 0;
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (spread)
				{
					spaceUsed += child.rect.size % xyAxis + child.layout.spacingFromParent;
				}
				else
				{
					spaceUsed = std::max(spaceUsed, child.rect.size % xyAxis);
				}
			}
			auto minScroll = std::min(space - spaceUsed, 0);
			auto maxScroll = 0;
			auto spaceLeft = std::max(0, space - spaceUsed);
			auto alignmentOffset = spaceLeft * Size(component.layout.alignment % psAxis) / 2;
			auto effectiveScroll = std::clamp(component.layout.scroll % psAxis, minScroll, maxScroll) + alignmentOffset;
			component.contentSpace % psAxis = space;
			component.contentSize % psAxis = spaceUsed;
			component.overflow % psAxis = -minScroll;
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (spread)
				{
					childPosition += child.layout.spacingFromParent;
				}
				child.rect.pos % xyAxis = childPosition;
				if (spread)
				{
					childPosition += child.rect.size % xyAxis;
				}
			}
			auto effectivePaddingBefore = (component.layout.order % psAxis == Order::highToLow ? component.layout.paddingAfter : component.layout.paddingBefore) % psAxis;
			for (auto &child : componentStore.GetChildRange(component))
			{
				if (component.layout.order % psAxis == Order::highToLow)
				{
					child.rect.pos % xyAxis = childPosition - child.rect.size % xyAxis - child.rect.pos % xyAxis;
				}
				child.rect.pos % xyAxis += component.rect.pos % xyAxis + effectiveScroll + effectivePaddingBefore;
				if (!spread)
				{
					child.rect.pos % xyAxis += ((spaceUsed - child.rect.size % xyAxis) * Size(component.layout.alignment % psAxis) + 1) / 2;
				}
				ClampPos(child.rect.pos);
			}
		}
		for (auto &child : componentStore.GetChildRange(component))
		{
			UpdateLayoutFillSpace(child);
		}
	}

	void View::UpdateLayout()
	{
#if DebugGuiView
		Log("UpdateLayout");
#endif
		if (rootIndex)
		{
			auto &root = componentStore[*rootIndex];
			UpdateLayoutContentSize(Axis::horizontal, root);
			if (rootRect)
			{
				root.rect.pos = rootRect->pos;
			}
			else
			{
				root.rect.pos = { 0, 0 };
			}
			UpdateLayoutFillSpace(root);
			rootEffectiveSize = root.rect.size;
		}
		else
		{
			rootEffectiveSize = { 0, 0 };
		}
	}
}
