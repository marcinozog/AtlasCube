#include "settings.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "audio_player.h"
#include "app_state.h"
#include "bt.h"
#include "theme.h"
#include "display.h"
#include "ui_manager.h"
#include "screensavers.h"
#include "defines.h"

static esp_err_t load_from_file(void);
static esp_err_t save_to_file(void);

static app_settings_t s_settings;


/*
esp_err_t settings_init(void)
*/
esp_err_t settings_init(void)
{
    theme_load_from_file();   // separate file /spiffs/theme.json (color palettes)

    if (load_from_file() != ESP_OK) {
        s_settings.audio.volume             = 15;
        for (int i = 0; i < 10; i++) s_settings.audio.eq[i] = 0;
        s_settings.audio.eq_enabled         = true;
        s_settings.playlist.curr_index      = 0;
        s_settings.playlist.resume_on_boot  = false;
        s_settings.playlist.was_playing     = false;
        s_settings.display.brightness       = 80;
        s_settings.display.screen           = SCREEN_CLOCK;
        s_settings.display.theme            = THEME_DARK;
        s_settings.display.bg_gradient      = true;
        s_settings.display.dim_schedule.enabled        = false;
        s_settings.display.dim_schedule.dim_hour       = 22;
        s_settings.display.dim_schedule.dim_minute     = 0;
        s_settings.display.dim_schedule.dim_brightness = 20;
        s_settings.display.dim_schedule.bright_hour    = 7;
        s_settings.display.dim_schedule.bright_minute  = 0;
        s_settings.display.dim_schedule.radio_off      = false;
        s_settings.display.dim_schedule.radio_on       = false;
        s_settings.display.dim_schedule.radio_station  = 0;
        s_settings.display.dim_schedule.radio_volume   = 25;
        s_settings.bluetooth.enable         = false;
        s_settings.bluetooth.show_screen    = true;
        s_settings.bluetooth.volume         = 15;
        s_settings.bluetooth.auto_switch    = true;
        strncpy(s_settings.ntp.server1, "pool.ntp.org",                sizeof(s_settings.ntp.server1) - 1);
        strncpy(s_settings.ntp.server2, "time.cloudflare.com",         sizeof(s_settings.ntp.server2) - 1);
        strncpy(s_settings.ntp.tz,      "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(s_settings.ntp.tz)      - 1);
        // WiFi — empty = AP mode on first boot
        s_settings.wifi.ssid[0]             = '\0';
        s_settings.wifi.password[0]         = '\0';
        // Device — empty hostname = auto "atlascube-xxxx" derived at mDNS start
        s_settings.device.hostname[0]       = '\0';
        // Screensaver
        s_settings.scrsaver.delay           = 60;
        s_settings.scrsaver.screensaver_id  = SCREENSAVER_CLOCKHANDS;
        // Photo-frame screensaver
        s_settings.scrsaver.photo_dir[0]    = '\0';
        strncpy(s_settings.scrsaver.photo_dir, "/sdcard/slides", sizeof(s_settings.scrsaver.photo_dir) - 1);
        s_settings.scrsaver.photo_order     = 1;   // random
        s_settings.scrsaver.photo_hold_s    = 8;
        s_settings.scrsaver.photo_effect    = 4;   // random-per-slide
        s_settings.scrsaver.photo_speed     = 3;
        // Dashboard screensaver — defaults to NBP USD/PLN exchange rate
        strncpy(s_settings.dashboard.title,     "USD/PLN",                                                       sizeof(s_settings.dashboard.title)     - 1);
        strncpy(s_settings.dashboard.url,       "https://api.nbp.pl/api/exchangerates/rates/A/USD?format=json", sizeof(s_settings.dashboard.url)       - 1);
        strncpy(s_settings.dashboard.json_path, "rates[0].mid",                                                  sizeof(s_settings.dashboard.json_path) - 1);
        strncpy(s_settings.dashboard.suffix,    " PLN",                                                          sizeof(s_settings.dashboard.suffix)    - 1);
        s_settings.dashboard.poll_interval_ms = 60000;
        // Dashboard notifications — off by default
        s_settings.dashboard.notify_enabled     = false;
        s_settings.dashboard.value_type         = DASHBOARD_VALUE_NUMBER;
        s_settings.dashboard.notify_num_low_en  = false;
        s_settings.dashboard.notify_num_low     = 0.0;
        s_settings.dashboard.notify_num_high_en = false;
        s_settings.dashboard.notify_num_high    = 0.0;
        s_settings.dashboard.notify_str_eq_en   = false;
        s_settings.dashboard.notify_str_eq[0]   = '\0';
        s_settings.dashboard.notify_str_ne_en   = false;
        s_settings.dashboard.notify_str_ne[0]   = '\0';

        save_to_file();
    }

    return ESP_OK;
}


/*
static esp_err_t load_from_file(void)
*/
static esp_err_t load_from_file(void)
{
    FILE *f = fopen(SETTINGS_FILE, "r");
    if (!f) return ESP_FAIL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(buffer, 1, size, f);
    buffer[size] = 0;
    fclose(f);

    cJSON *json = cJSON_Parse(buffer);
    free(buffer);

    if (!json) return ESP_FAIL;

    // ===== AUDIO =====
    cJSON *audio = cJSON_GetObjectItem(json, "audio");
    if (cJSON_IsObject(audio)) {

        cJSON *vol = cJSON_GetObjectItem(audio, "volume");
        if (cJSON_IsNumber(vol)) {
            s_settings.audio.volume = vol->valueint;
        }

        cJSON *eq = cJSON_GetObjectItem(audio, "eq");
        if (cJSON_IsArray(eq)) {
            int sz = cJSON_GetArraySize(eq);
            for (int i = 0; i < 10; i++) {
                if (i < sz) {
                    cJSON *item = cJSON_GetArrayItem(eq, i);
                    if (cJSON_IsNumber(item)) {
                        s_settings.audio.eq[i] = item->valueint;
                    }
                }
            }
        } else {
            ESP_LOGW("SETTINGS", "EQ missing → using defaults");
            for (int i = 0; i < 10; i++) s_settings.audio.eq[i] = 0;
        }

        cJSON *eq_en = cJSON_GetObjectItem(audio, "eq_enabled");
        if (cJSON_IsBool(eq_en)) {
            s_settings.audio.eq_enabled = cJSON_IsTrue(eq_en);
        } else {
            // missing in older file → default on
            s_settings.audio.eq_enabled = true;
        }
    }

    // ===== PLAYLIST =====
    // defaults (used if section is missing or partial)
    s_settings.playlist.resume_on_boot = false;
    s_settings.playlist.was_playing    = false;
    cJSON *playlist = cJSON_GetObjectItem(json, "playlist");
    if (cJSON_IsObject(playlist)) {
        cJSON *curr_index = cJSON_GetObjectItem(playlist, "curr_index");
        if (cJSON_IsNumber(curr_index)) {
            s_settings.playlist.curr_index = curr_index->valueint;
        }
        cJSON *rob = cJSON_GetObjectItem(playlist, "resume_on_boot");
        if (cJSON_IsBool(rob)) s_settings.playlist.resume_on_boot = cJSON_IsTrue(rob);
        cJSON *wp = cJSON_GetObjectItem(playlist, "was_playing");
        if (cJSON_IsBool(wp))  s_settings.playlist.was_playing = cJSON_IsTrue(wp);
    }

    // ===== DISPLAY =====
    cJSON *display = cJSON_GetObjectItem(json, "display");
    if (cJSON_IsObject(display)) {
        cJSON *scr = cJSON_GetObjectItem(display, "screen");

        if (cJSON_IsNumber(scr)) {
            s_settings.display.screen = scr->valueint;
        }
        cJSON *br = cJSON_GetObjectItem(display, "brightness");
        if (cJSON_IsNumber(br)) {
            s_settings.display.brightness = br->valueint;
        }
        // << new
        cJSON *th = cJSON_GetObjectItem(display, "theme");
        if (cJSON_IsString(th)) {
            s_settings.display.theme =
                (strcmp(th->valuestring, "light") == 0) ? THEME_LIGHT : THEME_DARK;
        }
        cJSON *bg = cJSON_GetObjectItem(display, "bg_gradient");
        s_settings.display.bg_gradient = cJSON_IsBool(bg) ? cJSON_IsTrue(bg) : true;

        // dim schedule defaults (used if section is missing or partial)
        s_settings.display.dim_schedule.enabled        = false;
        s_settings.display.dim_schedule.dim_hour       = 22;
        s_settings.display.dim_schedule.dim_minute     = 0;
        s_settings.display.dim_schedule.dim_brightness = 20;
        s_settings.display.dim_schedule.bright_hour    = 7;
        s_settings.display.dim_schedule.bright_minute  = 0;
        s_settings.display.dim_schedule.radio_off      = false;
        s_settings.display.dim_schedule.radio_on       = false;
        s_settings.display.dim_schedule.radio_station  = 0;
        s_settings.display.dim_schedule.radio_volume   = 25;

        cJSON *dim = cJSON_GetObjectItem(display, "dim_schedule");
        if (cJSON_IsObject(dim)) {
            cJSON *j;
            j = cJSON_GetObjectItem(dim, "enabled");        if (cJSON_IsBool(j))   s_settings.display.dim_schedule.enabled        = cJSON_IsTrue(j);
            j = cJSON_GetObjectItem(dim, "dim_hour");       if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.dim_hour       = j->valueint;
            j = cJSON_GetObjectItem(dim, "dim_minute");     if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.dim_minute     = j->valueint;
            j = cJSON_GetObjectItem(dim, "dim_brightness"); if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.dim_brightness = j->valueint;
            j = cJSON_GetObjectItem(dim, "bright_hour");    if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.bright_hour    = j->valueint;
            j = cJSON_GetObjectItem(dim, "bright_minute");  if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.bright_minute  = j->valueint;
            j = cJSON_GetObjectItem(dim, "radio_off");      if (cJSON_IsBool(j))   s_settings.display.dim_schedule.radio_off      = cJSON_IsTrue(j);
            j = cJSON_GetObjectItem(dim, "radio_on");       if (cJSON_IsBool(j))   s_settings.display.dim_schedule.radio_on       = cJSON_IsTrue(j);
            j = cJSON_GetObjectItem(dim, "radio_station");  if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.radio_station  = j->valueint;
            j = cJSON_GetObjectItem(dim, "radio_volume");   if (cJSON_IsNumber(j)) s_settings.display.dim_schedule.radio_volume   = j->valueint;
        }
    }
    

    // ===== BLUETOOTH =====
    cJSON *bluetooth = cJSON_GetObjectItem(json, "bluetooth");
    if (cJSON_IsObject(bluetooth)) {
        cJSON *bt = cJSON_GetObjectItem(bluetooth, "enable");
        if (cJSON_IsBool(bt)) {
            s_settings.bluetooth.enable = cJSON_IsTrue(bt);
        }
        cJSON *bt_screen = cJSON_GetObjectItem(bluetooth, "show_screen");
        if (cJSON_IsBool(bt_screen)) {
            s_settings.bluetooth.show_screen = cJSON_IsTrue(bt_screen);
        }
        cJSON *bt_vol = cJSON_GetObjectItem(bluetooth, "volume");
        if (cJSON_IsNumber(bt_vol)) {
            s_settings.bluetooth.volume = bt_vol->valueint;
        }
        cJSON *bt_auto = cJSON_GetObjectItem(bluetooth, "auto_switch");
        if (cJSON_IsBool(bt_auto)) {
            s_settings.bluetooth.auto_switch = cJSON_IsTrue(bt_auto);
        }
    }

    // ===== NTP =====
    cJSON *ntp = cJSON_GetObjectItem(json, "ntp");
    if (cJSON_IsObject(ntp)) {
        cJSON *srv1 = cJSON_GetObjectItem(ntp, "server1");
        cJSON *srv2 = cJSON_GetObjectItem(ntp, "server2");
        cJSON *tz   = cJSON_GetObjectItem(ntp, "tz");

        if (cJSON_IsString(srv1)) strncpy(s_settings.ntp.server1, srv1->valuestring, sizeof(s_settings.ntp.server1) - 1);
        if (cJSON_IsString(srv2)) strncpy(s_settings.ntp.server2, srv2->valuestring, sizeof(s_settings.ntp.server2) - 1);
        if (cJSON_IsString(tz))   strncpy(s_settings.ntp.tz,      tz->valuestring,   sizeof(s_settings.ntp.tz)      - 1);
    } else {
        // section missing in older file — insert defaults
        strncpy(s_settings.ntp.server1, "pool.ntp.org",                sizeof(s_settings.ntp.server1) - 1);
        strncpy(s_settings.ntp.server2, "time.cloudflare.com",         sizeof(s_settings.ntp.server2) - 1);
        strncpy(s_settings.ntp.tz,      "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(s_settings.ntp.tz)      - 1);
    }

    // ── SCREENSAVER ───────────────────────────────────────────────────────────
    // Photo-frame defaults — overridden below if a "photo" object is present.
    s_settings.scrsaver.photo_dir[0] = '\0';
    strncpy(s_settings.scrsaver.photo_dir, "/sdcard/slides", sizeof(s_settings.scrsaver.photo_dir) - 1);
    s_settings.scrsaver.photo_order  = 1;
    s_settings.scrsaver.photo_hold_s = 8;
    s_settings.scrsaver.photo_effect = 4;
    s_settings.scrsaver.photo_speed  = 3;

    cJSON *scrs = cJSON_GetObjectItem(json, "scrsaver");
    if (cJSON_IsObject(scrs)) {
        cJSON *dl = cJSON_GetObjectItem(scrs, "delay");
        cJSON *id = cJSON_GetObjectItem(scrs, "id");
        s_settings.scrsaver.delay = cJSON_IsNumber(dl) ? dl->valueint : 60;
        if (cJSON_IsString(id)) {
            s_settings.scrsaver.screensaver_id = screensaver_from_name(id->valuestring);
        } else if (cJSON_IsNumber(id) && screensaver_is_valid(id->valueint)) {
            s_settings.scrsaver.screensaver_id = id->valueint;
        } else {
            s_settings.scrsaver.screensaver_id = SCREENSAVER_CLOCKHANDS;
        }
        cJSON *ph = cJSON_GetObjectItem(scrs, "photo");
        if (cJSON_IsObject(ph)) {
            cJSON *pd  = cJSON_GetObjectItem(ph, "dir");
            cJSON *po  = cJSON_GetObjectItem(ph, "order");
            cJSON *phs = cJSON_GetObjectItem(ph, "hold_s");
            cJSON *pe  = cJSON_GetObjectItem(ph, "effect");
            cJSON *psp = cJSON_GetObjectItem(ph, "speed");
            if (cJSON_IsString(pd) && pd->valuestring[0]) {
                s_settings.scrsaver.photo_dir[0] = '\0';
                strncpy(s_settings.scrsaver.photo_dir, pd->valuestring, sizeof(s_settings.scrsaver.photo_dir) - 1);
            }
            if (cJSON_IsNumber(po))  s_settings.scrsaver.photo_order  = po->valueint ? 1 : 0;
            if (cJSON_IsNumber(phs)) s_settings.scrsaver.photo_hold_s = phs->valueint < 1 ? 1 : phs->valueint;
            if (cJSON_IsNumber(pe))  s_settings.scrsaver.photo_effect = (pe->valueint < 0 || pe->valueint > 4) ? 4 : pe->valueint;
            if (cJSON_IsNumber(psp)) s_settings.scrsaver.photo_speed  = (psp->valueint < 1) ? 1 : (psp->valueint > 5 ? 5 : psp->valueint);
        }
    } else {
        s_settings.scrsaver.delay          = 60;
        s_settings.scrsaver.screensaver_id = SCREENSAVER_CLOCKHANDS;
    }

    // ── DASHBOARD ─────────────────────────────────────────────────────────────
    cJSON *dash = cJSON_GetObjectItem(json, "dashboard");
    // start from defaults — missing fields keep their default value
    strncpy(s_settings.dashboard.title,     "USD/PLN",                                                       sizeof(s_settings.dashboard.title)     - 1);
    strncpy(s_settings.dashboard.url,       "https://api.nbp.pl/api/exchangerates/rates/A/USD?format=json", sizeof(s_settings.dashboard.url)       - 1);
    strncpy(s_settings.dashboard.json_path, "rates[0].mid",                                                  sizeof(s_settings.dashboard.json_path) - 1);
    strncpy(s_settings.dashboard.suffix,    " PLN",                                                          sizeof(s_settings.dashboard.suffix)    - 1);
    s_settings.dashboard.poll_interval_ms   = 60000;
    s_settings.dashboard.notify_enabled     = false;
    s_settings.dashboard.value_type         = DASHBOARD_VALUE_NUMBER;
    s_settings.dashboard.notify_num_low_en  = false;
    s_settings.dashboard.notify_num_low     = 0.0;
    s_settings.dashboard.notify_num_high_en = false;
    s_settings.dashboard.notify_num_high    = 0.0;
    s_settings.dashboard.notify_str_eq_en   = false;
    s_settings.dashboard.notify_str_eq[0]   = '\0';
    s_settings.dashboard.notify_str_ne_en   = false;
    s_settings.dashboard.notify_str_ne[0]   = '\0';
    if (cJSON_IsObject(dash)) {
        cJSON *t  = cJSON_GetObjectItem(dash, "title");
        cJSON *u  = cJSON_GetObjectItem(dash, "url");
        cJSON *jp = cJSON_GetObjectItem(dash, "json_path");
        cJSON *sf = cJSON_GetObjectItem(dash, "suffix");
        cJSON *pi = cJSON_GetObjectItem(dash, "poll_interval_ms");
        if (cJSON_IsString(t))  { s_settings.dashboard.title[0]     = '\0'; strncpy(s_settings.dashboard.title,     t->valuestring,  sizeof(s_settings.dashboard.title)     - 1); }
        if (cJSON_IsString(u))  { s_settings.dashboard.url[0]       = '\0'; strncpy(s_settings.dashboard.url,       u->valuestring,  sizeof(s_settings.dashboard.url)       - 1); }
        if (cJSON_IsString(jp)) { s_settings.dashboard.json_path[0] = '\0'; strncpy(s_settings.dashboard.json_path, jp->valuestring, sizeof(s_settings.dashboard.json_path) - 1); }
        if (cJSON_IsString(sf)) { s_settings.dashboard.suffix[0]    = '\0'; strncpy(s_settings.dashboard.suffix,    sf->valuestring, sizeof(s_settings.dashboard.suffix)    - 1); }
        if (cJSON_IsNumber(pi)) {
            int ms = pi->valueint;
            if (ms < 5000) ms = 5000;
            s_settings.dashboard.poll_interval_ms = ms;
        }

        cJSON *nx = cJSON_GetObjectItem(dash, "notify");
        if (cJSON_IsObject(nx)) {
            cJSON *en  = cJSON_GetObjectItem(nx, "enabled");
            cJSON *vt  = cJSON_GetObjectItem(nx, "value_type");
            cJSON *nle = cJSON_GetObjectItem(nx, "num_low_en");
            cJSON *nl  = cJSON_GetObjectItem(nx, "num_low");
            cJSON *nhe = cJSON_GetObjectItem(nx, "num_high_en");
            cJSON *nh  = cJSON_GetObjectItem(nx, "num_high");
            cJSON *see = cJSON_GetObjectItem(nx, "str_eq_en");
            cJSON *se  = cJSON_GetObjectItem(nx, "str_eq");
            cJSON *sne = cJSON_GetObjectItem(nx, "str_ne_en");
            cJSON *sn  = cJSON_GetObjectItem(nx, "str_ne");
            if (cJSON_IsBool(en))   s_settings.dashboard.notify_enabled     = cJSON_IsTrue(en);
            if (cJSON_IsString(vt)) s_settings.dashboard.value_type         = strcmp(vt->valuestring, "string") == 0 ? DASHBOARD_VALUE_STRING : DASHBOARD_VALUE_NUMBER;
            if (cJSON_IsBool(nle))  s_settings.dashboard.notify_num_low_en  = cJSON_IsTrue(nle);
            if (cJSON_IsNumber(nl)) s_settings.dashboard.notify_num_low     = nl->valuedouble;
            if (cJSON_IsBool(nhe))  s_settings.dashboard.notify_num_high_en = cJSON_IsTrue(nhe);
            if (cJSON_IsNumber(nh)) s_settings.dashboard.notify_num_high    = nh->valuedouble;
            if (cJSON_IsBool(see))  s_settings.dashboard.notify_str_eq_en   = cJSON_IsTrue(see);
            if (cJSON_IsString(se)) { s_settings.dashboard.notify_str_eq[0] = '\0'; strncpy(s_settings.dashboard.notify_str_eq, se->valuestring, sizeof(s_settings.dashboard.notify_str_eq) - 1); }
            if (cJSON_IsBool(sne))  s_settings.dashboard.notify_str_ne_en   = cJSON_IsTrue(sne);
            if (cJSON_IsString(sn)) { s_settings.dashboard.notify_str_ne[0] = '\0'; strncpy(s_settings.dashboard.notify_str_ne, sn->valuestring, sizeof(s_settings.dashboard.notify_str_ne) - 1); }
        }
    }

    // ── WIFI ──────────────────────────────────────────────────────────────────
    cJSON *wifi_obj = cJSON_GetObjectItem(json, "wifi");
    if (cJSON_IsObject(wifi_obj)) {
        cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi_obj, "password");
        if (cJSON_IsString(ssid)) strncpy(s_settings.wifi.ssid,     ssid->valuestring, sizeof(s_settings.wifi.ssid)     - 1);
        if (cJSON_IsString(pass)) strncpy(s_settings.wifi.password, pass->valuestring, sizeof(s_settings.wifi.password) - 1);
    } else {
        // older file without wifi section → AP on next restart
        s_settings.wifi.ssid[0]     = '\0';
        s_settings.wifi.password[0] = '\0';
    }

    // ── DEVICE ────────────────────────────────────────────────────────────────
    s_settings.device.hostname[0] = '\0';   // default: auto from MAC at mDNS start
    cJSON *device_obj = cJSON_GetObjectItem(json, "device");
    if (cJSON_IsObject(device_obj)) {
        cJSON *hn = cJSON_GetObjectItem(device_obj, "hostname");
        if (cJSON_IsString(hn)) strncpy(s_settings.device.hostname, hn->valuestring, sizeof(s_settings.device.hostname) - 1);
    }

    // DEBUG
    ESP_LOGI("SETTINGS", "WiFi SSID: \"%s\"", s_settings.wifi.ssid);
    ESP_LOGI("SETTINGS", "Volume: %d  BT: %d  curr_index: %d",
             s_settings.audio.volume, s_settings.bluetooth.enable,
             s_settings.playlist.curr_index);
    ESP_LOGI("SETTINGS", "Volume: %d",              s_settings.audio.volume);
    ESP_LOGI("SETTINGS", "curr_index: %d",          s_settings.playlist.curr_index);
    ESP_LOGI("SETTINGS", "Brightness: %d",          s_settings.display.brightness);
    ESP_LOGI("SETTINGS", "Show BT screen: %s",      s_settings.bluetooth.show_screen == true ? "true" : "false");
    ESP_LOGI("SETTINGS", "NTP srv1: %s",            s_settings.ntp.server1);
    ESP_LOGI("SETTINGS", "NTP srv2: %s",            s_settings.ntp.server2);
    ESP_LOGI("SETTINGS", "NTP TZ:   %s",            s_settings.ntp.tz);

    cJSON_Delete(json);
    return ESP_OK;
}


/*
static esp_err_t save_to_file(void)
*/
static esp_err_t save_to_file(void)
{
    cJSON *json = cJSON_CreateObject();

    // audio
    cJSON *audio = cJSON_CreateObject();
    cJSON_AddNumberToObject(audio, "volume", s_settings.audio.volume);
    cJSON *eq = cJSON_CreateIntArray(s_settings.audio.eq, 10);
    cJSON_AddItemToObject(audio, "eq", eq);
    cJSON_AddBoolToObject(audio,   "eq_enabled", s_settings.audio.eq_enabled);
    cJSON_AddItemToObject(json, "audio", audio);

    // playlist
    cJSON *playlist = cJSON_CreateObject();
    cJSON_AddNumberToObject(playlist, "curr_index", s_settings.playlist.curr_index);
    cJSON_AddBoolToObject  (playlist, "resume_on_boot", s_settings.playlist.resume_on_boot);
    cJSON_AddBoolToObject  (playlist, "was_playing",    s_settings.playlist.was_playing);
    cJSON_AddItemToObject(json, "playlist", playlist);

    // display
    cJSON *display = cJSON_CreateObject();
    cJSON_AddNumberToObject(display, "screen", s_settings.display.screen);
    cJSON_AddNumberToObject(display, "brightness", s_settings.display.brightness);
    cJSON_AddStringToObject(display, "theme",
        s_settings.display.theme == THEME_LIGHT ? "light" : "dark");
    cJSON_AddBoolToObject(display, "bg_gradient", s_settings.display.bg_gradient);
    cJSON *dim = cJSON_CreateObject();
    cJSON_AddBoolToObject  (dim, "enabled",        s_settings.display.dim_schedule.enabled);
    cJSON_AddNumberToObject(dim, "dim_hour",       s_settings.display.dim_schedule.dim_hour);
    cJSON_AddNumberToObject(dim, "dim_minute",     s_settings.display.dim_schedule.dim_minute);
    cJSON_AddNumberToObject(dim, "dim_brightness", s_settings.display.dim_schedule.dim_brightness);
    cJSON_AddNumberToObject(dim, "bright_hour",    s_settings.display.dim_schedule.bright_hour);
    cJSON_AddNumberToObject(dim, "bright_minute",  s_settings.display.dim_schedule.bright_minute);
    cJSON_AddBoolToObject  (dim, "radio_off",      s_settings.display.dim_schedule.radio_off);
    cJSON_AddBoolToObject  (dim, "radio_on",       s_settings.display.dim_schedule.radio_on);
    cJSON_AddNumberToObject(dim, "radio_station",  s_settings.display.dim_schedule.radio_station);
    cJSON_AddNumberToObject(dim, "radio_volume",   s_settings.display.dim_schedule.radio_volume);
    cJSON_AddItemToObject(display, "dim_schedule", dim);
    cJSON_AddItemToObject(json, "display", display);

    // bluetooth
    cJSON *bluetooth = cJSON_CreateObject();
    cJSON_AddBoolToObject(bluetooth,   "enable", s_settings.bluetooth.enable);
    cJSON_AddBoolToObject(bluetooth,   "show_screen", s_settings.bluetooth.show_screen);
    cJSON_AddNumberToObject(bluetooth, "volume", s_settings.bluetooth.volume);
    cJSON_AddBoolToObject(bluetooth,   "auto_switch", s_settings.bluetooth.auto_switch);
    cJSON_AddItemToObject(json, "bluetooth", bluetooth);

    // ntp
    cJSON *ntp = cJSON_CreateObject();
    cJSON_AddStringToObject(ntp, "server1", s_settings.ntp.server1);
    cJSON_AddStringToObject(ntp, "server2", s_settings.ntp.server2);
    cJSON_AddStringToObject(ntp, "tz",      s_settings.ntp.tz);
    cJSON_AddItemToObject(json, "ntp", ntp);

    // wifi
    cJSON *wifi_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_obj, "ssid",     s_settings.wifi.ssid);
    cJSON_AddStringToObject(wifi_obj, "password", s_settings.wifi.password);
    cJSON_AddItemToObject(json, "wifi", wifi_obj);

    // device
    cJSON *device_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(device_obj, "hostname", s_settings.device.hostname);
    cJSON_AddItemToObject(json, "device", device_obj);

    // screensaver
    cJSON *scrs = cJSON_CreateObject();
    cJSON_AddNumberToObject(scrs, "delay",  s_settings.scrsaver.delay);
    cJSON_AddStringToObject(scrs, "id",
        screensaver_name(s_settings.scrsaver.screensaver_id));
    cJSON *photo = cJSON_CreateObject();
    cJSON_AddStringToObject(photo, "dir",    s_settings.scrsaver.photo_dir);
    cJSON_AddNumberToObject(photo, "order",  s_settings.scrsaver.photo_order);
    cJSON_AddNumberToObject(photo, "hold_s", s_settings.scrsaver.photo_hold_s);
    cJSON_AddNumberToObject(photo, "effect", s_settings.scrsaver.photo_effect);
    cJSON_AddNumberToObject(photo, "speed",  s_settings.scrsaver.photo_speed);
    cJSON_AddItemToObject(scrs, "photo", photo);
    cJSON_AddItemToObject(json, "scrsaver", scrs);

    // dashboard
    cJSON *dash = cJSON_CreateObject();
    cJSON_AddStringToObject(dash, "title",            s_settings.dashboard.title);
    cJSON_AddStringToObject(dash, "url",              s_settings.dashboard.url);
    cJSON_AddStringToObject(dash, "json_path",        s_settings.dashboard.json_path);
    cJSON_AddStringToObject(dash, "suffix",           s_settings.dashboard.suffix);
    cJSON_AddNumberToObject(dash, "poll_interval_ms", s_settings.dashboard.poll_interval_ms);
    cJSON *notify = cJSON_CreateObject();
    cJSON_AddBoolToObject  (notify, "enabled",     s_settings.dashboard.notify_enabled);
    cJSON_AddStringToObject(notify, "value_type",  s_settings.dashboard.value_type == DASHBOARD_VALUE_STRING ? "string" : "number");
    cJSON_AddBoolToObject  (notify, "num_low_en",  s_settings.dashboard.notify_num_low_en);
    cJSON_AddNumberToObject(notify, "num_low",     s_settings.dashboard.notify_num_low);
    cJSON_AddBoolToObject  (notify, "num_high_en", s_settings.dashboard.notify_num_high_en);
    cJSON_AddNumberToObject(notify, "num_high",    s_settings.dashboard.notify_num_high);
    cJSON_AddBoolToObject  (notify, "str_eq_en",   s_settings.dashboard.notify_str_eq_en);
    cJSON_AddStringToObject(notify, "str_eq",      s_settings.dashboard.notify_str_eq);
    cJSON_AddBoolToObject  (notify, "str_ne_en",   s_settings.dashboard.notify_str_ne_en);
    cJSON_AddStringToObject(notify, "str_ne",      s_settings.dashboard.notify_str_ne);
    cJSON_AddItemToObject  (dash, "notify", notify);
    cJSON_AddItemToObject(json, "dashboard", dash);

    char *str = cJSON_PrintUnformatted(json);

    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f) {
        cJSON_Delete(json);
        free(str);
        return ESP_FAIL;
    }

    fwrite(str, 1, strlen(str), f);
    fclose(f);

    cJSON_Delete(json);
    free(str);
    return ESP_OK;
}


/*
void settings_apply(void)
*/
void settings_apply(void)
{
    audio_player_set_volume(s_settings.audio.volume);
    audio_player_set_eq_10(s_settings.audio.eq);
    audio_player_set_eq_enabled(s_settings.audio.eq_enabled);
    // Set last state to BT at start
    bt_set_enabled(s_settings.bluetooth.enable);
    // Set last volume to BT at start
    bt_set_volume(s_settings.bluetooth.volume);
    // Refresh BT connection
    bt_check_connection();

    ui_navigate(s_settings.display.screen);
    display_set_backlight(s_settings.display.brightness);

    app_state_patch_t patch = {
        .has_volume             = true, .volume    = s_settings.audio.volume,
        .has_eq                 = true,
        .has_eq_enabled         = true, .eq_enabled = s_settings.audio.eq_enabled,
        .has_curr_index         = true, .curr_index= s_settings.playlist.curr_index,
        .has_bt_enable          = true, .bt_enable = s_settings.bluetooth.enable,
        .has_bt_show_screen     = true, .bt_show_screen = s_settings.bluetooth.show_screen,
        .has_bt_volume          = true, .bt_volume = s_settings.bluetooth.volume,
        .has_bt_auto_switch     = true, .bt_auto_switch = s_settings.bluetooth.auto_switch,
        .has_display_brightness = true, .display_brightness = s_settings.display.brightness,
        .has_theme              = true, .theme     = s_settings.display.theme,
        .has_bg_gradient        = true, .bg_gradient = s_settings.display.bg_gradient,
        .has_scrsaver_delay     = true, .scrsaver_delay  = s_settings.scrsaver.delay,
        .has_scrsaver_id        = true, .scrsaver_id     = s_settings.scrsaver.screensaver_id,
    };
    memcpy(patch.eq, s_settings.audio.eq, sizeof(patch.eq));
    app_state_update(&patch);
}

app_settings_t* settings_get(void)       { return &s_settings; }
esp_err_t       settings_save(void)       { return save_to_file(); }


void settings_set_volume(int volume)
{
    s_settings.audio.volume = volume;
    audio_player_set_volume(volume);
    app_state_update(&(app_state_patch_t){ .has_volume = true, .volume = volume });
    save_to_file();
}

void settings_set_eq_10(int *bands)
{
    memcpy(s_settings.audio.eq, bands, sizeof(int) * 10);
    audio_player_set_eq_10(s_settings.audio.eq);
    app_state_patch_t patch = { .has_eq = true };
    memcpy(patch.eq, s_settings.audio.eq, sizeof(patch.eq));
    app_state_update(&patch);
    save_to_file();
}

void settings_set_eq_enabled(bool enabled)
{
    if (s_settings.audio.eq_enabled == enabled) return;
    s_settings.audio.eq_enabled = enabled;
    audio_player_set_eq_enabled(enabled);
    app_state_update(&(app_state_patch_t){
        .has_eq_enabled = true, .eq_enabled = enabled
    });
    save_to_file();
}

void settings_set_curr_index(int index)
{
    s_settings.playlist.curr_index = index;
    app_state_update(&(app_state_patch_t){ .has_curr_index = true, .curr_index = index });
    save_to_file();
}

void settings_set_resume_on_boot(bool enabled)
{
    if (s_settings.playlist.resume_on_boot == enabled) return;
    s_settings.playlist.resume_on_boot = enabled;
    save_to_file();
}

void settings_set_was_playing(bool playing)
{
    if (s_settings.playlist.was_playing == playing) return;
    s_settings.playlist.was_playing = playing;
    save_to_file();
}

void settings_set_screen(ui_screen_id_t screen)
{
    s_settings.display.screen = screen;
    app_state_update(&(app_state_patch_t){ .has_screen = true, .screen = screen });
    ui_navigate(screen);
    save_to_file();
}

void settings_set_brightness(int brightness)
{
    s_settings.display.brightness = brightness;
    app_state_update(&(app_state_patch_t) {.has_display_brightness = true, .display_brightness = brightness});
    display_set_backlight(s_settings.display.brightness);
    save_to_file();
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

void settings_set_night_schedule(const dim_schedule_t *ns)
{
    s_settings.display.dim_schedule.enabled        = ns->enabled;
    s_settings.display.dim_schedule.dim_hour       = clampi(ns->dim_hour,       0, 23);
    s_settings.display.dim_schedule.dim_minute     = clampi(ns->dim_minute,     0, 59);
    s_settings.display.dim_schedule.dim_brightness = clampi(ns->dim_brightness, 0, 100);
    s_settings.display.dim_schedule.bright_hour    = clampi(ns->bright_hour,    0, 23);
    s_settings.display.dim_schedule.bright_minute  = clampi(ns->bright_minute,  0, 59);
    s_settings.display.dim_schedule.radio_off      = ns->radio_off;
    s_settings.display.dim_schedule.radio_on       = ns->radio_on;
    s_settings.display.dim_schedule.radio_station  = (ns->radio_station < 0) ? 0 : ns->radio_station;
    s_settings.display.dim_schedule.radio_volume   = clampi(ns->radio_volume,   0, 100);
    save_to_file();
    // Caller (HTTP handler) invokes dim_schedule_apply_now() — kept out of this
    // module to avoid a settings_ex ↔ services circular dependency.
}

// persist=false skips the SPIFFS write — used by the BT auto-switch, which must
// stay off the flash (it runs on the BT UART task and can fire on every
// reconnect). It still keeps s_settings/app_state/GPIO consistent so the guard
// in the persisting variant can later switch the source back.
static void apply_bt_enable(bool enable, bool persist)
{
    if(s_settings.bluetooth.enable != enable) {
        s_settings.bluetooth.enable = enable;
        app_state_update(&(app_state_patch_t){ .has_bt_enable = true, .bt_enable = enable });
        bt_set_enabled(enable);
        if (persist)
            save_to_file();
        // Update www/screen BT Volume only if turn on
        if(enable)
            bt_set_volume(s_settings.bluetooth.volume);
    }
}

void settings_set_bt_enable(bool enable)
{
    apply_bt_enable(enable, true);
}

void settings_set_bt_enable_volatile(bool enable)
{
    apply_bt_enable(enable, false);
}

void settings_set_bt_show_screen(bool show)
{
    if(s_settings.bluetooth.show_screen != show) {
        s_settings.bluetooth.show_screen = show;
        app_state_update(&(app_state_patch_t){ .has_bt_show_screen = true, .bt_show_screen = show });
        save_to_file();
    }
}

void settings_set_bt_auto_switch(bool enable)
{
    if(s_settings.bluetooth.auto_switch != enable) {
        s_settings.bluetooth.auto_switch = enable;
        app_state_update(&(app_state_patch_t){ .has_bt_auto_switch = true, .bt_auto_switch = enable });
        save_to_file();
    }
}

void settings_set_bt_volume(int volume)
{
    s_settings.bluetooth.volume = volume;
    app_state_update(&(app_state_patch_t){ .has_bt_volume = true, .bt_volume = volume });
    bt_set_volume(volume);
    save_to_file();
}

/*
void settings_set_ntp(const char *server1, const char *server2, const char *tz)
Note: does NOT call ntp_service_reconfigure() — to avoid a circular dependency.
Reconfigure is called by http_server after saving.
*/
void settings_set_ntp(const char *server1, const char *server2, const char *tz)
{
    if (server1) strncpy(s_settings.ntp.server1, server1, sizeof(s_settings.ntp.server1) - 1);
    if (server2) strncpy(s_settings.ntp.server2, server2, sizeof(s_settings.ntp.server2) - 1);
    if (tz)      strncpy(s_settings.ntp.tz,      tz,      sizeof(s_settings.ntp.tz)      - 1);
    save_to_file();
}

void settings_set_theme(ui_theme_t t)
{
    ESP_LOGI("SETTINGS", "settings_set_theme called: t=%d", (int)t);
    s_settings.display.theme = t;
    theme_set(t);   // natychmiastowa zmiana palety
    app_state_update(&(app_state_patch_t){ .has_theme = true, .theme = t });
    save_to_file();
}

void settings_set_bg_gradient(bool enabled)
{
    if (s_settings.display.bg_gradient == enabled) return;
    s_settings.display.bg_gradient = enabled;
    app_state_update(&(app_state_patch_t){ .has_bg_gradient = true, .bg_gradient = enabled });
    save_to_file();
}

void settings_set_wifi(const char *ssid, const char *password)
{
    if (ssid)     strncpy(s_settings.wifi.ssid,     ssid,     sizeof(s_settings.wifi.ssid)     - 1);
    if (password) strncpy(s_settings.wifi.password, password, sizeof(s_settings.wifi.password) - 1);
    ESP_LOGI("SETTINGS", "WiFi saved: ssid=\"%s\"", s_settings.wifi.ssid);
    save_to_file();
}

void settings_set_hostname(const char *hostname)
{
    s_settings.device.hostname[0] = '\0';
    if (hostname) strncpy(s_settings.device.hostname, hostname, sizeof(s_settings.device.hostname) - 1);
    ESP_LOGI("SETTINGS", "Hostname saved: \"%s\"", s_settings.device.hostname);
    save_to_file();
}

void settings_set_scrsaver_delay(int delay)
{
    if (delay < 0) delay = 0;
    if (s_settings.scrsaver.delay == delay) return;
    s_settings.scrsaver.delay = delay;
    app_state_update(&(app_state_patch_t){
        .has_scrsaver_delay = true, .scrsaver_delay = delay
    });
    save_to_file();
}

void settings_set_scrsaver_id(int id)
{
    if (!screensaver_is_valid(id)) id = SCREENSAVER_CLOCKHANDS;
    if (s_settings.scrsaver.screensaver_id == id) return;
    s_settings.scrsaver.screensaver_id = id;
    app_state_update(&(app_state_patch_t){
        .has_scrsaver_id = true, .scrsaver_id = id
    });
    save_to_file();
}

static unsigned s_photo_gen = 0;

void settings_set_photo(const char *dir, int order, int hold_s, int effect, int speed)
{
    if (dir && dir[0]) {
        s_settings.scrsaver.photo_dir[0] = '\0';
        strncpy(s_settings.scrsaver.photo_dir, dir, sizeof(s_settings.scrsaver.photo_dir) - 1);
    }
    s_settings.scrsaver.photo_order  = order ? 1 : 0;
    s_settings.scrsaver.photo_hold_s = hold_s < 1 ? 1 : hold_s;
    s_settings.scrsaver.photo_effect = (effect < 0 || effect > 4) ? 4 : effect;
    s_settings.scrsaver.photo_speed  = speed < 1 ? 1 : (speed > 5 ? 5 : speed);
    s_photo_gen++;
    save_to_file();
}

unsigned settings_photo_generation(void)
{
    return s_photo_gen;
}

void settings_set_dashboard(const char *title,
                            const char *url,
                            const char *json_path,
                            const char *suffix,
                            int poll_interval_ms)
{
    if (title) {
        s_settings.dashboard.title[0] = '\0';
        strncpy(s_settings.dashboard.title, title, sizeof(s_settings.dashboard.title) - 1);
    }
    if (url) {
        s_settings.dashboard.url[0] = '\0';
        strncpy(s_settings.dashboard.url, url, sizeof(s_settings.dashboard.url) - 1);
    }
    if (json_path) {
        s_settings.dashboard.json_path[0] = '\0';
        strncpy(s_settings.dashboard.json_path, json_path, sizeof(s_settings.dashboard.json_path) - 1);
    }
    if (suffix) {
        s_settings.dashboard.suffix[0] = '\0';
        strncpy(s_settings.dashboard.suffix, suffix, sizeof(s_settings.dashboard.suffix) - 1);
    }
    if (poll_interval_ms > 0) {
        if (poll_interval_ms < 5000) poll_interval_ms = 5000;
        s_settings.dashboard.poll_interval_ms = poll_interval_ms;
    }
    save_to_file();
}