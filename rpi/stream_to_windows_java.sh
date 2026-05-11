#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PC_USER="${PC_USER:-}"
PC_HOST="${PC_HOST:-}"
PC_PROJECT_DIR="${PC_PROJECT_DIR:-}"
DEVICE="${DEVICE:-/dev/ttyUSB0}"
MODULE_ID="${MODULE_ID:-1}"
PERIOD_MS="${PERIOD_MS:-1000}"
REMOTE_COMMAND="${REMOTE_COMMAND:-}"

if [[ -z "${PC_USER}" || -z "${PC_HOST}" ]]; then
	echo "Set PC_USER and PC_HOST, for example:"
	echo "  PC_USER=piotrek PC_HOST=192.168.1.50 $0"
	exit 1
fi

make -C "${SCRIPT_DIR}" >/dev/null

if [[ -z "${REMOTE_COMMAND}" ]]; then
	if [[ -z "${PC_PROJECT_DIR}" ]]; then
		echo "Set PC_PROJECT_DIR or REMOTE_COMMAND."
		echo "Example for Git Bash as the Windows SSH shell:"
		echo "  PC_PROJECT_DIR=/c/Users/Piotrek/Desktop/Bms-C-Java-Rpi"
		exit 1
	fi
	REMOTE_COMMAND="cd \"${PC_PROJECT_DIR}\" && bash scripts/windows_receive_from_ssh.sh"
fi

"${SCRIPT_DIR}/tinybms_rpi" \
	--device "${DEVICE}" \
	--module "${MODULE_ID}" \
	--period-ms "${PERIOD_MS}" \
	--no-print \
	--bms-line \
	monitor | ssh "${PC_USER}@${PC_HOST}" "${REMOTE_COMMAND}"
