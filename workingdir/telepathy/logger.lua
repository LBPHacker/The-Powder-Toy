local function logger_raw(...)
	io.stderr:write(tostring(...))
end

local function logger_prefix_lines(prefix, fmt, ...)
	local stuff = tostring(fmt):format(...)
	local sane_prefix = tostring(prefix)
	logger.raw(sane_prefix .. stuff:gsub("\n[^$]", "\n" .. sane_prefix))
end

local function logger_info(fmt, ...)
	return logger.prefix_lines("I: ", fmt, ...)
end

local function logger_warning(fmt, ...)
	return logger.prefix_lines("W: ", fmt, ...)
end

local function logger_error(fmt, ...)
	return logger.prefix_lines("E: ", fmt, ...)
end

local function logger_fatal(fmt, ...)
	return logger.prefix_lines("F: ", fmt, ...)
end

return {
	raw = logger_raw,
	prefix_lines = logger_prefix_lines,
	info = logger_info,
	warning = logger_warning,
	error = logger_error,
	fatal = logger_fatal
}

