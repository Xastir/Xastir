/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * list_controller.h - Pure-C controller for list_gui.c
 *
 * Holds all non-Motif unit-conversion and string-formatting logic for
 * the Station List dialogs (All, Mobile, WX, Local, Time, Objects).
 * No Motif/X11 headers are included here.
 */

#ifndef LIST_CONTROLLER_H
#define LIST_CONTROLLER_H

#include <time.h>

/*
 * list_controller_t — holds the units mode so all conversion functions
 * can be tested with both metric and imperial without touching globals.
 */
typedef struct {
    int english_units;  /* 0 = metric, 1 = imperial */
} list_controller_t;

/* Initialise struct to safe defaults.  NULL-safe. */
void list_controller_init(list_controller_t *lc);

/*
 * Format the pos_time field for display in the station list.
 *
 * APRS stores pos_time as a 14+ character string; the list shows
 * "MM/DD HH:MM" by extracting fixed character positions.
 *
 * raw:       station->pos_time string (may be NULL or short).
 * buf/buflen: output buffer.
 * Returns 1 if raw is long enough and buf was filled; 0 otherwise.
 * A short/empty raw produces a single space in buf and returns 0.
 */
int list_controller_format_pos_time(const char *raw,
                                    char *buf, int buflen);

/*
 * Mobile-list speed conversion.  APRS stores speed in knots.
 *
 * raw_knots: raw speed string in knots.
 * If metric  (english_units==0): convert to km/h  (× 1.852),  "%.1f".
 * If imperial(english_units==1): convert to mph   (× 1.1508), "%.1f".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 */
int list_controller_format_speed(const list_controller_t *lc,
                                 const char *raw_knots,
                                 char *buf, int buflen);

/*
 * Mobile-list altitude conversion.  APRS stores altitude in metres.
 *
 * raw_m: raw altitude string in metres.
 * If metric  (english_units==0): copy verbatim.
 * If imperial(english_units==1): convert to feet (× 3.28084), "%.1f".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 */
int list_controller_format_altitude(const list_controller_t *lc,
                                    const char *raw_m,
                                    char *buf, int buflen);

/*
 * Mobile-list distance conversion.  calc_distance_course() returns
 * nautical miles.
 *
 * nm_val: distance in nautical miles (floating point).
 * If metric  (english_units==0): convert to km    (× 1.852),  "%0.1f".
 * If imperial(english_units==1): convert to miles (× 1.15078),"%0.1f".
 * buf/buflen: output buffer.
 * Returns 1 always (value is always valid).
 */
int list_controller_format_distance(const list_controller_t *lc,
                                    double nm_val,
                                    char *buf, int buflen);

/*
 * WX-list wind speed conversion.  wx_speed is in mph.
 *
 * raw_mph: raw string in miles per hour.
 * If metric  (english_units==0): convert to km/h (× 1.6094), "%d".
 * If imperial(english_units==1): copy verbatim as integer,    "%d".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 */
int list_controller_format_wx_wind(const list_controller_t *lc,
                                   const char *raw_mph,
                                   char *buf, int buflen);

/*
 * WX-list temperature conversion.  wx_temp is in degrees Fahrenheit.
 *
 * raw_f: raw string in degrees Fahrenheit.
 * If metric  (english_units==0): convert to Celsius ((F-32)*5/9), "%d".
 * If imperial(english_units==1): copy verbatim as integer,         "%d".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 */
int list_controller_format_wx_temp(const list_controller_t *lc,
                                   const char *raw_f,
                                   char *buf, int buflen);

/*
 * WX-list barometric pressure conversion.  wx_baro is in hPa.
 *
 * raw_hpa: raw string in hectopascals.
 * If metric  (english_units==0): copy verbatim (hPa display).
 * If imperial(english_units==1): convert to inHg (× 0.02953), "%0.2f".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 */
int list_controller_format_wx_baro(const list_controller_t *lc,
                                   const char *raw_hpa,
                                   char *buf, int buflen);

/*
 * WX-list rain conversion.  APRS encodes rain as hundredths of an inch.
 *
 * raw_hundredths: raw string in hundredths of inches.
 * If metric  (english_units==0): convert to mm (× 0.254),    "%0.2f".
 * If imperial(english_units==1): convert to inches (÷ 100),  "%0.2f".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 */
int list_controller_format_wx_rain(const list_controller_t *lc,
                                   const char *raw_hundredths,
                                   char *buf, int buflen);

/*
 * Decide whether the station list needs to be redrawn.
 *
 * Returns 1 if any of these conditions hold:
 *   - redo_list is non-zero AND (now - last_update) >= update_cycle seconds
 *   - cur_h != prev_h  (dialog height changed)
 *   - cur_w != prev_w  (dialog width changed)
 *   - english_units != units_last  (unit system toggled)
 * Returns 0 otherwise.
 */
int list_controller_needs_update(int redo_list,
                                 time_t last_update, time_t now,
                                 int update_cycle,
                                 int cur_h, int prev_h,
                                 int cur_w, int prev_w,
                                 int english_units, int units_last);

#endif /* LIST_CONTROLLER_H */
