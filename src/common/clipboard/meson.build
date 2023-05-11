clipboard_impl_factories = []
if host_platform == 'windows'
	powder_files += files('Windows.cpp')
	clipboard_impl_factories += [
		[ 'SDL_SYSWM_WINDOWS', 'WindowsClipboardFactory' ],
	]
elif host_platform == 'android'
	# TODO
elif host_platform == 'emscripten'
	# TODO
else
	powder_files += files([
		'External.cpp',
	])
	clipboard_impl_factories += [
		[ 'SDL_SYSWM_X11', 'ExternalClipboardFactory' ],
		[ 'SDL_SYSWM_WAYLAND', 'ExternalClipboardFactory' ],
	]
endif
powder_files += files('Dynamic.cpp')
render_files += files('Null.cpp')
font_files += files('Null.cpp')
