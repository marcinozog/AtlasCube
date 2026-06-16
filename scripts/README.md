# scripts/

## `build-flash.py` — the script you run

Configure your hardware **once** in [`main/include/defines.h`](../main/include/defines.h)
(display driver, touch, panel profile), then:

```bash
python scripts/build-flash.py
```

The **first run** clones and patches ESP-ADF for you — there's no separate setup
step. Afterwards it compresses the web UI, builds, and asks what to flash:

| Choice (`--scope`) | Flashes | Keeps |
|---|---|---|
| Everything / factory (`all`) | bootloader + partition table + app + `www` + `config` | — (works on a blank chip; resets settings) |
| Firmware only (`fw`) | app slot (OTA-style update) | web UI + settings |
| Firmware + Web UI (`ui`) | app + `www` partition | settings |
| Build only (`build`) | nothing (compile + `web/*.gz`) | everything |
| Erase all (`erase`) | wipes the whole flash (app + web UI + settings + NVS) | — |

Flags: `-p PORT`, `--scope all\|fw\|ui\|build\|erase` (skip the prompt),
`--monitor`, `--clean` (force a clean `sdkconfig`), `--setup` (re-run the
ESP-ADF/IDF patches).

Use **Everything / factory** (`all`) on a fresh or erased chip — it's the only
scope that also writes the bootloader and partition table, so the chip can boot.
`Firmware only` / `Firmware + Web UI` only update the app / web UI and need a
bootloader already present (a blank chip won't boot — `invalid header`). `all`
flashes the full web UI and resets settings to defaults; the device then comes up
in AP mode for Wi-Fi setup.

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
