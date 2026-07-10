#include "ui_timefmt.h"
#include "settings.h"
#include <stdio.h>

const char *ui_format_time(char *buf, size_t buf_len, const struct tm *t)
{
    if (!settings_get()->display.time_ampm) {
        snprintf(buf, buf_len, "%02d:%02d", t->tm_hour, t->tm_min);
        return "";
    }
    int h = t->tm_hour % 12;
    if (h == 0) h = 12;                      // 0:xx → 12:xx AM, 12:xx → 12:xx PM
    snprintf(buf, buf_len, "%d:%02d", h, t->tm_min);
    return (t->tm_hour < 12) ? "AM" : "PM";
}
