#!/usr/bin/env bash
# Switch the active hardware variant in main/include/defines.h.
# Equivalent to manually toggling the `// #define` / `#define` lines.
#
# Usage:   scripts/select-variant.sh <variant>
#          variants: ili9341 | st7796 | co5300
#
# After running:   idf.py fullclean && idf.py build
# (fullclean is needed so sdkconfig is regenerated from sdkconfig.variant)

set -euo pipefail

VARIANT="${1:-}"
case "${VARIANT}" in
    ili9341) DISPLAY=ILI9341; PROFILE=320x240;     TOUCH=FT6336U ;;
    st7796)  DISPLAY=ST7796;  PROFILE=480x320;     TOUCH=FT6336U ;;
    co5300)  DISPLAY=CO5300;  PROFILE=240X296;     TOUCH=CST816D ;;
    *) echo "Usage: $0 <ili9341|st7796|co5300>" >&2; exit 1 ;;
esac

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEFINES="${REPO_ROOT}/main/include/defines.h"

# For each item in the group, ensure exactly the `active` one is uncommented
# and the rest commented. Works whether the current state is active/commented;
# regex requires the define to stand alone on its line (no value after the name),
# so it won't touch DISPLAY_HOST / DISPLAY_PIN_* etc.
set_group() {
    local active="$1"; shift
    local tmp="${DEFINES}.tmp"
    for item in "$@"; do
        if [[ "${item}" == "${active}" ]]; then
            # Uncomment "// #define X" → "#define X" (no-op if already active)
            sed -E "s|^([[:space:]]*)//[[:space:]]*(#define[[:space:]]+${item})([[:space:]]*)$|\1\2\3|" \
                "${DEFINES}" > "${tmp}"
        else
            # Comment "#define X" → "// #define X" (no-op if already commented)
            sed -E "s|^([[:space:]]*)(#define[[:space:]]+${item})([[:space:]]*)$|\1// \2\3|" \
                "${DEFINES}" > "${tmp}"
        fi
        mv "${tmp}" "${DEFINES}"
    done
}

set_group "DISPLAY_${DISPLAY}"     DISPLAY_ILI9341 DISPLAY_ST7796 DISPLAY_CO5300 DISPLAY_SSD1322
set_group "TOUCH_${TOUCH}"         TOUCH_FT6336U TOUCH_CST816D
set_group "UI_PROFILE_${PROFILE}"  UI_PROFILE_240X296 UI_PROFILE_320x240 UI_PROFILE_480x320 UI_PROFILE_MONO_128X64

echo "Variant set: DISPLAY_${DISPLAY} + UI_PROFILE_${PROFILE} + TOUCH_${TOUCH}"
echo "Active defines:"
grep -E "^[[:space:]]*#define[[:space:]]+(DISPLAY_(ILI9341|ST7796|CO5300|SSD1322)|TOUCH_(FT6336U|CST816D)|UI_PROFILE_(240X296|320x240|480x320|MONO_128X64))[[:space:]]*$" "${DEFINES}" \
    | sed 's/^/  /'
