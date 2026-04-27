/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * list_controller.c - Pure-C controller for list_gui.c
 *
 * All non-Motif unit-conversion and string-formatting logic extracted
 * from the Station List dialogs.  No Motif/X11 headers included.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list_controller.h"

/* ------------------------------------------------------------------ */

void list_controller_init(list_controller_t *lc)
{
    if (!lc) return;
    lc->english_units = 0;  /* default: metric */
}

/* ------------------------------------------------------------------ */

int list_controller_format_pos_time(const char *raw,
                                    char *buf, int buflen)
{
    if (!buf || buflen < 1) return 0;
    if (!raw || strlen(raw) <= 13) {
        snprintf(buf, buflen, " ");
        return 0;
    }
    /* Extract "MM/DD HH:MM" from positions 0-3 and 8-11. */
    snprintf(buf, buflen, "%c%c/%c%c %c%c:%c%c",
             raw[0], raw[1],   /* month */
             raw[2], raw[3],   /* day   */
             raw[8], raw[9],   /* hour  */
             raw[10], raw[11]  /* min   */
            );
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_speed(const list_controller_t *lc,
                                 const char *raw_knots,
                                 char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;
    if (!raw_knots || raw_knots[0] == '\0') return 0;

    double knots = atof(raw_knots);
    if (lc->english_units == 0) {
        /* knots → km/h */
        snprintf(buf, buflen, "%.1f", knots * 1.852);
    } else {
        /* knots → mph */
        snprintf(buf, buflen, "%.1f", knots * 1.1508);
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_altitude(const list_controller_t *lc,
                                    const char *raw_m,
                                    char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;
    if (!raw_m || raw_m[0] == '\0') return 0;

    if (lc->english_units == 0) {
        /* metres verbatim */
        snprintf(buf, buflen, "%s", raw_m);
    } else {
        /* metres → feet */
        snprintf(buf, buflen, "%.1f", atof(raw_m) * 3.28084);
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_distance(const list_controller_t *lc,
                                    double nm_val,
                                    char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;

    if (lc->english_units == 0) {
        /* NM → km */
        snprintf(buf, buflen, "%0.1f", nm_val * 1.852);
    } else {
        /* NM → miles */
        snprintf(buf, buflen, "%0.1f", nm_val * 1.15078);
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_wx_wind(const list_controller_t *lc,
                                   const char *raw_mph,
                                   char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;
    if (!raw_mph || raw_mph[0] == '\0') return 0;

    if (lc->english_units == 0) {
        /* mph → km/h */
        snprintf(buf, buflen, "%d", (int)(atof(raw_mph) * 1.6094));
    } else {
        /* mph verbatim */
        snprintf(buf, buflen, "%d", atoi(raw_mph));
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_wx_temp(const list_controller_t *lc,
                                   const char *raw_f,
                                   char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;
    if (!raw_f || raw_f[0] == '\0') return 0;

    if (lc->english_units == 0) {
        /* °F → °C */
        snprintf(buf, buflen, "%d",
                 (int)(((atof(raw_f) - 32.0) * 5.0) / 9.0));
    } else {
        /* °F verbatim */
        snprintf(buf, buflen, "%d", atoi(raw_f));
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_wx_baro(const list_controller_t *lc,
                                   const char *raw_hpa,
                                   char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;
    if (!raw_hpa || raw_hpa[0] == '\0') return 0;

    if (lc->english_units == 0) {
        /* hPa verbatim */
        snprintf(buf, buflen, "%s", raw_hpa);
    } else {
        /* hPa → inHg */
        snprintf(buf, buflen, "%0.2f", atof(raw_hpa) * 0.02953);
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_format_wx_rain(const list_controller_t *lc,
                                   const char *raw_hundredths,
                                   char *buf, int buflen)
{
    if (!lc || !buf || buflen < 1) return 0;
    if (!raw_hundredths || raw_hundredths[0] == '\0') return 0;

    if (lc->english_units == 0) {
        /* hundredths-of-inch → mm */
        snprintf(buf, buflen, "%0.2f", atof(raw_hundredths) * 0.254);
    } else {
        /* hundredths-of-inch → inches */
        snprintf(buf, buflen, "%0.2f", atof(raw_hundredths) / 100.0);
    }
    return 1;
}

/* ------------------------------------------------------------------ */

int list_controller_needs_update(int redo_list,
                                 time_t last_update, time_t now,
                                 int update_cycle,
                                 int cur_h, int prev_h,
                                 int cur_w, int prev_w,
                                 int english_units, int units_last)
{
    if (redo_list && ((now - last_update) >= update_cycle)) return 1;
    if (cur_h != prev_h) return 1;
    if (cur_w != prev_w) return 1;
    if (english_units != units_last) return 1;
    return 0;
}
