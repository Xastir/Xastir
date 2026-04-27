/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * Unit tests for list_controller.c — the presentation-free kernel
 * extracted from list_gui.c.
 *
 * Usage: test_list_controller <test-name>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "tests/test_framework.h"
#include "list_controller.h"

/* ------------------------------------------------------------------ */
/* list_controller_init                                                 */
/* ------------------------------------------------------------------ */

int test_init_defaults(void)
{
    list_controller_t lc;
    memset(&lc, 0xFF, sizeof(lc));
    list_controller_init(&lc);
    TEST_ASSERT(lc.english_units == 0, "english_units defaults to 0 (metric)");
    TEST_PASS("list_controller_init: defaults to metric");
}

int test_init_null_safe(void)
{
    list_controller_init(NULL); /* must not crash */
    TEST_PASS("list_controller_init: NULL is safe");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_pos_time                                      */
/* ------------------------------------------------------------------ */

int test_format_pos_time_valid(void)
{
    /* "04262026123456" — 14 chars
     * [0,1]="04" month, [2,3]="26" day, [8,9]="12" hour, [10,11]="34" min */
    char buf[20];
    int r = list_controller_format_pos_time("04262026123456", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1 for valid input");
    TEST_ASSERT_STR_EQ("04/26 12:34", buf, "formats MM/DD HH:MM correctly");
    TEST_PASS("format_pos_time: valid 14-char string formats correctly");
}

int test_format_pos_time_short(void)
{
    char buf[20];
    int r = list_controller_format_pos_time("0426", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "returns 0 for short input");
    TEST_ASSERT_STR_EQ(" ", buf, "short input produces single space");
    TEST_PASS("format_pos_time: short string produces space and returns 0");
}

int test_format_pos_time_null(void)
{
    char buf[20];
    buf[0] = 'X';
    int r = list_controller_format_pos_time(NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "returns 0 for NULL input");
    TEST_PASS("format_pos_time: NULL input returns 0");
}

int test_format_pos_time_null_buf(void)
{
    int r = list_controller_format_pos_time("04262026123456", NULL, 0);
    TEST_ASSERT(r == 0, "returns 0 for NULL buf");
    TEST_PASS("format_pos_time: NULL buf returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_speed  (knots)                               */
/* ------------------------------------------------------------------ */

int test_format_speed_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_speed(&lc, "10", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 10 knots * 1.852 = 18.52 km/h */
    double v = atof(buf);
    TEST_ASSERT(v > 18.4 && v < 18.7, "10 knots -> ~18.5 km/h (metric)");
    TEST_PASS("format_speed: 10 knots -> km/h (metric)");
}

int test_format_speed_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_speed(&lc, "10", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 10 knots * 1.1508 = 11.508 mph */
    double v = atof(buf);
    TEST_ASSERT(v > 11.4 && v < 11.6, "10 knots -> ~11.5 mph (imperial)");
    TEST_PASS("format_speed: 10 knots -> mph (imperial)");
}

int test_format_speed_empty_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_speed(&lc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_speed: empty input returns 0");
}

int test_format_speed_null_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_speed(&lc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL input returns 0");
    TEST_PASS("format_speed: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_altitude  (metres)                           */
/* ------------------------------------------------------------------ */

int test_format_altitude_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_altitude(&lc, "100", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("100", buf, "metric: verbatim");
    TEST_PASS("format_altitude: 100m verbatim (metric)");
}

int test_format_altitude_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_altitude(&lc, "100", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 100m * 3.28084 = 328.084 ft */
    double v = atof(buf);
    TEST_ASSERT(v > 328.0 && v < 328.2, "100m -> ~328.1 ft (imperial)");
    TEST_PASS("format_altitude: 100m -> ~328.1ft (imperial)");
}

int test_format_altitude_empty_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_altitude(&lc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_altitude: empty input returns 0");
}

int test_format_altitude_null_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_altitude(&lc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL input returns 0");
    TEST_PASS("format_altitude: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_distance  (nautical miles)                   */
/* ------------------------------------------------------------------ */

int test_format_distance_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_distance(&lc, 10.0, buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 10 NM * 1.852 = 18.52 km */
    double v = atof(buf);
    TEST_ASSERT(v > 18.4 && v < 18.6, "10 NM -> ~18.5 km (metric)");
    TEST_PASS("format_distance: 10 NM -> km (metric)");
}

int test_format_distance_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_distance(&lc, 10.0, buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 10 NM * 1.15078 = 11.5078 miles */
    double v = atof(buf);
    TEST_ASSERT(v > 11.4 && v < 11.6, "10 NM -> ~11.5 miles (imperial)");
    TEST_PASS("format_distance: 10 NM -> miles (imperial)");
}

int test_format_distance_null_buf_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    int r = list_controller_format_distance(&lc, 10.0, NULL, 0);
    TEST_ASSERT(r == 0, "NULL buf returns 0");
    TEST_PASS("format_distance: NULL buf returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_wx_wind  (mph)                               */
/* ------------------------------------------------------------------ */

int test_format_wx_wind_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_wx_wind(&lc, "62", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 62 mph * 1.6094 = 99.78 -> (int)99 km/h */
    int v = atoi(buf);
    TEST_ASSERT(v == 99, "62 mph -> 99 km/h (metric)");
    TEST_PASS("format_wx_wind: 62 mph -> 99 km/h (metric)");
}

int test_format_wx_wind_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_wx_wind(&lc, "55", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("55", buf, "imperial: verbatim");
    TEST_PASS("format_wx_wind: 55 mph verbatim (imperial)");
}

int test_format_wx_wind_empty_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_wind(&lc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_wx_wind: empty input returns 0");
}

int test_format_wx_wind_null_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_wind(&lc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL input returns 0");
    TEST_PASS("format_wx_wind: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_wx_temp  (°F)                                */
/* ------------------------------------------------------------------ */

int test_format_wx_temp_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_wx_temp(&lc, "32", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("0", buf, "32F -> 0C");
    TEST_PASS("format_wx_temp: 32F -> 0C (metric)");
}

int test_format_wx_temp_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_wx_temp(&lc, "72", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("72", buf, "imperial: verbatim");
    TEST_PASS("format_wx_temp: 72F verbatim (imperial)");
}

int test_format_wx_temp_freezing_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    list_controller_format_wx_temp(&lc, "212", buf, sizeof(buf));
    TEST_ASSERT_STR_EQ("100", buf, "212F -> 100C");
    TEST_PASS("format_wx_temp: 212F -> 100C (metric)");
}

int test_format_wx_temp_empty_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_temp(&lc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_wx_temp: empty input returns 0");
}

int test_format_wx_temp_null_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_temp(&lc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL input returns 0");
    TEST_PASS("format_wx_temp: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_wx_baro  (hPa)                               */
/* ------------------------------------------------------------------ */

int test_format_wx_baro_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_wx_baro(&lc, "1013.2", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("1013.2", buf, "metric: verbatim");
    TEST_PASS("format_wx_baro: 1013.2 hPa verbatim (metric)");
}

int test_format_wx_baro_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_wx_baro(&lc, "1013.2", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 1013.2 * 0.02953 = 29.92 inHg */
    double v = atof(buf);
    TEST_ASSERT(v > 29.9 && v < 30.0, "1013.2 hPa -> ~29.92 inHg");
    TEST_PASS("format_wx_baro: 1013.2 hPa -> ~29.92 inHg (imperial)");
}

int test_format_wx_baro_empty_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_baro(&lc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_wx_baro: empty input returns 0");
}

int test_format_wx_baro_null_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_baro(&lc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL input returns 0");
    TEST_PASS("format_wx_baro: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_format_wx_rain  (hundredths of inch)                */
/* ------------------------------------------------------------------ */

int test_format_wx_rain_metric(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 0;
    char buf[20];
    int r = list_controller_format_wx_rain(&lc, "100", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 100 hundredths-in * 0.254 = 25.40 mm */
    double v = atof(buf);
    TEST_ASSERT(v > 25.3 && v < 25.5, "100 hundredths-in -> 25.40 mm");
    TEST_PASS("format_wx_rain: 100 -> 25.40 mm (metric)");
}

int test_format_wx_rain_imperial(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    lc.english_units = 1;
    char buf[20];
    int r = list_controller_format_wx_rain(&lc, "100", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    /* 100 hundredths-in / 100 = 1.00 inches */
    double v = atof(buf);
    TEST_ASSERT(v > 0.99 && v < 1.01, "100 hundredths-in -> 1.00 inch");
    TEST_PASS("format_wx_rain: 100 -> 1.00 inch (imperial)");
}

int test_format_wx_rain_empty_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_rain(&lc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_wx_rain: empty input returns 0");
}

int test_format_wx_rain_null_returns_zero(void)
{
    list_controller_t lc;
    list_controller_init(&lc);
    char buf[20];
    int r = list_controller_format_wx_rain(&lc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL input returns 0");
    TEST_PASS("format_wx_rain: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* list_controller_needs_update                                         */
/* ------------------------------------------------------------------ */

int test_needs_update_redo_and_elapsed(void)
{
    /* redo=1, 5s elapsed, cycle=2 → update needed */
    time_t now = 1000;
    time_t last = 994;
    int r = list_controller_needs_update(1, last, now, 2,
                                         400, 400, 700, 700, 0, 0);
    TEST_ASSERT(r == 1, "redo=1 + elapsed > cycle -> needs update");
    TEST_PASS("needs_update: redo + elapsed triggers update");
}

int test_needs_update_redo_insufficient_time(void)
{
    /* redo=1, only 1s elapsed, cycle=2 → no update */
    time_t now = 1000;
    time_t last = 999;
    int r = list_controller_needs_update(1, last, now, 2,
                                         400, 400, 700, 700, 0, 0);
    TEST_ASSERT(r == 0, "redo=1 but elapsed < cycle -> no update");
    TEST_PASS("needs_update: redo but insufficient elapsed -> no update");
}

int test_needs_update_height_changed(void)
{
    time_t now = 1000;
    time_t last = 1000;
    int r = list_controller_needs_update(0, last, now, 2,
                                         500, 400, 700, 700, 0, 0);
    TEST_ASSERT(r == 1, "height change -> needs update");
    TEST_PASS("needs_update: height change triggers update");
}

int test_needs_update_width_changed(void)
{
    time_t now = 1000;
    time_t last = 1000;
    int r = list_controller_needs_update(0, last, now, 2,
                                         400, 400, 800, 700, 0, 0);
    TEST_ASSERT(r == 1, "width change -> needs update");
    TEST_PASS("needs_update: width change triggers update");
}

int test_needs_update_units_changed(void)
{
    time_t now = 1000;
    time_t last = 1000;
    int r = list_controller_needs_update(0, last, now, 2,
                                         400, 400, 700, 700, 1, 0);
    TEST_ASSERT(r == 1, "units change -> needs update");
    TEST_PASS("needs_update: units change triggers update");
}

int test_needs_update_nothing_changed(void)
{
    time_t now = 1000;
    time_t last = 1000;
    int r = list_controller_needs_update(0, last, now, 2,
                                         400, 400, 700, 700, 0, 0);
    TEST_ASSERT(r == 0, "nothing changed -> no update");
    TEST_PASS("needs_update: no change -> no update needed");
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <test-name>\n", argv[0]);
        return 1;
    }

    const char *test = argv[1];

    if (!strcmp(test, "init_defaults"))             return test_init_defaults();
    if (!strcmp(test, "init_null_safe"))             return test_init_null_safe();
    if (!strcmp(test, "format_pos_time_valid"))      return test_format_pos_time_valid();
    if (!strcmp(test, "format_pos_time_short"))      return test_format_pos_time_short();
    if (!strcmp(test, "format_pos_time_null"))       return test_format_pos_time_null();
    if (!strcmp(test, "format_pos_time_null_buf"))   return test_format_pos_time_null_buf();
    if (!strcmp(test, "format_speed_metric"))        return test_format_speed_metric();
    if (!strcmp(test, "format_speed_imperial"))      return test_format_speed_imperial();
    if (!strcmp(test, "format_speed_empty_returns_zero"))  return test_format_speed_empty_returns_zero();
    if (!strcmp(test, "format_speed_null_returns_zero"))   return test_format_speed_null_returns_zero();
    if (!strcmp(test, "format_altitude_metric"))     return test_format_altitude_metric();
    if (!strcmp(test, "format_altitude_imperial"))   return test_format_altitude_imperial();
    if (!strcmp(test, "format_altitude_empty_returns_zero")) return test_format_altitude_empty_returns_zero();
    if (!strcmp(test, "format_altitude_null_returns_zero"))  return test_format_altitude_null_returns_zero();
    if (!strcmp(test, "format_distance_metric"))     return test_format_distance_metric();
    if (!strcmp(test, "format_distance_imperial"))   return test_format_distance_imperial();
    if (!strcmp(test, "format_distance_null_buf_returns_zero")) return test_format_distance_null_buf_returns_zero();
    if (!strcmp(test, "format_wx_wind_metric"))      return test_format_wx_wind_metric();
    if (!strcmp(test, "format_wx_wind_imperial"))    return test_format_wx_wind_imperial();
    if (!strcmp(test, "format_wx_wind_empty_returns_zero")) return test_format_wx_wind_empty_returns_zero();
    if (!strcmp(test, "format_wx_wind_null_returns_zero"))  return test_format_wx_wind_null_returns_zero();
    if (!strcmp(test, "format_wx_temp_metric"))      return test_format_wx_temp_metric();
    if (!strcmp(test, "format_wx_temp_imperial"))    return test_format_wx_temp_imperial();
    if (!strcmp(test, "format_wx_temp_freezing_metric")) return test_format_wx_temp_freezing_metric();
    if (!strcmp(test, "format_wx_temp_empty_returns_zero")) return test_format_wx_temp_empty_returns_zero();
    if (!strcmp(test, "format_wx_temp_null_returns_zero"))  return test_format_wx_temp_null_returns_zero();
    if (!strcmp(test, "format_wx_baro_metric"))      return test_format_wx_baro_metric();
    if (!strcmp(test, "format_wx_baro_imperial"))    return test_format_wx_baro_imperial();
    if (!strcmp(test, "format_wx_baro_empty_returns_zero")) return test_format_wx_baro_empty_returns_zero();
    if (!strcmp(test, "format_wx_baro_null_returns_zero"))  return test_format_wx_baro_null_returns_zero();
    if (!strcmp(test, "format_wx_rain_metric"))      return test_format_wx_rain_metric();
    if (!strcmp(test, "format_wx_rain_imperial"))    return test_format_wx_rain_imperial();
    if (!strcmp(test, "format_wx_rain_empty_returns_zero")) return test_format_wx_rain_empty_returns_zero();
    if (!strcmp(test, "format_wx_rain_null_returns_zero"))  return test_format_wx_rain_null_returns_zero();
    if (!strcmp(test, "needs_update_redo_and_elapsed"))  return test_needs_update_redo_and_elapsed();
    if (!strcmp(test, "needs_update_redo_insufficient_time")) return test_needs_update_redo_insufficient_time();
    if (!strcmp(test, "needs_update_height_changed")) return test_needs_update_height_changed();
    if (!strcmp(test, "needs_update_width_changed"))  return test_needs_update_width_changed();
    if (!strcmp(test, "needs_update_units_changed"))  return test_needs_update_units_changed();
    if (!strcmp(test, "needs_update_nothing_changed")) return test_needs_update_nothing_changed();

    fprintf(stderr, "Unknown test: %s\n", test);
    return 1;
}
