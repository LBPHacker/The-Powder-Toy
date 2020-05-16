project('tpt-mac-utils', [ 'c', 'objc' ])

subdir('src')

macutils_inc = include_directories('include')
macutils_dep = declare_dependency(
	link_with: static_library(
		'tpt-mac-utils',
		sources: macutils_files,
		include_directories: macutils_inc,
	),
	include_directories: macutils_inc,
	dependencies: dependency('Foundation'),
)
