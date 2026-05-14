#include "events_service.h"

#include "buzzer.h"
#include "melodies.h"
#include "cJSON.h"
#include "defines.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ntp_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "EVENTS";

// Scheduler tick — every 30 s is enough for minute-level granularity.
#define EVENTS_TICK_US (30 * 1000 * 1000ULL)

#define EVENTS_FILE_TMP EVENTS_FILE ".tmp"

// --------------------------------------------------------------------------
// Global state
// --------------------------------------------------------------------------

// We keep the event_t[EVENTS_MAX] buffers (~5KB each) in PSRAM — internal
// RAM is tight under WiFi/LVGL/mbedTLS and every KB matters during HTTPS
// renegotiation. Access only from tasks (timer, HTTP), never from ISR.
EXT_RAM_BSS_ATTR static event_t s_events[EVENTS_MAX];
EXT_RAM_BSS_ATTR static bool    s_fired_today[EVENTS_MAX];

static int              s_count = 0;
static int              s_last_day = -1;                // for day-change detection

static esp_timer_handle_t s_timer;
static SemaphoreHandle_t  s_mtx;
static bool               s_initialized;
static events_fire_cb_t   s_fire_cb = NULL;

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

static void lock(void)   { xSemaphoreTake(s_mtx, portMAX_DELAY); }
static void unlock(void) { xSemaphoreGive(s_mtx); }

static void gen_id(char out[EVENT_ID_LEN])
{
    uint32_t r = esp_random();
    snprintf(out, EVENT_ID_LEN, "%08lx", (unsigned long)r);
}

static int find_index_locked(const char *id)
{
    for (int i = 0; i < s_count; ++i) {
        if (strncmp(s_events[i].id, id, EVENT_ID_LEN) == 0) return i;
    }
    return -1;
}

static const char *type_to_str(event_type_t t)
{
    switch (t) {
        case EV_BIRTHDAY:    return "birthday";
        case EV_NAMEDAY:     return "nameday";
        case EV_REMINDER:    return "reminder";
        case EV_ANNIVERSARY: return "anniversary";
        default:             return "reminder";
    }
}

static event_type_t type_from_str(const char *s)
{
    if (!s) return EV_REMINDER;
    if (strcmp(s, "birthday")    == 0) return EV_BIRTHDAY;
    if (strcmp(s, "nameday")     == 0) return EV_NAMEDAY;
    if (strcmp(s, "anniversary") == 0) return EV_ANNIVERSARY;
    return EV_REMINDER;
}

static const char *rec_to_str(event_recurrence_t r)
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

static event_recurrence_t rec_from_str(const char *s)
{
    if (!s) return EV_REC_NONE;
    if (strcmp(s, "daily")   == 0) return EV_REC_DAILY;
    if (strcmp(s, "weekly")  == 0) return EV_REC_WEEKLY;
    if (strcmp(s, "monthly") == 0) return EV_REC_MONTHLY;
    if (strcmp(s, "yearly")  == 0) return EV_REC_YEARLY;
    return EV_REC_NONE;
}

// --------------------------------------------------------------------------
// JSON — load / save (atomic write via .tmp + rename)
// --------------------------------------------------------------------------

static esp_err_t load_from_file(void)
{
    FILE *f = fopen(EVENTS_FILE, "r");
    if (!f) return ESP_FAIL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size <= 0) { fclose(f); return ESP_FAIL; }

    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return ESP_ERR_NO_MEM; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { ESP_LOGW(TAG, "JSON parse failed"); return ESP_FAIL; }

    cJSON *arr = cJSON_GetObjectItem(root, "events");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return ESP_FAIL; }

    s_count = 0;
    int n = cJSON_GetArraySize(arr);
    for (int i = 0; i < n && s_count < EVENTS_MAX; ++i) {
        cJSON *it = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(it)) continue;

        event_t e = {0};

        cJSON *j = cJSON_GetObjectItem(it, "id");
        if (cJSON_IsString(j)) strncpy(e.id, j->valuestring, EVENT_ID_LEN - 1);
        if (e.id[0] == '\0') gen_id(e.id);

        j = cJSON_GetObjectItem(it, "type");
        e.type = type_from_str(cJSON_IsString(j) ? j->valuestring : NULL);

        j = cJSON_GetObjectItem(it, "title");
        if (cJSON_IsString(j)) strncpy(e.title, j->valuestring, EVENT_TITLE_LEN - 1);

        j = cJSON_GetObjectItem(it, "year");   if (cJSON_IsNumber(j)) e.year  = j->valueint;
        j = cJSON_GetObjectItem(it, "month");  if (cJSON_IsNumber(j)) e.month = j->valueint;
        j = cJSON_GetObjectItem(it, "day");    if (cJSON_IsNumber(j)) e.day   = j->valueint;
        j = cJSON_GetObjectItem(it, "hour");   if (cJSON_IsNumber(j)) e.hour  = j->valueint;
        j = cJSON_GetObjectItem(it, "minute"); if (cJSON_IsNumber(j)) e.minute= j->valueint;

        j = cJSON_GetObjectItem(it, "recurrence");
        e.recurrence = rec_from_str(cJSON_IsString(j) ? j->valuestring : NULL);

        j = cJSON_GetObjectItem(it, "enabled");
        e.enabled = cJSON_IsBool(j) ? cJSON_IsTrue(j) : true;

        s_events[s_count++] = e;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d events", s_count);
    return ESP_OK;
}

static esp_err_t save_to_file(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *arr  = cJSON_AddArrayToObject(root, "events");

    for (int i = 0; i < s_count; ++i) {
        const event_t *e = &s_events[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "id",                 e->id);
        cJSON_AddStringToObject(o, "type",               type_to_str(e->type));
        cJSON_AddStringToObject(o, "title",              e->title);
        cJSON_AddNumberToObject(o, "year",               e->year);
        cJSON_AddNumberToObject(o, "month",              e->month);
        cJSON_AddNumberToObject(o, "day",                e->day);
        cJSON_AddNumberToObject(o, "hour",               e->hour);
        cJSON_AddNumberToObject(o, "minute",             e->minute);
        cJSON_AddStringToObject(o, "recurrence",         rec_to_str(e->recurrence));
        cJSON_AddBoolToObject  (o, "enabled",            e->enabled);
        cJSON_AddItemToArray(arr, o);
    }

    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) return ESP_ERR_NO_MEM;

    FILE *f = fopen(EVENTS_FILE_TMP, "w");
    if (!f) { free(str); return ESP_FAIL; }
    fwrite(str, 1, strlen(str), f);
    fclose(f);
    free(str);

    // SPIFFS requires removing the destination file before rename.
    remove(EVENTS_FILE);
    if (rename(EVENTS_FILE_TMP, EVENTS_FILE) != 0) {
        ESP_LOGE(TAG, "rename tmp→target failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// --------------------------------------------------------------------------
// "Does this event fall on today?" per recurrence rules
// --------------------------------------------------------------------------

static bool matches_today(const event_t *e, const struct tm *lt)
{
    if (!e->enabled) return false;

    switch (e->recurrence) {
        case EV_REC_NONE:
            return (lt->tm_year + 1900 == e->year)
                && (lt->tm_mon  + 1    == e->month)
                && (lt->tm_mday        == e->day);

        case EV_REC_DAILY:
            return true;

        case EV_REC_WEEKLY: {
            // Weekday of the base date.
            struct tm base = {
                .tm_year = e->year - 1900,
                .tm_mon  = e->month - 1,
                .tm_mday = e->day,
                .tm_hour = 12,
            };
            time_t t = mktime(&base);
            if (t == (time_t)-1) return false;
            struct tm tmp;
            localtime_r(&t, &tmp);
            return tmp.tm_wday == lt->tm_wday;
        }

        case EV_REC_MONTHLY:
            return lt->tm_mday == e->day;

        case EV_REC_YEARLY:
            return (lt->tm_mon + 1 == e->month) && (lt->tm_mday == e->day);
    }
    return false;
}

// --------------------------------------------------------------------------
// Scheduler
// --------------------------------------------------------------------------

static void fire_event(const event_t *e)
{
    ESP_LOGI(TAG, "FIRE: [%s] %s (type=%s)",
             e->id, e->title, type_to_str(e->type));

    if (e->type == EV_BIRTHDAY || e->type == EV_ANNIVERSARY) {
        melody_play(MELODY_BIRTHDAY);
    } else {
        melody_play(MELODY_REMINDER);
    }

    if (s_fire_cb) s_fire_cb(e);
}

static void check_events(void)
{
    if (!ntp_service_is_synced()) return;

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);

    // Day changed → clear flags.
    if (s_last_day != lt.tm_mday) {
        s_last_day = lt.tm_mday;
        memset(s_fired_today, 0, sizeof(s_fired_today));
        ESP_LOGI(TAG, "Day changed → flags reset (today=%04d-%02d-%02d)",
                 lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    }

    int now_min = lt.tm_hour * 60 + lt.tm_min;

    lock();
    for (int i = 0; i < s_count; ++i) {
        event_t *e = &s_events[i];
        if (!matches_today(e, &lt)) continue;

        int target = e->hour * 60 + e->minute;

        // 1-minute window: scheduler ticks every 30 s, won't miss.
        if (!s_fired_today[i] && now_min >= target && now_min <= target + 1) {
            fire_event(e);
            s_fired_today[i] = true;
        }
    }
    unlock();
}

static void timer_cb(void *arg)
{
    (void)arg;
    check_events();
}

// --------------------------------------------------------------------------
// Public API
// --------------------------------------------------------------------------

esp_err_t events_service_init(void)
{
    if (s_initialized) return ESP_OK;

    s_mtx = xSemaphoreCreateMutex();
    if (!s_mtx) return ESP_ERR_NO_MEM;

    if (load_from_file() != ESP_OK) {
        ESP_LOGI(TAG, "No events.json yet → starting with empty list");
        s_count = 0;
        save_to_file();
    }

    esp_timer_create_args_t args = {
        .callback        = timer_cb,
        .name            = "events_tick",
    };
    esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) return err;

    err = esp_timer_start_periodic(s_timer, EVENTS_TICK_US);
    if (err != ESP_OK) return err;

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized, tick=%llu us", (unsigned long long)EVENTS_TICK_US);

    // First check immediately (not after 30 s).
    check_events();
    return ESP_OK;
}

void events_service_on_tz_changed(void)
{
    if (!s_initialized) return;
    ESP_LOGI(TAG, "TZ changed → resetting fired flags");
    lock();
    memset(s_fired_today, 0, sizeof(s_fired_today));
    s_last_day = -1;
    unlock();
    check_events();
}

esp_err_t events_add(event_t *ev)
{
    if (!ev) return ESP_ERR_INVALID_ARG;
    lock();
    if (s_count >= EVENTS_MAX) { unlock(); return ESP_ERR_NO_MEM; }

    if (ev->id[0] == '\0') gen_id(ev->id);
    // ID collision → regenerate a few times.
    for (int tries = 0; tries < 5 && find_index_locked(ev->id) >= 0; ++tries) {
        gen_id(ev->id);
    }

    s_events[s_count] = *ev;
    s_fired_today[s_count] = false;
    s_count++;
    esp_err_t err = save_to_file();
    unlock();

    if (err == ESP_OK) check_events();
    return err;
}

esp_err_t events_update(const char *id, const event_t *ev)
{
    if (!id || !ev) return ESP_ERR_INVALID_ARG;
    lock();
    int idx = find_index_locked(id);
    if (idx < 0) { unlock(); return ESP_ERR_NOT_FOUND; }

    event_t updated = *ev;
    strncpy(updated.id, id, EVENT_ID_LEN - 1);
    updated.id[EVENT_ID_LEN - 1] = '\0';

    s_events[idx] = updated;
    s_fired_today[idx] = false;   // allow firing again today
    esp_err_t err = save_to_file();
    unlock();

    if (err == ESP_OK) check_events();
    return err;
}

esp_err_t events_remove(const char *id)
{
    if (!id) return ESP_ERR_INVALID_ARG;
    lock();
    int idx = find_index_locked(id);
    if (idx < 0) { unlock(); return ESP_ERR_NOT_FOUND; }

    for (int i = idx; i < s_count - 1; ++i) {
        s_events[i]      = s_events[i + 1];
        s_fired_today[i] = s_fired_today[i + 1];
    }
    s_count--;
    esp_err_t err = save_to_file();
    unlock();
    return err;
}

int events_get_all(event_t *out, int max)
{
    if (!out || max <= 0) return 0;
    lock();
    int n = s_count < max ? s_count : max;
    memcpy(out, s_events, n * sizeof(event_t));
    unlock();
    return n;
}

int events_count(void)
{
    lock();
    int n = s_count;
    unlock();
    return n;
}

const event_t *events_find(const char *id)
{
    if (!id) return NULL;
    lock();
    int idx = find_index_locked(id);
    const event_t *p = (idx >= 0) ? &s_events[idx] : NULL;
    unlock();
    return p;
}

void events_service_set_fire_cb(events_fire_cb_t cb)
{
    s_fire_cb = cb;
}

const char *events_type_label(event_type_t t)
{
    switch (t) {
        case EV_BIRTHDAY:    return "BIRTHDAY";
        case EV_NAMEDAY:     return "NAME DAY";
        case EV_ANNIVERSARY: return "ANNIVERSARY";
        case EV_REMINDER:
        default:             return "REMINDER";
    }
}

int events_pending_today_count(void)
{
    if (!s_initialized || !ntp_service_is_synced()) return 0;

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    int now_min = lt.tm_hour * 60 + lt.tm_min;

    lock();
    int n = 0;
    for (int i = 0; i < s_count; ++i) {
        const event_t *e = &s_events[i];
        if (!e->enabled) continue;
        if (s_fired_today[i]) continue;
        if (!matches_today(e, &lt)) continue;

        int target = e->hour * 60 + e->minute;
        if (target >= now_min) n++;
    }
    unlock();
    return n;
}
