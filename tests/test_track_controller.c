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
 * Unit tests for track_controller.c — the presentation-free kernel
 * extracted from track_gui.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/test_framework.h"

#include "track_controller.h"

/* Controllable stub flags from test_track_controller_stubs.c */
extern int stub_valid_object_result;
extern int stub_valid_call_result;
extern int stub_valid_item_result;

/* Helper: reset all stubs to "invalid" before each test */
static void reset_stubs(void)
{
  stub_valid_object_result = 0;
  stub_valid_call_result   = 0;
  stub_valid_item_result   = 0;
}


/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

int test_init_default_values(void)
{
  track_controller_t tc;

  memset(&tc, 0x5a, sizeof(tc));
  track_controller_init(&tc);

  TEST_ASSERT(tc.station_on    == 0,                  "station_on zeroed after init");
  TEST_ASSERT(tc.track_me      == 0,                  "track_me zeroed after init");
  TEST_ASSERT(tc.case_sensitive == 0,                 "case_sensitive zeroed after init");
  TEST_ASSERT(tc.match_exact   == 0,                  "match_exact zeroed after init");
  TEST_ASSERT(tc.station_call[0] == '\0',             "station_call empty after init");
  TEST_ASSERT(tc.download_call[0] == '\0',            "download_call empty after init");
  TEST_ASSERT(tc.posit_start   == MAX_FINDU_DURATION, "posit_start = MAX_FINDU_DURATION after init");
  TEST_ASSERT(tc.posit_length  == MAX_FINDU_DURATION, "posit_length = MAX_FINDU_DURATION after init");
  TEST_ASSERT(tc.fetching_now  == 0,                  "fetching_now zeroed after init");

  TEST_PASS("track_controller_init: default values");
}

int test_init_null_safe(void)
{
  track_controller_init(NULL);  /* must not crash */
  TEST_PASS("track_controller_init: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* set_station                                                         */
/* ------------------------------------------------------------------ */

int test_set_station_valid_call(void)
{
  track_controller_t tc;
  int rc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_call_result = 1;

  rc = track_controller_set_station(&tc, "W1AW");

  TEST_ASSERT(rc == 1,              "returns 1 for valid call");
  TEST_ASSERT(tc.station_on == 1,   "station_on set to 1");
  TEST_ASSERT_STR_EQ("W1AW", tc.station_call, "station_call stores trimmed call");

  TEST_PASS("track_controller_set_station: valid call accepted");
}

int test_set_station_valid_object(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_call_result   = 0;
  stub_valid_object_result = 1;

  TEST_ASSERT(track_controller_set_station(&tc, "MYOBJECT") == 1,
              "returns 1 when valid_object matches");
  TEST_ASSERT(tc.station_on == 1, "station_on set for object");

  TEST_PASS("track_controller_set_station: valid object accepted");
}

int test_set_station_valid_item(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_item_result = 1;

  TEST_ASSERT(track_controller_set_station(&tc, "MYITEM") == 1,
              "returns 1 when valid_item matches");
  TEST_ASSERT(tc.station_on == 1, "station_on set for item");

  TEST_PASS("track_controller_set_station: valid item accepted");
}

int test_set_station_invalid(void)
{
  track_controller_t tc;
  int rc;

  track_controller_init(&tc);
  reset_stubs();   /* all stubs return 0 */

  rc = track_controller_set_station(&tc, "BADCALL!!!!");

  TEST_ASSERT(rc == 0,                    "returns 0 for invalid call");
  TEST_ASSERT(tc.station_on == 0,         "station_on stays 0");
  TEST_ASSERT(tc.station_call[0] == '\0', "station_call cleared on invalid");

  TEST_PASS("track_controller_set_station: invalid call rejected");
}

int test_set_station_trims_spaces(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_call_result = 1;

  track_controller_set_station(&tc, "W1AW   ");

  TEST_ASSERT_STR_EQ("W1AW", tc.station_call,
                     "trailing spaces stripped before storing");

  TEST_PASS("track_controller_set_station: trailing spaces stripped");
}

int test_set_station_trims_dash_zero(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_call_result = 1;

  track_controller_set_station(&tc, "W1AW-0");

  TEST_ASSERT_STR_EQ("W1AW", tc.station_call,
                     "'-0' SSID suffix stripped before storing");

  TEST_PASS("track_controller_set_station: '-0' suffix stripped");
}

int test_set_station_trims_both(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_call_result = 1;

  track_controller_set_station(&tc, "W1AW-0  ");

  TEST_ASSERT_STR_EQ("W1AW", tc.station_call,
                     "spaces then '-0' both stripped");

  TEST_PASS("track_controller_set_station: trailing spaces + '-0' both stripped");
}

int test_set_station_null_safe(void)
{
  track_controller_t tc;

  track_controller_init(&tc);

  TEST_ASSERT(track_controller_set_station(NULL, "W1AW") == 0,
              "NULL tc returns 0");
  TEST_ASSERT(track_controller_set_station(&tc, NULL) == 0,
              "NULL raw_call returns 0");

  TEST_PASS("track_controller_set_station: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* clear_station                                                       */
/* ------------------------------------------------------------------ */

int test_clear_station(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  reset_stubs();
  stub_valid_call_result = 1;
  track_controller_set_station(&tc, "W1AW");

  TEST_ASSERT(tc.station_on == 1, "pre-condition: station is on");

  track_controller_clear_station(&tc);

  TEST_ASSERT(tc.station_on      == 0,    "station_on cleared to 0");
  TEST_ASSERT(tc.station_call[0] == '\0', "station_call emptied");

  TEST_PASS("track_controller_clear_station: clears station state");
}

int test_clear_station_null_safe(void)
{
  track_controller_clear_station(NULL);  /* must not crash */
  TEST_PASS("track_controller_clear_station: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* clamp_posit_length                                                  */
/* ------------------------------------------------------------------ */

int test_clamp_within_max(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  tc.posit_start  = 50;
  tc.posit_length = 200;   /* will be clamped to posit_start */

  track_controller_clamp_posit_length(&tc);

  TEST_ASSERT(tc.posit_length == 50,
              "posit_length clamped to posit_start when start <= MAX");

  TEST_PASS("track_controller_clamp_posit_length: within MAX uses posit_start");
}

int test_clamp_at_max(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  tc.posit_start  = MAX_FINDU_DURATION;
  tc.posit_length = 1;

  track_controller_clamp_posit_length(&tc);

  TEST_ASSERT(tc.posit_length == MAX_FINDU_DURATION,
              "posit_length equals MAX_FINDU_DURATION when start == MAX");

  TEST_PASS("track_controller_clamp_posit_length: exactly at MAX");
}

int test_clamp_exceeds_max(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  tc.posit_start  = MAX_FINDU_DURATION + 50;   /* > MAX */
  tc.posit_length = MAX_FINDU_DURATION + 50;

  track_controller_clamp_posit_length(&tc);

  TEST_ASSERT(tc.posit_length == MAX_FINDU_DURATION,
              "posit_length capped at MAX_FINDU_DURATION when start > MAX");

  TEST_PASS("track_controller_clamp_posit_length: exceeds MAX capped");
}

int test_clamp_null_safe(void)
{
  track_controller_clamp_posit_length(NULL);  /* must not crash */
  TEST_PASS("track_controller_clamp_posit_length: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* can_start_download                                                  */
/* ------------------------------------------------------------------ */

int test_can_start_idle(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  tc.fetching_now = 0;

  TEST_ASSERT(track_controller_can_start_download(&tc, 0) == 1,
              "returns 1 when idle and read_file_busy=0");

  TEST_PASS("track_controller_can_start_download: idle returns 1");
}

int test_can_start_blocked_fetching(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  tc.fetching_now = 1;

  TEST_ASSERT(track_controller_can_start_download(&tc, 0) == 0,
              "returns 0 when fetching_now is set");

  TEST_PASS("track_controller_can_start_download: fetching_now blocks download");
}

int test_can_start_blocked_read_file(void)
{
  track_controller_t tc;

  track_controller_init(&tc);
  tc.fetching_now = 0;

  TEST_ASSERT(track_controller_can_start_download(&tc, 1) == 0,
              "returns 0 when read_file_busy is set");

  TEST_PASS("track_controller_can_start_download: read_file_busy blocks download");
}

int test_can_start_null_safe(void)
{
  TEST_ASSERT(track_controller_can_start_download(NULL, 0) == 0,
              "NULL tc returns 0");

  TEST_PASS("track_controller_can_start_download: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* build_findu_url                                                     */
/* ------------------------------------------------------------------ */

int test_build_url_basic(void)
{
  track_controller_t tc;
  char buf[512];
  int rc;

  track_controller_init(&tc);
  snprintf(tc.download_call, sizeof(tc.download_call), "W1AW-8");
  tc.posit_start  = 48;
  tc.posit_length = 24;

  rc = track_controller_build_findu_url(&tc, buf, sizeof(buf));

  TEST_ASSERT(rc == 0,                        "returns 0 on success");
  TEST_ASSERT(strstr(buf, "W1AW-8") != NULL,  "callsign in URL");
  TEST_ASSERT(strstr(buf, "48")     != NULL,  "posit_start in URL");
  TEST_ASSERT(strstr(buf, "24")     != NULL,  "posit_length in URL");
  TEST_ASSERT(strstr(buf, "findu.com") != NULL, "findu domain in URL");

  TEST_PASS("track_controller_build_findu_url: basic URL correct");
}

int test_build_url_null_args(void)
{
  track_controller_t tc;
  char buf[512];

  track_controller_init(&tc);

  TEST_ASSERT(track_controller_build_findu_url(NULL, buf, sizeof(buf)) != 0,
              "NULL tc returns error");
  TEST_ASSERT(track_controller_build_findu_url(&tc, NULL, sizeof(buf)) != 0,
              "NULL out returns error");
  TEST_ASSERT(track_controller_build_findu_url(&tc, buf, 0) != 0,
              "outsz==0 returns error");

  TEST_PASS("track_controller_build_findu_url: NULL/zero args return error");
}

int test_build_url_empty_call(void)
{
  track_controller_t tc;
  char buf[512];

  track_controller_init(&tc);
  /* download_call is empty after init */

  TEST_ASSERT(track_controller_build_findu_url(&tc, buf, sizeof(buf)) != 0,
              "empty download_call returns error");

  TEST_PASS("track_controller_build_findu_url: empty call returns error");
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
    {"init_default_values",             test_init_default_values},
    {"init_null_safe",                  test_init_null_safe},
    {"set_station_valid_call",          test_set_station_valid_call},
    {"set_station_valid_object",        test_set_station_valid_object},
    {"set_station_valid_item",          test_set_station_valid_item},
    {"set_station_invalid",             test_set_station_invalid},
    {"set_station_trims_spaces",        test_set_station_trims_spaces},
    {"set_station_trims_dash_zero",     test_set_station_trims_dash_zero},
    {"set_station_trims_both",          test_set_station_trims_both},
    {"set_station_null_safe",           test_set_station_null_safe},
    {"clear_station",                   test_clear_station},
    {"clear_station_null_safe",         test_clear_station_null_safe},
    {"clamp_within_max",                test_clamp_within_max},
    {"clamp_at_max",                    test_clamp_at_max},
    {"clamp_exceeds_max",               test_clamp_exceeds_max},
    {"clamp_null_safe",                 test_clamp_null_safe},
    {"can_start_idle",                  test_can_start_idle},
    {"can_start_blocked_fetching",      test_can_start_blocked_fetching},
    {"can_start_blocked_read_file",     test_can_start_blocked_read_file},
    {"can_start_null_safe",             test_can_start_null_safe},
    {"build_url_basic",                 test_build_url_basic},
    {"build_url_null_args",             test_build_url_null_args},
    {"build_url_empty_call",            test_build_url_empty_call},
    {NULL, NULL}
  };

  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <test name>\n", argv[0]);
    fprintf(stderr, "Available tests:\n");
    for (int i = 0; tests[i].name != NULL; i++)
    {
      fprintf(stderr, "  %s\n", tests[i].name);
    }
    return 1;
  }

  const char *test_name = argv[1];
  for (int i = 0; tests[i].name != NULL; i++)
  {
    if (strcmp(test_name, tests[i].name) == 0)
    {
      return tests[i].func();
    }
  }

  fprintf(stderr, "Unknown test: %s\n", test_name);
  return 1;
}
