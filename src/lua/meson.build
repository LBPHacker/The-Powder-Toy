luaconsole_files = files(
	'LuaButton.cpp',
	'LuaBz2.cpp',
	'LuaCheckbox.cpp',
	'LuaCompat.cpp',
	'LuaComponent.cpp',
	'LuaElements.cpp',
	'LuaEvent.cpp',
	'LuaFileSystem.cpp',
	'LuaGraphics.cpp',
	'LuaHttp.cpp',
	'LuaInterface.cpp',
	'LuaLabel.cpp',
	'LuaMisc.cpp',
	'LuaPlatform.cpp',
	'LuaProgressBar.cpp',
	'LuaRenderer.cpp',
	'LuaScriptInterface.cpp',
	'LuaSimulation.cpp',
	'LuaSlider.cpp',
	'LuaSocket.cpp',
	'LuaSmartRef.cpp',
	'LuaTextbox.cpp',
	'LuaTools.cpp',
	'LuaWindow.cpp',
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
	luaconsole_files += files('LuaSocketTcpNone.cpp')
elif host_platform == 'emscripten'
	luaconsole_files += files('LuaSocketTcpWebsocket.cpp')
else
	luaconsole_files += files('LuaSocketTcpHttp.cpp')
	alpn_backend = get_option('alpn_backend')
	if alpn_backend == 'auto'
		if static_variant == 'prebuilt' and host_platform != 'emscripten'
			alpn_backend = 'mbedtls'
		else
			alpn_backend = 'none'
		endif
	endif
	if alpn_backend == 'mbedtls'
		luaconsole_files += files('LuaSocketAlpnMbedtls.cpp')
	else
		luaconsole_files += files('LuaSocketAlpnNone.cpp')
	endif
endif
conf_data.set('LUACONSOLE', (lua_variant != 'none').to_string())

subdir('luascripts')

powder_files += luaconsole_files
