# ESP-NOW link — hardware pilot

The second control transport. Carries the **same command vocabulary** as the
WebSocket channel, over a different link.

| | |
|---|---|
| Peer | AtlasCubeController (hardware pilot), `D:\Projekty\VSCode\ESP32\AtlasCubeController` |
| Vocabulary | defined in [ws_protocol.md](ws_protocol.md) — **that file stays the source of truth** |
| This file | the link layer only: pairing, channel, framing, retries, and the pilot-only commands the WS transport does not need |

**Layering rule:** the link owns framing (sequence byte, page splitting,
retries). It never owns meaning. A pilot→radio command frame is stripped of its
framing and handed to the *same* `media_command_execute_text()` the WS server
calls. If a second interpretation of `next` ever appears in the ESP-NOW handler,
the two transports have already diverged — the whole point of this split is that
adding a command to the table in `ws_protocol.md` lights it up on both links at
once.

## Why this transport exists

The pilot is battery-powered and sleeps. That single fact drives every
difference from the WS contract:

- **It never associates with the AP.** No DHCP, no 2–5 s connect on every
  button press. It transmits raw ESP-NOW frames on a stored channel.
- **It cannot receive pushes.** The WS model ("device pushes full state on
  every change, there is no *give me the state* command") is unusable — the
  pilot is asleep when the state changes. This link is **pull**: the pilot asks,
  the radio answers, the pilot sleeps.
- **Every frame is acknowledged.** Unicast ESP-NOW is ACKed at the MAC layer,
  so `esp_now_send()`'s TX callback tells the pilot whether the radio heard it.
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

The radio registers the paired pilot as a peer with **`channel = 0`** — "use the
interface's current channel". Hardcoding a number here works until the router
next moves channel, then fails silently.

### Pilot side

The pilot has no interface channel to inherit, so `channel = 0` is **wrong on
this side**: its interface sits on the default channel 1 while the radio may be
on 6 or 11, and frames vanish without an error. The pilot instead:

1. stores the radio's MAC and channel in NVS at pairing (mirrored into RTC
   memory so it survives deep sleep without an NVS read),
2. calls `esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE)` explicitly before
   every transmission,
3. on TX-callback failure, retries twice on the stored channel, then **sweeps
   channels 1…13**, ~120 ms per channel, caching the winner,
4. refreshes the stored channel from the `ch` field that every radio reply
   carries — so a router-side channel change self-heals on the next successful
   exchange.

### Frame format

```
byte 0      sequence number (0…255, wraps)
bytes 1…N   payload, ASCII, not NUL-terminated
```

Dispatch on the payload's first byte, matching the WS rule: `{` means JSON,
anything else is a plain-text command. In practice pilot→radio is always plain
text and radio→pilot is always JSON.

**Sequence number.** The radio remembers the last sequence it accepted per peer
and silently drops a repeat. This exists because a lost *ACK* — not a lost
frame — makes the pilot retry something the radio already did, and the relative
commands (`next`, `prev`, `volp`, `volm`) are not idempotent: a retried `volp`
would step the volume twice. Replies echo the sequence of the request they
answer, so the pilot can match them.

**Size.** Both ends are ESP-IDF ≥ 5.4, so ESP-NOW v2 applies:
`ESP_NOW_MAX_DATA_LEN_V2` = 1490 B. The radio still caps replies at **1400 B**
and truncates before that limit rather than fragmenting — there is no
reassembly layer, by design.

## Pairing

Encryption is deliberately off. Pairing establishes *addressing*, not secrecy.

1. The radio opens a **pairing window** (60 s) from the web UI or the on-device
   settings screen. Outside that window `pair` frames are ignored.
2. The pilot's pairing menu broadcasts `pair` to `FF:FF:FF:FF:FF:FF`, sweeping
   channels 1…13 and waiting ~120 ms on each. Broadcast frames are **not**
   ACKed, so the radio's reply is the only confirmation.
3. The radio replies with `t:"pair"` and stores the pilot's MAC (taken from
   `esp_now_recv_info_t->src_addr`) in NVS.
4. The pilot stores the radio's MAC and channel.

From then on the radio accepts command frames **only from the stored MAC**.

> This is not a security boundary. MAC addresses are trivially spoofable and the
> payload is plaintext in the air. It is the same trust model the WS transport
> already states: the device trusts its own environment. It stops a neighbour's
> AtlasCube from driving your radio; it does not stop an attacker.

Country code must be set (`PL`/EU) before sweeping. Under IDF's default `"01"`
world-safe policy channels 12–13 are receive-only and transmissions there are
dropped.

## Pilot → radio

### Shared vocabulary

Every plain-text command in
[ws_protocol.md § Client → device](ws_protocol.md#client--device-plain-text-commands)
is valid here verbatim and goes through the same dispatcher: `play`, `stop`,
`toggle`, `next`, `prev`, `volp`, `volm`, `vol=N`, `playstation=N`,
`source=radio|sd|bt`.

**Prefer `vol=N` over `volp`/`volm`.** The pilot knows the current volume from
the last state reply, so it can send an absolute value. Absolute commands
survive a duplicate delivery unharmed; relative ones lean on the sequence-number
de-duplication.

### Pilot-only commands

These have no WS equivalent because WS clients get everything pushed.

| Frame | Reply | Notes |
|---|---|---|
| `get_state` | `t:"state"` | One snapshot. The pull-model replacement for the WS state broadcast. |
| `get_list=OFF,CNT` | `t:"list"` | One page of playlist names starting at 0-based `OFF`. `CNT` is clamped to what fits in a frame. |
| `ping` | `t:"pong"` | Link and channel check; cheapest way to confirm the stored channel is still right. |
| `pair` | `t:"pair"` | Broadcast only, honoured only inside the pairing window. |

## Radio → pilot

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
does not fit in an ESP-NOW frame and carries fields a pilot has no screen for.

### `t:"list"`

```json
{"t":"list","off":0,"total":23,
 "e":["AntyRadio ssl","AntyRadio","Play 90s","PARANORMALIUM"]}
```

Names only — **URLs never cross this link**. The pilot plays by index
(`playstation=N`), and the radio resolves the URL locally from
`playlist_get(index)`. Names are truncated to 32 chars.

`off` echoes the requested offset, `total` is `playlist_get_count()`, and `e` is
however many entries fit. A short `e` is not an error: the pilot advances `off`
by `e`'s length and asks again until `off + len(e) == total`.

### `t:"pong"` / `t:"pair"`

```json
{"t":"pong","ch":6,"ver":"0.45.0"}
{"t":"pair","ch":6,"name":"AtlasCube Radio","mac":"a0:b1:c2:d3:e4:f5"}
```

## Conventions and gotchas

- **The RX callback runs in the WiFi task.** It must only copy the frame into a
  queue. Calling `media_control_execute()` or anything touching SPIFFS from
  there will deadlock or overflow that stack.
- **`playstation=N` is 0-based**, same as everywhere else in the protocol.
- **Sleep tiering.** Deep sleep costs ~300–400 ms before the first frame can go
  out (bootloader dominates); light sleep wakes in under a millisecond. The
  pilot uses light sleep for a few minutes after the last press and deep sleep
  after that, so the only slow press is the one where the screen is lighting up
  anyway.
- **Keyboard wake.** 3×4 matrix: rows held low through
  `gpio_deep_sleep_hold_en()`, columns pulled up, `ext1` wake in `ANY_LOW` mode
  on the four column pins (ESP32-S3 supports `ANY_LOW`; the original ESP32 did
  not). The matrix is scanned normally after wake to identify the key. All pins
  must be in the RTC domain, GPIO0–21.
- **MQTT is not on this path.** [mqtt_svc.c](../components/mqtt_svc/mqtt_svc.c)
  encodes the same vocabulary as topic suffixes with separate payloads, and
  clamps where the plain-text form ignores (`volume`, `station`). Folding it
  onto the shared dispatcher would change MQTT's behaviour, so it is left alone
  — a separate decision, not part of this link.
- **Changing the shared vocabulary touches both files.** Add a command to
  `ws_protocol.md`'s table and it works here for free; the only thing that
  belongs in *this* file is framing, pairing, and the pull-model commands above.
