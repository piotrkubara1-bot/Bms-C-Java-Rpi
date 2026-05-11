#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck disable=SC1091
source "${PROJECT_DIR}/load_env.sh"

cd "${PROJECT_DIR}"
mkdir -p bin

if [[ "$(uname -s 2>/dev/null || true)" == MINGW* || "$(uname -s 2>/dev/null || true)" == MSYS* || "$(uname -s 2>/dev/null || true)" == CYGWIN* ]]; then
	CP_SEP=";"
else
	CP_SEP=":"
fi

javac -encoding UTF-8 -cp "lib/*" -d bin src/main/java/DataManager.java
java -cp "bin${CP_SEP}lib/*" DataManager
