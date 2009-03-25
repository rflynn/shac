#!/bin/bash
# this script is used to run shac against the splint tool for debugging
# get splint at http://splint.org/

splint \
+posixlib \
-predboolint \
-nullret \
-compdef \
-temptrans \
-branchstate \
-nullpass \
-mustfreefresh \
+matchanyintegral \
+charint \
-nullderef \
-nullstate \
-formattype \
-mustfreeonly \
-nullassign \
-mayaliasunique \
-unrecog \
shac.c 2>&1 | less

#+ignoresigns \
