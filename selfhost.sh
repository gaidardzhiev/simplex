#!/bin/sh
#Copyright (C) 2026 Ivan Gaydardzhiev
#Licensed under the GPL-3.0-only
set -eu

[ -f simplex2elf ] || make simplex2elf

sed '/^int nd_type_name/,$d' stage1/parse.x > simplex.x
cat stage1/codegen.x stage1/main.x >> simplex.x

./simplex2elf simplex.x -o stage1.out 2>/dev/null
./sx.sh ./stage1.out simplex.x stage2.out
cmp stage1.out stage2.out
./sx.sh ./stage2.out simplex.x stage3.out
cmp stage2.out stage3.out
printf 'self-hosting fixpoint: stage1 == stage2 == stage3\n'
sha256sum stage1.out stage2.out stage3.out

fail=0
for f in src/*.x stage1/lex.x stage1/parse.x tests/*.x; do
	[ -f "${f}" ] || continue
	./simplex2elf "${f}" -o ref.out 2>/dev/null
	./sx.sh ./stage2.out "${f}" x.out
	cmp -s ref.out x.out && printf 'IDENTICAL  %s\n' "${f}" || { printf 'DIFFERENT  %s\n' "${f}"; fail=1; }
done
rm -f ref.out x.out
exit "${fail}"
