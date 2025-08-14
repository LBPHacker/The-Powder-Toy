#pragma once
#include "Gui/View.hpp"
#include "Gui/ComplexInput.hpp"
#include "common/Plane.h"
#include "graphics/Pixel.h"

namespace Powder::Gui
{
	class StaticTexture;
}

namespace Powder::Activity
{
	class Game;

	class ColorPicker : public Gui::View
	{
		Game &game;
		Rgba8 prevColor;
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
			std::unique_ptr<Gui::StaticTexture> texture;
		};
		TextureInfo hueSaturationBackground;
		TextureInfo valueBackground;
		TextureInfo redBackground;
		TextureInfo greenBackground;
		TextureInfo blueBackground;
		TextureInfo alphaBackground;
		TextureInfo sampleBackground;
		template<class Func>
		void DrawSliderBackground(
			TextureInfo &textureInfo,
			Rect rect,
			Rect range,
			Func &&func
		);

		void Gui() final override;
		CanApplyKind CanApply() const final override;
		void Apply() final override;
		bool Cancel() final override;

	public:
		ColorPicker(Game &newGame);
		~ColorPicker();

		static PlaneAdapter<std::vector<pixel>> GetTransparencyPattern(Size2 size);
	};
}
