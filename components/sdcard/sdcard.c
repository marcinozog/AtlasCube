#include "sdcard.h"
#include "defines.h"
#include "board_pins.h"
#include "esp_log.h"

#ifdef HAS_SD_CARD
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif

static const char *TAG = "SDCARD";

#ifdef HAS_SD_CARD
static sdmmc_card_t *s_card = NULL;

bool sdcard_is_mounted(void)
{
    return s_card != NULL;
}

// One place for the FATFS parameters, shared by the mount and the explicit
// reformat, so a formatted card comes back exactly the way we mount one.
// `format_if_mount_failed` is false everywhere except sdcard_format().
static esp_vfs_fat_mount_config_t fat_config(bool format_if_mount_failed)
{
    esp_vfs_fat_mount_config_t cfg = {
        .format_if_mount_failed = format_if_mount_failed,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    return cfg;
}

// The real mount. Runs on the dedicated sd_mount worker task (see sdcard_init)
// so its ~2-3 KB stack cost never lands on a shallow caller (httpd/WS, UI).
static esp_err_t do_mount(bool format_if_mount_failed)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = fat_config(format_if_mount_failed);

    // SDMMC slot 1, 1-bit bus. On the ESP32-S3 the SDMMC signals are routed
    // through the GPIO matrix, so any free GPIO works — pins come from defines.h.
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = g_pins.sd_clk;
    slot_config.cmd = g_pins.sd_cmd;
    slot_config.d0  = g_pins.sd_d0;
    slot_config.cd  = g_pins.sd_cd; // GPIO_NUM_NC (-1) when no card-detect line
    // Enable the weak internal pull-ups as a fallback. CMD and D0 should still
    // carry ~10k external pull-ups on the PCB for reliable 1-bit operation.
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_err_t err = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config,
                                            &mount_config, &s_card);
    if (err != ESP_OK) {
        s_card = NULL;
        if (err == ESP_FAIL) {
            ESP_LOGW(TAG, "Mount failed — card present but no FAT filesystem?");
        } else {
            ESP_LOGW(TAG, "SD init failed (%s) — continuing without SD",
                     esp_err_to_name(err));
        }
        return err;
    }

    ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

// The real format. Runs on the dedicated sd_format worker task (see
// sdcard_format) because f_mkfs is even hungrier for stack than the mount.
static esp_err_t do_format(void)
{
    if (s_card == NULL) {
        // Nothing is mounted: a fresh or corrupt card that do_mount() turned
        // down for lack of a filesystem. esp_vfs_fat_sdcard_format_cfg() is no
        // help here — it needs a mounted card — so mounting with
        // format_if_mount_failed creates the filesystem instead.
        esp_err_t err = do_mount(true);
        if (err == ESP_OK) return ESP_OK;
        // ESP_FAIL is "card present, making the filesystem failed"; anything
        // else means the card itself never answered.
        return (err == ESP_FAIL) ? ESP_FAIL : ESP_ERR_NOT_FOUND;
    }

    esp_vfs_fat_mount_config_t cfg = fat_config(false);
    esp_err_t err = esp_vfs_fat_sdcard_format_cfg(SD_MOUNT_POINT, s_card, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Format failed (%s)", esp_err_to_name(err));
    }

    // That call remounts the fresh filesystem itself — but when the remount
    // fails it recycles the card handle and still reports the format's result,
    // which would leave us holding a stale s_card. Probe the mount to tell the
    // two apart.
    uint64_t total = 0, freeb = 0;
    if (esp_vfs_fat_info(SD_MOUNT_POINT, &total, &freeb) != ESP_OK) {
        ESP_LOGE(TAG, "Card is not mounted after format");
        s_card = NULL;   // let the next sdcard_init() mount from scratch
        return ESP_FAIL;
    }
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "SD card formatted, %llu MB free",
                 (unsigned long long)(freeb / (1024 * 1024)));
    }
    return err;
}

// What the worker task should do, and where it leaves the result.
typedef struct {
    bool format;                 // false = plain mount, true = reformat
    esp_err_t result;
    SemaphoreHandle_t done;
} sd_job_t;

static void sd_worker_task(void *arg)
{
    sd_job_t *job = (sd_job_t *)arg;
    job->result = job->format ? do_format() : do_mount(false);
    xSemaphoreGive(job->done);
    vTaskDelete(NULL);
}

// Runs the (stack-heavy) job on its own task and blocks until it finishes, so
// callers on shallow stacks (httpd/WS, UI, events) can mount and format safely.
static esp_err_t run_on_worker(bool format)
{
    sd_job_t job = { .format = format, .result = ESP_FAIL };

    job.done = xSemaphoreCreateBinary();
    if (!job.done) return ESP_ERR_NO_MEM;

    if (xTaskCreate(sd_worker_task, format ? "sd_format" : "sd_mount",
                    format ? 6144 : 4096, &job, 5, NULL) != pdPASS) {
        vSemaphoreDelete(job.done);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(job.done, portMAX_DELAY);
    vSemaphoreDelete(job.done);
    return job.result;
}

esp_err_t sdcard_init(void)
{
    if (s_card != NULL) {
        return ESP_OK; // already mounted — fast path, no task spawned
    }

    run_on_worker(false);
    return s_card ? ESP_OK : ESP_FAIL;
}

esp_err_t sdcard_format(void)
{
    return run_on_worker(true);
}

#else  // !HAS_SD_CARD — variant has no SD hardware

bool sdcard_is_mounted(void) { return false; }

esp_err_t sdcard_init(void) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t sdcard_format(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif
