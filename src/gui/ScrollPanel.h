#pragma once

#include "Component.h"
#include "LinearDelayAnimation.h"
#include "MomentumAnimation.h"

#include <array>
#include <cstdint>

namespace gui
{
	class ScrollPanel : public Component
	{
		std::shared_ptr<Component> interior;

		struct ScrollState
		{
			bool enabled = false;
			int draggerSize = 0;
			int draggerPosition = 0;
			bool dragActive = false;
			int dragInitialDragger = 0;
			int dragInitialMouse = 0;
			bool alignHigh = false;
			MomentumAnimation anim;
		};
		std::array<ScrollState, 2> scroll;
		bool scrollBarVisible = true;
		LinearDelayAnimation scrollBarPopup;

	public:
		ScrollPanel();

		std::shared_ptr<Component> Interior()
		{
			return interior;
		}

		void Tick() final override;
		void DrawAfterChildren() const final override;
		bool MouseWheel(Point current, int distance, int direction) final override;
		void MouseDragMove(Point current, int button) final override;
		void MouseDragBegin(Point current, int button) final override;
		void MouseDragEnd(Point current, int button) final override;

		void Size(Point newSize);
		Point Size() const
		{
			return Component::Size();
		}

		void ScrollBarVisible(bool newScrollBarVisible);
		bool ScrollBarVisible() const
		{
			return scrollBarVisible;
		}

		void AlignRight(bool newAlignRight)
		{
			scroll[0].alignHigh = newAlignRight;
		}
		bool AlignRight() const
		{
			return scroll[0].alignHigh;
		}

		void AlignBottom(bool newAlignBottom)
		{
			scroll[1].alignHigh = newAlignBottom;
		}
		bool AlignBottom() const
		{
			return scroll[1].alignHigh;
		}

		void Offset(Point offset);
		Point Offset() const
		{
			return interior->Position();
		}
	};
}
