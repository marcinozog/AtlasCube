#pragma once

#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Format the wall-clock time into buf honouring settings.display.time_ampm.
 * 24-hour mode: "HH:MM", returns "". 12-hour mode: "h:MM" (no leading zero),
 * returns "AM" or "PM". The suffix is returned separately because the large
 * clock fonts are digit-only (range 0x30-0x3A) and cannot render letters —
 * draw it as its own label with a text-capable font.
 */
const char *ui_format_time(char *buf, size_t buf_len, const struct tm *t);

/**
 * Format the date into buf honouring settings.display.date_mdy:
 * "YYYY-MM-DD" (default) or "MM/DD/YYYY". Day-of-week is not included —
 * callers that want it prepend it themselves.
 */
void ui_format_date(char *buf, size_t buf_len, const struct tm *t);

#ifdef __cplusplus
}
#endif
