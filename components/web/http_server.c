#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "radio_service.h"
#include "ws_server.h"
#include "settings.h"
#include "ntp_service.h"
#include "app_state.h"
#include "wifi_manager.h"
#include "theme.h"
#include "ui_manager.h"
#include "ui_events.h"
#include "ui_profile.h"
#include "fonts/ui_fonts.h"
#include "playlist.h"
#include "events_service.h"
#include "screensavers.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
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
    cJSON_AddItemToObject(json, "playlist", playlist);

    // display
    cJSON *display = cJSON_CreateObject();
    cJSON_AddNumberToObject(display, "brightness", s->display.brightness);
    cJSON_AddStringToObject(display, "theme",
        s->display.theme == THEME_LIGHT ? "light" : "dark");
    cJSON_AddItemToObject(json, "display", display);

    // bluetooth
    cJSON *bt = cJSON_CreateObject();
    cJSON_AddBoolToObject(bt,   "enable", s->bluetooth.enable);
    cJSON_AddBoolToObject(bt,   "show_screen", s->bluetooth.show_screen);
    cJSON_AddNumberToObject(bt, "volume", s->bluetooth.volume);
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

    // screensaver
    cJSON *scrs = cJSON_CreateObject();
    cJSON_AddNumberToObject(scrs, "delay",  s->scrsaver.delay);
    cJSON_AddStringToObject(scrs, "id",
        screensaver_name(s->scrsaver.screensaver_id));
    cJSON_AddItemToObject(json, "scrsaver", scrs);


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
    }

    // ── BLUETOOTH ─────────────────────────────────────────────────────────────
    cJSON *bt = cJSON_GetObjectItem(json, "bluetooth");
    if (cJSON_IsObject(bt)) {
        cJSON *en       = cJSON_GetObjectItem(bt, "enable");
        cJSON *sh_scr   = cJSON_GetObjectItem(bt, "show_screen");
        cJSON *bvol     = cJSON_GetObjectItem(bt, "volume");
        if (cJSON_IsBool(en))     settings_set_bt_enable(cJSON_IsTrue(en));
        if (cJSON_IsBool(sh_scr)) settings_set_bt_show_screen(cJSON_IsTrue(sh_scr));
        if (cJSON_IsNumber(bvol)) settings_set_bt_volume(bvol->valueint);
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
    cJSON_AddBoolToObject  (json, "bt_show_screen", s->bt_show_screen);
    cJSON_AddBoolToObject  (json, "time_synced",    s->time_synced);
    // WiFi mode — useful for the settings page UI
    cJSON_AddStringToObject(json, "wifi_mode",
        wifi_get_run_mode() == WIFI_RUN_MODE_AP ? "ap" : "sta");

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
        default:             return "reminder";
    }
}

static event_type_t ev_type_from_str(const char *s)
{
    if (!s) return EV_REMINDER;
    if (strcmp(s, "birthday")    == 0) return EV_BIRTHDAY;
    if (strcmp(s, "nameday")     == 0) return EV_NAMEDAY;
    if (strcmp(s, "anniversary") == 0) return EV_ANNIVERSARY;
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

static esp_err_t api_files_get_handler(httpd_req_t *req)
{
    DIR *d = opendir(WEB_ROOT);
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
        snprintf(fullpath, sizeof(fullpath), "%s/%s", WEB_ROOT, fname);
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

    bool do_gzip = (strcmp(ext, ".html") == 0 ||
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
        snprintf(dest_path, sizeof(dest_path), "%s/%s.gz", WEB_ROOT, name);
    else
        snprintf(dest_path, sizeof(dest_path), "%s/%s",    WEB_ROOT, name);

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
// Wildcard — serve files from SPIFFS
// ─────────────────────────────────────────────────────────────────────────────
static esp_err_t file_handler(httpd_req_t *req)
{
    char filepath[256];
    char gz_filepath[260];

    strcpy(filepath, WEB_ROOT);

    if (strcmp(req->uri, "/") == 0) {
        if (wifi_get_run_mode() == WIFI_RUN_MODE_AP) {
            strcat(filepath, "/wifi_setup.html");
        } else {
            strcat(filepath, "/index.html");
        }
    } else {
        if (strlen(filepath) + strlen(req->uri) >= sizeof(filepath)) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Path too long");
            return ESP_FAIL;
        }
        strcat(filepath, req->uri);
    }

    // Content-Type based on the original extension (before adding .gz)
    const char *content_type = "text/plain";
    if      (strstr(filepath, ".html")) content_type = "text/html; charset=utf-8";
    else if (strstr(filepath, ".css"))  content_type = "text/css";
    else if (strstr(filepath, ".json")) content_type = "application/json"; // must come before js
    else if (strstr(filepath, ".js"))   content_type = "application/javascript";
    else if (strstr(filepath, ".ico"))  content_type = "image/x-icon";

    // Try .gz first
    bool use_gz = false;
    snprintf(gz_filepath, sizeof(gz_filepath), "%s.gz", filepath);
    FILE *f = fopen(gz_filepath, "rb");
    if (f) {
        use_gz = true;
        ESP_LOGD("HTTP", "Serving gz: %s", gz_filepath);
    } else {
        f = fopen(filepath, "rb");
    }

    if (!f) {
        ESP_LOGW("HTTP", "File not found: %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, content_type);

    if (use_gz) {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    // Cache:
    //  - HTML: no-cache (entry point — must refresh after firmware update)
    //  - rest (CSS/JS/ICO/fonts): long cache so the browser doesn't keep hitting
    //    the ESP on every tab open. Critical while radio is playing —
    //    parallel asset fetching + TLS audio caused stream drops.
    bool is_html = (strstr(filepath, ".html") != NULL);
    if (is_html) {
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
    config.max_uri_handlers  = 28;
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

    httpd_uri_t api_ui_reset = {
        .uri = "/api/ui/profile/reset",
        .method = HTTP_POST,
        .handler = api_ui_profile_reset_handler,
    };
    httpd_register_uri_handler(server, &api_ui_reset);

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