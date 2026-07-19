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

const HOTSPOT_ACTIONS = [
    { value: 0, label: 'Play / stop' },
    { value: 6, label: 'Play / pause' },
    { value: 1, label: 'Previous' },
    { value: 2, label: 'Next' },
    { value: 3, label: 'Volume -' },
    { value: 4, label: 'Volume +' },
    { value: 5, label: 'Stop' },
    { value: 7, label: 'Open playlist' },
    { value: 8, label: 'Open SD browser' },
];

function touchHotspotFields(prefix) {
    const fields = [];
    for (let i = 1; i <= 6; i++) {
        const key = `${prefix}_hotspot_${i}`;
        fields.push(
            { key: `${key}_enabled`, label: 'Enabled', type: 'bool' },
            { key: `${key}_action`, label: 'Action', type: 'choice', default: (i - 1) % 6,
              options: HOTSPOT_ACTIONS },
            { key: `${key}_x`, label: 'X', type: 'number' },
            { key: `${key}_y`, label: 'Y', type: 'number' },
            { key: `${key}_w`, label: 'W', type: 'number', min: 8, default: 48 },
            { key: `${key}_h`, label: 'H', type: 'number', min: 8, default: 36 },
            { key: `${key}_radius`, label: 'Roundness %', type: 'number', min: 0, max: 100, default: 20 },
        );
    }
    return fields;
}

function touchHotspotGroups(prefix) {
    const subgroups = [];
    for (let i = 1; i <= 6; i++) {
        const key = `${prefix}_hotspot_${i}`;
        subgroups.push({
            title: `Hotspot ${i}`,
            enabledBy: `${key}_enabled`,
            fields: [`${key}_enabled`, `${key}_action`, `${key}_x`, `${key}_y`,
                     `${key}_w`, `${key}_h`, `${key}_radius`],
        });
    }
    return [{ title: 'Touch hotspots', fields: [], subgroups }];
}

// ── Field schemas — order is purely UI grouping; doesn't affect backend ────

const CLOCK_FIELDS = [
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

    { key: 'clock_show_strip', label: 'Show strip background', type: 'bool' },
    { key: 'clock_strip_x',    label: 'Strip X',      type: 'number' },
    { key: 'clock_strip_y',    label: 'Strip Y',      type: 'number' },
    { key: 'clock_strip_w',    label: 'Strip W',      type: 'number' },
    { key: 'clock_strip_h',    label: 'Strip H',      type: 'number' },
    { key: 'clock_strip_bg_opa', label: 'Strip BG opacity %', type: 'number', min: 0, max: 100, default: 100 },
    { key: 'clock_strip_station_x',       label: 'Station X',      type: 'number' },
    { key: 'clock_strip_station_y',       label: 'Station Y',      type: 'number' },
    { key: 'clock_strip_station_w',       label: 'Station W',      type: 'number' },
    { key: 'clock_strip_title_x',         label: 'Title X',        type: 'number' },
    { key: 'clock_strip_title_y',         label: 'Title Y',        type: 'number' },
    { key: 'clock_strip_title_w',         label: 'Title W',        type: 'number' },
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
    { key: 'radio_show_np_title',     label: 'Show track title', type: 'bool' },
    { key: 'radio_np_x',              label: 'NP X',             type: 'number' },
    { key: 'radio_np_y',              label: 'NP Y',             type: 'number' },
    { key: 'radio_show_station_icon', label: 'Show station icon', type: 'bool' },
    { key: 'radio_station_icon_x',    label: 'Station icon X',   type: 'number' },
    { key: 'radio_station_icon_y',    label: 'Station icon Y',   type: 'number' },
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
    { key: 'radio_vu_transparent',       label: 'Transparent bg',    type: 'bool' },
    { key: 'radio_needle_frame',         label: 'Thin frame',        type: 'bool' },
    { key: 'radio_needle_show_l',        label: 'Show left needle',  type: 'bool' },
    { key: 'radio_needle_l_x',           label: 'Left needle X',     type: 'number' },
    { key: 'radio_needle_l_y',           label: 'Left needle Y',     type: 'number' },
    { key: 'radio_needle_l_w',           label: 'Left needle W',     type: 'number', min: 20, max: 480 },
    { key: 'radio_needle_l_h',           label: 'Left needle H',     type: 'number', min: 20, max: 480 },
    { key: 'radio_needle_show_r',        label: 'Show right needle', type: 'bool' },
    { key: 'radio_needle_r_x',           label: 'Right needle X',    type: 'number' },
    { key: 'radio_needle_r_y',           label: 'Right needle Y',    type: 'number' },
    { key: 'radio_needle_r_w',           label: 'Right needle W',    type: 'number', min: 20, max: 480 },
    { key: 'radio_needle_r_h',           label: 'Right needle H',    type: 'number', min: 20, max: 480 },
    { key: 'radio_show_cassette',        label: 'Show animated wheels', type: 'bool' },
    { key: 'radio_animation_style',      label: 'Graphic', type: 'choice', default: 0,
      options: [{ value: 0, label: 'Cassette reels' }, { value: 1, label: 'Car rims' }] },
    { key: 'radio_show_wheel_left',      label: 'Show left wheel',       type: 'bool' },
    { key: 'radio_cassette_l_x',         label: 'Left wheel X',         type: 'number' },
    { key: 'radio_cassette_l_y',         label: 'Left wheel Y',         type: 'number' },
    { key: 'radio_cassette_l_size',      label: 'Left wheel size',      type: 'number', min: 16, max: 480 },
    { key: 'radio_show_wheel_right',     label: 'Show right wheel',      type: 'bool' },
    { key: 'radio_cassette_r_x',         label: 'Right wheel X',        type: 'number' },
    { key: 'radio_cassette_r_y',         label: 'Right wheel Y',        type: 'number' },
    { key: 'radio_cassette_r_size',      label: 'Right wheel size',     type: 'number', min: 16, max: 480 },
    { key: 'radio_show_weather', label: 'Show weather', type: 'bool' },
    { key: 'radio_weather_x', label: 'Weather X', type: 'number' },
    { key: 'radio_weather_y', label: 'Weather Y', type: 'number' },
    { key: 'radio_weather_w', label: 'Weather W', type: 'number' },
    { key: 'radio_weather_font', label: 'Weather font', type: 'font' },
    { key: 'radio_show_ctrl_overlay', label: 'Show tap controls overlay', type: 'bool', default: true },
    ...touchHotspotFields('radio'),
];

const SD_FIELDS = [
    { key: 'sd_title_x',    label: 'Title X',          type: 'number' },
    { key: 'sd_title_y',    label: 'Title Y',          type: 'number' },
    { key: 'sd_title_w',    label: 'Title W',          type: 'number' },
    { key: 'sd_title_font', label: 'Title font',       type: 'font'   },

    { key: 'sd_show_folder', label: 'Show folder',     type: 'bool'   },
    { key: 'sd_folder_y',    label: 'Folder Y',        type: 'number' },
    { key: 'sd_folder_font', label: 'Folder font',     type: 'font'   },

    { key: 'sd_show_info', label: 'Show info',         type: 'bool'   },
    { key: 'sd_info_y',    label: 'Info Y',            type: 'number' },
    { key: 'sd_info_font', label: 'Info font',         type: 'font'   },

    { key: 'sd_show_time',            label: 'Show playback time', type: 'bool' },
    { key: 'sd_time_y',               label: 'Time Y',            type: 'number' },

    { key: 'sd_show_bar',             label: 'Show progress bar', type: 'bool' },
    { key: 'sd_bar_x',                label: 'Bar X',             type: 'number' },
    { key: 'sd_bar_y',                label: 'Bar Y',             type: 'number' },
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
    { key: 'sd_vu_transparent',       label: 'Transparent bg',    type: 'bool' },
    { key: 'sd_needle_frame',         label: 'Thin frame',        type: 'bool' },
    { key: 'sd_needle_show_l',        label: 'Show left needle',  type: 'bool' },
    { key: 'sd_needle_l_x',           label: 'Left needle X',     type: 'number' },
    { key: 'sd_needle_l_y',           label: 'Left needle Y',     type: 'number' },
    { key: 'sd_needle_l_w',           label: 'Left needle W',     type: 'number', min: 20, max: 480 },
    { key: 'sd_needle_l_h',           label: 'Left needle H',     type: 'number', min: 20, max: 480 },
    { key: 'sd_needle_show_r',        label: 'Show right needle', type: 'bool' },
    { key: 'sd_needle_r_x',           label: 'Right needle X',    type: 'number' },
    { key: 'sd_needle_r_y',           label: 'Right needle Y',    type: 'number' },
    { key: 'sd_needle_r_w',           label: 'Right needle W',    type: 'number', min: 20, max: 480 },
    { key: 'sd_needle_r_h',           label: 'Right needle H',    type: 'number', min: 20, max: 480 },
    { key: 'sd_show_cassette',        label: 'Show animated wheels', type: 'bool' },
    { key: 'sd_animation_style',      label: 'Graphic', type: 'choice', default: 0,
      options: [{ value: 0, label: 'Cassette reels' }, { value: 1, label: 'Car rims' }] },
    { key: 'sd_show_wheel_left',      label: 'Show left wheel',       type: 'bool' },
    { key: 'sd_cassette_l_x',         label: 'Left wheel X',         type: 'number' },
    { key: 'sd_cassette_l_y',         label: 'Left wheel Y',         type: 'number' },
    { key: 'sd_cassette_l_size',      label: 'Left wheel size',      type: 'number', min: 16, max: 480 },
    { key: 'sd_show_wheel_right',     label: 'Show right wheel',      type: 'bool' },
    { key: 'sd_cassette_r_x',         label: 'Right wheel X',        type: 'number' },
    { key: 'sd_cassette_r_y',         label: 'Right wheel Y',        type: 'number' },
    { key: 'sd_cassette_r_size',      label: 'Right wheel size',     type: 'number', min: 16, max: 480 },
    { key: 'sd_show_weather', label: 'Show weather', type: 'bool' },
    { key: 'sd_weather_x', label: 'Weather X', type: 'number' },
    { key: 'sd_weather_y', label: 'Weather Y', type: 'number' },
    { key: 'sd_weather_w', label: 'Weather W', type: 'number' },
    { key: 'sd_weather_font', label: 'Weather font', type: 'font' },
    { key: 'sd_show_ctrl_overlay', label: 'Show tap controls overlay', type: 'bool', default: true },
    ...touchHotspotFields('sd'),
];

// Form-only grouping. Field schemas above remain the API/source-of-truth; these
// groups only decide how the editor presents them. `enabledBy` keeps the Show
// switch visible while hiding the controls that have no effect when it is off.
const FORM_GROUPS = {
    clock: [
        { title: 'Time', enabledBy: 'clock_show_time', fields: ['clock_show_time', 'clock_time_x', 'clock_time_y', 'clock_time_font'] },
        { title: 'Date', enabledBy: 'clock_show_date', fields: ['clock_show_date', 'clock_date_x', 'clock_date_y', 'clock_date_font'] },
        { title: 'Network info', enabledBy: 'clock_show_netinfo', fields: ['clock_show_netinfo', 'clock_netinfo_x', 'clock_netinfo_y', 'clock_netinfo_font'] },
        { title: 'Station / title', fields: ['clock_show_strip', 'clock_strip_x', 'clock_strip_y', 'clock_strip_w', 'clock_strip_h', 'clock_strip_bg_opa', 'clock_strip_station_x', 'clock_strip_station_y', 'clock_strip_station_w', 'clock_strip_title_x', 'clock_strip_title_y', 'clock_strip_title_w', 'clock_strip_station_font', 'clock_strip_title_font'] },
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
        { title: 'Now playing', enabledBy: 'radio_show_np', fields: ['radio_show_np', 'radio_show_np_title', 'radio_np_x', 'radio_np_y', 'radio_np_station_font', 'radio_np_title_font'] },
        { title: 'Station icon', enabledBy: 'radio_show_station_icon', fields: ['radio_show_station_icon', 'radio_station_icon_x', 'radio_station_icon_y', 'radio_station_icon_size'] },
        { title: 'Playback status', enabledBy: 'radio_show_playback_status', fields: ['radio_show_playback_status', 'radio_state_y', 'radio_state_font', 'radio_audio_info_y', 'radio_audio_info_font'] },
        { title: 'Mode indicator', enabledBy: 'radio_show_mode_indicator', fields: ['radio_show_mode_indicator', 'radio_mode_indic_x', 'radio_mode_indic_y'] },
        { title: 'Clock', enabledBy: 'radio_show_clock', fields: ['radio_show_clock', 'radio_clock_widget_x', 'radio_clock_widget_y', 'radio_clock_font'] },
        { title: 'Event indicator', enabledBy: 'radio_show_event_indicator', fields: ['radio_show_event_indicator', 'radio_event_indic_x', 'radio_event_indic_y'] },
        { title: 'Animated wheels', enabledBy: 'radio_show_cassette', fields: ['radio_show_cassette', 'radio_animation_style', 'radio_show_wheel_left', 'radio_cassette_l_x', 'radio_cassette_l_y', 'radio_cassette_l_size', 'radio_show_wheel_right', 'radio_cassette_r_x', 'radio_cassette_r_y', 'radio_cassette_r_size'] },
        { title: 'VU meter', enabledBy: 'radio_show_vu', fields: ['radio_show_vu', 'radio_vu_x', 'radio_vu_y', 'radio_vu_w', 'radio_vu_h', 'radio_vu_transparent'] },
        { title: 'Needle VU', fields: ['radio_needle_frame', 'radio_needle_show_l', 'radio_needle_l_x', 'radio_needle_l_y', 'radio_needle_l_w', 'radio_needle_l_h', 'radio_needle_show_r', 'radio_needle_r_x', 'radio_needle_r_y', 'radio_needle_r_w', 'radio_needle_r_h'] },
        { title: 'Weather', enabledBy: 'radio_show_weather', fields: ['radio_show_weather', 'radio_weather_x', 'radio_weather_y', 'radio_weather_w', 'radio_weather_font'] },
        { title: 'Tap controls overlay', enabledBy: 'radio_show_ctrl_overlay', fields: ['radio_show_ctrl_overlay'] },
        ...touchHotspotGroups('radio'),
    ],
    sd: [
        { title: 'Track title', fields: ['sd_title_x', 'sd_title_y', 'sd_title_w', 'sd_title_font'] },
        { title: 'Folder', enabledBy: 'sd_show_folder', fields: ['sd_show_folder', 'sd_folder_y', 'sd_folder_font'] },
        { title: 'Playback info', enabledBy: 'sd_show_info', fields: ['sd_show_info', 'sd_info_y', 'sd_info_font'] },
        { title: 'Playback time', enabledBy: 'sd_show_time', fields: ['sd_show_time', 'sd_time_y'] },
        { title: 'Progress bar', enabledBy: 'sd_show_bar', fields: ['sd_show_bar', 'sd_bar_x', 'sd_bar_y', 'sd_bar_w', 'sd_bar_h'] },
        { title: 'Mode indicator', enabledBy: 'sd_show_mode_indicator', fields: ['sd_show_mode_indicator', 'sd_mode_indic_x', 'sd_mode_indic_y'] },
        { title: 'Clock', enabledBy: 'sd_show_clock', fields: ['sd_show_clock', 'sd_clock_widget_x', 'sd_clock_widget_y', 'sd_clock_font'] },
        { title: 'Event indicator', enabledBy: 'sd_show_event_indicator', fields: ['sd_show_event_indicator', 'sd_event_indic_x', 'sd_event_indic_y'] },
        { title: 'Animated wheels', enabledBy: 'sd_show_cassette', fields: ['sd_show_cassette', 'sd_animation_style', 'sd_show_wheel_left', 'sd_cassette_l_x', 'sd_cassette_l_y', 'sd_cassette_l_size', 'sd_show_wheel_right', 'sd_cassette_r_x', 'sd_cassette_r_y', 'sd_cassette_r_size'] },
        { title: 'VU meter', enabledBy: 'sd_show_vu', fields: ['sd_show_vu', 'sd_vu_x', 'sd_vu_y', 'sd_vu_w', 'sd_vu_h', 'sd_vu_transparent'] },
        { title: 'Needle VU', fields: ['sd_needle_frame', 'sd_needle_show_l', 'sd_needle_l_x', 'sd_needle_l_y', 'sd_needle_l_w', 'sd_needle_l_h', 'sd_needle_show_r', 'sd_needle_r_x', 'sd_needle_r_y', 'sd_needle_r_w', 'sd_needle_r_h'] },
        { title: 'Weather', enabledBy: 'sd_show_weather', fields: ['sd_show_weather', 'sd_weather_x', 'sd_weather_y', 'sd_weather_w', 'sd_weather_font'] },
        { title: 'Tap controls overlay', enabledBy: 'sd_show_ctrl_overlay', fields: ['sd_show_ctrl_overlay'] },
        ...touchHotspotGroups('sd'),
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

// The layout preview can use the active screen's SD wallpaper as its
// background. It is decoded once and kept as a browser-native data URL so
// dragging widgets does not re-fetch or re-decode the .bin on every render.
// Each profile section carries a `<section>_wallpaper` override: "" inherits
// the global default (display.wallpaper_path), "none" forces the gradient.
let wallpaperPreviewUrl = '';
let wallpaperPreviewDim = 0;
let currentWallpaperPath = '';   // effective path for the ACTIVE section
let globalWallpaperPath = '';    // settings display.wallpaper_path
let globalWallpaperOn = false;   // settings display.wallpaper_on
let netWallpaperActive = false;  // device shows a fetched internet wallpaper
let keyboardSelection = null;

function sectionWallpaperKey() {
    return state.active + '_wallpaper';
}

function sectionLabelBgKey() {
    return state.active + '_label_bg_opa';
}

function updateLabelPlateControl() {
    const slider = document.getElementById('layout_label_bg_opa');
    const value = document.getElementById('layout_label_bg_opa_value');
    const opa = clamp(state[state.active][sectionLabelBgKey()] ?? 50, 0, 100);
    if (slider) slider.value = opa;
    if (value) value.textContent = opa + '%';
}

function sectionWallpaperValue() {
    return String(state[state.active][sectionWallpaperKey()] || '');
}

// Mirror of the firmware's resolution in ui_background_apply(): wallpaper_on
// gates every SD wallpaper (overrides included), then the section override
// wins ("none" → no wallpaper), "" falls back to the global default.
function effectiveWallpaperPath() {
    if (!globalWallpaperOn) return '';
    const ovr = sectionWallpaperValue();
    if (ovr === 'none') return '';
    if (ovr) return ovr;
    return globalWallpaperPath;
}

function isKeyboardSelection(fields) {
    return keyboardSelection &&
           keyboardSelection.section === state.active &&
           keyboardSelection.x === fields.x &&
           keyboardSelection.y === fields.y;
}

function showKeyboardSelection(el, fields) {
    if (!isKeyboardSelection(fields)) return;
    el.setAttribute('data-keyboard-selected', 'true');
    el.style.stroke = '#fff';
    el.style.strokeWidth = '1.5px';
    el.style.strokeDasharray = '3 2';
}

function selectForKeyboard(el, fields) {
    // Pointer handlers call preventDefault() to support dragging, which also
    // keeps focus in the last form control. Release it so subsequent arrow
    // keys are delivered to the page and can nudge the selected placeholder.
    const focused = document.activeElement;
    if (focused instanceof HTMLElement && focused !== document.body) focused.blur();

    keyboardSelection = {
        section: state.active,
        x: fields.x,
        y: fields.y,
    };
    for (const selected of document.querySelectorAll('#lcd [data-keyboard-selected]')) {
        selected.removeAttribute('data-keyboard-selected');
        selected.style.removeProperty('stroke');
        selected.style.removeProperty('stroke-width');
        selected.style.removeProperty('stroke-dasharray');
    }
    showKeyboardSelection(el, fields);
}

document.addEventListener('keydown', (e) => {
    const delta = {
        ArrowLeft:  [-1,  0],
        ArrowRight: [ 1,  0],
        ArrowUp:    [ 0, -1],
        ArrowDown:  [ 0,  1],
    }[e.key];
    if (!delta || !keyboardSelection || keyboardSelection.section !== state.active) return;
    if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;

    const target = e.target;
    if (target instanceof HTMLElement &&
        (target.isContentEditable || /^(INPUT|SELECT|TEXTAREA|BUTTON)$/.test(target.tagName))) return;

    const data = state[state.active];
    let moved = false;
    if (delta[0] && keyboardSelection.x !== undefined) {
        data[keyboardSelection.x] = (data[keyboardSelection.x] | 0) + delta[0];
        setFormValue(keyboardSelection.x, data[keyboardSelection.x]);
        moved = true;
    }
    if (delta[1] && keyboardSelection.y !== undefined) {
        data[keyboardSelection.y] = (data[keyboardSelection.y] | 0) + delta[1];
        setFormValue(keyboardSelection.y, data[keyboardSelection.y]);
        moved = true;
    }
    if (!moved) return;
    e.preventDefault();
    renderSvg();
});

function ensureLvBin() {
    if (window.LvBin) return Promise.resolve();
    return new Promise((resolve, reject) => {
        const script = document.createElement('script');
        script.src = 'lvbin.js';
        script.onload = resolve;
        script.onerror = () => reject(new Error('could not load lvbin.js'));
        document.head.appendChild(script);
    });
}

function updateWallpaperPickerLabel() {
    const label = document.getElementById('layout_wallpaper_name');
    if (!label) return;
    const ovr = sectionWallpaperValue();
    if (ovr === 'none') {
        label.textContent = '(none — gradient)';
    } else if (ovr) {
        label.textContent = ovr.split('/').pop();
    } else if (currentWallpaperPath) {
        label.textContent = currentWallpaperPath.split('/').pop() + ' (global)';
    } else {
        label.textContent = globalWallpaperOn ? '(none)' : '(wallpapers off)';
    }
    label.title = currentWallpaperPath;

    // Highlight the mode button matching the section's current state. Inline
    // styles, not the .active class — selectSection() strips .active from
    // every .section-tab while toggling the screen tabs.
    const mode = ovr === 'none' ? 'none' : (ovr ? 'sd' : 'global');
    const modeBtns = {
        sd:     document.getElementById('layout_wp_btn_sd'),
        global: document.getElementById('layout_wp_btn_global'),
        none:   document.getElementById('layout_wp_btn_none'),
    };
    for (const [key, btn] of Object.entries(modeBtns)) {
        if (!btn) continue;
        const on = key === mode;
        btn.style.background  = on ? 'var(--accent)' : '';
        btn.style.color       = on ? '#001019' : '';
        btn.style.borderColor = on ? 'var(--accent)' : '';
    }

    const preset = document.getElementById('layout_preset_name');
    if (preset) preset.textContent = presetPath() || '(select a wallpaper first)';
    updateLabelPlateControl();
}

function buildWallpaperPicker() {
    const frame = document.querySelector('.canvas-card .lcd-frame');
    if (!frame || document.getElementById('layout_wallpaper_picker')) return;

    const picker = document.createElement('div');
    picker.id = 'layout_wallpaper_picker';
    picker.style.cssText =
        'margin-top:10px;padding:9px 10px;border:1px solid var(--border);' +
        'border-radius:var(--radius-sm);background:var(--bg-panel)';

    const row = document.createElement('div');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;flex-wrap:wrap';
    const caption = document.createElement('span');
    caption.textContent = 'Screen wallpaper:';
    caption.style.cssText = 'font-size:11px;color:var(--text-dim)';
    const name = document.createElement('span');
    name.id = 'layout_wallpaper_name';
    name.textContent = 'Loading...';
    name.style.cssText =
        'min-width:0;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;' +
        'font-family:"JetBrains Mono",monospace;font-size:11px';
    // Three-state mode switch for the active screen: SD file override / inherit
    // the global default / no wallpaper. The button matching the section's
    // current mode is highlighted by updateWallpaperPickerLabel().
    const smallBtn = (id, label, title, onClick) => {
        const b = document.createElement('button');
        b.type = 'button';
        b.id = id;
        b.className = 'section-tab';
        b.textContent = label;
        b.title = title;
        b.style.cssText = 'padding:6px 10px';
        b.addEventListener('click', onClick);
        return b;
    };
    const button = smallBtn('layout_wp_btn_sd', 'Choose from SD...',
        'Pick a wallpaper file for this screen', toggleWallpaperBrowser);
    const defaultBtn = smallBtn('layout_wp_btn_global', 'Global',
        'Inherit the global wallpaper (Settings)',
        () => setSectionWallpaper('', 'Screen follows the global wallpaper.'));
    const noneBtn = smallBtn('layout_wp_btn_none', 'None',
        'No wallpaper on this screen (gradient)',
        () => setSectionWallpaper('none', 'Wallpaper disabled for this screen.'));

    // When checked, a wallpaper's saved preset is applied on switch without
    // the confirm() prompt. Sticky across visits via localStorage.
    const autoLabel = document.createElement('label');
    autoLabel.style.cssText =
        'display:flex;align-items:center;gap:5px;font-size:11px;' +
        'color:var(--text-dim);cursor:pointer;white-space:nowrap';
    const autoCheck = document.createElement('input');
    autoCheck.type = 'checkbox';
    autoCheck.id = 'layout_preset_autoload';
    autoCheck.checked = localStorage.getItem('layout_preset_autoload') === '1';
    autoCheck.addEventListener('change', () => {
        localStorage.setItem('layout_preset_autoload', autoCheck.checked ? '1' : '0');
    });
    autoLabel.append(autoCheck, document.createTextNode('Auto-apply preset'));
    row.append(caption, name, button, defaultBtn, noneBtn, autoLabel);

    const plateRow = document.createElement('div');
    plateRow.style.cssText =
        'display:grid;grid-template-columns:auto minmax(120px,1fr) 42px;' +
        'align-items:center;gap:8px;margin-top:8px;padding-top:8px;' +
        'border-top:1px solid var(--border)';
    const plateLabel = document.createElement('label');
    plateLabel.htmlFor = 'layout_label_bg_opa';
    plateLabel.textContent = 'Label plate opacity:';
    plateLabel.style.cssText = 'font-size:11px;color:var(--text-dim)';
    const plateSlider = document.createElement('input');
    plateSlider.type = 'range';
    plateSlider.id = 'layout_label_bg_opa';
    plateSlider.min = '0';
    plateSlider.max = '100';
    plateSlider.step = '5';
    plateSlider.addEventListener('input', () => {
        const opa = clamp(parseInt(plateSlider.value, 10) || 0, 0, 100);
        state[state.active][sectionLabelBgKey()] = opa;
        document.getElementById('layout_label_bg_opa_value').textContent = opa + '%';
        renderSvg();
    });
    const plateValue = document.createElement('span');
    plateValue.id = 'layout_label_bg_opa_value';
    plateValue.style.cssText = 'font-size:11px;text-align:right;color:var(--text-dim)';
    plateRow.append(plateLabel, plateSlider, plateValue);

    const browser = document.createElement('div');
    browser.id = 'layout_wallpaper_browser';
    browser.hidden = true;
    browser.style.marginTop = '8px';

    // Per-wallpaper layout presets — each save merges the active section into
    // the file on SD, leaving layouts saved for the other screens untouched.
    const presetRow = document.createElement('div');
    presetRow.style.cssText =
        'display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin-top:8px;' +
        'padding-top:8px;border-top:1px solid var(--border)';
    const presetCaption = document.createElement('span');
    presetCaption.textContent = 'Layout preset:';
    presetCaption.style.cssText = 'font-size:11px;color:var(--text-dim)';
    const presetName = document.createElement('span');
    presetName.id = 'layout_preset_name';
    presetName.textContent = '...';
    presetName.style.cssText =
        'min-width:0;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;' +
        'font-family:"JetBrains Mono",monospace;font-size:11px';
    const presetBtn = (label, onClick) => {
        const b = document.createElement('button');
        b.type = 'button';
        b.className = 'section-tab';
        b.textContent = label;
        b.style.cssText = 'padding:6px 10px';
        b.addEventListener('click', onClick);
        return b;
    };
    presetRow.append(presetCaption, presetName,
                     presetBtn('Save', savePreset),
                     presetBtn('Load', () => loadPreset()));

    const status = document.createElement('div');
    status.id = 'layout_wallpaper_status';
    status.style.cssText = 'min-height:14px;margin-top:5px;font-size:11px;color:var(--text-dim)';

    picker.append(row, plateRow, browser, presetRow, status);
    frame.insertAdjacentElement('afterend', picker);
    updateLabelPlateControl();
}

// ── Per-wallpaper layout presets on SD ─────────────────────────────────────
// One file per wallpaper: /wallpapers/layouts/<wallpaper-basename>.json with a
// screen-size stamp (same guard as ui_profile.json — a preset saved for
// another LCD is refused on load). Since wallpapers are assigned per screen,
// Save merges only the ACTIVE section into the file and Load applies only the
// active section — switching the SD screen's wallpaper never touches the
// radio/BT layouts. One file can accumulate layouts for several screens.

const LAYOUTS_DIR = '/wallpapers/layouts';

function presetPath() {
    if (!currentWallpaperPath) return '';
    const base = currentWallpaperPath.split('/').pop().replace(/\.bin$/i, '');
    return LAYOUTS_DIR + '/' + base + '.json';
}

function setPresetStatus(msg, error = false) {
    const el = document.getElementById('layout_wallpaper_status');
    if (!el) return;
    el.textContent = msg;
    el.style.color = error ? 'var(--red)' : 'var(--text-dim)';
}

async function savePreset() {
    const path = presetPath();
    if (!path) {
        setPresetStatus('Select a wallpaper first — presets are stored per wallpaper.', true);
        return;
    }
    setPresetStatus('Saving preset...');
    try {
        const preset = {
            w: state.meta.screen_w,
            h: state.meta.screen_h,
            wallpaper: currentWallpaperPath.split('/').pop(),
            sections: {},
        };
        // Merge into the existing file so layouts saved for other screens
        // under the same wallpaper survive.
        try {
            const old = await fetch('/api/sd/file?path=' + encodeURIComponent(path), {
                cache: 'no-store',
            });
            if (old.ok) {
                const parsed = await old.json();
                if (parsed && parsed.sections) preset.sections = parsed.sections;
            }
        } catch { /* no existing preset — start fresh */ }
        // Pin the wallpaper association in the stored copy: an inherited ("")
        // override would re-resolve against whatever the global default is at
        // load time, silently detaching the preset from its wallpaper.
        const sectionCopy = Object.assign({}, state[state.active]);
        sectionCopy[sectionWallpaperKey()] = currentWallpaperPath;
        preset.sections[state.active] = sectionCopy;
        // POST /api/sd/file auto-creates missing parent directories.
        const r = await fetch('/api/sd/file?path=' + encodeURIComponent(path), {
            method: 'POST',
            body: JSON.stringify(preset),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        setPresetStatus(`Preset (${SECTIONS[state.active].title}) saved to SD: ` + path);
    } catch (err) {
        setPresetStatus('Preset save failed: ' + err.message, true);
    }
}

// Fetches the preset for the active screen's wallpaper and applies the ACTIVE
// section to the device (which persists it to ui_profile.json and rebuilds).
async function loadPreset() {
    const path = presetPath();
    if (!path) {
        setPresetStatus('Select a wallpaper first — presets are stored per wallpaper.', true);
        return;
    }
    setPresetStatus('Loading preset...');
    try {
        const r = await fetch('/api/sd/file?path=' + encodeURIComponent(path), {
            cache: 'no-store',
        });
        if (r.status === 404) {
            setPresetStatus('No preset saved for this wallpaper yet.', true);
            return;
        }
        if (!r.ok) throw new Error('HTTP ' + r.status);
        const preset = await r.json();
        if (preset.w !== state.meta.screen_w || preset.h !== state.meta.screen_h) {
            setPresetStatus(
                `Preset was saved for a ${preset.w}×${preset.h} LCD — not applied.`, true);
            return;
        }
        const section = (preset.sections || {})[state.active];
        if (!section) {
            setPresetStatus(
                `Preset has no ${SECTIONS[state.active].title} layout yet — Save one first.`, true);
            return;
        }
        Object.assign(state[state.active], section);
        const post = await fetch(`/api/ui/profile/${state.active}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(state[state.active]),
        });
        if (!post.ok) throw new Error('HTTP ' + post.status);
        buildForm();
        // The preset may itself carry a wallpaper override — refresh the preview.
        await loadWallpaperPreview();
        renderSvg();
        setPresetStatus(`Preset (${SECTIONS[state.active].title}) applied — device screen rebuilding.`);
    } catch (err) {
        setPresetStatus('Preset load failed: ' + err.message, true);
    }
}

// After switching a screen's wallpaper, offer to apply its saved layout for
// this screen (if one exists) so wallpaper + matching layout travel together.
async function offerPresetForWallpaper() {
    const path = presetPath();
    if (!path) return;
    try {
        const r = await fetch('/api/sd/file?path=' + encodeURIComponent(path), {
            cache: 'no-store',
        });
        if (!r.ok) return;
        const preset = await r.json();
        if (!preset.sections || !preset.sections[state.active]) return;
        const auto = document.getElementById('layout_preset_autoload');
        if ((auto && auto.checked) ||
            confirm('A saved layout preset exists for this wallpaper. Apply it?')) {
            await loadPreset();
        }
    } catch {
        // No preset / no SD — nothing to offer.
    }
}

// ── Orphaned layout presets (Presets tab) ───────────────────────────────────
// Presets become orphans when their wallpaper .bin is deleted or renamed.
// A wallpaper can live anywhere on the card, so name matching against
// /wallpapers alone would flag false orphans — instead each preset is opened
// and the full paths it stores in its <section>_wallpaper fields are checked.
// Only when none of the referenced files exist is the preset an orphan.
const SD_MOUNT = '/sdcard';

// Existence checks share one /api/sd/list request per directory.
function sdDirFiles(dir, cache) {
    if (!cache.has(dir)) {
        cache.set(dir, fetch('/api/sd/list?path=' + encodeURIComponent(dir))
            .then(r => r.ok ? r.json() : { entries: [] })
            .then(d => new Set((d.entries || []).filter(e => !e.dir).map(e => e.name)))
            .catch(() => new Set()));
    }
    return cache.get(dir);
}

async function sdFileExists(relPath, cache) {
    const dir = relPath.replace(/\/[^/]+$/, '') || '/';
    return (await sdDirFiles(dir, cache)).has(relPath.split('/').pop());
}

// Walks a preset's JSON and collects every "/sdcard/..." string stored under a
// *_wallpaper key ("" and "none" fall through the startsWith test).
function collectWallpaperRefs(node, out = []) {
    if (Array.isArray(node)) { node.forEach(v => collectWallpaperRefs(v, out)); return out; }
    if (node && typeof node === 'object') {
        for (const [k, v] of Object.entries(node)) {
            if (k.endsWith('_wallpaper') && typeof v === 'string' &&
                v.startsWith(SD_MOUNT + '/')) {
                if (!out.includes(v)) out.push(v);
            } else {
                collectWallpaperRefs(v, out);
            }
        }
    }
    return out;
}

async function checkOrphanPresets() {
    const status = document.getElementById('presetOrphanStatus');
    document.getElementById('presetOrphanList').innerHTML = '';
    status.textContent = 'Scanning…';
    try {
        const r = await fetch('/api/sd/list?path=' + encodeURIComponent(LAYOUTS_DIR),
                              { cache: 'no-store' });
        if (!r.ok) {
            status.textContent = 'No presets found (SD card or ' + LAYOUTS_DIR +
                                 ' not available).';
            return;
        }
        const d = await r.json();
        const jsons = (d.entries || []).filter(e => !e.dir && /\.json$/i.test(e.name));
        if (!jsons.length) { status.textContent = 'No preset files found.'; return; }

        const cache = new Map();
        const orphans = [];
        let okCount = 0;
        for (const e of jsons) {
            const rel = LAYOUTS_DIR + '/' + e.name;
            let refs = [];
            try {
                const jr = await fetch('/api/sd/file?path=' + encodeURIComponent(rel),
                                       { cache: 'no-store' });
                if (jr.ok) refs = collectWallpaperRefs(await jr.json());
            } catch (_) { /* unreadable/invalid JSON — fall back to name matching */ }
            // Old presets may predate the stored full paths — assume the two
            // standard wallpaper locations for <stem>.bin.
            if (!refs.length) {
                const stem = e.name.replace(/\.json$/i, '');
                refs = [SD_MOUNT + '/wallpapers/' + stem + '.bin',
                        SD_MOUNT + '/wallpapers/saved/' + stem + '.bin'];
            }
            const found = await Promise.all(
                refs.map(p => sdFileExists(p.slice(SD_MOUNT.length), cache)));
            if (found.some(x => x)) okCount++;
            else orphans.push({ name: e.name, rel, refs });
        }
        renderOrphanList(orphans, okCount);
    } catch (err) {
        status.textContent = 'Scan failed: ' + err.message;
    }
}

function renderOrphanList(orphans, okCount) {
    const status = document.getElementById('presetOrphanStatus');
    const list = document.getElementById('presetOrphanList');
    list.innerHTML = '';
    if (!orphans.length) {
        status.textContent = '✓ No orphans — all ' + okCount +
                             ' preset(s) have their wallpaper.';
        return;
    }
    status.textContent = orphans.length + ' orphan(s) found, ' + okCount + ' preset(s) OK.';

    orphans.forEach(o => {
        const row = document.createElement('div');
        row.style.cssText = 'display:flex;align-items:center;gap:8px;padding:3px 0';
        const name = document.createElement('div');
        name.style.cssText = 'flex:1;font-family:monospace;font-size:12px;min-width:0';
        const title = document.createElement('div');
        title.textContent = '📄 ' + o.name;
        const missing = document.createElement('div');
        missing.style.cssText = 'opacity:.6;overflow-wrap:anywhere';
        missing.textContent = 'missing: ' + o.refs.join(', ');
        name.append(title, missing);
        const del = document.createElement('button');
        del.type = 'button';
        del.className = 'btn-secondary';
        del.textContent = '🗑 Delete';
        del.onclick = async () => {
            del.disabled = true;
            if (await deleteOrphanPreset(o)) row.remove();
            else del.disabled = false;
        };
        row.append(name, del);
        list.appendChild(row);
    });

    if (orphans.length > 1) {
        const all = document.createElement('button');
        all.type = 'button';
        all.className = 'btn-secondary';
        all.textContent = '🗑 Delete all orphans';
        all.style.marginTop = '6px';
        all.onclick = async () => {
            if (!confirm('Delete ' + orphans.length + ' orphaned preset file(s)?')) return;
            for (const o of orphans) await deleteOrphanPreset(o);
            checkOrphanPresets();   // re-scan to show the result
        };
        list.appendChild(all);
    }
}

async function deleteOrphanPreset(o) {
    try {
        const r = await fetch('/api/sd/file?path=' + encodeURIComponent(o.rel),
                              { method: 'DELETE' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        return true;
    } catch (err) {
        document.getElementById('presetOrphanStatus').textContent =
            'Delete of ' + o.name + ' failed: ' + err.message;
        return false;
    }
}

function wallpaperDirectory() {
    if (!currentWallpaperPath.startsWith('/sdcard/')) return '/wallpapers';
    const rel = currentWallpaperPath.slice('/sdcard'.length);
    return rel.replace(/\/[^/]+$/, '') || '/';
}

async function toggleWallpaperBrowser() {
    const browser = document.getElementById('layout_wallpaper_browser');
    if (!browser) return;
    if (!browser.hidden) {
        browser.hidden = true;
        return;
    }
    browser.hidden = false;
    await browseWallpaperDirectory(wallpaperDirectory());
}

async function browseWallpaperDirectory(path) {
    const browser = document.getElementById('layout_wallpaper_browser');
    if (!browser) return;
    browser.textContent = 'Loading...';
    try {
        const response = await fetch('/api/sd/list?path=' + encodeURIComponent(path), {
            cache: 'no-store',
        });
        if (!response.ok) throw new Error('HTTP ' + response.status);
        renderWallpaperDirectory(await response.json());
    } catch (err) {
        // Fresh cards may not have a /wallpapers directory yet; keep the
        // picker usable by falling back to the SD root in that case.
        if (path === '/wallpapers') {
            await browseWallpaperDirectory('/');
            return;
        }
        browser.textContent = 'SD folder unavailable: ' + err.message;
    }
}

function renderWallpaperDirectory(data) {
    const browser = document.getElementById('layout_wallpaper_browser');
    if (!browser) return;
    browser.innerHTML = '';
    const path = data.path || '/';

    const heading = document.createElement('div');
    heading.textContent = path;
    heading.style.cssText = 'margin-bottom:4px;font-family:monospace;font-size:11px;opacity:.75';
    const list = document.createElement('div');
    list.style.cssText =
        'max-height:190px;overflow:auto;border:1px solid var(--border);' +
        'border-radius:var(--radius-sm);background:var(--bg-card)';

    const addRow = (label, onClick) => {
        const row = document.createElement('div');
        row.textContent = label;
        row.style.cssText = 'padding:6px 9px;cursor:pointer;font-size:11px';
        row.onmouseenter = () => { row.style.background = 'var(--bg-input)'; };
        row.onmouseleave = () => { row.style.background = ''; };
        row.addEventListener('click', onClick);
        list.appendChild(row);
    };

    if (path !== '/') {
        const parent = path.replace(/\/[^/]+\/?$/, '') || '/';
        addRow('\u{1F4C1} ..', () => browseWallpaperDirectory(parent));
    }

    const entries = (data.entries || []).slice().sort((a, b) =>
        (!!b.dir - !!a.dir) ||
        a.name.localeCompare(b.name, undefined, { numeric: true, sensitivity: 'base' }));
    let fileCount = 0;
    for (const entry of entries) {
        const full = (path.endsWith('/') ? path : path + '/') + entry.name;
        if (entry.dir) {
            addRow('\u{1F4C1} ' + entry.name, () => browseWallpaperDirectory(full));
        } else if (entry.name.toLowerCase().endsWith('.bin')) {
            fileCount++;
            const active = currentWallpaperPath === '/sdcard' + full;
            addRow((active ? '\u2713 ' : '\u{1F5BC}\u{FE0F} ') + entry.name,
                   () => selectWallpaper(full));
        }
    }
    if (!fileCount && !entries.some(entry => entry.dir)) {
        const empty = document.createElement('div');
        empty.textContent = 'No .bin wallpapers in this folder.';
        empty.style.cssText = 'padding:7px 9px;font-size:11px;color:var(--text-dim)';
        list.appendChild(empty);
    }
    browser.append(heading, list);
}

// Store `value` ("", "none" or an fopen path) as the active section's
// wallpaper override and push the section to the device (persists to
// ui_profile.json and rebuilds the screen).
async function setSectionWallpaper(value, doneMsg) {
    const status = document.getElementById('layout_wallpaper_status');
    if (status) status.textContent = 'Applying wallpaper...';
    try {
        state[state.active][sectionWallpaperKey()] = value;
        const r = await fetch(`/api/ui/profile/${state.active}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(state[state.active]),
        });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        await loadWallpaperPreview();
        renderSvg();
        const browser = document.getElementById('layout_wallpaper_browser');
        if (browser) browser.hidden = true;
        if (status) status.textContent = doneMsg;
        return true;
    } catch (err) {
        if (status) status.textContent = 'Wallpaper change failed: ' + err.message;
        return false;
    }
}

async function selectWallpaper(relPath) {
    const fullPath = '/sdcard' + (relPath.startsWith('/') ? relPath : '/' + relPath);
    // The firmware renders SD wallpapers only when the global feature switch
    // is on — flip it FIRST, so the preview/preset logic below already sees
    // the effective path (with the switch off it resolves to "no wallpaper").
    if (!globalWallpaperOn) {
        const status = document.getElementById('layout_wallpaper_status');
        try {
            const r = await fetch('/api/settings', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ display: { wallpaper_on: true } }),
            });
            if (!r.ok) throw new Error('HTTP ' + r.status);
            globalWallpaperOn = true;
        } catch (err) {
            if (status) status.textContent =
                'Could not enable wallpapers globally: ' + err.message;
            return;
        }
    }
    if (!await setSectionWallpaper(fullPath, 'Wallpaper changed.')) return;
    await offerPresetForWallpaper();
}

async function loadWallpaperPreview() {
    wallpaperPreviewUrl = '';
    wallpaperPreviewDim = 0;
    netWallpaperActive = false;
    const section = state.active;
    try {
        const response = await fetch('/api/settings', { cache: 'no-store' });
        if (!response.ok) throw new Error('settings HTTP ' + response.status);
        const settings = await response.json();
        const display = settings.display || {};
        globalWallpaperPath = String(display.wallpaper_path || '');
        globalWallpaperOn = !!display.wallpaper_on;
        const path = effectiveWallpaperPath();
        currentWallpaperPath = path;
        updateWallpaperPickerLabel();

        // A fetched internet wallpaper replaces the inherited tier on the
        // device (until reboot or dismissal). Its pixels aren't available to
        // the editor, so show a "net wallpaper" placeholder instead of the
        // wrong SD file. Screens with their own override are unaffected.
        const ovr = sectionWallpaperValue();
        if (!ovr || !globalWallpaperOn) {
            try {
                const st = await fetch('/api/wallpaper/status', { cache: 'no-store' });
                const info = st.ok ? await st.json() : null;
                if (info && info.active && state.active === section) {
                    netWallpaperActive = true;
                    const label = document.getElementById('layout_wallpaper_name');
                    if (label) label.textContent = '(net wallpaper — until reboot)';
                    return;
                }
            } catch { /* status unavailable — fall through to the SD preview */ }
        }

        if (!path) return;

        // Settings store the fopen-ready "/sdcard/..." path, while the SD file
        // endpoint accepts a mount-relative path such as "/wallpapers/a.bin".
        const relPath = path.startsWith('/sdcard/') ? path.slice('/sdcard'.length) : path;
        const file = await fetch('/api/sd/file?path=' + encodeURIComponent(relPath), {
            cache: 'no-store',
        });
        if (!file.ok) throw new Error('wallpaper HTTP ' + file.status);

        await ensureLvBin();
        const decoded = window.LvBin.decodeToCanvas(await file.arrayBuffer());
        // Guard against a stale async result: the user may have switched tabs
        // (a different effective wallpaper) while the .bin was downloading.
        if (effectiveWallpaperPath() !== path) return;
        wallpaperPreviewUrl = decoded.canvas.toDataURL('image/png');
        wallpaperPreviewDim = clamp(display.wallpaper_dim || 0, 0, 100);
    } catch (err) {
        // The preview is optional: an absent SD card or stale path must not
        // prevent the profile editor from loading and applying layouts.
        console.warn('Wallpaper preview unavailable:', err);
    }
}

// ── Bootstrap ───────────────────────────────────────────────────────────────

window.addEventListener('DOMContentLoaded', async () => {
    buildWallpaperPicker();
    try {
        const meta = await fetch('/api/ui/profile/meta').then(r => r.json());
        state.meta = meta;
        document.getElementById('screen_dim').textContent =
            `${meta.screen_w} × ${meta.screen_h}`;

        // Pre-fetch every section so switching tabs is instant
        for (const name of Object.keys(SECTIONS)) {
            state[name] = await fetch(`/api/ui/profile/${name}`).then(r => r.json());
        }

        selectSection('clock');   // triggers the wallpaper preview load
    } catch (err) {
        setStatus('Failed to load profile: ' + err.message, true);
    }
});

function selectSection(name) {
    // 'presets' is not a screen — it swaps the editor grid for the
    // preset-housekeeping card and leaves state.active untouched, so returning
    // to any screen tab restores the editor exactly where it was.
    const isPresets = name === 'presets';
    if (!isPresets && !SECTIONS[name]) return;

    for (const tab of document.querySelectorAll('.section-tab')) {
        tab.classList.toggle('active', tab.dataset.section === name);
    }
    document.querySelector('.layout-grid').style.display = isPresets ? 'none' : '';
    document.getElementById('presets_card').style.display = isPresets ? '' : 'none';
    if (isPresets) { checkOrphanPresets(); return; }

    if (state.active !== name) keyboardSelection = null;
    state.active = name;

    document.getElementById('form_section_title').textContent = SECTIONS[name].title;
    buildForm();
    updateLabelPlateControl();
    renderSvg();
    // Wallpapers are per screen — refresh the preview for this tab, then
    // repaint once the (async) decode lands.
    loadWallpaperPreview().then(renderSvg);
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
        if (group.subgroups) {
            group.subgroups.forEach((subgroup, subgroupIndex) => {
                const subDetails = document.createElement('details');
                subDetails.className = 'form-subgroup';
                subDetails.dataset.groupIndex = groupIndex;
                subDetails.dataset.subgroupIndex = subgroupIndex;

                const subSummary = document.createElement('summary');
                const subTitle = document.createElement('span');
                subTitle.className = 'form-group-title';
                subTitle.textContent = subgroup.title;
                const subMeta = document.createElement('span');
                subMeta.className = 'form-group-meta';
                subSummary.append(subTitle, subMeta);
                subDetails.appendChild(subSummary);

                const subBody = document.createElement('div');
                subBody.className = 'form-subgroup-body';
                for (const key of subgroup.fields) {
                    const field = fieldByKey.get(key);
                    if (!field) continue;
                    subBody.appendChild(buildFormRow(field, data, subgroup, subDetails));
                }
                subDetails.appendChild(subBody);
                body.appendChild(subDetails);
                refreshGroup(subDetails, subgroup, data);
            });
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
    const body = details.children[1];
    if (body) {
        for (const row of body.children) {
            if (row.classList.contains('form-row-conditional')) row.hidden = !enabled;
        }
    }
    const meta = details.firstElementChild?.querySelector('.form-group-meta');
    if (meta) meta.textContent = groupSummary(group, data);

    if (details.classList.contains('form-subgroup')) {
        const parent = details.closest('.form-group');
        if (parent) {
            const parentGroup = FORM_GROUPS[state.active][Number(parent.dataset.groupIndex)];
            const parentMeta = parent.firstElementChild?.querySelector('.form-group-meta');
            if (parentMeta) parentMeta.textContent = groupSummary(parentGroup, data);
        }
    }
}

function groupSummary(group, data) {
    if (group.enabledBy && !data[group.enabledBy]) return 'Off';
    if (group.subgroups) {
        const enabled = group.subgroups.filter(s => !!data[s.enabledBy]).length;
        return `${enabled}/${group.subgroups.length} enabled`;
    }

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
    const details = el.closest('.form-subgroup, .form-group');
    if (details) {
        const parentGroup = FORM_GROUPS[state.active][Number(details.dataset.groupIndex)];
        const subgroupIndex = details.dataset.subgroupIndex;
        const group = subgroupIndex === undefined
            ? parentGroup
            : parentGroup.subgroups[Number(subgroupIndex)];
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
    if (wallpaperPreviewUrl) {
        const image = document.createElementNS(SVG_NS, 'image');
        image.setAttribute('x', 0);
        image.setAttribute('y', 0);
        image.setAttribute('width', W);
        image.setAttribute('height', H);
        image.setAttribute('preserveAspectRatio', 'none');
        image.setAttribute('href', wallpaperPreviewUrl);
        image.style.pointerEvents = 'none';
        svg.appendChild(image);
        if (wallpaperPreviewDim > 0) {
            const shade = rect(svg, { x: 0, y: 0, width: W, height: H });
            shade.setAttribute('fill', '#000');
            shade.setAttribute('fill-opacity', wallpaperPreviewDim / 100);
            shade.style.pointerEvents = 'none';
        }
    } else if (netWallpaperActive) {
        // The device shows an internet-fetched wallpaper we can't preview —
        // say so instead of leaving a silently misleading blank/SD background.
        const t = text(svg, W / 2, H / 2, 'net wallpaper', {
            'font-size': Math.max(12, Math.round(H / 10)),
            'text-anchor': 'middle',
            opacity: 0.4,
        });
        t.style.pointerEvents = 'none';
    }
    SECTIONS[state.active].renderer(svg);
}

// ── CLOCK renderer ──────────────────────────────────────────────────────────

function renderClock(svg) {
    const c = state.clock;
    const W = state.meta.screen_w;

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

    const sx = c.clock_strip_x, sy = c.clock_strip_y;
    const sw = c.clock_strip_w, sh = c.clock_strip_h;
    if (c.clock_show_strip) {
        drawFreeElement(svg, {
            x: sx, y: sy, w: sw, h: sh,
            label: 'strip', cls: 'panel',
            fillOpacity: clamp(c.clock_strip_bg_opa ?? 100, 0, 100) / 100,
            fields: { x: 'clock_strip_x', y: 'clock_strip_y',
                      w: 'clock_strip_w', h: 'clock_strip_h' },
        });
    }

    // Labels are anchored to the strip's top-centre (LV_ALIGN_TOP_MID in the
    // firmware), so x/y/w fields are offsets/width around that anchor; the
    // drag/resize handlers work on deltas, which suits offset fields as-is.
    // Old presets carry the legacy shared clock_strip_label_w.
    const stFh = fontHeight(c.clock_strip_station_font);
    const stW  = c.clock_strip_station_w ?? c.clock_strip_label_w ?? sw;
    drawFreeElement(svg, {
        x: sx + (sw - stW) / 2 + (c.clock_strip_station_x | 0),
        y: sy + c.clock_strip_station_y, w: stW, h: stFh,
        label: 'station', cls: 'label-rect',
        fields: { x: 'clock_strip_station_x', y: 'clock_strip_station_y',
                  w: 'clock_strip_station_w' },
        text: 'Atlas Radio', textSize: stFh,
    });
    const tiFh = fontHeight(c.clock_strip_title_font);
    const tiW  = c.clock_strip_title_w ?? c.clock_strip_label_w ?? sw;
    drawFreeElement(svg, {
        x: sx + (sw - tiW) / 2 + (c.clock_strip_title_x | 0),
        y: sy + c.clock_strip_title_y, w: tiW, h: tiFh,
        label: 'title', cls: 'label-rect',
        fields: { x: 'clock_strip_title_x', y: 'clock_strip_title_y',
                  w: 'clock_strip_title_w' },
        text: 'Song title', textSize: tiFh,
    });
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
        if (r.radio_show_wheel_left) {
            drawAnimatedWheel(svg, r.radio_cassette_l_x, r.radio_cassette_l_y,
                             r.radio_cassette_l_size, 'wheel L',
                             'radio_cassette_l_x', 'radio_cassette_l_y', 'radio_cassette_l_size');
        }
        if (r.radio_show_wheel_right) {
            drawAnimatedWheel(svg, r.radio_cassette_r_x, r.radio_cassette_r_y,
                             r.radio_cassette_r_size, 'wheel R',
                             'radio_cassette_r_x', 'radio_cassette_r_y', 'radio_cassette_r_size');
        }
    }

    if (r.radio_show_np) {
        // Now-playing widget = two stacked labels (station + title). Width is fixed
        // in firmware to screen_w - 20 (full-screen scrolling line); the title sits
        // a station-font-height + 4px below the station (mirrors the firmware).
        const stationFh = fontHeight(r.radio_np_station_font);
        const titleFh   = fontHeight(r.radio_np_title_font);
        const npW       = Math.max(W - r.radio_np_x - 10, 8);
        drawFreeElement(svg, {
            x: r.radio_np_x, y: r.radio_np_y, w: npW, h: stationFh,
            label: 'np_station', cls: 'label-rect',
            fields: { x: 'radio_np_x', y: 'radio_np_y' },
            text: 'Atlas Radio', textSize: stationFh,
        });
        if (r.radio_show_np_title) {
            drawFreeElement(svg, {
                x: r.radio_np_x, y: r.radio_np_y + stationFh + 4, w: npW, h: titleFh,
                label: 'np_title', cls: 'label-rect',
                fields: { x: 'radio_np_x', y: 'radio_np_y' },
                text: 'Title — Artist', textSize: titleFh,
            });
        }
    }

    if (r.radio_show_station_icon) {
        drawFreeElement(svg, {
            x: r.radio_station_icon_x, y: r.radio_station_icon_y,
            w: r.radio_station_icon_size, h: r.radio_station_icon_size,
            label: 'station icon', cls: 'panel',
            fields: { x: 'radio_station_icon_x', y: 'radio_station_icon_y',
                      w: 'radio_station_icon_size', h: 'radio_station_icon_size' },
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
            fields: { x: 'radio_vu_x', y: 'radio_vu_y',
                      w: 'radio_vu_w', h: 'radio_vu_h' },
        });
    }
    if (r.radio_needle_show_l) {
        drawFreeElement(svg, {
            x: r.radio_needle_l_x, y: r.radio_needle_l_y,
            w: r.radio_needle_l_w, h: r.radio_needle_l_h,
            label: 'VU-L', cls: 'label-rect',
            fields: { x: 'radio_needle_l_x', y: 'radio_needle_l_y',
                      w: 'radio_needle_l_w', h: 'radio_needle_l_h' },
        });
    }
    if (r.radio_needle_show_r) {
        drawFreeElement(svg, {
            x: r.radio_needle_r_x, y: r.radio_needle_r_y,
            w: r.radio_needle_r_w, h: r.radio_needle_r_h,
            label: 'VU-R', cls: 'label-rect',
            fields: { x: 'radio_needle_r_x', y: 'radio_needle_r_y',
                      w: 'radio_needle_r_w', h: 'radio_needle_r_h' },
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
    drawTouchHotspots(svg, 'radio', r);
}

// ── SD PLAYER renderer ───────────────────────────────────────────────────────

function renderSd(svg) {
    const s = state.sd;

    if (s.sd_show_cassette) {
        if (s.sd_show_wheel_left) {
            drawAnimatedWheel(svg, s.sd_cassette_l_x, s.sd_cassette_l_y,
                             s.sd_cassette_l_size, 'wheel L',
                             'sd_cassette_l_x', 'sd_cassette_l_y', 'sd_cassette_l_size');
        }
        if (s.sd_show_wheel_right) {
            drawAnimatedWheel(svg, s.sd_cassette_r_x, s.sd_cassette_r_y,
                             s.sd_cassette_r_size, 'wheel R',
                             'sd_cassette_r_x', 'sd_cassette_r_y', 'sd_cassette_r_size');
        }
    }

    // Title — fixed-width box (like bt_title), text centered inside.
    const sdTitleFh = fontHeight(s.sd_title_font);
    drawFreeElement(svg, {
        x: s.sd_title_x, y: s.sd_title_y, w: s.sd_title_w, h: sdTitleFh,
        label: 'title', cls: 'label-rect',
        fields: { x: 'sd_title_x', y: 'sd_title_y', w: 'sd_title_w' },
        text: 'Artist - Title', textSize: sdTitleFh,
    });
    if (s.sd_show_folder) {
        drawLabel(svg, 0, s.sd_folder_y, s.sd_folder_font, 'Folder   3/12',
                  'folder', { y: 'sd_folder_y' });
    }
    if (s.sd_show_info) {
        drawLabel(svg, 0, s.sd_info_y, s.sd_info_font, 'VOL: 42%   SHUFFLE   REPEAT ALL',
                  'info', { y: 'sd_info_y' });
    }

    if (s.sd_show_time) {
        drawLabel(svg, 0, s.sd_time_y, s.sd_info_font, '1:23 / 4:56',
                  'time', { y: 'sd_time_y' });
    }

    if (s.sd_show_bar && s.sd_bar_w > 0) {
        drawFreeElement(svg, {
            x: s.sd_bar_x, y: s.sd_bar_y, w: s.sd_bar_w, h: s.sd_bar_h,
            label: 'bar', cls: 'label-rect',
            fields: { x: 'sd_bar_x', y: 'sd_bar_y', w: 'sd_bar_w', h: 'sd_bar_h' },
        });
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
            fields: { x: 'sd_vu_x', y: 'sd_vu_y',
                      w: 'sd_vu_w', h: 'sd_vu_h' },
        });
    }
    if (s.sd_needle_show_l) {
        drawFreeElement(svg, {
            x: s.sd_needle_l_x, y: s.sd_needle_l_y,
            w: s.sd_needle_l_w, h: s.sd_needle_l_h,
            label: 'VU-L', cls: 'label-rect',
            fields: { x: 'sd_needle_l_x', y: 'sd_needle_l_y',
                      w: 'sd_needle_l_w', h: 'sd_needle_l_h' },
        });
    }
    if (s.sd_needle_show_r) {
        drawFreeElement(svg, {
            x: s.sd_needle_r_x, y: s.sd_needle_r_y,
            w: s.sd_needle_r_w, h: s.sd_needle_r_h,
            label: 'VU-R', cls: 'label-rect',
            fields: { x: 'sd_needle_r_x', y: 'sd_needle_r_y',
                      w: 'sd_needle_r_w', h: 'sd_needle_r_h' },
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
    drawTouchHotspots(svg, 'sd', s);
}

function drawTouchHotspots(svg, prefix, data) {
    for (let i = 1; i <= 6; i++) {
        const key = `${prefix}_hotspot_${i}`;
        if (!data[`${key}_enabled`]) continue;
        const action = HOTSPOT_ACTIONS.find(a => a.value === (data[`${key}_action`] | 0));
        const w = Math.max(8, data[`${key}_w`] | 0);
        const h = Math.max(8, data[`${key}_h`] | 0);
        drawFreeElement(svg, {
            x: data[`${key}_x`] | 0, y: data[`${key}_y`] | 0, w, h,
            label: `touch ${i}: ${action ? action.label : '?'}`,
            cls: 'label-rect', radius: Math.min(w, h) * clamp(data[`${key}_radius`] | 0, 0, 100) / 200,
            fields: { x: `${key}_x`, y: `${key}_y`, w: `${key}_w`, h: `${key}_h` },
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
    if (opts.fillOpacity !== undefined) {
        r.style.fillOpacity = opts.fillOpacity;
    } else if (opts.text) {
        // Floating labels use the active screen's configurable background plate.
        // Keep the editor's placeholder colour, but scale it so the opacity
        // slider has an immediate and truthful effect in Live preview.
        r.style.fillOpacity = clamp(
            state[state.active][sectionLabelBgKey()] ?? 50, 0, 100) / 100;
    }
    if (opts.radius !== undefined) {
        r.setAttribute('rx', opts.radius);
        r.setAttribute('ry', opts.radius);
    }
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

    // Corner resize whenever a width field exists; without a height field the
    // corners resize width only (height follows the font).
    if (opts.fields.w) {
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
    showKeyboardSelection(el, fields);
    el.addEventListener('pointerdown', (e) => {
        if (e.target !== el) return;
        e.preventDefault();
        selectForKeyboard(el, fields);
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
        selectForKeyboard(el, fields);
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
            if (fields.h) {
                if (dir.includes('t')) { ny = start.y + dy; nh = start.h - dy; }
                if (dir.includes('b')) {                     nh = start.h + dy; }
            }
            if (nw < 4) { nw = 4; if (dir.includes('l')) nx = start.x + start.w - 4; }
            if (fields.h && nh < 4) { nh = 4; if (dir.includes('t')) ny = start.y + start.h - 4; }
            }

            data[fields.x] = nx; data[fields.y] = ny;
            data[fields.w] = nw;
            if (fields.h && fields.h !== fields.w) data[fields.h] = nh;
            setFormValue(fields.x, nx); setFormValue(fields.y, ny);
            setFormValue(fields.w, nw);
            if (fields.h && fields.h !== fields.w) setFormValue(fields.h, nh);
            renderSvg();
        };
        const onUp = () => {
            window.removeEventListener('pointermove', onMove);
            window.removeEventListener('pointerup', onUp);
            // A handle click without an actual resize should still leave the
            // owning element visibly selected for keyboard nudging.
            renderSvg();
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
        // Reset also clears per-screen wallpaper overrides — refresh the preview.
        await loadWallpaperPreview();
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
    if (name && name.startsWith('touch ')) return 'ph-hotspot';
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
        VU: 'ph-vu', 'VU-L': 'ph-vu', 'VU-R': 'ph-vu',
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
