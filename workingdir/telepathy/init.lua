local config = require("telepathy.config")
local logger = require("telepathy.logger")
local module = require("telepathy.module")

local function compat(path)
	-- * TODO
end

local function init()
	-- * TODO

	local MODULES_DIR = config.modules_dir()
	if not fs.isDirectory(MODULES_DIR) then
		fs.makeDirectory(MODULES_DIR)
	end
	local files = fs.list(MODULES_DIR)
	if files then
		for ix_name, name in ipairs(files) do
			module.load(name)
		end
	else
		logger.error("failed to list modules in '%s'\n", MODULES_DIR)
	end
end

local function uninit()
	for ix_module, module_name in ipairs(module.list()) do
		module.unload(module_name)
	end
end

return {
	init = init,
	uninit = uninit,
	require = config.require,
	compat = compat
}

