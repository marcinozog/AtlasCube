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

#ifdef __cplusplus
}
#endif
