#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "radio_service.h"
#include "ws_server.h"
#include "settings.h"
#include "ntp_service.h"
#include "dim_schedule.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "mdns_service.h"
#include "theme.h"
#include "ui_manager.h"
#include "ui_events.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "playlist.h"
#include "events_service.h"
#include "screensavers.h"
#include "screensaver_dashboard.h"
#include "mqtt_svc.h"
#include "mqtt_config.h"
#include "sdcard.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "zlib.h"
#include "defines.h"

extern void ws_set_server(httpd_handle_t server);

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/settings  — returns full JSON with current settings state
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    app_settings_t *s = settings_get();

    cJSON *json = cJSON_CreateObject();

    // audio
    cJSON *audio = cJSON_CreateObject();
    cJSON_AddNumberToObject(audio, "volume", s->audio.volume);
    cJSON_AddItemToObject(audio, "eq", cJSON_CreateIntArray(s->audio.eq, 10));
    cJSON_AddBoolToObject(audio,   "eq_enabled", s->audio.eq_enabled);
    cJSON_AddItemToObject(json, "audio", audio);

    // playlist
    cJSON *playlist = cJSON_CreateObject();
    cJSON_AddNumberToObject(playlist, "curr_index", s->playlist.curr_index);
    cJSON_AddBoolToObject  (playlist, "resume_on_boot", s->playlist.resume_on_boot);
    cJSON_AddItemToObject(json, "playlist", playlist);

    // display
    cJSON *display = cJSON_CreateObject();
    cJSON_AddNumberToObject(display, "brightness", s->display.brightness);
    cJSON_AddStringToObject(display, "theme",
        s->display.theme == THEME_LIGHT ? "light" : "dark");
    cJSON_AddBoolToObject(display, "bg_gradient", s->display.bg_gradient);
    cJSON_AddBoolToObject(display, "wallpaper_on", s->display.wallpaper_on);
    cJSON_AddStringToObject(display, "wallpaper_path", s->display.wallpaper_path);
    cJSON_AddBoolToObject(display, "show_boot_info", s->display.show_boot_info);
    cJSON *dim = cJSON_CreateObject();
    cJSON_AddBoolToObject  (dim, "enabled",        s->display.dim_schedule.enabled);
    cJSON_AddNumberToObject(dim, "dim_hour",       s->display.dim_schedule.dim_hour);
    cJSON_AddNumberToObject(dim, "dim_minute",     s->display.dim_schedule.dim_minute);
    cJSON_AddNumberToObject(dim, "dim_brightness", s->display.dim_schedule.dim_brightness);
    cJSON_AddNumberToObject(dim, "bright_hour",    s->display.dim_schedule.bright_hour);
    cJSON_AddNumberToObject(dim, "bright_minute",  s->display.dim_schedule.bright_minute);
    cJSON_AddBoolToObject  (dim, "radio_off",      s->display.dim_schedule.radio_off);
    cJSON_AddBoolToObject  (dim, "radio_on",       s->display.dim_schedule.radio_on);
    cJSON_AddNumberToObject(dim, "radio_station",  s->display.dim_schedule.radio_station);
    cJSON_AddNumberToObject(dim, "radio_volume",   s->display.dim_schedule.radio_volume);
    cJSON_AddItemToObject(display, "dim_schedule", dim);
    cJSON_AddItemToObject(json, "display", display);

    // bluetooth
    cJSON *bt = cJSON_CreateObject();
    cJSON_AddBoolToObject(bt,   "enable", s->bluetooth.enable);
    cJSON_AddBoolToObject(bt,   "show_screen", s->bluetooth.show_screen);
    cJSON_AddNumberToObject(bt, "volume", s->bluetooth.volume);
    cJSON_AddBoolToObject(bt,   "auto_switch", s->bluetooth.auto_switch);
    cJSON_AddItemToObject(json, "bluetooth", bt);

    // ntp
    cJSON *ntp = cJSON_CreateObject();
    cJSON_AddStringToObject(ntp, "server1", s->ntp.server1);
    cJSON_AddStringToObject(ntp, "server2", s->ntp.server2);
    cJSON_AddStringToObject(ntp, "tz",      s->ntp.tz);
    cJSON_AddItemToObject(json, "ntp", ntp);

    // wifi — return password as empty string (security)
    cJSON *wifi_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi_obj, "ssid",     s->wifi.ssid);
    cJSON_AddStringToObject(wifi_obj, "password", "");   // never send the password back
    cJSON_AddItemToObject(json, "wifi", wifi_obj);

    // device — configured hostname (may be empty) + resolved effective name
    cJSON *device_obj = cJSON_CreateObject();
    char eff_host[32];
    cJSON_AddStringToObject(device_obj, "hostname",  s->device.hostname);
    cJSON_AddStringToObject(device_obj, "effective", mdns_effective_hostname(eff_host, sizeof(eff_host)));
    cJSON_AddItemToObject(json, "device", device_obj);

    // screensaver
    cJSON *scrs = cJSON_CreateObject();
    cJSON_AddNumberToObject(scrs, "delay",  s->scrsaver.delay);
    cJSON_AddStringToObject(scrs, "id",
        screensaver_name(s->scrsaver.screensaver_id));
    cJSON *scrs_photo = cJSON_CreateObject();
    cJSON_AddStringToObject(scrs_photo, "dir",    s->scrsaver.photo_dir);
    cJSON_AddNumberToObject(scrs_photo, "order",  s->scrsaver.photo_order);
    cJSON_AddNumberToObject(scrs_photo, "hold_s", s->scrsaver.photo_hold_s);
    cJSON_AddNumberToObject(scrs_photo, "effect", s->scrsaver.photo_effect);
    cJSON_AddNumberToObject(scrs_photo, "speed",  s->scrsaver.photo_speed);
    cJSON_AddNumberToObject(scrs_photo, "clock",      s->scrsaver.photo_clock);
    cJSON_AddNumberToObject(scrs_photo, "clock_size", s->scrsaver.photo_clock_size);
    cJSON_AddItemToObject(scrs, "photo", scrs_photo);
    cJSON_AddItemToObject(json, "scrsaver", scrs);

    // dashboard
    cJSON *dash = cJSON_CreateObject();
    cJSON_AddStringToObject(dash, "title",            s->dashboard.title);
    cJSON_AddStringToObject(dash, "url",              s->dashboard.url);
    cJSON_AddStringToObject(dash, "json_path",        s->dashboard.json_path);
    cJSON_AddStringToObject(dash, "suffix",           s->dashboard.suffix);
    cJSON_AddNumberToObject(dash, "poll_interval_ms", s->dashboard.poll_interval_ms);
    cJSON *notify = cJSON_CreateObject();
    cJSON_AddBoolToObject  (notify, "enabled",     s->dashboard.notify_enabled);
    cJSON_AddStringToObject(notify, "value_type",  s->dashboard.value_type == DASHBOARD_VALUE_STRING ? "string" : "number");
    cJSON_AddBoolToObject  (notify, "num_low_en",  s->dashboard.notify_num_low_en);
    cJSON_AddNumberToObject(notify, "num_low",     s->dashboard.notify_num_low);
    cJSON_AddBoolToObject  (notify, "num_high_en", s->dashboard.notify_num_high_en);
    cJSON_AddNumberToObject(notify, "num_high",    s->dashboard.notify_num_high);
    cJSON_AddBoolToObject  (notify, "str_eq_en",   s->dashboard.notify_str_eq_en);
    cJSON_AddStringToObject(notify, "str_eq",      s->dashboard.notify_str_eq);
    cJSON_AddBoolToObject  (notify, "str_ne_en",   s->dashboard.notify_str_ne_en);
    cJSON_AddStringToObject(notify, "str_ne",      s->dashboard.notify_str_ne);
    cJSON_AddItemToObject  (dash, "notify", notify);
    cJSON_AddItemToObject(json, "dashboard", dash);


    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);

    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/settings  — accepts JSON with fields to override
// Partial JSON is accepted (e.g. just { "ntp": {...} })
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_settings_post_handler(httpd_req_t *req)
{
    int total = req->content_len;

    if (total <= 0 || total > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total) {
        int ret = httpd_req_recv(req, buf + received, total - received);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = 0;

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // ── NTP ──────────────────────────────────────────────────────────────────
    cJSON *ntp = cJSON_GetObjectItem(json, "ntp");
    if (cJSON_IsObject(ntp)) {
        cJSON *srv1 = cJSON_GetObjectItem(ntp, "server1");
        cJSON *srv2 = cJSON_GetObjectItem(ntp, "server2");
        cJSON *tz   = cJSON_GetObjectItem(ntp, "tz");

        if (cJSON_IsString(srv1) && cJSON_IsString(srv2) && cJSON_IsString(tz)) {
            settings_set_ntp(srv1->valuestring, srv2->valuestring, tz->valuestring);
            ntp_service_reconfigure(srv1->valuestring, srv2->valuestring, tz->valuestring);
            dim_schedule_apply_now();   // TZ change shifts the dim window
        } else {
            ESP_LOGW("HTTP", "/api/settings POST: ntp section incomplete, skipping");
        }
    }

    // ── AUDIO ─────────────────────────────────────────────────────────────────
    cJSON *audio = cJSON_GetObjectItem(json, "audio");
    if (cJSON_IsObject(audio)) {
        cJSON *vol = cJSON_GetObjectItem(audio, "volume");
        if (cJSON_IsNumber(vol)) {
            settings_set_volume(vol->valueint);
        }
        cJSON *eq = cJSON_GetObjectItem(audio, "eq");
        if (cJSON_IsArray(eq) && cJSON_GetArraySize(eq) == 10) {
            int bands[10];
            for (int i = 0; i < 10; i++) {
                cJSON *item = cJSON_GetArrayItem(eq, i);
                bands[i] = cJSON_IsNumber(item) ? item->valueint : 0;
            }
            settings_set_eq_10(bands);
        }
        cJSON *eq_en = cJSON_GetObjectItem(audio, "eq_enabled");
        if (cJSON_IsBool(eq_en)) {
            settings_set_eq_enabled(cJSON_IsTrue(eq_en));
        }
    }

    // ── PLAYLIST ────────────────────────────────────────────────────────────
    cJSON *playlist = cJSON_GetObjectItem(json, "playlist");
    if (cJSON_IsObject(playlist)) {
        cJSON *rob = cJSON_GetObjectItem(playlist, "resume_on_boot");
        if (cJSON_IsBool(rob)) {
            ESP_LOGI("HTTP", "POST resume_on_boot: %d", cJSON_IsTrue(rob));
            settings_set_resume_on_boot(cJSON_IsTrue(rob));
        }
    }

    // ── DISPLAY ───────────────────────────────────────────────────────────────
    cJSON *display = cJSON_GetObjectItem(json, "display");
    if (cJSON_IsObject(display)) {
        cJSON *br = cJSON_GetObjectItem(display, "brightness");
        if (cJSON_IsNumber(br)) {
            ESP_LOGI("HTTP", "POST display brightness: %d", br->valueint);
            settings_set_brightness(br->valueint);
        }
        cJSON *th = cJSON_GetObjectItem(display, "theme");
        if (cJSON_IsString(th)) {
            ui_theme_t t = (strcmp(th->valuestring, "light") == 0)
                           ? THEME_LIGHT : THEME_DARK;
            ESP_LOGI("HTTP", "POST theme: string='%s' → enum=%d", th->valuestring, (int)t);
            settings_set_theme(t);
        }
        cJSON *bg = cJSON_GetObjectItem(display, "bg_gradient");
        if (cJSON_IsBool(bg)) {
            ESP_LOGI("HTTP", "POST bg_gradient: %d", cJSON_IsTrue(bg));
            settings_set_bg_gradient(cJSON_IsTrue(bg));
        }
        cJSON *wp  = cJSON_GetObjectItem(display, "wallpaper_on");
        cJSON *wpp = cJSON_GetObjectItem(display, "wallpaper_path");
        if (cJSON_IsBool(wp) || cJSON_IsString(wpp)) {
            app_settings_t *cur = settings_get();
            bool on = cJSON_IsBool(wp) ? cJSON_IsTrue(wp) : cur->display.wallpaper_on;
            const char *path = cJSON_IsString(wpp) ? wpp->valuestring
                                                   : cur->display.wallpaper_path;
            ESP_LOGI("HTTP", "POST wallpaper: on=%d path=%s", on, path);
            settings_set_wallpaper(on, path);
        }
        cJSON *sbi = cJSON_GetObjectItem(display, "show_boot_info");
        if (cJSON_IsBool(sbi)) {
            ESP_LOGI("HTTP", "POST show_boot_info: %d", cJSON_IsTrue(sbi));
            settings_set_show_boot_info(cJSON_IsTrue(sbi));
        }
        cJSON *scr = cJSON_GetObjectItem(display, "screen");
        if (cJSON_IsString(scr)) {
            ESP_LOGI("HTTP", "POST screen: %s", scr->valuestring);
            if      (strcmp(scr->valuestring, "radio") == 0) settings_set_screen(SCREEN_RADIO);
            else if (strcmp(scr->valuestring, "clock") == 0) settings_set_screen(SCREEN_CLOCK);
            else if (strcmp(scr->valuestring, "bt")    == 0) settings_set_screen(SCREEN_BT);
            else ESP_LOGW("HTTP", "POST screen: unknown '%s'", scr->valuestring);
        }
        cJSON *dim = cJSON_GetObjectItem(display, "dim_schedule");
        if (cJSON_IsObject(dim)) {
            // start from current values so partial updates are allowed
            app_settings_t *cur = settings_get();
            dim_schedule_t ns = cur->display.dim_schedule;
            cJSON *j;
            j = cJSON_GetObjectItem(dim, "enabled");        if (cJSON_IsBool(j))   ns.enabled        = cJSON_IsTrue(j);
            j = cJSON_GetObjectItem(dim, "dim_hour");       if (cJSON_IsNumber(j)) ns.dim_hour       = j->valueint;
            j = cJSON_GetObjectItem(dim, "dim_minute");     if (cJSON_IsNumber(j)) ns.dim_minute     = j->valueint;
            j = cJSON_GetObjectItem(dim, "dim_brightness"); if (cJSON_IsNumber(j)) ns.dim_brightness = j->valueint;
            j = cJSON_GetObjectItem(dim, "bright_hour");    if (cJSON_IsNumber(j)) ns.bright_hour    = j->valueint;
            j = cJSON_GetObjectItem(dim, "bright_minute");  if (cJSON_IsNumber(j)) ns.bright_minute  = j->valueint;
            j = cJSON_GetObjectItem(dim, "radio_off");      if (cJSON_IsBool(j))   ns.radio_off      = cJSON_IsTrue(j);
            j = cJSON_GetObjectItem(dim, "radio_on");       if (cJSON_IsBool(j))   ns.radio_on       = cJSON_IsTrue(j);
            j = cJSON_GetObjectItem(dim, "radio_station");  if (cJSON_IsNumber(j)) ns.radio_station  = j->valueint;
            j = cJSON_GetObjectItem(dim, "radio_volume");   if (cJSON_IsNumber(j)) ns.radio_volume   = j->valueint;
            ESP_LOGI("HTTP", "POST dim_schedule: en=%d %02d:%02d→%d%% %02d:%02d roff=%d ron=%d st=%d vol=%d",
                     ns.enabled, ns.dim_hour, ns.dim_minute, ns.dim_brightness, ns.bright_hour, ns.bright_minute,
                     ns.radio_off, ns.radio_on, ns.radio_station, ns.radio_volume);
            settings_set_night_schedule(&ns);
            dim_schedule_apply_now();
        }
    }

    // ── BLUETOOTH ─────────────────────────────────────────────────────────────
    cJSON *bt = cJSON_GetObjectItem(json, "bluetooth");
    if (cJSON_IsObject(bt)) {
        cJSON *en       = cJSON_GetObjectItem(bt, "enable");
        cJSON *sh_scr   = cJSON_GetObjectItem(bt, "show_screen");
        cJSON *bvol     = cJSON_GetObjectItem(bt, "volume");
        cJSON *bauto    = cJSON_GetObjectItem(bt, "auto_switch");
        if (cJSON_IsBool(en))     settings_set_bt_enable(cJSON_IsTrue(en));
        if (cJSON_IsBool(sh_scr)) settings_set_bt_show_screen(cJSON_IsTrue(sh_scr));
        if (cJSON_IsNumber(bvol)) settings_set_bt_volume(bvol->valueint);
        if (cJSON_IsBool(bauto))  settings_set_bt_auto_switch(cJSON_IsTrue(bauto));
    }

    // ── SCREENSAVER ───────────────────────────────────────────────────────────
    cJSON *scrs = cJSON_GetObjectItem(json, "scrsaver");
    if (cJSON_IsObject(scrs)) {
        cJSON *dl = cJSON_GetObjectItem(scrs, "delay");
        cJSON *id = cJSON_GetObjectItem(scrs, "id");
        if (cJSON_IsNumber(dl)) settings_set_scrsaver_delay(dl->valueint);
        if (cJSON_IsString(id)) {
            settings_set_scrsaver_id(screensaver_from_name(id->valuestring));
        } else if (cJSON_IsNumber(id)) {
            settings_set_scrsaver_id(id->valueint);
        }
        cJSON *ph = cJSON_GetObjectItem(scrs, "photo");
        if (cJSON_IsObject(ph)) {
            app_settings_t *cur = settings_get();
            cJSON *pd  = cJSON_GetObjectItem(ph, "dir");
            cJSON *po  = cJSON_GetObjectItem(ph, "order");
            cJSON *phs = cJSON_GetObjectItem(ph, "hold_s");
            cJSON *pe  = cJSON_GetObjectItem(ph, "effect");
            cJSON *psp = cJSON_GetObjectItem(ph, "speed");
            cJSON *pck = cJSON_GetObjectItem(ph, "clock");
            cJSON *pcs = cJSON_GetObjectItem(ph, "clock_size");
            settings_set_photo(
                cJSON_IsString(pd) ? pd->valuestring     : cur->scrsaver.photo_dir,
                cJSON_IsNumber(po) ? po->valueint        : cur->scrsaver.photo_order,
                cJSON_IsNumber(phs)? phs->valueint       : cur->scrsaver.photo_hold_s,
                cJSON_IsNumber(pe) ? pe->valueint        : cur->scrsaver.photo_effect,
                cJSON_IsNumber(psp)? psp->valueint       : cur->scrsaver.photo_speed,
                cJSON_IsNumber(pck)? pck->valueint       : cur->scrsaver.photo_clock,
                cJSON_IsNumber(pcs)? pcs->valueint       : cur->scrsaver.photo_clock_size);
        }
    }

    // ── DASHBOARD ─────────────────────────────────────────────────────────────
    cJSON *dash = cJSON_GetObjectItem(json, "dashboard");
    if (cJSON_IsObject(dash)) {
        cJSON *t  = cJSON_GetObjectItem(dash, "title");
        cJSON *u  = cJSON_GetObjectItem(dash, "url");
        cJSON *jp = cJSON_GetObjectItem(dash, "json_path");
        cJSON *sf = cJSON_GetObjectItem(dash, "suffix");
        cJSON *pi = cJSON_GetObjectItem(dash, "poll_interval_ms");
        bool dashboard_touched = (cJSON_IsString(t)  || cJSON_IsString(u) ||
                                  cJSON_IsString(jp) || cJSON_IsString(sf) ||
                                  cJSON_IsNumber(pi));
        settings_set_dashboard(
            cJSON_IsString(t)  ? t->valuestring  : NULL,
            cJSON_IsString(u)  ? u->valuestring  : NULL,
            cJSON_IsString(jp) ? jp->valuestring : NULL,
            cJSON_IsString(sf) ? sf->valuestring : NULL,
            cJSON_IsNumber(pi) ? pi->valueint    : 0);

        // Notification subsection — apply directly via the mutable settings
        // pointer (avoids a huge setter signature for ~10 fields).
        cJSON *nx = cJSON_GetObjectItem(dash, "notify");
        if (cJSON_IsObject(nx)) {
            app_settings_t *ds = settings_get();
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
            if (cJSON_IsBool(en))   ds->dashboard.notify_enabled     = cJSON_IsTrue(en);
            if (cJSON_IsString(vt)) ds->dashboard.value_type         = strcmp(vt->valuestring, "string") == 0 ? DASHBOARD_VALUE_STRING : DASHBOARD_VALUE_NUMBER;
            if (cJSON_IsBool(nle))  ds->dashboard.notify_num_low_en  = cJSON_IsTrue(nle);
            if (cJSON_IsNumber(nl)) ds->dashboard.notify_num_low     = nl->valuedouble;
            if (cJSON_IsBool(nhe))  ds->dashboard.notify_num_high_en = cJSON_IsTrue(nhe);
            if (cJSON_IsNumber(nh)) ds->dashboard.notify_num_high    = nh->valuedouble;
            if (cJSON_IsBool(see))  ds->dashboard.notify_str_eq_en   = cJSON_IsTrue(see);
            if (cJSON_IsString(se)) { ds->dashboard.notify_str_eq[0] = '\0'; strncpy(ds->dashboard.notify_str_eq, se->valuestring, sizeof(ds->dashboard.notify_str_eq) - 1); }
            if (cJSON_IsBool(sne))  ds->dashboard.notify_str_ne_en   = cJSON_IsTrue(sne);
            if (cJSON_IsString(sn)) { ds->dashboard.notify_str_ne[0] = '\0'; strncpy(ds->dashboard.notify_str_ne, sn->valuestring, sizeof(ds->dashboard.notify_str_ne) - 1); }
            settings_save();
            dashboard_touched = true;
        }

        if (dashboard_touched) {
            // Live-reload an active dashboard screensaver; no-op if inactive.
            screensaver_dashboard_settings_changed();
        }
    }

    // ── WIFI ──────────────────────────────────────────────────────────────────
    cJSON *wifi_obj = cJSON_GetObjectItem(json, "wifi");
    if (cJSON_IsObject(wifi_obj)) {
        cJSON *ssid = cJSON_GetObjectItem(wifi_obj, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi_obj, "password");
        if (cJSON_IsString(ssid) && ssid->valuestring[0] != '\0') {
            // password: if empty → keep the old one (do not overwrite)
            const char *new_pass = (cJSON_IsString(pass) && pass->valuestring[0] != '\0')
                                   ? pass->valuestring : NULL;
            settings_set_wifi(ssid->valuestring, new_pass);
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

    // ── DEVICE ────────────────────────────────────────────────────────────────
    cJSON *device_obj = cJSON_GetObjectItem(json, "device");
    if (cJSON_IsObject(device_obj)) {
        cJSON *hn = cJSON_GetObjectItem(device_obj, "hostname");
        if (cJSON_IsString(hn)) {
            settings_set_hostname(hn->valuestring);
            mdns_service_apply_hostname();   // live, no reboot
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/state  — current runtime state (app_state), not the file settings
// Used by the web UI to initialize widgets (e.g. theme toggle)
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_state_get_handler(httpd_req_t *req)
{
    app_state_t *s = app_state_get();

    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "theme",
        s->theme == THEME_LIGHT ? "light" : "dark");
    cJSON_AddNumberToObject(json, "volume",         s->volume);
    cJSON_AddNumberToObject(json, "radio_state",    s->radio_state);
    cJSON_AddStringToObject(json, "station",        s->station_name);
    cJSON_AddStringToObject(json, "title",          s->title);
    cJSON_AddBoolToObject  (json, "bt_enable",      s->bt_enable);
    cJSON_AddBoolToObject  (json, "bt_auto_switch", s->bt_auto_switch);
    cJSON_AddBoolToObject  (json, "bt_show_screen", s->bt_show_screen);
    cJSON_AddBoolToObject  (json, "time_synced",    s->time_synced);
    // WiFi mode — useful for the settings page UI
    cJSON_AddStringToObject(json, "wifi_mode",
        wifi_get_run_mode() == WIFI_RUN_MODE_AP ? "ap" : "sta");
    // Firmware version (git describe) — lets the web UI confirm what was flashed
    cJSON_AddStringToObject(json, "version", esp_app_get_description()->version);

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/theme  — returns both palettes + the active theme
// ─────────────────────────────────────────────────────────────────────────────
static void add_palette_json(cJSON *parent, const char *name,
                             const ui_theme_colors_t *c)
{
    cJSON *o = cJSON_CreateObject();
    char buf[8];
    #define F(k) do {                                                         \
        snprintf(buf, sizeof(buf), "#%06lX",                                  \
                 (unsigned long)(c->k & 0xFFFFFF));                           \
        cJSON_AddStringToObject(o, #k, buf);                                  \
    } while (0)
    F(bg_primary);
    F(bg_secondary);
    F(text_primary);
    F(text_secondary);
    F(text_muted);
    F(accent);
    F(bt_brand);
    F(status_ok);
    F(bg_grad_top);
    F(bg_grad_bottom);
    #undef F
    cJSON_AddItemToObject(parent, name, o);
}

static esp_err_t api_theme_get_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "current",
        theme_current() == THEME_LIGHT ? "light" : "dark");
    add_palette_json(json, "dark",  theme_palette_get(THEME_DARK));
    add_palette_json(json, "light", theme_palette_get(THEME_LIGHT));

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/theme  — body: { "dark":{...}, "light":{...}, "reset":"dark|light|all" }
// Palette fields: "#RRGGBB" hex strings. Partial JSON is accepted — missing
// fields stay unchanged. After saving we force a re-render of the screens.
// ─────────────────────────────────────────────────────────────────────────────
static void patch_palette_from_json(cJSON *obj, ui_theme_t t)
{
    if (!cJSON_IsObject(obj)) return;

    ui_theme_colors_t c = *theme_palette_get(t);

    #define PATCH(field) do {                                                 \
        cJSON *it = cJSON_GetObjectItem(obj, #field);                         \
        if (cJSON_IsString(it)) {                                             \
            const char *s = it->valuestring;                                  \
            if (*s == '#') s++;                                               \
            char *end = NULL;                                                 \
            unsigned long v = strtoul(s, &end, 16);                           \
            if (end && *end == 0) c.field = (uint32_t)(v & 0xFFFFFF);         \
        }                                                                     \
    } while (0)

    PATCH(bg_primary);
    PATCH(bg_secondary);
    PATCH(text_primary);
    PATCH(text_secondary);
    PATCH(text_muted);
    PATCH(accent);
    PATCH(bt_brand);
    PATCH(status_ok);
    PATCH(bg_grad_top);
    PATCH(bg_grad_bottom);
    #undef PATCH

    theme_palette_set(t, &c);
}

static esp_err_t api_theme_post_handler(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        received += r;
    }
    buf[received] = 0;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    // optional reset before the patch — enables "reset + override individual fields"
    cJSON *rst = cJSON_GetObjectItem(json, "reset");
    if (cJSON_IsString(rst)) {
        if (!strcmp(rst->valuestring, "dark") || !strcmp(rst->valuestring, "all"))
            theme_palette_reset(THEME_DARK);
        if (!strcmp(rst->valuestring, "light") || !strcmp(rst->valuestring, "all"))
            theme_palette_reset(THEME_LIGHT);
    }

    patch_palette_from_json(cJSON_GetObjectItem(json, "dark"),  THEME_DARK);
    patch_palette_from_json(cJSON_GetObjectItem(json, "light"), THEME_LIGHT);

    cJSON_Delete(json);

    theme_save_to_file();

    // Force a screen re-render (ui_manager.on_state_change compares the theme
    // enum, so a palette-only change would not trigger UI_EVT_THEME_CHANGED — push it manually)
    ui_event_t ev = { .type = UI_EVT_THEME_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/playlist  — returns array [{name,url}, ...] from memory
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_playlist_get_handler(httpd_req_t *req)
{
    cJSON *arr = cJSON_CreateArray();
    int n = playlist_get_count();
    for (int i = 0; i < n; i++) {
        const playlist_entry_t *e = playlist_get(i);
        if (!e) continue;
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", e->name);
        cJSON_AddStringToObject(o, "url",  e->url);
        cJSON_AddBoolToObject  (o, "favorite", e->favorite);
        cJSON_AddItemToArray(arr, o);
    }

    char *str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/playlist.csv  — downloads the playlist in the on-disk format
// (name\turl\t<0|1>\n). Reflects the last *saved* state, not unsaved edits.
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_playlist_csv_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/csv; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Disposition",
                      "attachment; filename=\"playlist.csv\"");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    char line[PLAYLIST_NAME_LEN + PLAYLIST_URL_LEN + 8];
    int n = playlist_get_count();
    for (int i = 0; i < n; i++) {
        const playlist_entry_t *e = playlist_get(i);
        if (!e) continue;
        int len = snprintf(line, sizeof(line), "%s\t%s\t%d\n",
                           e->name, e->url, e->favorite ? 1 : 0);
        if (len > 0) httpd_resp_send_chunk(req, line, len);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/playlist  — body: [{name,url}, ...] — overwrites the CSV file
// and reloads the playlist into memory.
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_playlist_post_handler(httpd_req_t *req)
{
    int total = req->content_len;
    if (total <= 0 || total > 32768) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad content length");
        return ESP_FAIL;
    }

    char *buf = malloc(total + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        received += r;
    }
    buf[received] = 0;

    cJSON *arr = cJSON_Parse(buf);
    free(buf);
    if (!cJSON_IsArray(arr)) {
        if (arr) cJSON_Delete(arr);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON array");
        return ESP_FAIL;
    }

    int n = cJSON_GetArraySize(arr);
    if (n > PLAYLIST_MAX_ENTRIES) {
        cJSON_Delete(arr);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Too many entries");
        return ESP_FAIL;
    }

    // atomic write: tmp → rename
    const char *tmp_path = PLAYLIST_FILE ".tmp";
    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        cJSON_Delete(arr);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open tmp file");
        return ESP_FAIL;
    }

    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *jn = cJSON_GetObjectItem(item, "name");
        cJSON *ju = cJSON_GetObjectItem(item, "url");
        cJSON *jf = cJSON_GetObjectItem(item, "favorite");
        if (!cJSON_IsString(jn) || !cJSON_IsString(ju))       continue;
        if (jn->valuestring[0] == 0 || ju->valuestring[0] == 0) continue;
        // sanitize: no tabs or newlines in fields
        for (char *p = jn->valuestring; *p; p++) if (*p == '\t' || *p == '\r' || *p == '\n') *p = ' ';
        for (char *p = ju->valuestring; *p; p++) if (*p == '\t' || *p == '\r' || *p == '\n') *p = ' ';
        int fav = cJSON_IsTrue(jf) ? 1 : 0;
        fprintf(f, "%s\t%s\t%d\n", jn->valuestring, ju->valuestring, fav);
    }
    fflush(f);
    fclose(f);
    cJSON_Delete(arr);

    // SPIFFS does not allow rename onto an existing target — remove the old one first
    remove(PLAYLIST_FILE);
    if (rename(tmp_path, PLAYLIST_FILE) != 0) {
        ESP_LOGE("HTTP", "rename %s → %s failed", tmp_path, PLAYLIST_FILE);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rename failed");
        return ESP_FAIL;
    }

    playlist_load();
    radio_resync_curr_index();   // re-anchor curr_index after reorder/edit

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// Events — helpers
// ─────────────────────────────────────────────────────────────────────────────
static const char *ev_type_str(event_type_t t)
{
    switch (t) {
        case EV_BIRTHDAY:    return "birthday";
        case EV_NAMEDAY:     return "nameday";
        case EV_REMINDER:    return "reminder";
        case EV_ANNIVERSARY: return "anniversary";
        case EV_VOICE:       return "voice";
        case EV_SCHEDULE:    return "schedule";
        default:             return "reminder";
    }
}

static event_type_t ev_type_from_str(const char *s)
{
    if (!s) return EV_REMINDER;
    if (strcmp(s, "birthday")    == 0) return EV_BIRTHDAY;
    if (strcmp(s, "nameday")     == 0) return EV_NAMEDAY;
    if (strcmp(s, "anniversary") == 0) return EV_ANNIVERSARY;
    if (strcmp(s, "alarm")       == 0) return EV_SCHEDULE;  // legacy → playback
    if (strcmp(s, "voice")       == 0) return EV_VOICE;
    if (strcmp(s, "schedule")    == 0) return EV_SCHEDULE;
    return EV_REMINDER;
}

static const char *ev_rec_str(event_recurrence_t r)
{
    switch (r) {
        case EV_REC_NONE:    return "none";
        case EV_REC_DAILY:   return "daily";
        case EV_REC_WEEKLY:  return "weekly";
        case EV_REC_MONTHLY: return "monthly";
        case EV_REC_YEARLY:  return "yearly";
        default:             return "none";
    }
}

static event_recurrence_t ev_rec_from_str(const char *s)
{
    if (!s) return EV_REC_NONE;
    if (strcmp(s, "daily")   == 0) return EV_REC_DAILY;
    if (strcmp(s, "weekly")  == 0) return EV_REC_WEEKLY;
    if (strcmp(s, "monthly") == 0) return EV_REC_MONTHLY;
    if (strcmp(s, "yearly")  == 0) return EV_REC_YEARLY;
    return EV_REC_NONE;
}

static cJSON *event_to_json(const event_t *e)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "id",                e->id);
    cJSON_AddStringToObject(o, "type",              ev_type_str(e->type));
    cJSON_AddStringToObject(o, "title",             e->title);
    cJSON_AddNumberToObject(o, "year",              e->year);
    cJSON_AddNumberToObject(o, "month",             e->month);
    cJSON_AddNumberToObject(o, "day",               e->day);
    cJSON_AddNumberToObject(o, "hour",              e->hour);
    cJSON_AddNumberToObject(o, "minute",            e->minute);
    cJSON_AddStringToObject(o, "recurrence",        ev_rec_str(e->recurrence));
    cJSON_AddBoolToObject  (o, "enabled",           e->enabled);
    cJSON_AddNumberToObject(o, "station",           e->station);
    cJSON_AddNumberToObject(o, "volume",            e->volume);
    cJSON_AddStringToObject(o, "sound",             e->sound);
    return o;
}

// Fills `e` with fields from JSON. Missing fields stay unchanged (important for PUT).
static void event_patch_from_json(event_t *e, const cJSON *obj)
{
    if (!cJSON_IsObject(obj)) return;

    const cJSON *j;
    j = cJSON_GetObjectItem(obj, "type");
    if (cJSON_IsString(j)) e->type = ev_type_from_str(j->valuestring);

    j = cJSON_GetObjectItem(obj, "title");
    if (cJSON_IsString(j)) {
        strncpy(e->title, j->valuestring, EVENT_TITLE_LEN - 1);
        e->title[EVENT_TITLE_LEN - 1] = '\0';
    }

    j = cJSON_GetObjectItem(obj, "year");   if (cJSON_IsNumber(j)) e->year   = j->valueint;
    j = cJSON_GetObjectItem(obj, "month");  if (cJSON_IsNumber(j)) e->month  = j->valueint;
    j = cJSON_GetObjectItem(obj, "day");    if (cJSON_IsNumber(j)) e->day    = j->valueint;
    j = cJSON_GetObjectItem(obj, "hour");   if (cJSON_IsNumber(j)) e->hour   = j->valueint;
    j = cJSON_GetObjectItem(obj, "minute"); if (cJSON_IsNumber(j)) e->minute = j->valueint;

    j = cJSON_GetObjectItem(obj, "recurrence");
    if (cJSON_IsString(j)) e->recurrence = ev_rec_from_str(j->valuestring);

    j = cJSON_GetObjectItem(obj, "enabled");
    if (cJSON_IsBool(j)) e->enabled = cJSON_IsTrue(j);

    j = cJSON_GetObjectItem(obj, "station");
    if (cJSON_IsNumber(j)) e->station = j->valueint;

    j = cJSON_GetObjectItem(obj, "volume");
    if (cJSON_IsNumber(j)) e->volume = j->valueint;

    j = cJSON_GetObjectItem(obj, "sound");
    if (cJSON_IsString(j)) {
        strncpy(e->sound, j->valuestring, EVENT_SOUND_LEN - 1);
        e->sound[EVENT_SOUND_LEN - 1] = '\0';
    }
}

// Validates field by field. Returns NULL if ok, otherwise an error message.
static const char *event_validate(const event_t *e)
{
    if (e->title[0] == '\0')                                 return "title empty";
    if (e->year < 1970 || e->year > 2100)                    return "year out of range";
    if (e->month < 1 || e->month > 12)                       return "month out of range";
    if (e->day < 1 || e->day > 31)                           return "day out of range";
    if (e->hour < 0   || e->hour > 23)                       return "hour out of range";
    if (e->minute < 0 || e->minute > 59)                     return "minute out of range";
    if (e->type < 0 || e->type >= EV_TYPE_COUNT)             return "type invalid";
    // Playback from the playlist (empty sound) needs a valid station; an SD
    // path is validated lazily at fire time (the card may be absent now).
    if (e->type == EV_SCHEDULE && e->sound[0] == '\0') {
        int n = playlist_get_count();
        if (n <= 0)                            return "playlist empty";
        if (e->station < 0 || e->station >= n) return "station out of range";
    }
    if (e->type == EV_SCHEDULE || e->type == EV_VOICE) {
        if (e->volume < 0 || e->volume > 100)  return "volume out of range";
    }
    return NULL;
}

// Reads POST/PUT body into a freshly allocated buffer (caller: free()).
static esp_err_t read_body(httpd_req_t *req, char **out_buf, int max_len)
{
    int total = req->content_len;
    if (total <= 0 || total > max_len) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad content length");
        return ESP_FAIL;
    }
    char *buf = malloc(total + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    int received = 0;
    while (received < total) {
        int r = httpd_req_recv(req, buf + received, total - received);
        if (r <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        received += r;
    }
    buf[received] = '\0';
    *out_buf = buf;
    return ESP_OK;
}

// Extracts id from a URI like /api/events/<id>[?...]. Returns false if empty.
static bool extract_event_id(const char *uri, char *out, size_t out_sz)
{
    const char *prefix = "/api/events/";
    const char *p = strstr(uri, prefix);
    if (!p) return false;
    p += strlen(prefix);
    size_t i = 0;
    while (*p && *p != '/' && *p != '?' && i < out_sz - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return i > 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/events  — returns array of all events
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_events_get_handler(httpd_req_t *req)
{
    // Heap instead of static — 50 × sizeof(event_t) is ~5KB, goes to PSRAM
    // automatically (threshold SPIRAM_MALLOC_ALWAYSINTERNAL=4096).
    // Static in .bss was wasting internal RAM unnecessarily.
    event_t *tmp = malloc(sizeof(event_t) * EVENTS_MAX);
    if (!tmp) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int n = events_get_all(tmp, EVENTS_MAX);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < n; ++i) {
        cJSON_AddItemToArray(arr, event_to_json(&tmp[i]));
    }
    free(tmp);

    char *str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/events  — body: event JSON (id optional). Returns the created object.
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_events_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (read_body(req, &buf, 2048) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    event_t e = {0};
    e.enabled = true;           // default on
    e.recurrence = EV_REC_NONE;
    event_patch_from_json(&e, json);

    // optional id
    cJSON *id = cJSON_GetObjectItem(json, "id");
    if (cJSON_IsString(id)) {
        strncpy(e.id, id->valuestring, EVENT_ID_LEN - 1);
        e.id[EVENT_ID_LEN - 1] = '\0';
    }
    cJSON_Delete(json);

    const char *err = event_validate(&e);
    if (err) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
        return ESP_FAIL;
    }

    esp_err_t rc = events_add(&e);
    if (rc != ESP_OK) {
        httpd_resp_send_err(req,
            rc == ESP_ERR_NO_MEM ? HTTPD_400_BAD_REQUEST : HTTPD_500_INTERNAL_SERVER_ERROR,
            rc == ESP_ERR_NO_MEM ? "Event limit reached" : "Add failed");
        return ESP_FAIL;
    }

    // Refresh event_indicator on the active screen without waiting for its
    // 30 s timer or a screen switch.
    ui_event_t st = { .type = UI_EVT_STATE_CHANGED };
    ui_event_send(&st);

    cJSON *out = event_to_json(&e);
    char *str = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// PUT /api/events/{id}  — body: partial or full event JSON
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_events_put_handler(httpd_req_t *req)
{
    char id[EVENT_ID_LEN];
    if (!extract_event_id(req->uri, id, sizeof(id))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id");
        return ESP_FAIL;
    }

    const event_t *cur = events_find(id);
    if (!cur) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Event not found");
        return ESP_FAIL;
    }

    event_t updated = *cur;         // start from current state → patch

    char *buf = NULL;
    if (read_body(req, &buf, 2048) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    event_patch_from_json(&updated, json);
    cJSON_Delete(json);

    const char *err = event_validate(&updated);
    if (err) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err);
        return ESP_FAIL;
    }

    if (events_update(id, &updated) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Update failed");
        return ESP_FAIL;
    }

    ui_event_t st = { .type = UI_EVT_STATE_CHANGED };
    ui_event_send(&st);

    cJSON *out = event_to_json(&updated);
    char *str = cJSON_PrintUnformatted(out);
    cJSON_Delete(out);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// DELETE /api/events/{id}
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_events_delete_handler(httpd_req_t *req)
{
    char id[EVENT_ID_LEN];
    if (!extract_event_id(req->uri, id, sizeof(id))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing id");
        return ESP_FAIL;
    }

    esp_err_t rc = events_remove(id);
    if (rc == ESP_ERR_NOT_FOUND) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Event not found");
        return ESP_FAIL;
    }
    if (rc != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
        return ESP_FAIL;
    }

    ui_event_t st = { .type = UI_EVT_STATE_CHANGED };
    ui_event_send(&st);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// UI PROFILE / LAYOUT EDITOR
// ─────────────────────────────────────────────────────────────────────────────
// GET  /api/ui/profile/meta   — screen dimensions + available fonts
// GET  /api/ui/profile/clock  — JSON with fields of the "clock" section
// POST /api/ui/profile/clock  — patch + save + rebuild active screen
// GET  /api/ui/profile/bt     — JSON with fields of the "bt" section
// POST /api/ui/profile/bt     — patch + save + rebuild active screen
// POST /api/ui/profile/reset  — full reset to compile-time defaults

static esp_err_t api_ui_profile_meta_get_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "screen_w", DISPLAY_WIDTH);
    cJSON_AddNumberToObject(json, "screen_h", DISPLAY_HEIGHT);

    cJSON *fonts = cJSON_CreateArray();
    int n = ui_font_list_count();
    for (int i = 0; i < n; ++i) {
        const char *id = ui_font_list_id(i);
        if (id) cJSON_AddItemToArray(fonts, cJSON_CreateString(id));
    }
    cJSON_AddItemToObject(json, "fonts", fonts);

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static esp_err_t api_ui_profile_clock_get_handler(httpd_req_t *req)
{
    cJSON *clock = (cJSON *)ui_profile_dump_clock();

    char *str = cJSON_PrintUnformatted(clock);
    cJSON_Delete(clock);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static esp_err_t api_ui_profile_clock_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (read_body(req, &buf, 4096) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    ui_profile_patch_clock(json);
    cJSON_Delete(json);

    ui_profile_save_to_file();

    // Rebuild active screen — done on lvgl_task via the event queue
    ui_event_t ev = { .type = UI_EVT_PROFILE_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_ui_profile_bt_get_handler(httpd_req_t *req)
{
    cJSON *bt = (cJSON *)ui_profile_dump_bt();

    char *str = cJSON_PrintUnformatted(bt);
    cJSON_Delete(bt);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static esp_err_t api_ui_profile_bt_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (read_body(req, &buf, 4096) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    ui_profile_patch_bt(json);
    cJSON_Delete(json);

    ui_profile_save_to_file();

    ui_event_t ev = { .type = UI_EVT_PROFILE_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_ui_profile_radio_get_handler(httpd_req_t *req)
{
    cJSON *radio = (cJSON *)ui_profile_dump_radio();

    char *str = cJSON_PrintUnformatted(radio);
    cJSON_Delete(radio);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static esp_err_t api_ui_profile_radio_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (read_body(req, &buf, 4096) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    ui_profile_patch_radio(json);
    cJSON_Delete(json);

    ui_profile_save_to_file();

    ui_event_t ev = { .type = UI_EVT_PROFILE_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_ui_profile_sd_get_handler(httpd_req_t *req)
{
    cJSON *sd = (cJSON *)ui_profile_dump_sd();

    char *str = cJSON_PrintUnformatted(sd);
    cJSON_Delete(sd);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static esp_err_t api_ui_profile_sd_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (read_body(req, &buf, 4096) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    ui_profile_patch_sd(json);
    cJSON_Delete(json);

    ui_profile_save_to_file();

    ui_event_t ev = { .type = UI_EVT_PROFILE_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

static esp_err_t api_ui_profile_reset_handler(httpd_req_t *req)
{
    ui_profile_reset();
    ui_profile_save_to_file();

    ui_event_t ev = { .type = UI_EVT_PROFILE_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT — GET/POST /api/mqtt — broker config + widget array (max 6)
// Stored in /config/mqtt.json by mqtt_config_*.
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_mqtt_get_handler(httpd_req_t *req)
{
    mqtt_config_t *c = mqtt_config_get();

    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject  (json, "enabled",    c->enabled);
    cJSON_AddStringToObject(json, "host",       c->host);
    cJSON_AddNumberToObject(json, "port",       c->port);
    cJSON_AddStringToObject(json, "username",   c->username);
    cJSON_AddStringToObject(json, "password",   "");  // never echo
    cJSON_AddStringToObject(json, "client_id",  c->client_id);
    cJSON_AddStringToObject(json, "base_topic", c->base_topic);

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
        mqtt_widget_t *w = &c->widgets[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "type",        mqtt_widget_type_name(w->type));
        cJSON_AddStringToObject(o, "title",       w->title);
        cJSON_AddStringToObject(o, "topic_cmd",   w->topic_cmd);
        cJSON_AddStringToObject(o, "topic_state", w->topic_state);
        cJSON_AddStringToObject(o, "json_path",   w->json_path);
        cJSON_AddStringToObject(o, "unit",        w->unit);
        cJSON_AddNumberToObject(o, "min",         w->min);
        cJSON_AddNumberToObject(o, "max",         w->max);
        cJSON_AddNumberToObject(o, "step",        w->step);
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(json, "widgets", arr);

    cJSON *ss = cJSON_CreateObject();
    cJSON_AddStringToObject(ss, "title",       c->screensaver.title);
    cJSON_AddStringToObject(ss, "topic_state", c->screensaver.topic_state);
    cJSON_AddStringToObject(ss, "json_path",   c->screensaver.json_path);
    cJSON_AddStringToObject(ss, "unit",        c->screensaver.unit);
    cJSON_AddItemToObject(json, "screensaver", ss);

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static void copy_str_field(char *dst, size_t dst_sz, const char *src)
{
    dst[0] = '\0';
    if (src) strncpy(dst, src, dst_sz - 1);
}

static esp_err_t api_mqtt_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    if (read_body(req, &buf, 16384) != ESP_OK) return ESP_FAIL;

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    mqtt_config_t *c = mqtt_config_get();
    cJSON *j;
    j = cJSON_GetObjectItem(json, "enabled");    if (cJSON_IsBool(j))   c->enabled = cJSON_IsTrue(j);
    j = cJSON_GetObjectItem(json, "host");       if (cJSON_IsString(j)) copy_str_field(c->host,       sizeof(c->host),       j->valuestring);
    j = cJSON_GetObjectItem(json, "port");       if (cJSON_IsNumber(j)) c->port = j->valueint;
    j = cJSON_GetObjectItem(json, "username");   if (cJSON_IsString(j)) copy_str_field(c->username,   sizeof(c->username),   j->valuestring);
    // password: empty from client = keep the old one (mirrors wifi behavior)
    j = cJSON_GetObjectItem(json, "password");
    if (cJSON_IsString(j) && j->valuestring[0] != '\0') {
        copy_str_field(c->password, sizeof(c->password), j->valuestring);
    }
    j = cJSON_GetObjectItem(json, "client_id");  if (cJSON_IsString(j)) copy_str_field(c->client_id,  sizeof(c->client_id),  j->valuestring);
    j = cJSON_GetObjectItem(json, "base_topic"); if (cJSON_IsString(j)) copy_str_field(c->base_topic, sizeof(c->base_topic), j->valuestring);

    cJSON *arr = cJSON_GetObjectItem(json, "widgets");
    if (cJSON_IsArray(arr)) {
        int n = cJSON_GetArraySize(arr);
        if (n > MQTT_MAX_WIDGETS) n = MQTT_MAX_WIDGETS;
        // Reset all slots first so a shorter list clears removed widgets
        for (int i = 0; i < MQTT_MAX_WIDGETS; ++i) {
            memset(&c->widgets[i], 0, sizeof(c->widgets[i]));
            c->widgets[i].type = MQTT_W_NONE;
            c->widgets[i].min  = 0; c->widgets[i].max = 100; c->widgets[i].step = 1;
        }
        for (int i = 0; i < n; ++i) {
            cJSON *o = cJSON_GetArrayItem(arr, i);
            if (!cJSON_IsObject(o)) continue;
            mqtt_widget_t *w = &c->widgets[i];
            cJSON *k;
            k = cJSON_GetObjectItem(o, "type");        if (cJSON_IsString(k)) w->type = mqtt_widget_type_from_name(k->valuestring);
            k = cJSON_GetObjectItem(o, "title");       if (cJSON_IsString(k)) copy_str_field(w->title,       sizeof(w->title),       k->valuestring);
            k = cJSON_GetObjectItem(o, "topic_cmd");   if (cJSON_IsString(k)) copy_str_field(w->topic_cmd,   sizeof(w->topic_cmd),   k->valuestring);
            k = cJSON_GetObjectItem(o, "topic_state"); if (cJSON_IsString(k)) copy_str_field(w->topic_state, sizeof(w->topic_state), k->valuestring);
            k = cJSON_GetObjectItem(o, "json_path");   if (cJSON_IsString(k)) copy_str_field(w->json_path,   sizeof(w->json_path),   k->valuestring);
            k = cJSON_GetObjectItem(o, "unit");        if (cJSON_IsString(k)) copy_str_field(w->unit,        sizeof(w->unit),        k->valuestring);
            k = cJSON_GetObjectItem(o, "min");         if (cJSON_IsNumber(k)) w->min  = k->valueint;
            k = cJSON_GetObjectItem(o, "max");         if (cJSON_IsNumber(k)) w->max  = k->valueint;
            k = cJSON_GetObjectItem(o, "step");        if (cJSON_IsNumber(k)) w->step = k->valueint;
            if (w->step < 1) w->step = 1;
        }
    }

    cJSON *ss = cJSON_GetObjectItem(json, "screensaver");
    if (cJSON_IsObject(ss)) {
        cJSON *k;
        memset(&c->screensaver, 0, sizeof(c->screensaver));
        k = cJSON_GetObjectItem(ss, "title");       if (cJSON_IsString(k)) copy_str_field(c->screensaver.title,       sizeof(c->screensaver.title),       k->valuestring);
        k = cJSON_GetObjectItem(ss, "topic_state"); if (cJSON_IsString(k)) copy_str_field(c->screensaver.topic_state, sizeof(c->screensaver.topic_state), k->valuestring);
        k = cJSON_GetObjectItem(ss, "json_path");   if (cJSON_IsString(k)) copy_str_field(c->screensaver.json_path,   sizeof(c->screensaver.json_path),   k->valuestring);
        k = cJSON_GetObjectItem(ss, "unit");        if (cJSON_IsString(k)) copy_str_field(c->screensaver.unit,        sizeof(c->screensaver.unit),        k->valuestring);
    }

    cJSON_Delete(json);

    mqtt_config_save();
    mqtt_svc_reconfigure();

    // Rebuild active screen so MQTT widget changes are visible immediately
    ui_event_t ev = { .type = UI_EVT_PROFILE_CHANGED };
    ui_event_send(&ev);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// FILE EDITOR — list + save raw text into /spiffs (html/css/js → .gz, else raw)
// GET /api/files            — JSON [{name, size, gz}] of editable files
// PUT /api/files/<name>     — body = plain text; server gzips when applicable
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t gzip_buffer(const char *src, size_t src_len,
                             uint8_t **out_buf, size_t *out_len)
{
    z_stream s = {0};
    // windowBits = 15 + 16 → gzip wrapper (matches tools/compress_web.py output)
    if (deflateInit2(&s, Z_BEST_COMPRESSION, Z_DEFLATED,
                     15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return ESP_FAIL;
    }
    unsigned long bound = deflateBound(&s, src_len);
    uint8_t *buf = malloc(bound);
    if (!buf) {
        deflateEnd(&s);
        return ESP_ERR_NO_MEM;
    }
    s.next_in   = (Bytef *)src;
    s.avail_in  = src_len;
    s.next_out  = buf;
    s.avail_out = bound;
    int r = deflate(&s, Z_FINISH);
    if (r != Z_STREAM_END) {
        free(buf);
        deflateEnd(&s);
        return ESP_FAIL;
    }
    *out_len = s.total_out;
    *out_buf = buf;
    deflateEnd(&s);
    return ESP_OK;
}

// Resolve ?root=config → /config (user settings), default → /spiffs (www).
static const char *files_root_from_query(httpd_req_t *req)
{
    const char *root = WEB_ROOT;
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen > 1 && qlen < 256) {
        char *q = malloc(qlen);
        if (q && httpd_req_get_url_query_str(req, q, qlen) == ESP_OK) {
            char val[16];
            if (httpd_query_key_value(q, "root", val, sizeof(val)) == ESP_OK &&
                strcmp(val, "config") == 0) {
                root = CONFIG_ROOT;
            }
        }
        free(q);
    }
    return root;
}

static esp_err_t api_files_get_handler(httpd_req_t *req)
{
    const char *root = files_root_from_query(req);
    DIR *d = opendir(root);
    if (!d) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "opendir failed");
        return ESP_FAIL;
    }

    cJSON *arr = cJSON_CreateArray();
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;

        const char *fname = e->d_name;
        size_t nlen = strlen(fname);
        // skip tmp leftovers from atomic writes
        if (nlen > 4 && strcmp(fname + nlen - 4, ".tmp") == 0) continue;

        char display[160];
        bool is_gz = false;
        if (nlen > 3 && strcmp(fname + nlen - 3, ".gz") == 0) {
            is_gz = true;
            if (nlen - 3 >= sizeof(display)) continue;
            memcpy(display, fname, nlen - 3);
            display[nlen - 3] = '\0';
        } else {
            if (nlen >= sizeof(display)) continue;
            strcpy(display, fname);
        }

        // binary formats not editable as text
        const char *ext = strrchr(display, '.');
        if (ext && strcmp(ext, ".ico") == 0) continue;

        char fullpath[320];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", root, fname);
        struct stat st = {0};
        long sz = (stat(fullpath, &st) == 0) ? (long)st.st_size : 0;

        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", display);
        cJSON_AddNumberToObject(o, "size", sz);
        cJSON_AddBoolToObject  (o, "gz",   is_gz);
        cJSON_AddItemToArray(arr, o);
    }
    closedir(d);

    char *str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// GET /api/spiffs  → { total, used, free } bytes of the SPIFFS partition
static esp_err_t api_spiffs_get_handler(httpd_req_t *req)
{
    size_t total = 0, used = 0;
    esp_spiffs_info("www", &total, &used);

    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "total", (double)total);
    cJSON_AddNumberToObject(o, "used",  (double)used);
    cJSON_AddNumberToObject(o, "free",  (double)(total - used));
    char *str = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

static esp_err_t api_files_put_handler(httpd_req_t *req)
{
    const char *prefix = "/api/files/";
    const char *p = strstr(req->uri, prefix);
    if (!p) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad URI");
        return ESP_FAIL;
    }
    p += strlen(prefix);
    if (*p == '\0' || *p == '/') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad name");
        return ESP_FAIL;
    }

    // URL-decode percent escapes (slashes for subpaths come through literally)
    char name[160];
    size_t ni = 0;
    while (*p && *p != '?' && ni < sizeof(name) - 1) {
        if (*p == '%' && p[1] && p[2]) {
            char hex[3] = { p[1], p[2], 0 };
            name[ni++] = (char)strtol(hex, NULL, 16);
            p += 3;
        } else {
            name[ni++] = *p++;
        }
    }
    name[ni] = '\0';

    if (strstr(name, "..") || name[0] == '/') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad name");
        return ESP_FAIL;
    }

    const char *ext = strrchr(name, '.');
    if (!ext) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No extension");
        return ESP_FAIL;
    }
    if (strcmp(ext, ".ico") == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Binary not supported");
        return ESP_FAIL;
    }

    const char *root = files_root_from_query(req);
    // config holds raw JSON only — never gzip, even for html/css/js names
    bool is_config = (strcmp(root, CONFIG_ROOT) == 0);
    bool do_gzip = !is_config &&
                   (strcmp(ext, ".html") == 0 ||
                    strcmp(ext, ".css")  == 0 ||
                    strcmp(ext, ".js")   == 0);

    int total = req->content_len;
    if (total < 0 || total > 65536) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad content length");
        return ESP_FAIL;
    }
    char *body = malloc((size_t)total + 1);
    if (!body) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    int recv = 0;
    while (recv < total) {
        int r = httpd_req_recv(req, body + recv, total - recv);
        if (r <= 0) {
            free(body);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        recv += r;
    }
    body[total] = '\0';

    char dest_path[256];
    if (do_gzip)
        snprintf(dest_path, sizeof(dest_path), "%s/%s.gz", root, name);
    else
        snprintf(dest_path, sizeof(dest_path), "%s/%s",    root, name);

    char tmp_path[260];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", dest_path);

    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        free(body);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open tmp");
        return ESP_FAIL;
    }

    size_t bytes_written = 0;
    esp_err_t rc = ESP_OK;
    if (do_gzip) {
        uint8_t *gz = NULL;
        size_t gz_len = 0;
        rc = gzip_buffer(body, (size_t)total, &gz, &gz_len);
        free(body);
        if (rc == ESP_OK) {
            bytes_written = fwrite(gz, 1, gz_len, f);
            free(gz);
            if (bytes_written != gz_len) rc = ESP_FAIL;
        }
    } else {
        bytes_written = fwrite(body, 1, (size_t)total, f);
        free(body);
        if (bytes_written != (size_t)total) rc = ESP_FAIL;
    }
    fclose(f);

    if (rc != ESP_OK) {
        remove(tmp_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
        return ESP_FAIL;
    }

    // SPIFFS rename onto existing target fails — remove first
    remove(dest_path);
    if (rename(tmp_path, dest_path) != 0) {
        ESP_LOGE("HTTP", "rename %s → %s failed", tmp_path, dest_path);
        remove(tmp_path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rename failed");
        return ESP_FAIL;
    }

    ESP_LOGI("HTTP", "Saved %s (%zu bytes%s)", dest_path, bytes_written,
             do_gzip ? ", gzipped" : "");

    cJSON *o = cJSON_CreateObject();
    cJSON_AddBoolToObject  (o, "ok",   true);
    cJSON_AddStringToObject(o, "name", name);
    cJSON_AddNumberToObject(o, "size", (double)bytes_written);
    cJSON_AddBoolToObject  (o, "gz",   do_gzip);
    char *str = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// SD card file manager — /api/sd/*
// Browse, upload, download and delete files on the FAT-mounted SD card. Every
// handler refuses with 503 when no card is mounted. The target path arrives in
// the `path` query param, is URL-decoded, rejected on ".." traversal and
// resolved under SD_MOUNT_POINT.
// ─────────────────────────────────────────────────────────────────────────────
#define SD_RECV_BUF_SIZE 4096

// esp_http_server has no HTTPD_503 enum — send the status text directly.
static esp_err_t sd_send_no_card(httpd_req_t *req)
{
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"error\":\"no_sd_card\"}");
    return ESP_FAIL;
}

// Percent-decode src into dst (NUL-terminated, capped at dstlen).
static void sd_url_decode(const char *src, char *dst, size_t dstlen)
{
    size_t di = 0;
    while (*src && di < dstlen - 1) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = { src[1], src[2], 0 };
            dst[di++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[di++] = ' ';
            src++;
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

// Read the `path` query param, validate it and build the absolute SD path into
// `out`. Returns false (and sends an HTTP error) on a missing card, missing/bad
// path or a traversal attempt. `def` is used when no path param is present.
static bool sd_resolve_path(httpd_req_t *req, const char *def, char *out, size_t outlen)
{
    if (sdcard_init() != ESP_OK) {   // lazy mount on first SD use
        sd_send_no_card(req);
        return false;
    }

    char rel[192];
    rel[0] = '\0';
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen > 1 && qlen < 512) {
        char *q = malloc(qlen);
        if (q && httpd_req_get_url_query_str(req, q, qlen) == ESP_OK) {
            char enc[192];
            if (httpd_query_key_value(q, "path", enc, sizeof(enc)) == ESP_OK) {
                sd_url_decode(enc, rel, sizeof(rel));
            }
        }
        free(q);
    }
    if (rel[0] == '\0' && def) {
        strncpy(rel, def, sizeof(rel) - 1);
        rel[sizeof(rel) - 1] = '\0';
    }
    if (rel[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing path");
        return false;
    }
    if (strstr(rel, "..")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad path");
        return false;
    }

    const char *r = rel;
    while (*r == '/') r++;   // collapse leading slashes
    int n = snprintf(out, outlen, "%s/%s", SD_MOUNT_POINT, r);
    if (n <= 0 || (size_t)n >= outlen) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Path too long");
        return false;
    }
    // strip trailing slash (keep the mount root intact)
    size_t L = strlen(out);
    while (L > strlen(SD_MOUNT_POINT) + 1 && out[L - 1] == '/') out[--L] = '\0';
    return true;
}

// GET /api/sd/info  → { total, used, free } bytes of the SD card (503 if absent)
static esp_err_t api_sd_info_handler(httpd_req_t *req)
{
    if (sdcard_init() != ESP_OK) return sd_send_no_card(req);   // lazy mount

    uint64_t total = 0, freeb = 0;
    if (esp_vfs_fat_info(SD_MOUNT_POINT, &total, &freeb) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fat_info failed");
        return ESP_FAIL;
    }

    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "total", (double)total);
    cJSON_AddNumberToObject(o, "used",  (double)(total - freeb));
    cJSON_AddNumberToObject(o, "free",  (double)freeb);
    char *str = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// GET /api/sd/list?path=/dir  → { path, entries:[{name,dir,size}] }
static esp_err_t api_sd_list_handler(httpd_req_t *req)
{
    char dirpath[256];
    if (!sd_resolve_path(req, "/", dirpath, sizeof(dirpath))) return ESP_FAIL;

    DIR *d = opendir(dirpath);
    if (!d) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not a directory");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", dirpath + strlen(SD_MOUNT_POINT));
    cJSON *arr = cJSON_AddArrayToObject(root, "entries");

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        // Sized for the worst case (dirpath + '/' + a max-length LFN name) so
        // -Werror=format-truncation can prove no truncation.
        char full[sizeof(dirpath) + 258];
        snprintf(full, sizeof(full), "%s/%s", dirpath, e->d_name);
        struct stat st = {0};
        bool is_dir = false;
        long sz = 0;
        if (stat(full, &st) == 0) {
            is_dir = S_ISDIR(st.st_mode);
            sz = (long)st.st_size;
        }
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "name", e->d_name);
        cJSON_AddBoolToObject  (o, "dir",  is_dir);
        cJSON_AddNumberToObject(o, "size", is_dir ? 0 : sz);
        cJSON_AddItemToArray(arr, o);
    }
    closedir(d);

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr(req, str);
    free(str);
    return ESP_OK;
}

// GET /api/sd/file?path=/dir/name  → streams the file as an attachment
static esp_err_t api_sd_get_handler(httpd_req_t *req)
{
    char path[256];
    if (!sd_resolve_path(req, NULL, path, sizeof(path))) return ESP_FAIL;

    struct stat st = {0};
    if (stat(path, &st) != 0 || S_ISDIR(st.st_mode)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not a file");
        return ESP_FAIL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Open failed");
        return ESP_FAIL;
    }

    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char disp[sizeof(path) + 40];   // header text + a full-length filename
    snprintf(disp, sizeof(disp), "attachment; filename=\"%s\"", base);
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", disp);

    char *buf = malloc(SD_RECV_BUF_SIZE);
    if (!buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }
    size_t n;
    esp_err_t ret = ESP_OK;
    while ((n = fread(buf, 1, SD_RECV_BUF_SIZE, f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, n) != ESP_OK) { ret = ESP_FAIL; break; }
    }
    free(buf);
    fclose(f);
    if (ret == ESP_OK) httpd_resp_send_chunk(req, NULL, 0);
    return ret;
}

// Create every intermediate directory in `path` up to (but not including) the
// final component, so an upload to /voice/<id>/<id>.wav lands in folders that may
// not exist yet. Walks each '/' after the mount point and mkdir()s the prefix,
// ignoring failures — fopen() reports the real error afterwards. `path` is
// modified in place and restored before returning.
static void sd_mkdir_parents(char *path)
{
    size_t root = strlen(SD_MOUNT_POINT);
    if (strlen(path) <= root + 1) return;
    for (char *p = path + root + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        mkdir(path, 0777);   // ignore EEXIST and other errors
        *p = '/';
    }
}

// Recursively delete a directory and its contents (depth-first), or a plain file.
// Lets the DELETE handler remove a whole event folder (e.g. /voice/<id>) in one
// call. Returns 0 on success. Tree depth is shallow in practice.
static int sd_rm_rf(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) != 0) return -1;
    if (!S_ISDIR(st.st_mode)) return remove(path);

    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *e;
    int rc = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        char child[512];
        snprintf(child, sizeof(child), "%s/%s", path, e->d_name);
        if (sd_rm_rf(child) != 0) rc = -1;
    }
    closedir(d);
    if (rc == 0) rc = rmdir(path);
    return rc;
}

// POST /api/sd/file?path=/dir/name  → streams the body into the file (overwrite)
static esp_err_t api_sd_post_handler(httpd_req_t *req)
{
    char path[256];
    if (!sd_resolve_path(req, NULL, path, sizeof(path))) return ESP_FAIL;

    // Auto-create any missing parent directories so uploads to a fresh folder
    // (e.g. /voice/<id>/<id>.wav) just work without separate mkdir steps.
    sd_mkdir_parents(path);

    FILE *f = fopen(path, "wb");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot create file");
        return ESP_FAIL;
    }

    char *buf = malloc(SD_RECV_BUF_SIZE);
    if (!buf) {
        fclose(f);
        remove(path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    int total = req->content_len;
    int received = 0;
    esp_err_t ret = ESP_OK;
    while (received < total) {
        int r = httpd_req_recv(req, buf, SD_RECV_BUF_SIZE);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (r <= 0) { ret = ESP_FAIL; break; }
        if (fwrite(buf, 1, r, f) != (size_t)r) { ret = ESP_FAIL; break; }
        received += r;
    }
    free(buf);
    fclose(f);

    if (ret != ESP_OK) {
        remove(path);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
        return ESP_FAIL;
    }

    ESP_LOGI("HTTP", "SD upload %s (%d bytes)", path, received);
    char body[48];
    snprintf(body, sizeof(body), "{\"ok\":true,\"size\":%d}", received);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, body);
    return ESP_OK;
}

// DELETE /api/sd/file?path=/dir/name  → removes a file or empty directory
static esp_err_t api_sd_delete_handler(httpd_req_t *req)
{
    char path[256];
    if (!sd_resolve_path(req, NULL, path, sizeof(path))) return ESP_FAIL;

    // Never let a delete reach the mount root — recursive folder delete below
    // would otherwise wipe the whole card.
    if (strlen(path) <= strlen(SD_MOUNT_POINT) + 1) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Refusing to delete root");
        return ESP_FAIL;
    }

    struct stat st = {0};
    if (stat(path, &st) != 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
        return ESP_FAIL;
    }
    // Directories are removed recursively (one call clears a whole event folder).
    int rc = S_ISDIR(st.st_mode) ? sd_rm_rf(path) : remove(path);
    if (rc != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Delete failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// POST /api/sd/mkdir?path=/dir  → creates a directory (no-op if it exists)
static esp_err_t api_sd_mkdir_handler(httpd_req_t *req)
{
    char path[256];
    if (!sd_resolve_path(req, NULL, path, sizeof(path))) return ESP_FAIL;

    if (mkdir(path, 0777) != 0 && errno != EEXIST) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "mkdir failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// POST /api/sd/rename?path=/dir/old&to=new  → renames within the same directory
static esp_err_t api_sd_rename_handler(httpd_req_t *req)
{
    char src[256];
    if (!sd_resolve_path(req, NULL, src, sizeof(src))) return ESP_FAIL;

    // `to` is a bare new name (no path) — the file stays in the same directory.
    char to[160];
    to[0] = '\0';
    size_t qlen = httpd_req_get_url_query_len(req) + 1;
    if (qlen > 1 && qlen < 512) {
        char *q = malloc(qlen);
        if (q && httpd_req_get_url_query_str(req, q, qlen) == ESP_OK) {
            char enc[160];
            if (httpd_query_key_value(q, "to", enc, sizeof(enc)) == ESP_OK) {
                sd_url_decode(enc, to, sizeof(to));
            }
        }
        free(q);
    }
    if (to[0] == '\0' || strchr(to, '/') || strstr(to, "..")) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad target name");
        return ESP_FAIL;
    }

    // dest = parent directory of src + '/' + to
    char *slash = strrchr(src, '/');
    int dirlen = slash ? (int)(slash - src) : (int)strlen(src);
    char dest[sizeof(src) + sizeof(to)];
    snprintf(dest, sizeof(dest), "%.*s/%s", dirlen, src, to);

    if (rename(src, dest) != 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Rename failed");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");
    return ESP_OK;
}

// Built-in setup page (Wi-Fi + UI upload), embedded in the app binary so a fresh
// or wiped www partition can still be provisioned. See components/web/setup.html.
extern const uint8_t setup_html_start[] asm("_binary_setup_html_start");
extern const uint8_t setup_html_end[]   asm("_binary_setup_html_end");

static esp_err_t serve_embedded_setup(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)setup_html_start,
                           setup_html_end - setup_html_start);
}

// ─────────────────────────────────────────────────────────────────────────────
// Wildcard — serve files from the www partition (/spiffs), falling back to the
// config partition (/config) for the settings JSON the browser fetches.
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t file_handler(httpd_req_t *req)
{
    char filepath[300];
    char gz_filepath[320];
    char relpath[256];

    // Resolve the request to a path relative to the filesystem roots.
    bool is_entry = false;
    if (strcmp(req->uri, "/") == 0) {
        is_entry = true;
        strcpy(relpath, wifi_get_run_mode() == WIFI_RUN_MODE_AP
                            ? "/wifi_setup.html" : "/index.html");
    } else {
        if (strlen(req->uri) >= sizeof(relpath)) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
            return ESP_FAIL;
        }
        strcpy(relpath, req->uri);
        is_entry = (strcmp(req->uri, "/index.html") == 0) ||
                   (strcmp(req->uri, "/wifi_setup.html") == 0);
    }

    // Strip query string — static-file paths must not include "?..." parts,
    // otherwise fopen() fails on e.g. /settings.html?tab=screensaver
    // when a deep link is refreshed.
    char *q = strchr(relpath, '?');
    if (q) *q = '\0';

    // Content-Type based on the original extension (before adding .gz)
    const char *content_type = "text/plain";
    if      (strstr(relpath, ".html")) content_type = "text/html; charset=utf-8";
    else if (strstr(relpath, ".css"))  content_type = "text/css";
    else if (strstr(relpath, ".json")) content_type = "application/json"; // must come before js
    else if (strstr(relpath, ".js"))   content_type = "application/javascript";
    else if (strstr(relpath, ".ico"))  content_type = "image/x-icon";

    // Look under the www root first, then the config root (settings JSON lives on
    // /config but is fetched by the browser through this handler).
    static const char *const roots[] = { WEB_ROOT, CONFIG_ROOT };
    bool use_gz = false;
    FILE *f = NULL;
    for (int i = 0; i < 2 && !f; i++) {
        snprintf(gz_filepath, sizeof(gz_filepath), "%s%s.gz", roots[i], relpath);
        f = fopen(gz_filepath, "rb");
        if (f) { use_gz = true; break; }
        snprintf(filepath, sizeof(filepath), "%s%s", roots[i], relpath);
        f = fopen(filepath, "rb");
    }

    if (!f) {
        // Fresh/empty www partition: still serve the built-in setup page on the
        // entry routes so Wi-Fi can be configured and the UI re-uploaded.
        if (is_entry) {
            ESP_LOGW("HTTP", "%s missing — serving embedded setup page", relpath);
            return serve_embedded_setup(req);
        }
        ESP_LOGW("HTTP", "File not found: %s", relpath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);

    if (use_gz) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    // Cache:
    //  - HTML: no-cache (entry point — must refresh after firmware update)
    //  - mutable data (/data/*, *.json configs, the playlist *.csv): no-cache —
    //    these change at runtime (playlist, settings, events). Long-caching them
    //    desynchronizes the UI from the device, e.g. a stale playlist.csv plays
    //    the wrong index.
    //  - rest (CSS/JS/ICO/fonts): long cache so the browser doesn't keep hitting
    //    the ESP on every tab open. Critical while radio is playing —
    //    parallel asset fetching + TLS audio caused stream drops.
    bool is_html    = (strstr(relpath, ".html") != NULL);
    bool is_mutable = (strstr(relpath, "/data/") != NULL) ||
                      (strstr(relpath, ".json")  != NULL) ||
                      (strstr(relpath, ".csv")   != NULL);
    if (is_html || is_mutable) {
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    } else {
        httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=86400");
    }

    char *buffer = malloc(4096);
    if (!buffer) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    size_t read_bytes;
    esp_err_t ret = ESP_OK;

    while ((read_bytes = fread(buffer, 1, 4096, f)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, read_bytes) != ESP_OK) {
            ESP_LOGW("HTTP", "Chunk send failed: %s", filepath);
            ret = ESP_FAIL;
            break;
        }
    }

    free(buffer);
    fclose(f);

    if (ret == ESP_OK) {
        httpd_resp_send_chunk(req, NULL, 0);
    }
    return ret;
}

// ─────────────────────────────────────────────────────────────────────────────
// OPTIONS /* — CORS preflight (browser sends before a POST with JSON)
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t options_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin",  "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/restart
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// POST /api/ota  — firmware (app) over-the-air update
// Streams the uploaded .bin straight into the inactive OTA slot, switches the
// boot partition and reboots. If no inactive slot exists (unexpected on the
// dual-slot layout) esp_ota_get_next_update_partition() returns NULL → 501.
// ─────────────────────────────────────────────────────────────────────────────
#define OTA_RECV_BUF_SIZE 4096

// Push a progress update to the on-device OTA screen. pct 0..100, or -1 = failed.
static void ota_ui_progress(int pct)
{
    ui_event_t ev = { .type = UI_EVT_OTA_PROGRESS, .ota_progress = pct };
    ui_event_send(&ev);
}

static esp_err_t api_ota_post_handler(httpd_req_t *req)
{
    const esp_partition_t *update = esp_ota_get_next_update_partition(NULL);
    if (update == NULL) {
        httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED,
                            "OTA not supported on this partition layout");
        return ESP_FAIL;
    }

    int total = req->content_len;
    if (total <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty body");
        return ESP_FAIL;
    }
    if ((size_t)total > update->size) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image larger than OTA slot");
        return ESP_FAIL;
    }

    // Bring up the progress screen FIRST and give the LVGL task a head start to
    // build + flush it. The next step, esp_ota_begin(), erases the slot, which
    // disables the flash cache and stalls everything executing from flash —
    // LVGL and the fonts included. Without this head start the screen paints in
    // slow motion during the erase. radio_stop() (stops playback, frees RAM,
    // avoids audio/flash contention) overlaps the screen render, and the short
    // delay lets the "0%" frame land before the erase storm. Tunable if a slow
    // panel needs longer to flush a full frame.
    ui_navigate(SCREEN_OTA);
    ota_ui_progress(0);
    radio_stop();
    vTaskDelay(pdMS_TO_TICKS(300));

    char *buf = malloc(OTA_RECV_BUF_SIZE);
    if (!buf) {
        ota_ui_progress(-1);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota = 0;
    esp_err_t err = esp_ota_begin(update, total, &ota);
    if (err != ESP_OK) {
        free(buf);
        ota_ui_progress(-1);
        ESP_LOGE("OTA", "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "ota_begin failed");
        return ESP_FAIL;
    }

    ESP_LOGI("OTA", "Receiving %d bytes → partition '%s'", total, update->label);

    int received = 0;
    int last_pct = 0;
    bool magic_checked = false;
    while (received < total) {
        int r = httpd_req_recv(req, buf, OTA_RECV_BUF_SIZE);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;
        }
        if (r <= 0) {
            esp_ota_abort(ota);
            free(buf);
            ota_ui_progress(-1);
            ESP_LOGE("OTA", "recv error after %d/%d bytes", received, total);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv error");
            return ESP_FAIL;
        }
        // The first byte of an ESP app image must be the magic 0xE9. Reject any
        // other file early so a wrong upload can't be written into the slot.
        if (!magic_checked) {
            if ((uint8_t)buf[0] != ESP_IMAGE_HEADER_MAGIC) {
                esp_ota_abort(ota);
                free(buf);
                ota_ui_progress(-1);
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not an ESP firmware image");
                return ESP_FAIL;
            }
            magic_checked = true;
        }
        err = esp_ota_write(ota, buf, r);
        if (err != ESP_OK) {
            esp_ota_abort(ota);
            free(buf);
            ota_ui_progress(-1);
            ESP_LOGE("OTA", "esp_ota_write failed: %s", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write error");
            return ESP_FAIL;
        }
        received += r;

        // Throttle UI updates: the event queue is small and drops on overflow,
        // so only push when the whole-percent figure moves by >= 2.
        int pct = (int)((int64_t)received * 100 / total);
        if (pct >= last_pct + 2) {
            last_pct = pct;
            ota_ui_progress(pct);
        }
    }
    free(buf);

    err = esp_ota_end(ota);   // validates the full image (signature/checksum)
    if (err != ESP_OK) {
        ota_ui_progress(-1);
        ESP_LOGE("OTA", "esp_ota_end failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Image validation failed");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        ota_ui_progress(-1);
        ESP_LOGE("OTA", "set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "set_boot failed");
        return ESP_FAIL;
    }

    ota_ui_progress(100);
    ESP_LOGI("OTA", "Update OK (%d bytes), rebooting into '%s'", received, update->label);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/ota/backup  — download the currently running app partition as a .bin
// Streams the whole active slot (including trailing 0xFF padding); the result is
// a valid, re-flashable image. Lets you snapshot working firmware before an
// update so you can roll back by re-uploading it.
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t api_ota_backup_handler(httpd_req_t *req)
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (run == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "No running partition");
        return ESP_FAIL;
    }

    char *buf = malloc(OTA_RECV_BUF_SIZE);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OOM");
        return ESP_FAIL;
    }

    char disp[96];
    snprintf(disp, sizeof(disp), "attachment; filename=\"atlascube-%s.bin\"",
             esp_app_get_description()->version);
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Content-Disposition", disp);

    for (size_t off = 0; off < run->size; off += OTA_RECV_BUF_SIZE) {
        size_t chunk = run->size - off;
        if (chunk > OTA_RECV_BUF_SIZE) chunk = OTA_RECV_BUF_SIZE;
        esp_err_t err = esp_partition_read(run, off, buf, chunk);
        if (err != ESP_OK) {
            ESP_LOGE("OTA", "backup read failed at %u: %s",
                     (unsigned)off, esp_err_to_name(err));
            free(buf);
            httpd_resp_send_chunk(req, NULL, 0);   // terminate the (already-open) stream
            return ESP_FAIL;
        }
        if (httpd_resp_send_chunk(req, buf, chunk) != ESP_OK) {
            free(buf);                              // client disconnected
            return ESP_FAIL;
        }
    }
    free(buf);
    httpd_resp_send_chunk(req, NULL, 0);            // end of stream
    ESP_LOGI("OTA", "Backup of '%s' (%u bytes) sent", run->label, (unsigned)run->size);
    return ESP_OK;
}

static esp_err_t api_restart_handler(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
    return ESP_OK;
}

void http_server_start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn      = httpd_uri_match_wildcard;
    config.max_uri_handlers  = 48;
    // WS handlers run on this task and chain deep: cJSON_Parse of the inbound
    // payload → radio/settings play path → send_full_state (cJSON build of the
    // whole state). The 4 KB HTTPD default overflows on that path (e.g. an
    // SD→radio play_index) — 8 KB gives the JSON build + play chain headroom.
    config.stack_size        = 8192;
    // max_open_sockets: each lwIP socket uses ~2KB internal RAM for buffers
    // TCP + control. On ESP32 with tight internal heap (radio TLS, WiFi, LVGL)
    // 13 sockets caused the TLS audio stream to drop on page open.
    // 7 is plenty: the browser opens 2-4 parallel connections,
    // plus WS and possibly a second tab.
    config.max_open_sockets  = 7;
    config.linger_timeout    = 0;         // ← release sockets immediately after close
    config.recv_wait_timeout = 3;         // ← don't wait forever for data (seconds)
    config.send_wait_timeout = 3;         // ← don't wait forever for send
    config.close_fn          = ws_on_close; // clean up WS slots when socket closes

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE("HTTP", "Failed to start server");
        return;
    }

    ws_set_server(server);
    ws_register(server);

    // ── API — must come before wildcard! ───────────────────────────────────
    httpd_uri_t api_get = {
        .uri     = "/api/settings",
        .method  = HTTP_GET,
        .handler = api_settings_get_handler,
    };
    httpd_register_uri_handler(server, &api_get);

    httpd_uri_t api_post = {
        .uri     = "/api/settings",
        .method  = HTTP_POST,
        .handler = api_settings_post_handler,
    };
    httpd_register_uri_handler(server, &api_post);

    httpd_uri_t api_state = {
        .uri     = "/api/state",
        .method  = HTTP_GET,
        .handler = api_state_get_handler,
    };
    httpd_register_uri_handler(server, &api_state);

    httpd_uri_t api_restart = {
        .uri     = "/api/restart",
        .method  = HTTP_POST,
        .handler = api_restart_handler,
    };
    httpd_register_uri_handler(server, &api_restart);

    httpd_uri_t api_ota = {
        .uri     = "/api/ota",
        .method  = HTTP_POST,
        .handler = api_ota_post_handler,
    };
    httpd_register_uri_handler(server, &api_ota);

    httpd_uri_t api_ota_backup = {
        .uri     = "/api/ota/backup",
        .method  = HTTP_GET,
        .handler = api_ota_backup_handler,
    };
    httpd_register_uri_handler(server, &api_ota_backup);

    httpd_uri_t api_theme_get = {
        .uri     = "/api/theme",
        .method  = HTTP_GET,
        .handler = api_theme_get_handler,
    };
    httpd_register_uri_handler(server, &api_theme_get);

    httpd_uri_t api_theme_post = {
        .uri     = "/api/theme",
        .method  = HTTP_POST,
        .handler = api_theme_post_handler,
    };
    httpd_register_uri_handler(server, &api_theme_post);

    httpd_uri_t api_playlist_get = {
        .uri     = "/api/playlist",
        .method  = HTTP_GET,
        .handler = api_playlist_get_handler,
    };
    httpd_register_uri_handler(server, &api_playlist_get);

    httpd_uri_t api_playlist_post = {
        .uri     = "/api/playlist",
        .method  = HTTP_POST,
        .handler = api_playlist_post_handler,
    };
    httpd_register_uri_handler(server, &api_playlist_post);

    httpd_uri_t api_playlist_csv = {
        .uri     = "/api/playlist.csv",
        .method  = HTTP_GET,
        .handler = api_playlist_csv_handler,
    };
    httpd_register_uri_handler(server, &api_playlist_csv);

    // ── EVENTS ────────────────────────────────────────────────────────────────
    httpd_uri_t api_events_get = {
        .uri     = "/api/events",
        .method  = HTTP_GET,
        .handler = api_events_get_handler,
    };
    httpd_register_uri_handler(server, &api_events_get);

    httpd_uri_t api_events_post = {
        .uri     = "/api/events",
        .method  = HTTP_POST,
        .handler = api_events_post_handler,
    };
    httpd_register_uri_handler(server, &api_events_post);

    httpd_uri_t api_events_put = {
        .uri     = "/api/events/*",
        .method  = HTTP_PUT,
        .handler = api_events_put_handler,
    };
    httpd_register_uri_handler(server, &api_events_put);

    httpd_uri_t api_events_delete = {
        .uri     = "/api/events/*",
        .method  = HTTP_DELETE,
        .handler = api_events_delete_handler,
    };
    httpd_register_uri_handler(server, &api_events_delete);

    // ── UI PROFILE / LAYOUT ───────────────────────────────────────────────────
    httpd_uri_t api_ui_meta = {
        .uri = "/api/ui/profile/meta",
        .method = HTTP_GET,
        .handler = api_ui_profile_meta_get_handler,
    };
    httpd_register_uri_handler(server, &api_ui_meta);

    httpd_uri_t api_ui_clock_get = {
        .uri = "/api/ui/profile/clock",
        .method = HTTP_GET,
        .handler = api_ui_profile_clock_get_handler,
    };
    httpd_register_uri_handler(server, &api_ui_clock_get);

    httpd_uri_t api_ui_clock_post = {
        .uri = "/api/ui/profile/clock",
        .method = HTTP_POST,
        .handler = api_ui_profile_clock_post_handler,
    };
    httpd_register_uri_handler(server, &api_ui_clock_post);

    httpd_uri_t api_ui_bt_get = {
        .uri = "/api/ui/profile/bt",
        .method = HTTP_GET,
        .handler = api_ui_profile_bt_get_handler,
    };
    httpd_register_uri_handler(server, &api_ui_bt_get);

    httpd_uri_t api_ui_bt_post = {
        .uri = "/api/ui/profile/bt",
        .method = HTTP_POST,
        .handler = api_ui_profile_bt_post_handler,
    };
    httpd_register_uri_handler(server, &api_ui_bt_post);

    httpd_uri_t api_ui_radio_get = {
        .uri = "/api/ui/profile/radio",
        .method = HTTP_GET,
        .handler = api_ui_profile_radio_get_handler,
    };
    httpd_register_uri_handler(server, &api_ui_radio_get);

    httpd_uri_t api_ui_radio_post = {
        .uri = "/api/ui/profile/radio",
        .method = HTTP_POST,
        .handler = api_ui_profile_radio_post_handler,
    };
    httpd_register_uri_handler(server, &api_ui_radio_post);

    httpd_uri_t api_ui_sd_get = {
        .uri = "/api/ui/profile/sd",
        .method = HTTP_GET,
        .handler = api_ui_profile_sd_get_handler,
    };
    httpd_register_uri_handler(server, &api_ui_sd_get);

    httpd_uri_t api_ui_sd_post = {
        .uri = "/api/ui/profile/sd",
        .method = HTTP_POST,
        .handler = api_ui_profile_sd_post_handler,
    };
    httpd_register_uri_handler(server, &api_ui_sd_post);

    httpd_uri_t api_ui_reset = {
        .uri = "/api/ui/profile/reset",
        .method = HTTP_POST,
        .handler = api_ui_profile_reset_handler,
    };
    httpd_register_uri_handler(server, &api_ui_reset);

    // ── MQTT ──────────────────────────────────────────────────────────────────
    httpd_uri_t api_mqtt_get = {
        .uri     = "/api/mqtt",
        .method  = HTTP_GET,
        .handler = api_mqtt_get_handler,
    };
    httpd_register_uri_handler(server, &api_mqtt_get);

    httpd_uri_t api_mqtt_post = {
        .uri     = "/api/mqtt",
        .method  = HTTP_POST,
        .handler = api_mqtt_post_handler,
    };
    httpd_register_uri_handler(server, &api_mqtt_post);

    // ── FILE EDITOR ───────────────────────────────────────────────────────────
    httpd_uri_t api_files_get = {
        .uri     = "/api/files",
        .method  = HTTP_GET,
        .handler = api_files_get_handler,
    };
    httpd_register_uri_handler(server, &api_files_get);

    httpd_uri_t api_files_put = {
        .uri     = "/api/files/*",
        .method  = HTTP_PUT,
        .handler = api_files_put_handler,
    };
    httpd_register_uri_handler(server, &api_files_put);

    httpd_uri_t api_spiffs_get = {
        .uri     = "/api/spiffs",
        .method  = HTTP_GET,
        .handler = api_spiffs_get_handler,
    };
    httpd_register_uri_handler(server, &api_spiffs_get);

    // ── SD CARD file manager ──────────────────────────────────────────────────
    httpd_uri_t api_sd_info = {
        .uri     = "/api/sd/info",
        .method  = HTTP_GET,
        .handler = api_sd_info_handler,
    };
    httpd_register_uri_handler(server, &api_sd_info);

    httpd_uri_t api_sd_list = {
        .uri     = "/api/sd/list",
        .method  = HTTP_GET,
        .handler = api_sd_list_handler,
    };
    httpd_register_uri_handler(server, &api_sd_list);

    httpd_uri_t api_sd_get = {
        .uri     = "/api/sd/file",
        .method  = HTTP_GET,
        .handler = api_sd_get_handler,
    };
    httpd_register_uri_handler(server, &api_sd_get);

    httpd_uri_t api_sd_post = {
        .uri     = "/api/sd/file",
        .method  = HTTP_POST,
        .handler = api_sd_post_handler,
    };
    httpd_register_uri_handler(server, &api_sd_post);

    httpd_uri_t api_sd_delete = {
        .uri     = "/api/sd/file",
        .method  = HTTP_DELETE,
        .handler = api_sd_delete_handler,
    };
    httpd_register_uri_handler(server, &api_sd_delete);

    httpd_uri_t api_sd_mkdir = {
        .uri     = "/api/sd/mkdir",
        .method  = HTTP_POST,
        .handler = api_sd_mkdir_handler,
    };
    httpd_register_uri_handler(server, &api_sd_mkdir);

    httpd_uri_t api_sd_rename = {
        .uri     = "/api/sd/rename",
        .method  = HTTP_POST,
        .handler = api_sd_rename_handler,
    };
    httpd_register_uri_handler(server, &api_sd_rename);

    // OPTIONS dla /api/restart — preflight CORS
    httpd_uri_t api_restart_options = {
        .uri     = "/api/restart",
        .method  = HTTP_OPTIONS,
        .handler = options_handler,
    };
    httpd_register_uri_handler(server, &api_restart_options);

    // ── OPTIONS wildcard — CORS preflight ─────────────────────────────────────
    httpd_uri_t options_uri = {
        .uri     = "/*",
        .method  = HTTP_OPTIONS,
        .handler = options_handler,
    };
    httpd_register_uri_handler(server, &options_uri);

    // ── wildcard GET — pliki SPIFFS ───────────────────────────────────────────
    httpd_uri_t file_uri = {
        .uri     = "/*",
        .method  = HTTP_GET,
        .handler = file_handler,
    };
    httpd_register_uri_handler(server, &file_uri);

    ESP_LOGI("HTTP", "Server started");
}