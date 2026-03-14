#!/usr/bin/env bash
set -euo pipefail

# release.sh - Reads release_matrix.csv and runs build.sh for each row.
#
# Usage:
#   ./release.sh
#   ./release.sh -f custom_matrix.csv

BUILD_CONFIG_FILE="../build_config"
source "${BUILD_CONFIG_FILE}"

LATEST_DIR="$(get_cfg builds_latest_dir)"
STATIC_DIR="$(get_cfg project_root)/static/firmware/releases"
MATRIX_FILE="$(get_cfg release_matrix_file)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -f|--file) MATRIX_FILE="$2"; shift 2 ;;
    *) echo "❌ Unknown arg: $1"; exit 1 ;;
  esac
done

[[ ! -f "$MATRIX_FILE" ]] && { echo "❌ Matrix file not found: $MATRIX_FILE"; exit 1; }

# Ensure destination directory exists
mkdir -p "$STATIC_DIR"

echo "🚀 Starting shell-based matrix build from ${MATRIX_FILE}"

# 1. Read the header row into an array
IFS=',' read -r -a headers < "$MATRIX_FILE"

# Find the index of the CHIP column
chip_idx=-1
for i in "${!headers[@]}"; do
  # Strip carriage returns just in case (Windows CRLF)
  headers[$i]="${headers[$i]//$'\r'/}"
  if [[ "${headers[$i]}" == "CHIP" ]]; then
    chip_idx=$i
  fi
done

if [[ $chip_idx -eq -1 ]]; then
  echo "❌ Error: 'CHIP' column not found in headers."
  exit 1
fi

row_num=1

# 2. Process the data rows (tail skips the header)
tail -n +2 "$MATRIX_FILE" | while IFS=',' read -r -a row_data || [[ -n "${row_data[*]}" ]]; do
  ((row_num++))

  # Skip completely empty lines
  [[ ${#row_data[@]} -eq 0 ]] && continue
  [[ -z "${row_data[0]//$'\r'/}" ]] && continue

  # Extract CHIP value
  chip_val="${row_data[$chip_idx]//$'\r'/}"

  # 3. Build the JSON string for the remaining columns
  json_payload="{"
  first=1

  for i in "${!headers[@]}"; do
    if [[ $i -ne $chip_idx ]]; then
      key="${headers[$i]}"
      val="${row_data[$i]//$'\r'/}"

      [[ $first -eq 0 ]] && json_payload+=","

      # Basic JSON typing: leave pure numbers and booleans unquoted, quote the rest
      if [[ "$val" =~ ^-?[0-9]+$ ]] || [[ "$val" == "true" ]] || [[ "$val" == "false" ]]; then
        json_payload+="\"${key}\":${val}"
      else
        # Escape any internal double quotes
        val="${val//\"/\\\"}"
        json_payload+="\"${key}\":\"${val}\""
      fi

      first=0
    fi
  done
  json_payload+="}"

  echo -e "\n======================================================="
  echo "📦 Row $row_num | CHIP: $chip_val | Config: $json_payload"
  echo "======================================================="

  # 4. Execute build.sh
  ./build.sh -c "$chip_val" --config_json "$json_payload" --build_notes ""

  # 5. Move artifacts to the static directory
  dest_dir="${STATIC_DIR}/row_${row_num}_${chip_val}"
  mkdir -p "$dest_dir"

  echo "🚚 Moving artifacts from ${LATEST_DIR}/ to ${dest_dir}/"
  # Moves the contents out of the latest directory
  mv "${LATEST_DIR}/"* "$dest_dir/" || { echo "⚠️  Warning: Move failed or dir was empty."; exit 1; }

done

echo -e "\n✅ All matrix rows processed and moved!"