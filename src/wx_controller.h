/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * wx_controller.h - Pure-C controller for wx_gui.c
 *
 * Holds all non-Motif unit-conversion and string-formatting logic for
 * the WX Station display and Weather Alert dialogs.
 * No Motif/X11 headers are included here.
 */

#ifndef WX_CONTROLLER_H
#define WX_CONTROLLER_H

/*
 * wx_controller_t — holds the units mode so all conversion functions
 * can be tested with both metric and imperial without touching globals.
 */
typedef struct {
    int english_units;  /* 0 = metric, 1 = imperial */
} wx_controller_t;

/* Initialise struct to safe defaults.  NULL-safe. */
void wx_controller_init(wx_controller_t *wc);

/*
 * Temperature conversion.
 * raw_f:  raw string value in degrees Fahrenheit (e.g. "72" or "72.5").
 * buf/buflen: output buffer.
 * If english_units == 0 (metric): convert to Celsius, format as "%03d".
 * If english_units == 1 (imperial): copy raw_f verbatim.
 * Returns 1 if raw_f is non-empty and buf was filled, 0 otherwise.
 * Returns 0 if wc or buf is NULL.
 */
int wx_controller_format_temp(const wx_controller_t *wc,
                              const char *raw_f,
                              char *buf, int buflen);

/*
 * Wind speed / gust conversion.
 * raw_mph: raw string in miles per hour.
 * If metric: convert to km/h, format as "%03d".
 * If imperial: copy verbatim.
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 * Returns 0 if wc or buf is NULL.
 */
int wx_controller_format_speed(const wx_controller_t *wc,
                               const char *raw_mph,
                               char *buf, int buflen);

/*
 * Rain conversion.  APRS encodes rain as hundredths of an inch.
 * raw_hundredths: raw string in hundredths of inches.
 * If metric: convert to mm, format as "%0.2f".  (1 hundredth in = 0.254 mm)
 * If imperial: convert to decimal inches, format as "%0.2f".
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 * Returns 0 if wc or buf is NULL.
 */
int wx_controller_format_rain(const wx_controller_t *wc,
                              const char *raw_hundredths,
                              char *buf, int buflen);

/*
 * Barometric pressure conversion.
 * raw_hpa: raw string in hPa (hectopascals).
 * If metric: copy verbatim (hPa display).
 * If imperial: convert to inches of mercury, format as "%0.2f".
 *   (1 hPa = 0.02953 inHg)
 * Returns 1 if non-empty and buf was filled, 0 otherwise.
 * Returns 0 if wc or buf is NULL.
 */
int wx_controller_format_baro(const wx_controller_t *wc,
                              const char *raw_hpa,
                              char *buf, int buflen);

/*
 * Extract the NWS "finger" handle from a selected weather alert list item.
 *
 * The alert list rows are formatted as:
 *   "%-9s %-5s %-9s ..."  (first field = alert->from, padded to 9 chars)
 *
 * This function:
 *   1. Copies the first 13 characters of selection into handle_buf.
 *   2. Removes all space characters (in-place).
 *   3. Truncates to 9 characters.
 *
 * Returns 1 if handle_buf is non-empty after processing, 0 otherwise.
 * Returns 0 if selection or handle_buf is NULL, or handle_buflen < 10.
 */
int wx_controller_extract_alert_handle(const char *selection,
                                       char *handle_buf,
                                       int handle_buflen);

#endif /* WX_CONTROLLER_H */
