#########################################################################
# File Name: autobuild.sh
# Author: BamdowCoco
# mail: 1341382546@qq.com
# Created Time: Wed 15 Apr 2026 04:52:28 PM CST
#########################################################################
#!/bin/bash

set -x
rm -rf `pwd`/build/*
cd `pwd`/build &&
	cmake .. &&
	make
