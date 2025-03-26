#include "LuaScriptInterface.h"
#include "Powder/Activity/Game.hpp"
#include "graphics/Renderer.h"
#include "graphics/Graphics.h"
#include "simulation/ElementGraphics.h"
#include "Misc.h"

static int renderMode(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L))
	{
		lsi->game.GetRendererSettings().renderMode = luaL_checkinteger(L, 1);
		return 0;
	}
	lua_pushinteger(L, lsi->game.GetRendererSettings().renderMode);
	return 1;
}

static int hud(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	int acount = lua_gettop(L);
	if (acount == 0)
	{
		lua_pushboolean(L, lsi->game.hud);
		return 1;
	}
	auto hudstate = lua_toboolean(L, 1);
	lsi->game.hud = hudstate;
	return 0;
}

static int debugHud(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	int acount = lua_gettop(L);
	if (acount == 0)
	{
		lua_pushboolean(L, lsi->game.debugHud);
		return 1;
	}
	auto debug = lua_toboolean(L, 1);
	lsi->game.debugHud = debug;
	return 0;
}

static int useDisplayPreset(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	int cmode = luaL_optint(L, 1, 3);
	if (cmode >= 0 && cmode < 11)
	{
		cmode = (cmode + 1) % 11;
	}
	if (cmode >= 0 && cmode < int32_t(Renderer::renderModePresets.size()))
		lsi->game.UseRendererPreset(cmode);
	else
		return luaL_error(L, "Invalid display mode");
	return 0;
}

static int fireSize(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L) < 1)
	{
		lua_pushnumber(L, lsi->game.GetRendererSettings().fireIntensity);
		return 1;
	}
	lsi->game.GetRendererSettings().fireIntensity = float(luaL_checknumber(L, 1));
	return 0;
}

static int displayMode(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L))
	{
		lsi->game.GetRendererSettings().displayMode = luaL_checkinteger(L, 1);
		return 0;
	}
	lua_pushinteger(L, lsi->game.GetRendererSettings().displayMode);
	return 1;
}


static int colorMode(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L))
	{
		lsi->game.GetRendererSettings().colorMode = luaL_checkinteger(L, 1);
		return 0;
	}
	lua_pushinteger(L, lsi->game.GetRendererSettings().colorMode);
	return 1;
}

static int decorations(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	int acount = lua_gettop(L);
	if (acount == 0)
	{
		lua_pushboolean(L, lsi->game.GetDrawDeco());
		return 1;
	}
	int decostate = lua_toboolean(L, 1);
	lsi->game.SetDrawDeco(decostate);
	return 0;
}

static int grid(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	int acount = lua_gettop(L);
	if (acount == 0)
	{
		lua_pushnumber(L, lsi->game.GetRendererSettings().gridSize);
		return 1;
	}
	int grid = luaL_optint(L, 1, -1);
	lsi->game.GetRendererSettings().gridSize = grid;
	return 0;
}

static int showBrush(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	int acount = lua_gettop(L);
	if (acount == 0)
	{
		lua_pushnumber(L, int(lsi->game.drawBrush));
		return 1;
	}
	int brush = luaL_optint(L, 1, -1);
	lsi->game.drawBrush = bool(brush);
	return 0;
}

static int depth3d(lua_State *L)
{
	GetLSI()->AssertInterfaceEvent();
	return luaL_error(L, "This feature is no longer supported");
}

static int zoomEnabled(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L) == 0)
	{
		lua_pushboolean(L, lsi->game.zoomShown);
		return 1;
	}
	else
	{
		luaL_checktype(L, -1, LUA_TBOOLEAN);
		lsi->game.zoomShown = lua_toboolean(L, -1);
		return 0;
	}
}

static int zoomMetrics(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L) == 0)
	{
		lua_pushnumber(L, lsi->game.zoomMetrics.from.pos.X );
		lua_pushnumber(L, lsi->game.zoomMetrics.from.pos.Y );
		lua_pushnumber(L, lsi->game.zoomMetrics.from.size.X);
		lua_pushnumber(L, lsi->game.zoomMetrics.from.size.Y);
		lua_pushnumber(L, lsi->game.zoomMetrics.to.X       );
		lua_pushnumber(L, lsi->game.zoomMetrics.to.Y       );
		lua_pushnumber(L, lsi->game.zoomMetrics.scale      );
		return 7;
	}
	Powder::Activity::Game::ZoomMetrics newMetrics;
	newMetrics.from.pos.X  = luaL_checkint(L, 1);
	newMetrics.from.pos.Y  = luaL_checkint(L, 2);
	newMetrics.from.size.X = luaL_checkint(L, 3);
	newMetrics.from.size.Y = luaL_checkint(L, 4);
	newMetrics.to.X        = luaL_checkint(L, 5);
	newMetrics.to.Y        = luaL_checkint(L, 6);
	newMetrics.scale       = luaL_checkint(L, 7);
	if (!lsi->game.CheckZoomMetrics(newMetrics))
	{
		return luaL_error(L, "Bad zoom metrics");
	}
	lsi->game.zoomMetrics = newMetrics;
	return 0;
}

static int separateThread(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	if (lua_gettop(L))
	{
		lsi->game.threadedRendering = lua_toboolean(L, 1);
		return 0;
	}
	lua_pushboolean(L, lsi->game.threadedRendering);
	return 1;
}

static int heatDisplayLimits(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	auto &rendererSettings = lsi->game.GetRendererSettings();
	if (lua_gettop(L))
	{
		auto write = [L](auto &setting, int index) {
			if (lua_isstring(L, index) && byteStringEqualsLiteral(tpt_lua_toByteString(L, index), "auto"))
			{
				setting = HdispLimitAuto{};
			}
			else
			{
				setting = HdispLimitExplicit{ float(luaL_checknumber(L, index)) };
			}
		};
		write(rendererSettings.wantHdispLimitMin, 1);
		write(rendererSettings.wantHdispLimitMax, 2);
		return 0;
	}
	auto read = [L](auto &setting) {
		if (auto *hdispLimitExplicit = std::get_if<HdispLimitExplicit>(&setting))
		{
			lua_pushnumber(L, hdispLimitExplicit->value);
		}
		else
		{
			lua_pushliteral(L, "auto");
		}
	};
	read(rendererSettings.wantHdispLimitMin);
	read(rendererSettings.wantHdispLimitMax);
	return 2;
}

static int heatDisplayAutoArea(lua_State *L)
{
	auto *lsi = GetLSI();
	lsi->AssertInterfaceEvent();
	auto &rendererSettings = lsi->game.GetRendererSettings();
	if (lua_gettop(L))
	{
		rendererSettings.autoHdispLimitArea.pos .X = luaL_checkinteger(L, 1);
		rendererSettings.autoHdispLimitArea.pos .Y = luaL_checkinteger(L, 2);
		rendererSettings.autoHdispLimitArea.size.X = luaL_checkinteger(L, 3);
		rendererSettings.autoHdispLimitArea.size.Y = luaL_checkinteger(L, 4);
		return 0;
	}
	lua_pushinteger(L, rendererSettings.autoHdispLimitArea.pos .X);
	lua_pushinteger(L, rendererSettings.autoHdispLimitArea.pos .Y);
	lua_pushinteger(L, rendererSettings.autoHdispLimitArea.size.X);
	lua_pushinteger(L, rendererSettings.autoHdispLimitArea.size.Y);
	return 4;
}

void LuaRenderer::Open(lua_State *L)
{
	static const luaL_Reg reg[] = {
#define LFUNC(v) { #v, v }
		LFUNC(renderMode),
		LFUNC(displayMode),
		LFUNC(colorMode),
		LFUNC(decorations),
		LFUNC(grid),
		LFUNC(debugHud),
		LFUNC(hud),
		LFUNC(showBrush),
		LFUNC(depth3d),
		LFUNC(zoomEnabled),
		LFUNC(zoomMetrics),
		LFUNC(fireSize),
		LFUNC(useDisplayPreset),
		LFUNC(separateThread),
		LFUNC(heatDisplayLimits),
		LFUNC(heatDisplayAutoArea),
#undef LFUNC
		{ nullptr, nullptr }
	};
	lua_newtable(L);
	luaL_register(L, nullptr, reg);
#define LCONST(v) lua_pushinteger(L, int(v)); lua_setfield(L, -2, #v)
	LCONST(PMODE);
	LCONST(PMODE_NONE);
	LCONST(PMODE_FLAT);
	LCONST(PMODE_BLOB);
	LCONST(PMODE_BLUR);
	LCONST(PMODE_GLOW);
	LCONST(PMODE_SPARK);
	LCONST(PMODE_FLARE);
	LCONST(PMODE_LFLARE);
	LCONST(PMODE_ADD);
	LCONST(PMODE_BLEND);
	LCONST(PSPEC_STICKMAN);
	LCONST(OPTIONS);
	LCONST(NO_DECO);
	LCONST(DECO_FIRE);
	LCONST(FIREMODE);
	LCONST(FIRE_ADD);
	LCONST(FIRE_BLEND);
	LCONST(FIRE_SPARK);
	LCONST(EFFECT);
	LCONST(EFFECT_GRAVIN);
	LCONST(EFFECT_GRAVOUT);
	LCONST(EFFECT_LINES);
	LCONST(EFFECT_DBGLINES);
	LCONST(RENDER_EFFE);
	LCONST(RENDER_FIRE);
	LCONST(RENDER_GLOW);
	LCONST(RENDER_BLUR);
	LCONST(RENDER_BLOB);
	LCONST(RENDER_BASC);
	LCONST(RENDER_NONE);
	LCONST(COLOUR_HEAT);
	LCONST(COLOUR_LIFE);
	LCONST(COLOUR_GRAD);
	LCONST(COLOUR_BASC);
	LCONST(COLOUR_DEFAULT);
	LCONST(DISPLAY_AIRC);
	LCONST(DISPLAY_AIRP);
	LCONST(DISPLAY_AIRV);
	LCONST(DISPLAY_AIRH);
	LCONST(DISPLAY_AIR);
	LCONST(DISPLAY_WARP);
	LCONST(DISPLAY_PERS);
	LCONST(DISPLAY_EFFE);
	LCONST(DISPLAY_AIRW);
#undef LCONST
	lua_pushvalue(L, -1);
	lua_setglobal(L, "renderer");
	lua_setglobal(L, "ren");
}
