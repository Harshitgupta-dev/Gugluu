#!/usr/bin/env sh
set -eu

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root_dir"

if command -v python3 >/dev/null 2>&1; then
	python3 -u "scripts/setup_dependencies.py" "$@"
elif command -v python >/dev/null 2>&1; then
	python -u "scripts/setup_dependencies.py" "$@"
else
	printf 'Python 3 is required to prepare dependencies and generate the project.\n' >&2
	exit 1
fi
