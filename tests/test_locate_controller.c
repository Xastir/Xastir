/*
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2000-2026 The Xastir Group
 *
 * Unit tests for locate_controller.c — the presentation-free kernel
 * extracted from locate_gui.c.
 *
 * Usage: test_locate_controller <test-name>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "tests/test_framework.h"
#include "locate_controller.h"

/* ------------------------------------------------------------------ */
/* locate_controller_init                                               */
/* ------------------------------------------------------------------ */

int test_init_default_values(void)
{
    locate_controller_t lc;
    memset(&lc, 0xFF, sizeof(lc)); /* poison */
    locate_controller_init(&lc);

    TEST_ASSERT(lc.station_call[0] == '\0',        "station_call empty after init");
    TEST_ASSERT(lc.station_case_sensitive == 0,    "station_case_sensitive zeroed");
    TEST_ASSERT(lc.station_match_exact == 0,       "station_match_exact zeroed");
    TEST_ASSERT(lc.place_name[0] == '\0',          "place_name empty after init");
    TEST_ASSERT(lc.state_name[0] == '\0',          "state_name empty after init");
    TEST_ASSERT(lc.county_name[0] == '\0',         "county_name empty after init");
    TEST_ASSERT(lc.quad_name[0] == '\0',           "quad_name empty after init");
    TEST_ASSERT(lc.type_name[0] == '\0',           "type_name empty after init");
    TEST_ASSERT(lc.gnis_filename[0] == '\0',       "gnis_filename empty after init");
    TEST_ASSERT(lc.place_case_sensitive == 0,      "place_case_sensitive zeroed");
    TEST_ASSERT(lc.place_match_exact == 0,         "place_match_exact zeroed");
    TEST_ASSERT(lc.match_count == 0,               "match_count zero after init");
    TEST_PASS("locate_controller_init: default values");
}

int test_init_null_safe(void)
{
    locate_controller_init(NULL); /* must not crash */
    TEST_PASS("locate_controller_init: NULL is safe");
}

/* ------------------------------------------------------------------ */
/* locate_controller_prepare_call                                       */
/* ------------------------------------------------------------------ */

int test_prepare_call_basic(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "W1AW");
    TEST_ASSERT(r == 1,                               "returns 1 for non-empty call");
    TEST_ASSERT_STR_EQ("W1AW", lc.station_call,      "callsign stored verbatim");
    TEST_PASS("prepare_call: basic callsign stored");
}

int test_prepare_call_trims_trailing_spaces(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "W1AW   ");
    TEST_ASSERT(r == 1,                               "returns 1 after trim");
    TEST_ASSERT_STR_EQ("W1AW", lc.station_call,      "trailing spaces stripped");
    TEST_PASS("prepare_call: trailing spaces stripped");
}

int test_prepare_call_strips_dash_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "W1AW-0");
    TEST_ASSERT(r == 1,                               "returns 1 after dash-zero strip");
    TEST_ASSERT_STR_EQ("W1AW", lc.station_call,      "dash-zero suffix removed");
    TEST_PASS("prepare_call: -0 suffix stripped");
}

int test_prepare_call_strips_dash_zero_with_spaces(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "W1AW-0 ");
    TEST_ASSERT(r == 1,                               "returns 1");
    TEST_ASSERT_STR_EQ("W1AW", lc.station_call,      "spaces then dash-zero stripped");
    TEST_PASS("prepare_call: spaces then -0 both stripped");
}

int test_prepare_call_preserves_dash_one(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "W1AW-1");
    TEST_ASSERT(r == 1,                               "returns 1");
    TEST_ASSERT_STR_EQ("W1AW-1", lc.station_call,    "-1 suffix kept");
    TEST_PASS("prepare_call: non-zero SSID preserved");
}

int test_prepare_call_empty_string_returns_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "");
    TEST_ASSERT(r == 0,                               "returns 0 for empty");
    TEST_ASSERT(lc.station_call[0] == '\0',           "station_call remains empty");
    TEST_PASS("prepare_call: empty string returns 0");
}

int test_prepare_call_spaces_only_returns_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, "   ");
    TEST_ASSERT(r == 0,                               "returns 0 for spaces-only");
    TEST_ASSERT(lc.station_call[0] == '\0',           "station_call empty");
    TEST_PASS("prepare_call: spaces-only returns 0");
}

int test_prepare_call_null_lc_returns_zero(void)
{
    int r = locate_controller_prepare_call(NULL, "W1AW");
    TEST_ASSERT(r == 0, "NULL lc returns 0");
    TEST_PASS("prepare_call: NULL lc is safe");
}

int test_prepare_call_null_call_returns_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_call(&lc, NULL);
    TEST_ASSERT(r == 0, "NULL call returns 0");
    TEST_PASS("prepare_call: NULL call is safe");
}

int test_prepare_call_truncates_long_call(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    /* 35 chars — longer than LOCATE_CALL_LEN (30) */
    int r = locate_controller_prepare_call(&lc, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    TEST_ASSERT(r == 1,                                           "returns 1 for non-empty");
    TEST_ASSERT((int)strlen(lc.station_call) <= LOCATE_CALL_LEN - 1,
                "truncated to LOCATE_CALL_LEN-1");
    TEST_PASS("prepare_call: long callsign truncated safely");
}

/* ------------------------------------------------------------------ */
/* locate_controller_prepare_place_query                                */
/* ------------------------------------------------------------------ */

int test_prepare_place_basic(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_place_query(&lc, "Portland", "OR", "", "", "", "");
    TEST_ASSERT(r == 1,                                "returns 1 for non-empty place");
    TEST_ASSERT_STR_EQ("Portland", lc.place_name,     "place_name stored");
    TEST_ASSERT_STR_EQ("OR",       lc.state_name,     "state_name stored");
    TEST_ASSERT_STR_EQ("",         lc.county_name,    "county_name empty");
    TEST_PASS("prepare_place: basic fields stored");
}

int test_prepare_place_trims_trailing_spaces(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_place_query(&lc, "Portland  ", "OR  ", "", "", "", "");
    TEST_ASSERT(r == 1,                               "returns 1");
    TEST_ASSERT_STR_EQ("Portland", lc.place_name,    "place_name trimmed");
    TEST_ASSERT_STR_EQ("OR",       lc.state_name,    "state_name trimmed");
    TEST_PASS("prepare_place: trailing spaces stripped");
}

int test_prepare_place_all_fields(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_place_query(&lc, "Salem", "OR", "Marion",
                                                  "Salem West", "populated place",
                                                  "/usr/share/gnis.txt");
    TEST_ASSERT(r == 1,                                        "returns 1");
    TEST_ASSERT_STR_EQ("Salem",              lc.place_name,   "place_name");
    TEST_ASSERT_STR_EQ("OR",                 lc.state_name,   "state_name");
    TEST_ASSERT_STR_EQ("Marion",             lc.county_name,  "county_name");
    TEST_ASSERT_STR_EQ("Salem West",         lc.quad_name,    "quad_name");
    TEST_ASSERT_STR_EQ("populated place",    lc.type_name,    "type_name");
    TEST_ASSERT_STR_EQ("/usr/share/gnis.txt", lc.gnis_filename, "gnis_filename");
    TEST_PASS("prepare_place: all six fields stored");
}

int test_prepare_place_empty_name_returns_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_place_query(&lc, "", "OR", "", "", "", "");
    TEST_ASSERT(r == 0, "returns 0 for empty place name");
    TEST_PASS("prepare_place: empty name returns 0");
}

int test_prepare_place_spaces_only_name_returns_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_place_query(&lc, "   ", "OR", "", "", "", "");
    TEST_ASSERT(r == 0, "returns 0 for spaces-only name");
    TEST_PASS("prepare_place: spaces-only name returns 0");
}

int test_prepare_place_null_fields_treated_as_empty(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    int r = locate_controller_prepare_place_query(&lc, "Portland", NULL, NULL, NULL, NULL, NULL);
    TEST_ASSERT(r == 1,                                "returns 1");
    TEST_ASSERT_STR_EQ("Portland", lc.place_name,     "place_name set");
    TEST_ASSERT_STR_EQ("",         lc.state_name,     "NULL state -> empty");
    TEST_ASSERT_STR_EQ("",         lc.county_name,    "NULL county -> empty");
    TEST_PASS("prepare_place: NULL optional fields treated as empty");
}

int test_prepare_place_null_lc_returns_zero(void)
{
    int r = locate_controller_prepare_place_query(NULL, "Portland", "OR", "", "", "", "");
    TEST_ASSERT(r == 0, "NULL lc returns 0");
    TEST_PASS("prepare_place: NULL lc is safe");
}

int test_prepare_place_gnis_filename_stored(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    locate_controller_prepare_place_query(&lc, "X", "", "", "", "", "/data/gnis.txt");
    TEST_ASSERT_STR_EQ("/data/gnis.txt", lc.gnis_filename, "gnis_filename stored verbatim");
    TEST_PASS("prepare_place: gnis filename stored verbatim");
}

/* ------------------------------------------------------------------ */
/* locate_controller_has_results / clear_results                        */
/* ------------------------------------------------------------------ */

int test_has_results_zero(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    TEST_ASSERT(locate_controller_has_results(&lc) == 0, "no results after init");
    TEST_PASS("has_results: 0 when match_count is 0");
}

int test_has_results_positive(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    lc.match_count = 3;
    TEST_ASSERT(locate_controller_has_results(&lc) == 1, "returns 1 when match_count > 0");
    TEST_PASS("has_results: 1 when match_count > 0");
}

int test_has_results_null_safe(void)
{
    TEST_ASSERT(locate_controller_has_results(NULL) == 0, "NULL returns 0");
    TEST_PASS("has_results: NULL is safe");
}

int test_clear_results(void)
{
    locate_controller_t lc;
    locate_controller_init(&lc);
    lc.match_count = 5;
    locate_controller_clear_results(&lc);
    TEST_ASSERT(lc.match_count == 0, "match_count reset to 0");
    TEST_PASS("clear_results: match_count reset to 0");
}

int test_clear_results_null_safe(void)
{
    locate_controller_clear_results(NULL); /* must not crash */
    TEST_PASS("clear_results: NULL is safe");
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

    if (!strcmp(test, "init_default_values"))          return test_init_default_values();
    if (!strcmp(test, "init_null_safe"))               return test_init_null_safe();
    if (!strcmp(test, "prepare_call_basic"))           return test_prepare_call_basic();
    if (!strcmp(test, "prepare_call_trims_trailing_spaces"))  return test_prepare_call_trims_trailing_spaces();
    if (!strcmp(test, "prepare_call_strips_dash_zero"))       return test_prepare_call_strips_dash_zero();
    if (!strcmp(test, "prepare_call_strips_dash_zero_with_spaces")) return test_prepare_call_strips_dash_zero_with_spaces();
    if (!strcmp(test, "prepare_call_preserves_dash_one"))     return test_prepare_call_preserves_dash_one();
    if (!strcmp(test, "prepare_call_empty_string_returns_zero")) return test_prepare_call_empty_string_returns_zero();
    if (!strcmp(test, "prepare_call_spaces_only_returns_zero")) return test_prepare_call_spaces_only_returns_zero();
    if (!strcmp(test, "prepare_call_null_lc_returns_zero"))   return test_prepare_call_null_lc_returns_zero();
    if (!strcmp(test, "prepare_call_null_call_returns_zero")) return test_prepare_call_null_call_returns_zero();
    if (!strcmp(test, "prepare_call_truncates_long_call"))    return test_prepare_call_truncates_long_call();
    if (!strcmp(test, "prepare_place_basic"))                 return test_prepare_place_basic();
    if (!strcmp(test, "prepare_place_trims_trailing_spaces")) return test_prepare_place_trims_trailing_spaces();
    if (!strcmp(test, "prepare_place_all_fields"))            return test_prepare_place_all_fields();
    if (!strcmp(test, "prepare_place_empty_name_returns_zero")) return test_prepare_place_empty_name_returns_zero();
    if (!strcmp(test, "prepare_place_spaces_only_name_returns_zero")) return test_prepare_place_spaces_only_name_returns_zero();
    if (!strcmp(test, "prepare_place_null_fields_treated_as_empty")) return test_prepare_place_null_fields_treated_as_empty();
    if (!strcmp(test, "prepare_place_null_lc_returns_zero"))  return test_prepare_place_null_lc_returns_zero();
    if (!strcmp(test, "prepare_place_gnis_filename_stored"))  return test_prepare_place_gnis_filename_stored();
    if (!strcmp(test, "has_results_zero"))                    return test_has_results_zero();
    if (!strcmp(test, "has_results_positive"))                return test_has_results_positive();
    if (!strcmp(test, "has_results_null_safe"))               return test_has_results_null_safe();
    if (!strcmp(test, "clear_results"))                       return test_clear_results();
    if (!strcmp(test, "clear_results_null_safe"))             return test_clear_results_null_safe();

    fprintf(stderr, "Unknown test: %s\n", test);
    return 1;
}
