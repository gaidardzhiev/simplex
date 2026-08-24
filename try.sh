#!/bin/sh
#Copyright (C) 2026 Ivan Gaydardzhiev
#Licensed under the GPL-3.0-only

[ ! -f simplex2elf ] && make; { n() { printf "\n\n\n"; }; run() { ./simplex2elf "$1" -o "${1%.x}.out" && "${1%.x}.out" && strace "${1%.x}.out" && n; }; n && for f in src/*.x stage1/*.x; do [ -f "$f" ] && run "$f"; done && n; }
