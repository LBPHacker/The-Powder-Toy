#pragma once

namespace gui
{
	class Alignment
	{
	public:
		static const int horizBits   = 3 << 0;
		static const int horizLeft   = 0 << 0;
		static const int horizCenter = 1 << 0;
		static const int horizRight  = 2 << 0;

		static const int vertBits    = 3 << 2;
		static const int vertTop     = 0 << 2;
		static const int vertCenter  = 1 << 2;
		static const int vertBottom  = 2 << 2;
	};
}
