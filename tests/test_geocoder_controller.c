/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025-2026 The Xastir Group
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Look at the README for more information on the program.
 */

/*
 * Unit tests for geocoder_controller.c — the presentation-free kernel
 * extracted from geocoder_gui.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/test_framework.h"

#include "geocoder_controller.h"


/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

int test_init_default_values(void)
{
    geocoder_controller_t gc;

    memset(&gc, 0x5a, sizeof(gc));
    geocoder_controller_init(&gc);

    TEST_ASSERT(gc.result_count == 0, "result_count zeroed after init");

    TEST_PASS("geocoder_controller_init: default values");
}

int test_init_null_safe(void)
{
    geocoder_controller_init(NULL);  /* must not crash */
    TEST_PASS("geocoder_controller_init: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* Country count                                                       */
/* ------------------------------------------------------------------ */

int test_country_count_positive(void)
{
    int n = geocoder_controller_country_count();
    TEST_ASSERT(n > 0, "country table has at least one entry");
    TEST_PASS("geocoder_controller_country_count: positive count");
}

int test_country_count_includes_sentinel(void)
{
    /* The sentinel is NOT counted; verify count doesn't include it by
       checking that label[count] returns NULL. */
    int n = geocoder_controller_country_count();
    TEST_ASSERT(geocoder_controller_country_label(n) == NULL,
                "entry at [count] is sentinel (NULL)");
    TEST_PASS("geocoder_controller_country_count: sentinel not counted");
}


/* ------------------------------------------------------------------ */
/* Country label / value access                                        */
/* ------------------------------------------------------------------ */

int test_country_label_first(void)
{
    const char *label = geocoder_controller_country_label(0);
    TEST_ASSERT(label != NULL, "first label is non-NULL");
    TEST_ASSERT_STR_EQ("None (Worldwide)", label, "first label is 'None (Worldwide)'");
    TEST_PASS("geocoder_controller_country_label: first entry");
}

int test_country_value_first(void)
{
    const char *value = geocoder_controller_country_value(0);
    TEST_ASSERT(value != NULL, "first value is non-NULL");
    TEST_ASSERT_STR_EQ("", value, "first value is empty string (no filter)");
    TEST_PASS("geocoder_controller_country_value: first entry (worldwide)");
}

int test_country_label_known(void)
{
    /* "United States" must be somewhere in the table. */
    int n = geocoder_controller_country_count();
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(geocoder_controller_country_label(i), "United States") == 0) {
            found = 1;
            break;
        }
    }
    TEST_ASSERT(found, "'United States' is in the country table");
    TEST_PASS("geocoder_controller_country_label: 'United States' present");
}

int test_country_value_known(void)
{
    /* "United States" → "us" */
    int n = geocoder_controller_country_count();
    int found = 0;
    for (int i = 0; i < n; i++) {
        if (strcmp(geocoder_controller_country_label(i), "United States") == 0) {
            TEST_ASSERT_STR_EQ("us", geocoder_controller_country_value(i),
                               "United States value is 'us'");
            found = 1;
            break;
        }
    }
    TEST_ASSERT(found, "found United States entry to check value");
    TEST_PASS("geocoder_controller_country_value: United States → 'us'");
}

int test_country_custom_entry(void)
{
    /* "--- Custom Code ---" must be the last entry before sentinel. */
    int n = geocoder_controller_country_count();
    const char *label = geocoder_controller_country_label(n - 1);
    const char *value = geocoder_controller_country_value(n - 1);
    TEST_ASSERT(label != NULL, "last entry label non-NULL");
    TEST_ASSERT_STR_EQ("--- Custom Code ---", label, "last label is Custom");
    TEST_ASSERT_STR_EQ("custom", value, "last value is 'custom'");
    TEST_PASS("geocoder_controller_country_label/value: custom entry is last");
}

int test_country_label_out_of_range(void)
{
    int n = geocoder_controller_country_count();
    TEST_ASSERT(geocoder_controller_country_label(n + 10) == NULL,
                "out-of-range index returns NULL label");
    TEST_ASSERT(geocoder_controller_country_label(-1) == NULL,
                "negative index returns NULL label");
    TEST_PASS("geocoder_controller_country_label: out-of-range returns NULL");
}

int test_country_value_out_of_range(void)
{
    int n = geocoder_controller_country_count();
    TEST_ASSERT(geocoder_controller_country_value(n + 10) == NULL,
                "out-of-range index returns NULL value");
    TEST_ASSERT(geocoder_controller_country_value(-1) == NULL,
                "negative index returns NULL value");
    TEST_PASS("geocoder_controller_country_value: out-of-range returns NULL");
}


/* ------------------------------------------------------------------ */
/* find_country_index                                                  */
/* ------------------------------------------------------------------ */

int test_find_country_index_known(void)
{
    int idx = geocoder_controller_find_country_index("us");
    TEST_ASSERT(idx > 0, "returns positive 1-based index for 'us'");

    /* The value at that 0-based position must be "us". */
    const char *val = geocoder_controller_country_value(idx - 1);
    TEST_ASSERT(val != NULL && strcmp(val, "us") == 0,
                "value at found index is 'us'");

    TEST_PASS("geocoder_controller_find_country_index: 'us' found");
}

int test_find_country_index_worldwide(void)
{
    /* Empty string is the value for "None (Worldwide)" — position 1. */
    int idx = geocoder_controller_find_country_index("");
    /* Empty string match should return 0 (not found) per the spec. */
    TEST_ASSERT(idx == 0, "empty string returns 0 (no match)");
    TEST_PASS("geocoder_controller_find_country_index: empty string returns 0");
}

int test_find_country_index_unknown(void)
{
    int idx = geocoder_controller_find_country_index("xx");
    TEST_ASSERT(idx == 0, "unknown value returns 0");
    TEST_PASS("geocoder_controller_find_country_index: unknown returns 0");
}

int test_find_country_index_null(void)
{
    int idx = geocoder_controller_find_country_index(NULL);
    TEST_ASSERT(idx == 0, "NULL returns 0");
    TEST_PASS("geocoder_controller_find_country_index: NULL returns 0");
}

int test_find_country_index_custom(void)
{
    int idx = geocoder_controller_find_country_index("custom");
    TEST_ASSERT(idx > 0, "returns positive index for 'custom'");
    TEST_PASS("geocoder_controller_find_country_index: 'custom' found");
}


/* ------------------------------------------------------------------ */
/* label_to_code                                                       */
/* ------------------------------------------------------------------ */

int test_label_to_code_normal(void)
{
    char code[64] = {0};
    int is_custom = -1;
    int rc = geocoder_controller_label_to_code("Germany", "", code, sizeof(code), &is_custom);

    TEST_ASSERT(rc == 1, "returns 1 for known country label");
    TEST_ASSERT_STR_EQ("de", code, "Germany → 'de'");
    TEST_ASSERT(is_custom == 0, "is_custom is 0 for standard entry");
    TEST_PASS("geocoder_controller_label_to_code: normal country label");
}

int test_label_to_code_worldwide(void)
{
    char code[64] = "UNCHANGED";
    int rc = geocoder_controller_label_to_code("None (Worldwide)", "", code, sizeof(code), NULL);

    TEST_ASSERT(rc == 0, "returns 0 for worldwide (empty code)");
    TEST_PASS("geocoder_controller_label_to_code: worldwide returns 0");
}

int test_label_to_code_custom_with_text(void)
{
    char code[64] = {0};
    int is_custom = 0;
    int rc = geocoder_controller_label_to_code("--- Custom Code ---", "us,ca",
                                               code, sizeof(code), &is_custom);

    TEST_ASSERT(rc == 1, "returns 1 for custom with text provided");
    TEST_ASSERT_STR_EQ("us,ca", code, "custom text passed through");
    TEST_ASSERT(is_custom == 1, "is_custom set to 1");
    TEST_PASS("geocoder_controller_label_to_code: custom with text");
}

int test_label_to_code_custom_no_text(void)
{
    char code[64] = {0};
    int rc = geocoder_controller_label_to_code("--- Custom Code ---", "",
                                               code, sizeof(code), NULL);

    TEST_ASSERT(rc == 0, "returns 0 for custom with no custom text");
    TEST_PASS("geocoder_controller_label_to_code: custom without text returns 0");
}

int test_label_to_code_custom_null_text(void)
{
    char code[64] = {0};
    int rc = geocoder_controller_label_to_code("--- Custom Code ---", NULL,
                                               code, sizeof(code), NULL);

    TEST_ASSERT(rc == 0, "returns 0 for custom with NULL text");
    TEST_PASS("geocoder_controller_label_to_code: custom with NULL text returns 0");
}

int test_label_to_code_unknown_label(void)
{
    char code[64] = {0};
    int rc = geocoder_controller_label_to_code("Narnia", "", code, sizeof(code), NULL);

    TEST_ASSERT(rc == 0, "returns 0 for unknown label");
    TEST_PASS("geocoder_controller_label_to_code: unknown label returns 0");
}

int test_label_to_code_null_label(void)
{
    char code[64] = {0};
    int rc = geocoder_controller_label_to_code(NULL, "", code, sizeof(code), NULL);

    TEST_ASSERT(rc == 0, "returns 0 for NULL label");
    TEST_PASS("geocoder_controller_label_to_code: NULL label returns 0");
}

int test_label_to_code_null_out(void)
{
    int rc = geocoder_controller_label_to_code("Germany", "", NULL, 64, NULL);

    TEST_ASSERT(rc == 0, "returns 0 when out_code is NULL");
    TEST_PASS("geocoder_controller_label_to_code: NULL out buffer returns 0");
}


/* ------------------------------------------------------------------ */
/* normalize_server_url                                                */
/* ------------------------------------------------------------------ */

int test_normalize_url_empty(void)
{
    char url[512] = "";
    int rc = geocoder_controller_normalize_server_url(url, sizeof(url));

    TEST_ASSERT(rc == 1, "returns 1 when URL was empty");
    TEST_ASSERT_STR_EQ(GEOCODER_NOMINATIM_DEFAULT_URL, url,
                       "default URL written when empty");
    TEST_PASS("geocoder_controller_normalize_server_url: empty → default");
}

int test_normalize_url_non_empty(void)
{
    char url[512] = "https://example.com/nominatim";
    int rc = geocoder_controller_normalize_server_url(url, sizeof(url));

    TEST_ASSERT(rc == 0, "returns 0 when URL was non-empty");
    TEST_ASSERT_STR_EQ("https://example.com/nominatim", url,
                       "non-empty URL left unchanged");
    TEST_PASS("geocoder_controller_normalize_server_url: non-empty unchanged");
}

int test_normalize_url_null(void)
{
    int rc = geocoder_controller_normalize_server_url(NULL, 512);
    TEST_ASSERT(rc == 0, "NULL url returns 0, no crash");
    TEST_PASS("geocoder_controller_normalize_server_url: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* has_results                                                         */
/* ------------------------------------------------------------------ */

int test_has_results_zero(void)
{
    geocoder_controller_t gc;
    geocoder_controller_init(&gc);
    TEST_ASSERT(geocoder_controller_has_results(&gc) == 0,
                "returns 0 when result_count is 0");
    TEST_PASS("geocoder_controller_has_results: 0 count returns 0");
}

int test_has_results_positive(void)
{
    geocoder_controller_t gc;
    geocoder_controller_init(&gc);
    gc.result_count = 5;
    TEST_ASSERT(geocoder_controller_has_results(&gc) == 1,
                "returns 1 when result_count > 0");
    TEST_PASS("geocoder_controller_has_results: positive count returns 1");
}

int test_has_results_null(void)
{
    TEST_ASSERT(geocoder_controller_has_results(NULL) == 0,
                "NULL returns 0");
    TEST_PASS("geocoder_controller_has_results: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* Test runner                                                         */
/* ------------------------------------------------------------------ */

typedef struct
{
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
    test_case_t tests[] =
    {
        {"init_default_values",              test_init_default_values},
        {"init_null_safe",                   test_init_null_safe},
        {"country_count_positive",           test_country_count_positive},
        {"country_count_includes_sentinel",  test_country_count_includes_sentinel},
        {"country_label_first",              test_country_label_first},
        {"country_value_first",              test_country_value_first},
        {"country_label_known",              test_country_label_known},
        {"country_value_known",              test_country_value_known},
        {"country_custom_entry",             test_country_custom_entry},
        {"country_label_out_of_range",       test_country_label_out_of_range},
        {"country_value_out_of_range",       test_country_value_out_of_range},
        {"find_country_index_known",         test_find_country_index_known},
        {"find_country_index_worldwide",     test_find_country_index_worldwide},
        {"find_country_index_unknown",       test_find_country_index_unknown},
        {"find_country_index_null",          test_find_country_index_null},
        {"find_country_index_custom",        test_find_country_index_custom},
        {"label_to_code_normal",             test_label_to_code_normal},
        {"label_to_code_worldwide",          test_label_to_code_worldwide},
        {"label_to_code_custom_with_text",   test_label_to_code_custom_with_text},
        {"label_to_code_custom_no_text",     test_label_to_code_custom_no_text},
        {"label_to_code_custom_null_text",   test_label_to_code_custom_null_text},
        {"label_to_code_unknown_label",      test_label_to_code_unknown_label},
        {"label_to_code_null_label",         test_label_to_code_null_label},
        {"label_to_code_null_out",           test_label_to_code_null_out},
        {"normalize_url_empty",              test_normalize_url_empty},
        {"normalize_url_non_empty",          test_normalize_url_non_empty},
        {"normalize_url_null",               test_normalize_url_null},
        {"has_results_zero",                 test_has_results_zero},
        {"has_results_positive",             test_has_results_positive},
        {"has_results_null",                 test_has_results_null},
        {NULL, NULL}
    };

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test name>\n", argv[0]);
        fprintf(stderr, "Available tests:\n");
        for (int i = 0; tests[i].name != NULL; i++) {
            fprintf(stderr, "  %s\n", tests[i].name);
        }
        return 1;
    }

    const char *test_name = argv[1];
    for (int i = 0; tests[i].name != NULL; i++) {
        if (strcmp(test_name, tests[i].name) == 0) {
            return tests[i].func();
        }
    }

    fprintf(stderr, "Unknown test: %s\n", test_name);
    return 1;
}
