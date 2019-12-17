import re
import sys

ref = sys.argv[1]

mesondefines = {} # and some more but who cares
with open('build/src/Config.h') as f:
	for l in f.readlines():
		match_define = re.match('\s*#define\s+(\S+)\s+(\S+)\s*', l)
		if match_define:
			mesondefines[match_define[1]] = match_define[2]

if re.match(r'refs/tags/[0-9]+\.[0-9]+', ref):
	print('::set-output name=TYPE::stable')
	print('::set-output name=NAME::v%s.%s.%s' % (mesondefines['SAVE_VERSION'], mesondefines['MINOR_VERSION'], mesondefines['BUILD_NUM']))
elif re.match(r'refs/tags/snapshot-[0-9]+', ref):
	print('::set-output name=TYPE::snapshot')
	print('::set-output name=NAME::snapshot-%s' % mesondefines['SNAPSHOT_ID'])
else:
	print('::set-output name=TYPE::dev')
	print('::set-output name=NAME::dev')
