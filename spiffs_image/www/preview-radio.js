'use strict';

// SCREEN_RADIO — mirrors screen_radio.c.
//
// Children are appended in that file's creation order, so anything that overlaps
// stacks here the way it stacks on the panel.

// ── Station icon — station_icon_widget.c ────────────────────────────────────
//
// The artwork is not part of the profile: it hangs off the playlist entry of the
// station currently playing (playlist_get(curr_index)->icon_path), as a path on
// the SD card holding an LVGL .bin.
function currentIconPath() {
    const e = S.playlist[S.live.curr_index | 0];
    const rel = e && typeof e.icon === 'string' ? e.icon : '';
    // Same rejection the widget applies before it even touches the card.
    if (!rel || rel[0] !== '/' || rel.includes('..')) return '';
    return rel;
}

// Reload only on a real change of path. The device skips an unchanged path
// (strcmp against s_path) and so must this: a state broadcast arrives on every
// volume tick, and refetching the .bin each time would pound the SD card.
let iconLoadedPath = null;

async function refreshStationIcon() {
    const el = S.els.stationIcon;
    if (!el) return;
    const rel = currentIconPath();
    if (rel === iconLoadedPath) return;
    iconLoadedPath = rel;

    if (!rel) {                       // clear_icon(): hide, don't leave stale art
        el.style.backgroundImage = '';
        el.style.display = 'none';
        return;
    }
    try {
        const f = await fetch('/api/sd/file?path=' + encodeURIComponent(rel), { cache: 'no-store' });
        if (!f.ok) throw new Error('HTTP ' + f.status);
        const dec = window.LvBin.decodeToCanvas(await f.arrayBuffer());
        if (currentIconPath() !== rel) return;   // station changed mid-download
        // LV_IMAGE_ALIGN_STRETCH into a size x size object: the .bin is loaded at
        // up to 64x64 and then stretched to the configured box, aspect ignored.
        el.style.backgroundImage = `url("${dec.canvas.toDataURL('image/png')}")`;
        el.style.backgroundSize  = '100% 100%';
        el.style.display = '';
    } catch (err) {
        // The widget logs "Cannot load %s" and simply shows nothing.
        console.warn('Station icon unavailable:', rel, err.message);
        el.style.backgroundImage = '';
        el.style.display = 'none';
        iconLoadedPath = null;        // retry if the station comes round again
    }
}

// ── Screen ──────────────────────────────────────────────────────────────────

async function renderRadioScreen() {
    const p    = S.prof.radio;
    const th   = S.pal;
    const opa  = clamp(p.radio_label_bg_opa ?? 50, 0, 100);
    const frag = document.createDocumentFragment();

    renderSharedStubs(frag, p, 'radio');   // wheels first, as radio_create() does

    // now_playing_widget: two independent boxed lines, each centre-anchored on
    // its own box (make_line: x + w / 2).
    if (p.radio_show_np) {
        const stationW = Math.max(p.radio_np_w | 0, 8);
        S.els.station = makeLabel({
            x: (p.radio_np_x | 0) + stationW / 2, y: p.radio_np_y | 0,
            fontId: p.radio_np_station_font, text: '', boxW: stationW, plate: opa,
            color: col(p.radio_np_color, th.accent),
        });
        frag.appendChild(S.els.station);

        if (p.radio_show_np_title) {
            const titleW = Math.max(p.radio_title_w | 0, 8);
            S.els.title = makeLabel({
                x: (p.radio_title_x | 0) + titleW / 2, y: p.radio_title_y | 0,
                fontId: p.radio_np_title_font, text: '', boxW: titleW, plate: opa,
                color: col(p.radio_title_color, th.text_secondary),
            });
            frag.appendChild(S.els.title);
        }
    }

    if (p.radio_show_station_icon) {
        // station_icon_widget_create() clamps the box to 16..64 px, starts hidden.
        const size = clamp(p.radio_station_icon_size | 0, 16, 64);
        S.els.stationIcon = box(p.radio_station_icon_x | 0, p.radio_station_icon_y | 0,
                                size, size, { display: 'none' });
        iconLoadedPath = null;
        frag.appendChild(S.els.stationIcon);
    }

    renderClockWidget(frag, p, 'radio');

    // weather_widget centres an icon + text pill inside a full-width frame. The
    // pill is drawn, the icon is not (its glyphs live in lv_font_weather_20), and
    // the reading is sample text — the device does not serve current conditions.
    if (p.radio_show_weather) {
        const wSpan = (p.radio_weather_w | 0) > 0 ? (p.radio_weather_w | 0) : S.meta.screen_w;
        frag.appendChild(makeLabel({
            x: (p.radio_weather_x | 0) + wSpan / 2, y: p.radio_weather_y | 0,
            fontId: p.radio_weather_font, text: '+21°C  54%', plate: opa,
            color: th.text_primary,
        }));
    }

    if (p.radio_show_playback_status) {
        S.els.state = makeLabel({
            x: p.radio_state_x | 0, y: p.radio_state_y | 0,
            fontId: p.radio_state_font, text: '', plate: opa,
            color: col(p.radio_state_color, th.status_ok),
        });
        frag.appendChild(S.els.state);
    }

    // make_info_label(): four independent centre-anchored labels sharing one font
    // and one colour override.
    const infoColor = col(p.radio_info_color, th.text_muted);
    const info = (key, xf, yf) => {
        S.els[key] = makeLabel({
            x: p[xf] | 0, y: p[yf] | 0, fontId: p.radio_audio_info_font,
            text: '', plate: opa, color: infoColor,
        });
        frag.appendChild(S.els[key]);
    };
    if (p.radio_samplerate_show) info('samplerate', 'radio_samplerate_x', 'radio_samplerate_y');
    if (p.radio_channels_show)   info('channels',   'radio_channels_x',   'radio_channels_y');
    if (p.radio_bitrate_show)    info('bitrate',    'radio_bitrate_x',    'radio_bitrate_y');
    if (p.radio_volume_show)     info('volume',     'radio_volume_x',     'radio_volume_y');

    // Created after everything else and before the slider, exactly as
    // radio_create() orders it — a hotspot over the slider must not shadow it.
    renderHotspots(frag, p, 'radio');

    screenEl.replaceChildren(frag);

    // The tap-to-show controls overlay is intentionally absent: it only exists once
    // the user touches the screen, so drawing it at rest would misrepresent the panel.

    await renderVolSlider(screenEl, p, 'radio');
    refreshLive();
}

// ── Live ────────────────────────────────────────────────────────────────────

// radio_state_str(): FINISHED shows as STOPPED, everything else is the state
// spelled in capitals.
function stateText(s) {
    const map = {
        stopped: 'STOPPED', playing: 'PLAYING', buffering: 'BUFFERING',
        error: 'ERROR', finished: 'STOPPED',
    };
    return map[s] || '?';
}

// refresh_from_state(): the metadata labels stay empty (and therefore hidden)
// until the decoder reports a valid stream; volume is always meaningful.
function refreshRadioLive() {
    const L = S.live;

    setLabelText(S.els.station, L.station_name || 'Atlas Radio');
    // The shared title field also carries the SD track, so radio only shows it
    // while radio is the active source.
    if (S.els.title) setLabelText(S.els.title, (!L.sd_active && L.title) ? L.title : '');

    if (S.els.state) setLabelText(S.els.state, stateText(L.radio));

    const sr = L.sr | 0, br = L.br | 0;
    if (S.els.samplerate) setLabelText(S.els.samplerate, sr > 0 ? `${sr} Hz` : '');
    if (S.els.channels)   setLabelText(S.els.channels,
        sr <= 0 ? '' : ((L.ch | 0) === 1 ? 'MONO' : 'STEREO'));
    if (S.els.bitrate)    setLabelText(S.els.bitrate,
        (sr > 0 && br > 0) ? `${Math.round(br / 1000)} kbps` : '');
    if (S.els.volume)     setLabelText(S.els.volume, `VOL: ${L.volume | 0}%`);

    refreshStationIcon();   // async; no-op unless the station actually changed
}
