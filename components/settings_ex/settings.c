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
        s_settings.display.brightness       = 80;
        s_settings.display.screen           = SCREEN_CLOCK;
        s_settings.display.theme            = THEME_DARK;
        s_settings.bluetooth.enable         = false;
        s_settings.bluetooth.show_screen    = true;
        s_settings.bluetooth.volume         = 15;
        strncpy(s_settings.ntp.server1, "pool.ntp.org",                sizeof(s_settings.ntp.server1) - 1);
        strncpy(s_settings.ntp.server2, "time.cloudflare.com",         sizeof(s_settings.ntp.server2) - 1);
        strncpy(s_settings.ntp.tz,      "CET-1CEST,M3.5.0,M10.5.0/3", sizeof(s_settings.ntp.tz)      - 1);
        // WiFi — empty = AP mode on first boot
        s_settings.wifi.ssid[0]             = '\0';
        s_settings.wifi.password[0]         = '\0';
        // Screensaver
        s_settings.scrsaver.delay           = 60;
        s_settings.scrsaver.screensaver_id  = SCREENSAVER_CLOCKHANDS;
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
    cJSON *playlist = cJSON_GetObjectItem(json, "playlist");
    if (cJSON_IsObject(playlist)) {
        cJSON *curr_index = cJSON_GetObjectItem(playlist, "curr_index");
        if (cJSON_IsNumber(curr_index)) {
            s_settings.playlist.curr_index = curr_index->valueint;
        }
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
    cJSON_AddItemToObject(json, "playlist", playlist);

    // display
    cJSON *display = cJSON_CreateObject();
    cJSON_AddNumberToObject(display, "screen", s_settings.display.screen);
    cJSON_AddNumberToObject(display, "brightness", s_settings.display.brightness);
    cJSON_AddStringToObject(display, "theme",
        s_settings.display.theme == THEME_LIGHT ? "light" : "dark");
    cJSON_AddItemToObject(json, "display", display);

    // bluetooth
    cJSON *bluetooth = cJSON_CreateObject();
    cJSON_AddBoolToObject(bluetooth,   "enable", s_settings.bluetooth.enable);
    cJSON_AddBoolToObject(bluetooth,   "show_screen", s_settings.bluetooth.show_screen);
    cJSON_AddNumberToObject(bluetooth, "volume", s_settings.bluetooth.volume);
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

    // screensaver
    cJSON *scrs = cJSON_CreateObject();
    cJSON_AddNumberToObject(scrs, "delay",  s_settings.scrsaver.delay);
    cJSON_AddStringToObject(scrs, "id",
        screensaver_name(s_settings.scrsaver.screensaver_id));
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
        .has_display_brightness = true, .display_brightness = s_settings.display.brightness,
        .has_theme              = true, .theme     = s_settings.display.theme,
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

void settings_set_bt_enable(bool enable)
{
    if(s_settings.bluetooth.enable != enable) {
        s_settings.bluetooth.enable = enable;
        app_state_update(&(app_state_patch_t){ .has_bt_enable = true, .bt_enable = enable });
        bt_set_enabled(enable);
        save_to_file();
        // Update www/screen BT Volume only if turn on
        if(enable)
            bt_set_volume(s_settings.bluetooth.volume);
    }
}

void settings_set_bt_show_screen(bool show)
{
    if(s_settings.bluetooth.show_screen != show) {
        s_settings.bluetooth.show_screen = show;
        app_state_update(&(app_state_patch_t){ .has_bt_show_screen = true, .bt_show_screen = show });
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

void settings_set_wifi(const char *ssid, const char *password)
{
    if (ssid)     strncpy(s_settings.wifi.ssid,     ssid,     sizeof(s_settings.wifi.ssid)     - 1);
    if (password) strncpy(s_settings.wifi.password, password, sizeof(s_settings.wifi.password) - 1);
    ESP_LOGI("SETTINGS", "WiFi saved: ssid=\"%s\"", s_settings.wifi.ssid);
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