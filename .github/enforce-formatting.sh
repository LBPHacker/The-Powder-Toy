#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'

git checkout $GITHUB_BASE_REF
git checkout $GITHUB_REF
git reset $GITHUB_BASE_REF
if ! [[ "$(./apply-format | wc -l)" -eq 0 ]]; then
	echo '##################################################################'
	echo '# ./apply-format output:                                         #'
	echo '##################################################################'
	echo
	./apply-format $GITHUB_BASE_REF
	echo
	echo '##################################################################'
	echo '# is it possible that you have not formatted your code properly? #'
	echo '##################################################################'
	exit 1
fi
git reset $GITHUB_REF
