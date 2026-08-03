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
    { value: 9, label: 'Open equalizer' },
];

// Must match UI_TOUCH_HOTSPOT_COUNT in ui_profile.h.
const HOTSPOT_COUNT = 8;

function touchHotspotFields(prefix) {
    const fields = [];
    for (let i = 1; i <= HOTSPOT_COUNT; i++) {
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
    for (let i = 1; i <= HOTSPOT_COUNT; i++) {
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
    { key: 'clock_time_color', label: 'Time colour',  type: 'color' },

    { key: 'clock_show_date', label: 'Show date',     type: 'bool' },
    { key: 'clock_date_x',    label: 'Date X',        type: 'number' },
    { key: 'clock_date_y',    label: 'Date Y',        type: 'number' },
    { key: 'clock_date_font', label: 'Date font',     type: 'font'   },
    { key: 'clock_date_color', label: 'Date colour',  type: 'color'  },

    { key: 'clock_show_netinfo', label: 'Show IP/host', type: 'bool'   },
    { key: 'clock_netinfo_x',    label: 'IP/host X',    type: 'number' },
    { key: 'clock_netinfo_y',    label: 'IP/host Y',    type: 'number' },
    { key: 'clock_netinfo_font', label: 'IP/host font', type: 'font'   },
    { key: 'clock_netinfo_color', label: 'IP/host colour', type: 'color' },

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
    { key: 'clock_strip_station_color',   label: 'Station colour', type: 'color'  },
    { key: 'clock_strip_title_color',     label: 'Title colour',   type: 'color'  },

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
    { key: 'clock_weather_w', label: 'Weather W (0 = full width)', type: 'number' },
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
    { key: 'bt_brand_color', label: 'Brand colour',    type: 'color'  },

    { key: 'bt_status_x',    label: 'Status X',        type: 'number' },
    { key: 'bt_status_y',    label: 'Status Y',        type: 'number' },
    { key: 'bt_status_font', label: 'Status font',     type: 'font'   },
    { key: 'bt_status_color', label: 'Status colour',  type: 'color'  },

    { key: 'bt_title_x',     label: 'Title X',         type: 'number' },
    { key: 'bt_title_y',     label: 'Title Y',         type: 'number' },
    { key: 'bt_title_w',     label: 'Title W',         type: 'number' },
    { key: 'bt_title_font',  label: 'Title font',      type: 'font'   },
    { key: 'bt_title_color', label: 'Title colour',    type: 'color'  },

    { key: 'bt_artist_x',    label: 'Artist X',        type: 'number' },
    { key: 'bt_artist_y',    label: 'Artist Y',        type: 'number' },
    { key: 'bt_artist_w',    label: 'Artist W',        type: 'number' },
    { key: 'bt_artist_font', label: 'Artist font',     type: 'font'   },
    { key: 'bt_artist_color', label: 'Artist colour',  type: 'color'  },

    { key: 'bt_time_x',      label: 'Time X',          type: 'number' },
    { key: 'bt_time_y',      label: 'Time Y',          type: 'number' },
    { key: 'bt_time_font',   label: 'Time font',       type: 'font'   },
    { key: 'bt_time_color',  label: 'Time colour',     type: 'color'  },

    { key: 'bt_vol_x',           label: 'Vol X',          type: 'number' },
    { key: 'bt_vol_y',           label: 'Vol Y',          type: 'number' },
    { key: 'bt_vol_label_font',  label: 'Vol label font', type: 'font'   },
    { key: 'bt_vol_color',       label: 'Vol colour',     type: 'color'  },

    { key: 'bt_show_mode_indicator', label: 'Show mode indic.', type: 'bool' },
    { key: 'bt_mode_indic_x',        label: 'Mode indic. X',    type: 'number' },
    { key: 'bt_mode_indic_y',        label: 'Mode indic. Y',    type: 'number' },
    { key: 'bt_show_clock',          label: 'Show clock',       type: 'bool' },
    { key: 'bt_clock_widget_x',      label: 'Clock X',          type: 'number' },
    { key: 'bt_clock_widget_y',      label: 'Clock Y',          type: 'number' },
    { key: 'bt_clock_font',          label: 'Clock font',       type: 'font'   },
    { key: 'bt_volslider_show',       label: 'Show volume slider', type: 'bool' },
    { key: 'bt_volslider_vertical',   label: 'Vertical',           type: 'bool' },
    { key: 'bt_volslider_knob_only',  label: 'Knob only (no track)', type: 'bool' },
    { key: 'bt_volslider_x',          label: 'Slider X',           type: 'number' },
    { key: 'bt_volslider_y',          label: 'Slider Y',           type: 'number' },
    { key: 'bt_volslider_w',          label: 'Slider W',           type: 'number', min: 8, max: 480 },
    { key: 'bt_volslider_h',          label: 'Slider H',           type: 'number', min: 8, max: 480 },
    { key: 'bt_volslider_knob_image', label: 'Knob image (.bin)',  type: 'text', placeholder: '/sdcard/assets/knobs/... (empty = colour knob)', sdPicker: { dir: '/assets/knobs' } },
    { key: 'bt_volslider_vol_max',    label: 'Max volume (%)',     type: 'number', min: 1, max: 100 },
    { key: 'bt_show_ctrl_overlay', label: 'Show tap controls overlay', type: 'bool', default: true },
    ...touchHotspotFields('bt'),
];

const RADIO_FIELDS = [
    { key: 'radio_show_np',           label: 'Show station name', type: 'bool' },
    { key: 'radio_np_x',              label: 'Station X',        type: 'number' },
    { key: 'radio_np_y',              label: 'Station Y',        type: 'number' },
    { key: 'radio_np_w',              label: 'Station W',        type: 'number' },
    { key: 'radio_np_station_font',   label: 'Station font',     type: 'font'   },
    { key: 'radio_np_color',          label: 'Station colour',   type: 'color'  },
    { key: 'radio_show_np_title',     label: 'Show track title', type: 'bool' },
    { key: 'radio_title_x',           label: 'Title X',          type: 'number' },
    { key: 'radio_title_y',           label: 'Title Y',          type: 'number' },
    { key: 'radio_title_w',           label: 'Title W',          type: 'number' },
    { key: 'radio_np_title_font',     label: 'Title font',       type: 'font'   },
    { key: 'radio_title_color',       label: 'Title colour',     type: 'color'  },
    { key: 'radio_show_station_icon', label: 'Show station icon', type: 'bool' },
    { key: 'radio_station_icon_x',    label: 'Station icon X',   type: 'number' },
    { key: 'radio_station_icon_y',    label: 'Station icon Y',   type: 'number' },
    { key: 'radio_station_icon_size', label: 'Station icon size', type: 'number', min: 16, max: 64, default: 64 },

    { key: 'radio_show_playback_status', label: 'Show playback status', type: 'bool' },
    { key: 'radio_state_x',           label: 'State X',          type: 'number' },
    { key: 'radio_state_y',           label: 'State Y',          type: 'number' },
    { key: 'radio_state_font',        label: 'State font',       type: 'font'   },
    { key: 'radio_state_color',       label: 'State colour',     type: 'color'  },

    { key: 'radio_audio_info_font',   label: 'Audio info font',  type: 'font'   },
    { key: 'radio_info_color',        label: 'Audio info colour', type: 'color' },
    { key: 'radio_samplerate_show',   label: 'Show sample rate', type: 'bool' },
    { key: 'radio_samplerate_x',      label: 'Sample rate X',    type: 'number' },
    { key: 'radio_samplerate_y',      label: 'Sample rate Y',    type: 'number' },
    { key: 'radio_channels_show',     label: 'Show channels',    type: 'bool' },
    { key: 'radio_channels_x',        label: 'Channels X',       type: 'number' },
    { key: 'radio_channels_y',        label: 'Channels Y',       type: 'number' },
    { key: 'radio_bitrate_show',      label: 'Show bitrate',     type: 'bool' },
    { key: 'radio_bitrate_x',         label: 'Bitrate X',        type: 'number' },
    { key: 'radio_bitrate_y',         label: 'Bitrate Y',        type: 'number' },
    { key: 'radio_volume_show',       label: 'Show volume',      type: 'bool' },
    { key: 'radio_volume_x',          label: 'Volume X',         type: 'number' },
    { key: 'radio_volume_y',          label: 'Volume Y',         type: 'number' },

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
    { key: 'radio_vu_bg_color',          label: 'Background colour', type: 'color' },
    { key: 'radio_vu_bar_color',         label: 'Bar colour',        type: 'color' },
    { key: 'radio_needle_transparent',   label: 'Transparent bg',    type: 'bool' },
    { key: 'radio_needle_bg_color',      label: 'Background colour', type: 'color' },
    { key: 'radio_needle_color',         label: 'Needle colour',     type: 'color' },
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
    { key: 'radio_stereo_frame',         label: 'Thin frame',        type: 'bool' },
    { key: 'radio_stereo_horizontal',    label: 'Horizontal bars',   type: 'bool' },
    { key: 'radio_stereo_transparent',   label: 'Transparent bg',    type: 'bool' },
    { key: 'radio_stereo_peak',          label: 'Peak hold',         type: 'bool' },
    { key: 'radio_stereo_zones',         label: 'Colour zones',      type: 'bool' },
    { key: 'radio_stereo_bg_color',      label: 'Background colour', type: 'color' },
    { key: 'radio_stereo_bar_color',     label: 'Bar colour',        type: 'color' },
    { key: 'radio_stereo_show_l',        label: 'Show left bar',     type: 'bool' },
    { key: 'radio_stereo_l_x',           label: 'Left bar X',        type: 'number' },
    { key: 'radio_stereo_l_y',           label: 'Left bar Y',        type: 'number' },
    { key: 'radio_stereo_l_w',           label: 'Left bar W',        type: 'number', min: 8, max: 480 },
    { key: 'radio_stereo_l_h',           label: 'Left bar H',        type: 'number', min: 8, max: 480 },
    { key: 'radio_stereo_show_r',        label: 'Show right bar',    type: 'bool' },
    { key: 'radio_stereo_r_x',           label: 'Right bar X',       type: 'number' },
    { key: 'radio_stereo_r_y',           label: 'Right bar Y',       type: 'number' },
    { key: 'radio_stereo_r_w',           label: 'Right bar W',       type: 'number', min: 8, max: 480 },
    { key: 'radio_stereo_r_h',           label: 'Right bar H',       type: 'number', min: 8, max: 480 },
    { key: 'radio_show_cassette',        label: 'Show animated wheels', type: 'bool' },
    { key: 'radio_animation_style',      label: 'Graphic', type: 'choice', default: 0,
      options: [{ value: 0, label: 'Cassette reels' }, { value: 1, label: 'Car rims' }] },
    { key: 'radio_wheels_reverse',       label: 'Reverse rotation',      type: 'bool' },
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
    { key: 'radio_weather_w', label: 'Weather W (0 = full width)', type: 'number' },
    { key: 'radio_weather_font', label: 'Weather font', type: 'font' },
    { key: 'radio_volslider_show',       label: 'Show volume slider', type: 'bool' },
    { key: 'radio_volslider_vertical',   label: 'Vertical',           type: 'bool' },
    { key: 'radio_volslider_knob_only',  label: 'Knob only (no track)', type: 'bool' },
    { key: 'radio_volslider_x',          label: 'Slider X',           type: 'number' },
    { key: 'radio_volslider_y',          label: 'Slider Y',           type: 'number' },
    { key: 'radio_volslider_w',          label: 'Slider W',           type: 'number', min: 8, max: 480 },
    { key: 'radio_volslider_h',          label: 'Slider H',           type: 'number', min: 8, max: 480 },
    { key: 'radio_volslider_knob_image', label: 'Knob image (.bin)',  type: 'text', placeholder: '/sdcard/assets/knobs/... (empty = colour knob)', sdPicker: { dir: '/assets/knobs' } },
    { key: 'radio_volslider_vol_max',    label: 'Max volume (%)',     type: 'number', min: 1, max: 100 },
    { key: 'radio_show_ctrl_overlay', label: 'Show tap controls overlay', type: 'bool', default: true },
    ...touchHotspotFields('radio'),
];

const SD_FIELDS = [
    { key: 'sd_title_x',    label: 'Title X',          type: 'number' },
    { key: 'sd_title_y',    label: 'Title Y',          type: 'number' },
    { key: 'sd_title_w',    label: 'Title W',          type: 'number' },
    { key: 'sd_title_font', label: 'Title font',       type: 'font'   },
    { key: 'sd_title_color', label: 'Title colour',    type: 'color'  },

    { key: 'sd_show_folder', label: 'Show folder',     type: 'bool'   },
    { key: 'sd_folder_x',    label: 'Folder X',        type: 'number' },
    { key: 'sd_folder_y',    label: 'Folder Y',        type: 'number' },
    { key: 'sd_folder_font', label: 'Folder font',     type: 'font'   },
    { key: 'sd_folder_color', label: 'Folder colour',  type: 'color'  },

    { key: 'sd_info_font',    label: 'Info font',      type: 'font'   },
    { key: 'sd_info_color',   label: 'Info colour',    type: 'color'  },
    { key: 'sd_volume_show',  label: 'Show volume',    type: 'bool'   },
    { key: 'sd_volume_x',     label: 'Volume X',       type: 'number' },
    { key: 'sd_volume_y',     label: 'Volume Y',       type: 'number' },
    { key: 'sd_status_show',  label: 'Show status flags', type: 'bool' },
    { key: 'sd_status_x',     label: 'Status X',       type: 'number' },
    { key: 'sd_status_y',     label: 'Status Y',       type: 'number' },

    { key: 'sd_show_time',            label: 'Show playback time', type: 'bool' },
    { key: 'sd_time_x',               label: 'Time X',            type: 'number' },
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
    { key: 'sd_vu_bg_color',          label: 'Background colour', type: 'color' },
    { key: 'sd_vu_bar_color',         label: 'Bar colour',        type: 'color' },
    { key: 'sd_needle_transparent',   label: 'Transparent bg',    type: 'bool' },
    { key: 'sd_needle_bg_color',      label: 'Background colour', type: 'color' },
    { key: 'sd_needle_color',         label: 'Needle colour',     type: 'color' },
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
    { key: 'sd_stereo_frame',         label: 'Thin frame',        type: 'bool' },
    { key: 'sd_stereo_horizontal',    label: 'Horizontal bars',   type: 'bool' },
    { key: 'sd_stereo_transparent',   label: 'Transparent bg',    type: 'bool' },
    { key: 'sd_stereo_peak',          label: 'Peak hold',         type: 'bool' },
    { key: 'sd_stereo_zones',         label: 'Colour zones',      type: 'bool' },
    { key: 'sd_stereo_bg_color',      label: 'Background colour', type: 'color' },
    { key: 'sd_stereo_bar_color',     label: 'Bar colour',        type: 'color' },
    { key: 'sd_stereo_show_l',        label: 'Show left bar',     type: 'bool' },
    { key: 'sd_stereo_l_x',           label: 'Left bar X',        type: 'number' },
    { key: 'sd_stereo_l_y',           label: 'Left bar Y',        type: 'number' },
    { key: 'sd_stereo_l_w',           label: 'Left bar W',        type: 'number', min: 8, max: 480 },
    { key: 'sd_stereo_l_h',           label: 'Left bar H',        type: 'number', min: 8, max: 480 },
    { key: 'sd_stereo_show_r',        label: 'Show right bar',    type: 'bool' },
    { key: 'sd_stereo_r_x',           label: 'Right bar X',       type: 'number' },
    { key: 'sd_stereo_r_y',           label: 'Right bar Y',       type: 'number' },
    { key: 'sd_stereo_r_w',           label: 'Right bar W',       type: 'number', min: 8, max: 480 },
    { key: 'sd_stereo_r_h',           label: 'Right bar H',       type: 'number', min: 8, max: 480 },
    { key: 'sd_show_cassette',        label: 'Show animated wheels', type: 'bool' },
    { key: 'sd_animation_style',      label: 'Graphic', type: 'choice', default: 0,
      options: [{ value: 0, label: 'Cassette reels' }, { value: 1, label: 'Car rims' }] },
    { key: 'sd_wheels_reverse',       label: 'Reverse rotation',      type: 'bool' },
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
    { key: 'sd_weather_w', label: 'Weather W (0 = full width)', type: 'number' },
    { key: 'sd_weather_font', label: 'Weather font', type: 'font' },
    { key: 'sd_volslider_show',       label: 'Show volume slider', type: 'bool' },
    { key: 'sd_volslider_vertical',   label: 'Vertical',           type: 'bool' },
    { key: 'sd_volslider_knob_only',  label: 'Knob only (no track)', type: 'bool' },
    { key: 'sd_volslider_x',          label: 'Slider X',           type: 'number' },
    { key: 'sd_volslider_y',          label: 'Slider Y',           type: 'number' },
    { key: 'sd_volslider_w',          label: 'Slider W',           type: 'number', min: 8, max: 480 },
    { key: 'sd_volslider_h',          label: 'Slider H',           type: 'number', min: 8, max: 480 },
    { key: 'sd_volslider_knob_image', label: 'Knob image (.bin)',  type: 'text', placeholder: '/sdcard/assets/knobs/... (empty = colour knob)', sdPicker: { dir: '/assets/knobs' } },
    { key: 'sd_volslider_vol_max',    label: 'Max volume (%)',     type: 'number', min: 1, max: 100 },
    { key: 'sd_show_ctrl_overlay', label: 'Show tap controls overlay', type: 'bool', default: true },
    ...touchHotspotFields('sd'),
];

// Equalizer — the 10 bands are spread across the group's SPAN (eq_group_w): the
// first slider starts on the left edge, the last one ends on the right edge, so
// both align with a wallpaper to the pixel and individual gaps may differ by 1 px
// (see eqGeom). One knob image is shared by every band and does not affect the
// geometry (it may overhang its band); the screen background uses the generic
// per-screen wallpaper picker (eq_wallpaper).
const EQ_FIELDS = [
    { key: 'eq_info_x',    label: 'Value X',    type: 'number' },
    { key: 'eq_info_y',    label: 'Value Y',    type: 'number' },
    { key: 'eq_info_font', label: 'Value font', type: 'font'   },
    { key: 'eq_group_x',   label: 'Group X',    type: 'number' },
    { key: 'eq_group_y',   label: 'Group Y',    type: 'number' },
    { key: 'eq_group_w',   label: 'Group W (span)', type: 'number', min: 8, max: 960 },
    { key: 'eq_slider_h',  label: 'Slider H',   type: 'number', min: 4, max: 480 },
    { key: 'eq_slider_w',  label: 'Slider W (track)', type: 'number', min: 1, max: 200 },
    { key: 'eq_group_centre', label: 'Gaps follow the span (see summary)',
      type: 'action', button: 'Centre X', action: centreEqGroup },
    { key: 'eq_freq_hide', label: 'Hide frequency labels', type: 'bool' },
    { key: 'eq_hint_hide', label: 'Hide legend', type: 'bool' },
    { key: 'eq_hint_x',    label: 'Legend X (from centre)', type: 'number' },
    { key: 'eq_hint_y',    label: 'Legend Y (from bottom)', type: 'number' },
    { key: 'eq_curve_x',   label: 'Curve X',    type: 'number' },
    { key: 'eq_curve_y',   label: 'Curve Y',    type: 'number' },
    { key: 'eq_curve_w',   label: 'Curve W',    type: 'number', min: 0, max: 480 },
    { key: 'eq_curve_h',   label: 'Curve H',    type: 'number', min: 0, max: 480 },
    { key: 'eq_knob_image', label: 'Knob image (.bin)', type: 'text',
      placeholder: '/sdcard/assets/knobs/... (empty = colour knob)',
      sdPicker: { dir: '/assets/knobs' } },
    { key: 'eq_knob_w', label: 'Knob width (px, 0 = fill band)', type: 'number', min: 0, max: 200 },
    { key: 'eq_knob_only', label: 'Knob only (no track)', type: 'bool' },
];

// The two list screens — the station playlist and the SD file browser — have
// the same shape and one section each, so each can be lined up with its own
// wallpaper. The list box is the element that matters against artwork: size it
// to the frame the wallpaper leaves free and the rows land inside it (their
// width is derived from the box, never stored). A smaller box is also a cheaper
// scroll — the device only repaints what the box covers.
// `accentLabel` names the rows each screen picks out of the list — the playing
// station, or the ".." and folder entries.
function listFields(prefix, accentLabel) {
    return [
        { key: `${prefix}_list_x`,       label: 'List X',      type: 'number' },
        { key: `${prefix}_list_y`,       label: 'List Y',      type: 'number' },
        { key: `${prefix}_list_w`,       label: 'List W',      type: 'number', min: 16, max: 960 },
        { key: `${prefix}_list_h`,       label: 'List H',      type: 'number', min: 16, max: 960 },
        { key: `${prefix}_item_h`,       label: 'Row height',  type: 'number', min: 4,  max: 200 },
        { key: `${prefix}_item_pad`,     label: 'Row gap',     type: 'number', min: 0,  max: 100 },
        { key: `${prefix}_row_pad_left`, label: 'Text indent', type: 'number', min: 0,  max: 200 },
        { key: `${prefix}_row_font`,     label: 'Row font',    type: 'font'   },
        { key: `${prefix}_row_bg_color`,      label: 'Row plate colour',  type: 'color' },
        { key: `${prefix}_row_text_color`,    label: 'Row text colour',   type: 'color' },
        { key: `${prefix}_row_accent_color`,  label: `${accentLabel} colour`, type: 'color' },
        { key: `${prefix}_cursor_bg_color`,   label: 'Selected plate colour', type: 'color' },
        { key: `${prefix}_cursor_text_color`, label: 'Selected text colour',  type: 'color' },
        { key: `${prefix}_header_hide`,  label: 'Hide header bar', type: 'bool' },
        { key: `${prefix}_header_h`,     label: 'Header height',   type: 'number', min: 0, max: 200 },
        { key: `${prefix}_header_font`,  label: 'Header font', type: 'font'   },
        { key: `${prefix}_label_x`,      label: 'Title X (from left)',   type: 'number' },
        { key: `${prefix}_label_y`,      label: 'Title Y (from middle)', type: 'number' },
        { key: `${prefix}_hint_hide`,    label: 'Hide legend', type: 'bool' },
        { key: `${prefix}_hint_x`,       label: 'Legend X (from right)',  type: 'number' },
        { key: `${prefix}_hint_y`,       label: 'Legend Y (from middle)', type: 'number' },
    ];
}

function listGroups(prefix) {
    return [
        { heading: 'List' },
        { title: 'List box', summary: d => listBoxSummary(d, prefix),
          fields: [`${prefix}_list_x`, `${prefix}_list_y`, `${prefix}_list_w`, `${prefix}_list_h`] },
        { title: 'Rows', fields: [`${prefix}_item_h`, `${prefix}_item_pad`,
                                  `${prefix}_row_pad_left`, `${prefix}_row_font`] },
        // Unset colours follow the theme, which is tuned for a plain background;
        // over a wallpaper the list usually wants its own.
        { title: 'Row colours', fields: [`${prefix}_row_bg_color`, `${prefix}_row_text_color`,
                                         `${prefix}_row_accent_color`, `${prefix}_cursor_bg_color`,
                                         `${prefix}_cursor_text_color`] },
        { heading: 'Header' },
        // Title and legend are drawn inside the header bar, so they hang off its
        // toggle: hiding the bar takes them with it instead of leaving two
        // groups of coordinates that no longer place anything.
        { title: 'Header bar', enabledBy: `!${prefix}_header_hide`,
          summary: d => `H ${d[`${prefix}_header_h`] | 0}`,
          fields: [`${prefix}_header_hide`, `${prefix}_header_h`, `${prefix}_header_font`],
          subgroups: [
              { title: 'Title', fields: [`${prefix}_label_x`, `${prefix}_label_y`] },
              { title: 'Legend', enabledBy: `!${prefix}_hint_hide`,
                fields: [`${prefix}_hint_hide`, `${prefix}_hint_x`, `${prefix}_hint_y`] },
          ] },
    ];
}

const PLAYLIST_FIELDS = listFields('playlist', 'Playing station');
const BROWSER_FIELDS  = listFields('browser',  'Folders & ..');

// Form-only grouping. Field schemas above remain the API/source-of-truth; these
// groups only decide how the editor presents them. `enabledBy` keeps the Show
// switch visible while hiding the controls that have no effect when it is off.
// Groups are shown in this order. A `{ heading }` entry is a non-collapsible
// category separator (rendered by buildForm as a divider, not a <details>);
// it still occupies an array index, so dataset.groupIndex stays aligned.
const FORM_GROUPS = {
    clock: [
        { heading: 'Text & labels' },
        { title: 'Time', enabledBy: 'clock_show_time', fields: ['clock_show_time', 'clock_time_x', 'clock_time_y', 'clock_time_font', 'clock_time_color'] },
        { title: 'Date', enabledBy: 'clock_show_date', fields: ['clock_show_date', 'clock_date_x', 'clock_date_y', 'clock_date_font', 'clock_date_color'] },
        { title: 'Network info', enabledBy: 'clock_show_netinfo', fields: ['clock_show_netinfo', 'clock_netinfo_x', 'clock_netinfo_y', 'clock_netinfo_font', 'clock_netinfo_color'] },
        { title: 'Station / title', fields: ['clock_show_strip', 'clock_strip_x', 'clock_strip_y', 'clock_strip_w', 'clock_strip_h', 'clock_strip_bg_opa', 'clock_strip_station_x', 'clock_strip_station_y', 'clock_strip_station_w', 'clock_strip_title_x', 'clock_strip_title_y', 'clock_strip_title_w', 'clock_strip_station_font', 'clock_strip_title_font', 'clock_strip_station_color', 'clock_strip_title_color'] },
        { title: 'Calendar', enabledBy: 'clock_show_calendar', fields: ['clock_show_calendar', 'clock_calendar_x', 'clock_calendar_y', 'clock_calendar_w', 'clock_calendar_font'] },
        { title: 'Weather', enabledBy: 'clock_show_weather', fields: ['clock_show_weather', 'clock_weather_x', 'clock_weather_y', 'clock_weather_w', 'clock_weather_font'] },
        { heading: 'Indicators' },
        { title: 'Mode indicator', enabledBy: 'clock_show_mode_indicator', fields: ['clock_show_mode_indicator', 'clock_mode_indic_x', 'clock_mode_indic_y'] },
        { title: 'Event indicator', enabledBy: 'clock_show_event_indicator', fields: ['clock_show_event_indicator', 'clock_event_indic_x', 'clock_event_indic_y'] },
    ],
    bt: [
        { heading: 'Text & labels' },
        { title: 'Device status', fields: ['bt_brand_x', 'bt_brand_y', 'bt_brand_font', 'bt_brand_color', 'bt_status_x', 'bt_status_y', 'bt_status_font', 'bt_status_color'] },
        { title: 'Track title', fields: ['bt_title_x', 'bt_title_y', 'bt_title_w', 'bt_title_font', 'bt_title_color'] },
        { title: 'Artist', fields: ['bt_artist_x', 'bt_artist_y', 'bt_artist_w', 'bt_artist_font', 'bt_artist_color'] },
        { title: 'Playback', fields: ['bt_time_x', 'bt_time_y', 'bt_time_font', 'bt_time_color', 'bt_vol_x', 'bt_vol_y', 'bt_vol_label_font', 'bt_vol_color'] },
        { heading: 'Indicators & clock' },
        { title: 'Bluetooth mark', enabledBy: 'bt_show_circle', fields: ['bt_show_circle', 'bt_circle_x', 'bt_circle_y', 'bt_circle_w', 'bt_circle_h', 'bt_icon_font'] },
        { title: 'Mode indicator', enabledBy: 'bt_show_mode_indicator', fields: ['bt_show_mode_indicator', 'bt_mode_indic_x', 'bt_mode_indic_y'] },
        { title: 'Clock', enabledBy: 'bt_show_clock', fields: ['bt_show_clock', 'bt_clock_widget_x', 'bt_clock_widget_y', 'bt_clock_font'] },
        { heading: 'Controls' },
        { title: 'Volume slider', enabledBy: 'bt_volslider_show', fields: ['bt_volslider_show', 'bt_volslider_vertical', 'bt_volslider_knob_only', 'bt_volslider_x', 'bt_volslider_y', 'bt_volslider_w', 'bt_volslider_h', 'bt_volslider_knob_image', 'bt_volslider_vol_max'] },
        { title: 'Tap controls overlay', enabledBy: 'bt_show_ctrl_overlay', fields: ['bt_show_ctrl_overlay'] },
        ...touchHotspotGroups('bt'),
    ],
    radio: [
        { heading: 'Text & labels' },
        { title: 'Station name', enabledBy: 'radio_show_np', fields: ['radio_show_np', 'radio_np_x', 'radio_np_y', 'radio_np_w', 'radio_np_station_font', 'radio_np_color'] },
        { title: 'Track title', enabledBy: 'radio_show_np_title', fields: ['radio_show_np_title', 'radio_title_x', 'radio_title_y', 'radio_title_w', 'radio_np_title_font', 'radio_title_color'] },
        { title: 'Playback status', enabledBy: 'radio_show_playback_status', fields: ['radio_show_playback_status', 'radio_state_x', 'radio_state_y', 'radio_state_font', 'radio_state_color'] },
        { title: 'Weather', enabledBy: 'radio_show_weather', fields: ['radio_show_weather', 'radio_weather_x', 'radio_weather_y', 'radio_weather_w', 'radio_weather_font'] },
        { title: 'Audio info', fields: ['radio_audio_info_font', 'radio_info_color'], subgroups: [
            { title: 'Sample rate', enabledBy: 'radio_samplerate_show', fields: ['radio_samplerate_show', 'radio_samplerate_x', 'radio_samplerate_y'] },
            { title: 'Channels (stereo/mono)', enabledBy: 'radio_channels_show', fields: ['radio_channels_show', 'radio_channels_x', 'radio_channels_y'] },
            { title: 'Bitrate', enabledBy: 'radio_bitrate_show', fields: ['radio_bitrate_show', 'radio_bitrate_x', 'radio_bitrate_y'] },
            { title: 'Volume', enabledBy: 'radio_volume_show', fields: ['radio_volume_show', 'radio_volume_x', 'radio_volume_y'] },
        ] },
        { title: 'VU meters', fields: [], subgroups: [
            { title: 'Bar VU', enabledBy: 'radio_show_vu', fields: ['radio_show_vu', 'radio_vu_x', 'radio_vu_y', 'radio_vu_w', 'radio_vu_h', 'radio_vu_transparent', 'radio_vu_bg_color', 'radio_vu_bar_color'] },
            { title: 'Needle VU', fields: ['radio_needle_transparent', 'radio_needle_bg_color', 'radio_needle_color', 'radio_needle_show_l', 'radio_needle_l_x', 'radio_needle_l_y', 'radio_needle_l_w', 'radio_needle_l_h', 'radio_needle_show_r', 'radio_needle_r_x', 'radio_needle_r_y', 'radio_needle_r_w', 'radio_needle_r_h'] },
            { title: 'Stereo bar VU', fields: ['radio_stereo_frame', 'radio_stereo_horizontal', 'radio_stereo_transparent', 'radio_stereo_peak', 'radio_stereo_zones', 'radio_stereo_bg_color', 'radio_stereo_bar_color', 'radio_stereo_show_l', 'radio_stereo_l_x', 'radio_stereo_l_y', 'radio_stereo_l_w', 'radio_stereo_l_h', 'radio_stereo_show_r', 'radio_stereo_r_x', 'radio_stereo_r_y', 'radio_stereo_r_w', 'radio_stereo_r_h'] },
        ] },
        { heading: 'Indicators & clock' },
        { title: 'Station icon', enabledBy: 'radio_show_station_icon', fields: ['radio_show_station_icon', 'radio_station_icon_x', 'radio_station_icon_y', 'radio_station_icon_size'] },
        { title: 'Mode indicator', enabledBy: 'radio_show_mode_indicator', fields: ['radio_show_mode_indicator', 'radio_mode_indic_x', 'radio_mode_indic_y'] },
        { title: 'Event indicator', enabledBy: 'radio_show_event_indicator', fields: ['radio_show_event_indicator', 'radio_event_indic_x', 'radio_event_indic_y'] },
        { title: 'Clock', enabledBy: 'radio_show_clock', fields: ['radio_show_clock', 'radio_clock_widget_x', 'radio_clock_widget_y', 'radio_clock_font'] },
        { heading: 'Controls' },
        { title: 'Volume slider', enabledBy: 'radio_volslider_show', fields: ['radio_volslider_show', 'radio_volslider_vertical', 'radio_volslider_knob_only', 'radio_volslider_x', 'radio_volslider_y', 'radio_volslider_w', 'radio_volslider_h', 'radio_volslider_knob_image', 'radio_volslider_vol_max'] },
        { title: 'Tap controls overlay', enabledBy: 'radio_show_ctrl_overlay', fields: ['radio_show_ctrl_overlay'] },
        ...touchHotspotGroups('radio'),
        { heading: 'Decoration' },
        { title: 'Animated wheels', enabledBy: 'radio_show_cassette', fields: ['radio_show_cassette', 'radio_animation_style', 'radio_wheels_reverse', 'radio_show_wheel_left', 'radio_cassette_l_x', 'radio_cassette_l_y', 'radio_cassette_l_size', 'radio_show_wheel_right', 'radio_cassette_r_x', 'radio_cassette_r_y', 'radio_cassette_r_size'] },
    ],
    sd: [
        { heading: 'Text & labels' },
        { title: 'Track title', fields: ['sd_title_x', 'sd_title_y', 'sd_title_w', 'sd_title_font', 'sd_title_color'] },
        { title: 'Folder', enabledBy: 'sd_show_folder', fields: ['sd_show_folder', 'sd_folder_x', 'sd_folder_y', 'sd_folder_font', 'sd_folder_color'] },
        { title: 'Weather', enabledBy: 'sd_show_weather', fields: ['sd_show_weather', 'sd_weather_x', 'sd_weather_y', 'sd_weather_w', 'sd_weather_font'] },
        { title: 'Playback info', fields: ['sd_info_font', 'sd_info_color'], subgroups: [
            { title: 'Volume', enabledBy: 'sd_volume_show', fields: ['sd_volume_show', 'sd_volume_x', 'sd_volume_y'] },
            { title: 'Status flags', enabledBy: 'sd_status_show', fields: ['sd_status_show', 'sd_status_x', 'sd_status_y'] },
        ] },
        { heading: 'Playback' },
        { title: 'Playback time', enabledBy: 'sd_show_time', fields: ['sd_show_time', 'sd_time_x', 'sd_time_y'] },
        { title: 'Progress bar', enabledBy: 'sd_show_bar', fields: ['sd_show_bar', 'sd_bar_x', 'sd_bar_y', 'sd_bar_w', 'sd_bar_h'] },
        { title: 'VU meters', fields: [], subgroups: [
            { title: 'Bar VU', enabledBy: 'sd_show_vu', fields: ['sd_show_vu', 'sd_vu_x', 'sd_vu_y', 'sd_vu_w', 'sd_vu_h', 'sd_vu_transparent', 'sd_vu_bg_color', 'sd_vu_bar_color'] },
            { title: 'Needle VU', fields: ['sd_needle_transparent', 'sd_needle_bg_color', 'sd_needle_color', 'sd_needle_show_l', 'sd_needle_l_x', 'sd_needle_l_y', 'sd_needle_l_w', 'sd_needle_l_h', 'sd_needle_show_r', 'sd_needle_r_x', 'sd_needle_r_y', 'sd_needle_r_w', 'sd_needle_r_h'] },
            { title: 'Stereo bar VU', fields: ['sd_stereo_frame', 'sd_stereo_horizontal', 'sd_stereo_transparent', 'sd_stereo_peak', 'sd_stereo_zones', 'sd_stereo_bg_color', 'sd_stereo_bar_color', 'sd_stereo_show_l', 'sd_stereo_l_x', 'sd_stereo_l_y', 'sd_stereo_l_w', 'sd_stereo_l_h', 'sd_stereo_show_r', 'sd_stereo_r_x', 'sd_stereo_r_y', 'sd_stereo_r_w', 'sd_stereo_r_h'] },
        ] },
        { heading: 'Indicators & clock' },
        { title: 'Mode indicator', enabledBy: 'sd_show_mode_indicator', fields: ['sd_show_mode_indicator', 'sd_mode_indic_x', 'sd_mode_indic_y'] },
        { title: 'Event indicator', enabledBy: 'sd_show_event_indicator', fields: ['sd_show_event_indicator', 'sd_event_indic_x', 'sd_event_indic_y'] },
        { title: 'Clock', enabledBy: 'sd_show_clock', fields: ['sd_show_clock', 'sd_clock_widget_x', 'sd_clock_widget_y', 'sd_clock_font'] },
        { heading: 'Controls' },
        { title: 'Volume slider', enabledBy: 'sd_volslider_show', fields: ['sd_volslider_show', 'sd_volslider_vertical', 'sd_volslider_knob_only', 'sd_volslider_x', 'sd_volslider_y', 'sd_volslider_w', 'sd_volslider_h', 'sd_volslider_knob_image', 'sd_volslider_vol_max'] },
        { title: 'Tap controls overlay', enabledBy: 'sd_show_ctrl_overlay', fields: ['sd_show_ctrl_overlay'] },
        ...touchHotspotGroups('sd'),
        { heading: 'Decoration' },
        { title: 'Animated wheels', enabledBy: 'sd_show_cassette', fields: ['sd_show_cassette', 'sd_animation_style', 'sd_wheels_reverse', 'sd_show_wheel_left', 'sd_cassette_l_x', 'sd_cassette_l_y', 'sd_cassette_l_size', 'sd_show_wheel_right', 'sd_cassette_r_x', 'sd_cassette_r_y', 'sd_cassette_r_size'] },
    ],
    eq: [
        { heading: 'Text & labels' },
        { title: 'Value label', fields: ['eq_info_x', 'eq_info_y', 'eq_info_font'] },
        { title: 'Legend', enabledBy: '!eq_hint_hide',
          fields: ['eq_hint_hide', 'eq_hint_x', 'eq_hint_y'] },
        { heading: 'Bands' },
        { title: 'Sliders group', summary: eqGroupSummary,
          fields: ['eq_group_x', 'eq_group_y', 'eq_group_w', 'eq_slider_h',
                   'eq_slider_w', 'eq_group_centre', 'eq_freq_hide'] },
        { title: 'Response curve', fields: ['eq_curve_x', 'eq_curve_y', 'eq_curve_w', 'eq_curve_h'] },
        { heading: 'Artwork' },
        { title: 'Knob image', fields: ['eq_knob_image', 'eq_knob_w', 'eq_knob_only'] },
    ],
    playlist: listGroups('playlist'),
    browser:  listGroups('browser'),
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
    eq:    { title: 'Equalizer', fields: EQ_FIELDS,    renderer: renderEq    },
    playlist: { title: 'Playlist',   fields: PLAYLIST_FIELDS, renderer: renderPlaylist },
    browser:  { title: 'SD Browser', fields: BROWSER_FIELDS,  renderer: renderBrowser  },
};

const state = {
    meta:   { screen_w: 320, screen_h: 240, fonts: [] },
    active: 'clock',
    clock:  {},
    bt:     {},
    radio:  {},
    sd:     {},
    eq:     {},
    playlist: {},
    browser: {},
};

// The layout preview can use the active screen's SD wallpaper as its
// background. It is decoded once and kept as a browser-native data URL so
// dragging widgets does not re-fetch or re-decode the .bin on every render.
// Each profile section carries a `<section>_wallpaper` source field:
// "" / "none" = General (gradient/solid, or the internet wallpaper when one is
// fetched), "net0".."net9" = that internet slot, else an SD .bin path.
let wallpaperPreviewUrl = '';
let wallpaperPreviewDim = 0;
let currentWallpaperPath = '';   // effective SD path for the ACTIVE section
let netWallpaperActive = false;  // device shows a fetched internet wallpaper
// Internet slots. The count comes from the firmware (/api/wallpaper/status);
// 1 is the safe default so a pre-slots device still gets a working single-slot
// UI instead of ten phantom pickers.
let netWpSlotCount = 1;
let netWpSlotKnown = false;      // the count has been read from the device
let netWpUrls      = [];         // index = slot, mirrors display.wallpaper_urls
let netWpCurSlot   = 0;          // slot the Background tab is editing
let keyboardSelection = null;
const ONLINE_WALLPAPER_CATALOG = 'https://atlascube.net/wallpapers/catalog.php';
const ONLINE_WALLPAPER_ORIGIN = 'https://atlascube.net';
const ONLINE_WALLPAPER_MAX_BYTES = 8 * 1024 * 1024;

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

// Slot named by a per-screen override, mirroring net_slot_of() in the firmware:
// "net0".."net9" name a slot, bare "net" is the pre-slots spelling for slot 0,
// anything else (a path, "none", "") is not an internet override.
// Deliberately NOT validated against netWpSlotCount: that count arrives with the
// Background tab's status poll, and a section override is read before then. A
// syntactic 0-9 keeps "net3" an internet override at page load instead of
// briefly resolving it as an SD path; the firmware bounds it for real.
function netSlotOf(ovr) {
    if (!ovr || ovr.slice(0, 3) !== 'net') return -1;
    if (ovr.length === 3) return 0;
    if (ovr.length !== 4) return -1;
    const n = ovr.charCodeAt(3) - 48;
    return (n >= 0 && n <= 9) ? n : -1;
}

// Mirror of the firmware's resolution in ui_background_apply(): an explicit
// per-screen SD file resolves to its path here. General ("" / "none") and
// Internet ("net0".."net9") carry no SD file — their preview is the gradient, or
// the fetched internet wallpaper (via /api/wallpaper/image?slot=N) when the
// device has one, with a "net wallpaper" text placeholder as the fallback.
function effectiveWallpaperPath() {
    const ovr = sectionWallpaperValue();
    if (ovr && ovr !== 'none' && netSlotOf(ovr) < 0) return ovr;
    return '';
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
    const slot = netSlotOf(ovr);
    if (slot >= 0) {
        label.textContent = '(internet slot ' + (slot + 1) + ')';
        const sel = document.getElementById('layout_wp_net_slot');
        if (sel) sel.value = String(slot);   // the picker follows the section
    } else if (ovr && ovr !== 'none') {
        label.textContent = ovr.split('/').pop();
    } else {
        // General: "" or "none" — gradient/solid, or the internet wallpaper
        // when the device has one fetched.
        label.textContent = netWallpaperActive ? '(general → internet)'
                                               : '(general — gradient)';
    }
    label.title = currentWallpaperPath;

    // Highlight the mode button matching the section's current state. Inline
    // styles, not the .active class — selectSection() strips .active from
    // every .section-tab while toggling the screen tabs.
    const mode = slot >= 0 ? 'net'
               : (ovr && ovr !== 'none') ? 'sd'
               : 'general';
    const modeBtns = {
        sd:      document.getElementById('layout_wp_btn_sd'),
        net:     document.getElementById('layout_wp_btn_net'),
        general: document.getElementById('layout_wp_btn_general'),
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
    // Three-way source switch for the active screen: a specific SD file /
    // Internet (the fetched wallpaper) / General (gradient-solid, replaced by
    // the internet wallpaper when one is fetched). The button matching the
    // section's current mode is highlighted by updateWallpaperPickerLabel().
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
    // Hidden file input backing the Upload button: converts a browser image to
    // the panel-sized .bin, stores it on SD next to the current wallpaper (or in
    // this panel's wallpaper folder when the screen has none yet) and assigns it
    // to the active screen — without touching the global wallpaper.
    const uploadInput = document.createElement('input');
    uploadInput.type = 'file';
    uploadInput.accept = 'image/*';
    uploadInput.hidden = true;
    uploadInput.addEventListener('change',
        () => uploadWallpaperTo(wallpaperDirectory(), uploadInput));
    const uploadBtn = smallBtn('layout_wp_btn_upload', '⬆ Upload...',
        'Upload an image as this screen\'s wallpaper', () => uploadInput.click());
    uploadBtn.style.cssText = 'padding:6px 10px;color:var(--accent);border-color:var(--accent)';
    const onlineBtn = smallBtn('layout_wp_btn_online', 'Online gallery',
        'Browse wallpapers from atlascube.net', toggleOnlineWallpaperGallery);
    onlineBtn.style.cssText = 'padding:6px 10px;color:#ff6b6f;border-color:#e5484d';
    const button = smallBtn('layout_wp_btn_sd', 'Choose from SD...',
        'Pick a wallpaper file for this screen', toggleWallpaperBrowser);
    // Internet: which of the fetched slots this screen shows. The select is the
    // slot, the button applies it — so assigning slot 3 to one screen and slot 1
    // to another is two clicks, and the select follows whatever the section
    // already has (updateWallpaperPickerLabel).
    const netSel = document.createElement('select');
    netSel.id = 'layout_wp_net_slot';
    netSel.className = 'field-input';
    netSel.style.cssText = 'width:auto;padding:5px 6px;font-size:11px';
    for (let i = 0; i < netWpSlotCount; i++) {
        const o = document.createElement('option');
        o.value = String(i);
        o.textContent = 'slot ' + (i + 1);
        netSel.appendChild(o);
    }
    netSel.addEventListener('change', () => {
        if (netSlotOf(sectionWallpaperValue()) < 0) return;   // not on Internet yet
        setSectionWallpaper('net' + netSel.value,
                            'Screen shows internet slot ' + (+netSel.value + 1) + '.');
    });
    const netBtn = smallBtn('layout_wp_btn_net', 'Internet',
        'Show a fetched internet wallpaper on this screen',
        () => setSectionWallpaper('net' + netSel.value,
                                  'Screen shows internet slot ' + (+netSel.value + 1) + '.'));
    const generalBtn = smallBtn('layout_wp_btn_general', 'General',
        'Standard background — gradient/solid, or the internet wallpaper when fetched',
        () => setSectionWallpaper('', 'Screen uses the general background.'));

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
    row.append(uploadBtn, onlineBtn, button, netBtn, netSel, generalBtn, autoLabel, uploadInput);

    // The active wallpaper path gets its own full-width line under the
    // buttons — inside the button row it was squeezed to a few characters.
    const nameRow = document.createElement('div');
    nameRow.style.cssText =
        'display:flex;align-items:center;gap:8px;margin-top:7px;min-width:0';
    nameRow.append(caption, name);

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

    const onlineGallery = document.createElement('div');
    onlineGallery.id = 'layout_online_wallpaper_gallery';
    onlineGallery.className = 'online-wallpaper-gallery';
    onlineGallery.hidden = true;

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

    picker.append(row, nameRow, plateRow, browser, onlineGallery, presetRow, status);
    frame.insertAdjacentElement('afterend', picker);
    updateLabelPlateControl();
}

// ── Per-wallpaper layout presets on SD ─────────────────────────────────────
// One file per wallpaper .bin, stored under /wallpapers/layouts as a mirror of
// where that .bin lives (see presetPath). The w/h stamp remains a second guard
// against a misplaced or damaged preset. Since wallpapers are assigned per
// screen, Save merges only the ACTIVE section into the file and Load applies
// only the active section. One file can therefore accumulate layouts for
// several screens sharing that artwork, but never for different panel
// resolutions — and no longer for unrelated artworks that share a filename.

const LAYOUTS_DIR = '/wallpapers/layouts';

// A preset mirrors its wallpaper's own place under /wallpapers, so one .bin has
// exactly one preset file:
//   /sdcard/wallpapers/320x240/radio-sd-player/japanese.bin
//        → /wallpapers/layouts/320x240/radio-sd-player/japanese.json
// Screens that share an artwork (Radio + SD Player, Playlist + SD Browser) keep
// sharing its file — they are different sections inside it. Screens that only
// share a NAME across category folders no longer collide, which they did while
// presets were filed by basename alone.
// The resolution segment is always the panel's, so the stamp inside the file can
// never disagree with the directory it sits in.
function presetPath() {
    if (!currentWallpaperPath) return '';
    const resolution = `${state.meta.screen_w}x${state.meta.screen_h}`;
    const rel = currentWallpaperPath.startsWith(SD_MOUNT + '/')
        ? currentWallpaperPath.slice(SD_MOUNT.length)
        : currentWallpaperPath;
    // Everything below the wallpaper's own resolution folder is mirrored — the
    // category subfolder, "internet", or nothing at all.
    const m = rel.match(/^\/wallpapers\/\d+x\d+\/(.+)$/i);
    if (m) return `${LAYOUTS_DIR}/${resolution}/${m[1].replace(/\.bin$/i, '')}.json`;
    // A wallpaper from anywhere else on the card keeps the flat layout.
    return legacyPresetPath();
}

// Where presets lived before they mirrored the wallpaper tree: one file per
// basename per resolution. Still read (never written) so existing presets load.
function legacyPresetPath() {
    if (!currentWallpaperPath) return '';
    const base = currentWallpaperPath.split('/').pop().replace(/\.bin$/i, '');
    return `${LAYOUTS_DIR}/${state.meta.screen_w}x${state.meta.screen_h}/${base}.json`;
}

// Reads one preset file. null = not there; anything else that went wrong throws,
// so callers can tell "no preset yet" from "the card did not answer".
async function readPresetJson(path) {
    const r = await fetch('/api/sd/file?path=' + encodeURIComponent(path),
                          { cache: 'no-store' });
    if (r.status === 404) return null;
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return await r.json();
}

// The mirrored file, else the legacy flat one. Returns { preset, path, legacy }
// or null when neither exists.
async function readPreset() {
    const path = presetPath();
    if (!path) return null;
    const preset = await readPresetJson(path);
    if (preset) return { preset, path, legacy: false };

    const old = legacyPresetPath();
    if (old === path) return null;
    const legacyPreset = await readPresetJson(old);
    return legacyPreset ? { preset: legacyPreset, path: old, legacy: true } : null;
}

// Sections carried over from a legacy flat file may belong to a DIFFERENT
// artwork that happened to share this basename — that was the whole point of
// moving presets into the wallpaper's folder. Keep only what pins this exact
// wallpaper, plus sections too old to pin anything (not ours to drop).
function sectionsForCurrentWallpaper(sections) {
    const kept = {};
    for (const [name, data] of Object.entries(sections)) {
        const pinned = data && data[`${name}_wallpaper`];
        if (!pinned || pinned === currentWallpaperPath) kept[name] = data;
    }
    return kept;
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
        // Carry over what is already stored for the OTHER screens. Only a
        // correctly stamped file is merged: the resolution directory keeps panel
        // variants separate, and the check keeps a damaged file from
        // contaminating the new preset.
        //
        // A read that fails for any reason other than "not there" aborts the
        // save — writing now would silently drop those sections, and a card busy
        // with playback is exactly when this read fails.
        const existing = await readPreset();
        if (existing && existing.preset.w === state.meta.screen_w &&
            existing.preset.h === state.meta.screen_h && existing.preset.sections) {
            preset.sections = existing.legacy
                ? sectionsForCurrentWallpaper(existing.preset.sections)
                : existing.preset.sections;
        }
        // Pin the SD wallpaper path into the stored copy so the preset stays
        // attached to its file (presets only exist for screens set to an SD
        // wallpaper; General/Internet screens carry no path here).
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
        setPresetStatus('Preset save failed: ' + err.message + ' — nothing written.', true);
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
        // Mirrored location first, then the legacy flat one.
        const found = await readPreset();
        if (!found) {
            setPresetStatus('No preset saved for this wallpaper yet.', true);
            return;
        }
        const preset = found.preset;
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
        migrateLegacyEqSection(section);
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

// Presets saved before the group became a band SPAN. Convert in place (mirroring
// the conversions in load_eq() on the device) so an old preset still reproduces
// its look instead of silently keeping whatever the device currently has. Old
// presets predate the slider fields too, so missing values come from the device's
// current section — which is what the old layout was built on.
function migrateLegacyEqSection(section) {
    if (!section) return;
    const n   = EQ_FREQ_LABELS.length;
    const val = k => (section[k] !== undefined ? section[k] : state.eq[k]);
    const sw  = Math.max(1, val('eq_slider_w') | 0);

    if (section.eq_band_gap !== undefined) {
        // Interim shape: a uniform gap, span = 10 sliders + 9 gaps.
        const gap = Math.max(0, section.eq_band_gap | 0);
        section.eq_group_w = sw * n + gap * (n - 1);
        delete section.eq_band_gap;
        return;
    }
    if (section.eq_group_w === undefined || section.eq_slider_w !== undefined) return;

    // Released shape: eq_group_w was the box width, the band column gw/10, and the
    // slider was centred in its column — so the span is nine columns plus one
    // slider, starting half a gap further in.
    const gw = section.eq_group_w | 0, gh = section.eq_group_h | 0;
    delete section.eq_group_h;
    if (gw <= 0) { delete section.eq_group_w; return; }
    const pitch = Math.max(sw, Math.floor(gw / n));
    section.eq_group_w = pitch * (n - 1) + sw;
    section.eq_group_x = (val('eq_group_x') | 0) + Math.floor((pitch - sw) / 2);
    if (gh > 0) {
        const freqArea = val('eq_freq_hide')
            ? 0 : clamp(Math.round(pitch * 0.7), 7, 14) + 4;
        section.eq_slider_h = Math.max(4, gh - freqArea);
    }
}

// After switching a screen's wallpaper, offer to apply its saved layout for
// this screen (if one exists) so wallpaper + matching layout travel together.
async function offerPresetForWallpaper() {
    if (!presetPath()) return;
    try {
        const found = await readPreset();
        if (!found) return;
        const preset = found.preset;
        if (preset.w !== state.meta.screen_w || preset.h !== state.meta.screen_h)
            return;
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

// ── Layout preset housekeeping (Presets tab) ────────────────────────────────
// Presets become invalid when their resolution stamp differs from their
// directory; they become orphans when their wallpaper .bin is deleted/renamed.
// A wallpaper can live anywhere on the card, so name matching against
// /wallpapers alone would flag false orphans — instead each preset is opened
// and the full paths it stores in its <section>_wallpaper fields are checked.
// Both kinds of problem are reported, with deletion left to the user.
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

async function listSdEntries(relDir) {
    const r = await fetch('/api/sd/list?path=' + encodeURIComponent(relDir),
                          { cache: 'no-store' });
    if (!r.ok) return [];
    const d = await r.json();
    return d.entries || [];
}

// Presets sit either directly in the resolution folder (wallpapers filed flat,
// and everything saved before presets mirrored the wallpaper tree) or one level
// deeper, in the mirror of the wallpaper's category folder. Both are scanned.
async function listResolutionPresetFiles() {
    const root = await fetch('/api/sd/list?path=' + encodeURIComponent(LAYOUTS_DIR),
                             { cache: 'no-store' });
    if (!root.ok) return null;
    const data = await root.json();
    const dirs = (data.entries || []).filter(
        e => e.dir && /^\d+x\d+$/i.test(e.name));
    const isJson = e => !e.dir && /\.json$/i.test(e.name);
    const nested = await Promise.all(dirs.map(async dir => {
        const relDir = LAYOUTS_DIR + '/' + dir.name;
        const resolution = dir.name.toLowerCase();
        const entries = await listSdEntries(relDir);
        const asPreset = (baseDir, e) => ({
            name: e.name,
            rel: baseDir + '/' + e.name,
            resolution,
        });
        const here = entries.filter(isJson).map(e => asPreset(relDir, e));
        const deeper = await Promise.all(entries.filter(e => e.dir).map(async sub => {
            const subDir = relDir + '/' + sub.name;
            return (await listSdEntries(subDir)).filter(isJson)
                .map(e => asPreset(subDir, e));
        }));
        return here.concat(deeper.flat());
    }));
    return nested.flat();
}

async function checkOrphanPresets() {
    const status = document.getElementById('presetOrphanStatus');
    document.getElementById('presetOrphanList').innerHTML = '';
    status.textContent = 'Scanning…';
    try {
        const jsons = await listResolutionPresetFiles();
        if (jsons === null) {
            status.textContent = 'No presets found (SD card or ' + LAYOUTS_DIR +
                                 ' not available).';
            return;
        }
        if (!jsons.length) { status.textContent = 'No preset files found.'; return; }

        const cache = new Map();
        const problems = [];
        let okCount = 0;
        for (const e of jsons) {
            let refs = [];
            let invalid = '';
            try {
                const jr = await fetch('/api/sd/file?path=' + encodeURIComponent(e.rel),
                                       { cache: 'no-store' });
                if (!jr.ok) throw new Error('HTTP ' + jr.status);
                const preset = await jr.json();
                const expected = e.resolution.split('x').map(Number);
                if (preset.w !== expected[0] || preset.h !== expected[1]) {
                    invalid = `resolution stamp ${preset.w}×${preset.h} does not match ` +
                              e.resolution + ' directory';
                } else {
                    refs = collectWallpaperRefs(preset);
                }
            } catch (err) {
                invalid = 'unreadable or invalid JSON (' + err.message + ')';
            }
            if (invalid) {
                problems.push({ name: e.rel, rel: e.rel, detail: invalid });
                continue;
            }
            // If no full wallpaper paths were stored, look for <stem>.bin in the
            // standard wallpaper locations: first the exact mirror of where this
            // preset sits (a preset in .../320x240/home/ belongs to the wallpaper
            // in /wallpapers/320x240/home/), then this panel's resolution folder
            // and its "internet" subfolder (where the device drops fetched
            // internet wallpapers), plus the flat pre-resolution layout.
            if (!refs.length) {
                const stem = e.name.replace(/\.json$/i, '');
                const mirrored = e.rel.slice(LAYOUTS_DIR.length)   // /320x240/home/x.json
                                      .replace(/\/[^/]+$/, '');    // /320x240/home
                refs = ['/wallpapers' + mirrored,
                        '/wallpapers/' + e.resolution,
                        '/wallpapers/' + e.resolution + '/internet',
                        '/wallpapers',
                        '/wallpapers/saved',   // pre-resolution internet saves
                       ].map(dir => SD_MOUNT + dir + '/' + stem + '.bin');
            }
            const found = await Promise.all(
                refs.map(p => sdFileExists(p.slice(SD_MOUNT.length), cache)));
            if (found.some(x => x)) okCount++;
            else problems.push({
                name: e.rel,
                rel: e.rel,
                detail: 'missing wallpaper: ' + refs.join(', '),
            });
        }
        renderPresetProblems(problems, okCount);
    } catch (err) {
        status.textContent = 'Scan failed: ' + err.message;
    }
}

function renderPresetProblems(problems, okCount) {
    const status = document.getElementById('presetOrphanStatus');
    const list = document.getElementById('presetOrphanList');
    list.innerHTML = '';
    if (!problems.length) {
        status.textContent = '✓ All ' + okCount + ' preset(s) are valid.';
        return;
    }
    status.textContent = problems.length + ' problem preset(s) found, ' +
                         okCount + ' preset(s) OK.';

    problems.forEach(o => {
        const row = document.createElement('div');
        row.style.cssText = 'display:flex;align-items:center;gap:8px;padding:3px 0';
        const name = document.createElement('div');
        name.style.cssText = 'flex:1;font-family:monospace;font-size:12px;min-width:0';
        const title = document.createElement('div');
        title.textContent = '📄 ' + o.name;
        const missing = document.createElement('div');
        missing.style.cssText = 'opacity:.6;overflow-wrap:anywhere';
        missing.textContent = o.detail;
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

    if (problems.length > 1) {
        const all = document.createElement('button');
        all.type = 'button';
        all.className = 'btn-secondary';
        all.textContent = '🗑 Delete all problem presets';
        all.style.marginTop = '6px';
        all.onclick = async () => {
            if (!confirm('Delete ' + problems.length + ' problem preset file(s)?')) return;
            for (const o of problems) await deleteOrphanPreset(o);
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

// A wallpaper .bin is scaled to the panel, so — exactly like a layout preset —
// it only makes sense on one resolution and is filed under
// /wallpapers/<width>x<height>. Wallpapers left in the old flat /wallpapers by
// earlier firmware keep working: screens store the full path they were assigned
// and the browser falls back to the flat folder.
const WALLPAPERS_DIR = '/wallpapers';

// Screen → online-gallery category. The same slugs name both the catalog
// category on atlascube.net and the SD subfolder a wallpaper is filed under, so
// a card keeps the gallery's grouping. Radio and SD Player share one category;
// so do the two list screens (playlist, SD browser) — a list wallpaper is a
// frame around a list, and both frame the same shape.
const SECTION_WALLPAPER_CATEGORY = {
    clock: 'home',
    bt:    'wireless',
    radio: 'radio-sd-player',
    sd:    'radio-sd-player',
    eq:    'equalizer',
    playlist: 'playlist-sd-browser',
    browser:  'playlist-sd-browser',   // both list screens share one category
};

function sectionWallpaperCategory() {
    return SECTION_WALLPAPER_CATEGORY[state.active] || 'radio-sd-player';
}

function panelWallpaperDir() {
    return WALLPAPERS_DIR + '/' + state.meta.screen_w + 'x' + state.meta.screen_h;
}

// Per-screen wallpaper folder: this panel's resolution plus the category
// subfolder, e.g. /wallpapers/320x240/radio-sd-player.
function sectionWallpaperDir() {
    return panelWallpaperDir() + '/' + sectionWallpaperCategory();
}

// Where the "Upload" button and the SD browser start: the folder of the
// wallpaper this screen already uses, otherwise this screen's category folder.
function wallpaperDirectory() {
    if (!currentWallpaperPath.startsWith('/sdcard/')) return sectionWallpaperDir();
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
    setOnlineWallpaperGalleryOpen(false);
    browser.hidden = false;
    await browseWallpaperDirectory(wallpaperDirectory());
}

function browseWallpaperDirectory(path) {
    const browser = document.getElementById('layout_wallpaper_browser');
    if (!browser) return;
    // A card may have neither the category folder, the resolution folder nor
    // /wallpapers yet; step down to whichever exists. No Move here: uploads and
    // gallery installs file themselves per screen, and moving a .bin behind the
    // profile's back only leaves the screen pointing at a path that is gone.
    // The Assets tab keeps a browser that can move files.
    SdBrowse.open(browser, {
        start: path,
        fallback: [panelWallpaperDir(), WALLPAPERS_DIR, '/'],
        filterExt: '.bin',
        maxHeight: '190px',
        rowFontSize: '11px',
        emptyText: 'No .bin wallpapers in this folder.',
        // Flag the wallpaper currently applied to this screen with a check mark.
        fileLabel: (full, e) =>
            (currentWallpaperPath === '/sdcard' + full ? '\u2713 ' : '\u{1F5BC}\u{FE0F} ') + e.name,
        fileActions: full => [
            { label: '\ud83d\udc41', title: 'Preview',
              onClick: () => ensureLvBin().then(() => window.LvBin.openPreview(full)) },
        ],
        onFile: full => selectWallpaper(full),
    });
}

// Convert the picked image, store it on the SD card under `dir` and hand the
// resulting file to the regular selection flow (enables wallpapers globally if
// needed, sets the section override, offers the layout preset).
async function uploadWallpaperTo(dir, input) {
    const file = input.files && input.files[0];
    input.value = '';
    if (!file) return;
    const status = document.getElementById('layout_wallpaper_status');
    const note = msg => { if (status) status.textContent = msg; };
    try {
        await ensureLvBin();
        const saveAs = askWallpaperSaveAs(file.name);
        if (saveAs === null) {
            note('Upload cancelled.');
            return;
        }
        const relPath = await window.LvBin.uploadImage(
            file, dir, state.meta.screen_w, state.meta.screen_h, note, saveAs);
        await selectWallpaper(relPath);
    } catch (err) {
        note('Upload failed: ' + err.message);
    }
}

function askWallpaperSaveAs(suggestedFilename) {
    const suggested = String(suggestedFilename || 'wallpaper').replace(/\.[^.]*$/, '');
    const entered = window.prompt(
        'Save wallpaper as (.bin is added automatically; an existing file with the same name will be replaced):',
        suggested);
    if (entered === null) return null;
    const trimmed = entered.trim();
    return window.LvBin.fileStem(trimmed || suggested);
}

// ── Assets tab ────────────────────────────────────────────────────────────────
// Generic image → RGB565 .bin uploader for small UI artwork (knob images, …).
// Unlike the wallpaper uploader it takes an explicit size and folder and does
// not touch any screen's background — the resulting .bin is just parked on SD
// for a widget field to reference.

function assetDir() {
    const raw = (document.getElementById('asset_dir').value || '/assets/knobs').trim();
    return ('/' + raw).replace(/\/+/g, '/').replace(/\/+$/, '') || '/';
}

function assetFilePicked() {
    const input = document.getElementById('asset_file');
    const label = document.getElementById('asset_file_name');
    const file = input.files && input.files[0];
    if (label) label.textContent = file ? file.name : 'No file selected';
}

function fittedAssetDimensions(width, height) {
    const sourceWidth = Math.max(1, Number(width) || 64);
    const sourceHeight = Math.max(1, Number(height) || 64);
    const scale = Math.min(1, 480 / sourceWidth, 480 / sourceHeight);
    return {
        w: Math.max(4, Math.round(sourceWidth * scale)),
        h: Math.max(4, Math.round(sourceHeight * scale)),
    };
}

function askAssetDimensions(width, height) {
    const suggested = fittedAssetDimensions(width, height);
    const dimensions = window.prompt(
        'Output .bin resolution (WIDTHxHEIGHT, from 4x4 to 480x480):',
        `${suggested.w}x${suggested.h}`);
    if (dimensions === null) return null;

    const match = dimensions.trim().match(/^(\d+)\s*[x×]\s*(\d+)$/i);
    if (!match)
        throw new Error('Invalid resolution. Use WIDTHxHEIGHT, for example 64x64.');
    const w = Number(match[1]);
    const h = Number(match[2]);
    if (w < 4 || w > 480 || h < 4 || h > 480)
        throw new Error('Invalid resolution. Width and height must be between 4 and 480 px.');
    return { w, h };
}

async function imageFileDimensions(file) {
    if (typeof createImageBitmap === 'function') {
        const bitmap = await createImageBitmap(file);
        const dimensions = { width: bitmap.width, height: bitmap.height };
        bitmap.close();
        return dimensions;
    }

    const url = URL.createObjectURL(file);
    try {
        const image = new Image();
        image.src = url;
        await image.decode();
        return { width: image.naturalWidth, height: image.naturalHeight };
    } finally {
        URL.revokeObjectURL(url);
    }
}

async function uploadAsset() {
    const status = document.getElementById('asset_status');
    const note = msg => { if (status) status.textContent = msg; };
    const input = document.getElementById('asset_file');
    const file = input.files && input.files[0];
    if (!file) { note('Pick an image first.'); return; }
    const dir = assetDir();
    try {
        await ensureLvBin();
        const source = await imageFileDimensions(file);
        const dimensions = askAssetDimensions(source.width, source.height);
        if (dimensions === null) { note('Upload cancelled.'); return; }
        const stem = window.prompt(
            'Save asset as (.bin is added automatically; an existing file with the same name is replaced):',
            window.LvBin.fileStem(file.name));
        if (stem === null) { note('Upload cancelled.'); return; }
        const saveAs = window.LvBin.fileStem(stem.trim() || file.name);
        const relPath = await window.LvBin.uploadImage(
            file, dir, dimensions.w, dimensions.h, note, saveAs);
        input.value = '';
        assetFilePicked();   // reset the "chosen file" label
        // Widgets reference the fopen-ready "/sdcard/..." path (the 📂 SD picker
        // on a knob-image field fills exactly this).
        note('Saved. Reference as /sdcard' + relPath +
             '  (' + dimensions.w + '×' + dimensions.h + ').');
        browseAssets();
    } catch (err) {
        note('Upload failed: ' + err.message);
    }
}

// ── General tab: park a panel-sized wallpaper on SD ─────────────────────────
// Like the per-screen uploader (uploadWallpaperTo) but WITHOUT the selectWallpaper
// step — it scales the image to the panel resolution and stores it in this
// panel's wallpaper folder so it can be assigned per screen later, leaving
// every screen's current background untouched.

function generalWallpaperFilePicked() {
    const input = document.getElementById('general_wp_file');
    const label = document.getElementById('general_wp_file_name');
    const file = input.files && input.files[0];
    if (label) label.textContent = file ? file.name : 'No file selected';
}

async function uploadGeneralWallpaper() {
    const status = document.getElementById('general_wp_status');
    const note = msg => { if (status) status.textContent = msg; };
    const input = document.getElementById('general_wp_file');
    const file = input.files && input.files[0];
    if (!file) { note('Pick an image first.'); return; }
    const w = state.meta.screen_w, h = state.meta.screen_h;
    try {
        await ensureLvBin();
        const saveAs = askWallpaperSaveAs(file.name);
        if (saveAs === null) { note('Upload cancelled.'); return; }
        const relPath = await window.LvBin.uploadImage(
            file, panelWallpaperDir(), w, h, note, saveAs);
        input.value = '';
        generalWallpaperFilePicked();   // reset the "chosen file" label
        note('Saved to /sdcard' + relPath + ' (' + w + '×' + h +
             '). Assign it per screen with the 🗂 picker when ready.');
    } catch (err) {
        note('Upload failed: ' + err.message);
    }
}

function browseAssets() {
    const list = document.getElementById('asset_list');
    if (!list) return;
    SdBrowse.open(list, {
        start: assetDir(),
        filterExt: '.bin',
        fileIcon: '🖼️ ',
        emptyText: 'No .bin assets in this folder yet.',
        allowMkdir: true,
        allowMove: true,
        // Keep the Folder input in sync so an upload lands where you browsed to.
        onDirChange: dir => { const i = document.getElementById('asset_dir'); if (i) i.value = dir; },
        fileActions: full => [
            { label: '👁 Preview', title: 'Preview', onClick: () => window.LvBin.openPreview(full) },
            { label: '🗑 Delete',  title: 'Delete',  onClick: () => deleteAsset(full) },
        ],
    });
}

async function deleteAsset(path) {
    if (!window.confirm('Delete ' + path + '?')) return;
    const status = document.getElementById('asset_status');
    try {
        const r = await fetch('/api/sd/file?path=' + encodeURIComponent(path), { method: 'DELETE' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        if (status) status.textContent = 'Deleted ' + path + '.';
        browseAssets();
    } catch (err) {
        if (status) status.textContent = 'Delete failed: ' + err.message;
    }
}

// ── Generic SD .bin picker (modal) ──────────────────────────────────────────
// A self-contained folder browser that resolves to a picked .bin path via
// `onPick`. Used by widget fields (e.g. the knob image) to point at an existing
// asset without typing the path. Starts in `startDir`, falling back to the SD
// root if that folder does not exist yet.
function setOnlineAssetGalleryOpen(open) {
    const panel = document.getElementById('online_asset_gallery');
    const button = document.getElementById('asset_btn_online');
    if (panel) panel.hidden = !open;
    if (button) {
        button.style.background = open ? '#e5484d' : '';
        button.style.color = open ? '#fff' : '#ff6b6f';
    }
}

async function toggleOnlineAssetGallery() {
    const panel = document.getElementById('online_asset_gallery');
    if (!panel) return;
    const open = panel.hidden;
    setOnlineAssetGalleryOpen(open);
    if (open) await loadOnlineAssetGallery();
}

async function loadOnlineAssetGallery() {
    const panel = document.getElementById('online_asset_gallery');
    if (!panel) return;
    onlineWallpaperMessage(panel, 'Loading assets...');

    try {
        const endpoint = new URL(ONLINE_WALLPAPER_CATALOG);
        endpoint.searchParams.set('type', 'assets');
        const response = await fetch(endpoint.toString(), {
            cache: 'no-store',
            mode: 'cors',
        });
        if (!response.ok) {
            if (response.status === 400)
                throw new Error('the server catalog does not support assets yet');
            throw new Error('catalog HTTP ' + response.status);
        }
        const catalog = await response.json();
        if (catalog.type !== 'assets' || !Array.isArray(catalog.assets))
            throw new Error('invalid catalog response');
        renderOnlineAssetGallery(catalog);
    } catch (err) {
        onlineWallpaperMessage(panel,
            'Online gallery unavailable: ' + err.message +
            '. Check that this browser has internet access.');
    }
}

function renderOnlineAssetGallery(catalog) {
    const panel = document.getElementById('online_asset_gallery');
    if (!panel) return;
    panel.replaceChildren();

    const heading = document.createElement('div');
    heading.className = 'online-wallpaper-heading';
    const title = document.createElement('span');
    title.textContent = 'AtlasCube assets';
    const count = document.createElement('span');
    count.textContent = `${catalog.assets.length} available`;
    heading.append(title, count);
    panel.appendChild(heading);

    if (!catalog.assets.length) {
        onlineWallpaperMessage(panel, 'No assets published yet.');
        return;
    }

    const grid = document.createElement('div');
    grid.className = 'online-wallpaper-grid';
    grid.style.setProperty('--online-wallpaper-ratio', '1 / 1');

    for (const item of catalog.assets) {
        let imageUrl;
        try { imageUrl = trustedOnlineWallpaperUrl(item.image); }
        catch { continue; }

        const card = document.createElement('article');
        card.className = 'online-wallpaper-card';
        const image = document.createElement('img');
        image.src = imageUrl;
        image.alt = String(item.title || 'Online asset');
        image.loading = 'lazy';

        const body = document.createElement('div');
        body.className = 'online-wallpaper-body';
        const cardTitle = document.createElement('div');
        cardTitle.className = 'online-wallpaper-title';
        cardTitle.textContent = String(item.title || item.filename || 'Asset');
        cardTitle.title = cardTitle.textContent;
        const meta = document.createElement('div');
        meta.className = 'online-wallpaper-meta';
        meta.textContent = 'Loading source size...';
        image.addEventListener('load', () => {
            meta.textContent = `${image.naturalWidth}×${image.naturalHeight} source`;
        });
        image.addEventListener('error', () => {
            meta.textContent = 'Source size unavailable';
        });

        const actions = document.createElement('div');
        actions.className = 'online-wallpaper-actions';
        const preview = document.createElement('a');
        preview.href = imageUrl;
        preview.target = '_blank';
        preview.rel = 'noopener';
        preview.textContent = 'Preview';
        const install = document.createElement('button');
        install.type = 'button';
        install.textContent = 'Install';
        install.addEventListener('click', () => installOnlineAsset(item, install, image));
        actions.append(preview, install);
        body.append(cardTitle, meta, actions);
        card.append(image, body);
        grid.appendChild(card);
    }
    panel.appendChild(grid);
}

async function installOnlineAsset(item, button, previewImage) {
    const status = document.getElementById('asset_status');
    const note = message => { if (status) status.textContent = message; };
    const oldLabel = button.textContent;
    button.disabled = true;
    button.textContent = 'Working...';

    try {
        await ensureLvBin();
        if (previewImage && !previewImage.complete) {
            try { await previewImage.decode(); } catch (_) {}
        }
        const sourceWidth = previewImage?.naturalWidth || 0;
        const sourceHeight = previewImage?.naturalHeight || 0;
        const dimensions = askAssetDimensions(sourceWidth, sourceHeight);
        if (dimensions === null) {
            note('Installation cancelled.');
            return;
        }

        const suggested = window.LvBin.fileStem(item.filename || item.id || 'online-asset');
        const entered = window.prompt(
            'Save asset as (.bin is added automatically; an existing file with the same name is replaced):',
            suggested);
        if (entered === null) {
            note('Installation cancelled.');
            return;
        }

        const imageUrl = trustedOnlineWallpaperUrl(item.image);
        note('Downloading online asset...');
        const response = await fetch(imageUrl, { cache: 'no-store', mode: 'cors' });
        if (!response.ok) throw new Error('image HTTP ' + response.status);
        const declaredSize = Number(response.headers.get('content-length') || 0);
        if (declaredSize > ONLINE_WALLPAPER_MAX_BYTES)
            throw new Error('image is too large');
        const blob = await response.blob();
        if (blob.size > ONLINE_WALLPAPER_MAX_BYTES)
            throw new Error('image is too large');
        if (!/^image\/(png|jpeg|webp)$/i.test(blob.type))
            throw new Error('unsupported image format');

        const filename = String(item.filename || 'online-asset.png').split(/[\\/]/).pop();
        const file = new File([blob], filename, { type: blob.type });
        const saveAs = window.LvBin.fileStem(entered.trim() || suggested);
        const relPath = await window.LvBin.uploadImage(
            file, assetDir(), dimensions.w, dimensions.h, note, saveAs);
        note('Saved. Reference as /sdcard' + relPath +
             ` (${dimensions.w}×${dimensions.h}).`);
        browseAssets();
    } catch (err) {
        note('Online asset failed: ' + err.message);
    } finally {
        button.disabled = false;
        button.textContent = oldLabel;
    }
}

let sdPickerOverlay = null;

function ensureSdPicker() {
    if (sdPickerOverlay) return;
    const overlay = document.createElement('div');
    overlay.style.cssText =
        'position:fixed;inset:0;background:rgba(0,0,0,.78);display:none;' +
        // Deliberately below the preview overlay (lvbin.js) so a preview opened
        // from a file row surfaces on top of this modal, not behind it.
        'align-items:center;justify-content:center;z-index:1900;padding:20px';
    const box = document.createElement('div');
    box.style.cssText =
        'background:var(--bg-panel,#1b1b1b);border:1px solid var(--border,#333);' +
        'border-radius:10px;padding:14px;width:min(420px,94vw);max-height:90vh;' +
        'display:flex;flex-direction:column;gap:10px';
    const head = document.createElement('div');
    head.style.cssText = 'display:flex;justify-content:space-between;align-items:center';
    const title = document.createElement('span');
    title.textContent = 'Choose a .bin from SD';
    title.style.cssText = 'font-weight:600;font-size:13px';
    const close = document.createElement('button');
    close.className = 'btn-secondary';
    close.textContent = '✕';
    close.onclick = () => { overlay.style.display = 'none'; };
    head.append(title, close);
    const body = document.createElement('div');
    body.id = 'sd_picker_body';
    box.append(head, body);
    overlay.appendChild(box);
    overlay.addEventListener('click', e => { if (e.target === overlay) overlay.style.display = 'none'; });
    document.body.appendChild(overlay);
    sdPickerOverlay = overlay;
}

function openSdBinPicker(startDir, onPick) {
    ensureSdPicker();
    sdPickerOverlay.style.display = 'flex';
    const body = document.getElementById('sd_picker_body');
    // Default folder may not exist on a fresh card — fall back to root.
    SdBrowse.open(body, {
        start: startDir || '/',
        fallback: '/',
        filterExt: '.bin',
        fileIcon: '🖼️ ',
        maxHeight: '50vh',
        emptyText: 'No .bin files in this folder.',
        fileActions: full => [
            { label: '👁', title: 'Preview',
              onClick: () => ensureLvBin().then(() => window.LvBin.openPreview(full)) },
        ],
        onFile: full => {
            sdPickerOverlay.style.display = 'none';
            if (onPick) onPick(full);
        },
    });
}

function setOnlineWallpaperGalleryOpen(open) {
    const panel = document.getElementById('layout_online_wallpaper_gallery');
    const button = document.getElementById('layout_wp_btn_online');
    if (panel) panel.hidden = !open;
    if (button) {
        button.style.background = open ? '#e5484d' : '';
        button.style.color = open ? '#fff' : '#ff6b6f';
    }
}

async function toggleOnlineWallpaperGallery() {
    const panel = document.getElementById('layout_online_wallpaper_gallery');
    if (!panel) return;
    const open = panel.hidden;
    setOnlineWallpaperGalleryOpen(open);
    if (!open) return;

    const sdBrowser = document.getElementById('layout_wallpaper_browser');
    if (sdBrowser) sdBrowser.hidden = true;
    await loadOnlineWallpaperGallery();
}

function onlineWallpaperMessage(panel, message) {
    panel.replaceChildren();
    const note = document.createElement('div');
    note.className = 'online-wallpaper-message';
    note.textContent = message;
    panel.appendChild(note);
}

function trustedOnlineWallpaperUrl(raw) {
    const url = new URL(String(raw || ''));
    if (url.origin !== ONLINE_WALLPAPER_ORIGIN ||
        url.pathname !== '/wallpapers/asset.php') {
        throw new Error('catalog returned an untrusted image URL');
    }
    return url.toString();
}

async function loadOnlineWallpaperGallery() {
    const panel = document.getElementById('layout_online_wallpaper_gallery');
    if (!panel) return;
    // The gallery can be opened without ever visiting the Background tab, and it
    // renders a slot picker — so make sure the count is real first.
    await ensureNetWpSlotCount();
    const w = Number(state.meta.screen_w);
    const h = Number(state.meta.screen_h);
    const resolution = `${w}x${h}`;
    const category = sectionWallpaperCategory();
    onlineWallpaperMessage(panel, `Loading ${w} × ${h} wallpapers...`);

    try {
        const endpoint = new URL(ONLINE_WALLPAPER_CATALOG);
        endpoint.searchParams.set('resolution', resolution);
        endpoint.searchParams.set('category', category);
        const response = await fetch(endpoint.toString(), {
            cache: 'no-store',
            mode: 'cors',
        });
        if (!response.ok) throw new Error('catalog HTTP ' + response.status);
        const catalog = await response.json();
        if (catalog.resolution !== resolution || !Array.isArray(catalog.wallpapers))
            throw new Error('invalid catalog response');
        renderOnlineWallpaperGallery(catalog, category);
    } catch (err) {
        onlineWallpaperMessage(panel,
            'Online gallery unavailable: ' + err.message +
            '. Check that this browser has internet access.');
    }
}

function renderOnlineWallpaperGallery(catalog, category) {
    const panel = document.getElementById('layout_online_wallpaper_gallery');
    if (!panel) return;
    panel.replaceChildren();

    const heading = document.createElement('div');
    heading.className = 'online-wallpaper-heading';
    const title = document.createElement('span');
    const categoryLabel = catalog.categories?.[category] ||
        (category === 'home' ? 'Home' : category === 'wireless' ? 'Wireless' : 'Radio / SD Player');
    title.textContent = `${catalog.resolution} · ${categoryLabel}`;
    const count = document.createElement('span');
    count.textContent = `${catalog.wallpapers.length} available`;
    heading.append(title, count);
    panel.appendChild(heading);

    if (!catalog.wallpapers.length) {
        onlineWallpaperMessage(panel, 'No wallpapers published for this resolution yet.');
        return;
    }

    // Which internet slot the "Use in slot" buttons target. One control for the
    // whole gallery rather than one per card — the cards are a grid of many.
    const slotRow = document.createElement('div');
    slotRow.className = 'online-wallpaper-meta';
    slotRow.style.cssText = 'display:flex;align-items:center;gap:8px;margin:0 0 8px;flex-wrap:wrap';
    const slotLabel = document.createElement('span');
    slotLabel.textContent = 'Use in slot:';
    const slotSel = document.createElement('select');
    slotSel.id = 'online_wp_slot';
    slotSel.className = 'field-input';
    slotSel.style.cssText = 'width:auto;padding:4px 6px;font-size:11px';
    for (let i = 0; i < netWpSlotCount; i++) {
        const o = document.createElement('option');
        o.value = String(i);
        o.textContent = 'slot ' + (i + 1);
        slotSel.appendChild(o);
    }
    const slotHint = document.createElement('span');
    slotHint.style.opacity = '.75';
    slotHint.textContent = 'the device downloads it itself — no SD card needed';
    slotRow.append(slotLabel, slotSel, slotHint);
    panel.appendChild(slotRow);

    const grid = document.createElement('div');
    grid.className = 'online-wallpaper-grid';
    const [rw, rh] = catalog.resolution.split('x').map(Number);
    grid.style.setProperty('--online-wallpaper-ratio', `${rw} / ${rh}`);

    for (const item of catalog.wallpapers) {
        let imageUrl;
        try { imageUrl = trustedOnlineWallpaperUrl(item.image); }
        catch { continue; }

        const card = document.createElement('article');
        card.className = 'online-wallpaper-card';
        const image = document.createElement('img');
        image.src = imageUrl;
        image.alt = String(item.title || 'Online wallpaper');
        image.loading = 'lazy';

        const body = document.createElement('div');
        body.className = 'online-wallpaper-body';
        const cardTitle = document.createElement('div');
        cardTitle.className = 'online-wallpaper-title';
        cardTitle.textContent = String(item.title || item.filename || 'Wallpaper');
        cardTitle.title = cardTitle.textContent;
        const meta = document.createElement('div');
        meta.className = 'online-wallpaper-meta';
        meta.textContent = item.aiGenerated ? 'AI-generated artwork' : '';

        const actions = document.createElement('div');
        actions.className = 'online-wallpaper-actions';
        const preview = document.createElement('a');
        preview.href = imageUrl;
        preview.target = '_blank';
        preview.rel = 'noopener';
        preview.textContent = 'Preview';
        // Two ways to take a wallpaper: onto the SD card as a .bin (converted
        // here in the browser), or into a RAM slot the device downloads itself.
        // The slot route is the one that works without an SD card at all.
        const useSlot = document.createElement('button');
        useSlot.type = 'button';
        useSlot.textContent = 'Use in slot';
        useSlot.title = 'Store this URL in the selected internet slot and fetch it now';
        useSlot.addEventListener('click', () => useOnlineWallpaperInSlot(item, useSlot));
        const install = document.createElement('button');
        install.type = 'button';
        install.textContent = 'Install to SD';
        install.addEventListener('click', () => installOnlineWallpaper(item, install));
        actions.append(preview, useSlot, install);
        body.append(cardTitle, meta, actions);
        card.append(image, body);
        grid.appendChild(card);
    }
    panel.appendChild(grid);
}

// Hand a catalog wallpaper to the device instead of to the SD card: store its
// URL in an internet slot, have the device download and decode it, and pin the
// active screen to that slot. Nothing is written to SD and the browser converts
// nothing — this is the path for devices with no card at all.
async function useOnlineWallpaperInSlot(item, button) {
    const status = document.getElementById('layout_wallpaper_status');
    const note = message => { if (status) status.textContent = message; };
    const oldLabel = button.textContent;
    button.disabled = true;
    button.textContent = 'Working...';
    try {
        const imageUrl = trustedOnlineWallpaperUrl(item.image);

        // The device decodes JPEG only. Refusing an obvious PNG here beats
        // spending a fetch — and therefore a radio-stop window — on something
        // that can only fail on the device.
        if (/\.(png|webp|gif|bmp)(\?|$)/i.test(imageUrl)) {
            throw new Error('the device decodes JPEG only, and this file is not JPEG — ' +
                            'use "Install to SD", or publish a JPEG version in the gallery');
        }

        await ensureNetWpSlotCount();   // seeds netWpUrls — see the note there

        const slot = (() => {
            const sel = document.getElementById('online_wp_slot');
            const n = sel ? parseInt(sel.value, 10) : 0;
            return (n >= 0 && n < netWpSlotCount) ? n : 0;
        })();

        // Persist the URL into that slot, then fetch it. Keep the local mirror in
        // step so the Background tab shows the same thing without a reload.
        while (netWpUrls.length < netWpSlotCount) netWpUrls.push('');
        netWpUrls[slot] = imageUrl;
        note('Saving to slot ' + (slot + 1) + '...');
        await postDisplay({ wallpaper_urls: netWpUrls.slice(0, netWpSlotCount) });

        note('Device is downloading (the radio pauses)...');
        const r = await fetch('/api/wallpaper/fetch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ url: imageUrl, slot }),
        });
        const j = await r.json();
        if (j.result !== 'started') throw new Error('device is busy (' + j.result + ')');

        // Pin the screen the gallery was opened for, so the result is visible
        // straight away rather than only on whatever screen inherits slot 0.
        await setSectionWallpaper('net' + slot,
                                  'Screen shows internet slot ' + (slot + 1) + '.');
        buildNetWpSlotSelect();
        pollNetWallpaper();
        setOnlineWallpaperGalleryOpen(false);
    } catch (err) {
        note('Slot assignment failed: ' + err.message);
    } finally {
        button.disabled = false;
        button.textContent = oldLabel;
    }
}

async function installOnlineWallpaper(item, button) {
    const status = document.getElementById('layout_wallpaper_status');
    const note = message => { if (status) status.textContent = message; };
    const oldLabel = button.textContent;
    button.disabled = true;
    button.textContent = 'Working...';
    try {
        await ensureLvBin();
        const saveAs = askWallpaperSaveAs(item.filename || item.id || 'online-wallpaper');
        if (saveAs === null) {
            note('Installation cancelled.');
            return;
        }
        const imageUrl = trustedOnlineWallpaperUrl(item.image);
        note('Downloading online wallpaper...');
        const response = await fetch(imageUrl, { cache: 'no-store', mode: 'cors' });
        if (!response.ok) throw new Error('image HTTP ' + response.status);
        const declaredSize = Number(response.headers.get('content-length') || 0);
        if (declaredSize > ONLINE_WALLPAPER_MAX_BYTES)
            throw new Error('image is too large');
        const blob = await response.blob();
        if (blob.size > ONLINE_WALLPAPER_MAX_BYTES)
            throw new Error('image is too large');
        if (!/^image\/(png|jpeg|webp)$/i.test(blob.type))
            throw new Error('unsupported image format');

        const fallbackName = String(item.id || 'online-wallpaper') + '.png';
        const filename = String(item.filename || fallbackName).split(/[\\/]/).pop();
        const file = new File([blob], filename, { type: blob.type });
        const relPath = await window.LvBin.uploadImage(
            file, sectionWallpaperDir(), state.meta.screen_w, state.meta.screen_h,
            note, saveAs);
        await selectWallpaper(relPath);
        setOnlineWallpaperGalleryOpen(false);
    } catch (err) {
        note('Online wallpaper failed: ' + err.message);
    } finally {
        button.disabled = false;
        button.textContent = oldLabel;
    }
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
    // A per-screen SD file is honoured regardless of the global wallpaper
    // switch, so assigning one no longer touches global settings — that also
    // avoids dismissing a fetched internet wallpaper other screens may show.
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
        const path = effectiveWallpaperPath();
        currentWallpaperPath = path;
        updateWallpaperPickerLabel();

        // A fetched internet wallpaper shows on the General tier and on screens
        // set to Internet (until reboot or dismissal). The firmware serves its
        // pixels at /api/wallpaper/image, so preview them instead of the
        // gradient. A screen with an explicit SD file is unaffected.
        const ovr = sectionWallpaperValue();
        const ovrSlot  = netSlotOf(ovr);
        const isSdFile = ovr && ovr !== 'none' && ovrSlot < 0;
        if (!isSdFile) {
            // A screen pinned to a slot previews THAT slot; General previews
            // slot 0, which is what the firmware falls back to.
            const previewSlot = ovrSlot >= 0 ? ovrSlot : 0;
            try {
                const st = await fetch('/api/wallpaper/status', { cache: 'no-store' });
                const info = st.ok ? await st.json() : null;
                if (info && info.active && state.active === section) {
                    netWallpaperActive = true;
                    const label = document.getElementById('layout_wallpaper_name');
                    if (label) label.textContent = '(net wallpaper — until reboot)';
                    // The firmware serves the fetched pixels as an LVGL .bin —
                    // decode them exactly like an SD wallpaper. If this fails,
                    // the textual "net wallpaper" placeholder stays as fallback.
                    const img = await fetch('/api/wallpaper/image?slot=' + previewSlot,
                                            { cache: 'no-store' });
                    if (img.ok) {
                        await ensureLvBin();
                        const decoded = window.LvBin.decodeToCanvas(await img.arrayBuffer());
                        // Same stale-async guard as the SD path: the user may
                        // have switched tabs or assigned an SD file meanwhile.
                        if (state.active === section && !effectiveWallpaperPath()) {
                            wallpaperPreviewUrl = decoded.canvas.toDataURL('image/png');
                            wallpaperPreviewDim = clamp(display.wallpaper_dim || 0, 0, 100);
                        }
                    }
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
    // 'presets', 'general' and 'internet' aren't screens — they swap the editor
    // grid for a full-width card and leave state.active untouched, so returning
    // to any screen tab restores the editor exactly where it was.
    const isPresets  = name === 'presets';
    const isGeneral  = name === 'general';
    const isInternet = name === 'internet';
    const isAssets   = name === 'assets';
    const isSpecial  = isPresets || isGeneral || isInternet || isAssets;
    if (!isSpecial && !SECTIONS[name]) return;

    for (const tab of document.querySelectorAll('.section-tab')) {
        tab.classList.toggle('active', tab.dataset.section === name);
    }
    document.querySelector('.layout-grid').style.display = isSpecial ? 'none' : '';
    document.getElementById('presets_card').style.display  = isPresets  ? '' : 'none';
    document.getElementById('general_card').style.display  = isGeneral  ? '' : 'none';
    document.getElementById('internet_card').style.display = isInternet ? '' : 'none';
    document.getElementById('assets_card').style.display   = isAssets   ? '' : 'none';
    if (isAssets)               { browseAssets(); return; }
    if (isPresets)              { checkOrphanPresets(); return; }
    if (isGeneral || isInternet) {
        loadBackgroundTab();
        // The thumbnail costs a ~panel-sized download — fetch it only for the
        // tab that shows it, not every General visit.
        if (isInternet) refreshNetWpThumb();
        return;
    }

    if (state.active !== name) keyboardSelection = null;
    state.active = name;

    document.getElementById('form_section_title').textContent = SECTIONS[name].title;
    buildForm();
    updateLabelPlateControl();
    renderSvg();
    // Wallpapers are per screen — refresh the preview for this tab, then
    // repaint once the (async) decode lands.
    loadWallpaperPreview().then(renderSvg);
    // The online gallery's category follows the active screen; close it on a
    // tab switch so it can't show the previous screen's wallpapers (and reopens
    // fresh on demand instead of re-fetching the catalog on every jump).
    setOnlineWallpaperGalleryOpen(false);
}

// ── Background tabs (global) ──────────────────────────────────────────────────
// The "🎨 General" and "🌍 Internet" section tabs host the global background
// config that used to live in Settings → Display → Wallpapers: General owns the
// gradient/solid look plus the global wallpaper brightness (dim, applies to SD
// and internet wallpapers alike); Internet owns the internet-wallpaper
// fetch/schedule. Both tabs share one loader (loadBackgroundTab);
// every control posts straight to /api/settings and the device repaints itself.
let netWpTimer = null;

// Returns the request promise so callers that must sequence work after the
// settings landed can await it; the many fire-and-forget callers ignore it.
function postDisplay(patch) {
    return fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display: patch }),
    }).catch(console.error);
}

// Populate the tab's controls from the current settings.
// Fill the slot <select> once the count is known. Rebuilt rather than patched:
// the firmware's NET_WP_SLOTS is the authority and arrives with the status poll.
function buildNetWpSlotSelect() {
    const sel = document.getElementById('netWpSlot');
    if (sel) {
        const keep = sel.value;
        sel.textContent = '';
        for (let i = 0; i < netWpSlotCount; i++) {
            const o = document.createElement('option');
            o.value = String(i);
            o.textContent = 'Slot ' + (i + 1) + (netWpUrls[i] ? '' : ' (empty)');
            sel.appendChild(o);
        }
        sel.value = (keep && +keep < netWpSlotCount) ? keep : '0';
        netWpCurSlot = +sel.value;
    }

    // The per-screen picker is built before the slot count is known (it lives on
    // the canvas card, not this tab), so re-fill it here with the real count.
    const pick = document.getElementById('layout_wp_net_slot');
    if (pick && pick.options.length !== netWpSlotCount) {
        const keep = pick.value;
        pick.textContent = '';
        for (let i = 0; i < netWpSlotCount; i++) {
            const o = document.createElement('option');
            o.value = String(i);
            o.textContent = 'slot ' + (i + 1);
            pick.appendChild(o);
        }
        pick.value = (keep && +keep < netWpSlotCount) ? keep : '0';
        updateWallpaperPickerLabel();   // re-sync it with the active section
    }
}

// Switching slots persists whatever was typed for the previous one, so an edit
// is never lost by clicking away from it.
function netWpSlotChanged() {
    const sel = document.getElementById('netWpSlot');
    const field = document.getElementById('netWpUrl');
    const typed = field.value.trim();
    if (typed !== (netWpUrls[netWpCurSlot] || '')) {
        netWpUrls[netWpCurSlot] = typed;
        postDisplay({ wallpaper_urls: netWpUrls.slice(0, netWpSlotCount) });
    }
    netWpCurSlot = +sel.value;
    field.value = netWpUrls[netWpCurSlot] || '';
    syncNetWpPreset();
    refreshNetWpThumb();
}

// Slot count AND the current per-slot URLs, once per page load. Anything that
// renders a slot picker or patches one slot awaits this first.
//
// Seeding the URL mirror matters as much as the count: the patch sends the whole
// `wallpaper_urls` array, so writing one slot from an unseeded mirror would blank
// every other slot the user had configured.
async function ensureNetWpSlotCount() {
    if (netWpSlotKnown) return;
    netWpSlotKnown = true;
    try {
        const s = await fetch('/api/wallpaper/status', { cache: 'no-store' });
        if (s.ok) {
            const j = await s.json();
            if (j.slots > 0) netWpSlotCount = j.slots;
        }
    } catch { /* offline or old firmware — keep the default */ }

    if (!netWpUrls.length) {
        try {
            const r = await fetch('/api/settings', { cache: 'no-store' });
            if (r.ok) {
                const d = (await r.json()).display || {};
                netWpUrls = Array.isArray(d.wallpaper_urls) ? d.wallpaper_urls.slice()
                                                            : [d.wallpaper_url || ''];
            }
        } catch { /* leave it empty — the caller pads and only writes its own slot */ }
    }
}

async function loadBackgroundTab() {
    try {
        await ensureNetWpSlotCount();   // the pickers are sized from it

        const r = await fetch('/api/settings', { cache: 'no-store' });
        if (!r.ok) throw new Error('settings HTTP ' + r.status);
        const d = (await r.json()).display || {};

        const grad = d.bg_gradient !== false;   // default on
        document.getElementById('layoutBgGrad') ?.classList.toggle('active', grad);
        document.getElementById('layoutBgSolid')?.classList.toggle('active', !grad);

        // Slider shows brightness (100 - dim): right = brighter.
        const bright = 100 - (d.wallpaper_dim || 0);
        document.getElementById('wp_dim_slider').value = bright;
        document.getElementById('wp_dim_value').textContent = bright + '%';

        // Array form when the firmware has slots, the old scalar otherwise.
        netWpUrls = Array.isArray(d.wallpaper_urls) ? d.wallpaper_urls.slice()
                                                    : [d.wallpaper_url || ''];
        buildNetWpSlotSelect();
        document.getElementById('netWpUrl').value = netWpUrls[netWpCurSlot] || '';
        syncNetWpPreset();
        const mode = d.wallpaper_fetch_mode || 0;
        document.getElementById('netWpMode').value = String(mode);
        const timeEl = document.getElementById('netWpTime');
        timeEl.style.display = (mode === 2) ? '' : 'none';
        const p2 = n => String(n === undefined ? 0 : n).padStart(2, '0');
        timeEl.value = p2(d.wallpaper_fetch_hour === undefined ? 4 : d.wallpaper_fetch_hour) +
                       ':' + p2(d.wallpaper_fetch_min);
    } catch (err) {
        console.error('background load failed', err);
    }
}

// General background look — gradient or solid. wallpaper_on is forced off: the
// global SD wallpaper was retired in favour of per-screen sources.
function setGeneralBackground(mode) {
    const grad = mode === 'gradient';
    document.getElementById('layoutBgGrad') ?.classList.toggle('active', grad);
    document.getElementById('layoutBgSolid')?.classList.toggle('active', !grad);
    postDisplay({ wallpaper_on: false, bg_gradient: grad });
}

function setWallpaperDim(v) {
    postDisplay({ wallpaper_dim: parseInt(v, 10) || 0 });
}

// Show what the device will actually request: {w}/{h} expanded to the panel
// size, or a note that a placeholder-less URL is fetched as-is and scaled.
function updateNetWpResolved() {
    const el  = document.getElementById('netWpResolved');
    const url = document.getElementById('netWpUrl').value.trim();
    const w = state.meta && state.meta.screen_w, h = state.meta && state.meta.screen_h;
    if (!url || !w) { el.textContent = ''; return; }
    if (url.includes('{w}') || url.includes('{h}')) {
        el.textContent = '→ ' + url.replaceAll('{w}', w).replaceAll('{h}', h);
    } else {
        el.textContent = '→ fetched as-is, scaled to ' + w + '×' + h + ' on the device';
    }
}

function netWpPresetChanged() {
    const v = document.getElementById('netWpPreset').value;
    if (v) document.getElementById('netWpUrl').value = v;   // '' = Custom: keep what's typed
    updateNetWpResolved();
}

// The URL field is the single source of truth; the select just mirrors it
// (matching preset, or "Custom URL…" otherwise).
function syncNetWpPreset() {
    const url = document.getElementById('netWpUrl').value.trim();
    const sel = document.getElementById('netWpPreset');
    const match = Array.from(sel.options).find(o => o.value && o.value === url);
    sel.value = match ? match.value : '';
    updateNetWpResolved();
}

// Persist URL + auto-refresh mode/time in one patch; the firmware re-arms its
// scheduler on this POST.
// Capture the URL field into the slot array. Every action that persists or
// fetches goes through this, so the typed value and the stored slot agree.
function netWpCaptureUrl() {
    const url = document.getElementById('netWpUrl').value.trim();
    netWpUrls[netWpCurSlot] = url;
    return url;
}

function saveNetWpSchedule() {
    const mode = parseInt(document.getElementById('netWpMode').value, 10) || 0;
    const timeEl = document.getElementById('netWpTime');
    timeEl.style.display = (mode === 2) ? '' : 'none';
    const [h, m] = (timeEl.value || '04:00').split(':').map(Number);
    netWpCaptureUrl();
    postDisplay({
        wallpaper_urls:       netWpUrls.slice(0, netWpSlotCount),
        wallpaper_fetch_mode: mode,
        wallpaper_fetch_hour: h,
        wallpaper_fetch_min:  m,
    });
}

function fetchNetWallpaper() {
    const url = netWpCaptureUrl();
    const st  = document.getElementById('netWpStatus');
    if (!url) return;
    postDisplay({ wallpaper_urls: netWpUrls.slice(0, netWpSlotCount) });   // keep the stored slot in sync
    st.textContent = 'starting…';
    fetch('/api/wallpaper/fetch', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url, slot: netWpCurSlot }),
    }).then(r => r.json()).then(j => {
        if (j.result !== 'started') { st.textContent = j.result; return; }
        pollNetWallpaper();
    }).catch(() => { st.textContent = 'request failed'; });
}

// Refresh every configured slot the way the device does after a reboot: one
// batch, one radio-stop window, progress reported by the status poll.
function fetchAllNetWallpapers() {
    const st = document.getElementById('netWpStatus');
    netWpCaptureUrl();
    postDisplay({ wallpaper_urls: netWpUrls.slice(0, netWpSlotCount) });
    st.textContent = 'starting…';
    fetch('/api/wallpaper/fetch', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ all: true }),
    }).then(r => r.json()).then(j => {
        if (j.result !== 'started') { st.textContent = j.result; return; }
        pollNetWallpaper();
    }).catch(() => { st.textContent = 'request failed'; });
}

// Persist the fetched wallpaper on the SD card so it shows up in the per-screen
// SD picker. Background settings are untouched — this only collects the image.
function saveNetWallpaper() {
    const st = document.getElementById('netWpStatus');
    st.textContent = 'saving…';
    fetch('/api/wallpaper/save?slot=' + netWpCurSlot, { method: 'POST' })
        .then(r => r.json())
        .then(j => {
            st.textContent = (j.result === 'ok') ? ('saved: ' + j.path)
                                                 : (j.error || 'save failed');
        })
        .catch(() => { st.textContent = 'request failed'; });
}

function pollNetWallpaper() {
    clearTimeout(netWpTimer);
    fetch('/api/wallpaper/status').then(r => r.json()).then(j => {
        // A batch says which slot it is on, so a ten-slot refresh does not look
        // like a hung "busy".
        const batch = j.total > 1 && j.status === 'busy';
        const text  = batch ? ('busy ' + Math.min(j.done + 1, j.total) + '/' + j.total)
                            : j.status;
        // The Background tab's status line only exists while that tab is built —
        // a fetch started from the gallery reports into the picker line instead.
        const el = document.getElementById('netWpStatus') ||
                   document.getElementById('layout_wallpaper_status');
        if (el) el.textContent = text;
        if (j.status === 'busy') {
            netWpTimer = setTimeout(pollNetWallpaper, 1000);
        } else if (j.status === 'ok') {
            if (Array.isArray(j.filled)) buildNetWpSlotSelect();
            // A fresh image landed — update the tab thumbnail and the layout
            // preview. Retries cover the beat between the fetch task reporting
            // "ok" and the LVGL task committing the buffer.
            refreshNetWpThumb(3);
            loadWallpaperPreview().then(renderSvg);
        }
    }).catch(console.error);
}

// Thumbnail of the fetched wallpaper under "Fetch now" — the same .bin decode
// as the layout preview. Hidden when the device has nothing fetched.
async function refreshNetWpThumb(retries = 0) {
    const img = document.getElementById('netWpPreviewImg');
    if (!img) return;
    try {
        const r = await fetch('/api/wallpaper/image?slot=' + netWpCurSlot, { cache: 'no-store' });
        if (!r.ok) throw new Error('HTTP ' + r.status);
        await ensureLvBin();
        const decoded = window.LvBin.decodeToCanvas(await r.arrayBuffer());
        img.src = decoded.canvas.toDataURL('image/png');
        img.style.display = '';
    } catch {
        if (retries > 0) {
            setTimeout(() => refreshNetWpThumb(retries - 1), 700);
        } else {
            img.style.display = 'none';
        }
    }
}

// ── Form ────────────────────────────────────────────────────────────────────

function buildForm() {
    const root  = document.getElementById('form_section');
    const data  = state[state.active];
    const fields = SECTIONS[state.active].fields;
    const fieldByKey = new Map(fields.map(f => [f.key, f]));
    const groups = FORM_GROUPS[state.active];
    root.querySelectorAll('.form-group, .form-category').forEach(n => n.remove());

    if (!openFormGroups[state.active]) {
        // Start with every group collapsed.
        openFormGroups[state.active] = new Set();
    }

    groups.forEach((group, groupIndex) => {
        if (group.heading) {
            const cat = document.createElement('div');
            cat.className = 'form-category';
            cat.textContent = group.heading;
            root.appendChild(cat);
            return;
        }
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

// Colour overrides round-trip as 0xRRGGBB integers (0 = inherit theme); the
// <input type="color"> speaks '#rrggbb'.
function numToHex(n) {
    return '#' + (((n | 0) >>> 0) & 0xffffff).toString(16).padStart(6, '0');
}
function hexToNum(h) {
    return parseInt(h.slice(1), 16) | 0;
}
// '#rrggbb' for an override, null when the field is unset — renderers pass that
// straight to a fill and leave the placeholder styling in place.
function colorOrNull(n) {
    return n ? numToHex(n) : null;
}

function buildFormRow(field, data, group, details) {
    const row = document.createElement('div');
    row.className = 'form-row';
    row.dataset.field = field.key;
    // Every row except the toggle itself disappears when the group is off.
    if (group.enabledBy && field.key !== groupToggleKey(group))
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
    } else if (field.type === 'text') {
        input = document.createElement('input');
        input.type = 'text';
        input.value = data[field.key] ?? field.default ?? '';
        if (field.placeholder) input.placeholder = field.placeholder;
        input.addEventListener('input', () => {
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
    } else if (field.type === 'action') {
        // A button, not a profile field: it edits other fields (and never lands
        // in the POSTed section object, since nothing writes data[field.key]).
        input = document.createElement('button');
        input.type = 'button';
        input.className = 'btn-secondary';
        input.textContent = field.button;
        input.addEventListener('click', () => {
            field.action(data);
            refreshGroup(details, group, data);
        });
    } else if (field.type === 'color') {
        // 0 = inherit the theme colour. A checkbox gates the picker so "no
        // override" stays representable (a colour input always holds a value).
        input = document.createElement('span');
        input.className = 'color-field';
        const en = document.createElement('input');
        en.type = 'checkbox';
        en.title = 'Override theme colour';
        const pick = document.createElement('input');
        pick.type = 'color';
        pick.style.marginLeft = '6px';
        const cur = (data[field.key] | 0) >>> 0;
        en.checked = cur !== 0;
        pick.value = numToHex(cur || (field.default ?? 0xffffff));
        pick.disabled = !en.checked;
        const commit = () => {
            pick.disabled = !en.checked;
            // Nudge pure black to 0x000001 so it isn't read back as "inherit".
            data[field.key] = en.checked ? (hexToNum(pick.value) || 1) : 0;
            refreshGroup(details, group, data);
            renderSvg();
        };
        en.addEventListener('change', commit);
        pick.addEventListener('input', commit);
        input.appendChild(en);
        input.appendChild(pick);
    }
    input.id = 'fld_' + field.key;
    row.appendChild(input);

    // Optional "pick a .bin from SD" button that fills a text path field.
    if (field.type === 'text' && field.sdPicker) {
        const pick = document.createElement('button');
        pick.type = 'button';
        pick.className = 'btn-secondary';
        pick.textContent = '📂 SD';
        pick.title = 'Choose a .bin from the SD card';
        pick.style.marginLeft = '6px';
        pick.addEventListener('click', () => {
            openSdBinPicker(field.sdPicker.dir || '/', full => {
                // /api/sd/list paths are mount-relative; the firmware fopen()s
                // the "/sdcard/..." path, so store that in the profile field.
                const stored = '/sdcard' + full;
                input.value = stored;
                data[field.key] = stored;
                refreshGroup(details, group, data);
                renderSvg();
            });
        });
        row.appendChild(pick);
    }
    return row;
}

// `enabledBy` names the bool that gates a group. Prefix it with '!' when the
// stored flag is inverted (a *_hide field) — that way a group can be gated
// either way round without a second, mirror-image option.
function groupToggleKey(group) {
    const key = group.enabledBy;
    return key && key[0] === '!' ? key.slice(1) : key;
}

function groupEnabled(group, data) {
    if (!group.enabledBy) return true;
    const on = !!data[groupToggleKey(group)];
    return group.enabledBy[0] === '!' ? !on : on;
}

function refreshGroup(details, group, data) {
    const enabled = groupEnabled(group, data);
    details.classList.toggle('is-disabled', !enabled);
    const body = details.children[1];
    if (body) {
        for (const row of body.children) {
            // Nested subgroups belong to their parent's toggle just as much as
            // its own rows do: hiding the header hides the title and legend
            // that live inside it.
            if (row.classList.contains('form-row-conditional') ||
                row.classList.contains('form-subgroup')) row.hidden = !enabled;
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
    if (!groupEnabled(group, data)) return 'Off';
    // Groups whose geometry is derived rather than stored field-by-field (the EQ
    // sliders group) report it themselves — the _x/_w sniffing below cannot.
    if (group.summary) return group.summary(data);
    if (group.subgroups) {
        // A subgroup without an enabledBy toggle is always-on (like a top-level
        // group without one), so count it as enabled rather than undercounting.
        const enabled = group.subgroups.filter(s => groupEnabled(s, data)).length;
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
            font: c.clock_time_font, hugsText: true,
            text: '88:88', textSize: fh,
            textFill: c.clock_time_color ? numToHex(c.clock_time_color) : null,
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
            font: c.clock_date_font, hugsText: true,
            text: 'Mon  2026-05-01', textSize: fh,
            textFill: c.clock_date_color ? numToHex(c.clock_date_color) : null,
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
            font: c.clock_netinfo_font, hugsText: true,
            text: '192.168.1.50  host.local', textSize: fh,
            textFill: c.clock_netinfo_color ? numToHex(c.clock_netinfo_color) : null,
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
            font: c.clock_calendar_font,
            text: '18:30  Dentist appt.', textSize: fh,
        });
    }

    if (c.clock_show_weather) {
        // Weather line — top-left anchored, width for centered text (0 → full).
        const fh = fontHeight(c.clock_weather_font);
        const ww = c.clock_weather_w > 0 ? c.clock_weather_w : W;
        drawWeatherElement(svg, {
            x: c.clock_weather_x, y: c.clock_weather_y, w: ww, h: fh,
            label: 'weather',
            fields: { x: 'clock_weather_x', y: 'clock_weather_y', w: 'clock_weather_w' },
            font: c.clock_weather_font,
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
        font: c.clock_strip_station_font,
        text: 'Atlas Radio', textSize: stFh,
        textFill: c.clock_strip_station_color ? numToHex(c.clock_strip_station_color) : null,
    });
    const tiFh = fontHeight(c.clock_strip_title_font);
    const tiW  = c.clock_strip_title_w ?? c.clock_strip_label_w ?? sw;
    drawFreeElement(svg, {
        x: sx + (sw - tiW) / 2 + (c.clock_strip_title_x | 0),
        y: sy + c.clock_strip_title_y, w: tiW, h: tiFh,
        label: 'title', cls: 'label-rect',
        fields: { x: 'clock_strip_title_x', y: 'clock_strip_title_y',
                  w: 'clock_strip_title_w' },
        font: c.clock_strip_title_font,
        text: 'Song title', textSize: tiFh,
        textFill: c.clock_strip_title_color ? numToHex(c.clock_strip_title_color) : null,
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
              'brand', { x: 'bt_brand_x', y: 'bt_brand_y' }, true,
              b.bt_brand_color ? numToHex(b.bt_brand_color) : null);
    drawLabel(svg, b.bt_status_x, b.bt_status_y, b.bt_status_font, 'Connected',
              'status', { x: 'bt_status_x', y: 'bt_status_y' }, true,
              b.bt_status_color ? numToHex(b.bt_status_color) : null);

    // Track title — scrolling label, fixed width
    const titleFh = fontHeight(b.bt_title_font);
    drawFreeElement(svg, {
        x: b.bt_title_x, y: b.bt_title_y, w: b.bt_title_w, h: titleFh,
        label: 'title', cls: 'label-rect',
        fields: { x: 'bt_title_x', y: 'bt_title_y', w: 'bt_title_w' },
        font: b.bt_title_font,
        text: 'Track title', textSize: titleFh,
        textFill: b.bt_title_color ? numToHex(b.bt_title_color) : null,
    });

    // Artist — scrolling label, fixed width
    const artistFh = fontHeight(b.bt_artist_font);
    drawFreeElement(svg, {
        x: b.bt_artist_x, y: b.bt_artist_y, w: b.bt_artist_w, h: artistFh,
        label: 'artist', cls: 'label-rect',
        fields: { x: 'bt_artist_x', y: 'bt_artist_y', w: 'bt_artist_w' },
        font: b.bt_artist_font,
        text: 'Artist', textSize: artistFh,
        textFill: b.bt_artist_color ? numToHex(b.bt_artist_color) : null,
    });

    // Time "0:00 / 0:00"
    drawLabel(svg, b.bt_time_x, b.bt_time_y, b.bt_time_font, '0:00 / 0:00',
              'time', { x: 'bt_time_x', y: 'bt_time_y' }, true,
              b.bt_time_color ? numToHex(b.bt_time_color) : null);

    // Vol label — independently positioned center-anchored element.
    drawLabel(svg, b.bt_vol_x, b.bt_vol_y, b.bt_vol_label_font, 'VOL: 50%',
              'vol', { x: 'bt_vol_x', y: 'bt_vol_y' }, true,
              b.bt_vol_color ? numToHex(b.bt_vol_color) : null);

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
    drawVolSlider(svg, 'bt', b);
    drawTouchHotspots(svg, 'bt', b);
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
        // Station and title are independent single-line boxes (x/y/w each);
        // text is centered in the box and scrolls when it doesn't fit.
        const stationFh = fontHeight(r.radio_np_station_font);
        drawFreeElement(svg, {
            x: r.radio_np_x, y: r.radio_np_y,
            w: Math.max(r.radio_np_w | 0, 8), h: stationFh,
            label: 'np_station', cls: 'label-rect',
            fields: { x: 'radio_np_x', y: 'radio_np_y', w: 'radio_np_w' },
            font: r.radio_np_station_font,
            text: 'Atlas Radio', textSize: stationFh,
            textFill: r.radio_np_color ? numToHex(r.radio_np_color) : null,
        });
        if (r.radio_show_np_title) {
            const titleFh = fontHeight(r.radio_np_title_font);
            drawFreeElement(svg, {
                x: r.radio_title_x, y: r.radio_title_y,
                w: Math.max(r.radio_title_w | 0, 8), h: titleFh,
                label: 'np_title', cls: 'label-rect',
                fields: { x: 'radio_title_x', y: 'radio_title_y', w: 'radio_title_w' },
                font: r.radio_np_title_font,
                text: 'Title — Artist', textSize: titleFh,
                textFill: r.radio_title_color ? numToHex(r.radio_title_color) : null,
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
        drawLabel(svg, r.radio_state_x, r.radio_state_y, r.radio_state_font, 'PLAYING',
                  'state', { x: 'radio_state_x', y: 'radio_state_y' }, true,
                  r.radio_state_color ? numToHex(r.radio_state_color) : null);
    }
    // Audio-info split — independent center-anchored labels, one shared font
    // and one shared colour override.
    const rInfoFill = r.radio_info_color ? numToHex(r.radio_info_color) : null;
    if (r.radio_samplerate_show) {
        drawLabel(svg, r.radio_samplerate_x, r.radio_samplerate_y, r.radio_audio_info_font,
                  '44100 Hz', 'rate',
                  { x: 'radio_samplerate_x', y: 'radio_samplerate_y' }, true, rInfoFill);
    }
    if (r.radio_channels_show) {
        drawLabel(svg, r.radio_channels_x, r.radio_channels_y, r.radio_audio_info_font,
                  'STEREO', 'ch',
                  { x: 'radio_channels_x', y: 'radio_channels_y' }, true, rInfoFill);
    }
    if (r.radio_bitrate_show) {
        drawLabel(svg, r.radio_bitrate_x, r.radio_bitrate_y, r.radio_audio_info_font,
                  '128 kbps', 'kbps',
                  { x: 'radio_bitrate_x', y: 'radio_bitrate_y' }, true, rInfoFill);
    }
    if (r.radio_volume_show) {
        drawLabel(svg, r.radio_volume_x, r.radio_volume_y, r.radio_audio_info_font,
                  'VOL: 42%', 'vol',
                  { x: 'radio_volume_x', y: 'radio_volume_y' }, true, rInfoFill);
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
    if (r.radio_stereo_show_l) {
        drawFreeElement(svg, {
            x: r.radio_stereo_l_x, y: r.radio_stereo_l_y,
            w: r.radio_stereo_l_w, h: r.radio_stereo_l_h,
            label: 'BAR-L', cls: 'label-rect',
            fields: { x: 'radio_stereo_l_x', y: 'radio_stereo_l_y',
                      w: 'radio_stereo_l_w', h: 'radio_stereo_l_h' },
        });
    }
    if (r.radio_stereo_show_r) {
        drawFreeElement(svg, {
            x: r.radio_stereo_r_x, y: r.radio_stereo_r_y,
            w: r.radio_stereo_r_w, h: r.radio_stereo_r_h,
            label: 'BAR-R', cls: 'label-rect',
            fields: { x: 'radio_stereo_r_x', y: 'radio_stereo_r_y',
                      w: 'radio_stereo_r_w', h: 'radio_stereo_r_h' },
        });
    }
    drawVolSlider(svg, 'radio', r);
    if (r.radio_show_weather) {
        const fh = fontHeight(r.radio_weather_font);
        const ww = r.radio_weather_w > 0 ? r.radio_weather_w : W;
        drawWeatherElement(svg, {
            x: r.radio_weather_x, y: r.radio_weather_y, w: ww, h: fh,
            label: 'weather',
            fields: { x: 'radio_weather_x', y: 'radio_weather_y', w: 'radio_weather_w' },
            font: r.radio_weather_font,
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
        font: s.sd_title_font,
        text: 'Artist - Title', textSize: sdTitleFh,
        textFill: s.sd_title_color ? numToHex(s.sd_title_color) : null,
    });
    if (s.sd_show_folder) {
        drawLabel(svg, s.sd_folder_x, s.sd_folder_y, s.sd_folder_font, 'Folder   3/12',
                  'folder', { x: 'sd_folder_x', y: 'sd_folder_y' }, true,
                  s.sd_folder_color ? numToHex(s.sd_folder_color) : null);
    }
    // Info row split — volume, status flags and the counter are independent
    // labels sharing one font and one colour override.
    const sdInfoFill = s.sd_info_color ? numToHex(s.sd_info_color) : null;
    if (s.sd_volume_show) {
        drawLabel(svg, s.sd_volume_x, s.sd_volume_y, s.sd_info_font, 'VOL: 42%',
                  'vol', { x: 'sd_volume_x', y: 'sd_volume_y' }, true, sdInfoFill);
    }
    if (s.sd_status_show) {
        drawLabel(svg, s.sd_status_x, s.sd_status_y, s.sd_info_font,
                  'SHUFFLE   REPEAT ALL',
                  'status', { x: 'sd_status_x', y: 'sd_status_y' }, true, sdInfoFill);
    }

    if (s.sd_show_time) {
        drawLabel(svg, s.sd_time_x, s.sd_time_y, s.sd_info_font, '1:23 / 4:56',
                  'time', { x: 'sd_time_x', y: 'sd_time_y' }, true, sdInfoFill);
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
    if (s.sd_stereo_show_l) {
        drawFreeElement(svg, {
            x: s.sd_stereo_l_x, y: s.sd_stereo_l_y,
            w: s.sd_stereo_l_w, h: s.sd_stereo_l_h,
            label: 'BAR-L', cls: 'label-rect',
            fields: { x: 'sd_stereo_l_x', y: 'sd_stereo_l_y',
                      w: 'sd_stereo_l_w', h: 'sd_stereo_l_h' },
        });
    }
    if (s.sd_stereo_show_r) {
        drawFreeElement(svg, {
            x: s.sd_stereo_r_x, y: s.sd_stereo_r_y,
            w: s.sd_stereo_r_w, h: s.sd_stereo_r_h,
            label: 'BAR-R', cls: 'label-rect',
            fields: { x: 'sd_stereo_r_x', y: 'sd_stereo_r_y',
                      w: 'sd_stereo_r_w', h: 'sd_stereo_r_h' },
        });
    }
    drawVolSlider(svg, 'sd', s);
    if (s.sd_show_weather) {
        const fh = fontHeight(s.sd_weather_font);
        const ww = s.sd_weather_w > 0 ? s.sd_weather_w : state.meta.screen_w;
        drawWeatherElement(svg, {
            x: s.sd_weather_x, y: s.sd_weather_y, w: ww, h: fh,
            label: 'weather',
            fields: { x: 'sd_weather_x', y: 'sd_weather_y', w: 'sd_weather_w' },
            font: s.sd_weather_font,
            text: '+21 C  Partly cloudy  54%', textSize: fh,
        });
    }
    drawTouchHotspots(svg, 'sd', s);
}

// ── EQUALIZER renderer ──────────────────────────────────────────────────────
// Movable pieces: value label, sliders group (drag to move, corners change the
// gap and the slider height), response-curve box, and legend. The bands
// schematic inside the group uses a representative curve so the preview reads as
// an equaliser.
const EQ_FREQ_LABELS = ['31', '62', '125', '250', '500', '1k', '2k', '4k', '8k', '16k'];
const EQ_PREVIEW_LEVELS = [0.68, 0.55, 0.62, 0.40, 0.50, 0.70, 0.46, 0.60, 0.74, 0.52];

// Mirror of band_x() in screen_equalizer.c + ui_profile_eq_group_box(): the ten
// bands are spread across the span with per-band rounding, so band 0 sits on the
// left edge, band 9 ends on the right edge, and individual gaps may differ by 1 px.
// Knob artwork is deliberately NOT part of the geometry — it is only drawn on its
// band and may overhang — otherwise a wide knob would put a floor of 10 × its width
// under the group. Only freqArea is an approximation (the device measures its
// frequency font, which is not editable here); the band positions are exact.
function eqGeom(e) {
    const n         = EQ_FREQ_LABELS.length;
    const sliderW   = Math.max(1, e.eq_slider_w | 0);
    const gw        = Math.max(sliderW, e.eq_group_w | 0);
    const travel    = gw - sliderW;                 // room shared by the 9 steps
    const step      = Math.max(1, Math.round(travel / (n - 1)));
    const freqShown = !e.eq_freq_hide;
    const labelFh   = freqShown ? clamp(Math.round(step * 0.7), 7, 14) : 0;
    const freqArea  = freqShown ? labelFh + 4 : 0;
    const sliderH   = Math.max(4, e.eq_slider_h | 0);
    // Knob artwork width: its own setting, or the whole band column when auto.
    const hasKnob   = !!(e.eq_knob_image && e.eq_knob_image.trim());
    const knobW     = !hasKnob ? 0 : ((e.eq_knob_w | 0) > 0 ? (e.eq_knob_w | 0) : step);
    return {
        n, sliderW, gw, travel, step, freqShown, labelFh, freqArea, sliderH,
        hasKnob, knobW,
        bandX: i => Math.round(i * travel / (n - 1)),
        avgGap: travel / (n - 1) - sliderW,
        gh: sliderH + freqArea,
    };
}

// "Centre on screen" — with a derived width this lands to the pixel, which is
// what makes a group line up with a symmetric wallpaper.
function centreEqGroup() {
    const e = state.eq;
    e.eq_group_x = Math.round((state.meta.screen_w - eqGeom(e).gw) / 2);
    setFormValue('eq_group_x', e.eq_group_x);
    renderSvg();
}

function eqGroupSummary(data) {
    const g = eqGeom(data);
    const overlap = g.knobW > g.step ? ' · knobs overlap' : '';
    return `${data.eq_group_x | 0}, ${data.eq_group_y | 0} · ${g.gw}×${g.gh}` +
           ` · gap ~${g.avgGap.toFixed(1)}${overlap}`;
}

// ── List screens (playlist / SD browser) ────────────────────────────────────
// Mirror of UI_LIST_BOX_PAD: the inner padding of the list box, which the
// firmware subtracts to get the row width.
const LIST_BOX_PAD = 2;

function listPitch(d, prefix) {
    return Math.max(1, (d[`${prefix}_item_h`] | 0) + (d[`${prefix}_item_pad`] | 0));
}

function listBoxSummary(d, prefix) {
    const inner = (d[`${prefix}_list_h`] | 0) - 2 * LIST_BOX_PAD;
    const rows = Math.max(0, Math.floor(inner / listPitch(d, prefix)));
    return `${d[`${prefix}_list_x`] | 0}, ${d[`${prefix}_list_y`] | 0} · ` +
           `${d[`${prefix}_list_w`] | 0}×${d[`${prefix}_list_h`] | 0} · ~${rows} rows`;
}

// One renderer for both list screens: same geometry, only the sample content
// and the legend text differ.
function renderList(svg, prefix, title, hint, names) {
    const p = state[prefix];
    const W = state.meta.screen_w;
    const headerH = p[`${prefix}_header_hide`] ? 0 : (p[`${prefix}_header_h`] | 0);

    // Header strip: always the full-width top strip on the device, so only its
    // height is editable (form) — there is nothing to drag horizontally.
    if (headerH > 0) {
        rect(svg, { x: 0, y: 0, width: W, height: headerH, class: 'label-rect ph-container' });
        tag(svg, 2, 7, 'header');

        const hf  = p[`${prefix}_header_font`];
        const ty  = headerH / 2 + (p[`${prefix}_label_y`] | 0) - fontLineHeight(hf) / 2;
        text(svg, (p[`${prefix}_label_x`] | 0) + 2, ty + fontBaseline(hf), title,
             { 'font-size': fontHeight(hf) });

        if (!p[`${prefix}_hint_hide`]) {
            const rf = p[`${prefix}_row_font`];
            const hy = headerH / 2 + (p[`${prefix}_hint_y`] | 0) - fontLineHeight(rf) / 2;
            // Right-aligned on the device (RIGHT_MID + a negative offset).
            text(svg, W + (p[`${prefix}_hint_x`] | 0) - 2, hy + fontBaseline(rf), hint,
                 { 'font-size': fontHeight(rf), 'text-anchor': 'end' });
        }
    }

    // The list box — the one element to line up with a wallpaper. Rows are drawn
    // inside it as decorations: their width is derived from the box, so there is
    // nothing per-row to drag.
    const lx = p[`${prefix}_list_x`] | 0, ly = p[`${prefix}_list_y`] | 0;
    const lw = p[`${prefix}_list_w`] | 0, lh = p[`${prefix}_list_h`] | 0;
    if (lw <= 0 || lh <= 0) return;

    drawFreeElement(svg, {
        x: lx, y: ly, w: lw, h: lh,
        label: 'list', cls: 'label-rect ph-container',
        fillOpacity: 0.12,
        fields: { x: `${prefix}_list_x`, y: `${prefix}_list_y`,
                  w: `${prefix}_list_w`, h: `${prefix}_list_h` },
    });

    const pitch  = listPitch(p, prefix);
    const rowH   = Math.max(1, p[`${prefix}_item_h`] | 0);
    const rowW   = Math.max(8, lw - 2 * LIST_BOX_PAD);
    const rowX   = lx + LIST_BOX_PAD;
    const indent = p[`${prefix}_row_pad_left`] | 0;
    const rf     = p[`${prefix}_row_font`];
    const opa    = clamp(p[`${prefix}_label_bg_opa`] ?? 100, 0, 100) / 100;

    // Unset colours keep the placeholder classes, so the preview still reads as
    // "follows the theme" rather than committing to one palette's colours.
    const rowFill    = colorOrNull(p[`${prefix}_row_bg_color`]);
    const rowText    = colorOrNull(p[`${prefix}_row_text_color`]);
    const cursorFill = colorOrNull(p[`${prefix}_cursor_bg_color`]);
    const cursorText = colorOrNull(p[`${prefix}_cursor_text_color`]);

    for (let i = 0; ; i++) {
        const y = ly + LIST_BOX_PAD + i * pitch;
        if (y + rowH > ly + lh - LIST_BOX_PAD) break;
        const isCursor = i === 0;             // the first row stands in for the encoder cursor
        const r = rect(svg, {
            x: rowX, y, width: rowW, height: rowH,
            class: `label-rect ${isCursor ? 'ph-media-primary' : 'ph-default'}`,
        });
        const fill = isCursor ? cursorFill : rowFill;
        if (fill) r.style.fill = fill;
        r.style.fillOpacity = opa;
        r.style.pointerEvents = 'none';       // the box underneath stays draggable
        const t = text(svg, rowX + indent,
                       y + (rowH - fontLineHeight(rf)) / 2 + fontBaseline(rf),
                       names[i % names.length],
                       { 'font-size': fontHeight(rf) });
        const tFill = isCursor ? cursorText : rowText;
        if (tFill) t.style.fill = tFill;
        t.style.pointerEvents = 'none';
    }
}

function renderPlaylist(svg) {
    renderList(svg, 'playlist', 'Playlist', 'press - play   swipe<>/long - exit',
               ['1. Radio Nowy Świat', '2. Radio 357', '3. Trójka', '4. Chillout FM',
                '5. Jazz Radio', '6. Classic FM', '7. Rock Antenne', '8. Nocturne']);
}

function renderBrowser(svg) {
    renderList(svg, 'browser', 'SD: music', 'press - open   swipe<>/long - back',
               ['..', 'Albums', 'Podcasts', 'Soundtracks',
                '01 - Intro.mp3', '02 - Nightdrive.mp3', '03 - Coda.flac', '04 - Outro.mp3']);
}

function renderEq(svg) {
    const e = state.eq;
    const W = state.meta.screen_w, H = state.meta.screen_h;
    const hasKnob = !!(e.eq_knob_image && e.eq_knob_image.trim());

    // Active-band value — a movable top-left-anchored label (like other screens).
    drawLabel(svg, e.eq_info_x | 0, e.eq_info_y | 0, e.eq_info_font, '125 Hz: +3 dB',
              'value', { x: 'eq_info_x', y: 'eq_info_y' }, false);

    // Response curve — a movable + resizable box with a representative spline.
    const cvW = e.eq_curve_w | 0, cvH = e.eq_curve_h | 0;
    if (cvW > 0 && cvH > 0) {
        const cvX = e.eq_curve_x | 0, cvY = e.eq_curve_y | 0;
        drawFreeElement(svg, {
            x: cvX, y: cvY, w: cvW, h: cvH,
            label: 'curve', cls: 'label-rect',
            fields: { x: 'eq_curve_x', y: 'eq_curve_y', w: 'eq_curve_w', h: 'eq_curve_h' },
        });
        const pts = EQ_PREVIEW_LEVELS.map((lvl, i) =>
            `${cvX + i / (EQ_PREVIEW_LEVELS.length - 1) * cvW},${cvY + (1 - lvl) * cvH}`).join(' ');
        const poly = document.createElementNS(SVG_NS, 'polyline');
        poly.setAttribute('points', pts);
        poly.setAttribute('fill', 'none');
        poly.style.stroke = 'var(--accent)';
        poly.style.strokeWidth = '2px';
        poly.style.pointerEvents = 'none';   // don't block the box's drag handles
        svg.appendChild(poly);
    }

    // Sliders group — a movable + resizable box; the bands schematic is drawn
    // inside (decorations are pointer-transparent so the box stays draggable),
    // frequency labels below unless hidden. Matches the device group model.
    const g = eqGeom(e);
    const { n, gw, gh, sliderW, sliderH, freqShown, labelFh, freqArea } = g;
    if (gw > 0 && gh > 0) {
        const gx = e.eq_group_x | 0, gy = e.eq_group_y | 0;
        drawFreeElement(svg, {
            x: gx, y: gy, w: gw, h: gh,
            label: 'sliders', cls: 'label-rect',
            // Corners drag the span directly; height is the slider height plus the
            // frequency strip, so it needs translating.
            fields: {
                x: 'eq_group_x', y: 'eq_group_y',
                w: 'eq_group_w', h: 'eq_slider_h',
                hToValue: h => Math.max(4, h - freqArea),
                hFromValue: sh => sh + freqArea,
            },
        });
        const inert = el => { el.style.pointerEvents = 'none'; return el; };
        const knobOnly  = !!e.eq_knob_only;
        const trackW    = Math.max(2, sliderW);
        // Artwork keeps its own width (it may overhang the band); without artwork
        // the plain LVGL knob is a touch wider than the track.
        const knobW     = hasKnob ? Math.max(2, g.knobW) : trackW + 4;
        const knobH     = Math.max(4, Math.round(sliderH * 0.1));

        for (let i = 0; i < n; i++) {
            const cx    = gx + g.bandX(i) + sliderW / 2;
            const level = EQ_PREVIEW_LEVELS[i];              // 0 (bottom) .. 1 (top)
            const knobY = gy + (sliderH - knobH) * (1 - level);

            if (!knobOnly) {
                inert(rect(svg, {
                    x: cx - trackW / 2, y: gy, width: trackW, height: sliderH,
                    rx: trackW / 2, fill: '#8a97a3', 'fill-opacity': 0.35,
                }));
                const fill = inert(rect(svg, {
                    x: cx - trackW / 2, y: knobY + knobH / 2,
                    width: trackW, height: gy + sliderH - (knobY + knobH / 2),
                    rx: trackW / 2, 'fill-opacity': 0.75,
                }));
                fill.style.fill = 'var(--accent)';
            }
            const knob = inert(rect(svg, {
                x: cx - knobW / 2, y: knobY, width: knobW, height: knobH,
                rx: hasKnob ? 3 : Math.min(knobW, knobH) / 2, class: 'label-rect',
            }));
            if (hasKnob) {
                knob.style.stroke = 'var(--accent)';
                knob.style.strokeWidth = '1.5px';
            }
            if (freqShown) {
                inert(text(svg, cx, gy + sliderH + labelFh, EQ_FREQ_LABELS[i], {
                    'font-size': labelFh, 'text-anchor': 'middle', opacity: 0.6,
                }));
            }
        }
    }

    // Legend/hint — movable, offset-anchored (X from centre, Y from bottom).
    if (!e.eq_hint_hide) {
        const fh = Math.max(9, Math.round(H / 20));
        const txt = 'swipe = back';
        const tw = Math.round(fh * 0.55) * txt.length;
        drawFreeElement(svg, {
            x: Math.round(W / 2 - tw / 2 + (e.eq_hint_x | 0)),
            y: Math.round(H + (e.eq_hint_y | 0) - fh),
            w: tw, h: fh, label: 'legend', cls: 'label-rect',
            fields: { x: 'eq_hint_x', y: 'eq_hint_y' },
            text: txt, textSize: fh,
        });
    }
}

function drawVolSlider(svg, prefix, data) {
    if (!data[`${prefix}_volslider_show`]) return;
    let w = data[`${prefix}_volslider_w`] | 0, h = data[`${prefix}_volslider_h`] | 0;
    let wField = `${prefix}_volslider_w`, hField = `${prefix}_volslider_h`;
    // Firmware swaps the box when it contradicts the chosen orientation —
    // mirror that here (fields swap too, so resize keeps editing the right one).
    if (!!data[`${prefix}_volslider_vertical`] !== (h > w)) {
        [w, h] = [h, w];
        [wField, hField] = [hField, wField];
    }
    drawFreeElement(svg, {
        x: data[`${prefix}_volslider_x`] | 0, y: data[`${prefix}_volslider_y`] | 0,
        w, h,
        label: data[`${prefix}_volslider_knob_only`] ? 'vol knob' : 'vol slider',
        cls: 'label-rect',
        fields: { x: `${prefix}_volslider_x`, y: `${prefix}_volslider_y`,
                  w: wField, h: hField },
    });
}

function drawTouchHotspots(svg, prefix, data) {
    for (let i = 1; i <= HOTSPOT_COUNT; i++) {
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

function drawLabel(svg, x, y, fontId, text_str, name, fields, anchorCenter, fill) {
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
        font: fontId, hugsText: true,
        text: text_str, textSize: fh,
        textFill: fill || null,
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

// opts.font — the label's font id. Given one, the box is sized and the sample
// text baselined from real LVGL metrics instead of the nominal font size, which
// is what makes the preview line up with the panel.
// opts.hugsText — the box width is a character-count estimate of the text (as
// opposed to a configurable fixed-width box), so the plate's horizontal padding
// widens it on the device too.
function drawFreeElement(svg, opts) {
    // ui_label_scrim() pads a plated label by 1px vertically and 6px each side,
    // and that padding counts into the object — so it is part of the rectangle
    // the user lines up against the wallpaper.
    const plateOpa = opts.text
        ? clamp(state[state.active][sectionLabelBgKey()] ?? 50, 0, 100) : 0;
    const padV = plateOpa > 0 ? 1 : 0;
    const padH = plateOpa > 0 && opts.hugsText ? 6 : 0;
    const x = opts.x - padH;
    const y = opts.y;
    const w = opts.w + padH * 2;
    const h = opts.font ? fontLineHeight(opts.font) + padV * 2 : opts.h;

    const r = rect(svg, {
        x, y, width: w, height: h,
        class: `${opts.cls} ${placeholderClass(opts.label)}`,
    });
    if (opts.fillOpacity !== undefined) {
        r.style.fillOpacity = opts.fillOpacity;
    } else if (opts.text) {
        // Floating labels use the active screen's configurable background plate.
        // Keep the editor's placeholder colour, but scale it so the opacity
        // slider has an immediate and truthful effect in Live preview.
        r.style.fillOpacity = plateOpa / 100;
    }
    if (opts.radius !== undefined) {
        r.setAttribute('rx', opts.radius);
        r.setAttribute('ry', opts.radius);
    }
    setupMove(r, svg, opts.fields);

    tag(svg, x + 2, y + 7, opts.label);

    if (opts.text) {
        // Clip the sample text to the box — the firmware clips widget content
        // the same way, so overflow would misrepresent the on-device look.
        const cid = 'el_clip_' + (clipSeq++);
        const cp = document.createElementNS(SVG_NS, 'clipPath');
        cp.setAttribute('id', cid);
        const cr = document.createElementNS(SVG_NS, 'rect');
        cr.setAttribute('x', x);
        cr.setAttribute('y', y);
        cr.setAttribute('width', w);
        cr.setAttribute('height', h);
        cp.appendChild(cr);
        svg.appendChild(cp);
        const tattr = {
            'font-size': Math.min(opts.textSize, h),
            'text-anchor': 'middle',
            'clip-path': 'url(#' + cid + ')',
        };
        if (opts.textFill) tattr.fill = opts.textFill;
        const base = opts.font ? padV + fontBaseline(opts.font) : h * 0.78;
        text(svg, x + w / 2, y + base, opts.text, tattr);
    }

    // Corner resize whenever a width field exists; without a height field the
    // corners resize width only (height follows the font).
    if (opts.fields.w) {
        addCornerHandles(svg, x, y, w, h, opts.fields);
    }
}

// Weather is special: its box (W) is a full-width centering FRAME that draws no
// background on the device — the actual plate hugs the centered icon+text. So
// draw the span as a dashed guide (movable, W-editable in the form) and a
// filled pill sized to the rendered content, centered within the span.
function drawWeatherElement(svg, opts) {
    // The row is as tall as the widget's font, but never below the icon font.
    const h = Math.max(opts.font ? fontLineHeight(opts.font) : opts.h, 20);
    rect(svg, {
        x: opts.x, y: opts.y, width: opts.w, height: h,
        class: `label-frame ${placeholderClass('weather')}`,
    });
    // The span's visible stroke is too thin to grab and its interior stays
    // click-through, so drags on the border go through a fat invisible edge.
    const grip = rect(svg, {
        x: opts.x, y: opts.y, width: opts.w, height: h,
        class: 'label-frame-grip',
    });
    setupMove(grip, svg, opts.fields, false);
    tag(svg, opts.x + 2, opts.y + 7, opts.label);

    const cx = opts.x + opts.w / 2;
    const base = opts.font ? fontBaseline(opts.font) : h * 0.78;
    const t = text(svg, cx, opts.y + base, opts.text, {
        'font-size': Math.min(opts.textSize, h),
        'text-anchor': 'middle',
    });
    // Size the pill to the rendered text (+ horizontal padding), centered.
    const pw = t.getBBox().width + 12;
    const pill = rect(svg, {
        x: cx - pw / 2, y: opts.y, width: pw, height: h,
        class: `label-rect ${placeholderClass('weather')}`,
    });
    // The pill is the visible plate, so it is also the primary drag handle and
    // the element that carries the keyboard-selection outline.
    setupMove(pill, svg, opts.fields);
    svg.insertBefore(pill, t);           // paint the pill behind the text

    // Corners resize the span's width only — weather has no height field, the
    // font drives its height. Drawn last so they sit above pill and grip.
    addCornerHandles(svg, opts.x, opts.y, opts.w, h, opts.fields);
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
        setupResize(handle, svg, fields, cr.dir, w, h);
    }
}

function setupMove(el, svg, fields, selectable = true) {
    if (selectable) showKeyboardSelection(el, fields);
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

// baseW/baseH are the size the element was actually drawn with. They matter for
// fields where 0 means "auto" (weather's centering span → full screen width):
// without them the first drag would grow from 0 instead of from what is on
// screen. baseH is also the starting height for elements whose height field is not
// the drawn height: fields.hToValue/hFromValue translate between the two (EQ group
// → slider height, with the frequency label strip on top).
function setupResize(el, svg, fields, dir, baseW, baseH) {
    el.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        e.stopPropagation();
        selectForKeyboard(el, fields);
        el.setPointerCapture(e.pointerId);

        const data = state[state.active];
        const start = {
            mx: e.clientX, my: e.clientY,
            x: data[fields.x] | 0, y: data[fields.y] | 0,
            w: (data[fields.w] | 0) || (baseW | 0),
            h: fields.hToValue ? (baseH | 0) : (data[fields.h] | 0),
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

            const hVal = fields.hToValue ? fields.hToValue(nh) : nh;
            // A translated field can be clamped (slider height bottoms out at 4),
            // so re-derive the drawn height and pin the edge the user is NOT
            // dragging — otherwise the box would creep upwards.
            if (fields.hFromValue && dir.includes('t'))
                ny = start.y + start.h - fields.hFromValue(hVal);
            data[fields.x] = nx; data[fields.y] = ny;
            data[fields.w] = nw;
            if (fields.h && fields.h !== fields.w) data[fields.h] = hVal;
            setFormValue(fields.x, nx); setFormValue(fields.y, ny);
            setFormValue(fields.w, nw);
            if (fields.h && fields.h !== fields.w) setFormValue(fields.h, hVal);
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

// Nominal glyph size, parsed from the font id. Good enough for the SVG
// font-size and the character-count width estimates, but NOT for vertical
// placement — use the metrics below for that.
function fontHeight(id) {
    if (!id) return 14;
    const m = id.match(/_(\d+)(_pl)?$/);
    return m ? parseInt(m[1], 10) : 14;
}

// The device serves the real LVGL metrics of every registered font
// (/api/ui/profile/meta → font_metrics). They decide where a label lands: its
// box is line_height tall — not the nominal size, which runs ~10% short for the
// text fonts and is wildly off for the digit-only ones — and its baseline sits
// base_line above the box bottom. The fallbacks are the ratios those metrics
// average out to, for firmware that predates the field.
function fontMetrics(id) {
    return (state.meta.font_metrics || {})[id] || null;
}

function fontLineHeight(id) {
    const m = fontMetrics(id);
    return m ? m.h : Math.round(fontHeight(id) * 1.09);
}

// Baseline offset, measured from the top of the box.
function fontBaseline(id) {
    const m = fontMetrics(id);
    return m ? m.h - m.b : Math.round(fontLineHeight(id) * 0.82);
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
        rate: 'ph-info', ch: 'ph-info', kbps: 'ph-info', vol: 'ph-info',
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
