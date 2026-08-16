# ESP-NOW link — hardware remote

The second control transport. Carries the **same command vocabulary** as the
WebSocket channel, over a different link.

| | |
|---|---|
| Peer | AtlasCubeController (hardware remote), `D:\Projekty\VSCode\ESP32\AtlasCubeController` |
| Vocabulary | defined in [ws_protocol.md](ws_protocol.md) — **that file stays the source of truth** |
| This file | the link layer only: pairing, channel, framing, retries, and the remote-only commands the WS transport does not need |

**Layering rule:** the link owns framing (sequence byte, page splitting,
retries). It never owns meaning. A remote→radio command frame is stripped of its
framing and handed to the *same* `media_command_execute_text()` the WS server
calls. If a second interpretation of `next` ever appears in the ESP-NOW handler,
the two transports have already diverged — the whole point of this split is that
adding a command to the table in `ws_protocol.md` lights it up on both links at
once.

## Why this transport exists

The remote is battery-powered and sleeps. That single fact drives every
difference from the WS contract:

- **It does not associate at all.** No DHCP, no 2–5 s connect on every button
  press. It transmits raw ESP-NOW frames on a stored channel — and that now
  covers its settings and diagnostics screens too, not just playback. The
  AtlasCubeController joins the AP only when it is built to run the WS transport
  instead of this link, which is a startup choice, not something a screen does.

  > This used to say the remote joined the AP on demand for the configuration
  > screens, "which do not fit in a 1490 B frame". That was true of the whole
  > settings tree and false of what a remote actually edits: a handful of scalars
  > and a ten-band array, which fit with room to spare. See `get_cfg` below.
- **It cannot receive pushes.** The WS model ("device pushes full state on
  every change, there is no *give me the state* command") is unusable — the
  remote is asleep when the state changes. This link is **pull**: the remote asks,
  the radio answers, the remote sleeps.
- **Every frame is acknowledged.** Unicast ESP-NOW is ACKed at the MAC layer,
  so `esp_now_send()`'s TX callback tells the remote whether the radio heard it.
  WS commands are fire-and-forget; these are not.

## Link layer

### Radio side

ESP-NOW runs alongside the existing STA interface, on the same radio and the
same channel the router imposed. `esp_now_init()` is called **after** WiFi is up
in STA mode.

Two preconditions, both already true today:

| Requirement | Status |
|---|---|
| `esp_wifi_set_ps(WIFI_PS_NONE)` — a power-saving STA sleeps between beacons and drops ESP-NOW frames | already set in [wifi_manager.c:188](../components/network/wifi_manager.c#L188) |
| Mains power, WiFi always on | by design, the radio never sleeps |

The radio registers the paired remote as a peer with **`channel = 0`** — "use the
interface's current channel". Hardcoding a number here works until the router
next moves channel, then fails silently.

**The link starts lazily.** `espnow_link_init()` reads the stored peer from NVS
and, if there is none, returns without creating anything: no RX queue, no worker
task, no `esp_now_init()` — so not even a receive callback. It comes up on one of
two events, boot with a peer already stored, or
`espnow_link_pair_window_open()`, which is always someone pressing a button in
the web UI or on the settings screen. Most AtlasCubes will never have a remote,
and the link is ~10 KB of internal DRAM once the worker's 8 KB stack is counted —
in a firmware where the LVGL flush buffer and ESP-ADF's task stacks compete for
that same pool, this is not rounding error.

A pairing window that closes with nobody paired leaves the link up until the next
reboot. Tearing it back down means deleting a task that may be mid-frame, which
needs a shutdown sentinel through the queue and a self-deleting worker — more
machinery than a case reached only by pressing the button yourself is worth.

**Built out entirely** by commenting out `HAS_ESPNOW_REMOTE` in
[main/include/defines.h](../main/include/defines.h): the component becomes stubs,
so no caller needs an `#ifdef`. With the lazy start above there is no RAM left to
save, so this only reclaims flash — the point of it is a build that genuinely has
no ESP-NOW rather than one that has it and stays quiet. `GET /api/espnow` then
answers `supported:false` and `POST /api/espnow/pair` answers 501, because `www`
is a single bundle shared by every variant and the page has to hide its Remote
section on its own.

**The RX callback filters by MAC before the queue.** It sees every ESP-NOW frame
in the air, a neighbour's radio included; without the check each one would cost a
queue slot and a worker wake-up only to be rejected. `handle_frame()` still holds
the authoritative check — the callback only spares the trip. Frames from unknown
MACs pass solely while the pairing window is open, which is where broadcast
`pair` arrives.

### Remote side

The remote normally has no interface channel to inherit, so `channel = 0` is
**wrong on this side**: its interface sits on the default channel 1 while the
radio may be on 6 or 11, and frames vanish without an error. The remote instead:

1. stores the radio's MAC and channel in NVS at pairing (mirrored into RTC
   memory so it survives deep sleep without an NVS read),
2. calls `esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE)` explicitly before
   every transmission — **unless it is currently associated**, see below,
3. on TX-callback failure, retries twice on the stored channel, then **sweeps
   channels 1…13**, ~120 ms per channel, caching the winner,
4. refreshes the stored channel from the `ch` field that every radio reply
   carries — so a router-side channel change self-heals on the next successful
   exchange.

**When the remote is associated.** A remote that also runs a WiFi station — the
AtlasCubeController does when it is built for the WS transport, and a
differently-built remote may for reasons of its own — must skip step 2 while that
association is up.
`esp_wifi_set_channel()` on an associated interface tears the association down.
It is also unnecessary: both ends are then on the router's channel by
definition. So the send path branches on association state, and step 2 applies
only to the unassociated case that everything else here assumes.

### Frame format

```
byte 0      sequence number (0…255, wraps)
bytes 1…N   payload, ASCII, not NUL-terminated
```

Dispatch on the payload's first byte, matching the WS rule: `{` means JSON,
anything else is a plain-text command. In practice remote→radio is always plain
text and radio→remote is always JSON.

**Sequence number.** The radio remembers the last sequence it accepted per peer
and silently drops a repeat. This exists because a lost *ACK* — not a lost
frame — makes the remote retry something the radio already did, and the relative
commands (`next`, `prev`, `volp`, `volm`) are not idempotent: a retried `volp`
would step the volume twice. Replies echo the sequence of the request they
answer, so the remote can match them.

**Size.** Both ends are ESP-IDF ≥ 5.4, so ESP-NOW v2 applies:
`ESP_NOW_MAX_DATA_LEN_V2` = 1470 B. The radio still caps replies at **1400 B**
and truncates before that limit rather than fragmenting — there is no
reassembly layer, by design.

## Pairing

Encryption is deliberately off. Pairing establishes *addressing*, not secrecy.

1. The radio opens a **pairing window** (60 s) from the web UI or the on-device
   settings screen. Outside that window `pair` frames are ignored.
2. The remote's pairing menu broadcasts `pair` to `FF:FF:FF:FF:FF:FF`, sweeping
   channels 1…13 and waiting ~120 ms on each. Broadcast frames are **not**
   ACKed, so the radio's reply is the only confirmation.
3. The radio replies with `t:"pair"` and stores the remote's MAC (taken from
   `esp_now_recv_info_t->src_addr`) in NVS.
4. The remote stores the radio's MAC and channel.

From then on the radio accepts command frames **only from the stored MAC**.

> This is not a security boundary. MAC addresses are trivially spoofable and the
> payload is plaintext in the air. It is the same trust model the WS transport
> already states: the device trusts its own environment. It stops a neighbour's
> AtlasCube from driving your radio; it does not stop an attacker.

Country code must be set (`PL`/EU) before sweeping. Under IDF's default `"01"`
world-safe policy channels 12–13 are receive-only and transmissions there are
dropped.

## Remote → radio

### Shared vocabulary

Every plain-text command in
[ws_protocol.md § Client → device](ws_protocol.md#client--device-plain-text-commands)
is valid here verbatim and goes through the same dispatcher: `play`, `stop`,
`toggle`, `next`, `prev`, `volp`, `volm`, `vol=N`, `playstation=N`,
`source=radio|sd|bt`.

**Prefer `vol=N` over `volp`/`volm`.** The remote knows the current volume from
the last state reply, so it can send an absolute value. Absolute commands
survive a duplicate delivery unharmed; relative ones lean on the sequence-number
de-duplication.

### Remote-only commands

The playback ones have no WS equivalent because WS clients get everything pushed.
The settings and diagnostics ones do — `GET`/`POST /api/settings` and
`GET /api/diag` — but those are REST, and a remote that reached for them would
have to associate. Same data, a transport that does not cost a DHCP lease.

| Frame | Reply | Notes |
|---|---|---|
| `get_state` | `t:"state"` | One snapshot. The pull-model replacement for the WS state broadcast. |
| `get_list=OFF,CNT` | `t:"list"` | One page of playlist names starting at 0-based `OFF`. `CNT` is clamped to what fits in a frame. |
| `get_sd=OFF,CNT` | `t:"sd"` | One page of the **current SD folder**, same paging rules. |
| `sd_open=N` | — | Descend into entry `N`. Ignored unless `N` is a folder. |
| `sd_up` | — | One level up; no-op at the browse root. |
| `sd_play_index=N` | — | Play entry `N`. Ignored unless `N` is a track. |
| `get_cfg` | `t:"cfg"` | The settings a remote may edit, all of them, in one reply. |
| `set_brightness=N` | — | Panel brightness, 0…100. Out of range is **ignored, not clamped** — as with `vol=N`. |
| `set_eq_enabled=0\|1` | — | Equaliser on/off. |
| `set_eq_10=G0,…,G9` | — | Ten band gains in dB. **Ten or none** — a short list is ignored, not zero-padded. Same name and same rule as the WS `set_eq_10`, different encoding. |
| `get_diag` | `t:"diag"` | Health snapshot, read-only. |
| `ping` | `t:"pong"` | Link and channel check; cheapest way to confirm the stored channel is still right. Also the remote's only way to read its own signal strength — the reply carries the RSSI the radio measured. |
| `pair` | `t:"pair"` | Broadcast only, honoured only inside the pairing window. |

### Why the settings are three commands and not one patch

`POST /api/settings` takes a partial JSON patch, and mirroring that here was the
obvious design. It is not the one implemented, for a reason worth writing down:
that handler is 400 lines whose sections trail side effects — `flip`, `invert`
and `bgr` post `UI_EVT_BG_CHANGED`, a background change calls
`net_wallpaper_dismiss()`, the NTP section reconfigures the time service and
re-applies the dim schedule. Feeding a remote's patch into it means lifting all of
that into a component both transports can reach: a refactor of code every radio
runs, for a peripheral most of them will never have.

The three keys above are the ones with no such tail — bare setters, nothing to
keep in step. **That is also the limit.** A remote that wants `display.flip`,
`display.theme` or anything under `wallpaper`/`ntp` is asking for a setting whose
meaning lives in that handler, and the answer then is to extract
`settings_apply_patch()` first, not to add a fourth `set_*` here.

### Browsing the card by index

The remote never sees a path, exactly as it never sees a station's URL. What makes
that work is that `sd_player` already keeps a cursor — the last folder it
scanned — so "entry 3" has a meaning without the remote saying where from.

**Folders and tracks share one numbering, folders first.** `nf` in the reply says
how many of the listing's entries are folders, so an index below it is a folder
and one at or above it is a track. The same index therefore means the same thing
in `get_sd`, `sd_open` and `sd_play_index`, and neither end has to agree about
anything else.

**`sd_play_index=N` is not the WS `sd_play` command.** That one takes a folder
and plays it; this one takes an entry index in the current listing. The names are
close enough to be worth reading twice.

**The three navigation commands do not reply**, which costs an extra exchange on
every descent and is the price of correctness. They are not idempotent — a
retried `sd_open` would descend twice — so they have to sit on the *mutating*
side of the sequence-number dedup, where a duplicate is dropped. A command the
dedup may drop cannot be the one that carries the new listing back, so the remote
reads it with `get_sd` afterwards. Descending costs ~300 ms and survives a lost
frame.

## Radio → remote

JSON, short keys, sized for one frame. All string fields are truncated, never
fragmented.

### `t:"state"`

```json
{"t":"state","src":"radio","st":1,"vol":45,"idx":3,
 "stn":"TOK FM","ttl":"Poranek Radia TOK FM","ch":6}
```

| Key | Meaning |
|---|---|
| `src` | active source: `radio` \| `sd` \| `bt` (from `media_source_current()`) |
| `st` | 1 = playing, 0 = stopped |
| `vol` | 0…100 |
| `idx` | 0-based playlist index, `-1` when not applicable |
| `stn` | first screen line (station / album), truncated to 48 chars |
| `ttl` | second screen line (title), truncated to 48 chars |
| `ch` | the radio's current WiFi channel — see the self-healing rule above |

This is a deliberately trimmed subset of the WS `type:"state"` snapshot, which
does not fit in an ESP-NOW frame and carries fields a remote has no screen for.

### `t:"list"`

```json
{"t":"list","off":0,"total":23,
 "e":["AntyRadio ssl","AntyRadio","Play 90s","PARANORMALIUM"]}
```

Names only — **URLs never cross this link**. The remote plays by index
(`playstation=N`), and the radio resolves the URL locally from
`playlist_get(index)`. Names are truncated to 32 chars.

`off` echoes the requested offset, `total` is `playlist_get_count()`, and `e` is
however many entries fit. A short `e` is not an error: the remote advances `off`
by `e`'s length and asks again until `off + len(e) == total`.

### `t:"sd"`

```json
{"t":"sd","card":1,"dir":"rock","up":1,"off":0,"total":23,"nf":2,
 "e":["live","studio","01 - Intro.mp3","02 - ..."]}
```

| Key | Meaning |
|---|---|
| `card` | 1 when the card is mounted. 0 means there is nothing to browse — distinct from an empty folder, which the remote has to word differently |
| `dir` | the current folder's **name**, not its path; `""` at the browse root |
| `up` | 1 when there is a level to go up to |
| `off`, `total`, `e` | as in `t:"list"`; `total` counts folders *and* tracks |
| `nf` | how many entries of the whole listing are folders — they come first |

Names are truncated to 32 characters like the playlist's. File names run longer
than station names, but a remote row shows fewer characters than that anyway.

The first `get_sd` of a boot mounts the card and scans the root, so it can take
far longer than the 300 ms a remote waits for a reply. `get_sd` is a query and is
answered on a retry, so this surfaces as one slow first look rather than a
failure — but it is why a remote should not treat a single unanswered `get_sd` as
"no card".

> **The browse cursor is shared.** `sd_player`'s listing buffers are the same
> ones the web UI's `sd_list` scans into, so a browser session moves the folder
> under a remote that is looking at it. Two consequences, and only the first is
> handled here: a remote **must** compare the `dir` of each page against the one
> its list was built for and reload from scratch when they differ, rather than
> splicing pages from two folders together. The second has no fix on this side —
> between a remote's last page and its `sd_play_index`, the cursor can move and
> the wrong track plays. The window is seconds and needs someone browsing the
> card from two places at once; if that stops being hypothetical, the answer is a
> folder token on the command rather than a second scan context.

### `t:"cfg"`

```json
{"t":"cfg","br":75,"eqen":true,"eq":[0,0,2,0,0,0,0,1,3,3],"ch":6}
```

| Key | Meaning |
|---|---|
| `br` | `display.brightness`, 0…100 |
| `eqen` | `audio.eq_enabled` |
| `eq` | `audio.eq` — ten band gains in dB, always all ten |

Everything the remote can write, in one reply, with no way to ask for a subset.
There is no path-addressed read here and that is not an oversight: walking paths
needs the settings tree as cJSON, and the only code that builds that tree is the
`/api/settings` GET handler, by hand. Three keys did not justify extracting it.

### `t:"diag"`

```json
{"t":"diag","ver":"0.45.0","upd":false,"new":"","up":91240,"t10":452,
 "heap":118400,"hmin":96112,"ssid":"dom","rssi":-58,"sd":1,"sdfree":29123,
 "cpu0":12,"cpu1":8,"ch":6}
```

| Key | Meaning |
|---|---|
| `ver` | running firmware, `git describe` |
| `upd`, `new` | a newer release was seen, and its tag (`""` when none) |
| `up` | uptime, seconds |
| `t10` | die temperature in **tenths** of °C — `452` is 45.2. **Absent** when the sensor never installed; there is no sentinel to test for |
| `heap`, `hmin` | internal heap free now, and the low-water mark since boot, bytes |
| `ssid`, `rssi` | the radio's association; `rssi` is 0 when unassociated |
| `sd`, `sdfree` | card mounted, and its free space in **MB** (not bytes) |
| `cpu0`, `cpu1` | per-core load %, **since this link's previous `get_diag`** |

A trimmed subset of `GET /api/diag`. What is missing — IDF version, panel
geometry, flash size, www staleness, the MQTT group — is bug-report material read
from a browser, and the full snapshot is ~800 B of mostly variable-length strings,
which is a poor bet against a cap that truncates rather than fragments.

**`cpu0`/`cpu1` are deltas and the link keeps its own baseline.** `diag.h` requires
one `diag_cpu_state_t` per caller; sharing the web handler's would have the remote
and the browser consuming each other's interval, and both would read near zero.
The first `get_diag` after boot therefore reports `0,0` — it only sets the mark.

### `t:"pong"` / `t:"pair"`

```json
{"t":"pong","ch":6,"ver":"0.45.0","rssi":-58}
{"t":"pair","ch":6,"name":"AtlasCube Radio","mac":"a0:b1:c2:d3:e4:f5"}
```

`rssi` is the strength of *that ping frame* as the radio's WiFi driver saw it,
in dBm. It is here because a remote cannot measure how well its own transmissions
arrive — there is no TX-side RSSI — so this is the only number it can turn into a
signal-bar indicator. It describes the remote→radio direction; the reverse is not
strictly symmetric, but at these power levels it is the useful approximation.

## Watching the link from the radio

The radio counts activity so the web UI can show whether the remote is actually
talking to it: age of the last accepted frame, that frame's RSSI, frames accepted
since boot, replies the remote's MAC did not ACK (`esp_now_register_send_cb`), and
the last command. `espnow_link_get_status()` returns all of it; `GET /api/espnow`
publishes it next to the pairing state.

**There is no "connected" flag, and adding one would be a lie.** ESP-NOW is
connectionless and the remote sleeps between button presses — six hours of silence
is a healthy remote on battery. So the UI shows the *age* of the last contact and
lets the reader judge. A real online/offline indicator needs a keepalive in the
contract (the remote pings every N minutes while awake, the radio allows ~3N before
going grey); that is a change to both ends and has not been made.

## Conventions and gotchas

- **The RX callback runs in the WiFi task.** It must only copy the frame into a
  queue. Calling `media_control_execute()` or anything touching SPIFFS from
  there will deadlock or overflow that stack.
- **`playstation=N` is 0-based**, same as everywhere else in the protocol.
- **Sleep tiering.** Deep sleep costs ~300–400 ms before the first frame can go
  out (bootloader dominates); light sleep wakes in under a millisecond. The
  remote uses light sleep for a few minutes after the last press and deep sleep
  after that, so the only slow press is the one where the screen is lighting up
  anyway.
- **Wake source.** Whatever wakes the remote must sit in the chip's RTC/LP domain,
  and that domain is far smaller than the full pin count — check
  `SOC_RTCIO_PIN_COUNT` for the target rather than assuming. It is **GPIO0–21 on
  the ESP32-S3 but only GPIO0–7 on the ESP32-C6**, which is what the
  AtlasCubeController runs; on that board the eight LP pins are nearly all spoken
  for by the panel and the digitizer. The controller wakes on its CST816D touch
  INT line, so a press on the screen is the wake event and no separate key
  hardware is involved. A remote built around a key matrix instead would hold rows
  low through `gpio_deep_sleep_hold_en()`, pull the columns up, and take `ext1`
  wake on the columns — the C6 supports a per-pin trigger level
  (`SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN`), the S3 only one mode for all.
- **MQTT is not on this path.** [mqtt_svc.c](../components/mqtt_svc/mqtt_svc.c)
  encodes the same vocabulary as topic suffixes with separate payloads, and
  clamps where the plain-text form ignores (`volume`, `station`). Folding it
  onto the shared dispatcher would change MQTT's behaviour, so it is left alone
  — a separate decision, not part of this link.
- **Changing the shared vocabulary touches both files.** Add a command to
  `ws_protocol.md`'s table and it works here for free; the only thing that
  belongs in *this* file is framing, pairing, and the pull-model commands above.
