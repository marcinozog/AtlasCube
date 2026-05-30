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

string(REGEX MATCH "\n[ \t]*#define[ \t]+DISPLAY_(ILI9341|ST7796|CO5300|SSD1322)[^A-Za-z0-9_]"
       _ "${_atlas_defines}")
set(ATLAS_DISPLAY "${CMAKE_MATCH_1}")

string(REGEX MATCH "\n[ \t]*#define[ \t]+TOUCH_(CST816D|FT6336U)[^A-Za-z0-9_]"
       _ "${_atlas_defines}")
set(ATLAS_TOUCH "${CMAKE_MATCH_1}")

string(REGEX MATCH "\n[ \t]*#define[ \t]+FLASH_(8MB|16MB)[^A-Za-z0-9_]"
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

# Bundle spiffs_image/web into the `storage` SPIFFS partition.
# Off by default for fast code-only flashing. Enable per-build with:
#   ATLAS_SPIFFS=1 idf.py build         (Linux/macOS)
#   $env:ATLAS_SPIFFS=1; idf.py build   (PowerShell)
# CI/release always sets ATLAS_SPIFFS=1 so the flashed firmware ships the web UI.
if(DEFINED ENV{ATLAS_SPIFFS} AND NOT "$ENV{ATLAS_SPIFFS}" STREQUAL "0")
    spiffs_create_partition_image(storage spiffs_image/web FLASH_IN_PROJECT)
endif()