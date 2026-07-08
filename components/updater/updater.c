// Public glue for the auto-update component. The real logic (version check,
// www staleness, pull-OTA) is in the prebuilt lib/libupdater_impl.a; this file
// provides only what must be compiled per-variant in the public build: the
// FW_VARIANT accessor the prebuilt reads at runtime.
#include "updater.h"

#ifndef FW_VARIANT
// Fallback for builds where CMake did not inject the variant key (should not
// happen in a normal build — CMakeLists.txt always defines it).
#define FW_VARIANT "unknown"
#endif

const char *app_fw_variant(void)
{
    return FW_VARIANT;
}
