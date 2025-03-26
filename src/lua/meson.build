luaconsole_files = files(
#	'LuaButton.cpp', # TODO-REDO_UI
	'LuaBz2.cpp',
#	'LuaCheckbox.cpp', # TODO-REDO_UI
	'LuaCompat.cpp',
#	'LuaComponent.cpp', # TODO-REDO_UI
	'LuaElements.cpp',
	'LuaEvent.cpp',
	'LuaFileSystem.cpp',
	'LuaGraphics.cpp',
	'LuaHttp.cpp',
	'LuaInterface.cpp',
#	'LuaLabel.cpp', # TODO-REDO_UI
	'LuaMisc.cpp',
	'LuaPlatform.cpp',
#	'LuaProgressBar.cpp', # TODO-REDO_UI
	'LuaRenderer.cpp',
	'LuaScriptInterface.cpp',
	'LuaSimulation.cpp',
#	'LuaSlider.cpp', # TODO-REDO_UI
	'LuaSocket.cpp',
	'LuaSmartRef.cpp',
#	'LuaTextbox.cpp', # TODO-REDO_UI
	'LuaTools.cpp',
#	'LuaWindow.cpp', # TODO-REDO_UI
)
if lua_variant != 'luajit'
	luaconsole_files += files(
		'LuaBit.cpp',
	)
endif
if host_platform == 'windows'
	luaconsole_files += files('LuaSocketWindows.cpp')
else
	luaconsole_files += files('LuaSocketDefault.cpp')
endif
if not enable_http
	luaconsole_files += files('LuaSocketTCPNone.cpp')
elif host_platform == 'emscripten'
	luaconsole_files += files('LuaSocketTCPWebsocket.cpp')
else
	luaconsole_files += files('LuaSocketTCPHttp.cpp')
endif
conf_data.set('LUACONSOLE', (lua_variant != 'none').to_string())

subdir('luascripts')

powder_files += luaconsole_files
