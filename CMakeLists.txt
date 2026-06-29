cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    $ENV{ADF_PATH}/components
)

# ── Variant selection ────────────────────────────────────────────────────────
# Single source of truth for the HW variant: main/include/defines.h.
# We parse the active DISPLAY_* / TOUCH_* defines there and generate
# sdkconfig.variant so CMake/Kconfig stay in sync — no need to edit sdkconfig.
set(DEFINES_H "${CMAKE_CURRENT_SOURCE_DIR}/main/include/defines.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DEFINES_H}")
file(READ "${DEFINES_H}" _atlas_defines)
set(_atlas_defines "${_atlas_defines}\n")

string(REGEX MATCH "\n[ \t]*#define[ \t]+DISPLAY_(ILI9341|ST7796|ILI9488|CO5300|SSD1322)[^A-Za-z0-9_]"
       _ "${_atlas_defines}")
set(ATLAS_DISPLAY "${CMAKE_MATCH_1}")

string(REGEX MATCH "\n[ \t]*#define[ \t]+TOUCH_(CST816D|FT6336U|XPT2046|NONE)[^A-Za-z0-9_]"
       _ "${_atlas_defines}")
set(ATLAS_TOUCH "${CMAKE_MATCH_1}")

string(REGEX MATCH "\n[ \t]*#define[ \t]+FLASH_(16MB)[^A-Za-z0-9_]"
       _ "${_atlas_defines}")
set(ATLAS_FLASH "${CMAKE_MATCH_1}")

if(NOT ATLAS_DISPLAY)
    message(FATAL_ERROR "No active DISPLAY_* define in ${DEFINES_H}")
endif()
if(NOT ATLAS_TOUCH)
    message(FATAL_ERROR "No active TOUCH_* define in ${DEFINES_H}")
endif()
if(NOT ATLAS_FLASH)
    message(FATAL_ERROR "No active FLASH_* define in ${DEFINES_H}")
endif()

message(STATUS "AtlasCube variant: DISPLAY=${ATLAS_DISPLAY}, TOUCH=${ATLAS_TOUCH}, FLASH=${ATLAS_FLASH}")

set(VARIANT_SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig.variant")
file(WRITE "${VARIANT_SDKCONFIG}"
    "# Auto-generated from main/include/defines.h — do not edit\n"
    "CONFIG_DISPLAY_${ATLAS_DISPLAY}=y\n"
    "CONFIG_TOUCH_${ATLAS_TOUCH}=y\n"
    "CONFIG_ESPTOOLPY_FLASHSIZE_${ATLAS_FLASH}=y\n"
    "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"partitions${ATLAS_FLASH}.csv\"\n"
    "CONFIG_PARTITION_TABLE_FILENAME=\"partitions${ATLAS_FLASH}.csv\"\n"
)
set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;${VARIANT_SDKCONFIG}")
# ─────────────────────────────────────────────────────────────────────────────

include_directories(${CMAKE_SOURCE_DIR}/main/include)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(IDF_COMPONENT_MANAGER_VERBOSE 1)

# Derive firmware version from `git describe` (annotated tags).
# Embedded in the app descriptor and visible via esp_app_get_description().
set(PROJECT_VER_FROM_GIT 1)

project(atlascube)

# ── LVGL heap pool → PSRAM ────────────────────────────────────────────────────
# CONFIG_LV_CONF_SKIP=y, so LVGL is configured entirely from Kconfig (there is no
# lv_conf.h) — and Kconfig exposes no knob for LV_ATTRIBUTE_LARGE_RAM_ARRAY.
# That attribute marks LVGL's builtin TLSF pool (work_mem_int, = LV_MEM_SIZE,
# 64 KB here); left empty it lands in the scarce internal DRAM. Force-include
# esp_attr.h and point the attribute at EXT_RAM_BSS_ATTR so the whole pool moves
# to PSRAM .bss (needs CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y). Only the
# object/style/anim pool moves — the display flush buffer is allocated separately
# in the drivers (DMA|INTERNAL), so render performance is unaffected.
idf_component_get_property(_lvgl_lib lvgl__lvgl COMPONENT_LIB)
target_compile_options(${_lvgl_lib} PRIVATE -include esp_attr.h)
target_compile_definitions(${_lvgl_lib} PRIVATE LV_ATTRIBUTE_LARGE_RAM_ARRAY=EXT_RAM_BSS_ATTR)

# Build both SPIFFS partition images so the merged release .bin (and a full
# `idf.py flash`) ships a working device:
#   www    — the compressed web UI. Run spiffs_image/tools/compress_web.py first;
#            build.py / CI do this automatically.
#   config — default user settings for a fresh device.
# An OTA app update never touches either partition; a full flash re-seeds them.
# For code-only dev iteration that must preserve on-device settings, flash just
# the app slot with `idf.py app-flash` instead of `idf.py flash`.
spiffs_create_partition_image(www    spiffs_image/web    FLASH_IN_PROJECT)
spiffs_create_partition_image(config spiffs_image/config FLASH_IN_PROJECT)