#!/bin/sh
# this script runs specific options against valgrind for debugging
# get valgrind at http://valgrind.kde.org/

valgrind \
-v \
--leak-check=yes \
--show-reachable=yes \
./shac $@ 2>&1 | less

