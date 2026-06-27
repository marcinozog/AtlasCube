# lib/

Precompiled internal libraries linked into the firmware. Each `.a` is built for
`xtensa-esp32s3` and pulled in by the component that uses it via
`add_prebuilt_library(... "${CMAKE_SOURCE_DIR}/lib/<name>.a")`.

These are committed binaries (the `*.a` ignore rule has an explicit exception for
this directory) so a plain `idf.py build` and CI work without extra steps.
