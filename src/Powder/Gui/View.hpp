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
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_Cursor SDL_Cursor;

#define DebugGuiView 0

namespace Powder::Gui
{
	class Host;
	class Stack;
	class StaticTexture;

	class View : public NoCopy
	{
	public:
		using TextFlagBase = uint32_t;
		enum class TextFlags : TextFlagBase
		{
			none            = 0,
			selectable      = 1 << 0,
			multiline       = 1 << 1,
			autoWidth       = 1 << 2,
			autoHeight      = 1 << 3,
			replaceWithDots = 1 << 4,
		};

		using TextboxFlagBase = uint32_t;
		enum class TextboxFlags : TextboxFlagBase
		{
			none      = 0,
			multiline = 1 << 0,
			password  = 1 << 1,
		};

		using Pos              = int32_t;
		using Size             = int32_t;
		using Pos2             = Vec2<Pos>;
		using Mat2x2           = Mat2<Pos>;
		using Size2            = Vec2<Size>;
		using Rect             = ::Rect<int32_t>;
		using MouseButtonIndex = int32_t;

		static constexpr Pos2 RobustClamp(Pos2 pos, Rect rect)
		{
			pos.X = std::max(std::min(pos.X, rect.BottomRight().X), rect.TopLeft().X);
			pos.Y = std::max(std::min(pos.Y, rect.BottomRight().Y), rect.TopLeft().Y);
			return pos;
		}

		using CursorBase = int32_t;
		enum class Cursor : CursorBase
		{
			arrow,
			ibeam,
			wait,
			crosshair,
			waitarrow,
			sizenwse,
			sizenesw,
			sizewe,
			sizens,
			sizeall,
			no,
			hand,
		};

		struct StringView
		{
			std::string_view view;

			StringView() = default;

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
		struct Common
		{
			auto operator <=>(const Common &) const = default;
		};
		using SetSizeSize = std::variant<
			Size,
			SpanAll,
			Common
		>;
		template<class Item>
		struct ExtendedSize2
		{
			Item X, Y;

			auto operator <=>(const ExtendedSize2 &) const = default;
		};

		using SetPaddingSize = std::variant<
			Size,
			Common
		>;
		using SetSpacingSize = std::variant<
			Size,
			Common
		>;

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
			ShapeTextParameters stp;
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
			std::optional<int32_t> limit;

			TextboxContext(View &)
			{
			}
		};
		struct DropdownContext
		{
			std::vector<std::string> lastItems;
			std::vector<std::string> items;
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
			Pos2 overscroll{ 0, 0 };

			ScrollpanelContext(View &newView) : stickyScroll({
				{ newView.GetHost(), MomentumAnimation<float>::ExponentialProfile{ 0.297553f, 30.f } }, // 0.297553f = std::pow(0.98f, 60.f)
				{ newView.GetHost(), MomentumAnimation<float>::ExponentialProfile{ 0.297553f, 30.f } },
			})
			{
			}
		};
		struct PopupContext
		{
			bool open = false;
			Rect rootRect{ { 0, 0 }, { 0, 0 } };
			ComponentIndex layoutRootIndex;

			PopupContext(View &)
			{
			}
		};
		struct SliderContext
		{
			Pos2 minValue{ 0, 0 };
			Pos2 maxValue{ 0, 0 };
			Pos2 inclusiveOffset{ 0, 0 };
			Rect rangeRect{ { 0, 0 }, { 0, 0 } };
			Rect thumbRect{ { 0, 0 }, { 0, 0 } };
			bool changed = false;
			std::shared_ptr<StaticTexture> background;

			SliderContext(View &)
			{
			}
		};
		struct DraggerContext
		{
			std::optional<Pos2> mouseDownPos;
			std::optional<Pos2> posAtMouseDown;
			bool changed = false;

			DraggerContext(View &)
			{
			}
		};
		using ComponentContext = std::variant<
			TextContext,
			TextboxContext,
			DropdownContext,
			ScrollpanelContext,
			PopupContext,
			SliderContext,
			DraggerContext
		>;

		using ComponentKeyStoreIndex = int32_t;
		using ComponentKeyStoreSize = int32_t;
		struct ComponentKeyStoreSpan
		{
			ComponentKeyStoreIndex base;
			ComponentKeyStoreSize size;
		};
#if DebugGuiView
		uint32_t componentGeneration = 0;
#endif
		struct Component : public CrossFrameItem
		{
#if DebugGuiView
			uint32_t generation;
#endif
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
				// this uses the typical layout, x is horizontal and y is vertical
				std::optional<Pos2> forcePosition;
				Axis primaryAxis = Axis::horizontal;
				bool layered = false;
				bool immaterial = false;
				std::optional<Rect> rootRect;
				Size spacingFromParent = 0; // not directly user-configurable, only through the parent's spacing and AddSpacing
				// in the case of these properties, the meaning of primary vs secondary (i.e. which one is
				// horizontal and which one is vertical) is evaluated from the component's perspective, i.e.
				// its primaryAxis property. primary is stored in x while secondary is stored in y
				ExtendedSize2<Alignment> alignment = { Alignment::center, Alignment::center };
				ExtendedSize2<Order> order = { Order::lowToHigh, Order::lowToHigh };
				// same but evaluated from the parent's perspective
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
				Size textFirstLineIndent = 0;

				bool operator ==(const Content &) const = default;
			};
			Content prevContent;

			// user-configurable properties
			Size spacing = 0;
			Layout layout;
			Content content;
			bool handleMouse = true;
			bool handleButtons = false;
			bool handleWheel = false;
			bool handleInput = false;
			std::optional<Cursor> cursor;

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
			SDL_Texture *prevRenderTarget;
		};
		std::deque<ComponentStackItem> componentStack;
		std::optional<ComponentIndex> rootIndex;
		std::vector<ComponentIndex> layoutRootIndices;
		std::vector<SDL_Texture *> layoutRootTextures; // [0] is an alias for the render target managed by the current Stack
		void ShrinkLayoutRootTextures(int32_t newSize);
		int32_t nextLayoutRootTextureIndex = 0;
		std::optional<ComponentIndex> prevSiblingIndex;

		std::vector<ComponentIndex> popupIndices;
		void ClosePopupInternal(Component &component);

		Size extraSpacing = 0;

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

		bool shouldUpdateLayout = false;
		void UpdateLayout();
		void UpdateLayoutRoot(Component &component);
		void UpdateLayoutContentSize(Axis parentPrimaryAxis, Component &component);
		void UpdateLayoutFillSpace(Component &component);

#if DebugGuiView
		int32_t debugFindIterationCount = 0;
		int32_t debugRelinkIterationCount = 0;
		void DebugDump(int32_t depth, Component &component);
#endif

		std::optional<Pos2> mousePos; // TODO-REDO_UI: move mouse tracking to Host
		bool cancelWhenRootMouseDown = false;
		bool allowGlobalQuit = true;
		std::optional<ByteString> title;
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
				int32_t sym;
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

		bool wantTextInput = false;
		void UpdateTextInput();

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

		Cursor cursor = Cursor::arrow;
		std::vector<SDL_Cursor *> cursors;
		void UpdateCursor();

	protected:
		virtual bool GetWantTextInput() const;

#if DebugGuiView
	protected:
		bool shouldDoDebugDump = false;
#endif

		Host &host;

		void BeginTextInternal(
			ComponentKey key,
			StringView text,
			std::optional<StringView> overflowText,
			std::optional<StringView> placeholderText,
			TextFlags textFlags,
			Rgba8 color
		);
		void BeginTextboxInternal(
			ComponentKey key,
			std::string &str,
			std::optional<StringView> formattedText,
			std::optional<StringView> placeholderText,
			TextboxFlags textboxFlags,
			Rgba8 color
		);
		void BeginSliderInternal(
			ComponentKey key,
			ExtendedSize2<Pos *> value,
			Pos2 minValue,
			Pos2 maxValue,
			Size thumbSize,
			ExtendedSize2<bool> maxInclusive
		);
		void BeginDraggerInternal(
			ComponentKey key,
			ExtendedSize2<Pos *> value,
			bool *mouseDown
		);

	public:
		View(Host &newHost) : host(newHost)
		{
		}
		virtual ~View();

		virtual void HandleBeforeFrame(); // No need to call this from overriding functions.
		virtual bool HandleEvent(const SDL_Event &event); // Call this at the end of overriding functions if the event should be subject to default handling.
		virtual void HandleTick(); // No need to call this from overriding functions.
		virtual void HandleBeforeGui(); // No need to call this from overriding functions.
		virtual bool HandleGui(); // No need to call this from overriding functions.
		virtual void HandleAfterGui(); // No need to call this from overriding functions.
		virtual void HandleAfterFrame(); // No need to call this from overriding functions.

		using DispositionFlagsBase = uint32_t;
		enum class DispositionFlags : DispositionFlagsBase
		{
			none          = 0,
			okCancel      = 0 << 0,
			applyCancel   = 1 << 0,
			yesNo         = 2 << 0,
			wordingBits   = 3 << 0,
			okNormal      = 0 << 2,
			okDisabled    = 1 << 2,
			okMissing     = 2 << 2,
			okBits        = 3 << 2,
			cancelEffort  = 1 << 4,
			cancelMissing = 1 << 5,
		};
		// yes => none
		// no  => okDisabled
		// onlyOk => cancelMissing
		// onlyCancel => okMissing
		virtual DispositionFlags GetDisposition() const; // No need to call this from overriding functions.
		virtual void Ok(); // No need to call this from overriding functions.
		virtual std::optional<std::string> GetOkText() const; // No need to call this from overriding functions.
		virtual bool Cancel(); // No need to call this from overriding functions.

		static bool MayBeHandledExclusively(const SDL_Event &event);
		enum class ExitEventType
		{
			none,
			exit,
			ok,
		};
		ExitEventType ClassifyExitEvent(const SDL_Event &event) const;

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

		const std::optional<ByteString> &GetTitle() const
		{
			return title;
		}
		void SetTitle(std::optional<ByteString> newTitle)
		{
			title = std::move(newTitle);
		}

		virtual void Exit(); // Call this at the end of overriding functions.
		void PushAboveThis(std::shared_ptr<View> view);

		Host &GetHost() const
		{
			return host;
		}

		void SetPrimaryAxis(Axis newPrimaryAxis);
		void SetLayered(bool newLayered);
		void SetImmaterial(bool newImmaterial);
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
		void ForcePosition(std::optional<Pos2> newPosition);
		void SetAlignment(Alignment newAlignment);
		void SetAlignmentSecondary(Alignment newAlignmentSecondary);
		void SetOrder(Order newOrder);
		void SetOrderSecondary(Order newOrderSecondary);
		void SetSpacing(SetSpacingSize newSpacing);
		void AddSpacing(Size addSpacing);
		void SetPadding(SetPaddingSize newPadding);
		void SetPadding(Size newPadding, Size newPaddingSecondary);
		void SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondary);
		void SetPadding(Size newPaddingBefore, Size newPaddingAfter, Size newPaddingSecondaryBefore, Size newPaddingSecondaryAfter);
		void SetHandleMouse(bool newHandleMouse);
		void SetHandleButtons(bool newHandleButtons);
		void SetHandleWheel(bool newHandleWheel);
		void SetHandleInput(bool newHandleInput);
#if DebugGuiView
		void SetDebugMe(bool newDebugMe);
		uint32_t GetComponentGeneration() const
		{
			return GetCurrentComponent()->generation;
		}
#endif
		void SetTextAlignment(Alignment newHorizontalTextAlignment, Alignment newVerticalTextAlignment);
		void SetTextFirstLineIndent(Size newFirstLineIndent);
		void SetTextPadding(Size newPadding);
		void SetTextPadding(Size newHorizontalPadding, Size newVerticalPadding);
		void SetTextPadding(Size newHorizontalPaddingBefore, Size newHorizontalPaddingAfter, Size newVerticalPaddingBefore, Size newVerticalPaddingAfter);
		void SetCursor(std::optional<Cursor> newCursor);

		void SetRootRect(std::optional<Rect> newRootRect);
		Size2 GetRootEffectiveSize() const;
		void SetMousePos(std::optional<Pos2> newMousePos);
		std::optional<Pos2> GetMousePos() const;
		void SetCancelWhenRootMouseDown(bool newExitWhenRootMouseDown);
		void SetAllowGlobalQuit(bool newAllowGlobalQuit);

		bool IsHovered() const;
		Size GetOverflow() const;
		Size GetOverflowSecondary() const;
		Rect GetRect() const;
		bool IsMouseDown(MouseButtonIndex button) const;
		bool IsClicked(MouseButtonIndex button) const;
		std::optional<Pos2> GetScrollDistance() const;
		bool HasInputFocus() const;
		void GiveInputFocus();
		void SetEnabled(bool newEnabled);

		void RequestRepeatFrame();

#define GuiViewScopedProxy(primary, func) \
		template<class ...Args> \
		primary ## ScopedTag Scoped ## func(Args &&...args) \
		{ \
			Begin ## func(std::forward<Args>(args)...); \
			return { *this }; \
		}
#define GuiViewScopedDef(endret, primary) \
		endret End ## primary(); \
		class primary ## ScopedTag : public NoCopy \
		{ \
			View &view; \
		public: \
			primary ## ScopedTag(View &newView) : view(newView) {} \
			~ primary ## ScopedTag() { view.End ## primary(); } \
		}; \
		GuiViewScopedProxy(primary, primary)
#define GuiViewScopedAlt(primary, func) \
		GuiViewScopedProxy(primary, func) \
		void Begin ## func
#define GuiViewScopedUse(func) \
		void Begin ## func
#define GuiViewScoped(endret, func) \
		GuiViewScopedDef(endret, func) \
		GuiViewScopedUse(func)
		GuiViewScoped(void, Component)(ComponentKey key);

		GuiViewScopedDef(void, Panel);
		GuiViewScopedUse(Panel)(ComponentKey key);
		GuiViewScopedAlt(Panel, HPanel)(ComponentKey key);
		GuiViewScopedAlt(Panel, VPanel)(ComponentKey key);

		GuiViewScopedDef(void, Scrollpanel);
		GuiViewScopedUse(Scrollpanel)(ComponentKey key);
		GuiViewScopedUse(Scrollpanel)(ComponentKey key, ScrollpanelDimension primary);
		GuiViewScopedUse(Scrollpanel)(ComponentKey key, ScrollpanelDimension primary, ScrollpanelDimension secondary);
		Pos2 ScrollpanelGetOverscroll() const;
		void ScrollpanelSetScroll(View::Pos2 newScroll);

		GuiViewScoped(void, Modal)(ComponentKey key, Rect rootRect);

		GuiViewScoped(void, Dialog)(ComponentKey key, StringView title, std::optional<Size> size = std::nullopt, bool error = false);

		struct ProgressdialogStateProgress
		{
		};
		struct ProgressdialogStateDone
		{
			bool error;
			StringView message;
		};
		using ProgressdialogState = std::variant<
			ProgressdialogStateProgress,
			ProgressdialogStateDone
		>;
		GuiViewScoped(void, Progressdialog)(ComponentKey key, StringView title, ProgressdialogState state);
		void Progressdialog(ComponentKey key, StringView title, ProgressdialogState state);

		GuiViewScopedDef(void, Text);
		GuiViewScopedUse(Text)(ComponentKey key, StringView text, TextFlags textFlags, Rgba8 color = 0xFFFFFFFF_argb);
		GuiViewScopedUse(Text)(ComponentKey key, StringView text, StringView overflowText, TextFlags textFlags, Rgba8 color = 0xFFFFFFFF_argb);
		void Text(ComponentKey key, StringView text, Rgba8 color = 0xFFFFFFFF_argb);
		int32_t TextGetWrappedLines() const;

		using ButtonFlagBase = uint32_t;
		enum class ButtonFlags : ButtonFlagBase
		{
			none         = 0,
			stuck        = 1 << 0,
			autoWidth    = 1 << 1,
			truncateText = 1 << 2,
		};
		GuiViewScoped(bool, Button)(ComponentKey key, StringView text, ButtonFlags buttonFlags, Rgba8 color = 0xFFFFFFFF_argb, Rgba8 backgroundColor = 0x00000000_argb);
		bool Button(ComponentKey key, StringView text, SetSizeSize size = SpanAll{}, ButtonFlags buttonFlags = ButtonFlags::none);

		using CheckboxFlagBase = uint32_t;
		enum class CheckboxFlags : CheckboxFlagBase
		{
			none      = 0,
			autoWidth = 1 << 0,
			multiline = 1 << 1,
			round     = 1 << 2,
		};
		GuiViewScoped(bool, Checkbox)(ComponentKey key, StringView text, bool &checked, CheckboxFlags checkboxFlags, Rgba8 color = 0xFFFFFFFF_argb);
		bool Checkbox(ComponentKey key, StringView text, bool &checked, SetSizeSize size = SpanAll{}, CheckboxFlags checkboxFlags = CheckboxFlags::none);

		GuiViewScoped(bool, ColorButton)(ComponentKey key, Rgb8 color = 0xFFFFFF_rgb);

		GuiViewScopedDef(bool, Textbox)
		GuiViewScopedUse(Textbox)(ComponentKey key, std::string &str, TextboxFlags textboxFlags, Rgba8 color = 0xFFFFFFFF_argb);
		GuiViewScopedUse(Textbox)(ComponentKey key, std::string &str, StringView placeholderText, TextboxFlags textboxFlags, Rgba8 color = 0xFFFFFFFF_argb);
		GuiViewScopedUse(Textbox)(ComponentKey key, std::string &str, StringView formattedText, StringView placeholderText, TextboxFlags textboxFlags, Rgba8 color = 0xFFFFFFFF_argb);
		void TextboxSelectAll();
		bool Textbox(ComponentKey key, std::string &str, SetSizeSize size = SpanAll{});
		bool Textbox(ComponentKey key, std::string &str, StringView placeholderText, SetSizeSize size = SpanAll{});
		void TextboxSetLimit(std::optional<int32_t> newLimit);

		GuiViewScopedDef(bool, Dropdown);
		GuiViewScopedUse(Dropdown)(ComponentKey key, int32_t &selected);
		void DropdownItem(StringView item);
		bool DropdownGetChanged() const;
		bool Dropdown(ComponentKey key, std::span<StringView> items, int32_t &selected, SetSizeSize size = SpanAll{});

		template<class Enum>
		GuiViewScopedUse(Dropdown)(ComponentKey key, Enum &selected)
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
		void Separator(ComponentKey key, Size size, Rgba8 color = 0xFFFFFFFF_argb);

		void Spinner(ComponentKey key, Size size, Rgba8 color = 0xFFFFFFFF_argb);

		void TextSeparator(ComponentKey key, StringView text, Rgba8 color = 0xFFFFFFFF_argb);
		void TextSeparator(ComponentKey key, StringView text, Size size, Rgba8 color = 0xFFFFFFFF_argb);
		GuiViewScoped(void, TextSeparator)(ComponentKey key, StringView text, Size size, Rgba8 color);

		bool MaybeBeginPopup(ComponentKey key, bool open);
		void EndPopup();
		void PopupSetAnchor(Pos2 newAnchor);
		void PopupClose();

		bool MaybeBeginContextmenu(ComponentKey key);
		bool ContextmenuButton(ComponentKey key, StringView item);
		void EndContextmenu();

		GuiViewScopedDef(bool, Slider)
		GuiViewScopedUse(Slider)(ComponentKey key, Pos &value, Pos minValue, Pos maxValue, Size thumbSize, bool maxInclusive = true);
		GuiViewScopedUse(Slider)(ComponentKey key, Pos2 &value, Pos2 minValue, Pos2 maxValue, Size thumbSize, ExtendedSize2<bool> maxInclusive = { true, true });
		bool Slider(ComponentKey key, Pos &value, Pos minValue, Pos maxValue);
		bool Slider(ComponentKey key, Pos2 &value, Pos2 minValue, Pos2 maxValue);
		Rect SliderGetRange() const;
		Pos SliderMapPosition(Pos pos) const;
		Pos2 SliderMapPosition(Pos2 pos) const;
		void SliderSetBackground(std::shared_ptr<StaticTexture> newBackground);
		Rect SliderGetBackgroundRect() const;

		GuiViewScopedDef(bool, Dragger)
		GuiViewScopedUse(Dragger)(ComponentKey key, Pos &value);
		GuiViewScopedUse(Dragger)(ComponentKey key, Pos2 &value);

		struct Progress
		{
			Size numerator;
			Size denominator;
		};
		void Progressbar(ComponentKey key, std::optional<Progress> progress, SetSizeSize size = SpanAll{});

		void PushMessage(std::string title, std::string message, bool error, std::function<void ()> done);
		void PushConfirm(std::string title, std::string message, std::optional<std::string> okText, std::function<void (bool)> done);
		void PushInput(std::string title, std::string message, std::string initial, std::optional<std::string> placeholder, std::function<bool (std::optional<std::string>)> done);
#undef GuiViewScoped
#undef GuiViewScopedUse
#undef GuiViewScopedAlt
#undef GuiViewScopedDef
#undef GuiViewScopedProxy
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

	constexpr View::DispositionFlags operator |(View::DispositionFlags lhs, View::DispositionFlags rhs)
	{
		return View::DispositionFlags(View::DispositionFlagsBase(lhs) | View::DispositionFlagsBase(rhs));
	}
}
