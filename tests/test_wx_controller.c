/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * Unit tests for wx_controller.c — the presentation-free kernel
 * extracted from wx_gui.c.
 *
 * Usage: test_wx_controller <test-name>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "tests/test_framework.h"
#include "wx_controller.h"

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Approximate float equality (2 decimal places). */
static int feq2(const char *s, double expected)
{
    double v = atof(s);
    double diff = v - expected;
    if (diff < 0) diff = -diff;
    return diff < 0.005;
}

/* ------------------------------------------------------------------ */
/* wx_controller_init                                                   */
/* ------------------------------------------------------------------ */

int test_init_defaults(void)
{
    wx_controller_t wc;
    memset(&wc, 0xFF, sizeof(wc));
    wx_controller_init(&wc);
    TEST_ASSERT(wc.english_units == 0, "english_units defaults to 0 (metric)");
    TEST_PASS("wx_controller_init: defaults to metric");
}

int test_init_null_safe(void)
{
    wx_controller_init(NULL); /* must not crash */
    TEST_PASS("wx_controller_init: NULL is safe");
}

/* ------------------------------------------------------------------ */
/* wx_controller_format_temp                                            */
/* ------------------------------------------------------------------ */

int test_format_temp_metric(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    int r = wx_controller_format_temp(&wc, "32", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1 for non-empty input");
    TEST_ASSERT_STR_EQ("000", buf, "32F = 000C");
    TEST_PASS("format_temp: 32F -> 000C (metric)");
}

int test_format_temp_metric_negative(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    wx_controller_format_temp(&wc, "14", buf, sizeof(buf));
    /* 14F = (14-32)*5/9 = -10C */
    TEST_ASSERT_STR_EQ("-10", buf, "14F -> -10C");
    TEST_PASS("format_temp: 14F -> -10C (metric)");
}

int test_format_temp_imperial(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 1;
    char buf[20];
    int r = wx_controller_format_temp(&wc, "72", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("72", buf, "imperial: pass through verbatim");
    TEST_PASS("format_temp: 72F verbatim (imperial)");
}

int test_format_temp_empty_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    int r = wx_controller_format_temp(&wc, "", (char[20]){}, 20);
    TEST_ASSERT(r == 0, "empty input returns 0");
    TEST_PASS("format_temp: empty input returns 0");
}

int test_format_temp_null_raw_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_temp(&wc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL raw_f returns 0");
    TEST_PASS("format_temp: NULL raw_f returns 0");
}

int test_format_temp_null_wc_returns_zero(void)
{
    char buf[20];
    int r = wx_controller_format_temp(NULL, "72", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL wc returns 0");
    TEST_PASS("format_temp: NULL wc returns 0");
}

int test_format_temp_null_buf_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    int r = wx_controller_format_temp(&wc, "72", NULL, 20);
    TEST_ASSERT(r == 0, "NULL buf returns 0");
    TEST_PASS("format_temp: NULL buf returns 0");
}

int test_format_temp_boiling_metric(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    wx_controller_format_temp(&wc, "212", buf, sizeof(buf));
    /* 212F = 100C */
    TEST_ASSERT_STR_EQ("100", buf, "212F -> 100C");
    TEST_PASS("format_temp: 212F -> 100C (metric)");
}

/* ------------------------------------------------------------------ */
/* wx_controller_format_speed                                           */
/* ------------------------------------------------------------------ */

int test_format_speed_metric(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    int r = wx_controller_format_speed(&wc, "0", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("000", buf, "0 mph -> 000 km/h");
    TEST_PASS("format_speed: 0 mph -> 000 km/h (metric)");
}

int test_format_speed_metric_conversion(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    wx_controller_format_speed(&wc, "62", buf, sizeof(buf));
    /* 62 mph * 1.6094 = 99.78 km/h -> (int) = 99 */
    int v = atoi(buf);
    TEST_ASSERT(v == 99, "62 mph -> 099 km/h (truncated)");
    TEST_PASS("format_speed: 62 mph -> 099 km/h (metric)");
}

int test_format_speed_imperial(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 1;
    char buf[20];
    int r = wx_controller_format_speed(&wc, "55", buf, sizeof(buf));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("55", buf, "imperial: pass through");
    TEST_PASS("format_speed: 55 mph verbatim (imperial)");
}

int test_format_speed_empty_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_speed(&wc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty returns 0");
    TEST_PASS("format_speed: empty input returns 0");
}

int test_format_speed_null_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_speed(&wc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL returns 0");
    TEST_PASS("format_speed: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* wx_controller_format_rain                                            */
/* ------------------------------------------------------------------ */

int test_format_rain_metric(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    int r = wx_controller_format_rain(&wc, "100", buf, sizeof(buf));
    /* 100 hundredths * 0.254 = 25.40 mm */
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT(feq2(buf, 25.40), "100 hundredths -> 25.40 mm");
    TEST_PASS("format_rain: 100 hundredths -> 25.40 mm (metric)");
}

int test_format_rain_imperial(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 1;
    char buf[20];
    int r = wx_controller_format_rain(&wc, "100", buf, sizeof(buf));
    /* 100 / 100 = 1.00 inches */
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT(feq2(buf, 1.00), "100 hundredths -> 1.00 inch");
    TEST_PASS("format_rain: 100 hundredths -> 1.00 inch (imperial)");
}

int test_format_rain_zero_metric(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    wx_controller_format_rain(&wc, "0", buf, sizeof(buf));
    TEST_ASSERT(feq2(buf, 0.0), "0 hundredths -> 0.00 mm");
    TEST_PASS("format_rain: 0 hundredths -> 0.00 mm");
}

int test_format_rain_empty_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_rain(&wc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty returns 0");
    TEST_PASS("format_rain: empty input returns 0");
}

int test_format_rain_null_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_rain(&wc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL returns 0");
    TEST_PASS("format_rain: NULL input returns 0");
}

/* ------------------------------------------------------------------ */
/* wx_controller_format_baro                                            */
/* ------------------------------------------------------------------ */

int test_format_baro_metric(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 0;
    char buf[20];
    int r = wx_controller_format_baro(&wc, "1013", buf, sizeof(buf));
    /* metric: copy verbatim */
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("1013", buf, "hPa copied verbatim (metric)");
    TEST_PASS("format_baro: 1013 hPa verbatim (metric)");
}

int test_format_baro_imperial(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    wc.english_units = 1;
    char buf[20];
    int r = wx_controller_format_baro(&wc, "1013", buf, sizeof(buf));
    /* 1013 hPa * 0.02953 = 29.93 inHg */
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT(feq2(buf, 29.91), "1013 hPa -> ~29.91 inHg");
    TEST_PASS("format_baro: 1013 hPa -> ~29.91 inHg (imperial)");
}

int test_format_baro_empty_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_baro(&wc, "", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "empty returns 0");
    TEST_PASS("format_baro: empty input returns 0");
}

int test_format_baro_null_returns_zero(void)
{
    wx_controller_t wc;
    wx_controller_init(&wc);
    char buf[20];
    int r = wx_controller_format_baro(&wc, NULL, buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL returns 0");
    TEST_PASS("format_baro: NULL input returns 0");
}

int test_format_baro_null_wc_returns_zero(void)
{
    char buf[20];
    int r = wx_controller_format_baro(NULL, "1013", buf, sizeof(buf));
    TEST_ASSERT(r == 0, "NULL wc returns 0");
    TEST_PASS("format_baro: NULL wc returns 0");
}

/* ------------------------------------------------------------------ */
/* wx_controller_extract_alert_handle                                   */
/* ------------------------------------------------------------------ */

int test_extract_handle_basic(void)
{
    char handle[14];
    /* Typical alert list row: "AFGNPW    NWS-WARN    Until:..." */
    int r = wx_controller_extract_alert_handle(
                "AFGNPW    NWS-WARN  rest...", handle, sizeof(handle));
    TEST_ASSERT(r == 1, "returns 1 for non-empty result");
    /* First 13 chars: "AFGNPW    NWS" -> remove spaces -> "AFGNPWNWS" -> truncate 9 */
    TEST_ASSERT_STR_EQ("AFGNPWNWS", handle, "handle extracted correctly");
    TEST_PASS("extract_alert_handle: basic extraction");
}

int test_extract_handle_short_input(void)
{
    char handle[14];
    /* Input shorter than 13 chars */
    int r = wx_controller_extract_alert_handle("W1AW", handle, sizeof(handle));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("W1AW", handle, "short input preserved");
    TEST_PASS("extract_alert_handle: short input handled");
}

int test_extract_handle_all_spaces_first13(void)
{
    char handle[14];
    int r = wx_controller_extract_alert_handle(
                "             more text", handle, sizeof(handle));
    TEST_ASSERT(r == 0, "returns 0 when result empty after space removal");
    TEST_PASS("extract_alert_handle: all-spaces first13 returns 0");
}

int test_extract_handle_null_selection_returns_zero(void)
{
    char handle[14];
    int r = wx_controller_extract_alert_handle(NULL, handle, sizeof(handle));
    TEST_ASSERT(r == 0, "NULL selection returns 0");
    TEST_PASS("extract_alert_handle: NULL selection returns 0");
}

int test_extract_handle_null_buf_returns_zero(void)
{
    int r = wx_controller_extract_alert_handle("W1AW", NULL, 14);
    TEST_ASSERT(r == 0, "NULL handle_buf returns 0");
    TEST_PASS("extract_alert_handle: NULL handle_buf returns 0");
}

int test_extract_handle_short_buflen_returns_zero(void)
{
    char handle[5];
    /* buflen < 10 is rejected */
    int r = wx_controller_extract_alert_handle("W1AW", handle, sizeof(handle));
    TEST_ASSERT(r == 0, "buflen < 10 returns 0");
    TEST_PASS("extract_alert_handle: short buflen returns 0");
}

int test_extract_handle_no_spaces(void)
{
    char handle[14];
    /* No spaces in first 13 chars: result is first 9 chars */
    int r = wx_controller_extract_alert_handle(
                "ABCDEFGHIJKLM rest", handle, sizeof(handle));
    TEST_ASSERT(r == 1, "returns 1");
    TEST_ASSERT_STR_EQ("ABCDEFGHI", handle, "truncated to 9 chars");
    TEST_PASS("extract_alert_handle: truncated to 9 when no spaces");
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                             */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <test-name>\n", argv[0]);
        return 1;
    }

    const char *test = argv[1];

    if (!strcmp(test, "init_defaults"))               return test_init_defaults();
    if (!strcmp(test, "init_null_safe"))              return test_init_null_safe();
    if (!strcmp(test, "format_temp_metric"))          return test_format_temp_metric();
    if (!strcmp(test, "format_temp_metric_negative")) return test_format_temp_metric_negative();
    if (!strcmp(test, "format_temp_imperial"))        return test_format_temp_imperial();
    if (!strcmp(test, "format_temp_empty_returns_zero")) return test_format_temp_empty_returns_zero();
    if (!strcmp(test, "format_temp_null_raw_returns_zero")) return test_format_temp_null_raw_returns_zero();
    if (!strcmp(test, "format_temp_null_wc_returns_zero")) return test_format_temp_null_wc_returns_zero();
    if (!strcmp(test, "format_temp_null_buf_returns_zero")) return test_format_temp_null_buf_returns_zero();
    if (!strcmp(test, "format_temp_boiling_metric")) return test_format_temp_boiling_metric();
    if (!strcmp(test, "format_speed_metric"))         return test_format_speed_metric();
    if (!strcmp(test, "format_speed_metric_conversion")) return test_format_speed_metric_conversion();
    if (!strcmp(test, "format_speed_imperial"))       return test_format_speed_imperial();
    if (!strcmp(test, "format_speed_empty_returns_zero")) return test_format_speed_empty_returns_zero();
    if (!strcmp(test, "format_speed_null_returns_zero")) return test_format_speed_null_returns_zero();
    if (!strcmp(test, "format_rain_metric"))          return test_format_rain_metric();
    if (!strcmp(test, "format_rain_imperial"))        return test_format_rain_imperial();
    if (!strcmp(test, "format_rain_zero_metric"))     return test_format_rain_zero_metric();
    if (!strcmp(test, "format_rain_empty_returns_zero")) return test_format_rain_empty_returns_zero();
    if (!strcmp(test, "format_rain_null_returns_zero")) return test_format_rain_null_returns_zero();
    if (!strcmp(test, "format_baro_metric"))          return test_format_baro_metric();
    if (!strcmp(test, "format_baro_imperial"))        return test_format_baro_imperial();
    if (!strcmp(test, "format_baro_empty_returns_zero")) return test_format_baro_empty_returns_zero();
    if (!strcmp(test, "format_baro_null_returns_zero")) return test_format_baro_null_returns_zero();
    if (!strcmp(test, "format_baro_null_wc_returns_zero")) return test_format_baro_null_wc_returns_zero();
    if (!strcmp(test, "extract_handle_basic"))        return test_extract_handle_basic();
    if (!strcmp(test, "extract_handle_short_input"))  return test_extract_handle_short_input();
    if (!strcmp(test, "extract_handle_all_spaces_first13")) return test_extract_handle_all_spaces_first13();
    if (!strcmp(test, "extract_handle_null_selection_returns_zero")) return test_extract_handle_null_selection_returns_zero();
    if (!strcmp(test, "extract_handle_null_buf_returns_zero")) return test_extract_handle_null_buf_returns_zero();
    if (!strcmp(test, "extract_handle_short_buflen_returns_zero")) return test_extract_handle_short_buflen_returns_zero();
    if (!strcmp(test, "extract_handle_no_spaces"))    return test_extract_handle_no_spaces();

    fprintf(stderr, "Unknown test: %s\n", test);
    return 1;
}
