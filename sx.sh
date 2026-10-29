#!/bin/sh
#Copyright (C) 2026 Ivan Gaydardzhiev
#Licensed under the GPL-3.0-only
set -eu

[ "${#}" -eq 3 ] || { printf 'usage: %s compiler src.x out\n' "${0}" >&2; exit 1; }

t="${3}.txt"
{ cat "${2}"; printf '\000'; } | "${1}" > "${t}" || { cat "${t}" >&2; rm -f "${t}"; exit 1; }
printf "$(cat "${t}")" > "${3}"
chmod 755 "${3}"
rm -f "${t}"
