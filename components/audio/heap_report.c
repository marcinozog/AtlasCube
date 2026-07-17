#include "heap_report.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"   // xTaskCreatePinnedToCoreWithCaps
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


/* ── per-task CPU dump ──────────────────────────────────────────────────────
   Delta-based: percentages cover only the last sample period, not since boot.
   The runtime counter total sums BOTH cores, so a task's per-core share uses
   total_delta / portNUM_PROCESSORS as the 100% base. Unpinned tasks report
   core '*' (their runtime may span both cores). */

#define CPU_REPORT_PERIOD_MS  5000
#define CPU_REPORT_TOP        10
#define CPU_REPORT_MAX_TASKS  48

typedef struct {
    TaskHandle_t handle;
    uint32_t     runtime;
} cpu_prev_t;

static cpu_prev_t s_cpu_prev[CPU_REPORT_MAX_TASKS];
static int        s_cpu_prev_count = 0;
static uint32_t   s_cpu_prev_total = 0;

static uint32_t cpu_prev_runtime(TaskHandle_t h, bool *found)
{
    for (int i = 0; i < s_cpu_prev_count; i++) {
        if (s_cpu_prev[i].handle == h) {
            *found = true;
            return s_cpu_prev[i].runtime;
        }
    }
    *found = false;
    return 0;
}

static void cpu_report_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CPU_REPORT_PERIOD_MS));

        UBaseType_t cap = uxTaskGetNumberOfTasks() + 4;
        TaskStatus_t *st = malloc(cap * sizeof(TaskStatus_t));
        if (!st) continue;

        uint32_t total = 0;
        UBaseType_t n = uxTaskGetSystemState(st, cap, &total);
        if (n == 0) { free(st); continue; }

        if (s_cpu_prev_total != 0) {
            // Unsigned math stays correct across the 32-bit us-counter wrap.
            // pulTotalRunTime is the raw esp_timer count (wall clock), NOT the
            // sum over cores — a task pinned to one core at full load already
            // reads 100% of it, so it is the per-core base as-is.
            uint32_t total_delta = total - s_cpu_prev_total;
            uint32_t core_base   = total_delta;
            if (core_base > 0) {
                uint32_t delta[CPU_REPORT_MAX_TASKS] = {0};
                for (UBaseType_t i = 0; i < n && i < CPU_REPORT_MAX_TASKS; i++) {
                    bool found;
                    uint32_t prev = cpu_prev_runtime(st[i].xHandle, &found);
                    // Tasks born this period have no baseline — skip once.
                    delta[i] = found ? (st[i].ulRunTimeCounter - prev) : 0;
                }

                char line[256];
                int  pos = 0;
                for (int k = 0; k < CPU_REPORT_TOP; k++) {
                    int best = -1;
                    for (UBaseType_t i = 0; i < n && i < CPU_REPORT_MAX_TASKS; i++) {
                        if (delta[i] > 0 && (best < 0 || delta[i] > delta[best])) {
                            best = (int)i;
                        }
                    }
                    if (best < 0) break;

                    unsigned pct = (unsigned)(((uint64_t)delta[best] * 100) / core_base);
                    // TaskStatus_t.xCoreID needs CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
                    // (off in this project) — query the affinity per handle instead.
                    BaseType_t core = xTaskGetCoreID(st[best].xHandle);
                    char corec = (core == 0) ? '0' : (core == 1) ? '1' : '*';
                    int w = snprintf(line + pos, sizeof(line) - pos, "%s[%c] %u%%  ",
                                     st[best].pcTaskName, corec, pct);
                    if (w < 0 || w >= (int)(sizeof(line) - pos)) break;
                    pos += w;
                    delta[best] = 0;   // consumed — exclude from next pick
                }
                if (pos > 0) ESP_LOGW("CPU", "%s", line);
            }
        }

        s_cpu_prev_count = (n < CPU_REPORT_MAX_TASKS) ? (int)n : CPU_REPORT_MAX_TASKS;
        for (int i = 0; i < s_cpu_prev_count; i++) {
            s_cpu_prev[i].handle  = st[i].xHandle;
            s_cpu_prev[i].runtime = st[i].ulRunTimeCounter;
        }
        s_cpu_prev_total = total;

        free(st);
    }
}

void cpu_report_start(void)
{
    // Diagnostic-only observer: PSRAM stack (off the audio hot path), prio 1 so
    // it never competes with audio/UI — it only needs to run between samples.
    xTaskCreatePinnedToCoreWithCaps(cpu_report_task, "cpu_report", 4096, NULL, 1,
                                    NULL, tskNO_AFFINITY, MALLOC_CAP_SPIRAM);
}
