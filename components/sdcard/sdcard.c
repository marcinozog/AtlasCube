#include "sdcard.h"
#include "defines.h"
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

// The real mount. Runs on the dedicated sd_mount worker task (see sdcard_init)
// so its ~2-3 KB stack cost never lands on a shallow caller (httpd/WS, UI).
static esp_err_t do_mount(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, // never reformat the user's card
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    // SDMMC slot 1, 1-bit bus. On the ESP32-S3 the SDMMC signals are routed
    // through the GPIO matrix, so any free GPIO works — pins come from defines.h.
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = SD_PIN_CLK;
    slot_config.cmd = SD_PIN_CMD;
    slot_config.d0  = SD_PIN_D0;
    slot_config.cd  = SD_PIN_CD; // GPIO_NUM_NC (-1) when no card-detect line
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

static void mount_task(void *arg)
{
    SemaphoreHandle_t done = (SemaphoreHandle_t)arg;
    do_mount();
    xSemaphoreGive(done);
    vTaskDelete(NULL);
}

esp_err_t sdcard_init(void)
{
    if (s_card != NULL) {
        return ESP_OK; // already mounted — fast path, no task spawned
    }

    // Run the (stack-heavy) mount on its own task and block until it finishes,
    // so callers on shallow stacks (httpd/WS, UI, events) can mount safely.
    SemaphoreHandle_t done = xSemaphoreCreateBinary();
    if (!done) return ESP_ERR_NO_MEM;

    if (xTaskCreate(mount_task, "sd_mount", 4096, done, 5, NULL) != pdPASS) {
        vSemaphoreDelete(done);
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(done, portMAX_DELAY);
    vSemaphoreDelete(done);

    return s_card ? ESP_OK : ESP_FAIL;
}

#else  // !HAS_SD_CARD — variant has no SD hardware

bool sdcard_is_mounted(void) { return false; }

esp_err_t sdcard_init(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif
