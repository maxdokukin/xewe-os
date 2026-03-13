#!/usr/bin/env bash
set -euo pipefail

# upload.sh — Flash a single merged image (firmware.bin at 0x0) from a specific build dir.
# All paths are supplied by build.sh.
#
# Required:
#   -c, --chip        c3|c6|s3
#   -p, --port        Serial port
#
# Optional:
#       --build-dir   Absolute path to the build folder (…/builds/<ts>-<ver>-<chip>-<proj>)
#       --baud        Baud (default: 921600)

BUILD_CONFIG_FILE="../build_config"
source "${BUILD_CONFIG_FILE}"

BUILD_DIR="$(get_cfg builds_latest_dir)"

ESP_CHIP=""
ESP_PORT=""
ESP_BAUD="921600"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -c|--chip)      ESP_CHIP="${2:-}"; shift 2 ;;
    -p|--port)      ESP_PORT="${2:-}"; shift 2 ;;
    -b|--baud)      ESP_BAUD="${2:-}"; shift 2 ;;
    --build-dir)    BUILD_DIR="${2:-}"; shift 2 ;;
    *) echo "Unknown arg: $1"; usage ;;
  esac
done

[[ -z "${ESP_CHIP}"  ]] && usage
[[ -z "${ESP_PORT}"  ]] && usage

chip_to_esptool_id() {
  case "$1" in
    c3) echo "esp32c3" ;;
    c6) echo "esp32c6" ;;
    s3) echo "esp32s3" ;;
  esac
}

ESPID="$(chip_to_esptool_id "${ESP_CHIP}")"
ESPTOOL_CMD=$(python_bin) -m esptool;
FIRMWARE_BIN = BUILD_DIR/binary/.bin

echo "📦 Build       : ${BUILD_DIR}"
echo "📄 Image       : ${FIRMWARE_BIN}"
echo "🔌 Port        : ${ESP_PORT}"
echo "⚡ Baud        : ${ESP_BAUD}"
echo "🔧 Chip        : ${ESPID}"
echo "🧰 Esptool     : ${ESPTOOL_CMD}"
echo "🚀 Flashing merged image @ 0x00000000 …"

# shellcheck disable=SC2086
${ESPTOOL_CMD} \
  --chip "${ESPID}" \
  --port "${ESP_PORT}" \
  --baud "${ESP_BAUD}" \
  write_flash 0x0 "${FIRMWARE_BIN}"

echo "✅ Upload complete."
