local telepathy = require("telepathy")

local function dependencies()
	return {}
end

local button_1_down = false
local function mouse_button_down(x, y, button)
	if button == 1 then
		button_1_down = true
	end
end
local function mouse_button_up(x, y, button)
	if button == 1 then
		button_1_down = false
	end
end
local function mouse_move(x, y, dx, dy)
	if button_1_down then
		sim.partCreate(-2, x, y, elem.DEFAULT_PT_DMND)
	end
end

local function init()
	print("brush.init")
	evt.register(evt.mousedown, mouse_button_down)
	evt.register(evt.mouseup, mouse_button_up)
	evt.register(evt.mousemove, mouse_move)
end

local function uninit()
	evt.unregister(evt.mousedown, mouse_button_down)
	evt.unregister(evt.mouseup, mouse_button_up)
	evt.unregister(evt.mousemove, mouse_move)
	print("brush.uninit")
end

return {
	dependencies = dependencies,
	init = init,
	uninit = uninit
}

