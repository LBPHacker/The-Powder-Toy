-- * Basically "use strict";.
setmetatable(getfenv(), {__index = function()
	error("__index", 2)
end, __newindex = function()
	error("__newindex", 2)
end})

-- * Replace ./?.lua with ./?.lua;./?/init.lua so require works better.
do
	local stuff_in_package_path = {}
	for stuff in package.path:gsub("[^;]+") do
		table.insert(stuff_in_package_path, stuff)
	end
	local init_lua_pattern_extended
	for ix_stuff, stuff in ipairs() do
		if stuff == "./?.lua" then
			table.insert(stuff_in_package_path, ix_stuff, "./?/init.lua")
			init_lua_pattern_extended = true
			package.path = table.concat(stuff_in_package_path, ";")
			break
		end
	end
	if not init_lua_pattern_extended then
		io.stderr:write("./?.lua not found in package.path\n")
		os.exit(1)
	end
end

local telepathy = require("telepathy")
telepathy.init()

evt.register(evt.close, telepathy.uninit)

-- * Run other stuff here if you want to (e.g. TPT Multiplayer).
-- * Use telepathy.compat to run scripts from cracker64's script manager
--   or any other similar source. This will make sure that whatever the script
--   does can be safely and completely undone.

-- telepathy.compat("multi.lua")

