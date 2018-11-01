-- * Basically "use strict";.
setmetatable(getfenv(), {__index = function()
	error("__index", 2)
end, __newindex = function()
	error("__newindex", 2)
end})

-- * Replace ./?.lua with ./?.lua;./?/init.lua so require works better.
local init_lua_pattern_extended
package.path = package.path:gsub("(;?)(%./%?%.lua)(;?)", function(cap1, cap2, cap3)
	init_lua_pattern_extended = true
	return cap1 .. cap2 .. ";./?/init.lua" .. cap3
end)
if not init_lua_pattern_extended then
	io.stderr:write("./?.lua not found in package.path\n")
	os.exit(1)
end

local telepathy = require("telepathy")
telepathy.init()

tpt.register_close(telepathy.uninit)

-- * Run other stuff here if you want to (e.g. TPT Multiplayer).
-- * Use telepathy.compat to run scripts from cracker64's script manager
--   or any other similar source. This will make sure that whatever the script
--   does can be safely and completely undone.

-- telepathy.compat("multi.lua")


