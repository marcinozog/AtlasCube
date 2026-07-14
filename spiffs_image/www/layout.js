// ════════════════════════════════════════════════════════════════════════════
//   AtlasCube — Layout Editor
//
//   All coordinates of editable elements are absolute relative to the LCD
//   screen (top-left origin). Each "free element" with w/h fields is movable
//   AND resizable (4 corner handles). Elements with only x/y (e.g. labels)
//   are positionable only — their size is determined by font metrics.
//
//   Sections (tabs): clock (= Home screen), radio, bt, sd — each has its own
//   field schema, renderer, and JSON state. Selecting a tab switches what the
//   form edits and what the SVG draws. The "clock" key is historical: it holds
//   the Home screen's fields (clock face + strip + indicators + calendar).
// ════════════════════════════════════════════════════════════════════════════

const SVG_NS = 'http://www.w3.org/2000/svg';

// ── Field schemas — order is purely UI grouping; doesn't affect backend ────

const CLOCK_FIELDS = [
    { key: 'clock_panel_x', label: 'Panel X',  type: 'number' },
    { key: 'clock_panel_y', label: 'Panel Y',  type: 'number' },
    { key: 'clock_panel_w', label: 'Panel W',  type: 'number' },
    { key: 'clock_panel_h', label: 'Panel H',  type: 'number' },

    { key: 'clock_show_time', label: 'Show time',     type: 'bool' },
    { key: 'clock_time_x',    label: 'Time X',        type: 'number' },
    { key: 'clock_time_y',    label: 'Time Y',        type: 'number' },
    { key: 'clock_time_font', label: 'Time font',     type: 'font'   },

    { key: 'clock_show_date', label: 'Show date',     type: 'bool' },
    { key: 'clock_date_x',    label: 'Date X',        type: 'number' },
    { key: 'clock_date_y',    label: 'Date Y',        type: 'number' },
    { key: 'clock_date_font', label: 'Date font',     type: 'font'   },

    { key: 'clock_show_netinfo', label: 'Show IP/host', type: 'bool'   },
    { key: 'clock_netinfo_x',    label: 'IP/host X',    type: 'number' },
    { key: 'clock_netinfo_y',    label: 'IP/host Y',    type: 'number' },
    { key: 'clock_netinfo_font', label: 'IP/host font', type: 'font'   },

    { key: 'clock_show_strip', label: 'Show strip',   type: 'bool' },
    { key: 'clock_strip_x',    label: 'Strip X',      type: 'number' },
    { key: 'clock_strip_y',    label: 'Strip Y',      type: 'number' },
    { key: 'clock_strip_w',    label: 'Strip W',      type: 'number' },
    { key: 'clock_strip_h',    label: 'Strip H',      type: 'number' },
    { key: 'clock_strip_bg_opa', label: 'Strip BG opacity %', type: 'number', min: 0, max: 100, default: 100 },
    { key: 'clock_strip_label_w',         label: 'Strip label W',  type: 'number' },
    { key: 'clock_strip_station_y',       label: 'Station Y',      type: 'number' },
    { key: 'clock_strip_title_y',         label: 'Title Y',        type: 'number' },
    { key: 'clock_strip_station_font',    label: 'Station font',   type: 'font'   },
    { key: 'clock_strip_title_font',      label: 'Title font',     type: 'font'   },

    { key: 'clock_show_mode_indicator',  label: 'Show mode indic.',  type: 'bool' },
    { key: 'clock_mode_indic_x',         label: 'Mode indic. X',     type: 'number' },
    { key: 'clock_mode_indic_y',         label: 'Mode indic. Y',     type: 'number' },
    { key: 'clock_show_event_indicator', label: 'Show event indic.', type: 'bool' },
    { key: 'clock_event_indic_x',        label: 'Event indic. X',    type: 'number' },
    { key: 'clock_event_indic_y',        label: 'Event indic. Y',    type: 'number' },

    { key: 'clock_show_calendar', label: 'Show calendar',  type: 'bool' },
    { key: 'clock_calendar_x',    label: 'Calendar X',     type: 'number' },
    { key: 'clock_calendar_y',    label: 'Calendar Y',     type: 'number' },
    { key: 'clock_calendar_w',    label: 'Calendar W',     type: 'number' },
    { key: 'clock_calendar_font', label: 'Calendar font',  type: 'font'   },
    { key: 'clock_show_weather', label: 'Show weather', type: 'bool' },
    { key: 'clock_weather_x', label: 'Weather X', type: 'number' },
    { key: 'clock_weather_y', label: 'Weather Y', type: 'number' },
    { key: 'clock_weather_w', label: 'Weather W', type: 'number' },
    { key: 'clock_weather_font', label: 'Weather font', type: 'font' },
];

const BT_FIELDS = [
    { key: 'bt_show_circle', label: 'Show circle',     type: 'bool' },
    { key: 'bt_circle_x',    label: 'Circle X',        type: 'number' },
    { key: 'bt_circle_y',    label: 'Circle Y',        type: 'number' },
    { key: 'bt_circle_w',    label: 'Circle W',        type: 'number' },
    { key: 'bt_circle_h',    label: 'Circle H',        type: 'number' },
    { key: 'bt_icon_font',   label: 'Icon font',       type: 'font'   },

    { key: 'bt_brand_x',     label: 'Brand X',         type: 'number' },
    { key: 'bt_brand_y',     label: 'Brand Y',         type: 'number' },
    { key: 'bt_brand_font',  label: 'Brand font',      type: 'font'   },

    { key: 'bt_status_x',    label: 'Status X',        type: 'number' },
    { key: 'bt_status_y',    label: 'Status Y',        type: 'number' },
    { key: 'bt_status_font', label: 'Status font',     type: 'font'   },

    { key: 'bt_title_x',     label: 'Title X',         type: 'number' },
    { key: 'bt_title_y',     label: 'Title Y',         type: 'number' },
    { key: 'bt_title_w',     label: 'Title W',         type: 'number' },
    { key: 'bt_title_font',  label: 'Title font',      type: 'font'   },

    { key: 'bt_artist_x',    label: 'Artist X',        type: 'number' },
    { key: 'bt_artist_y',    label: 'Artist Y',        type: 'number' },
    { key: 'bt_artist_w',    label: 'Artist W',        type: 'number' },
    { key: 'bt_artist_font', label: 'Artist font',     type: 'font'   },

    { key: 'bt_time_x',      label: 'Time X',          type: 'number' },
    { key: 'bt_time_y',      label: 'Time Y',          type: 'number' },
    { key: 'bt_time_font',   label: 'Time font',       type: 'font'   },

    { key: 'bt_vol_label_font',  label: 'Vol label font', type: 'font'   },

    { key: 'bt_show_mode_indicator', label: 'Show mode indic.', type: 'bool' },
    { key: 'bt_mode_indic_x',        label: 'Mode indic. X',    type: 'number' },
    { key: 'bt_mode_indic_y',        label: 'Mode indic. Y',    type: 'number' },
    { key: 'bt_show_clock',          label: 'Show clock',       type: 'bool' },
    { key: 'bt_clock_widget_x',      label: 'Clock X',          type: 'number' },
    { key: 'bt_clock_widget_y',      label: 'Clock Y',          type: 'number' },
    { key: 'bt_clock_font',          label: 'Clock font',       type: 'font'   },
];

const RADIO_FIELDS = [
    { key: 'radio_show_np',           label: 'Show now-playing', type: 'bool' },
    { key: 'radio_np_x',              label: 'NP X',             type: 'number' },
    { key: 'radio_np_y',              label: 'NP Y',             type: 'number' },
    { key: 'radio_station_icon_size', label: 'Station icon size', type: 'number', min: 16, max: 64, default: 64 },
    { key: 'radio_np_station_font',   label: 'NP station font',  type: 'font'   },
    { key: 'radio_np_title_font',     label: 'NP title font',    type: 'font'   },

    { key: 'radio_show_playback_status', label: 'Show playback status', type: 'bool' },
    { key: 'radio_state_y',           label: 'State Y',          type: 'number' },
    { key: 'radio_state_font',        label: 'State font',       type: 'font'   },

    { key: 'radio_audio_info_y',      label: 'Audio info Y',     type: 'number' },
    { key: 'radio_audio_info_font',   label: 'Audio info font',  type: 'font'   },

    { key: 'radio_show_mode_indicator',  label: 'Show mode indic.',  type: 'bool' },
    { key: 'radio_mode_indic_x',         label: 'Mode indic. X',     type: 'number' },
    { key: 'radio_mode_indic_y',         label: 'Mode indic. Y',     type: 'number' },
    { key: 'radio_show_clock',           label: 'Show clock',        type: 'bool' },
    { key: 'radio_clock_widget_x',       label: 'Clock X',           type: 'number' },
    { key: 'radio_clock_widget_y',       label: 'Clock Y',           type: 'number' },
    { key: 'radio_clock_font',           label: 'Clock font',        type: 'font'   },
    { key: 'radio_show_event_indicator', label: 'Show event indic.', type: 'bool' },
    { key: 'radio_event_indic_x',        label: 'Event indic. X',    type: 'number' },
    { key: 'radio_event_indic_y',        label: 'Event indic. Y',    type: 'number' },

    { key: 'radio_show_vu',              label: 'Show VU meter',     type: 'bool' },
    { key: 'radio_vu_x',                 label: 'VU X',              type: 'number' },
    { key: 'radio_vu_y',                 label: 'VU Y',              type: 'number' },
    { key: 'radio_vu_w',                 label: 'VU W',              type: 'number' },
    { key: 'radio_vu_h',                 label: 'VU H',              type: 'number' },
    { key: 'radio_show_cassette',        label: 'Show animated wheels', type: 'bool' },
    { key: 'radio_animation_style',      label: 'Graphic', type: 'choice', default: 0,
      options: [{ value: 0, label: 'Cassette reels' }, { value: 1, label: 'Car rims' }] },
    { key: 'radio_cassette_l_x',         label: 'Left wheel X',         type: 'number' },
    { key: 'radio_cassette_l_y',         label: 'Left wheel Y',         type: 'number' },
    { key: 'radio_cassette_l_size',      label: 'Left wheel size',      type: 'number', min: 16, max: 480 },
    { key: 'radio_cassette_r_x',         label: 'Right wheel X',        type: 'number' },
    { key: 'radio_cassette_r_y',         label: 'Right wheel Y',        type: 'number' },
    { key: 'radio_cassette_r_size',      label: 'Right wheel size',     type: 'number', min: 16, max: 480 },
    { key: 'radio_show_weather', label: 'Show weather', type: 'bool' },
    { key: 'radio_weather_x', label: 'Weather X', type: 'number' },
    { key: 'radio_weather_y', label: 'Weather Y', type: 'number' },
    { key: 'radio_weather_w', label: 'Weather W', type: 'number' },
    { key: 'radio_weather_font', label: 'Weather font', type: 'font' },
];

const SD_FIELDS = [
    { key: 'sd_title_y',    label: 'Title Y',          type: 'number' },
    { key: 'sd_title_font', label: 'Title font',       type: 'font'   },

    { key: 'sd_show_folder', label: 'Show folder',     type: 'bool'   },
    { key: 'sd_folder_y',    label: 'Folder Y',        type: 'number' },
    { key: 'sd_folder_font', label: 'Folder font',     type: 'font'   },

    { key: 'sd_show_info', label: 'Show info',         type: 'bool'   },
    { key: 'sd_info_y',    label: 'Info Y',            type: 'number' },
    { key: 'sd_info_font', label: 'Info font',         type: 'font'   },

    { key: 'sd_show_bar',             label: 'Show progress bar', type: 'bool' },
    { key: 'sd_bar_w',                label: 'Bar W',             type: 'number' },
    { key: 'sd_bar_h',                label: 'Bar H',             type: 'number' },

    { key: 'sd_show_mode_indicator',  label: 'Show mode indic.',  type: 'bool' },
    { key: 'sd_mode_indic_x',         label: 'Mode indic. X',     type: 'number' },
    { key: 'sd_mode_indic_y',         label: 'Mode indic. Y',     type: 'number' },
    { key: 'sd_show_clock',           label: 'Show clock',        type: 'bool' },
    { key: 'sd_clock_widget_x',       label: 'Clock X',           type: 'number' },
    { key: 'sd_clock_widget_y',       label: 'Clock Y',           type: 'number' },
    { key: 'sd_clock_font',           label: 'Clock font',        type: 'font'   },
    { key: 'sd_show_event_indicator', label: 'Show event indic.', type: 'bool' },
    { key: 'sd_event_indic_x',        label: 'Event indic. X',    type: 'number' },
    { key: 'sd_event_indic_y',        label: 'Event indic. Y',    type: 'number' },

    { key: 'sd_show_vu',              label: 'Show VU meter',     type: 'bool' },
    { key: 'sd_vu_x',                 label: 'VU X',              type: 'number' },
    { key: 'sd_vu_y',                 label: 'VU Y',              type: 'number' },
    { key: 'sd_vu_w',                 label: 'VU W',              type: 'number' },
    { key: 'sd_vu_h',                 label: 'VU H',              type: 'number' },
    { key: 'sd_show_cassette',        label: 'Show animated wheels', type: 'bool' },
    { key: 'sd_animation_style',      label: 'Graphic', type: 'choice', default: 0,
      options: [{ value: 0, label: 'Cassette reels' }, { value: 1, label: 'Car rims' }] },
    { key: 'sd_cassette_l_x',         label: 'Left wheel X',         type: 'number' },
    { key: 'sd_cassette_l_y',         label: 'Left wheel Y',         type: 'number' },
    { key: 'sd_cassette_l_size',      label: 'Left wheel size',      type: 'number', min: 16, max: 480 },
    { key: 'sd_cassette_r_x',         label: 'Right wheel X',        type: 'number' },
    { key: 'sd_cassette_r_y',         label: 'Right wheel Y',        type: 'number' },
    { key: 'sd_cassette_r_size',      label: 'Right wheel size',     type: 'number', min: 16, max: 480 },
    { key: 'sd_show_weather', label: 'Show weather', type: 'bool' },
    { key: 'sd_weather_x', label: 'Weather X', type: 'number' },
    { key: 'sd_weather_y', label: 'Weather Y', type: 'number' },
    { key: 'sd_weather_w', label: 'Weather W', type: 'number' },
    { key: 'sd_weather_font', label: 'Weather font', type: 'font' },
];

// Form-only grouping. Field schemas above remain the API/source-of-truth; these
// groups only decide how the editor presents them. `enabledBy` keeps the Show
// switch visible while hiding the controls that have no effect when it is off.
const FORM_GROUPS = {
    clock: [
        { title: 'Panel', fields: ['clock_panel_x', 'clock_panel_y', 'clock_panel_w', 'clock_panel_h'] },
        { title: 'Time', enabledBy: 'clock_show_time', fields: ['clock_show_time', 'clock_time_x', 'clock_time_y', 'clock_time_font'] },
        { title: 'Date', enabledBy: 'clock_show_date', fields: ['clock_show_date', 'clock_date_x', 'clock_date_y', 'clock_date_font'] },
        { title: 'Network info', enabledBy: 'clock_show_netinfo', fields: ['clock_show_netinfo', 'clock_netinfo_x', 'clock_netinfo_y', 'clock_netinfo_font'] },
        { title: 'Station strip', enabledBy: 'clock_show_strip', fields: ['clock_show_strip', 'clock_strip_x', 'clock_strip_y', 'clock_strip_w', 'clock_strip_h', 'clock_strip_bg_opa', 'clock_strip_label_w', 'clock_strip_station_y', 'clock_strip_title_y', 'clock_strip_station_font', 'clock_strip_title_font'] },
        { title: 'Mode indicator', enabledBy: 'clock_show_mode_indicator', fields: ['clock_show_mode_indicator', 'clock_mode_indic_x', 'clock_mode_indic_y'] },
        { title: 'Event indicator', enabledBy: 'clock_show_event_indicator', fields: ['clock_show_event_indicator', 'clock_event_indic_x', 'clock_event_indic_y'] },
        { title: 'Calendar', enabledBy: 'clock_show_calendar', fields: ['clock_show_calendar', 'clock_calendar_x', 'clock_calendar_y', 'clock_calendar_w', 'clock_calendar_font'] },
        { title: 'Weather', enabledBy: 'clock_show_weather', fields: ['clock_show_weather', 'clock_weather_x', 'clock_weather_y', 'clock_weather_w', 'clock_weather_font'] },
    ],
    bt: [
        { title: 'Bluetooth mark', enabledBy: 'bt_show_circle', fields: ['bt_show_circle', 'bt_circle_x', 'bt_circle_y', 'bt_circle_w', 'bt_circle_h', 'bt_icon_font'] },
        { title: 'Device status', fields: ['bt_brand_x', 'bt_brand_y', 'bt_brand_font', 'bt_status_x', 'bt_status_y', 'bt_status_font'] },
        { title: 'Track title', fields: ['bt_title_x', 'bt_title_y', 'bt_title_w', 'bt_title_font'] },
        { title: 'Artist', fields: ['bt_artist_x', 'bt_artist_y', 'bt_artist_w', 'bt_artist_font'] },
        { title: 'Playback', fields: ['bt_time_x', 'bt_time_y', 'bt_time_font', 'bt_vol_label_font'] },
        { title: 'Mode indicator', enabledBy: 'bt_show_mode_indicator', fields: ['bt_show_mode_indicator', 'bt_mode_indic_x', 'bt_mode_indic_y'] },
        { title: 'Clock', enabledBy: 'bt_show_clock', fields: ['bt_show_clock', 'bt_clock_widget_x', 'bt_clock_widget_y', 'bt_clock_font'] },
    ],
    radio: [
        { title: 'Now playing', enabledBy: 'radio_show_np', fields: ['radio_show_np', 'radio_np_x', 'radio_np_y', 'radio_station_icon_size', 'radio_np_station_font', 'radio_np_title_font'] },
        { title: 'Playback status', enabledBy: 'radio_show_playback_status', fields: ['radio_show_playback_status', 'radio_state_y', 'radio_state_font', 'radio_audio_info_y', 'radio_audio_info_font'] },
        { title: 'Mode indicator', enabledBy: 'radio_show_mode_indicator', fields: ['radio_show_mode_indicator', 'radio_mode_indic_x', 'radio_mode_indic_y'] },
        { title: 'Clock', enabledBy: 'radio_show_clock', fields: ['radio_show_clock', 'radio_clock_widget_x', 'radio_clock_widget_y', 'radio_clock_font'] },
        { title: 'Event indicator', enabledBy: 'radio_show_event_indicator', fields: ['radio_show_event_indicator', 'radio_event_indic_x', 'radio_event_indic_y'] },
        { title: 'Animated wheels', enabledBy: 'radio_show_cassette', fields: ['radio_show_cassette', 'radio_animation_style', 'radio_cassette_l_x', 'radio_cassette_l_y', 'radio_cassette_l_size', 'radio_cassette_r_x', 'radio_cassette_r_y', 'radio_cassette_r_size'] },
        { title: 'VU meter', enabledBy: 'radio_show_vu', fields: ['radio_show_vu', 'radio_vu_x', 'radio_vu_y', 'radio_vu_w', 'radio_vu_h'] },
        { title: 'Weather', enabledBy: 'radio_show_weather', fields: ['radio_show_weather', 'radio_weather_x', 'radio_weather_y', 'radio_weather_w', 'radio_weather_font'] },
    ],
    sd: [
        { title: 'Track title', fields: ['sd_title_y', 'sd_title_font'] },
        { title: 'Folder', enabledBy: 'sd_show_folder', fields: ['sd_show_folder', 'sd_folder_y', 'sd_folder_font'] },
        { title: 'Playback info', enabledBy: 'sd_show_info', fields: ['sd_show_info', 'sd_info_y', 'sd_info_font'] },
        { title: 'Progress bar', enabledBy: 'sd_show_bar', fields: ['sd_show_bar', 'sd_bar_w', 'sd_bar_h'] },
        { title: 'Mode indicator', enabledBy: 'sd_show_mode_indicator', fields: ['sd_show_mode_indicator', 'sd_mode_indic_x', 'sd_mode_indic_y'] },
        { title: 'Clock', enabledBy: 'sd_show_clock', fields: ['sd_show_clock', 'sd_clock_widget_x', 'sd_clock_widget_y', 'sd_clock_font'] },
        { title: 'Event indicator', enabledBy: 'sd_show_event_indicator', fields: ['sd_show_event_indicator', 'sd_event_indic_x', 'sd_event_indic_y'] },
        { title: 'Animated wheels', enabledBy: 'sd_show_cassette', fields: ['sd_show_cassette', 'sd_animation_style', 'sd_cassette_l_x', 'sd_cassette_l_y', 'sd_cassette_l_size', 'sd_cassette_r_x', 'sd_cassette_r_y', 'sd_cassette_r_size'] },
        { title: 'VU meter', enabledBy: 'sd_show_vu', fields: ['sd_show_vu', 'sd_vu_x', 'sd_vu_y', 'sd_vu_w', 'sd_vu_h'] },
        { title: 'Weather', enabledBy: 'sd_show_weather', fields: ['sd_show_weather', 'sd_weather_x', 'sd_weather_y', 'sd_weather_w', 'sd_weather_font'] },
    ],
};

// Remember expanded groups while switching screen tabs or rebuilding the form.
const openFormGroups = {};

// ── Sections registry ──────────────────────────────────────────────────────
// Each entry: { title, fields, renderer (active section's renderSvg) }

const SECTIONS = {
    clock: { title: 'Home',      fields: CLOCK_FIELDS, renderer: renderClock },
    bt:    { title: 'Bluetooth', fields: BT_FIELDS,    renderer: renderBt    },
    radio: { title: 'Radio',     fields: RADIO_FIELDS, renderer: renderRadio },
    sd:    { title: 'SD Player', fields: SD_FIELDS,    renderer: renderSd    },
};

const state = {
    meta:   { screen_w: 320, screen_h: 240, fonts: [] },
    active: 'clock',
    clock:  {},
    bt:     {},
    radio:  {},
    sd:     {},
};

// ── Bootstrap ───────────────────────────────────────────────────────────────

window.addEventListener('DOMContentLoaded', async () => {
    try {
        const meta = await fetch('/api/ui/profile/meta').then(r => r.json());
        state.meta = meta;
        document.getElementById('screen_dim').textContent =
            `${meta.screen_w} × ${meta.screen_h}`;

        // Pre-fetch every section so switching tabs is instant
        for (const name of Object.keys(SECTIONS)) {
            state[name] = await fetch(`/api/ui/profile/${name}`).then(r => r.json());
        }

        selectSection('clock');
    } catch (err) {
        setStatus('Failed to load profile: ' + err.message, true);
    }
});

function selectSection(name) {
    if (!SECTIONS[name]) return;
    state.active = name;

    document.getElementById('form_section_title').textContent = SECTIONS[name].title;
    for (const tab of document.querySelectorAll('.section-tab')) {
        tab.classList.toggle('active', tab.dataset.section === name);
    }

    buildForm();
    renderSvg();
}

// ── Form ────────────────────────────────────────────────────────────────────

function buildForm() {
    const root  = document.getElementById('form_section');
    const data  = state[state.active];
    const fields = SECTIONS[state.active].fields;
    const fieldByKey = new Map(fields.map(f => [f.key, f]));
    const groups = FORM_GROUPS[state.active];
    root.querySelectorAll('.form-group').forEach(n => n.remove());

    if (!openFormGroups[state.active]) openFormGroups[state.active] = new Set([0]);

    groups.forEach((group, groupIndex) => {
        const details = document.createElement('details');
        details.className = 'form-group';
        details.dataset.groupIndex = groupIndex;
        details.open = openFormGroups[state.active].has(groupIndex);

        const summary = document.createElement('summary');
        const title = document.createElement('span');
        title.className = 'form-group-title';
        title.textContent = group.title;
        const meta = document.createElement('span');
        meta.className = 'form-group-meta';
        summary.append(title, meta);
        details.appendChild(summary);

        const body = document.createElement('div');
        body.className = 'form-group-body';
        for (const key of group.fields) {
            const field = fieldByKey.get(key);
            if (!field) continue;
            const row = buildFormRow(field, data, group, details);
            body.appendChild(row);
        }
        details.appendChild(body);
        root.appendChild(details);

        details.addEventListener('toggle', () => {
            const opened = openFormGroups[state.active];
            if (details.open) opened.add(groupIndex);
            else              opened.delete(groupIndex);
        });
        refreshGroup(details, group, data);
    });
}

function buildFormRow(field, data, group, details) {
    const row = document.createElement('div');
    row.className = 'form-row';
    row.dataset.field = field.key;
    if (group.enabledBy && field.key !== group.enabledBy)
        row.classList.add('form-row-conditional');

    const lab = document.createElement('label');
    lab.textContent = field.label;
    lab.htmlFor = 'fld_' + field.key;
    row.appendChild(lab);

    let input;
    if (field.type === 'number') {
        input = document.createElement('input');
        input.type = 'number';
        input.value = data[field.key] ?? field.default ?? 0;
        if (field.min !== undefined) input.min = field.min;
        if (field.max !== undefined) input.max = field.max;
        input.addEventListener('input', () => {
            let value = parseInt(input.value, 10) | 0;
            if (field.min !== undefined) value = Math.max(field.min, value);
            if (field.max !== undefined) value = Math.min(field.max, value);
            input.value = value;
            data[field.key] = value;
            refreshGroup(details, group, data);
            renderSvg();
        });
    } else if (field.type === 'bool') {
        input = document.createElement('input');
        input.type = 'checkbox';
        input.checked = !!data[field.key];
        input.addEventListener('change', () => {
            data[field.key] = input.checked;
            refreshGroup(details, group, data);
            renderSvg();
        });
    } else if (field.type === 'font') {
        input = document.createElement('select');
        for (const id of state.meta.fonts) {
            const o = document.createElement('option');
            o.value = id;
            o.textContent = id;
            input.appendChild(o);
        }
        input.value = data[field.key] ?? '';
        input.addEventListener('change', () => {
            data[field.key] = input.value;
            refreshGroup(details, group, data);
            renderSvg();
        });
    } else if (field.type === 'choice') {
        input = document.createElement('select');
        for (const option of field.options) {
            const o = document.createElement('option');
            o.value = option.value;
            o.textContent = option.label;
            input.appendChild(o);
        }
        input.value = data[field.key] ?? field.default ?? 0;
        input.addEventListener('change', () => {
            data[field.key] = parseInt(input.value, 10) | 0;
            refreshGroup(details, group, data);
            renderSvg();
        });
    }
    input.id = 'fld_' + field.key;
    row.appendChild(input);
    return row;
}

function refreshGroup(details, group, data) {
    const enabled = !group.enabledBy || !!data[group.enabledBy];
    details.classList.toggle('is-disabled', !enabled);
    for (const row of details.querySelectorAll('.form-row-conditional'))
        row.hidden = !enabled;
    details.querySelector('.form-group-meta').textContent = groupSummary(group, data);
}

function groupSummary(group, data) {
    if (group.enabledBy && !data[group.enabledBy]) return 'Off';

    const keys = group.fields;
    const xKey = keys.find(k => /_x$/.test(k));
    const yKey = keys.find(k => /_y$/.test(k));
    const wKey = keys.find(k => /_w$/.test(k));
    const hKey = keys.find(k => /_h$/.test(k));
    const parts = [];
    if (xKey && yKey) parts.push(`${data[xKey] | 0}, ${data[yKey] | 0}`);
    else if (yKey)    parts.push(`Y ${data[yKey] | 0}`);
    if (wKey && hKey) parts.push(`${data[wKey] | 0}×${data[hKey] | 0}`);
    else if (wKey)    parts.push(`W ${data[wKey] | 0}`);
    return parts.join(' · ') || 'On';
}

function setFormValue(key, val) {
    const el = document.getElementById('fld_' + key);
    if (!el) return;
    if (el.type === 'checkbox') el.checked = !!val;
    else                        el.value   = val;
    const details = el.closest('.form-group');
    if (details) {
        const group = FORM_GROUPS[state.active][Number(details.dataset.groupIndex)];
        refreshGroup(details, group, state[state.active]);
    }
}

// ── SVG render — dispatch to per-section renderer ───────────────────────────

const SCALE = 2;
const HANDLE_SIZE = 4;
let clipSeq = 0;   // unique clipPath ids within one renderSvg() pass

function renderSvg() {
    const svg = document.getElementById('lcd');
    const W = state.meta.screen_w, H = state.meta.screen_h;
    svg.setAttribute('viewBox', `0 0 ${W} ${H}`);
    svg.setAttribute('width',  W * SCALE);
    svg.setAttribute('height', H * SCALE);
    svg.innerHTML = '';

    rect(svg, { x: 0, y: 0, width: W, height: H, class: 'lcd-bg' });
    SECTIONS[state.active].renderer(svg);
}

// ── CLOCK renderer ──────────────────────────────────────────────────────────

function renderClock(svg) {
    const c = state.clock;
    const W = state.meta.screen_w;

    drawFreeElement(svg, {
        x: c.clock_panel_x, y: c.clock_panel_y,
        w: c.clock_panel_w, h: c.clock_panel_h,
        label: 'panel', cls: 'panel',
        fields: { x: 'clock_panel_x', y: 'clock_panel_y',
                  w: 'clock_panel_w', h: 'clock_panel_h' },
    });

    if (c.clock_show_time) {
        const fh = fontHeight(c.clock_time_font);
        const tw = Math.round(fh * 0.55) * 5;     // "00:00"
        // Center-anchored: clock_time_x is the middle of the time text.
        drawFreeElement(svg, {
            x: c.clock_time_x - Math.round(tw / 2), y: c.clock_time_y, w: tw, h: fh,
            label: 'time', cls: 'label-rect',
            fields: { x: 'clock_time_x', y: 'clock_time_y' },
            text: '88:88', textSize: fh,
        });
    }
    if (c.clock_show_date) {
        const fh = fontHeight(c.clock_date_font);
        const tw = Math.round(fh * 0.55) * 16;    // "Mon  YYYY-MM-DD"
        // Center-anchored: clock_date_x is the middle of the date text.
        drawFreeElement(svg, {
            x: c.clock_date_x - Math.round(tw / 2), y: c.clock_date_y, w: tw, h: fh,
            label: 'date', cls: 'label-rect',
            fields: { x: 'clock_date_x', y: 'clock_date_y' },
            text: 'Mon  2026-05-01', textSize: fh,
        });
    }
    if (c.clock_show_netinfo) {
        const fh = fontHeight(c.clock_netinfo_font);
        const tw = Math.round(fh * 0.55) * 28;    // "192.168.1.50   host.local"
        // Center-anchored: clock_netinfo_x is the middle of the IP/host text.
        drawFreeElement(svg, {
            x: c.clock_netinfo_x - Math.round(tw / 2), y: c.clock_netinfo_y, w: tw, h: fh,
            label: 'ip', cls: 'label-rect',
            fields: { x: 'clock_netinfo_x', y: 'clock_netinfo_y' },
            text: '192.168.1.50  host.local', textSize: fh,
        });
    }

    if (c.clock_show_mode_indicator) {
        drawFreeElement(svg, {
            x: c.clock_mode_indic_x, y: c.clock_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'clock_mode_indic_x', y: 'clock_mode_indic_y' },
        });
    }
    if (c.clock_show_event_indicator) {
        drawFreeElement(svg, {
            x: c.clock_event_indic_x, y: c.clock_event_indic_y, w: 16, h: 16,
            label: 'evt', cls: 'label-rect',
            fields: { x: 'clock_event_indic_x', y: 'clock_event_indic_y' },
        });
    }

    if (c.clock_show_calendar) {
        // Scrolling agenda line — top-left anchored, width for scroll (0 → full).
        const fh = fontHeight(c.clock_calendar_font);
        const cw = c.clock_calendar_w > 0 ? c.clock_calendar_w : W;
        drawFreeElement(svg, {
            x: c.clock_calendar_x, y: c.clock_calendar_y, w: cw, h: fh,
            label: 'calendar', cls: 'label-rect',
            fields: { x: 'clock_calendar_x', y: 'clock_calendar_y', w: 'clock_calendar_w' },
            text: '18:30  Dentist appt.', textSize: fh,
        });
    }

    if (c.clock_show_weather) {
        // Weather line — top-left anchored, width for centered text (0 → full).
        const fh = fontHeight(c.clock_weather_font);
        const ww = c.clock_weather_w > 0 ? c.clock_weather_w : W;
        drawWeatherElement(svg, {
            x: c.clock_weather_x, y: c.clock_weather_y, w: ww, h: Math.max(fh, 20),
            label: 'weather',
            fields: { x: 'clock_weather_x', y: 'clock_weather_y', w: 'clock_weather_w' },
            text: '+21 C  Partly cloudy  54%', textSize: fh,
        });
    }

    if (c.clock_show_strip) {
        const sx = c.clock_strip_x, sy = c.clock_strip_y;
        const sw = c.clock_strip_w, sh = c.clock_strip_h;
        drawFreeElement(svg, {
            x: sx, y: sy, w: sw, h: sh,
            label: 'strip', cls: 'panel',
            fillOpacity: clamp(c.clock_strip_bg_opa ?? 100, 0, 100) / 100,
            fields: { x: 'clock_strip_x', y: 'clock_strip_y',
                      w: 'clock_strip_w', h: 'clock_strip_h' },
        });

        const labW = clamp(c.clock_strip_label_w, 8, sw);
        drawStripLabel(svg, sx, sy, c.clock_strip_station_y, labW, sw,
                       'station', 'clock_strip_station_y');
        drawStripLabel(svg, sx, sy, c.clock_strip_title_y, labW, sw,
                       'title', 'clock_strip_title_y');
    }
}

function drawStripLabel(svg, sx, sy, yWithinStrip, labelW, stripW, name, fieldKey) {
    const x = sx + (stripW - labelW) / 2;
    const y = sy + yWithinStrip;
    const h = 14;
    const r = rect(svg, {
        x, y, width: labelW, height: h,
        class: `label-rect ${placeholderClass(name)}`,
    });
    text(svg, x + 4, y + 10, name, { 'font-size': 8 });
    setupYDrag(r, fieldKey);
}

// ── BT renderer ────────────────────────────────────────────────────────────

function renderBt(svg) {
    const b = state.bt;
    const W = state.meta.screen_w;

    if (b.bt_show_circle) {
        // Circle as a free element with rounded SVG (preview only)
        const r = rect(svg, {
            x: b.bt_circle_x, y: b.bt_circle_y,
            width: b.bt_circle_w, height: b.bt_circle_h,
            rx: Math.min(b.bt_circle_w, b.bt_circle_h) / 2,
            ry: Math.min(b.bt_circle_w, b.bt_circle_h) / 2,
            class: `panel ${placeholderClass('circle')}`,
        });
        setupMove(r, svg, { x: 'bt_circle_x', y: 'bt_circle_y' });
        addCornerHandles(svg,
            b.bt_circle_x, b.bt_circle_y, b.bt_circle_w, b.bt_circle_h,
            { x: 'bt_circle_x', y: 'bt_circle_y',
              w: 'bt_circle_w', h: 'bt_circle_h' });
        // Centered "BT" text
        const cx = b.bt_circle_x + b.bt_circle_w / 2;
        const cy = b.bt_circle_y + b.bt_circle_h / 2;
        const fh = fontHeight(b.bt_icon_font);
        text(svg, cx, cy + fh * 0.35, 'BT', {
            'font-size': fh, 'text-anchor': 'middle',
        });
        tag(svg, b.bt_circle_x + 2, b.bt_circle_y + 7, 'circle');
    }

    drawLabel(svg, b.bt_brand_x, b.bt_brand_y, b.bt_brand_font, 'Bluetooth Audio',
              'brand', { x: 'bt_brand_x', y: 'bt_brand_y' }, true);
    drawLabel(svg, b.bt_status_x, b.bt_status_y, b.bt_status_font, 'Connected',
              'status', { x: 'bt_status_x', y: 'bt_status_y' }, true);

    // Track title — scrolling label, fixed width
    const titleFh = fontHeight(b.bt_title_font);
    drawFreeElement(svg, {
        x: b.bt_title_x, y: b.bt_title_y, w: b.bt_title_w, h: titleFh,
        label: 'title', cls: 'label-rect',
        fields: { x: 'bt_title_x', y: 'bt_title_y', w: 'bt_title_w' },
        text: 'Track title', textSize: titleFh,
    });

    // Artist — scrolling label, fixed width
    const artistFh = fontHeight(b.bt_artist_font);
    drawFreeElement(svg, {
        x: b.bt_artist_x, y: b.bt_artist_y, w: b.bt_artist_w, h: artistFh,
        label: 'artist', cls: 'label-rect',
        fields: { x: 'bt_artist_x', y: 'bt_artist_y', w: 'bt_artist_w' },
        text: 'Artist', textSize: artistFh,
    });

    // Time "0:00 / 0:00"
    drawLabel(svg, b.bt_time_x, b.bt_time_y, b.bt_time_font, '0:00 / 0:00',
              'time', { x: 'bt_time_x', y: 'bt_time_y' }, true);

    // Vol label is center-anchored on bt_time_x, one line below time — non-draggable preview
    const timeFh   = fontHeight(b.bt_time_font);
    const volFh    = fontHeight(b.bt_vol_label_font);
    const volX     = b.bt_time_x;
    const volY     = b.bt_time_y + timeFh + 4;
    text(svg, volX, volY + volFh * 0.78, 'VOL: 50%', { 'font-size': volFh, 'text-anchor': 'middle' });
    tag(svg, volX + 2, volY + 7, 'vol');

    if (b.bt_show_mode_indicator) {
        drawFreeElement(svg, {
            x: b.bt_mode_indic_x, y: b.bt_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'bt_mode_indic_x', y: 'bt_mode_indic_y' },
        });
    }
    if (b.bt_show_clock) {
        // clock_widget — "00:00" label, sized by the configured clock font
        drawLabel(svg, b.bt_clock_widget_x, b.bt_clock_widget_y, b.bt_clock_font,
                  '00:00', 'clock',
                  { x: 'bt_clock_widget_x', y: 'bt_clock_widget_y' });
    }
}

// ── RADIO renderer ─────────────────────────────────────────────────────────

function renderRadio(svg) {
    const r = state.radio;
    const W = state.meta.screen_w;

    if (r.radio_show_cassette) {
        drawAnimatedWheel(svg, r.radio_cassette_l_x, r.radio_cassette_l_y,
                         r.radio_cassette_l_size, 'wheel L',
                         'radio_cassette_l_x', 'radio_cassette_l_y', 'radio_cassette_l_size');
        drawAnimatedWheel(svg, r.radio_cassette_r_x, r.radio_cassette_r_y,
                         r.radio_cassette_r_size, 'wheel R',
                         'radio_cassette_r_x', 'radio_cassette_r_y', 'radio_cassette_r_size');
    }

    if (r.radio_show_np) {
        // Now-playing widget = two stacked labels (station + title). Width is fixed
        // in firmware to screen_w - 20 (full-screen scrolling line); the title sits
        // a station-font-height + 4px below the station (mirrors the firmware).
        const stationFh = fontHeight(r.radio_np_station_font);
        const titleFh   = fontHeight(r.radio_np_title_font);
        const npW       = Math.max(W - 20, 8);
        drawFreeElement(svg, {
            x: r.radio_np_x, y: r.radio_np_y, w: npW, h: stationFh,
            label: 'np_station', cls: 'label-rect',
            fields: { x: 'radio_np_x', y: 'radio_np_y' },
            text: 'Atlas Radio', textSize: stationFh,
        });
        drawFreeElement(svg, {
            x: r.radio_np_x, y: r.radio_np_y + stationFh + 4, w: npW, h: titleFh,
            label: 'np_title', cls: 'label-rect',
            fields: { x: 'radio_np_x', y: 'radio_np_y' },
            text: 'Title — Artist', textSize: titleFh,
        });
    }

    if (r.radio_show_playback_status) {
        drawLabel(svg, 0, r.radio_state_y, r.radio_state_font, 'PLAYING',
                  'state', { y: 'radio_state_y' });
        drawLabel(svg, 0, r.radio_audio_info_y, r.radio_audio_info_font,
                  '44100 Hz  2ch  128kbps   VOL: 42%',
                  'info', { y: 'radio_audio_info_y' });
    }

    if (r.radio_show_mode_indicator) {
        drawFreeElement(svg, {
            x: r.radio_mode_indic_x, y: r.radio_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'radio_mode_indic_x', y: 'radio_mode_indic_y' },
        });
    }
    if (r.radio_show_clock) {
        drawLabel(svg, r.radio_clock_widget_x, r.radio_clock_widget_y, r.radio_clock_font,
                  '00:00', 'clock',
                  { x: 'radio_clock_widget_x', y: 'radio_clock_widget_y' }, true);
    }
    if (r.radio_show_event_indicator) {
        drawFreeElement(svg, {
            x: r.radio_event_indic_x, y: r.radio_event_indic_y, w: 16, h: 16,
            label: 'evt', cls: 'label-rect',
            fields: { x: 'radio_event_indic_x', y: 'radio_event_indic_y' },
        });
    }
    if (r.radio_show_vu) {
        drawFreeElement(svg, {
            x: r.radio_vu_x, y: r.radio_vu_y, w: r.radio_vu_w, h: r.radio_vu_h,
            label: 'VU', cls: 'label-rect',
            fields: { x: 'radio_vu_x', y: 'radio_vu_y' },
        });
    }
    if (r.radio_show_weather) {
        const fh = fontHeight(r.radio_weather_font);
        const ww = r.radio_weather_w > 0 ? r.radio_weather_w : W;
        drawWeatherElement(svg, {
            x: r.radio_weather_x, y: r.radio_weather_y, w: ww, h: Math.max(fh, 20),
            label: 'weather',
            fields: { x: 'radio_weather_x', y: 'radio_weather_y', w: 'radio_weather_w' },
            text: '+21 C  Partly cloudy  54%', textSize: fh,
        });
    }
}

// ── SD PLAYER renderer ───────────────────────────────────────────────────────

function renderSd(svg) {
    const s = state.sd;

    if (s.sd_show_cassette) {
        drawAnimatedWheel(svg, s.sd_cassette_l_x, s.sd_cassette_l_y,
                         s.sd_cassette_l_size, 'wheel L',
                         'sd_cassette_l_x', 'sd_cassette_l_y', 'sd_cassette_l_size');
        drawAnimatedWheel(svg, s.sd_cassette_r_x, s.sd_cassette_r_y,
                         s.sd_cassette_r_size, 'wheel R',
                         'sd_cassette_r_x', 'sd_cassette_r_y', 'sd_cassette_r_size');
    }

    drawLabel(svg, 0, s.sd_title_y, s.sd_title_font, 'Artist - Title',
              'title', { y: 'sd_title_y' });
    if (s.sd_show_folder) {
        drawLabel(svg, 0, s.sd_folder_y, s.sd_folder_font, 'Folder   3/12',
                  'folder', { y: 'sd_folder_y' });
    }
    if (s.sd_show_info) {
        drawLabel(svg, 0, s.sd_info_y, s.sd_info_font, 'VOL: 42%   SHUFFLE   REPEAT ALL',
                  'info', { y: 'sd_info_y' });
    }

    if (s.sd_show_bar && s.sd_bar_w > 0) {
        // Firmware centers the bar and anchors it under the (optional) time row;
        // approximate that here for a size/position hint (not draggable).
        const fh = fontHeight(s.sd_info_font);
        const timeRow = s.sd_show_time ? fh + 4 : 0;
        const by = s.sd_info_y + fh + 4 + timeRow;
        const bx = Math.round((state.meta.screen_w - s.sd_bar_w) / 2);
        rect(svg, { x: bx, y: by, width: s.sd_bar_w, height: s.sd_bar_h,
                    class: `label-rect ${placeholderClass('bar')}` });
        tag(svg, bx + 2, by + 7, 'bar');
    }

    if (s.sd_show_mode_indicator) {
        drawFreeElement(svg, {
            x: s.sd_mode_indic_x, y: s.sd_mode_indic_y, w: 16, h: 16,
            label: 'mode', cls: 'label-rect',
            fields: { x: 'sd_mode_indic_x', y: 'sd_mode_indic_y' },
        });
    }
    if (s.sd_show_clock) {
        drawLabel(svg, s.sd_clock_widget_x, s.sd_clock_widget_y, s.sd_clock_font,
                  '00:00', 'clock',
                  { x: 'sd_clock_widget_x', y: 'sd_clock_widget_y' }, true);
    }
    if (s.sd_show_event_indicator) {
        drawFreeElement(svg, {
            x: s.sd_event_indic_x, y: s.sd_event_indic_y, w: 16, h: 16,
            label: 'evt', cls: 'label-rect',
            fields: { x: 'sd_event_indic_x', y: 'sd_event_indic_y' },
        });
    }
    if (s.sd_show_vu) {
        drawFreeElement(svg, {
            x: s.sd_vu_x, y: s.sd_vu_y, w: s.sd_vu_w, h: s.sd_vu_h,
            label: 'VU', cls: 'label-rect',
            fields: { x: 'sd_vu_x', y: 'sd_vu_y' },
        });
    }
    if (s.sd_show_weather) {
        const fh = fontHeight(s.sd_weather_font);
        const ww = s.sd_weather_w > 0 ? s.sd_weather_w : state.meta.screen_w;
        drawWeatherElement(svg, {
            x: s.sd_weather_x, y: s.sd_weather_y, w: ww, h: Math.max(fh, 20),
            label: 'weather',
            fields: { x: 'sd_weather_x', y: 'sd_weather_y', w: 'sd_weather_w' },
            text: '+21 C  Partly cloudy  54%', textSize: fh,
        });
    }
}

function drawLabel(svg, x, y, fontId, text_str, name, fields, anchorCenter) {
    const fh = fontHeight(fontId);
    const tw = Math.round(fh * 0.55) * Math.max(text_str.length, 5);
    if (fields.x === undefined) {
        // Centered label: firmware draws it full-width with centered text.
        x = Math.round((state.meta.screen_w - tw) / 2);
    } else if (anchorCenter) {
        // Center-anchored: x is the middle of the text; drag still moves it.
        x -= Math.round(tw / 2);
    }
    drawFreeElement(svg, {
        x, y, w: tw, h: fh,
        label: name, cls: 'label-rect',
        fields,
        text: text_str, textSize: fh,
    });
}

function drawAnimatedWheel(svg, x, y, size, label, xField, yField, sizeField) {
    size = Math.max(16, size | 0);
    const fields = { x: xField, y: yField, w: sizeField, h: sizeField, square: true };
    drawFreeElement(svg, {
        x, y, w: size, h: size,
        label, cls: 'label-rect animated-wheel', fields,
    });
}

// ── Free element (move + 4 corner resize) ──────────────────────────────────

function drawFreeElement(svg, opts) {
    const r = rect(svg, {
        x: opts.x, y: opts.y, width: opts.w, height: opts.h,
        class: `${opts.cls} ${placeholderClass(opts.label)}`,
    });
    if (opts.fillOpacity !== undefined) r.style.fillOpacity = opts.fillOpacity;
    setupMove(r, svg, opts.fields);

    tag(svg, opts.x + 2, opts.y + 7, opts.label);

    if (opts.text) {
        // Clip the sample text to the box — the firmware clips widget content
        // the same way, so overflow would misrepresent the on-device look.
        const cid = 'el_clip_' + (clipSeq++);
        const cp = document.createElementNS(SVG_NS, 'clipPath');
        cp.setAttribute('id', cid);
        const cr = document.createElementNS(SVG_NS, 'rect');
        cr.setAttribute('x', opts.x);
        cr.setAttribute('y', opts.y);
        cr.setAttribute('width', opts.w);
        cr.setAttribute('height', opts.h);
        cp.appendChild(cr);
        svg.appendChild(cp);
        text(svg, opts.x + opts.w / 2, opts.y + opts.h * 0.78, opts.text, {
            'font-size': Math.min(opts.textSize, opts.h),
            'text-anchor': 'middle',
            'clip-path': 'url(#' + cid + ')',
        });
    }

    if (opts.fields.w && opts.fields.h) {
        addCornerHandles(svg, opts.x, opts.y, opts.w, opts.h, opts.fields);
    }
}

// Weather is special: its box (W) is a full-width centering FRAME that draws no
// background on the device — the actual plate hugs the centered icon+text. So
// draw the span as a dashed guide (movable, W-editable in the form) and a
// filled pill sized to the rendered content, centered within the span.
function drawWeatherElement(svg, opts) {
    const frame = rect(svg, {
        x: opts.x, y: opts.y, width: opts.w, height: opts.h,
        class: `label-frame ${placeholderClass('weather')}`,
    });
    setupMove(frame, svg, opts.fields);
    tag(svg, opts.x + 2, opts.y + 7, opts.label);

    const cx = opts.x + opts.w / 2;
    const t = text(svg, cx, opts.y + opts.h * 0.78, opts.text, {
        'font-size': Math.min(opts.textSize, opts.h),
        'text-anchor': 'middle',
    });
    // Size the pill to the rendered text (+ horizontal padding), centered.
    const pw = t.getBBox().width + 12;
    const pill = rect(svg, {
        x: cx - pw / 2, y: opts.y, width: pw, height: opts.h,
        class: `label-rect ${placeholderClass('weather')}`,
    });
    pill.style.pointerEvents = 'none';   // let drags fall through to the frame
    svg.insertBefore(pill, t);           // paint the pill behind the text
}

function addCornerHandles(svg, x, y, w, h, fields) {
    const corners = [
        { cx: x,     cy: y,     dir: 'tl' },
        { cx: x + w, cy: y,     dir: 'tr' },
        { cx: x,     cy: y + h, dir: 'bl' },
        { cx: x + w, cy: y + h, dir: 'br' },
    ];
    for (const cr of corners) {
        const handle = rect(svg, {
            x: cr.cx - HANDLE_SIZE / 2,
            y: cr.cy - HANDLE_SIZE / 2,
            width: HANDLE_SIZE, height: HANDLE_SIZE,
            class: 'corner-handle',
            'data-corner': cr.dir,
        });
        setupResize(handle, svg, fields, cr.dir);
    }
}

function setupMove(el, svg, fields) {
    el.addEventListener('pointerdown', (e) => {
        if (e.target !== el) return;
        e.preventDefault();
        el.setPointerCapture(e.pointerId);
        el.classList.add('dragging');

        const data = state[state.active];
        const start = { mx: e.clientX, my: e.clientY,
                        x: data[fields.x] | 0, y: data[fields.y] | 0 };
        const px = svg.getBoundingClientRect();
        const pxPerU = { x: px.width / state.meta.screen_w,
                         y: px.height / state.meta.screen_h };

        const onMove = (ev) => {
            const dx = Math.round((ev.clientX - start.mx) / pxPerU.x);
            const dy = Math.round((ev.clientY - start.my) / pxPerU.y);
            if (fields.x !== undefined) {
                data[fields.x] = start.x + dx;
                setFormValue(fields.x, data[fields.x]);
            }
            data[fields.y] = start.y + dy;
            setFormValue(fields.y, data[fields.y]);
            renderSvg();
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
            el.classList.remove('dragging');
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
    });
}

function setupResize(el, svg, fields, dir) {
    el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        el.setPointerCapture(e.pointerId);

        const data = state[state.active];
        const start = {
            mx: e.clientX, my: e.clientY,
            x: data[fields.x] | 0, y: data[fields.y] | 0,
            w: data[fields.w] | 0, h: data[fields.h] | 0,
        };
        const px = svg.getBoundingClientRect();
        const pxPerU = { x: px.width / state.meta.screen_w,
                         y: px.height / state.meta.screen_h };

        const onMove = (ev) => {
            const dx = Math.round((ev.clientX - start.mx) / pxPerU.x);
            const dy = Math.round((ev.clientY - start.my) / pxPerU.y);
            let nx = start.x, ny = start.y, nw = start.w, nh = start.h;
            if (fields.square) {
                const dwX = dir.includes('r') ? dx : -dx;
                const dwY = dir.includes('b') ? dy : -dy;
                const delta = Math.abs(dwX) >= Math.abs(dwY) ? dwX : dwY;
                const size = Math.max(16, start.w + delta);
                if (dir.includes('l')) nx = start.x + start.w - size;
                if (dir.includes('t')) ny = start.y + start.h - size;
                nw = nh = size;
            } else {
            if (dir.includes('l')) { nx = start.x + dx; nw = start.w - dx; }
            if (dir.includes('r')) {                     nw = start.w + dx; }
            if (dir.includes('t')) { ny = start.y + dy; nh = start.h - dy; }
            if (dir.includes('b')) {                     nh = start.h + dy; }
            if (nw < 4) { nw = 4; if (dir.includes('l')) nx = start.x + start.w - 4; }
            if (nh < 4) { nh = 4; if (dir.includes('t')) ny = start.y + start.h - 4; }
            }

            data[fields.x] = nx; data[fields.y] = ny;
            data[fields.w] = nw;
            if (fields.h !== fields.w) data[fields.h] = nh;
            setFormValue(fields.x, nx); setFormValue(fields.y, ny);
            setFormValue(fields.w, nw);
            if (fields.h !== fields.w) setFormValue(fields.h, nh);
            renderSvg();
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
    });
}

// ── Y-only drag (used by strip station/title labels) ──────────────────────

function setupYDrag(el, field) {
    el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        el.setPointerCapture(e.pointerId);
        el.classList.add('dragging');

        const data = state[state.active];
        const startY = e.clientY;
        const startVal = data[field] | 0;
        const pxPerU = el.ownerSVGElement.getBoundingClientRect().height
                       / state.meta.screen_h;

        const onMove = (ev) => {
            const dy = Math.round((ev.clientY - startY) / pxPerU);
            const v = startVal + dy;
            if (v !== data[field]) {
                data[field] = v;
                setFormValue(field, v);
                renderSvg();
            }
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
            el.classList.remove('dragging');
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp);
    });
}

// ── Apply / reset ──────────────────────────────────────────────────────────

async function applyProfile() {
    const btn = document.getElementById('btn_apply');
    btn.disabled = true;
    setStatus('Applying...');
    try {
        const r = await fetch(`/api/ui/profile/${state.active}`, {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify(state[state.active]),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        setStatus(`Applied (${state.active}) — device screen rebuilding.`);
    } catch (err) {
        setStatus('Apply failed: ' + err.message, true);
    } finally {
        btn.disabled = false;
    }
}

async function resetProfile() {
    if (!confirm('Reset ALL layout sections to factory defaults?')) return;
    setStatus('Resetting...');
    try {
        await fetch('/api/ui/profile/reset', { method: 'POST' });
        for (const name of Object.keys(SECTIONS)) {
            state[name] = await fetch(`/api/ui/profile/${name}`).then(r => r.json());
        }
        // Rebuild so switches also refresh conditional rows and group summaries.
        buildForm();
        renderSvg();
        setStatus('Reset to defaults.');
    } catch (err) {
        setStatus('Reset failed: ' + err.message, true);
    }
}

// ── helpers ─────────────────────────────────────────────────────────────────

function fontHeight(id) {
    if (!id) return 14;
    const m = id.match(/_(\d+)(_pl)?$/);
    return m ? parseInt(m[1], 10) : 14;
}

// Keep the preview readable when elements overlap: the colour communicates the
// element's role consistently across Home, Radio, SD and Bluetooth screens.
function placeholderClass(name) {
    const classes = {
        time: 'ph-time', clock: 'ph-time',
        date: 'ph-date',
        station: 'ph-media-primary', np_station: 'ph-media-primary', brand: 'ph-media-primary',
        title: 'ph-media-secondary', np_title: 'ph-media-secondary', artist: 'ph-media-secondary',
        ip: 'ph-info', info: 'ph-info', status: 'ph-info', state: 'ph-info', folder: 'ph-info',
        mode: 'ph-mode',
        evt: 'ph-event',
        calendar: 'ph-calendar',
        weather: 'ph-weather',
        VU: 'ph-vu',
        bar: 'ph-progress',
        panel: 'ph-container', strip: 'ph-container', circle: 'ph-container',
    };
    return classes[name] || 'ph-default';
}

function rect(parent, attrs) {
    const r = document.createElementNS(SVG_NS, 'rect');
    for (const k in attrs) r.setAttribute(k, attrs[k]);
    parent.appendChild(r);
    return r;
}
function text(parent, x, y, txt, attrs = {}) {
    const t = document.createElementNS(SVG_NS, 'text');
    t.setAttribute('x', x); t.setAttribute('y', y);
    t.setAttribute('class', 'lcd-text');
    for (const k in attrs) t.setAttribute(k, attrs[k]);
    t.textContent = txt;
    parent.appendChild(t);
    return t;
}
function tag(parent, x, y, txt) {
    const t = document.createElementNS(SVG_NS, 'text');
    t.setAttribute('x', x); t.setAttribute('y', y);
    t.setAttribute('class', 'field-tag');
    t.textContent = txt;
    parent.appendChild(t);
}
function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v | 0)); }
function setStatus(msg, error = false) {
    const el = document.getElementById('status_msg');
    el.textContent = msg;
    el.style.color = error ? 'var(--red)' : 'var(--text-dim)';
}
