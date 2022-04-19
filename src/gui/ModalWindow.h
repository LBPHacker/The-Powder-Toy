#pragma once
#include "Config.h"

#include "Component.h"
#include "LinearDelayAnimation.h"

#include <cstdint>

namespace gui
{
	class Button;
	class SDLWindow;
	
	class ModalWindow : public Component
	{
	public:
		using CancelCallback = std::function<void ()>;
		using OkayCallback = std::function<void ()>;

	private:
		CancelCallback cancel;
		OkayCallback okay;
		bool quittable = true;
		bool wantBackdrop = false;
		bool drawToolTips = true;
		bool shadedToolTips = true;
		bool pushed = false;
		int fpsLimit = 60;
		int rpsLimit = 0;
		LinearDelayAnimation toolTipAnim;
		std::shared_ptr<const ToolTipInfo> toolTipBackup;

		std::shared_ptr<const ToolTipInfo> ToolTipToDraw() const;

	public:
		ModalWindow();

		virtual void Tick() override;
		virtual void Quit() override;
		virtual void DrawAfterChildren() const override;

		void Quittable(bool newQuittable)
		{
			quittable = newQuittable;
		}
		bool Quittable() const
		{
			return quittable;
		}

		void FPSLimit(int newFpsLimit)
		{
			fpsLimit = newFpsLimit;
		}
		int FPSLimit() const
		{
			return fpsLimit;
		}

		void RPSLimit(int newRpsLimit)
		{
			rpsLimit = newRpsLimit;
		}
		int RPSLimit() const
		{
			return rpsLimit;
		}

		void WantBackdrop(bool newWantBackdrop)
		{
			wantBackdrop = newWantBackdrop;
		}
		bool WantBackdrop() const
		{
			return wantBackdrop;
		}

		virtual bool MouseDown(Point current, int button) override;
		virtual bool KeyPress(int sym, int scan, bool repeat, bool shift, bool ctrl, bool alt) override;
		virtual void Cancel();
		virtual void Okay();

		friend class SDLWindow;

		void Cancel(CancelCallback newCancel)
		{
			cancel = newCancel;
		}
		CancelCallback Cancel() const
		{
			return cancel;
		}

		void Okay(OkayCallback newOkay)
		{
			okay = newOkay;
		}
		OkayCallback Okay() const
		{
			return okay;
		}

		void DrawToolTips(bool newDrawToolTips)
		{
			drawToolTips = newDrawToolTips;
		}
		bool DrawToolTips() const
		{
			return drawToolTips;
		}

		void ShadedToolTips(bool newShadedToolTips)
		{
			shadedToolTips = newShadedToolTips;
		}
		bool ShadedToolTips() const
		{
			return shadedToolTips;
		}
	};
}
