#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="${SCRIPT_DIR}/../.venv"

usage_fail() { echo "❌ $1" >&2; exit 1; }

# Initialize variables to prevent unbound variable errors
ESP_CHIP="" PROJECT_ROOT="" WORK_DIR="" TARGET_DIR=""
PROJECT_NAME="" VERSION_NEXT="" TS_ISO="" LIBS_LIST="" FQBN_EXTRA_OPTS="" CONFIG_JSON_RAW=""

# ---------- Parse args ----------
while [[ $# -gt 0 ]]; do
  case "$1" in
    -t|--type) ESP_CHIP="${2:-}"; shift 2 ;;
    --project-root) PROJECT_ROOT="${2:-}"; shift 2 ;;
    --work-dir) WORK_DIR="${2:-}"; shift 2 ;;
    --target-dir) TARGET_DIR="${2:-}"; shift 2 ;;
    --project-name) PROJECT_NAME="${2:-}"; shift 2 ;;
    --version) VERSION_NEXT="${2:-}"; shift 2 ;;
    --timestamp) TS_ISO="${2:-}"; shift 2 ;;
    --libs) LIBS_LIST="${2:-}"; shift 2 ;;
    --fqbn-extra) FQBN_EXTRA_OPTS="${2:-}"; shift 2 ;;
    --config_json) CONFIG_JSON_RAW="${2:-}"; shift 2 ;;
    -h|--help) exit 0 ;;
    *) usage_fail "Unknown arg: $1" ;;
  esac
done

[[ -z "$ESP_CHIP" || -z "$PROJECT_ROOT" || -z "$WORK_DIR" || -z "$TARGET_DIR" || -z "$PROJECT_NAME" || -z "$VERSION_NEXT" || -z "$TS_ISO" ]] && usage_fail "Missing required arguments."

BUILD_START="$(date -u +%s)"

# ---------- Tooling checks ----------
command -v arduino-cli >/dev/null || usage_fail "'arduino-cli' missing in PATH"
arduino-cli core list | grep -q 'esp32:esp32' || usage_fail "Espressif core missing. Run: arduino-cli core install esp32:esp32"

find_python() {
  for p in "${VENV_DIR}/bin/python3" "${VENV_DIR}/bin/python" "python3" "python"; do
    command -v "$p" >/dev/null 2>&1 && { echo "$p"; return 0; }
  done
  return 1
}

# ---------- config_json check ----------
CONFIG_JSON_EMBED="null"
if [[ -n "$CONFIG_JSON_RAW" ]]; then
  PY="$(find_python)" || usage_fail "Python needed for --config_json validation"
  CONFIG_JSON_EMBED="$("$PY" -c 'import json,sys; print(json.dumps(json.loads(sys.argv[1]), separators=(",",":"), sort_keys=True))' "$CONFIG_JSON_RAW")" || usage_fail "Invalid --config_json"
fi

# ---------- Chip mapping ----------
case "$ESP_CHIP" in
  c6) FQBN_BOARD="esp32c6"; CHIP_FAMILY="ESP32-C6" ;;
  s3) FQBN_BOARD="esp32s3"; CHIP_FAMILY="ESP32-S3" ;;
  *)  FQBN_BOARD="esp32c3"; CHIP_FAMILY="ESP32-C3" ;;
esac

FQBN_BASE="esp32:esp32:${FQBN_BOARD}"
FQBN="${FQBN_BASE}:CDCOnBoot=cdc,CPUFreq=160,DebugLevel=none,EraseFlash=all,FlashMode=qio,FlashSize=4M,JTAGAdapter=default,PartitionScheme=no_ota,UploadSpeed=921600${FQBN_EXTRA_OPTS:+,${FQBN_EXTRA_OPTS}}"

# ---------- Detect sketch ----------
SKETCH_PATH=$(find "$PROJECT_ROOT" "$PROJECT_ROOT/src" -maxdepth 1 -name '*.ino' 2>/dev/null | head -n 1)
[[ -z "$SKETCH_PATH" ]] && usage_fail "No .ino found in project root or src/"
SKETCH_NAME="$(basename "$SKETCH_PATH" .ino)"

# ---------- Directories & Libs ----------
OUTPUT_DIR="${TARGET_DIR}/output"
BINARY_DIR="${TARGET_DIR}/binary"
mkdir -p "$WORK_DIR" "$OUTPUT_DIR" "$BINARY_DIR" "${TARGET_DIR}/src"

LIB_FLAGS=()
if [[ -n "$LIBS_LIST" ]]; then
  IFS=':' read -r -a libs <<< "$LIBS_LIST"
  for lp in "${libs[@]}"; do
    [[ -d "$lp" ]] && LIB_FLAGS+=( --libraries "$lp" )
  done
fi

# ---------- Compile ----------
echo "🔧 Compiling $SKETCH_NAME for $CHIP_FAMILY..."
# Note: set -euo pipefail ensures tee will exit the script with the proper code if arduino-cli fails
arduino-cli compile --fqbn "$FQBN" --build-path "$WORK_DIR" --warnings default ${LIB_FLAGS[@]+"${LIB_FLAGS[@]}"} "$SKETCH_PATH" | tee "${TARGET_DIR}/compile.log"

# ---------- Merge ----------
MERGED_BIN="${WORK_DIR}/${SKETCH_NAME}.ino.merged.bin"
if [[ ! -f "$MERGED_BIN" ]]; then
  PY="$(find_python)" || usage_fail "Python/esptool needed to merge binaries"
  APP_BIN=$(find "$WORK_DIR" -maxdepth 1 -name "${SKETCH_NAME}.ino.bin" | head -n 1)
  BOOT_BIN=$(find "$WORK_DIR" -name 'bootloader*.bin' | head -n 1)
  PART_BIN=$(find "$WORK_DIR" -name '*partitions*.bin' | head -n 1)
  [[ -f "$APP_BIN" && -f "$BOOT_BIN" && -f "$PART_BIN" ]] || usage_fail "Missing merge components"

  echo "🔗 Merging binaries..."
  "$PY" -m esptool merge_bin -o "$MERGED_BIN" 0x0 "$BOOT_BIN" 0x8000 "$PART_BIN" 0x10000 "$APP_BIN"
fi

# ---------- Snapshot & Outputs ----------
[[ -d "${PROJECT_ROOT}/src" ]] && cp -a "${PROJECT_ROOT}/src/"* "${TARGET_DIR}/src/" || cp -a "$SKETCH_PATH" "${TARGET_DIR}/src/"
if [[ -n "$LIBS_LIST" ]]; then
  mkdir -p "${TARGET_DIR}/lib"
  for lp in "${libs[@]}"; do [[ -d "$lp" ]] && cp -a "$lp" "${TARGET_DIR}/lib/"; done
fi

find "$WORK_DIR" -maxdepth 1 -type f \( -name "*.bin" -o -name "*.elf" -o -name "*.map" \) -exec cp -a {} "$OUTPUT_DIR/" \;

BIN_FILE="${VERSION_NEXT}-${CHIP_FAMILY}-${PROJECT_NAME}.bin"
cp -a "$MERGED_BIN" "${BINARY_DIR}/${BIN_FILE}"

# ---------- Manifest ----------
cat > "${BINARY_DIR}/manifest.json" <<EOF
{
  "name": "${PROJECT_NAME}",
  "version": "${VERSION_NEXT}",
  "new_install_improv_wait_time": 0,
  "builds": [ { "chipFamily": "${CHIP_FAMILY}", "parts": [ { "path": "${BIN_FILE}", "offset": 0 } ] } ]
}
EOF

# ---------- Meta ----------
BUILD_TIME=$(( $(date -u +%s) - BUILD_START ))
# Micro helper for JSON escaping
esc() { local s="${1//\\/\\\\}"; s="${s//\"/\\\"}"; echo "${s//$'\n'/\\n}"; }
REL="${PROJECT_NAME}/${TARGET_DIR#*${PROJECT_NAME}/}/binary"

cat > "${BINARY_DIR}/meta.json" <<EOF
{
  "type": "$(esc "$ESP_CHIP")",
  "chip_family": "$(esc "$CHIP_FAMILY")",
  "project_name": "$(esc "$PROJECT_NAME")",
  "version": "$(esc "$VERSION_NEXT")",
  "timestamp_param": "$(esc "$TS_ISO")",
  "config": $CONFIG_JSON_EMBED,
  "fqbn": "$(esc "$FQBN")",
  "fqbn_base": "$(esc "$FQBN_BASE")",
  "fqbn_extra": "$(esc "$FQBN_EXTRA_OPTS")",
  "build_time_sec": $BUILD_TIME,
  "artifacts": {
    "binary_filename": "$(esc "$BIN_FILE")",
    "path_rel_binary": "$(esc "$REL/$BIN_FILE")",
    "path_rel_manifest_json": "$(esc "$REL/manifest.json")",
    "path_rel_meta_json": "$(esc "$REL/meta.json")",
    "path_abs_binary": "$(esc "${BINARY_DIR}/$BIN_FILE")",
    "path_abs_manifest_json": "$(esc "${BINARY_DIR}/manifest.json")",
    "path_abs_meta_json": "$(esc "${BINARY_DIR}/meta.json")"
  }
}
EOF

echo "✅ Build complete in ${BUILD_TIME}s: ${BINARY_DIR}/${BIN_FILE}"