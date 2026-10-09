#!/bin/sh
#Copyright (C) 2026 Ivan Gaydardzhiev
#Licensed under the GPL-3.0-only

G='\033[0;32m'
R='\033[0;31m'
N='\033[0m'
X='simplex2elf'

[ ! -f "${X}" ] && make

fprint() {
	printf "[%s] Test: %-25s\nExpected: %s\nCaptured: %s %b\n\n" "$(date '+%Y-%m-%d %H:%M:%S')" "${1}" "${2}" "${3}" "${4}"
}

fth() {
	./"${X}" src/hello.x -o src/hello.out
	captured=$(src/hello.out)
	expected="Hello from the Simplex World!"
	[ "${captured}" = "${expected}" ] && {
		fprint "Print Hello World" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Print Hello World" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 2
	}
}

fta() {
	./"${X}" src/alloc.x -o src/alloc.out
	captured=$(src/alloc.out)
	expected="6295588
6295604
16
111
222
333"
	[ "${captured}" = "${expected}" ] && {
		fprint "Alloc" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Alloc" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 3
	}
}

ftr() {
	./"${X}" src/arr.x -o src/arr.out
	captured=$(src/arr.out)
	expected="10
20
30
40"
	[ "${captured}" = "${expected}" ] && {
		fprint "Array" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Array" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 4
	}
}

ftb() {
	./"${X}" src/bytes.x -o src/bytes.out
	captured=$(src/bytes.out)
	expected="104
101
108
108
111
10"
	[ "${captured}" = "${expected}" ] && {
		fprint "Bytes" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Bytes" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 5
	}
}

ftf() {
	./"${X}" src/fact.x -o src/fact.out
	captured=$(src/fact.out)
	expected="3628800"
	[ "${captured}" = "${expected}" ] && {
		fprint "Fact" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Fact" "${expected}" "${captured}" "\n${R}FAILED${N}"
		return 6
	}
}

ftl() {
	./"${X}" src/loop.x -o src/loop.out
	captured=$(src/loop.out)
	expected="loop fact(10) = 3628800"
	[ "${captured}" = "${expected}" ] && {
		fprint "Loop" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Loop" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 7
	}
}

ftn() {
	./"${X}" src/nodepool.x -o src/nodepool.out
	captured=$(src/nodepool.out)
	expected="15
18"
	[ "${captured}" = "${expected}" ] && {
		fprint "Nodepool" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Nodepool" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 8
	}
}

ftp() {
	./"${X}" src/ptr.x -o src/ptr.out
	captured=$(src/ptr.out)
	expected="8
16"
	[ "${captured}" = "${expected}" ] && {
		fprint "Ptr" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Ptr" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 9
	}
}

ftv() {
	./"${X}" src/vars.x -o src/vars.out
	captured=$(src/vars.out)
	expected="Hello from Simplex!
528"
	[ "${captured}" = "${expected}" ] && {
		fprint "Vars" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Vars" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 10
	}
}

ftx() {
	./"${X}" stage1/lex.x -o stage1/lex.out
	captured=$(stage1/lex.out)
	expected="calling lx_one
5242880
5242884
3 0
2 0
22 0
0 42
30 0
3 0
2 0
22 0
2 0
8 0
0 1
30 0
done"
	[ "${captured}" = "${expected}" ] && {
		fprint "Stage1 Lex" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Stage1 Lex" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 11
	}
}

fty() {
	./"${X}" stage1/parse.x -o stage1/parse.out
	captured=$(stage1/parse.out)
	expected="!!! AST !!!
PROG
  FN fact
    BLOCK
      IF
        BIN
          ID n
          NUM 0
        BLOCK
          RETURN
            NUM 1
      RETURN
        BIN
          ID n
          CALL fact
            BIN
              ID n
              NUM 1
  CALL putint
    CALL fact
      NUM 10
  CALL putstr
    STR"
	[ "${captured}" = "${expected}" ] && {
		fprint "Stage1 Parse" "${expected}" "${captured}" "\n${G}PASSED${N}\n"
		return 0
	} || {
		fprint "Stage1 Parse" "${expected}" "${captured}" "\n${R}FAILED${N}\n"
		return 12
	}
}

{ fth && fta && ftr && ftb && ftf && ftl && ftn && ftp && ftv && ftx && fty; ret="${?}"; } || exit 1

[ "${ret}" -eq 0 ] 2>/dev/null || printf "%s\n" "${ret}"
