#pragma once
#include "Alignment.hpp"
#include "MomentumAnimation.hpp"
#include "TextWrapper.hpp"
#include "Common/Color.hpp"
#include "Common/CrossFrameItemStore.hpp"
#include "Common/NoCopy.hpp"
#include "Common/Rect.hpp"
#include "Common/Vec2.hpp"
#include "common/String.h"
#include "Lang/Format.hpp"
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

typedef union SDL_Event SDL_Event;

#define DebugGuiView 0

namespace Powder::Gui
{
	class Host;
	class Stack;

	struct DropdownView;

	class View : public NoCopy
	{
	public:
		using Pos              = int32_t;
		using Size             = int32_t;
		using Pos2             = Vec2<Pos>;
		using Size2            = Vec2<Size>;
		using Rect             = ::Rect<int32_t>;
		using MouseButtonIndex = int32_t;

		static constexpr Pos2 RobustClamp(Pos2 pos, Rect rect)
		{
			pos.X = std::max(std::min(pos.X, rect.BottomRight().X), rect.TopLeft().X);
			pos.Y = std::max(std::min(pos.Y, rect.BottomRight().Y), rect.TopLeft().Y);
			return pos;
		}

		struct StringView
		{
			std::string_view view;

			template<size_t N>
			StringView(const char (&arr)[N]) : view(arr, N - 1)
			{
				static_assert(N > 0);
			}

			StringView(std::string_view newView) : view(newView)
			{
			}

			StringView(const std::string &str) : view(str)
			{
			}

			StringView(const ByteString &s) : view(s.data(), s.size())
			{
			}

			operator std::string_view() const
			{
				return view;
			}
		};

		struct ComponentKey
		{
			using NumericKey = int32_t;
			std::variant<
				std::string_view,
				NumericKey
			> key;

			ComponentKey(const char *newKey) : key(newKey)
			{
			}

			ComponentKey(StringView newKey) : key(newKey)
			{
			}

			ComponentKey(NumericKey newKey) : key(newKey)
			{
			}

			std::string_view GetBytes() const;
		};

		struct MinSizeFitChildren
		{
			auto operator <=>(const MinSizeFitChildren &) const = default;
		};
		using MinSize = std::variant<
			Size,
			MinSizeFitChildren
		>;
		struct MaxSizeInfinite
		{
			auto operator <=>(const MaxSizeInfinite &) const = default;
		};
		struct MaxSizeFitParent
		{
			auto operator <=>(const MaxSizeFitParent &) const = default;
		};
		using MaxSize = std::variant<
			Size,
			MaxSizeInfinite,
			MaxSizeFitParent
		>;
		struct SpanAll
		{
			auto operator <=>(const SpanAll &) const = default;
		};
		using SetSizeSize = std::variant<
			Size,
			SpanAll
		>;
		template<class Item>
		struct ExtendedSize2
		{
			Item X, Y;

			auto operator <=>(const ExtendedSize2 &) const = default;
		};

		using AxisBase = int32_t;
		enum class Axis : AxisBase
		{
			horizontal,
			vertical,
		};

		enum class Order
		{
			lowToHigh,
			highToLow,
			leftToRight = lowToHigh,
			rightToLeft = highToLow,
			topToBottom = lowToHigh,
			bottomToTop = highToLow,
		};

		struct ScrollpanelDimension
		{
			enum class Visibility
			{
				never,
				ifUseful,
				always,
			};
			Visibility visibility = Visibility::ifUseful;
			bool reverseSide = false;
		};

	private:
		struct Component;
		using ComponentIndex = CrossFrameItem::DerivedIndex<Component>;

		struct TextContext
		{
			std::optional<ShapeTextParameters> stp;
			std::optional<Pos2> mouseDownPos;
			TextWrapper::ClearIndex selectionMax = 0;
			TextWrapper::ClearIndex selectionBegin = 0;
			TextWrapper::ClearIndex selectionEnd = 0;

			TextContext(View &)
			{
			}
		};
		struct TextboxContext
		{
			ComponentIndex textComponentIndex;
			std::optional<Pos2> upDownPrev;
			bool changed = false;

			TextboxContext(View &)
			{
			}
		};
		struct DropdownContext
		{
			std::vector<std::string> lastItems;
			std::vector<std::string> items;
			std::shared_ptr<DropdownView> dropdownView;
			int32_t lastSelected = 0;
			bool clicked = false;
			bool changed = false;

			DropdownContext(View &)
			{
			}
		};
		struct ScrollpanelContext
		{
			ExtendedSize2<ScrollpanelDimension> dimension; // laid out similarly to Component::Layout::scroll
			ExtendedSize2<MomentumAnimation<float>> stickyScroll;
			std::optional<Pos2> mouseDownInThumbPos;

			ScrollpanelContext(View &newView) : stickyScroll({
				{ newView.GetHost(), MomentumAnimation<float>::ExponentialProfile{ 0.297553f, 30.f } }, // 0.297553f = std::pow(0.98f, 60.f)
				{ newView.GetHost(), MomentumAnimation<float>::ExponentialProfile{ 0.297553f, 30.f } },
			})
			{
			}
		};
		using ComponentContext = std::variant<
			TextContext,
			TextboxContext,
			DropdownContext,
			ScrollpanelContext
		>;

		using ComponentKeyStoreIndex = int32_t;
		using ComponentKeyStoreSize = int32_t;
		struct ComponentKeyStoreSpan
		{
			ComponentKeyStoreIndex base;
			ComponentKeyStoreSize size;
		};
		struct Component : public CrossFrameItem
		{
			ComponentKeyStoreSpan key;
			std::optional<ComponentIndex> firstChildIndex;
			std::optional<ComponentIndex> nextSiblingIndex;
			std::optional<ComponentIndex> forwardInputFocusIndex;
			Rect rect = RectSized(Pos2{ 0, 0 }, Size2{ 0, 0 });
			bool fillSatisfied = false;
			bool hovered = false;
			Size2 contentSpace = { 0, 0 };
			Size2 contentSize = { 0, 0 };
			Size2 overflow = { 0, 0 };

			struct Layout
			{
				Axis primaryAxis = Axis::horizontal;
				bool layered = false;
				Size spacingFromParent = 0; // not directly user-configurable, only through the parent's spacing and AddSpacing
				ExtendedSize2<Alignment> alignment = { Alignment::center, Alignment::center };
				ExtendedSize2<Order> order = { Order::lowToHigh, Order::lowToHigh };
				// in the case of these properties, the meaning of primary vs secondary (i.e. which one is
				// horizontal and which one is vertical) is evaluated from the parent's perspective, i.e.
				// its primaryAxis property. primary is stored in x while secondary is stored in y
				Size2 parentFillRatio = { 1, 1 };
				ExtendedSize2<MinSize> minSize = { MinSizeFitChildren{}, MinSizeFitChildren{} };
				ExtendedSize2<MaxSize> maxSize = { MaxSizeInfinite{}, MaxSizeInfinite{} };
				Pos2 scroll = { 0, 0 };
				Pos2 paddingBefore = { 0, 0 };
				Pos2 paddingAfter = { 0, 0 };

				bool operator ==(const Layout &) const = default;
			};
			Layout prevLayout;

			struct Content
			{
				bool enabled = true;
				Alignment horizontalTextAlignment = Alignment::center;
				Alignment verticalTextAlignment = Alignment::center;
				// these two are (horizontal, vertical)
				Pos2 textPaddingBefore = { 0, 0 };
				Pos2 textPaddingAfter = { 0, 0 };

				bool operator ==(const Content &) const = default;
			};
			Content prevContent;

			// user-configurable properties
			Size spacing = 0;
			Layout layout;
			Content content;
			bool handleButtons = false;
			bool handleWheel = false;
			bool handleInput = false;

#if DebugGuiView
			bool debugMe = false;
#endif

			std::unique_ptr<ComponentContext> context;
		};

		class ComponentStore : public CrossFrameItemStore<ComponentStore, Component>
		{
			View &view; // TODO-REDO_UI: turn into a base

#if DebugGuiView
		public:
#endif
			std::vector<char> keyData;

			std::string_view DereferenceSpan(ComponentKeyStoreSpan span) const;
			void PruneKeys();

		public:
			ComponentStore(View &newView) : view(newView)
			{
			}

			ComponentKeyStoreSpan InternKey(ComponentKey key);
			bool CompareKey(ComponentKeyStoreSpan interned, ComponentKey key) const;
#if DebugGuiView
			void DumpKey(ComponentKeyStoreSpan interned) const;
#endif

			struct ChildRange;
			struct ChildIterator // TODO-REDO_UI: turn into a proper iterator
			{
				ComponentStore &store;
				std::optional<ComponentIndex> index;

				bool operator !=(const ChildIterator &other) const;
				Component &operator *();
				ChildIterator &operator ++();
			};

			struct ChildRange // TODO-REDO_UI: turn into a proper range
			{
				ComponentStore &store;
				Component &component;

				ChildIterator begin()
				{
					return { store, component.firstChildIndex };
				}

				ChildIterator end()
				{
					return { store, std::nullopt };
				}
			};

			ChildRange GetChildRange(Component &component)
			{
				return { *this, component };
			}

			ComponentIndex Alloc();
			void Free(CrossFrameItem::Index index);
			bool EndFrame();
		};
		ComponentStore componentStore{ *this };

		struct ComponentStackItem
		{
			ComponentIndex index;
			Rect clipRect;
		};
		std::deque<ComponentStackItem> componentStack;
		std::optional<ComponentIndex> rootIndex;
		std::optional<ComponentIndex> prevSiblingIndex;

		Size extraSpacing = 0;

		std::optional<Rect> rootRect;
		Size2 rootEffectiveSize = { 0, 0 };

		ComponentIndex FindOrAllocComponent(ComponentKey key);
		Component &GetOrAllocComponent(ComponentKey key);
		std::optional<ComponentIndex> GetCurrentComponentIndex() const;
		Component *GetCurrentComponent();
		const Component *GetCurrentComponent() const;
		std::optional<ComponentIndex> GetParentComponentIndex() const;
		Component *GetParentComponent();
		const Component *GetParentComponent() const;
		ComponentIndex GetComponentIndex(const Component &component) const;

		template<class Context>
		Context &GetContext(Component &component)
		{
			return std::get<Context>(*component.context);
		}

		template<class Context>
		const Context &GetContext(const Component &component) const
		{
			return std::get<Context>(*component.context);
		}

		template<class Context>
		Context &SetContext(Component &component)
		{
			if (!component.context)
			{
				component.context = std::make_unique<ComponentContext>(std::in_place_type_t<Context>{}, *this);
			}
			return GetContext<Context>(component);
		}

		bool shouldRepeatFrame = false;
		void UpdateShouldRepeatFrame(Component &component);

		bool shouldUpdateLayout = false;
		void UpdateShouldUpdateLayout(Component &component);
		void UpdateLayout();
		void UpdateLayoutContentSize(Axis parentPrimaryAxis, Component &component);
		void UpdateLayoutFillSpace(Component &component);

#if DebugGuiView
		int32_t debugFindIterationCount = 0;
		int32_t debugRelinkIterationCount = 0;
		void DebugDump(int32_t depth, Component &component);
#endif

		std::optional<Pos2> mousePos; // TODO-REDO_UI: move mouse tracking to Host
		bool exitWhenRootMouseDown = false;
		std::optional<ComponentIndex> underMouseIndex;
		std::optional<ComponentIndex> handleButtonsIndex;
		std::optional<ComponentIndex> handleWheelIndex;
		struct ComponentMouseButtonEvent
		{
			ComponentIndex component;
			MouseButtonIndex button;
		};
		std::optional<ComponentMouseButtonEvent> componentMouseDownEvent;
		std::optional<ComponentMouseButtonEvent> componentMouseClickEvent;
		struct ComponentMouseWheelEvent
		{
			ComponentIndex component;
			Pos2 distance;
		};
		std::optional<ComponentMouseWheelEvent> componentMouseScrollEvent;
		std::optional<ComponentIndex> handleInputIndex;
		struct InputFocus
		{
			ComponentIndex component;
			struct KeyDownEvent
			{
				int sym;
				bool repeat;
				bool ctrl;
				bool shift;
				bool alt;
			};
			struct TextInputEvent
			{
				std::string input;
			};
			using Event = std::variant<
				KeyDownEvent,
				TextInputEvent
			>;
			std::deque<Event> events;
		};
		std::optional<InputFocus> inputFocus;
		void UpdateUnderMouse(Component &component);
		void SetInputFocus(std::optional<ComponentIndex> componentIndex);

		virtual void Gui() = 0;

		bool onTop = false;
		virtual void SetOnTop(bool newOnTop); // No need to call this from overriding functions.

		bool rendererUp = false;
		virtual void SetRendererUp(bool newRendererUp); // No need to call this from overriding functions.

		Stack *stack = nullptr;
		void HandleMouseEnter();
		void HandleMouseLeave();
		void HandleWindowHidden();
		void HandleWindowShown();

#if DebugGuiView
	protected:
		bool shouldDoDebugDump = false;
#endif

		Host &host;

	public:
		View(Host &newHost) : host(newHost)
		{
		}
		virtual ~View() = default;

		virtual void HandleBeforeFrame(); // No need to call this from overriding functions.
		virtual bool HandleEvent(const SDL_Event &event); // Call this at the end of overriding functions if the event should be subject to default handling.
		virtual void HandleTick(); // No need to call this from overriding functions.
		virtual void HandleBeforeGui(); // No need to call this from overriding functions.
		virtual bool HandleGui(); // No need to call this from overriding functions.
		virtual void HandleAfterGui(); // No need to call this from overriding functions.
		virtual void HandleAfterFrame(); // No need to call this from overriding functions.

		static bool MayBeHandledExclusively(const SDL_Event &event);
		enum class ExitEventType
		{
			none,
			exit,
			tryOk,
		};
		static ExitEventType ClassifyExitEvent(const SDL_Event &event);

		bool HandleExitEvent(const SDL_Event &event)
		{
			if (ClassifyExitEvent(event) == Gui::View::ExitEventType::exit)
			{
				Exit();
				return true;
			}
			return false;
		}

		template<class OkHandler>
		bool HandleExitEvent(const SDL_Event &event, OkHandler &okHandler)
		{
			switch (ClassifyExitEvent(event))
			{
			case Gui::View::ExitEventType::none:
				break;

			case Gui::View::ExitEventType::exit:
				Exit();
				return true;

			case Gui::View::ExitEventType::tryOk:
				if (okHandler.CanTryOk())
				{
					okHandler.Ok();
					Exit();
				}
				return true;
			}
			return false;
		}

		bool GetOnTop() const
		{
			return onTop;
		}
		void SetOnTopOuter(bool newOnTop);

		bool GetRendererUp() const
		{
			return rendererUp;
		}
		void SetRendererUpOuter(bool newRendererUp);

		Stack *GetStack() const
		{
			return stack;
		}
		void SetStack(Stack *newStack)
		{
			stack = newStack;
		}

		void Exit();
		void PushAboveThis(std::shared_ptr<View> view);

		Host &GetHost() const
		{
			return host;
		}

		void SetPrimaryAxis(Axis newPrimaryAxis);
		void SetLayered(bool newLayered);
		void SetParentFillRatio(Size newParentFillRatio);
		void SetParentFillRatioSecondary(Size newParentFillRatioSecondary);
		void SetMinSize(MinSize newMinSize);
		void SetMinSizeSecondary(MinSize newMinSizeSecondary);
		void SetMaxSize(MaxSize newMaxSize);
		void SetMaxSizeSecondary(MaxSize newMaxSizeSecondary);
		void SetSize(SetSizeSize newSize);
		void SetSizeSecondary(SetSizeSize newSizeSecondary);
		void SetScroll(Size newScroll);
		void SetScrollSecondary(Size newScrollSecondary);
		void SetHaveScrollbar(bool newHaveScrollbar);
		void SetHaveScrollbarSecondary(bool newHaveScrollbarSecondary);
		void SetAlignment(Alignment newAlignment);
		void SetAlignmentSecondary(Alignment newAlignmentSecondary);
		void SetOrder(Order newOrder);
		void SetOrderSecondary(Order newOrderSecondary);
		void SetSpacing(Size newSpacing);
		void AddSpacing(Size addSpacing);
		void SetPadding(Size newPadding);
		void SetPadding(Size newPadding, Size newPaddingSecondary);
		void SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondary);
		void SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondaryBefore, Size newPaddingSecondaryAfter);
		void SetHandleButtons(bool newHandleButtons);
		void SetHandleWheel(bool newHandleWheel);
		void SetHandleInput(bool newHandleInput);
#if DebugGuiView
		void SetDebugMe(bool newDebugMe);
#endif
		void SetTextAlignment(Alignment newHorizontalTextAlignment, Alignment newVerticalTextAlignment);
		void SetTextPadding(Size newHorizontalPadding);
		void SetTextPadding(Size newHorizontalPadding, Size newVerticalPadding);
		void SetTextPadding(Size newHorizontalPaddingBefore, Size newHorizontalPaddingAfter, Size newVerticalPaddingBefore, Size newVerticalPaddingAfter);

		void SetRootRect(std::optional<Rect> newRootRect);
		Size2 GetRootEffectiveSize() const;
		void SetMousePos(std::optional<Pos2> newMousePos);
		std::optional<Pos2> GetMousePos() const;
		void SetExitWhenRootMouseDown(bool newExitWhenRootMouseDown);

		bool IsHovered() const;
		Size GetOverflow() const;
		Size GetOverflowSecondary() const;
		Rect GetRect() const;
		bool IsMouseDown() const; // don't want to include sdl headers just for SDL_BUTTON_LEFT
		bool IsMouseDown(MouseButtonIndex button) const;
		bool IsClicked() const;
		bool IsClicked(MouseButtonIndex button) const;
		std::optional<Pos2> GetScrollDistance() const;
		bool HasInputFocus() const;
		void GiveInputFocus();
		void SetEnabled(bool newEnabled);

		void RequestRepeatFrame();

		void BeginComponent(ComponentKey key);
		void EndComponent();

		void BeginPanel(ComponentKey key);
		void BeginHPanel(ComponentKey key);
		void BeginVPanel(ComponentKey key);
		void EndPanel();

		void BeginScrollpanel(ComponentKey key);
		void BeginScrollpanel(ComponentKey key, ScrollpanelDimension primary);
		void BeginScrollpanel(ComponentKey key, ScrollpanelDimension primary, ScrollpanelDimension secondary);
		void EndScrollpanel();

		void BeginModal(ComponentKey key);
		void EndModal();

		void BeginDialog(ComponentKey key, StringView title, std::optional<int32_t> size = std::nullopt);
		bool EndDialog(bool allowOk);

		template<class OkHandler>
		void EndDialog(OkHandler &okHandler)
		{
			if (EndDialog(okHandler.CanTryOk()))
			{
				okHandler.Ok();
			}
		}

		using TextFlagBase = uint32_t;
		enum class TextFlags : TextFlagBase
		{
			none       = 0,
			selectable = 1 << 0,
			multiline  = 1 << 1,
			autoWidth  = 1 << 2,
			autoHeight = 1 << 3,
		};
		void BeginText(ComponentKey key, StringView text, TextFlags textFlags, Rgba8 color = 0xFFFFFFFF_argb);
		void EndText();
		void Text(StringView text, Rgba8 color = 0xFFFFFFFF_argb);
		void Text(ComponentKey key, StringView text, Rgba8 color = 0xFFFFFFFF_argb);

		using ButtonFlagBase = uint32_t;
		enum class ButtonFlags : ButtonFlagBase
		{
			none      = 0,
			stuck     = 1 << 0,
			autoWidth = 1 << 1,
		};
		void BeginButton(ComponentKey key, StringView text, ButtonFlags buttonFlags, Rgba8 color = 0xFFFFFFFF_argb);
		bool EndButton();
		bool Button(StringView text, SetSizeSize size = SpanAll{}, ButtonFlags buttonFlags = ButtonFlags::none);
		bool Button(ComponentKey key, StringView text, SetSizeSize size = SpanAll{}, ButtonFlags buttonFlags = ButtonFlags::none);

		using CheckboxFlagBase = uint32_t;
		enum class CheckboxFlags : CheckboxFlagBase
		{
			none      = 0,
			autoWidth = 1 << 0,
			multiline = 1 << 1,
			round     = 1 << 2,
		};
		void BeginCheckbox(ComponentKey key, StringView text, bool &checked, CheckboxFlags checkboxFlags, Rgba8 color = 0xFFFFFFFF_argb);
		bool EndCheckbox();
		bool Checkbox(StringView text, bool &checked, SetSizeSize size = SpanAll{}, CheckboxFlags checkboxFlags = CheckboxFlags::none);
		bool Checkbox(ComponentKey key, StringView text, bool &checked, SetSizeSize size = SpanAll{}, CheckboxFlags checkboxFlags = CheckboxFlags::none);

		void BeginColorButton(ComponentKey key, Rgb8 color = 0xFFFFFF_rgb);
		bool EndColorButton();

		using TextboxFlagBase = uint32_t;
		enum class TextboxFlags : TextboxFlagBase
		{
			none      = 0,
			multiline = 1 << 0,
		};
		void BeginTextbox(ComponentKey key, std::string &str, TextboxFlags textboxFlags, Rgba8 color = 0xFFFFFFFF_argb);
		bool EndTextbox();
		void TextboxSelectAll();
		bool Textbox(ComponentKey key, std::string &str, SetSizeSize size = SpanAll{});

		void BeginDropdown(ComponentKey key, int32_t &selected);
		bool EndDropdown();
		void DropdownItem(StringView item);
		bool DropdownGetChanged() const;
		bool Dropdown(ComponentKey key, std::span<StringView> items, int32_t &selected, SetSizeSize size = SpanAll{});

		template<class Enum>
		void BeginDropdown(ComponentKey key, Enum &selected)
		{
			auto proxy = int32_t(selected);
			BeginDropdown(key, proxy);
			selected = Enum(proxy);
		}

		template<class Enum>
		bool Dropdown(ComponentKey key, std::span<StringView> items, Enum &selected, SetSizeSize size = SpanAll{})
		{
			auto proxy = int32_t(selected);
			auto changed = Dropdown(key, items, proxy, size);
			selected = Enum(proxy);
			return changed;
		}

		void Separator(ComponentKey key, Rgba8 color = 0xFFFFFFFF_argb);
		void Separator(ComponentKey key, int32_t size, Rgba8 color = 0xFFFFFFFF_argb);

		void TextSeparator(ComponentKey key, StringView text, Rgba8 color = 0xFFFFFFFF_argb);
		void TextSeparator(ComponentKey key, StringView text, int32_t size, Rgba8 color = 0xFFFFFFFF_argb);
		void BeginTextSeparator(ComponentKey key, StringView text, int32_t size, Rgba8 color);
		void EndTextSeparator();
	};

	constexpr View::ButtonFlags operator |(View::ButtonFlags lhs, View::ButtonFlags rhs)
	{
		return View::ButtonFlags(View::ButtonFlagBase(lhs) | View::ButtonFlagBase(rhs));
	}

	constexpr View::CheckboxFlags operator |(View::CheckboxFlags lhs, View::CheckboxFlags rhs)
	{
		return View::CheckboxFlags(View::CheckboxFlagBase(lhs) | View::CheckboxFlagBase(rhs));
	}

	constexpr View::TextFlags operator |(View::TextFlags lhs, View::TextFlags rhs)
	{
		return View::TextFlags(View::TextFlagBase(lhs) | View::TextFlagBase(rhs));
	}
}
