#!/usr/bin/env bash
set -euo pipefail

# release.sh — Orchestrates a multi-chip release.
#
# Workflow:
#  1. Sets .version_state to RELEASE version.
#  2. Asks user for Release Notes via VI.
#  3. Compiles artifacts.
#  4. Adds notes to the binary folder.
#  5. Copies ONLY the 'binary' folder to 'static/firmware/releases/<ver>/<build_name>'.
#  6. Pushes Binaries to 'binaries' branch.
#  7. Increments .version_state to NEXT version.
#  8. Commits the new .version_state to 'binaries' branch.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# ---------- Config ----------
BUILD_ROOT="${PROJECT_ROOT}/build"
BUILDS_DIR="${BUILD_ROOT}/builds"
WORK_DIR="${BUILDS_DIR}/cache"
STATE_FILE="${BUILD_ROOT}/builds/.version_state"
DEFAULT_VENV="${SCRIPT_DIR}/.venv"

# Static Directory Config
STATIC_RELEASES_ROOT="${PROJECT_ROOT}/static/firmware/releases"

REMOTE="origin"
BRANCH="binaries"

# ---------- 1. Input Validation ----------
TARGET_CHIPS=("$@")
[[ ${#TARGET_CHIPS[@]} -eq 0 ]] && { echo "❌ Usage: $0 <chip1> [chip2] ..."; exit 1; }

for chip in "${TARGET_CHIPS[@]}"; do
  [[ "$chip" =~ ^(c3|c6|s3)$ ]] || { echo "❌ Invalid chip: $chip"; exit 1; }
done

# ---------- 2. Read & Validate Version ----------
read_kv() { grep -E "^$1=" "$2" | cut -d'=' -f2- || true; }
write_kv() {
  local file="$1" k="$2" v="$3"
  if grep -qE "^${k}=" "${file}"; then
    sed -i.bak -E "s|^${k}=.*|${k}=${v}|" "${file}" && rm -f "${file}.bak"
  else
    echo "${k}=${v}" >> "${file}"
  fi
}

[[ ! -f "${STATE_FILE}" ]] && { echo "❌ Missing ${STATE_FILE}"; exit 1; }

CUR_MAJOR="$(read_kv MAJOR "${STATE_FILE}")"
CUR_MINOR="$(read_kv MINOR "${STATE_FILE}")"
CUR_PATCH="$(read_kv PATCH "${STATE_FILE}")"
PROJECT_NAME="$(read_kv PROJECT "${STATE_FILE}")"
[[ -z "${PROJECT_NAME}" ]] && PROJECT_NAME="$(basename "${PROJECT_ROOT}")"

echo "ℹ️  Current State: ${CUR_MAJOR}.${CUR_MINOR}.${CUR_PATCH}"
echo -n "🎯 Enter Release Version (X.Y.Z): "
read -r INPUT_VER

[[ "${INPUT_VER}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "❌ Invalid format"; exit 1; }
IFS='.' read -r IN_MAJOR IN_MINOR IN_PATCH <<< "${INPUT_VER}"

# Semantic Check
VER_OK=0
if [[ $((10#$IN_MAJOR)) -gt $((10#$CUR_MAJOR)) ]]; then VER_OK=1;
elif [[ $((10#$IN_MAJOR)) -eq $((10#$CUR_MAJOR)) ]]; then
  if [[ $((10#$IN_MINOR)) -gt $((10#$CUR_MINOR)) ]]; then VER_OK=1;
  elif [[ $((10#$IN_MINOR)) -eq $((10#$CUR_MINOR)) ]]; then
    if [[ $((10#$IN_PATCH)) -ge $((10#$CUR_PATCH)) ]]; then VER_OK=1; fi
  fi
fi
[[ $VER_OK -eq 0 ]] && { echo "❌ Error: Version must be >= Current"; exit 1; }

echo "🚀 Starting Release: ${INPUT_VER}"

# ---------- 2.5. Feature: Release Notes ----------
echo "📝 Opening VI to capture release comments..."
sleep 1
NOTES_TMP="$(mktemp)"
trap 'rm -f "${NOTES_TMP}"' EXIT

{
  echo "Release Version: ${INPUT_VER}"
  echo "Project: ${PROJECT_NAME}"
  echo "----------------------------------------"
  echo ""
} > "${NOTES_TMP}"

vi "${NOTES_TMP}"
echo "✅ Release notes captured."

# ---------- 3. Set State to RELEASE Version ----------
TS_SHORT="$(date +"%Y%m%d-%H%M%S")"
TS_ISO="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"

write_kv "${STATE_FILE}" MAJOR "${IN_MAJOR}"
write_kv "${STATE_FILE}" MINOR "${IN_MINOR}"
write_kv "${STATE_FILE}" PATCH "${IN_PATCH}"
write_kv "${STATE_FILE}" LAST_BUILD_TS "${TS_SHORT}"
echo "💾 [1/2] State set to RELEASE: ${INPUT_VER}"

# ---------- 4. Build, Copy & Push Loop ----------
BUILD_INFO_H="${PROJECT_ROOT}/src/build_info.h"
mkdir -p "$(dirname "${BUILD_INFO_H}")"
cat > "${BUILD_INFO_H}" <<EOF
#pragma once
#define BUILD_VERSION   ${INPUT_VER}
#define BUILD_TIMESTAMP ${TS_ISO}
EOF

chip_to_family() { case "$1" in c3) echo "ESP32-C3";; c6) echo "ESP32-C6";; s3) echo "ESP32-S3";; esac; }

for CHIP in "${TARGET_CHIPS[@]}"; do
  CHIP_FAMILY="$(chip_to_family "${CHIP}")"
  # The folder name created in builds/
  BUILD_DIR_NAME="${TS_SHORT}-${INPUT_VER}-${CHIP_FAMILY}-${PROJECT_NAME}"
  TARGET_DIR="${BUILDS_DIR}/${BUILD_DIR_NAME}"

  echo; echo "🧱 [${CHIP_FAMILY}] Compiling..."
  "${SCRIPT_DIR}/compile.sh" -t "${CHIP}" --project-root "${PROJECT_ROOT}" --builds-dir "${BUILDS_DIR}" --work-dir "${WORK_DIR}" --target-dir "${TARGET_DIR}" --project-name "${PROJECT_NAME}" --version "${INPUT_VER}" --timestamp "${TS_ISO}" --venv "${DEFAULT_VENV}"

  # --- Add Release Notes to Binary Folder ---
  # We put the notes inside 'binary' so they get copied along with the firmware
  NOTES_DEST_DIR="${TARGET_DIR}/binary"
  mkdir -p "${NOTES_DEST_DIR}"
  cp "${NOTES_TMP}" "${NOTES_DEST_DIR}/release_notes.txt"

  # --- UPDATE: Copy ONLY 'binary' folder to Static ---
  # Destination: static/firmware/releases/<version>/<build_dir_name>
  STATIC_DEST="${STATIC_RELEASES_ROOT}/${INPUT_VER}/${BUILD_DIR_NAME}"

  echo "📂 [${CHIP_FAMILY}] Copying artifacts to: ${STATIC_DEST}"
  mkdir -p "${STATIC_DEST}"

  # This copies target_dir/binary -> static_dest/binary
  cp -r "${TARGET_DIR}/binary" "${STATIC_DEST}/"
  # --------------------------------------------------

  echo "📤 [${CHIP_FAMILY}] Pushing to '${BRANCH}'..."
  "${SCRIPT_DIR}/push_to_git.sh" --project-root "${PROJECT_ROOT}" --target-dir "${TARGET_DIR}" --version "${INPUT_VER}"
done

# ---------- 5. Set State to NEXT DEV Version ----------
echo; echo "📈 Incrementing Minor Version..."
NEXT_MINOR=$(( 10#${IN_MINOR} + 1 ))

write_kv "${STATE_FILE}" MAJOR "${IN_MAJOR}"
write_kv "${STATE_FILE}" MINOR "${NEXT_MINOR}"
write_kv "${STATE_FILE}" PATCH "000"

NEW_STATE="${IN_MAJOR}.${NEXT_MINOR}.0"
echo "💾 [2/2] State set to NEXT DEV: ${NEW_STATE}"

# ---------- 6. Commit NEW State to 'binaries' ----------
commit_state_only() {
    local index_file
    index_file="$(mktemp)"
    export GIT_INDEX_FILE="${index_file}"
    trap 'rm -f "${index_file}"' RETURN

    local rel_state="${STATE_FILE#"${PROJECT_ROOT}/"}"
    local blob
    blob="$(git -C "${PROJECT_ROOT}" hash-object -w -- "${STATE_FILE}")"

    local remote_tip=""
    git -C "${PROJECT_ROOT}" fetch "${REMOTE}" "${BRANCH}" >/dev/null 2>&1 || true
    if git -C "${PROJECT_ROOT}" ls-remote --exit-code --heads "${REMOTE}" "${BRANCH}" >/dev/null 2>&1; then
        remote_tip="$(git -C "${PROJECT_ROOT}" rev-parse "refs/remotes/${REMOTE}/${BRANCH}^{commit}")"
    fi

    if [[ -n "${remote_tip}" ]]; then
        git -C "${PROJECT_ROOT}" read-tree "${remote_tip}^{tree}"
    else
        git -C "${PROJECT_ROOT}" read-tree --empty
    fi

    git -C "${PROJECT_ROOT}" update-index --add --cacheinfo 100644 "${blob}" "${rel_state}"

    local new_tree new_commit
    new_tree="$(git -C "${PROJECT_ROOT}" write-tree)"

    if [[ -n "${remote_tip}" ]]; then
        new_commit="$(git -C "${PROJECT_ROOT}" commit-tree "${new_tree}" -p "${remote_tip}" -m "chore: bump version state to ${NEW_STATE}")"
    else
        new_commit="$(git -C "${PROJECT_ROOT}" commit-tree "${new_tree}" -m "chore: bump version state to ${NEW_STATE}")"
    fi

    echo "📌 Committing next version state to '${BRANCH}'..."
    if git -C "${PROJECT_ROOT}" push "${REMOTE}" "${new_commit}:refs/heads/${BRANCH}"; then
        echo "✅ Pushed state ${NEW_STATE} to ${BRANCH}"
    else
        echo "❌ Failed to push next state (race condition?)"
        exit 1
    fi
}

commit_state_only

echo "✅ Release Complete."