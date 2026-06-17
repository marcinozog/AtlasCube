#include "lv_bin_image.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "LV_BIN";

// LVGL v9 binary image header (little-endian, 12 bytes) — see scripts/img2lvgl.py.
#define LV_BIN_MAGIC   0x19
#define LV_BIN_RGB565  0x12

typedef struct __attribute__((packed)) {
    uint8_t  magic;
    uint8_t  cf;
    uint16_t flags;
    uint16_t w;
    uint16_t h;
    uint16_t stride;
    uint16_t reserved;
} bin_header_t;

lv_image_dsc_t *lv_bin_image_load(const char *path, int require_w, int require_h)
{
    if (!path || !path[0]) return NULL;

    FILE *fp = fopen(path, "rb");
    if (!fp) { ESP_LOGW(TAG, "open %s failed", path); return NULL; }

    bin_header_t h;
    if (fread(&h, sizeof(h), 1, fp) != 1 ||
        h.magic != LV_BIN_MAGIC || h.cf != LV_BIN_RGB565 ||
        (require_w && h.w != require_w) || (require_h && h.h != require_h)) {
        ESP_LOGW(TAG, "%s: bad header or size %ux%u", path, h.w, h.h);
        fclose(fp);
        return NULL;
    }

    const size_t px = (size_t)h.w * h.h * 2;
    uint16_t *buf = heap_caps_malloc(px, MALLOC_CAP_SPIRAM);
    lv_image_dsc_t *dsc = buf ? calloc(1, sizeof(*dsc)) : NULL;
    if (!buf || !dsc) {
        ESP_LOGE(TAG, "%s: alloc failed (%u B)", path, (unsigned)px);
        free(buf);
        fclose(fp);
        return NULL;
    }

    const size_t got = fread(buf, (size_t)h.w * 2, h.h, fp);
    fclose(fp);
    if (got != h.h) {
        ESP_LOGW(TAG, "%s: short read (%u/%u rows)", path, (unsigned)got, h.h);
        free(buf);
        free(dsc);
        return NULL;
    }

    dsc->header.magic  = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf     = LV_COLOR_FORMAT_RGB565;
    dsc->header.w      = h.w;
    dsc->header.h      = h.h;
    dsc->header.stride = h.w * 2;
    dsc->data_size     = (uint32_t)px;
    dsc->data          = (const uint8_t *)buf;
    ESP_LOGI(TAG, "loaded %s (%ux%u)", path, h.w, h.h);
    return dsc;
}

void lv_bin_image_free(lv_image_dsc_t *dsc)
{
    if (!dsc) return;
    lv_image_cache_drop(dsc);
    free((void *)dsc->data);
    free(dsc);
}
