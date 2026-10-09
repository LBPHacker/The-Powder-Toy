powder_src += files(
	'Translation.cpp',
	'StringView.cpp',
	'Language.cpp',
)

subdir('Defs')

foreach language_def : language_defs
	name = language_def[0]
	embedded_files += [
		[ 'LanguageDef_@0@'.format(name), files('Defs/@0@.lang'.format(name)), 'bzip2' ],
	]
endforeach
