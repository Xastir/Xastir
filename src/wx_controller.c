/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * wx_controller.c - Pure-C controller for wx_gui.c
 *
 * No Motif/X11 headers are included here; all logic is pure C.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "wx_controller.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void wx_controller_init(wx_controller_t *wc)
{
    if (!wc)
        return;
    wc->english_units = 0;
}

int wx_controller_format_temp(const wx_controller_t *wc,
                              const char *raw_f,
                              char *buf, int buflen)
{
    if (!wc || !buf || buflen < 1)
        return 0;
    if (!raw_f || raw_f[0] == '\0')
        return 0;

    if (!wc->english_units)
    {
        /* Fahrenheit → Celsius */
        int c = (int)(((atof(raw_f) - 32.0) * 5.0) / 9.0);
        snprintf(buf, (size_t)buflen, "%03d", c);
    }
    else
    {
        snprintf(buf, (size_t)buflen, "%s", raw_f);
    }
    return 1;
}

int wx_controller_format_speed(const wx_controller_t *wc,
                               const char *raw_mph,
                               char *buf, int buflen)
{
    if (!wc || !buf || buflen < 1)
        return 0;
    if (!raw_mph || raw_mph[0] == '\0')
        return 0;

    if (!wc->english_units)
    {
        /* mph → km/h */
        int kph = (int)(atof(raw_mph) * 1.6094);
        snprintf(buf, (size_t)buflen, "%03d", kph);
    }
    else
    {
        snprintf(buf, (size_t)buflen, "%s", raw_mph);
    }
    return 1;
}

int wx_controller_format_rain(const wx_controller_t *wc,
                              const char *raw_hundredths,
                              char *buf, int buflen)
{
    if (!wc || !buf || buflen < 1)
        return 0;
    if (!raw_hundredths || raw_hundredths[0] == '\0')
        return 0;

    if (!wc->english_units)
    {
        /* hundredths of inches → mm */
        double mm = atof(raw_hundredths) * 0.254;
        snprintf(buf, (size_t)buflen, "%0.2f", mm);
    }
    else
    {
        /* hundredths of inches → decimal inches */
        double in = atof(raw_hundredths) / 100.0;
        snprintf(buf, (size_t)buflen, "%0.2f", in);
    }
    return 1;
}

int wx_controller_format_baro(const wx_controller_t *wc,
                              const char *raw_hpa,
                              char *buf, int buflen)
{
    if (!wc || !buf || buflen < 1)
        return 0;
    if (!raw_hpa || raw_hpa[0] == '\0')
        return 0;

    if (!wc->english_units)
    {
        /* Metric: display hPa verbatim */
        snprintf(buf, (size_t)buflen, "%s", raw_hpa);
    }
    else
    {
        /* hPa → inches of mercury */
        double inhg = atof(raw_hpa) * 0.02953;
        snprintf(buf, (size_t)buflen, "%0.2f", inhg);
    }
    return 1;
}

int wx_controller_extract_alert_handle(const char *selection,
                                       char *handle_buf,
                                       int handle_buflen)
{
    char *ptr;
    int len;

    if (!selection || !handle_buf || handle_buflen < 10)
        return 0;

    /* Copy first 13 characters */
    snprintf(handle_buf, (size_t)handle_buflen < 14 ? (size_t)handle_buflen : 14,
             "%s", selection);
    handle_buf[13 < handle_buflen - 1 ? 13 : handle_buflen - 1] = '\0';

    /* Remove all spaces */
    ptr = handle_buf;
    while ((ptr = strchr(handle_buf, ' ')) != NULL)
    {
        memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
    }

    /* Truncate to 9 characters */
    len = (int)strlen(handle_buf);
    if (len > 9)
        handle_buf[9] = '\0';

    return (handle_buf[0] != '\0') ? 1 : 0;
}
