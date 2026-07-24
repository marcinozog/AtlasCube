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

// RGB565 channel extract helpers (little-endian value already in a uint16_t).
#define BIN_R5(p) (((p) >> 11) & 0x1F)
#define BIN_G6(p) (((p) >>  5) & 0x3F)
#define BIN_B5(p) ( (p)        & 0x1F)

lv_image_dsc_t *lv_bin_image_load_scaled(const char *path, int dst_w, int dst_h)
{
    if (dst_w <= 0 || dst_h <= 0) return NULL;

    lv_image_dsc_t *src = lv_bin_image_load(path, 0, 0);
    if (!src) return NULL;

    const int sw = src->header.w, sh = src->header.h;
    if (sw == dst_w && sh == dst_h) return src;   // no resampling needed

    const size_t dpx = (size_t)dst_w * dst_h * 2;
    uint16_t *dbuf = heap_caps_malloc(dpx, MALLOC_CAP_SPIRAM);
    lv_image_dsc_t *dsc = dbuf ? calloc(1, sizeof(*dsc)) : NULL;
    if (!dbuf || !dsc) {
        ESP_LOGE(TAG, "%s: scale alloc failed (%u B)", path, (unsigned)dpx);
        free(dbuf);
        lv_bin_image_free(src);
        return NULL;
    }

    const uint16_t *sbuf = (const uint16_t *)src->data;
    for (int y = 0; y < dst_h; y++) {
        const float fy = dst_h > 1 ? (float)y * (sh - 1) / (dst_h - 1) : 0.0f;
        const int   y0 = (int)fy;
        const int   y1 = y0 < sh - 1 ? y0 + 1 : y0;
        const float wy = fy - y0;
        for (int x = 0; x < dst_w; x++) {
            const float fx = dst_w > 1 ? (float)x * (sw - 1) / (dst_w - 1) : 0.0f;
            const int   x0 = (int)fx;
            const int   x1 = x0 < sw - 1 ? x0 + 1 : x0;
            const float wx = fx - x0;

            const uint16_t p00 = sbuf[y0 * sw + x0], p01 = sbuf[y0 * sw + x1];
            const uint16_t p10 = sbuf[y1 * sw + x0], p11 = sbuf[y1 * sw + x1];

            const float rt = BIN_R5(p00) + wx * (BIN_R5(p01) - BIN_R5(p00));
            const float rb = BIN_R5(p10) + wx * (BIN_R5(p11) - BIN_R5(p10));
            const float gt = BIN_G6(p00) + wx * (BIN_G6(p01) - BIN_G6(p00));
            const float gb = BIN_G6(p10) + wx * (BIN_G6(p11) - BIN_G6(p10));
            const float bt = BIN_B5(p00) + wx * (BIN_B5(p01) - BIN_B5(p00));
            const float bb = BIN_B5(p10) + wx * (BIN_B5(p11) - BIN_B5(p10));

            const int r = (int)(rt + wy * (rb - rt) + 0.5f);
            const int g = (int)(gt + wy * (gb - gt) + 0.5f);
            const int b = (int)(bt + wy * (bb - bt) + 0.5f);
            dbuf[y * dst_w + x] = (uint16_t)((r << 11) | (g << 5) | b);
        }
    }

    dsc->header.magic  = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf     = LV_COLOR_FORMAT_RGB565;
    dsc->header.w      = dst_w;
    dsc->header.h      = dst_h;
    dsc->header.stride = dst_w * 2;
    dsc->data_size     = (uint32_t)dpx;
    dsc->data          = (const uint8_t *)dbuf;
    lv_bin_image_free(src);   // native pixels no longer needed
    ESP_LOGI(TAG, "scaled %s to %dx%d", path, dst_w, dst_h);
    return dsc;
}

void lv_bin_image_free(lv_image_dsc_t *dsc)
{
    if (!dsc) return;
    lv_image_cache_drop(dsc);
    free((void *)dsc->data);
    free(dsc);
}
