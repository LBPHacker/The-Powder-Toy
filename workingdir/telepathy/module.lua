local params = require("telepathy.params")
local logger = require("telepathy.logger")

local loaded_modules = {}

local function module_status(name)
	name = tostring(name)
	local module_info = loaded_modules[name]
	if not module_info then
		return "unloaded"
	end

	return module_info.status, unpack(module_info.error_message)
end

local function module_release_resources(name)
	-- * TODO
end

local function module_unload(name)
	name = tostring(name)
	local module_info = loaded_modules[name]
	if not module_info then
		return nil, "not currently loaded"
	end

	-- * Unload dependents. 
	for dependent in pairs(module_info.depends_on_this) do
		module_unload(dependent)
	end

	-- * Uninitialise module.
	xpcall(function()
		module_info.exports.uninit()
	end, function(err)
		module_info.status = "failed"
		module_info.error_message = { ".uninit failed", err }
		logger.error("failed to uninitialise module '%s': %s\n", name, err)
		logger.info("%s\n", debug.traceback())
	end)

	-- * Remove cross-references from dependencies.
	for dependency in pairs(module_info.this_depends_on) do
		loaded_modules[dependency].depends_on_this[name] = nil
	end
	
	module_release_resources(name)
	loaded_modules[name] = nil

	return true
end

local function module_load(name)
	name = tostring(name)
	if loaded_modules[name] then
		return nil, "already loaded"
	end

	-- * Add module info to module table regardless of whether it fails to load,
	--   initialise, get dependencies in order or whatever.
	local module_info = {
		status = "unknown",
		error_message = {},
		exports = false,
		this_depends_on = {},
		depends_on_this = {}
	}
	loaded_modules[name] = module_info

	-- * Load module chunk.
	local ok
	ok, module_info.exports = xpcall(function()
		return params.require(name)
	end, function(err)
		module_info.status = "failed"
		module_info.error_message = { "failed to load", err }
		logger.error("failed to load module '%s' for initialisation: %s\n", name, err)
		logger.info("%s\n", debug.traceback())
	end)
	if not ok then
		return nil, unpack(module_info.error_message)
	end

	-- * Query dependencies.
	local ok = xpcall(function()
		local dependencies = module_info.exports.dependencies()
		if type(dependencies) ~= "table" then
			error(".dependencies returned non-table")
		end
		for ix_dependency, dependency in pairs(dependencies) do
			module_info.this_depends_on[tostring(dependency)] = true
		end
	end, function(err)
		module_info.status = "failed"
		module_info.error_message = { ".dependencies failed", err }
		logger.error("failed to query dependencies of module '%s': %s\n", name, err)
		logger.info("%s\n", debug.traceback())
	end)

	-- * Load dependencies. As the policy is that a module is present in the
	--   module table until unloaded regardless of whether it is initialised,
	--   it's okay to reference the module table entries of dead modules too
	--   to list the module currently being loaded as a dependency.
	for dependency in pairs(module_info.this_depends_on) do
		module_load(dependency)
		local dependency_status = module_status(dependency)

		-- * Give up loading the current module if a dependency is not
		--   in the initialised state despite having been requested to be loaded.
		if dependency_status ~= "initialised" then
			module_info.status = "failed"
			module_info.error_message = { "unmet dependency", dependency }
			logger.error("failed to initialise dependency '%s' of module '%s'\n", dependency, name)
		end

		-- * Don't touch modules that are not in a well-defined state.
		if dependency_status ~= "unknown" then
			-- * Cross-reference the module being loaded as a dependent of the dependency.
			loaded_modules[dependency].depends_on_this[name] = true
		end
	end
	if module_info.failed then
		return nil, unpack(module_info.error_message)
	end

	-- * Initialise module.
	local ok = xpcall(function()
		module_info.exports.init()
	end, function(err)
		module_info.status = "failed"
		module_info.error_message = { ".init failed", err }
		logger.error("failed to initialise module '%s': %s\n", name, err)
		logger.info("%s\n", debug.traceback())
	end)
	if not ok then
		module_release_resources(name)
		return nil, unpack(module_info.error_message)
	end
	module_info.status = "initialised"

	return true
end

local function module_dependencies(name)
	name = tostring(name)
	local module_info = loaded_modules[name]
	if not module_info then
		return nil, "not currently loaded"
	end
	
	local dependencies_array = {}
	for dependency in pairs(module_info.this_depends_on) do
		table.insert(dependencies_array, dependency)
	end
	return dependencies_array
end

local function module_dependents(name)
	name = tostring(name)
	local module_info = loaded_modules[name]
	if not module_info then
		return nil, "not currently loaded"
	end
	
	local dependents_array = {}
	for dependent in pairs(module_info.depends_on_this) do
		table.insert(dependents_array, dependent)
	end
	return dependents_array
end

local function module_list()
	local modules_array = {}
	for module_name in pairs(loaded_modules) do
		table.insert(modules_array, module_name)
	end
	return modules_array
end

return {
	load = module_load,
	unload = module_unload,
	list = module_list,
	status = module_status,
	dependencies = module_dependencies,
	dependents = module_dependents
}

