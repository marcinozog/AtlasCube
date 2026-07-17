#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*
void heap_report(const char *where)
Diagnostic: logs the internal-RAM picture at a named point — total free, the
largest contiguous block (the real constraint for the TLS handshake and httpd
pbufs), the DMA-capable largest block (WiFi/LCD), and the min-ever free. One log
line; left in as a lightweight ongoing health check of internal RAM.
*/
void heap_report(const char *where);

/*
void cpu_report_start(void)
Diagnostic: spawns a prio-1 task that every 5 s logs the top tasks by CPU share
over the last period (delta-based, per-core % with a [core] suffix, '*' =
unpinned). Used to attribute core-1 saturation during heavy streams (48 kHz
HLS). Needs CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS + USE_TRACE_FACILITY.
*/
void cpu_report_start(void);

#ifdef __cplusplus
}
#endif
