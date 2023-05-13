#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 15
sudo apt update
sudo apt install clang-format-15

git checkout $GITHUB_BASE_REF
git checkout $GITHUB_HEAD_REF
git reset $GITHUB_BASE_REF
if ! [[ "$(./apply-format | wc -l)" -eq 0 ]]; then
	echo '##################################################################'
	echo '# ./apply-format output:                                         #'
	echo '##################################################################'
	echo
	./apply-format
	echo
	echo '##################################################################'
	echo '# is it possible that you have not formatted your code properly? #'
	echo '##################################################################'
	exit 1
fi
git reset $GITHUB_HEAD_REF
