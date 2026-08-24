#!/bin/sh
#Copyright (C) 2026 Ivan Gaydardzhiev
#Licensed under the GPL-3.0-only

for obj in *.out; do
	printf "#### %s ####\n" "${obj}"
	readelf -a "${obj}"
	objdump -d "${obj}"
	hexdump "${obj}"
	printf "\n\n"
done
