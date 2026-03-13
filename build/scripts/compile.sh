#!/usr/bin/env bash
set -euo pipefail

# compile.sh - Build a versioned ESP32 firmware using Arduino CLI.
# All paths/options are supplied by build.sh.
#
# Required flags:
# --chip c3|c6|s3
# --version <X.Y.ZZZ>
# --timestamp <ISO-8601 UTC>
# Optional flags:
# --fqbn-extra <comma-separated FQBN opts>
# --config_json <JSON string> (additional params written into meta.json)
#
# Output layout (under --target-dir):
# output/ binary/{firmware.bin, <ver>-<chip>-<project>.bin, manifest.json, meta.json}

#-------------------------#
#--- GET THE VARIABLES ---#
#-------------------------#
# ---------- Build timing ----------
BUILD_START_EPOCH="$(date -u +%s)"

# load config
CONFIG_FILE="/Users/max/Codebase/github/xewe-os/build/build_config"
source "${CONFIG_FILE}"

# Load config values
PROJECT_ROOT="$(get_cfg project_root)"
BUILDS_DIR="$(get_cfg builds_dir)"
WORK_DIR="$(get_cfg builds_cache_dir)"
PROJECT_NAME="$(get_cfg project_name)"
LIBS_DIR="$(get_cfg libraries_dir)"
PYTHON_BIN="$(get_cfg python_bin)"

ESP_CHIP=""
VERSION=""
TS_ISO=""
FQBN_EXTRA_OPTS=""
CONFIG_JSON_RAW=""

usage_fail() { echo "❌ $1" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --chip) ESP_CHIP="${2:?missing value}"; shift 2 ;;
    --version) VERSION="${2:?missing value}"; shift 2 ;;
    --timestamp) TS_ISO="${2:?missing value}"; shift 2 ;;
    --fqbn-extra) FQBN_EXTRA_OPTS="${2:?missing value}"; shift 2 ;;
    --config_json) CONFIG_JSON_RAW="${2:?missing value}"; shift 2 ;;
    *) usage_fail "Unknown arg: $1" ;;
  esac
done

[[ -z "${ESP_CHIP}" ]] && usage_fail "Missing --chip"
[[ -z "${VERSION}" ]] && usage_fail "Missing --version"
[[ -z "${TS_ISO}" ]] && usage_fail "Missing --timestamp"

case "${ESP_CHIP}" in
  c3|c6|s3) ;;
  *) usage_fail "Invalid --chip: ${ESP_CHIP} (expected c3, c6, or s3)" ;;
esac

# ---------- validate/normalize --config_json ----------

if [[ -n "${CONFIG_JSON_RAW//[[:space:]]/}" ]]; then
  if ! CONFIG_JSON_VALIDATED="$("${PYTHON_BIN}" -c 'import json,sys
s=sys.argv[1]
try:
    obj=json.loads(s)
except Exception as e:
    print(str(e), file=sys.stderr)
    sys.exit(2)
print(json.dumps(obj, separators=(",",":"), sort_keys=True))
' "${CONFIG_JSON_RAW}")"; then
    usage_fail "Invalid --config_json (must be valid JSON): ${CONFIG_JSON_RAW}"
  fi
else
  CONFIG_JSON_VALIDATED="\"\""
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
SKETCH_PATH="$(get_cfg project_ino_file)"

# ---------- Prep directories ----------
TARGET_DIR="${BUILDS_DIR}/${TS_ISO}-${VERSION}-${ESP_CHIP}-${PROJECT_NAME}"
mkdir -p "${TARGET_DIR}"
OUTPUT_DIR="${TARGET_DIR}/output"
BINARY_DIR="${TARGET_DIR}/binary"

mkdir -p "${OUTPUT_DIR}" "${BINARY_DIR}"

echo "🔧 Arduino FQBN: ${FQBN}"
echo "📄 Sketch: ${SKETCH_PATH}"
echo "📚 Using libs: ${LIBS_DIR}"
echo "📁 Target dir: ${TARGET_DIR}"
echo "🧰 Work path: ${WORK_DIR}"

COMPILE_ARGS=(
  compile
  --fqbn "${FQBN}"
  --build-path "${WORK_DIR}"
  --warnings default
  --libraries "${LIBS_DIR}"
  "${SKETCH_PATH}"
)


#--------------------------#
#--- /GET THE VARIABLES ---#
#--------------------------#
#-------------------#
#--- PRE COMPILE ---#
#-------------------#

# ---------- Snapshot sources & libs ----------
cp -a "${PROJECT_ROOT}/src" "${TARGET_DIR}/src"
cp -a "${LIBS_DIR}" "${TARGET_DIR}/libs"


#--------------------#
#--- /PRE COMPILE ---#
#--------------------#
#--------------#
#--- COMPILE---#
#--------------#
BUILD_RC=0
if arduino-cli "${COMPILE_ARGS[@]}"; then
  BUILD_RC=0
else
  BUILD_RC=$?
fi

[[ "${BUILD_RC}" -eq 0 ]] || {
  echo "❌ Compile failed (see ${TARGET_DIR}/compile.log)"
  exit "${BUILD_RC}"
}

#---------------#
#--- /COMPILE---#
#---------------#
#-------------------------#
#--- PROCESS ARTIFACTS ---#
#-------------------------#
# ---------- Find or create merged binary ----------
MERGED_BIN="$(find "${WORK_DIR}" -maxdepth 1 -name "${PROJECT_NAME}.ino.merged.bin" -print -quit || true)"

# Place final merged image (versioned + generic alias)
MERGED_BIN_FILENAME="${VERSION}-${ESP_CHIP}-${PROJECT_NAME}.bin"
cp -a "${MERGED_BIN}" "${BINARY_DIR}/${MERGED_BIN_FILENAME}"

# ---------- Manifest (ESP Web Tools v10) ----------
cat > "${BINARY_DIR}/manifest.json" <<EOF
{
  "name": "${PROJECT_NAME}",
  "version": "${VERSION}",
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

echo "📝 Wrote manifest -> ${BINARY_DIR}/manifest.json"

# --- meta.json (build parameters/context) ---
# ---------- Build timing end ----------
BUILD_END_EPOCH="$(date -u +%s)"
BUILD_TIME_SEC=$(( BUILD_END_EPOCH - BUILD_START_EPOCH ))
MINS=$(( BUILD_TIME_SEC / 60 ))
SECS=$(( BUILD_TIME_SEC % 60 ))

json_escape() {
  local s="${1-}"
  s="${s//\\/\\\\}"
  s="${s//\"/\\\"}"
  s="${s//$'\n'/\\n}"
  s="${s//$'\r'/\\r}"
  s="${s//$'\t'/\\t}"
  echo -n "$s"
}

TARGET_DIR_REL="${PROJECT_NAME}${TARGET_DIR#*${PROJECT_NAME}}"
REL_BINARY_PATH="${TARGET_DIR_REL}/binary/${MERGED_BIN_FILENAME}"
REL_MANIFEST_PATH="${TARGET_DIR_REL}/binary/manifest.json"
REL_META_PATH="${TARGET_DIR_REL}/binary/meta.json"
META_PATH="${BINARY_DIR}/meta.json"

{
  echo "{"
  echo "  \"type\": \"$(json_escape "${ESP_CHIP}")\","
  echo "  \"chip_family\": \"$(json_escape "${CHIP_FAMILY}")\","
  echo "  \"project_name\": \"$(json_escape "${PROJECT_NAME}")\","
  echo "  \"version\": \"$(json_escape "${VERSION}")\","
  echo "  \"timestamp_param\": \"$(json_escape "${TS_ISO}")\","
  echo "  \"config\": ${CONFIG_JSON_VALIDATED},"
  echo "  \"fqbn\": \"$(json_escape "${FQBN}")\","
  echo "  \"fqbn_extra\": \"$(json_escape "${FQBN_EXTRA_OPTS}")\","
  echo "  \"build_time_sec\": ${BUILD_TIME_SEC},"
  echo "  \"artifacts\": {"
  echo "    \"binary_filename\": \"$(json_escape "${MERGED_BIN_FILENAME}")\","
  echo "    \"path_rel_binary\": \"$(json_escape "${REL_BINARY_PATH}")\","
  echo "    \"path_rel_manifest_json\": \"$(json_escape "${REL_MANIFEST_PATH}")\","
  echo "    \"path_rel_meta_json\": \"$(json_escape "${REL_META_PATH}")\","
  echo "    \"path_abs_binary\": \"$(json_escape "${BINARY_DIR}/${MERGED_BIN_FILENAME}")\","
  echo "    \"path_abs_manifest_json\": \"$(json_escape "${BINARY_DIR}/manifest.json")\","
  echo "    \"path_abs_meta_json\": \"$(json_escape "${BINARY_DIR}/meta.json")\""
  echo "  }"
  echo "}"
} > "${META_PATH}"

echo "🎉 Build complete."
echo "⏱️ Total build time: ${MINS}m ${SECS}s"
echo " ➤ Final dir : ${TARGET_DIR}"
echo " ➤ Firmware  : ${BINARY_DIR}/firmware.bin"
echo " ➤ Version   : ${VERSION}"
#--------------------------#
#--- /PROCESS ARTIFACTS ---#
#--------------------------#