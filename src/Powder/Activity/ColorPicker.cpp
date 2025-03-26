#include "ColorPicker.hpp"
#include "Game.hpp"
#include "Misc.h"
#include "Gui/Host.hpp"
#include "Gui/StaticTexture.hpp"

namespace Powder::Activity
{
	namespace
	{
		constexpr Gui::View::Size dialogWidth  = 357;
		constexpr Gui::View::Size hsAreaHeight = 130;

		std::optional<Gui::View::Pos> ParseComponent(
			const std::string &str,
			Gui::View::Pos minValue,
			Gui::View::Pos maxValue,
			bool maxInclusive
		)
		{
			try
			{
				auto newPage = ByteString(str).ToNumber<Gui::View::Pos>();
				if (newPage >= minValue && (maxInclusive ? (newPage <= maxValue) : (newPage < maxValue)))
				{
					return newPage;
				}
			}
			catch (const std::runtime_error &)
			{
			}
			return std::nullopt;
		}

		inline std::optional<Rgba8> ParseArgb32(const std::string &str, bool withAlpha)
		{
			std::istringstream ss(str);
			uint32_t value;
			if (!(ss >> std::hex >> value))
			{
				return std::nullopt;
			}
			auto color = RGBA::Unpack(value);
			if (!withAlpha)
			{
				color.Alpha = 255;
			}
			return color;
		}
	}

	PlaneAdapter<std::vector<pixel>> ColorPicker::GetTransparencyPattern(Size2 size)
	{
		constexpr int32_t patternSize = 2;
		PlaneAdapter<std::vector<pixel>> pattern(size);
		for (auto p : pattern.Size().OriginRect())
		{
			pattern[p] = (((p.X / patternSize + p.Y / patternSize) & 1) ? 0xFFFFFF_rgb : 0xC8C8C8_rgb).Pack();
		}
		return pattern;
	}

	template<class Func>
	void ColorPicker::SetSliderBackground(
		TextureInfo &textureInfo,
		Rect range,
		Func &&func
	)
	{
		auto rect = SliderGetBackgroundRect();
		if (rect.size.X < 0 || rect.size.Y < 0)
		{
			return;
		}
		auto &g = GetHost();
		range.pos -= rect.pos;
		if (textureInfo.rect != rect || textureInfo.range != range)
		{
			textureInfo.rect = rect;
			textureInfo.range = range;
			textureInfo.texture.reset();
		}
		if (!textureInfo.texture)
		{
			PlaneAdapter<std::vector<pixel>> data = GetTransparencyPattern(rect.size);
			auto dataRect = rect.size.OriginRect();
			for (auto p : dataRect)
			{
				auto color = RGB::Unpack(data[p]);
				auto rangeP = Gui::View::RobustClamp(p, range);
				color = color.Blend(func(SliderMapPosition(rect.pos + rangeP)));
				data[p] = color.Pack();
			}
			textureInfo.texture = std::make_shared<Gui::StaticTexture>(g, false, std::move(data));
		}
		SliderSetBackground(textureInfo.texture);
	}

	ColorPicker::ColorPicker(Gui::Host &newHost, bool newWithAlpha, Rgba8 initialColor, std::function<void (Rgba8)> newDone) :
		View(newHost),
		withAlpha(newWithAlpha),
		prevColor(initialColor),
		done(newDone)
	{
		if (!withAlpha)
		{
			prevColor.Alpha = 255;
		}
		red  .value = prevColor.Red  ;
		green.value = prevColor.Green;
		blue .value = prevColor.Blue ;
		alpha.value = prevColor.Alpha;
		RGB_to_HSV(red.value, green.value, blue.value, &hue.value, &saturation.value, &value.value);
	}

	ColorPicker::~ColorPicker() = default;

	void ColorPicker::Gui()
	{
		auto &g = GetHost();
		auto thumbSize = g.GetCommonMetrics().size;
		auto dialog = ScopedDialog("colorPicker", "Pick color", dialogWidth); // TODO-REDO_UI-TRANSLATE
		auto sliderTextbox = [&, this](StringView key, ColorComponent &colorComponent, Pos minValue, Pos maxValue, bool maxInclusive, bool &changed) {
			{
				auto label = ScopedText("label", key, TextFlags::none);
				SetSize(15);
			}
			struct Rwpb
			{
				ColorPicker &view;
				Pos &pos;
				Pos minValue;
				Pos maxValue;
				bool maxInclusive;
				Pos Read() { return pos; }
				void Write(Pos value) { pos = value; }
				std::optional<Pos> Parse(const std::string &str) { return ParseComponent(str, minValue, maxValue, maxInclusive); }
				std::string Build(Pos value) { return ByteString::Build(value); }
			};
			Rwpb rwpb{ *this, colorComponent.value, minValue, maxValue, maxInclusive };
			colorComponent.input.BeginTextbox("textbox", rwpb);
			SetSize(50);
			return colorComponent.input.EndTextbox(rwpb);
		};
		bool hsvChanged = false;
		bool rgbChanged = false;
		bool aChanged = false;
		{
			auto hsPanel = ScopedHPanel("hs");
			SetSpacing(Common{});
			auto hs = Pos2{ hue.value, saturation.value };
			BeginSlider("slider", hs, { 0, 0 }, { 360, 255 }, thumbSize, { false, true });
			SetSizeSecondary(hsAreaHeight);
			SetSliderBackground(hueSaturationBackground, SliderGetRange(), [this](Pos2 p) {
				int32_t r, g, b;
				HSV_to_RGB(p.X, p.Y, value.value, &r, &g, &b);
				return RGBA(r, g, b, alpha.value);
			});
			if (EndSlider())
			{
				hue.value = hs.X;
				saturation.value = hs.Y;
				hsvChanged = true;
			}
			auto right = ScopedVPanel("right");
			SetParentFillRatio(0);
			SetAlignment(Gui::Alignment::bottom);
			SetSpacing(Common{});
			{
				struct Rwpb
				{
					ColorPicker &view;
					Rgba8 Read() { return RGBA(view.red.value, view.green.value, view.blue.value, view.alpha.value); }
					void Write(Rgba8 value)
					{
						view.red  .value = value.Red  ;
						view.green.value = value.Green;
						view.blue .value = value.Blue ;
						if (!view.withAlpha)
						{
							value.Alpha = 255;
						}
						view.alpha.value = value.Alpha;
					}
					std::optional<Rgba8> Parse(const std::string &str) { return ParseArgb32(str, view.withAlpha); }
					std::string Build(Rgba8 value)
					{
						auto width = view.withAlpha ? 8 : 6;
						auto packed = value.Pack() & (view.withAlpha ? UINT32_C(0xFFFFFFFF) : UINT32_C(0x00FFFFFF));
						return ByteString::Build(Format::Hex(), Format::Uppercase(), Format::Width(width), packed);
					}
				};
				Rwpb rwpb{ *this };
				argb32.BeginTextbox("argb32", rwpb);
				SetSize(Common{});
				if (argb32.EndTextbox(rwpb))
				{
					rgbChanged = true;
					aChanged = true;
				}
			}
			{
				auto sample = ScopedComponent("sample");
				auto r = GetRect();
				auto textureRect = r.Inset(2);
				if (textureRect.size.X > 0 && textureRect.size.Y > 0)
				{
					if (sampleBackground.rect != textureRect)
					{
						sampleBackground.rect = textureRect;
						sampleBackground.texture.reset();
					}
					if (!sampleBackground.texture)
					{
						auto data = GetTransparencyPattern(textureRect.size);
						for (auto p : textureRect.size.OriginRect())
						{
							auto color = RGB::Unpack(data[p]);
							auto toBlend = p.Y < textureRect.size.Y / 2 ? prevColor : RGBA(red.value, green.value, blue.value, alpha.value);
							if (p.X < textureRect.size.X / 2)
							{
								toBlend.Alpha = 255;
							}
							data[p] = color.Blend(toBlend).Pack();
						}
						sampleBackground.texture = std::make_shared<Gui::StaticTexture>(g, false, std::move(data));
					}
					g.DrawStaticTexture(textureRect, *sampleBackground.texture, 0xFFFFFFFF_argb);
					g.DrawRect(r, 0xFFFFFFFF_argb);
				}
			}
			auto slider = [&, this](StringView key, ColorComponent &colorComponent, Pos minValue, Pos maxValue, bool maxInclusive, bool &changed) {
				auto top = ScopedHPanel(key);
				SetSize(Common{});
				SetSpacing(Common{});
				changed |= sliderTextbox(key, colorComponent, minValue, maxValue, maxInclusive, changed);
			};
			slider("H", hue, 0, 360, false, hsvChanged);
			slider("S", saturation, 0, 255, true, hsvChanged);
		}
		auto slider = [&, this](StringView key, TextureInfo &textureInfo, ColorComponent &colorComponent, Pos minValue, Pos maxValue, bool &changed, auto func) {
			auto top = ScopedHPanel(key);
			SetSize(Common{});
			SetSpacing(Common{});
			{
				auto vertical = ScopedVPanel("vertical");
				BeginSlider("slider", colorComponent.value, minValue, maxValue, thumbSize, true);
				SetSliderBackground(textureInfo, SliderGetRange(), func);
				if (EndSlider())
				{
					changed = true;
				}
			}
			changed |= sliderTextbox(key, colorComponent, minValue, maxValue, true, changed);
		};
		slider("V", valueBackground, value, 0, 255, hsvChanged, [this](Pos2 p) {
			int32_t r, g, b;
			HSV_to_RGB(hue.value, saturation.value, p.X, &r, &g, &b);
			return RGBA(r, g, b, alpha.value);
		});
		slider("R", redBackground, red, 0, 255, rgbChanged, [this](Pos2 p) {
			return RGBA(p.X, green.value, blue.value, alpha.value);
		});
		slider("G", greenBackground, green, 0, 255, rgbChanged, [this](Pos2 p) {
			return RGBA(red.value, p.X, blue.value, alpha.value);
		});
		slider("B", blueBackground, blue, 0, 255, rgbChanged, [this](Pos2 p) {
			return RGBA(red.value, green.value, p.X, alpha.value);
		});
		if (withAlpha)
		{
			slider("A", alphaBackground, alpha, 0, 255, aChanged, [this](Pos2 p) {
				return RGBA(red.value, green.value, blue.value, p.X);
			});
		}
		if (hsvChanged)
		{
			HSV_to_RGB(hue.value, saturation.value, value.value, &red.value, &green.value, &blue.value);
		}
		if (rgbChanged)
		{
			RGB_to_HSV(red.value, green.value, blue.value, &hue.value, &saturation.value, &value.value);
		}
		if (hsvChanged || rgbChanged || aChanged)
		{
			hueSaturationBackground.texture.reset();
			valueBackground        .texture.reset();
			redBackground          .texture.reset();
			greenBackground        .texture.reset();
			blueBackground         .texture.reset();
			alphaBackground        .texture.reset();
			sampleBackground       .texture.reset();
			hue       .input.ForceRead();
			saturation.input.ForceRead();
			value     .input.ForceRead();
			red       .input.ForceRead();
			green     .input.ForceRead();
			blue      .input.ForceRead();
			alpha     .input.ForceRead();
			argb32          .ForceRead();
		}
	}

	ColorPicker::DispositionFlags ColorPicker::GetDisposition() const
	{
		return DispositionFlags::none;
	}

	void ColorPicker::Ok()
	{
		if (done)
		{
			done(RGBA(red.value, green.value, blue.value, alpha.value));
		}
		Exit();
	}
}
