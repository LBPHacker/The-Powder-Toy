if host_platform == 'windows'
	if use_bluescreen
		project_deps += [
			cpp_compiler.find_library('dbghelp'),
			cpp_compiler.find_library('psapi'),
		]
	endif
	stacktrace_files = files('Windows.cpp')
elif host_platform == 'darwin'
	# TODO: good impl; current one is only slightly better than nothing
	stacktrace_files = files('Execinfo.cpp')
	# export symbols so backtrace_symbols works, see https://www.gnu.org/software/libc/manual/html_node/Backtraces.html
	bluescreen_export_symbols = true
elif host_platform == 'linux'
	# TODO: again, this is more like "posix" than "linux"
	stacktrace_files = files('Execinfo.cpp')
	# export symbols so backtrace_symbols works, see above
	bluescreen_export_symbols = true
	if host_machine.system() in [ 'freebsd' ]
		project_deps += [
			cpp_compiler.find_library('execinfo'),
		]
	endif
else
	stacktrace_files = files('Null.cpp')
endif
