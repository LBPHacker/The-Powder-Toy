local telepathy = require("telepathy")

local function dependencies()
	return {}
end

local function mouse(mouse_x, mouse_y, button, event, wheel)
	if button == 1 then
		if event == 1 or event == 3 then
			sim.partCreate(-2, mouse_x, mouse_y, elem.DEFAULT_PT_DMND)
		end
	end
end

local function init()
	print("brush.init")
	tpt.register_mouseclick(mouse)
end

local function uninit()
	tpt.unregister_mouseclick(mouse)
	print("brush.uninit")
end

return {
	dependencies = dependencies,
	init = init,
	uninit = uninit
}

