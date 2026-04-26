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
 * Unit tests for bulletin_controller.c — the presentation-free kernel
 * extracted from bulletin_gui.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/test_framework.h"

#include "bulletin_controller.h"


/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */

int test_init_default_values(void)
{
  bulletin_controller_t bc;

  /* Poison with garbage so init has to clear everything. */
  memset(&bc, 0x5a, sizeof(bc));

  bulletin_controller_init(&bc);

  TEST_ASSERT(bc.range              == 0,          "range zeroed after init");
  TEST_ASSERT(bc.view_zero_distance == 0,          "view_zero_distance zeroed after init");
  TEST_ASSERT(bc.pop_up_enabled     == 0,          "pop_up_enabled zeroed after init");
  TEST_ASSERT(bc.new_flag           == 0,          "new_flag zeroed after init");
  TEST_ASSERT(bc.new_count          == 0,          "new_count zeroed after init");
  TEST_ASSERT(bc.first_new_time     == (time_t)0,  "first_new_time zeroed after init");
  TEST_ASSERT(bc.last_new_time      == (time_t)0,  "last_new_time zeroed after init");
  TEST_ASSERT(bc.last_check         == (time_t)0,  "last_check zeroed after init");

  TEST_PASS("bulletin_controller_init: default values");
}

int test_init_null_safe(void)
{
  bulletin_controller_init(NULL);  /* must not crash */
  TEST_PASS("bulletin_controller_init: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* in_range                                                            */
/* ------------------------------------------------------------------ */

int test_in_range_zero_dist_without_view_zero(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.view_zero_distance = 0;

  /* distance 0.0 and view_zero_distance off → reject */
  TEST_ASSERT(bulletin_controller_in_range(&bc, 0.0) == 0,
              "zero distance rejected when view_zero_distance disabled");
  TEST_PASS("bulletin_controller_in_range: zero dist rejected without view_zero");
}

int test_in_range_zero_dist_with_view_zero(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.view_zero_distance = 1;

  TEST_ASSERT(bulletin_controller_in_range(&bc, 0.0) == 1,
              "zero distance accepted when view_zero_distance enabled");
  TEST_PASS("bulletin_controller_in_range: zero dist accepted with view_zero");
}

int test_in_range_unlimited(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range = 0;  /* 0 = no limit */

  TEST_ASSERT(bulletin_controller_in_range(&bc, 999.9) == 1,
              "range==0 accepts large distance");
  TEST_ASSERT(bulletin_controller_in_range(&bc, 1.0) == 1,
              "range==0 accepts small positive distance");
  TEST_PASS("bulletin_controller_in_range: range==0 unlimited");
}

int test_in_range_within(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range = 100;

  TEST_ASSERT(bulletin_controller_in_range(&bc, 50.0) == 1,
              "50 miles is within 100-mile range");
  TEST_PASS("bulletin_controller_in_range: within range accepted");
}

int test_in_range_outside(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range = 100;

  TEST_ASSERT(bulletin_controller_in_range(&bc, 150.0) == 0,
              "150 miles rejected by 100-mile range");
  TEST_PASS("bulletin_controller_in_range: outside range rejected");
}

int test_in_range_exact_boundary(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range = 100;

  /* (int)100.0 == 100 == bc->range → accepted */
  TEST_ASSERT(bulletin_controller_in_range(&bc, 100.0) == 1,
              "exact boundary distance accepted");
  TEST_PASS("bulletin_controller_in_range: exact boundary accepted");
}

int test_in_range_null_safe(void)
{
  TEST_ASSERT(bulletin_controller_in_range(NULL, 50.0) == 0,
              "NULL bc returns 0 (reject)");
  TEST_PASS("bulletin_controller_in_range: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* record_new                                                          */
/* ------------------------------------------------------------------ */

int test_record_new_first_call(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);

  bulletin_controller_record_new(&bc, (time_t)1000);

  TEST_ASSERT(bc.new_flag       == 1,           "new_flag set after first record_new");
  TEST_ASSERT(bc.first_new_time == (time_t)1000, "first_new_time set to sec_heard");
  TEST_ASSERT(bc.last_new_time  == (time_t)1000, "last_new_time set to sec_heard");
  TEST_PASS("bulletin_controller_record_new: first call sets both brackets");
}

int test_record_new_later_extends_last(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);

  bulletin_controller_record_new(&bc, (time_t)1000);
  bulletin_controller_record_new(&bc, (time_t)2000);

  TEST_ASSERT(bc.first_new_time == (time_t)1000, "first_new_time stays at 1000");
  TEST_ASSERT(bc.last_new_time  == (time_t)2000, "last_new_time advances to 2000");
  TEST_PASS("bulletin_controller_record_new: later time extends last_new_time only");
}

int test_record_new_earlier_extends_first(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);

  bulletin_controller_record_new(&bc, (time_t)2000);
  bulletin_controller_record_new(&bc, (time_t)500);

  TEST_ASSERT(bc.first_new_time == (time_t)500,  "first_new_time moves back to 500");
  TEST_ASSERT(bc.last_new_time  == (time_t)2000, "last_new_time stays at 2000");
  TEST_PASS("bulletin_controller_record_new: earlier time extends first_new_time only");
}

int test_record_new_null_safe(void)
{
  bulletin_controller_record_new(NULL, (time_t)1000);  /* must not crash */
  TEST_PASS("bulletin_controller_record_new: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* count_one                                                           */
/* ------------------------------------------------------------------ */

int test_count_one_out_of_range(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range          = 100;
  bc.first_new_time = (time_t)1000;

  /* distance out of range → should not count */
  bulletin_controller_count_one(&bc, 200.0, (time_t)1500);

  TEST_ASSERT(bc.new_count == 0, "out-of-range bulletin not counted");
  TEST_PASS("bulletin_controller_count_one: out-of-range skipped");
}

int test_count_one_too_old(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range          = 0;   /* unlimited */
  bc.first_new_time = (time_t)2000;

  /* sec_heard < first_new_time → too old, not counted */
  bulletin_controller_count_one(&bc, 50.0, (time_t)1000);

  TEST_ASSERT(bc.new_count == 0, "bulletin older than first_new_time not counted");
  TEST_PASS("bulletin_controller_count_one: too-old bulletin skipped");
}

int test_count_one_valid(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.range          = 0;   /* unlimited */
  bc.first_new_time = (time_t)1000;

  bulletin_controller_count_one(&bc, 50.0, (time_t)1500);

  TEST_ASSERT(bc.new_count == 1, "valid bulletin increments count to 1");

  bulletin_controller_count_one(&bc, 75.0, (time_t)1600);
  TEST_ASSERT(bc.new_count == 2, "second valid bulletin increments count to 2");

  TEST_PASS("bulletin_controller_count_one: valid bulletins counted");
}

int test_count_one_null_safe(void)
{
  bulletin_controller_count_one(NULL, 50.0, (time_t)1000);  /* must not crash */
  TEST_PASS("bulletin_controller_count_one: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* should_check                                                        */
/* ------------------------------------------------------------------ */

int test_should_check_too_soon(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.last_check    = (time_t)1000;
  bc.new_flag      = 1;
  bc.last_new_time = (time_t)900;

  /* curr_sec within the interval → reject */
  TEST_ASSERT(bulletin_controller_should_check(&bc, (time_t)1000 + BULLETIN_CHECK_INTERVAL - 1) == 0,
              "too soon: returns 0");
  TEST_PASS("bulletin_controller_should_check: within interval returns 0");
}

int test_should_check_no_flag(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.last_check    = (time_t)1000;
  bc.new_flag      = 0;  /* no new bulletins */
  bc.last_new_time = (time_t)900;
  time_t curr      = (time_t)1000 + BULLETIN_CHECK_INTERVAL + 1;

  TEST_ASSERT(bulletin_controller_should_check(&bc, curr) == 0,
              "no new_flag: returns 0");
  TEST_PASS("bulletin_controller_should_check: no flag returns 0");
}

int test_should_check_settle_not_elapsed(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.last_check    = (time_t)1000;
  bc.new_flag      = 1;
  /* last_new_time is recent enough that settle period hasn't passed */
  bc.last_new_time = (time_t)1000 + BULLETIN_CHECK_INTERVAL + 1;
  time_t curr      = bc.last_new_time + BULLETIN_SETTLE_TIME - 1;

  TEST_ASSERT(bulletin_controller_should_check(&bc, curr) == 0,
              "settle period not elapsed: returns 0");
  TEST_PASS("bulletin_controller_should_check: settle period not elapsed returns 0");
}

int test_should_check_all_gates_pass(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.last_check    = (time_t)1000;
  bc.new_flag      = 1;
  bc.last_new_time = (time_t)1000;
  time_t curr      = (time_t)1000 + BULLETIN_CHECK_INTERVAL + BULLETIN_SETTLE_TIME + 1;

  TEST_ASSERT(bulletin_controller_should_check(&bc, curr) == 1,
              "all gates passed: returns 1");
  TEST_PASS("bulletin_controller_should_check: all gates pass returns 1");
}

int test_should_check_null_safe(void)
{
  TEST_ASSERT(bulletin_controller_should_check(NULL, (time_t)9999) == 0,
              "NULL bc returns 0");
  TEST_PASS("bulletin_controller_should_check: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* reset_batch                                                         */
/* ------------------------------------------------------------------ */

int test_reset_batch(void)
{
  bulletin_controller_t bc;
  bulletin_controller_init(&bc);
  bc.new_flag      = 1;
  bc.new_count     = 5;
  bc.last_new_time = (time_t)2000;
  bc.first_new_time = (time_t)1000;

  bulletin_controller_reset_batch(&bc);

  TEST_ASSERT(bc.new_flag       == 0,           "new_flag cleared after reset");
  TEST_ASSERT(bc.new_count      == 0,           "new_count cleared after reset");
  TEST_ASSERT(bc.first_new_time == (time_t)2001, "first_new_time = last_new_time + 1");
  TEST_PASS("bulletin_controller_reset_batch: clears flag, count, advances first_new_time");
}

int test_reset_batch_null_safe(void)
{
  bulletin_controller_reset_batch(NULL);  /* must not crash */
  TEST_PASS("bulletin_controller_reset_batch: NULL is safe");
}


/* ------------------------------------------------------------------ */
/* format_line                                                         */
/* ------------------------------------------------------------------ */

int test_format_line_basic(void)
{
  char buf[256];
  int rc;

  rc = bulletin_format_line("W1AW", "BLN0XYZ", "Jan 01 12:00",
                            42.5, "miles",
                            "Test bulletin message",
                            buf, sizeof(buf));

  TEST_ASSERT(rc == 0, "format_line returns 0 on success");

  /* Check that tag+3 is used: "BLN0XYZ" → "XYZ" after the colon */
  TEST_ASSERT(strstr(buf, "XYZ") != NULL, "tag+3 prefix used (BLN stripped)");
  TEST_ASSERT(strstr(buf, "BLN") == NULL, "BLN prefix stripped from display");
  TEST_ASSERT(strstr(buf, "W1AW") != NULL, "from_call present");
  TEST_ASSERT(strstr(buf, "42.5") != NULL || strstr(buf, " 42.5") != NULL,
              "distance present");
  TEST_ASSERT(strstr(buf, "Test bulletin message") != NULL, "message present");
  TEST_ASSERT(buf[strlen(buf)-1] == '\n', "trailing newline present");

  TEST_PASS("bulletin_format_line: basic output correct");
}

int test_format_line_null_args(void)
{
  char buf[256];

  TEST_ASSERT(bulletin_format_line(NULL, "BLN0A", "t", 0.0, "m", "msg", buf, sizeof(buf)) != 0,
              "NULL from_call returns error");
  TEST_ASSERT(bulletin_format_line("W1AW", NULL, "t", 0.0, "m", "msg", buf, sizeof(buf)) != 0,
              "NULL tag returns error");
  TEST_ASSERT(bulletin_format_line("W1AW", "BLN0A", NULL, 0.0, "m", "msg", buf, sizeof(buf)) != 0,
              "NULL time_str returns error");
  TEST_ASSERT(bulletin_format_line("W1AW", "BLN0A", "t", 0.0, NULL, "msg", buf, sizeof(buf)) != 0,
              "NULL dist_unit returns error");
  TEST_ASSERT(bulletin_format_line("W1AW", "BLN0A", "t", 0.0, "m", NULL, buf, sizeof(buf)) != 0,
              "NULL message returns error");
  TEST_ASSERT(bulletin_format_line("W1AW", "BLN0A", "t", 0.0, "m", "msg", NULL, sizeof(buf)) != 0,
              "NULL out returns error");

  TEST_PASS("bulletin_format_line: NULL args return error");
}

int test_format_line_zero_outsz(void)
{
  char buf[256];
  buf[0] = 'X';

  TEST_ASSERT(bulletin_format_line("W1AW", "BLN0A", "t", 0.0, "m", "msg", buf, 0) != 0,
              "outsz==0 returns error");
  TEST_ASSERT(buf[0] == 'X', "zero-size buffer left untouched");

  TEST_PASS("bulletin_format_line: outsz==0 returns error");
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
    {"in_range_zero_dist_without_view_zero", test_in_range_zero_dist_without_view_zero},
    {"in_range_zero_dist_with_view_zero",    test_in_range_zero_dist_with_view_zero},
    {"in_range_unlimited",               test_in_range_unlimited},
    {"in_range_within",                  test_in_range_within},
    {"in_range_outside",                 test_in_range_outside},
    {"in_range_exact_boundary",          test_in_range_exact_boundary},
    {"in_range_null_safe",               test_in_range_null_safe},
    {"record_new_first_call",            test_record_new_first_call},
    {"record_new_later_extends_last",    test_record_new_later_extends_last},
    {"record_new_earlier_extends_first", test_record_new_earlier_extends_first},
    {"record_new_null_safe",             test_record_new_null_safe},
    {"count_one_out_of_range",           test_count_one_out_of_range},
    {"count_one_too_old",                test_count_one_too_old},
    {"count_one_valid",                  test_count_one_valid},
    {"count_one_null_safe",              test_count_one_null_safe},
    {"should_check_too_soon",            test_should_check_too_soon},
    {"should_check_no_flag",             test_should_check_no_flag},
    {"should_check_settle_not_elapsed",  test_should_check_settle_not_elapsed},
    {"should_check_all_gates_pass",      test_should_check_all_gates_pass},
    {"should_check_null_safe",           test_should_check_null_safe},
    {"reset_batch",                      test_reset_batch},
    {"reset_batch_null_safe",            test_reset_batch_null_safe},
    {"format_line_basic",                test_format_line_basic},
    {"format_line_null_args",            test_format_line_null_args},
    {"format_line_zero_outsz",           test_format_line_zero_outsz},
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
