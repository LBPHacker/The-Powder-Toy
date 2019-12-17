graphics_dep = declare_dependency(link_with: static_library(
	'graphics',
	sources: files(
		'Graphics.cpp',
		#'OpenGLGraphics.cpp', # this is dead right now
		'RasterGraphics.cpp',
		'../resampler/resampler.cpp',
	),
	dependencies: project_dep,
	include_directories: project_inc,
	c_args: project_c_args,
	cpp_args: project_cpp_args,
	cpp_pch: '../../pch/pch.h',
))
