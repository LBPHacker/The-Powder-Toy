python = import('python')
python3_prog = python.find_installation('python3')

to_array = generator(
	python3_prog,
	output: [
		'@PLAINNAME@.cpp',
		'@PLAINNAME@.hpp',
	],
	depfile: '@PLAINNAME@.dep',
	arguments: [
		join_paths(meson.current_source_dir(), 'to-array.py'),
		'@OUTPUT0@',
		'@OUTPUT1@',
		'@DEPFILE@',
		'@INPUT@',
		'@EXTRA_ARGS@',
	],
)

powder_src += to_array.process('Font.bz2', extra_args: 'FontBz2')
powder_src += to_array.process('1844659.cps', extra_args: 'Save1844659')
