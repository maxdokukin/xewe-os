#!/usr/bin/env bash
set -euo pipefail
# compile.sh — Build a versioned ESP32 firmware using Arduino CLI.
# All paths/options are supplied by build.sh.
#
# Required flags:
# -t, --type c3|c6|s3
# --project-root <abs path>
# --builds-dir <abs path>
# --work-dir <abs path> (Arduino --build-path)
# --target-dir <abs path> (final build folder)
# --project-name <name>
# --version <X.Y.ZZZ>
# --timestamp <ISO-8601 UTC>
# --libs <path[:path...]> (each becomes --libraries)
# --fqbn-extra <comma-separated FQBN opts>
#
# Optional flags:
# --config_json <JSON string> (additional params written into meta.json)
#
# Output layout (under --target-dir):
# output/ binary/{firmware.bin, <ver>-<chip>-<project>.bin, manifest.json, meta.json}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# ---------- Parse args ----------
ESP_CHIP=""
PROJECT_ROOT=""
BUILDS_DIR=""
WORK_DIR=""
TARGET_DIR=""
PROJECT_NAME=""
VERSION_NEXT=""
TS_ISO=""
LIBS_LIST=""
FQBN_EXTRA_OPTS=""
CONFIG_JSON_RAW=""
VENV_DIR="${SCRIPT_DIR}/../.venv"
usage_fail() { echo "❌ $1"; exit 1; }
while [[ $# -gt 0 ]]; do
case "$1" in
-t|--type) ESP_CHIP="${2:-}"; shift 2 ;;
--project-root) PROJECT_ROOT="${2:-}"; shift 2 ;;
--builds-dir) BUILDS_DIR="${2:-}"; shift 2 ;;
--work-dir) WORK_DIR="${2:-}"; shift 2 ;;
--target-dir) TARGET_DIR="${2:-}"; shift 2 ;;
--project-name) PROJECT_NAME="${2:-}"; shift 2 ;;
--version) VERSION_NEXT="${2:-}"; shift 2 ;;
--timestamp) TS_ISO="${2:-}"; shift 2 ;;
--libs) LIBS_LIST="${2:-}"; shift 2 ;;
--fqbn-extra) FQBN_EXTRA_OPTS="${2:-}"; shift 2 ;;
--config_json) CONFIG_JSON_RAW="${2:-}"; shift 2 ;;
-h|--help) sed -n '1,140p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
*) usage_fail "Unknown arg: $1" ;;
esac
done
[[ -z "${ESP_CHIP}" ]] && usage_fail "Missing --type"
[[ -z "${PROJECT_ROOT}" ]] && usage_fail "Missing --project-root"
[[ -z "${BUILDS_DIR}" ]] && usage_fail "Missing --builds-dir"
[[ -z "${WORK_DIR}" ]] && usage_fail "Missing --work-dir"
[[ -z "${TARGET_DIR}" ]] && usage_fail "Missing --target-dir"
[[ -z "${PROJECT_NAME}" ]] && usage_fail "Missing --project-name"
[[ -z "${VERSION_NEXT}" ]] && usage_fail "Missing --version"
[[ -z "${TS_ISO}" ]] && usage_fail "Missing --timestamp"
# ---------- Build timing ----------
BUILD_START_EPOCH="$(date -u +%s)"
# ---------- Tooling checks ----------
if ! command -v arduino-cli >/dev/null 2>&1; then
usage_fail "'arduino-cli' not found in PATH"
fi
if ! arduino-cli core list | grep -q 'esp32:esp32'; then
usage_fail "Espressif core not installed. Run: arduino-cli core install esp32:esp32"
fi
# ---------- NEW: validate/normalize --config_json ----------
pick_python_for_json() {
if [[ -n "${VENV_DIR}" && -x "${VENV_DIR}/bin/python3" ]]; then
echo "${VENV_DIR}/bin/python3"; return 0
fi
if [[ -n "${VENV_DIR}" && -x "${VENV_DIR}/bin/python" ]]; then
echo "${VENV_DIR}/bin/python"; return 0
fi
if command -v python3 >/dev/null 2>&1; then
echo "python3"; return 0
fi
if command -v python >/dev/null 2>&1; then
echo "python"; return 0
fi
return 1
}
CONFIG_JSON_NORM=""
CONFIG_JSON_EMBED="null"
if [[ -n "${CONFIG_JSON_RAW}" ]]; then
PY_FOR_JSON=""
if ! PY_FOR_JSON="$(pick_python_for_json)"; then
usage_fail "--config_json provided but no python found to validate it (python3/python)."
fi
set +e
CONFIG_JSON_NORM="$("${PY_FOR_JSON}" -c 'import json,sys
s=sys.argv[1]
try:
obj=json.loads(s)
except Exception as e:
print(str(e), file=sys.stderr)
sys.exit(2)
print(json.dumps(obj, separators=(",",":"), sort_keys=True))
' "${CONFIG_JSON_RAW}")"
RC=$?
set -e
if [[ $RC -ne 0 ]]; then
usage_fail "Invalid --config_json (must be valid JSON): ${CONFIG_JSON_RAW}"
fi
CONFIG_JSON_EMBED="${CONFIG_JSON_NORM}"
fi
# ---------- Chip mapping ----------
chip_to_fqbn_board() {
case "$1" in
c3) echo "esp32c3" ;;
c6) echo "esp32c6" ;;
s3) echo "esp32s3" ;;
*) echo "esp32c3" ;;
esac
}
chip_to_family_str() {
case "$1" in
c3) echo "ESP32-C3" ;;
c6) echo "ESP32-C6" ;;
s3) echo "ESP32-S3" ;;
esac
}
FQBN_BOARD="$(chip_to_fqbn_board "${ESP_CHIP}")"
CHIP_FAMILY="$(chip_to_family_str "${ESP_CHIP}")"
FQBN_BASE="esp32:esp32:${FQBN_BOARD}"
FQBN_OPTS_DEFAULT="\
CDCOnBoot=cdc,\
CPUFreq=160,\
DebugLevel=none,\
EraseFlash=all,\
FlashMode=qio,\
FlashSize=4M,\
JTAGAdapter=default,\
PartitionScheme=no_ota,\
UploadSpeed=921600\
"
FQBN_OPTS="${FQBN_OPTS_DEFAULT}${FQBN_EXTRA_OPTS:+,${FQBN_EXTRA_OPTS}}"
FQBN="${FQBN_BASE}:${FQBN_OPTS}"
# ---------- Detect sketch ----------
detect_sketch() {
local candidate
if [[ -f "${PROJECT_ROOT}/${PROJECT_NAME}.ino" ]]; then
echo "${PROJECT_ROOT}/${PROJECT_NAME}.ino"; return
fi
candidate="$(find "${PROJECT_ROOT}" -maxdepth 1 -name '*.ino' | head -n 1 || true)"
[[ -n "${candidate}" ]] && { echo "${candidate}"; return; }
candidate="$(find "${PROJECT_ROOT}/src" -maxdepth 1 -name '*.ino' 2>/dev/null | head -n 1 || true)"
[[ -n "${candidate}" ]] && { echo "${candidate}"; return; }
echo ""
}
SKETCH_PATH="$(detect_sketch)"
[[ -n "${SKETCH_PATH}" ]] || usage_fail "Could not locate .ino in project root or src/"
SKETCH_NAME="$(basename "${SKETCH_PATH}" .ino)"
# ---------- Prep directories ----------
OUTPUT_DIR="${TARGET_DIR}/output"
BINARY_DIR="${TARGET_DIR}/binary"
mkdir -p "${WORK_DIR}" "${TARGET_DIR}" "${OUTPUT_DIR}" "${BINARY_DIR}"
echo "🔧 Arduino FQBN: ${FQBN}"
echo "📄 Sketch: ${SKETCH_PATH}"
echo "🧰 Work path: ${WORK_DIR}"
echo "📁 Target dir: ${TARGET_DIR}"
# ---------- Build arguments ----------
COMPILE_ARGS=( compile --fqbn "${FQBN}" --build-path "${WORK_DIR}" --warnings default )
# Libraries (split colon-separated)
declare -a LIB_FLAGS=()
declare -a USED_LIBS=()
if [[ -n "${LIBS_LIST}" ]]; then
IFS=':' read -r -a _libarr <<< "${LIBS_LIST}"
for lp in "${_libarr[@]}"; do
if [[ -d "${lp}" ]]; then
LIB_FLAGS+=( --libraries "${lp}" )
USED_LIBS+=( "${lp}" )
echo "📚 Using libs: ${lp}"
else
echo "⚠️ Skipping non-existent libs dir: ${lp}"
fi
done
fi
# Capture compile command (for meta.json)
declare -a _COMPILE_CMD_ARR=( arduino-cli "${COMPILE_ARGS[@]}" )
if ((${#LIB_FLAGS[@]})); then
_COMPILE_CMD_ARR+=( "${LIB_FLAGS[@]}" )
fi
_COMPILE_CMD_ARR+=( "${SKETCH_PATH}" )
shell_join() {
local out=""
local a
for a in "$@"; do
out+="$(printf '%q ' "$a")"
done
echo -n "${out% }"
}
COMPILE_CMD_STR="$(shell_join "${_COMPILE_CMD_ARR[@]}")"
# ---------- Compile ----------
set +e
if ((${#LIB_FLAGS[@]})); then
arduino-cli "${COMPILE_ARGS[@]}" "${LIB_FLAGS[@]}" "${SKETCH_PATH}" | tee "${TARGET_DIR}/compile.log"
else
arduino-cli "${COMPILE_ARGS[@]}" "${SKETCH_PATH}" | tee "${TARGET_DIR}/compile.log"
fi
BUILD_RC="${PIPESTATUS[0]}"
set -e
[[ "${BUILD_RC}" -eq 0 ]] || { echo "❌ Compile failed (see ${TARGET_DIR}/compile.log)"; exit "${BUILD_RC}"; }
# ---------- Find or create merged binary ----------
ESPTOOL_CMD=""
MERGE_METHOD="found"
MERGED_BIN="$(find "${WORK_DIR}" -maxdepth 1 -name "${SKETCH_NAME}.ino.merged.bin" -print -quit || true)"
if [[ -z "${MERGED_BIN}" ]]; then
echo "ℹ️ No *.merged.bin found; attempting merge via esptool…"
MERGE_METHOD="merged"
pick_esptool() {
if [[ -n "${VENV_DIR}" && -x "${VENV_DIR}/bin/python3" ]]; then
echo "${VENV_DIR}/bin/python3 -m esptool"; return 0
fi
if [[ -n "${VENV_DIR}" && -x "${VENV_DIR}/bin/python" ]]; then
echo "${VENV_DIR}/bin/python -m esptool"; return 0
fi
if command -v esptool.py >/dev/null 2>&1; then
echo "esptool.py"; return 0
fi
if command -v python3 >/dev/null 2>&1; then
echo "python3 -m esptool"; return 0
fi
if command -v python >/dev/null 2>&1; then
echo "python -m esptool"; return 0
fi
return 1
}
if ! ESPTOOL_CMD=$(pick_esptool); then
echo "❌ esptool not found; cannot merge binaries. Install via brew/pip or provide --venv."
exit 1
fi
APP_BIN="$(find "${WORK_DIR}" -maxdepth 1 -name "${SKETCH_NAME}.ino.bin" -print -quit || true)"
BOOT_BIN="$(find "${WORK_DIR}" -type f -name 'bootloader*.bin' -print -quit || true)"
PART_BIN="$(find "${WORK_DIR}" -type f -name '*partitions*.bin' -print -quit || true)"
[[ -f "${APP_BIN}" && -f "${BOOT_BIN}" && -f "${PART_BIN}" ]] || { echo "❌ Missing components to merge"; exit 1; }
MERGED_BIN="${WORK_DIR}/${SKETCH_NAME}.ino.merged.bin"
echo "🔗 Merging → ${MERGED_BIN}"
# shellcheck disable=SC2086
${ESPTOOL_CMD} merge_bin -o "${MERGED_BIN}" \
0x0 "${BOOT_BIN}" \
0x8000 "${PART_BIN}" \
0x10000 "${APP_BIN}"
echo "✅ Created merged image."
else
echo "✅ Found merged image: ${MERGED_BIN}"
fi
# ---------- Snapshot sources & libs ----------
if [[ -d "${PROJECT_ROOT}/src" ]]; then
cp -a "${PROJECT_ROOT}/src" "${TARGET_DIR}/src"
else
mkdir -p "${TARGET_DIR}/src"
cp -a "${SKETCH_PATH}" "${TARGET_DIR}/src/"
fi
if [[ -n "${LIBS_LIST}" ]]; then
mkdir -p "${TARGET_DIR}/lib"
IFS=':' read -r -a _libarr2 <<< "${LIBS_LIST}"
for lp in "${_libarr2[@]}"; do
[[ -d "${lp}" ]] && cp -a "${lp}" "${TARGET_DIR}/lib/$(basename "${lp}")"
done
fi
# ---------- Collect outputs ----------
find "${WORK_DIR}" -maxdepth 1 -type f \( -name "*.bin" -o -name "*.elf" -o -name "*.map" \) -exec cp -a {} "${OUTPUT_DIR}/" \;
# Place final merged image (Versioned + generic alias)
MERGED_BIN_FILENAME="${VERSION_NEXT}-${CHIP_FAMILY}-${PROJECT_NAME}.bin"
cp -a "${MERGED_BIN}" "${BINARY_DIR}/${MERGED_BIN_FILENAME}"
# ---------- Manifest (ESP Web Tools v10) ----------
cat > "${BINARY_DIR}/manifest.json" <<EOF
{
"name": "${PROJECT_NAME}",
"version": "${VERSION_NEXT}",
"new_install_improv_wait_time": 0,
"builds": [
{
"chipFamily": "${CHIP_FAMILY}",
"parts": [
{ "path": "${MERGED_BIN_FILENAME}", "offset": 0 }
]
}
]
}
EOF
echo "📝 Wrote manifest → ${BINARY_DIR}/manifest.json"
# ---------- Build timing end ----------
BUILD_END_EPOCH="$(date -u +%s)"
BUILD_TIME_SEC=$(( BUILD_END_EPOCH - BUILD_START_EPOCH ))
MINS=$(( BUILD_TIME_SEC / 60 ))
SECS=$(( BUILD_TIME_SEC % 60 ))
# --- meta.json (build parameters/context) ---
json_escape() {
local s="${1-}"
s="${s//\\/\\\\}"
s="${s//\"/\\\"}"
s="${s//$'\n'/\\n}"
s="${s//$'\r'/\\r}"
s="${s//$'\t'/\\t}"
echo -n "$s"
}
json_array() {
local first=1
local s
echo -n "["
for s in "$@"; do
if [[ $first -eq 0 ]]; then echo -n ", "; fi
first=0
echo -n "\"$(json_escape "$s")\""
done
echo -n "]"
}
REL_ROOT_BINARY="${PROJECT_NAME}/${TARGET_DIR#*${PROJECT_NAME}/}/binary"
REL_BINARY_PATH="${REL_ROOT_BINARY}/${MERGED_BIN_FILENAME}"
REL_MANIFEST_PATH="${REL_ROOT_BINARY}/manifest.json"
REL_META_PATH="${REL_ROOT_BINARY}/meta.json"
META_PATH="${BINARY_DIR}/meta.json"
{
echo "{"
echo " \"type\": \"$(json_escape "${ESP_CHIP}")\","
echo " \"chip_family\": \"$(json_escape "${CHIP_FAMILY}")\","
echo " \"project_name\": \"$(json_escape "${PROJECT_NAME}")\","
echo " \"version\": \"$(json_escape "${VERSION_NEXT}")\","
echo " \"timestamp_param\": \"$(json_escape "${TS_ISO}")\","
echo " \"config\": ${CONFIG_JSON_EMBED},"
echo " \"fqbn\": \"$(json_escape "${FQBN}")\","
echo " \"fqbn_base\": \"$(json_escape "${FQBN_BASE}")\","
echo " \"fqbn_extra\": \"$(json_escape "${FQBN_EXTRA_OPTS}")\","
echo " \"build_time_sec\": ${BUILD_TIME_SEC},"
echo " \"artifacts\": {"
echo " \"binary_filename\": \"$(json_escape "${MERGED_BIN_FILENAME}")\","
echo " \"path_rel_binary\": \"$(json_escape "${REL_BINARY_PATH}")\","
echo " \"path_rel_manifest_json\": \"$(json_escape "${REL_MANIFEST_PATH}")\","
echo " \"path_rel_meta_json\": \"$(json_escape "${REL_META_PATH}")\","
echo " \"path_abs_binary\": \"$(json_escape "${BINARY_DIR}/${MERGED_BIN_FILENAME}")\","
echo " \"path_abs_manifest_json\": \"$(json_escape "${BINARY_DIR}/manifest.json")\","
echo " \"path_abs_meta_json\": \"$(json_escape "${BINARY_DIR}/meta.json")\""
echo " }"
echo "}"
} > "${META_PATH}"
echo "🎉 Build complete."
echo "⏱️ Total build time: ${MINS}m ${SECS}s"
echo " ➤ Final dir : ${TARGET_DIR}"
echo " ➤ Firmware : ${BINARY_DIR}/firmware.bin"
echo " ➤ Version : ${VERSION_NEXT}"
