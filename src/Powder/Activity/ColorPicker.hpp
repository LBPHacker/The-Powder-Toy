#pragma once
#include "Gui/View.hpp"
#include "Gui/ComplexInput.hpp"
#include "common/Plane.h"
#include "graphics/Pixel.h"
#include <functional>

namespace Powder::Gui
{
	class Host;
	class StaticTexture;
}

namespace Powder::Activity
{
	class ColorPicker : public Gui::View
	{
		bool withAlpha;
		Rgba8 prevColor;
		std::function<void (Rgba8)> done;
		struct ColorComponent
		{
			Gui::NumberInputContext<Pos> input;
			Pos value = 0;
		};
		ColorComponent hue;
		ColorComponent saturation;
		ColorComponent value;
		ColorComponent red;
		ColorComponent green;
		ColorComponent blue;
		ColorComponent alpha;
		Gui::NumberInputContext<Rgba8> argb32;

		struct TextureInfo
		{
			Rect rect{ { 0, 0 }, { 0, 0 } };
			Rect range{ { 0, 0 }, { 0, 0 } };
			std::shared_ptr<Gui::StaticTexture> texture;
		};
		TextureInfo hueSaturationBackground;
		TextureInfo valueBackground;
		TextureInfo redBackground;
		TextureInfo greenBackground;
		TextureInfo blueBackground;
		TextureInfo alphaBackground;
		TextureInfo sampleBackground;
		template<class Func>
		void SetSliderBackground(
			TextureInfo &textureInfo,
			Rect range,
			Func &&func
		);

		void Gui() final override;
		DispositionFlags GetDisposition() const final override;
		void Ok() final override;

	public:
		ColorPicker(Gui::Host &newHost, bool newWithAlpha, Rgba8 initialColor, std::function<void (Rgba8)> newDone);
		~ColorPicker();

		static PlaneAdapter<std::vector<pixel>> GetTransparencyPattern(Size2 size);
	};
}
