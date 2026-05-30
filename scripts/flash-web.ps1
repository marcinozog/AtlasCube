# Flash firmware WITH the SPIFFS web UI bundled (run from the ESP-IDF terminal).
# Use after changing files in spiffs_image/www/.
# Usage:  ./scripts/flash-web.ps1 -p COM5 flash
$env:ATLAS_SPIFFS = "1"
python spiffs_image/tools/compress_web.py
idf.py reconfigure
idf.py @args
# Reset project back to fast (no-SPIFFS) config so extension buttons stay quick
Remove-Item Env:ATLAS_SPIFFS -ErrorAction SilentlyContinue
idf.py reconfigure
