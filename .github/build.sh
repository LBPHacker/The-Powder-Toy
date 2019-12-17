#!/bin/bash

set -euo pipefail
IFS=$'\n\t'

if [ -z "${PLATFORM_SHORT-}" ]; then
	>&2 echo "PLATFORM_SHORT not set" 
	exit 1
fi

if [ -z "${build_sh_init-}" ]; then
	if [ $PLATFORM_SHORT == "win" ]; then
		for i in C:/Program\ Files\ \(x86\)/Microsoft\ Visual\ Studio/**/**/VC/Auxiliary/Build/vcvarsall.bat; do
			vcvarsall_path=$i
		done
		cat << BUILD_INIT_BAT > .github/build_init.bat
@echo off
call "${vcvarsall_path}" x64
bash -c 'build_sh_init=1 ./.github/build.sh'
BUILD_INIT_BAT
		./.github/build_init.bat
	else
		build_sh_init=1 ./.github/build.sh
	fi
	exit 0
fi

other_flags=
bin_postfix=64
if [ $PLATFORM_SHORT == "win" ]; then
	other_flags=-Db_vscrt=mt
	bin_postfix=$bin_postfix.exe
fi
meson -Dbuildtype=release -Db_pie=false -Db_staticpic=false -Db_lto=true -Dstatic=prebuilt -Dignore_updates=false -Dinstall_check=true $other_flags build
cd build
ninja
7z a ../powder.zip powder$bin_postfix render$bin_postfix font$bin_postfix
cd ..
7z a powder.zip README.md LICENSE
