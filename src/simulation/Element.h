#pragma once
#include "common/Vec2.h"
#include "graphics/Pixel.h"
#include "ElementDefs.h"
#include "Particle.h"
#include "Simulation.h"
#include "StructProperty.h"
#include "ElementNumbers.h"
#include "SimImpls.h"
#include <memory>

template<class, class ...Args>
struct CallbackTupleHelper
{
	using Type = std::tuple<Args...>;
};

#define auto Simulation
using UpdateFunc = int (*)(UPDATE_FUNC_ARGS);
using CreateFunc = void (*)(ELEMENT_CREATE_FUNC_ARGS);
using CreateAllowedFunc = bool (*)(ELEMENT_CREATE_ALLOWED_FUNC_ARGS);
using ChangeTypeFunc = void (*)(ELEMENT_CHANGETYPE_FUNC_ARGS);
#undef auto

template<class Func>
using CallbackHolder = CallbackTupleHelper<int
#define CALLBACK_TYPE(Var) , typename RewriteArg1<SimVariant<Var> *, Func>::Type
ALL_SIM_IMPLS(CALLBACK_TYPE)
#undef CALLBACK_TYPE
>::Type;

class Renderer;
struct GraphicsFuncContext;
class VideoBuffer;
struct Particle;
class Element
{
public:
	ByteString Identifier;
	String Name;
	RGB Colour;
	int MenuVisible;
	int MenuSection;
	int MenuSort;
	int Enabled;

	float Advection;
	float AirDrag;
	float AirLoss;
	float Loss;
	float Collision;
	float Gravity;
	float NewtonianGravity;
	float Diffusion;
	float HotAir;
	int Falldown;
	int Flammable;
	int Explosive;
	int Meltable;
	int Hardness;
	// Photon wavelengths are ANDed with this value when a photon hits an element, meaning that only wavelengths present in both this value and the original photon will remain in the reflected photon
	unsigned int PhotonReflectWavelengths;
	int Weight;
	unsigned char HeatConduct;
	float HeatCapacity; // Volumetric heat capacity per one pixel. Must be nonzero. The default value is 1.0f.
	String Description;
	unsigned int Properties;
	unsigned int CarriesTypeIn;

	float LowPressure;
	int LowPressureTransition;
	float HighPressure;
	int HighPressureTransition;
	float LowTemperature;
	int LowTemperatureTransition;
	float HighTemperature;
	int HighTemperatureTransition;

	CallbackHolder<UpdateFunc> Update;
	int (*Graphics) (GRAPHICS_FUNC_ARGS);

	CallbackHolder<CreateFunc> Create;
	CallbackHolder<CreateAllowedFunc> CreateAllowed;
	CallbackHolder<ChangeTypeFunc> ChangeType;

	bool (*CtypeDraw) (CTYPEDRAW_FUNC_ARGS);

	std::unique_ptr<VideoBuffer> (*IconGenerator)(int, Vec2<int>);

	Particle DefaultProperties;

	Element();
	static int defaultGraphics(GRAPHICS_FUNC_ARGS);
	static int legacyUpdate(UPDATE_FUNC_ARGS);
	static bool basicCtypeDraw(CTYPEDRAW_FUNC_ARGS);
	static bool ctypeDrawVInTmp(CTYPEDRAW_FUNC_ARGS);
	static bool ctypeDrawVInCtype(CTYPEDRAW_FUNC_ARGS);

	/** Returns a list of properties, their type and offset within the structure that can be changed
	 by higher-level processes referring to them by name such as Lua or the property tool **/
	static std::vector<StructProperty> const &GetProperties();

#define ELEMENT_NUMBERS_DECLARE(name, id) void Element_ ## name ();
	ELEMENT_NUMBERS(ELEMENT_NUMBERS_DECLARE)
#undef ELEMENT_NUMBERS_DECLARE
};

#define ASSIGN_SIM_CALLBACK_INNER(Var, Callback, Template) std::get<VariantIndex<SimImpls, Var>()>(Callback) = Template;
#define ASSIGN_SIM_CALLBACK(Callback, Template) ALL_SIM_IMPLS(ASSIGN_SIM_CALLBACK_INNER, Callback, Template)
