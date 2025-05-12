local ffi_ok, ffi = pcall(require, "ffi")
if not ffi_ok then
	return
end

local partsUd, pmapUd, photonsUd = ...
ffi.cdef([[
	struct Particle
	{
		int type;
		int life, ctype;
		float x, y, vx, vy;
		float temp;
		int tmp3;
		int tmp4;
		int flags;
		int tmp;
		int tmp2;
		unsigned int dcolour;
	};
]])
local parts   = ffi.cast("struct Particle *", partsUd  )
local pmap    = ffi.cast("int *"            , pmapUd   )
local photons = ffi.cast("int *"            , photonsUd)

local partPropertyReal = sim.partProperty
local function partPropertyFfi(i, p, v)
	if i < 0 or i >= sim.MAX_PARTS or parts[i].type == 0 then
		return partPropertyReal(i, p, v)
	end
	if not v then
		if     p == "type"    or p == sim.FIELD_TYPE    then return parts[i].type
		elseif p == "temp"    or p == sim.FIELD_TEMP    then return parts[i].temp
		elseif p == "ctype"   or p == sim.FIELD_CTYPE   then return parts[i].ctype
		elseif p == "tmp"     or p == sim.FIELD_TMP     then return parts[i].tmp
		elseif p == "tmp2"    or p == sim.FIELD_TMP2    then return parts[i].tmp2
		elseif p == "life"    or p == sim.FIELD_LIFE    then return parts[i].life
		elseif p == "tmp3"    or p == sim.FIELD_TMP3    then return parts[i].tmp3
		elseif p == "tmp4"    or p == sim.FIELD_TMP4    then return parts[i].tmp4
		elseif p == "x"       or p == sim.FIELD_X       then return parts[i].x
		elseif p == "y"       or p == sim.FIELD_Y       then return parts[i].y
		elseif p == "vx"      or p == sim.FIELD_VX      then return parts[i].vx
		elseif p == "vy"      or p == sim.FIELD_VY      then return parts[i].vy
		elseif p == "flags"   or p == sim.FIELD_FLAGS   then return parts[i].flags
		elseif p == "dcolour" or p == sim.FIELD_DCOLOUR then return parts[i].dcolour
		else
			return partPropertyReal(i, p)
		end
	end
	if     p == "temp"    or p == sim.FIELD_TEMP    then parts[i].temp    = v
	elseif p == "ctype"   or p == sim.FIELD_CTYPE   then parts[i].ctype   = v
	elseif p == "tmp"     or p == sim.FIELD_TMP     then parts[i].tmp     = v
	elseif p == "tmp2"    or p == sim.FIELD_TMP2    then parts[i].tmp2    = v
	elseif p == "life"    or p == sim.FIELD_LIFE    then parts[i].life    = v
	elseif p == "tmp3"    or p == sim.FIELD_TMP3    then parts[i].tmp3    = v
	elseif p == "tmp4"    or p == sim.FIELD_TMP4    then parts[i].tmp4    = v
	elseif p == "vx"      or p == sim.FIELD_VX      then parts[i].vx      = v
	elseif p == "vy"      or p == sim.FIELD_VY      then parts[i].vy      = v
	elseif p == "flags"   or p == sim.FIELD_FLAGS   then parts[i].flags   = v
	elseif p == "dcolour" or p == sim.FIELD_DCOLOUR then parts[i].dcolour = v
	else
		partPropertyReal(i, p, v)
	end
end

local partExistsReal = sim.partExists
local function partExistsFfi(i)
	return not (i < 0 or i >= sim.MAX_PARTS or parts[i].type == 0)
end

local pmapReal = sim.pmap
local function pmapFfi(x, y)
	local l = y * sim.XRES + x
	if x < 0 or x >= sim.XRES or l < 0 or l >= sim.MAX_PARTS then
		return pmapReal(x, y)
	end
	local amalgam = pmap[l]
	if amalgam ~= 0 then
		return bit.rshift(amalgam, sim.PMAPBITS)
	end
end

local photonsReal = sim.photons
local function photonsFfi(x, y)
	local l = y * sim.XRES + x
	if x < 0 or x >= sim.XRES or l < 0 or l >= sim.MAX_PARTS then
		return photonsReal(x, y)
	end
	local amalgam = photons[l]
	if amalgam ~= 0 then
		return bit.rshift(amalgam, sim.PMAPBITS)
	end
end

local partIDReal = sim.partID
local function partIDFfi(x, y)
	local l = y * sim.XRES + x
	if x < 0 or x >= sim.XRES or l < 0 or l >= sim.MAX_PARTS then
		return partIDReal(x, y)
	end
	local amalgam = pmap[l]
	if amalgam == 0 then
		amalgam = photons[l]
	end
	if amalgam ~= 0 then
		return bit.rshift(amalgam, sim.PMAPBITS)
	end
end

local partPropertyCurrent
function sim.partProperty(...)
	return partPropertyCurrent(...)
end

local partExistsCurrent
function sim.partExists(...)
	return partExistsCurrent(...)
end

local pmapCurrent
function sim.pmap(...)
	return pmapCurrent(...)
end

local photonsCurrent
function sim.photons(...)
	return photonsCurrent(...)
end

local partIDCurrent
function sim.partID(...)
	return partIDCurrent(...)
end

function sim.useFfi(use)
	partPropertyCurrent = use and partPropertyFfi or partPropertyReal
	partExistsCurrent   = use and partExistsFfi   or partExistsReal
	pmapCurrent         = use and pmapFfi         or pmapReal
	photonsCurrent      = use and photonsFfi      or photonsReal
	partIDCurrent       = use and partIDFfi       or partIDReal
end
sim.useFfi(true)
