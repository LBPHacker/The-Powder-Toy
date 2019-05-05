#include "simulation/ElementCommon.h"
//#TPT-Directive ElementClass Element_ALHL PT_ALHL 187
Element_ALHL::Element_ALHL()
{
	Identifier = "DEFAULT_PT_ALHL";
	Name = "ALHL";
	Colour = PIXPACK(0xD5DEB8);
	MenuVisible = 1;
	MenuSection = SC_LIQUID;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 20;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 15;

	Temperature = R_TEMP-2.0f	+273.15f;
	HeatConduct = 35;
	Description = "Alcohol.";

	Properties = TYPE_LIQUID|PROP_CONDUCTS|PROP_LIFE_DEC|PROP_NEUTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &Element_ALHL::update;
}

//#TPT-Directive ElementHeader Element_ALHL static int update(UPDATE_FUNC_ARGS)
int Element_ALHL::update(UPDATE_FUNC_ARGS)
{
	//for (int rx=-1; rx<2; rx++)
	//{
		//for (int ry=-1; ry<2; ry++)
		//{
			//if (BOUNDS_CHECK && (rx || ry))
			//{
				//int r = pmap[y+ry][x+rx];
				//if (!r)
				//{
					//continue;
				//}
				//// something?
			//}
		//}
	//}
	return 0;
}

Element_ALHL::~Element_ALHL() {}
