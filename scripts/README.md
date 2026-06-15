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
| First flash (blank chip) | bootloader + partition table + app | — (leaves `www` empty → setup page) |
| Firmware only | app slot (OTA-style update) | web UI + settings |
| Firmware + Web UI | app + `www` partition | settings |
| Everything (factory) | app + `www` + `config` | — (resets settings) |
| Build only | nothing (compile + `web/*.gz`) | everything |

Flags: `-p PORT`, `--scope fresh\|fw\|ui\|all\|build` (skip the prompt),
`--monitor`, `--clean` (force a clean `sdkconfig`), `--setup` (re-run the
ESP-ADF/IDF patches).

Use **First flash** on a fresh or erased chip — `Firmware only` writes just the
app and needs a bootloader already present, so a blank chip won't boot
(`invalid header`). First flash writes the bootloader and partition table too,
leaving `www` empty so the device boots into the built-in setup page (enter
Wi-Fi, upload the web UI).

The web UI (`www`) and your settings (`config`) live in separate flash
partitions, so reflashing code or the UI never wipes your settings — only a
factory flash resets them. To tweak the UI without flashing, edit files live in
the browser (the on-device file editor / built-in setup page).

## `env_setup.py` — shared module (don't run it)

Not a script — a library of toolchain helpers (locate ESP-IDF/ESP-ADF, clone
ESP-ADF if missing, patch ESP-ADF + ESP-IDF). Imported by both `build-flash.py`
and `../ci/build.py` so the setup logic has a single home.

## `../ci/build.py` — CI / release only

Builds the merged, distributable per-variant image
(`build/AtlasCube-<variant>.bin`) and is what the CI matrix runs. It selects the
variant from a CLI argument (overwriting `defines.h`) and reuses the setup from
`env_setup.py`. **Most users don't need it** — use `build-flash.py` above.

## `../spiffs_image/tools/compress_web.py`

Compresses `spiffs_image/www/` → `spiffs_image/web/*.gz` for the `www` partition
image. Run automatically by both scripts above; can also be run standalone.
