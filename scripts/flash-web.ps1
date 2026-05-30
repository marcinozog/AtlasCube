# Flash firmware WITH the SPIFFS web UI bundled (run from the ESP-IDF terminal).
# Use after changing files in spiffs_image/www/.
# Usage:  ./scripts/flash-web.ps1 [-p COM5] [idf.py args]
#   No args  -> defaults to `flash` (build + flash on the auto-detected port).
#   e.g.     ./scripts/flash-web.ps1 -p COM5 flash monitor
$env:ATLAS_SPIFFS = "1"
python spiffs_image/tools/compress_web.py
# Force a reconfigure so CMake picks up ATLAS_SPIFFS=1 (env changes don't retrigger configure)
idf.py reconfigure
if ($args.Count -gt 0) { idf.py @args } else { idf.py flash }
# Reset project back to fast (no-SPIFFS) config so extension buttons stay quick
Remove-Item Env:ATLAS_SPIFFS -ErrorAction SilentlyContinue
idf.py reconfigure
