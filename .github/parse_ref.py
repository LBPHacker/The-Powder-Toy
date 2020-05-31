import os
import re

ref = os.getenv('GITHUB_REF')
match_release = re.match('refs/tags/v(\d+).(\d+).(\d+)', ref)
match_snapshot = re.match('refs/tags/snapshot-(\d+)', ref)

if match_release:
	print('::set-output name=TYPE::release')
	print('::set-output name=MAJOR::' + str(match_release[1]))
	print('::set-output name=MINOR::' + str(match_release[2]))
	print('::set-output name=BUILD::' + str(match_release[3]))
	print('::set-output name=NAME::v' + str(match_release[1]) + '.' + str(match_release[2]) + '.' + str(match_release[3]))
elif match_snapshot:
	print('::set-output name=TYPE::snapshot')
	print('::set-output name=ID::' + str(match_release[1]))
	print('::set-output name=NAME::snapshot-' + str(match_release[1]))
else:
	print('::set-output name=TYPE::master')
	print('::set-output name=NAME::master')
