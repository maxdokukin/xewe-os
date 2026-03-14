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
STATE_FILE="$(get_cfg build_state_file)"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -f|--file) MATRIX_FILE="$2"; shift 2 ;;
    *) echo "❌ Unknown arg: $1"; exit 1 ;;
  esac
done

[[ ! -f "$MATRIX_FILE" ]] && { echo "❌ Matrix file not found: $MATRIX_FILE"; exit 1; }

# Helper to fetch the version identically to build.sh
get_version() {
  if [[ -f "$STATE_FILE" ]]; then
    # Run in a subshell so we don't pollute the current environment
    (source "$STATE_FILE" && echo "${MAJOR:-0}.${MINOR:-0}.${PATCH:-0}")
  else
    echo "0.0.0"
  fi
}

CURRENT_VERSION="$(get_version)"
VERSION_DIR="${STATIC_DIR}/${CURRENT_VERSION}"
MAP_FILE="${VERSION_DIR}/firmware_map.csv"

mkdir -p "$VERSION_DIR"
echo "🚀 Starting matrix build from ${MATRIX_FILE} (Version: ${CURRENT_VERSION})"

# 1. Read the header row into an array
IFS=',' read -r -a headers < "$MATRIX_FILE"

# Find the indices of CHIP and _BUILD_NOTES columns (case-insensitive) and clean headers
chip_idx=-1
notes_idx=-1
map_header=""

for i in "${!headers[@]}"; do
  # Strip carriage returns safely
  raw_header="${headers[$i]:-}"
  clean_header="${raw_header//$'\r'/}"
  headers[$i]="$clean_header"

  if [[ "$clean_header" =~ ^[Cc][Hh][Ii][Pp]$ ]]; then
    chip_idx=$i
    map_header+="${clean_header},"
  elif [[ "$clean_header" =~ ^_[Bb][Uu][Ii][Ll][Dd]_[Nn][Oo][Tt][Ee][Ss]$ ]]; then
    notes_idx=$i
    # Intentionally do NOT append to map_header so it is excluded from the CSV
  else
    map_header+="${clean_header},"
  fi
done

if [[ $chip_idx -eq -1 ]]; then
  echo "❌ Error: 'CHIP' column not found in headers."
  exit 1
fi

echo "🗺️  Initialized firmware map at ${MAP_FILE}"
[[ $notes_idx -ne -1 ]] && echo "📝 Detected _BUILD_NOTES column at index $notes_idx"

# Write only the headers (minus _BUILD_NOTES) to the map file, stripping the trailing comma
echo "${map_header%,}" > "$MAP_FILE"

row_num=1

# 2. Process the data rows (safely handling empty rows with :- fallbacks)
tail -n +2 "$MATRIX_FILE" | while IFS=',' read -r -a row_data || [[ -n "${row_data[*]:-}" ]]; do
  ((row_num++))

  # Safely skip completely empty lines or rows missing data
  [[ ${#row_data[@]} -eq 0 ]] && continue

  first_col="${row_data[0]:-}"
  [[ -z "${first_col//$'\r'/}" ]] && continue

  # Safely extract CHIP value
  chip_raw="${row_data[$chip_idx]:-}"
  chip_val="${chip_raw//$'\r'/}"

  # Safely extract _BUILD_NOTES value if the column exists
  build_notes_val=""
  if [[ $notes_idx -ne -1 ]]; then
    notes_raw="${row_data[$notes_idx]:-}"
    build_notes_val="${notes_raw//$'\r'/}"
  fi

  # 3. Build the JSON string and the nested directory path simultaneously
  json_payload="{"
  first=1
  nested_path=""

  for i in "${!headers[@]}"; do
    key="${headers[$i]}"
    # Protect against empty trailing columns or unbound indexes
    val="${row_data[$i]:-}"
    val="${val//$'\r'/}"

    # Skip build notes in path generation and JSON payload
    if [[ $i -eq $notes_idx ]]; then
      continue
    fi

    # Build the folder path structure (col_1/col_2/...)
    # Strip quotes, backslashes, and replace spaces with underscores for safe folder names
    dir_name="${val//\"/}"
    dir_name="${dir_name//\\/}"
    dir_name="${dir_name// /_}"
    [[ -z "$dir_name" ]] && dir_name="empty"

    nested_path="${nested_path}/${dir_name}"

    # Build the JSON Payload (skipping the CHIP column)
    if [[ $i -ne $chip_idx ]]; then
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

  # Assemble the final target directory
  dest_dir="${VERSION_DIR}${nested_path}"

  echo -e "\n======================================================="
  echo "📦 Row $row_num | CHIP: $chip_val"
  echo "📂 Path: ${dest_dir}"
  echo "⚙️  Config: $json_payload"
  [[ -n "$build_notes_val" ]] && echo "📝 Notes: $build_notes_val"
  echo "======================================================="

  # Pass the dynamically extracted build notes to the build.sh script
  ./build.sh -c "$chip_val" --config_json "$json_payload" --build_notes "$build_notes_val"

  mkdir -p "$dest_dir"

  echo "🚚 Moving artifacts to ${dest_dir}/"
  if ! mv "${LATEST_DIR}/binary/"* "$dest_dir/" 2>/dev/null; then
     echo "❌ Error: Move failed or latest/binary/ dir was empty."
     exit 1
  fi

  # Copy the build notes file into the release destination if there were build notes provided
  [[ -n "$build_notes_val" ]] && cp "${LATEST_DIR}/build_notes.txt" "$dest_dir/" 2>/dev/null || true

done

echo -e "\n✅ All matrix rows processed! Header map saved to ${MAP_FILE}"