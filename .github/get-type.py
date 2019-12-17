import re
import sys

ref = sys.argv[1]

md = {} # and some more but who cares
with open('build/src/Config.h') as f:
	for l in f.readlines():
		match_define = re.match(r'\s*#define\s+(\S+)\s+(\S+)\s*', l)
		if match_define:
			md[match_define[1]] = match_define[2]

if re.match(r'refs/tags/v[0-9]+\.[0-9]+', ref):
	print('::set-output name=type::stable')
	print('::set-output name=name::v{0}.{1}.{2}'.format(md['SAVE_VERSION'], md['MINOR_VERSION'], md['BUILD_NUM']))
elif re.match(r'refs/tags/v[0-9]+\.[0-9]+b', ref):
	print('::set-output name=type::beta')
	print('::set-output name=name::v{0}.{1}.{2}b'.format(md['SAVE_VERSION'], md['MINOR_VERSION'], md['BUILD_NUM']))
	beta = 'yes'
elif re.match(r'refs/tags/snapshot-[0-9]+', ref):
	print('::set-output name=type::snapshot')
	print('::set-output name=name::snapshot-{0}'.format(md['SNAPSHOT_ID']))
	snapshot = 'yes'
else:
	print('::set-output name=type::dev')
	print('::set-output name=name::dev')
