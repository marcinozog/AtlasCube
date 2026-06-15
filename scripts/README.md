# scripts/

## `build-flash.py` — the script you run

Configure your hardware **once** in [`main/include/defines.h`](../main/include/defines.h)
(display driver, touch, panel profile), then:

```bash
python scripts/build-flash.py
```

The **first run** clones and patches ESP-ADF for you — there's no separate setup
step. Afterwards it compresses the web UI, builds, and asks what to flash:

| Choice | Flashes | Keeps |
|---|---|---|
| Firmware only | app slot | web UI + settings |
| Firmware + Web UI | app + `www` partition | settings |
| Everything (factory) | app + `www` + `config` | — (resets settings) |
| Build only | nothing (compile + `web/*.gz`) | everything |

Flags: `-p PORT`, `--scope fw\|ui\|all\|build` (skip the prompt), `--monitor`,
`--clean` (force a clean `sdkconfig`), `--setup` (re-run the ESP-ADF/IDF patches).

The web UI (`www`) and your settings (`config`) live in separate flash
partitions, so reflashing code or the UI never wipes your settings — only a
factory flash resets them. To tweak the UI without flashing, edit files live in
the browser (the on-device file editor / built-in setup page).

## `build.py` — CI / release only

Builds the merged, distributable per-variant image
(`build/AtlasCube-<variant>.bin`) and is what the CI matrix runs. It selects the
variant from a CLI argument and applies the same ESP-ADF/IDF patches as
`build-flash.py` (which reuses its setup code). **Most users don't need it** —
use `build-flash.py` above.

## `../spiffs_image/tools/compress_web.py`

Compresses `spiffs_image/www/` → `spiffs_image/web/*.gz` for the `www` partition
image. Run automatically by both scripts above; can also be run standalone.
