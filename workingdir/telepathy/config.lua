local MODULES_DIR = "telepathy/modules"
local function modules_dir()
	return MODULES_DIR
end

local REQUIRE_PATH = MODULES_DIR:gsub("[\\/]", ".")
local function require_path()
	return REQUIRE_PATH
end

local PROFILE_DIR = "telepathy/profile"
local function profile_dir()
	return PROFILE_DIR
end

local function conf_require(module)
	return require(require_path() .. "." .. tostring(module))
end

return {
	modules_dir = modules_dir,
	require_path = require_path,
	profile_dir = profile_dir,
	require = conf_require
}
