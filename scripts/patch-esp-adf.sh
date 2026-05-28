#!/usr/bin/env bash
# Idempotent setup for ESP-ADF + ESP-IDF needed to build AtlasCube.
# Run once locally after installing ESP-IDF + cloning ESP-ADF.
# CI runs it on every job — repeat runs are a no-op.
#
# Usage:   scripts/patch-esp-adf.sh
# Env:     ADF_PATH (required) — path to esp-adf checkout
#          IDF_PATH (required) — path to esp-idf checkout

set -euo pipefail

: "${ADF_PATH:?ADF_PATH must be set (path to esp-adf checkout)}"
: "${IDF_PATH:?IDF_PATH must be set (path to esp-idf checkout)}"

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOARD_NAME="esp32_s3_atlascube"
BOARD_SRC="${REPO_ROOT}/components/audio_board/${BOARD_NAME}"
ADF_AUDIO_BOARD="${ADF_PATH}/components/audio_board"

echo "==> ESP-ADF: ${ADF_PATH}"
echo "==> ESP-IDF: ${IDF_PATH}"
echo "==> Repo:    ${REPO_ROOT}"

# ── 1. Submodules ───────────────────────────────────────────────────────────
echo "==> Initializing ESP-ADF submodules"
git -C "${ADF_PATH}" submodule update --init \
    components/esp-adf-libs components/esp-sr

# ── 2. Board sources ────────────────────────────────────────────────────────
if [[ ! -d "${BOARD_SRC}" ]]; then
    echo "ERROR: board sources missing at ${BOARD_SRC}" >&2
    exit 1
fi
echo "==> Installing board sources to ${ADF_AUDIO_BOARD}/${BOARD_NAME}"
rm -rf "${ADF_AUDIO_BOARD}/${BOARD_NAME}"
cp -r "${BOARD_SRC}" "${ADF_AUDIO_BOARD}/${BOARD_NAME}"

# ── 3. Patch Kconfig.projbuild ──────────────────────────────────────────────
KCONFIG="${ADF_AUDIO_BOARD}/Kconfig.projbuild"
if grep -q "ESP32_S3_ATLASCUBE_BOARD" "${KCONFIG}"; then
    echo "==> Kconfig.projbuild already patched"
else
    echo "==> Patching ${KCONFIG}"
    awk '
        !done && /^endchoice/ {
            print "config ESP32_S3_ATLASCUBE_BOARD"
            print "    bool \"ESP32-S3-AtlasCube\""
            done = 1
        }
        { print }
    ' "${KCONFIG}" > "${KCONFIG}.tmp"
    mv "${KCONFIG}.tmp" "${KCONFIG}"
fi

# ── 4. Patch CMakeLists.txt ─────────────────────────────────────────────────
CMAKE="${ADF_AUDIO_BOARD}/CMakeLists.txt"
if grep -q "CONFIG_ESP32_S3_ATLASCUBE_BOARD" "${CMAKE}"; then
    echo "==> CMakeLists.txt already patched"
else
    echo "==> Patching ${CMAKE}"
    awk '
        /register_component\(\)/ && !done {
            print "if (CONFIG_ESP32_S3_ATLASCUBE_BOARD)"
            print "message(STATUS \"Current board name is \" CONFIG_ESP32_S3_ATLASCUBE_BOARD)"
            print "list(APPEND COMPONENT_ADD_INCLUDEDIRS ./esp32_s3_atlascube)"
            print "set(COMPONENT_SRCS"
            print "./esp32_s3_atlascube/board.c"
            print "./esp32_s3_atlascube/board_pins_config.c"
            print ")"
            print "endif()"
            print ""
            done = 1
        }
        { print }
    ' "${CMAKE}" > "${CMAKE}.tmp"
    mv "${CMAKE}.tmp" "${CMAKE}"
fi

# ── 5. Patch component.mk (legacy GNU Make build, harmless if unused) ───────
COMP_MK="${ADF_AUDIO_BOARD}/component.mk"
if [[ -f "${COMP_MK}" ]]; then
    if grep -q "CONFIG_ESP32_S3_ATLASCUBE_BOARD" "${COMP_MK}"; then
        echo "==> component.mk already patched"
    else
        echo "==> Patching ${COMP_MK}"
        cat >> "${COMP_MK}" <<'EOF'

ifdef CONFIG_ESP32_S3_ATLASCUBE_BOARD
COMPONENT_ADD_INCLUDEDIRS += ./esp32_s3_atlascube
COMPONENT_SRCDIRS += ./esp32_s3_atlascube
endif
EOF
    fi
else
    echo "==> No component.mk in ESP-ADF (CMake-only build) — skipping"
fi

# ── 6. ESP-IDF FreeRTOS patch ───────────────────────────────────────────────
PATCH="${ADF_PATH}/idf_patches/idf_v5.5_freertos.patch"
if [[ ! -f "${PATCH}" ]]; then
    echo "WARN: FreeRTOS patch not found at ${PATCH} — skipping" >&2
else
    if git -C "${IDF_PATH}" apply --reverse --check --ignore-whitespace "${PATCH}" >/dev/null 2>&1; then
        echo "==> FreeRTOS patch already applied"
    elif git -C "${IDF_PATH}" apply --check --ignore-whitespace "${PATCH}" >/dev/null 2>&1; then
        echo "==> Applying FreeRTOS patch"
        git -C "${IDF_PATH}" apply --ignore-whitespace "${PATCH}"
    else
        echo "ERROR: FreeRTOS patch neither applicable nor already-applied" >&2
        echo "       Inspect ${IDF_PATH} state manually." >&2
        exit 1
    fi
fi

echo "==> patch-esp-adf.sh: done"
