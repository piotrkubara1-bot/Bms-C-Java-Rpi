#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

# shellcheck disable=SC1091
source "${PROJECT_DIR}/load_env.sh"

export BMS_API_INGEST_URL="${BMS_API_INGEST_URL:-http://127.0.0.1:8090/api/ingest}"

cd "${PROJECT_DIR}"
mkdir -p bin

echo "[windows_receive_from_ssh] project dir: ${PROJECT_DIR}" >&2
echo "[windows_receive_from_ssh] ingest url: ${BMS_API_INGEST_URL}" >&2

UNAME="$(uname -s 2>/dev/null || true)"
if [[ "${UNAME}" == MINGW* || "${UNAME}" == MSYS* || "${UNAME}" == CYGWIN* ]]; then
	CP_SEP=";"
	JAVAC_CMD="javac"
	JAVA_CMD="java"
elif grep -qi microsoft /proc/version 2>/dev/null; then
	CP_SEP=";"
	JAVAC_CMD="javac.exe"
	JAVA_CMD="java.exe"
else
	CP_SEP=":"
	JAVAC_CMD="javac"
	JAVA_CMD="java"
fi

echo "[windows_receive_from_ssh] java command: ${JAVA_CMD}" >&2

"${JAVAC_CMD}" -encoding UTF-8 -cp "lib/*" -d bin src/main/java/DataManager.java
"${JAVA_CMD}" -cp "bin${CP_SEP}lib/*" DataManager
