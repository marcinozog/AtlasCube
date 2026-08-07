# Wallpapers — how to put one on the screen

A user guide. For the data model behind it (profile fields, PSRAM slots,
fetch scheduling) see [`layout_editor.md`](layout_editor.md#per-screen-wallpapers).

Everything happens in the **Layout editor** (`layout.html`, linked from the
web UI toolbar). Wallpapers are per screen: Home, SD Player, SD Browser,
Radio, Playlist, Bluetooth and Equalizer each carry their own background,
so pick the screen's tab first — whatever you do next applies to that tab.

## The one thing to understand

An internet wallpaper takes **two** steps, and doing only the first is the
usual reason "nothing happened":

1. **Fill a slot** — a slot (there are ten) gets an image URL, and the
   device downloads and decodes the picture into RAM.
2. **Point a screen at that slot** — the screen's background source becomes
   *Internet · slot N*.

The **🌍 Internet** tab does step 1 only. The **Online gallery** does both
in one click. That is the whole difference between the two routes below.

## Route A — Online gallery (no SD card needed)

The fastest path, and the only one that works on a device with no SD card.

1. Open the **Layout editor** and select the tab of the screen you want to
   change (e.g. **🏠 Home**).
2. Under the preview, click **Online gallery**. It lists the wallpapers
   published for your panel resolution and for that screen.
3. At the top of the gallery, set **Use in slot:** to a slot number
   (`slot 1` if you have no reason to pick another). Anything already in
   that slot is replaced.
4. Click **Use in slot** on the card you like.

The device downloads the image itself (the radio goes quiet for those few
seconds), and the screen is pinned to that slot right away — no browser
conversion, no SD card, nothing to copy.

**Install to SD** on the same card is the other route: the browser converts
the image to a panel-sized `.bin` and stores it on the SD card. Use it when
you want the wallpaper to survive without a network, or for a card the slot
route refuses.

## Route B — your own URL (Internet tab)

For a picture that is not in the gallery, or for a service that returns a
new photo every day.

1. **Layout editor → 🌍 Internet**.
2. Pick a **Slot**. Everything below the selector edits *that* slot.
3. Choose a preset from the dropdown (NASA APOD, Lorem Picsum, LoremFlickr,
   Cataas, PlaceDog) or **Custom URL…** and type your own. `{w}` and `{h}`
   in a URL are replaced with your panel's width and height, and NASA APOD
   API links are resolved to the actual image automatically.
4. Click **🌍 Fetch now**. A thumbnail appears when the device has the
   picture — that is your confirmation the slot is filled.
5. Set **Auto refresh** (see below).
6. **Go back to the screen's tab** and, under the preview, click
   **Internet** with the slot dropdown next to it set to the same slot.
   Only now does the screen show it.

**Save to SD** stores what the device fetched as a `.bin` on the card, which
turns a one-off download into a permanent wallpaper.

## Auto refresh — why a slot comes back empty

Fetched wallpapers live in RAM only. They are gone after a reboot, and are
restored only if the device is told to fetch them again:

| Auto refresh | What happens after a reboot |
|---|---|
| **Off (manual only)** | slots stay empty until you press *Fetch now* |
| **Once after boot** | every slot with a URL is re-downloaded ~30 s after boot |
| **After boot + daily at hh:mm** | as above, plus a fresh download each day |

The setting is **shared by all slots**. With it off, the web UI shows a
warning strip next to every slot control, with a one-click
**Fetch after every boot** button — that is the fix.

All slots are refreshed in one batch, and the device shows a pill
("Updating wallpapers 2/5") while it runs. The radio starts after the batch
finishes rather than being cut off mid-song.

## Route C — a file from the SD card

1. Select the screen's tab.
2. **⬆ Upload…** converts an image in the browser and assigns it to this
   screen in one go, or **Choose from SD…** picks a `.bin` already on the
   card.

Uploads are filed by panel resolution and screen category:

```
/wallpapers/<width>x<height>/<category>/<name>.bin
```

`home`, `radio-sd-player` (Radio and SD Player share it),
`playlist-sd-browser` (both list screens share it), `wireless`, `equalizer`.

## Which background wins

Per screen, in order:

1. an **SD file** assigned to that screen — always wins,
2. **Internet · slot N** — that slot's fetched image,
3. **General** — the gradient/solid background from the **🎨 General** tab
   (replaced by slot 1 while one is fetched).

So a screen that still shows an old picture after you filled a slot is
almost certainly set to an SD file. Click **Internet** on that screen.

## Costs and limits

- One filled slot costs one panel-sized image in RAM. Empty slots cost
  nothing, so ten configured slots are only expensive once they all hold a
  picture.
- The device decodes **JPEG only**. The gallery refuses PNG/WebP cards for
  the slot route up front — use *Install to SD* for those.
- Each download pauses a playing radio stream for a few seconds.
- Ten slots exist so different screens can show different pictures; there
  is no need to use more than one.

## Troubleshooting

| Symptom | Cause |
|---|---|
| Slot fetched, screen unchanged | screen is still on *General* or an SD file — press **Internet** on that screen's tab (Route B, step 6) |
| Wallpaper gone after a power cycle | **Auto refresh** is off — set *Once after boot*, or *Save to SD* |
| "the device decodes JPEG only" | that card is a PNG/WebP — use **Install to SD** |
| "device is busy" | another fetch is still running; wait for it and retry |
| Gallery empty for your panel | no wallpapers published for that resolution yet |
| Picture stretched or cropped oddly | the source aspect ratio differs from the panel; it is scaled to fill |
