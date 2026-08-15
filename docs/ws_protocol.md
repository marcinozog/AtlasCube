# Control protocol — WebSocket + REST

The wire contract between the device and every remote client. There are three
clients today and they all speak exactly this:

| Client | Repo |
|---|---|
| Built-in web UI | [spiffs_image/www/](../spiffs_image/www/) (`app.js`, `bt.js`, `sd.js`, …) |
| AtlasCubeRemote (Android) | `D:\Projekty\AndroidStudio\AtlasCubeRemote` |
| AtlasCubeController (hardware pilot) | `D:\Projekty\VSCode\ESP32\AtlasCubeController` |

This file is the single source of truth for the contract. Anything that changes
[components/ws/ws_server.c](../components/ws/ws_server.c) or the `/api/*`
handlers in [components/web/http_server.c](../components/web/http_server.c)
must update this file **in the same commit** — otherwise the three clients drift
apart, each carrying its own stale copy of the protocol.

## Transport

| | |
|---|---|
| HTTP server | port **80**, no TLS |
| WebSocket | `ws://<host>/ws` — the only control channel |
| Authentication | **none** — the device trusts everything on the LAN |
| Discovery | mDNS `_http._tcp` on port 80, instance name `AtlasCube Radio`, TXT `host=<fqdn>` and `path=/` ([components/network/mdns_service.c](../components/network/mdns_service.c)) |
| Max WS clients | 8 (`MAX_WS_CLIENTS`); a 9th handshake is logged and gets no slot — it still receives the initial state but no broadcasts |
| Max incoming frame | 4096 B (`WS_MAX_FRAME_LEN`); a larger frame drops the client's socket |
| Second transport | the hardware pilot speaks the plain-text vocabulary below over ESP-NOW instead of WS — link layer in [espnow_link.md](espnow_link.md) |

**Dispatch rule:** a frame whose first byte is `{` is parsed as JSON, everything
else is treated as a plain-text command. One frame = one command; there is no
batching and no framing beyond that.

**Push model:** the device pushes the full state on every new WS connection
(`send_full_state()` in the handshake branch) and again after every state change
(`app_state_subscribe(on_state_change)`). Consequences for clients:

- **There is no "give me the state" command.** No `getindex`, no `get_state`.
  Connect and wait — the first frame arrives unprompted.
- Every `type:"state"` frame is a **full snapshot**, never a delta. Clients may
  overwrite their whole model on each frame.
- Commands are **fire-and-forget**: no ack, no per-command reply, no error
  frame. An unknown or malformed command is logged on the device
  (`Unknown plain CMD: …`) and silently dropped. Confirmation only ever arrives
  as the next state broadcast.

## Client → device: plain-text commands

Handled by `media_command_execute_text()` in
[components/services/media_control.c](../components/services/media_control.c) —
transport-independent, and therefore **shared verbatim with the ESP-NOW pilot
link** ([espnow_link.md](espnow_link.md)). A command added to this table works on
both links at once; it must never be reimplemented per transport.

These are **semantic transport** commands: they
act on whatever source is playing right now (radio / SD / BT), resolved by
`media_source_current()` — the client does not need to know what is playing.

| Frame | Effect |
|---|---|
| `play` | Play/resume the active source (no-op if already playing) |
| `stop` | Stop the active source (tears down, keeps the queue) |
| `toggle` | Play/stop toggle on the active source |
| `next` | Next track / station (radio wraps around the playlist) |
| `prev` | Previous track / station |
| `volp` | Volume **+5** (clamped to 100) |
| `volm` | Volume **−5** (clamped to 0) |
| `vol=N` | Set volume; `N` outside 0…100 is ignored, not clamped |
| `playstation=N` | Play playlist entry `N` — **0-based** |
| `source=radio\|sd\|bt` | Explicit source switch; the target service takes over the output |

## Client → device: JSON commands

Every JSON frame is an object with a `"cmd"` string. Unknown `cmd` values and
missing/wrong-typed arguments are ignored without a reply.

### Radio and playlist

| `cmd` | Arguments | Notes |
|---|---|---|
| `play` | `url` (string), `finite` (bool), `station` (string), `title` (string), `offset_bytes` (number), `curr_index` (number) | Play an arbitrary URL. `finite:true` = podcast episode: EOF is a clean end, no retry. `station`/`title` set the two screen lines (podcasts carry no ICY metadata); both are cleared when omitted. `offset_bytes` resumes mid-file via a Range request. `curr_index` only updates the stored index. |
| `stop` | — | Stops the radio specifically (unlike plain `stop`, which is source-aware) |
| `play_index` | `index` (number) | **0-based**; same as `playstation=N` |
| `play_file` | `path` (string) | Test hook: play one local file from the SD card |
| `set_volume` | `value` (number) | 0…100 |
| `set_eq_10` | `bands` (array of 10 numbers) | Rejected unless the array has **exactly 10** entries. Gains in dB, one per band (31 Hz … 16 kHz). The firmware does not clamp; the web UI exposes −13…+6. |

### SD-card player

| `cmd` | Arguments | Notes |
|---|---|---|
| `sd_play` | `dir` (string, optional) | Play a folder; `null`/omitted = current folder |
| `sd_play_path` | `path` (string) | Play one file |
| `sd_next` / `sd_prev` / `sd_stop` | — | |
| `sd_pause` | — | Toggles in-place pause |
| `sd_shuffle` | — | **Toggles** shuffle (not a setter) |
| `sd_repeat` | — | **Cycles** repeat: none → all → one |
| `sd_list` | `dir` (string, optional) | Rescans and replies with a `type:"sd_list"` broadcast (see below) |

### Bluetooth

| `cmd` | Arguments | Notes |
|---|---|---|
| `bt_enable` | `value` (bool) | Persisted |
| `bt_volume` | `value` (number) | 0…100, module volume |
| `bt_sync_vol` | `value` (bool) | Keep the module volume in sync with the phone |
| `bt_cmd` | `value` (string) | Raw AT string passed to the module (`AT+PV`, `AT+PA`, …). Module-specific — prefer the semantic commands below. |
| `bt_play` | — | Makes BT the active source (enabled **volatile** — no settings write, because the httpd task stack cannot take the full-settings JSON build) and tells the module to play |
| `bt_pause` / `bt_next` / `bt_prev` | — | Semantic transport, resolved through the BT module descriptor |
| `bt_reboot` | — | Reboots the BT module |

### Screens

| `cmd` | Arguments | Notes |
|---|---|---|
| `set_screen` | `value`: `radio` \| `home` \| `bt` | `clock` is accepted as a legacy alias for `home` |

## Device → client messages

### `type:"state"` — full snapshot

Built by `send_full_state()`. Sent on connect and on every state change.

| Field | Type | Meaning |
|---|---|---|
| `type` | string | `"state"` |
| `radio` | string | `stopped` \| `playing` \| `buffering` \| `error` \| `finished` |
| `source` | string | `radio` \| `sd` \| `bt` — **resolved server-side**, so clients don't replicate the nuance that a paused SD queue still counts as SD |
| `volume` | number | 0…100 |
| `url` | string | Current stream URL |
| `station_name` | string | `""` when unknown |
| `title` | string | ICY / tag title, `""` when unknown |
| `curr_index` | number | Current playlist index, **0-based** |
| `sr` | number | Sample rate (Hz); `0` = nothing decoding |
| `bits` | number | Bit depth |
| `ch` | number | Channel count |
| `br` | number | Bitrate (bit/s) |
| `fmt` | number | Codec id, see table below |
| `eq` | array[10] | Current EQ gains |
| `rssi` | number | WiFi RSSI in dBm; `0` when STA is not connected (AP mode) |
| `sd_active` | bool | SD player is the active source |
| `sd_index` / `sd_count` | number | Position in the scanned queue |
| `sd_track` | string | Current file name |
| `sd_dir` | string | Folder of the current track |
| `sd_paused` | bool | |
| `sd_shuffle` | bool | |
| `sd_repeat` | number | `0` none, `1` all, `2` one |
| `sd_position_ms` | number | Playback position of the current SD track, ms. `0` when idle. Frozen while `sd_paused`, and runs ~0.5 s ahead of the speaker (pipeline startup latency). **Extrapolate it** — see below |
| `sd_duration_ms` | number | Length of the current SD track, ms. `0` = unknown (FLAC/AAC, or an unparseable header) — a client showing a progress bar must hide it rather than divide by zero |
| `bt_enable` | bool | |
| `bt_state` | number | `0` connected, `1` disconnected, `2` discoverable (`bt_state_t`) |
| `bt_playing` | bool | Phone-side AVRCP playback is running. The only way to tell which direction a BT play/pause should go: `bt_play` / `bt_pause` act on the module whatever the active source is, while the plain-text `toggle` follows `media_source_current()` and would hit the radio or the SD player instead |
| `bt_volume` | number | 0…100 |
| `bt_vol_sync` | bool | |
| `bt_title` / `bt_artist` | string | AVRCP metadata |
| `bt_duration_ms` | number | Track length (ms) |
| `bt_position_s` | number | Position (s) |
| `bt_codec` | string | e.g. `"LDAC"` |
| `bt_sample_rate` / `bt_bits` | number | BT link audio format |

`fmt` codec ids (ADF `music_info.codec_fmt`, mirrored in `codecMap` in
[app.js](../spiffs_image/www/app.js)):

| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| UNK | RAW | WAV | MP3 | AAC | OPUS | M4A | MP4 | FLAC |

| 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 |
|---|---|---|---|---|---|---|---|
| OGG | TSAAC | AMR-NB | AMR-WB | PCM | M3U8 | PLS | UNSUP |

**Not in the state frame**, even though clients often want it: `eq_enabled`,
`mono`, brightness, theme, screensaver and screen config. Those live behind
`GET /api/settings` — the WS state is playback state only.

### `type:"sd_list"` — folder listing

Reply to the `sd_list` command. Broadcast to **all** clients, not just the
requester.

```json
{ "type":"sd_list", "dir":"/sdcard/music/rock", "parent":"/sdcard/music",
  "folders":["live","studio"], "tracks":["01.mp3","02.mp3"] }
```

`parent` is `""` at the music root (it never escapes the root).

### `type:"bt_log"` — BT module trace

```json
{ "type":"bt_log", "data":"<raw line from the BT module>" }
```

Diagnostics for the BT page; safe to ignore.

## REST endpoints for control clients

Only what a remote needs. The full surface (settings, theme, pins, files, OTA,
wallpaper, UI profiles) is registered in
[components/web/http_server.c](../components/web/http_server.c) around
`http_server_start()`.

| Method + path | Purpose |
|---|---|
| `GET /api/state` | HTTP snapshot for reachability probes and fields absent from the WS state: `theme`, `radio_state` (**numeric enum here, unlike the WS string**), `station`, `title`, `volume`, `bt_enable`, `bt_auto_switch`, `bt_show_screen`, `sd_show_screen`, `radio_show_screen`, `follow_source`, `time_synced`, `wifi_mode` (`sta`\|`ap`), `version` (git describe), `www_outdated`, `www_version`, `www_expected` |
| `GET /api/playlist` | `[{name,url,favorite,stationuuid,icon}, …]` in stored order |
| `POST /api/playlist` | Same array; **overwrites** the whole playlist file and reloads it. Max 512 entries, body ≤ 32 KB. Per entry: `name` < 64, `url` < 256, `stationuuid` < 37, `icon` < 128 chars; entries missing `name`/`url` or violating a limit are **skipped silently**, not rejected. `icon` is an SD-relative path starting with `/`, no `..`. Written atomically (tmp → rename). |
| `GET /api/playlist.csv` | The on-disk format: `name\turl\t<0\|1>\tstationuuid\ticon_path\n` (three-column legacy files still load) |
| `/api/events*` | Reminders, playback schedules, calendar mirror — see [events.md](events.md) |
| `GET /api/settings`, `POST /api/settings` | Device configuration. POST is a **partial patch**; sections: `display`, `bluetooth`, `audio`, `wifi`, `ntp`, `playlist`, `scrsaver`, `dashboard`, `trace`. `display.wallpaper_urls` is an array of internet-wallpaper slot URLs (index = slot); the older scalar `display.wallpaper_url` still reads and writes **slot 0**, so a pre-slots client keeps working. `display.asset_urls` is the same shape for the four internet-asset slots (small PNG artwork such as slider knobs, addressed as `asset0`..`asset3` from ui_profile image fields); they share the wallpapers' `wallpaper_fetch_mode` schedule and batch. `trace` is one boolean per runtime diagnostic-log flag (`touch`, `display`, `audio`, `web`, `espnow` today) — extra serial-console output per subsystem, off by default and persisted, so an armed flag also covers the next boot. **Read the flag names from the GET response rather than hard-coding them**: the device emits its own table, and a firmware that gains a flag advertises it there |
| `POST /api/wifi/scan` | Starts an async scan for nearby APs; `{"ok":true,"busy":true}`. A no-op while one is already running. In STA mode the device stays connected but hops off-channel, so a playing stream may stutter. |
| `GET /api/wifi/scan` | Polls it: `{"busy":false,"aps":[{"ssid","rssi","secure"}, …]}`. `aps` is empty while `busy` is true; poll ~700 ms until it clears. Max 20 entries, hidden SSIDs skipped, deduplicated by SSID and sorted by `rssi` (dBm, negative — closer to 0 is stronger). |
| `POST /api/restart` | Reboot (`OPTIONS` is handled for CORS preflight) |
| `GET /api/diag` | Read-only health snapshot for bug reports, grouped: `fw` (`version`, `build`, `idf`, `variant`, `update_available`, `update_latest`), `www` (`outdated`, `version`, `expected`), `psram` and `internal` (`total`, `free`, `min_free`; `internal` adds `largest`), `hw` (`chip`, `revision`, `cores`, `flash_size`, `panel_w`, `panel_h`, plus `temp_c` — chip die temperature in °C, one decimal, omitted when the sensor is unavailable; it runs 15–25 °C above ambient, so it is a trend, not a room reading), `net` (`connected`, `ssid`, `ip`, `rssi`, `mac`, plus `sockets`/`sockets_max` — HTTP sessions in use out of the pool shared by the web UI, the WebSocket and the app; the request being answered counts as one, and `sockets` is `-1` if the count is unavailable), `mqtt` (`enabled`, `connected`, `host`, `port` — the whole group is absent on firmware built without MQTT), `sd` (`mounted`, `total`, `free` — zeros unless the card is already mounted), `cpu` (`core0`, `core1`, % **since the previous GET**, so the first read returns 0) and `uptime_s`. Byte counts are bytes. Same data as the on-device Settings → System → Diagnostics screen. |

## Conventions and gotchas

- **All station indices are 0-based** — `playstation=N`, `play_index`,
  `curr_index`, and the `station` field of a schedule event.
- **Index = position in the stored playlist**, i.e. the order returned by
  `GET /api/playlist`. The device does **no** sorting. The web editor pins
  favourites to the top *before saving*, which is why the stored order usually
  looks favourites-first — but that is an editor convention, not a device rule.
  A client that writes the playlist itself owns the resulting indices.
- **Plain `stop` ≠ JSON `stop`.** The plain command is source-aware
  (`media_control_execute`); the JSON one always stops the radio.
- **`sd_shuffle` / `sd_repeat` are toggles/cycles, not setters.** To reach a
  known state, read it back from the state broadcast.
- **`sd_position_ms` does not tick.** The state broadcast fires on state
  *changes*, not on a timer, so a client that just displays the field shows a
  counter that only moves when something else happens. Extrapolate instead —
  the value is a wall-clock delta, so the device's own screen and a client run
  the same arithmetic:

  ```
  shown = sd_position_ms + (now - when_the_frame_arrived)   // freeze if sd_paused
  ```

  Every new broadcast re-anchors it, so drift cannot accumulate. Do not ask for
  periodic broadcasts to avoid this: that would cost every connected client a
  frame per second for a number it can derive.
- **Volume steps are fixed at 5** for `volp`/`volm`. A client wanting a
  different step must send `vol=N`.
- `vol=N` **ignores** out-of-range values while `volp`/`volm` clamp — do not
  rely on the device to clamp an explicit setpoint.
- **No acks.** A client that needs certainty must watch the state broadcast; a
  command that changes nothing produces no traffic at all.
- **Broadcasts are shared.** `sd_list` goes to every connected client, so a
  client must tolerate replies to requests it never made.
