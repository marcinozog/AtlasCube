#include "heap_report.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "HEAP";

void heap_report(const char *where)
{
    // The handshake/httpd failures are about the *largest contiguous* internal
    // block, not total free — so report both, plus the DMA-capable subset
    // (WiFi/LCD) and the min-ever free as a low-water mark.
    size_t int_free   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t int_large  = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    size_t int_min    = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    size_t dma_large  = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    ESP_LOGW(TAG, "[%s] INT free=%u largest=%u min=%u | DMA largest=%u | PSRAM free=%u",
             where ? where : "?",
             (unsigned)int_free, (unsigned)int_large, (unsigned)int_min,
             (unsigned)dma_large, (unsigned)psram_free);
}
