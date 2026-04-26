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
 * Unit tests for popup_controller.c — the presentation-free kernel
 * extracted from popup_gui.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/test_framework.h"

#include "popup_controller.h"


int test_init_zeroes_state(void)
{
  popup_controller_t pc;
  int i;

  /* fill with garbage so init has something to clear */
  memset(&pc, 0x5a, sizeof(pc));

  popup_controller_init(&pc);

  for (i = 0; i < POPUP_MAX_SLOTS; i++)
  {
    TEST_ASSERT(pc.slots[i].active == 0, "all slots inactive after init");
    TEST_ASSERT(pc.slots[i].sec_opened == 0, "all slot timestamps cleared after init");
  }
  TEST_ASSERT(pc.last_timeout_check == 0, "last_timeout_check zeroed after init");

  TEST_PASS("popup_controller_init: zeroes state");
}

int test_find_free_slot_empty_partial_full(void)
{
  popup_controller_t pc;
  int i;

  popup_controller_init(&pc);

  TEST_ASSERT(popup_controller_find_free_slot(&pc) == 0,
              "empty controller hands out slot 0 first");

  /* occupy slots 0..2 — find_free should return 3 */
  popup_controller_open_slot(&pc, 0, 1000);
  popup_controller_open_slot(&pc, 1, 1000);
  popup_controller_open_slot(&pc, 2, 1000);
  TEST_ASSERT(popup_controller_find_free_slot(&pc) == 3,
              "first free slot when 0..2 are taken is 3");

  /* fill everything */
  for (i = 0; i < POPUP_MAX_SLOTS; i++)
  {
    popup_controller_open_slot(&pc, i, 1000);
  }
  TEST_ASSERT(popup_controller_find_free_slot(&pc) == -1,
              "full controller returns -1");

  TEST_PASS("popup_controller_find_free_slot: empty/partial/full");
}

int test_open_slot_marks_active(void)
{
  popup_controller_t pc;

  popup_controller_init(&pc);

  TEST_ASSERT(popup_controller_slot_is_active(&pc, 4) == 0,
              "slot 4 is inactive before open");

  popup_controller_open_slot(&pc, 4, 12345);

  TEST_ASSERT(popup_controller_slot_is_active(&pc, 4) == 1,
              "slot 4 active after open");
  TEST_ASSERT(pc.slots[4].sec_opened == 12345,
              "slot 4 records sec_opened");
  TEST_ASSERT(popup_controller_slot_is_active(&pc, 3) == 0,
              "neighbouring slot 3 untouched");
  TEST_ASSERT(popup_controller_slot_is_active(&pc, 5) == 0,
              "neighbouring slot 5 untouched");

  TEST_PASS("popup_controller_open_slot: marks slot active");
}

int test_close_slot_clears_state(void)
{
  popup_controller_t pc;

  popup_controller_init(&pc);
  popup_controller_open_slot(&pc, 7, 99999);
  TEST_ASSERT(popup_controller_slot_is_active(&pc, 7) == 1, "precondition: slot 7 active");

  popup_controller_close_slot(&pc, 7);

  TEST_ASSERT(popup_controller_slot_is_active(&pc, 7) == 0,
              "slot 7 inactive after close");
  TEST_ASSERT(pc.slots[7].sec_opened == 0,
              "sec_opened cleared on close");

  TEST_PASS("popup_controller_close_slot: clears state");
}

int test_clear_resets_all_slots(void)
{
  popup_controller_t pc;
  int i;

  popup_controller_init(&pc);
  for (i = 0; i < POPUP_MAX_SLOTS; i++)
  {
    popup_controller_open_slot(&pc, i, 5000 + i);
  }
  pc.last_timeout_check = 9999;

  popup_controller_clear(&pc);

  for (i = 0; i < POPUP_MAX_SLOTS; i++)
  {
    TEST_ASSERT(pc.slots[i].active == 0, "slot inactive after clear");
    TEST_ASSERT(pc.slots[i].sec_opened == 0, "slot timestamp cleared after clear");
  }
  /* clear() intentionally does NOT touch last_timeout_check — that's a sweep
     concern, not a slot concern. Lock that contract in. */
  TEST_ASSERT(pc.last_timeout_check == 9999,
              "clear() does not reset the timeout-check timestamp");

  TEST_PASS("popup_controller_clear: resets all slots");
}

int test_timeout_check_within_interval(void)
{
  popup_controller_t pc;

  popup_controller_init(&pc);
  popup_controller_mark_timeout_check(&pc, 1000);

  TEST_ASSERT(popup_controller_should_run_timeout_check(&pc, 1000) == 0,
              "no sweep at exact same time");
  TEST_ASSERT(popup_controller_should_run_timeout_check(&pc, 1000 + POPUP_TIMEOUT_CHECK_INTERVAL) == 0,
              "no sweep exactly at the boundary");

  TEST_PASS("popup_controller_should_run_timeout_check: within interval -> 0");
}

int test_timeout_check_after_interval(void)
{
  popup_controller_t pc;

  popup_controller_init(&pc);
  popup_controller_mark_timeout_check(&pc, 1000);

  TEST_ASSERT(popup_controller_should_run_timeout_check(&pc, 1000 + POPUP_TIMEOUT_CHECK_INTERVAL + 1) == 1,
              "sweep one second past the interval");

  popup_controller_mark_timeout_check(&pc, 5000);
  TEST_ASSERT(pc.last_timeout_check == 5000, "mark_timeout_check stores the new time");

  TEST_PASS("popup_controller_should_run_timeout_check: after interval -> 1");
}

int test_slot_expired_decision(void)
{
  popup_controller_t pc;

  popup_controller_init(&pc);

  /* inactive slot: never expired */
  TEST_ASSERT(popup_controller_slot_expired(&pc, 0, 1000000) == 0,
              "inactive slot is never expired");

  /* young slot */
  popup_controller_open_slot(&pc, 1, 1000);
  TEST_ASSERT(popup_controller_slot_expired(&pc, 1, 1000 + POPUP_MAX_AGE_SECONDS) == 0,
              "slot at exactly max age is not yet expired");

  /* old slot */
  TEST_ASSERT(popup_controller_slot_expired(&pc, 1, 1000 + POPUP_MAX_AGE_SECONDS + 1) == 1,
              "slot one second past max age is expired");

  TEST_PASS("popup_controller_slot_expired: decisions correct");
}

int test_message_valid_combinations(void)
{
  TEST_ASSERT(popup_controller_message_valid("banner", "msg") == 1, "both present -> 1");
  TEST_ASSERT(popup_controller_message_valid(NULL,     "msg") == 0, "null banner -> 0");
  TEST_ASSERT(popup_controller_message_valid("banner", NULL)  == 0, "null message -> 0");
  TEST_ASSERT(popup_controller_message_valid(NULL,     NULL)  == 0, "both null -> 0");

  /* empty strings are still valid — original code only NULL-checked */
  TEST_ASSERT(popup_controller_message_valid("", "") == 1, "empty strings still valid");

  TEST_PASS("popup_controller_message_valid: NULL handling correct");
}

int test_format_slot_name_basic_and_wrap(void)
{
  char buf[POPUP_SLOT_NAME_SIZE];

  popup_controller_format_slot_name(5, buf, sizeof(buf));
  TEST_ASSERT_STR_EQ("        5", buf, "single-digit slot formatted as right-aligned 9-wide");

  popup_controller_format_slot_name(123, buf, sizeof(buf));
  TEST_ASSERT_STR_EQ("      123", buf, "three-digit slot formatted correctly");

  /* original code does idx % 1000 to keep the result in 9 chars */
  popup_controller_format_slot_name(1234, buf, sizeof(buf));
  TEST_ASSERT_STR_EQ("      234", buf, "wrap by mod 1000");

  TEST_PASS("popup_controller_format_slot_name: format and wrap correct");
}

int test_defensive_null_and_oob(void)
{
  popup_controller_t pc;
  char buf[POPUP_SLOT_NAME_SIZE];

  popup_controller_init(&pc);

  /* NULL controller — must not crash, sane returns */
  popup_controller_init(NULL);
  popup_controller_clear(NULL);
  TEST_ASSERT(popup_controller_find_free_slot(NULL) == -1, "NULL pc -> no slot");
  TEST_ASSERT(popup_controller_slot_is_active(NULL, 0) == 0, "NULL pc -> not active");
  popup_controller_open_slot(NULL, 0, 0);   /* must not crash */
  popup_controller_close_slot(NULL, 0);     /* must not crash */
  TEST_ASSERT(popup_controller_should_run_timeout_check(NULL, 0) == 0,
              "NULL pc -> never sweep");
  popup_controller_mark_timeout_check(NULL, 0);   /* must not crash */
  TEST_ASSERT(popup_controller_slot_expired(NULL, 0, 0) == 0,
              "NULL pc -> not expired");

  /* Out-of-range index — must not crash, sane returns */
  TEST_ASSERT(popup_controller_slot_is_active(&pc, -1) == 0, "negative idx -> not active");
  TEST_ASSERT(popup_controller_slot_is_active(&pc, POPUP_MAX_SLOTS) == 0,
              "idx == count -> not active");
  popup_controller_open_slot(&pc, -1, 0);                /* must not crash */
  popup_controller_open_slot(&pc, POPUP_MAX_SLOTS, 0);   /* must not crash */
  popup_controller_close_slot(&pc, -1);                  /* must not crash */
  popup_controller_close_slot(&pc, POPUP_MAX_SLOTS);     /* must not crash */
  TEST_ASSERT(popup_controller_slot_expired(&pc, -1, 0) == 0, "neg idx -> not expired");
  TEST_ASSERT(popup_controller_slot_expired(&pc, POPUP_MAX_SLOTS, 0) == 0,
              "oob idx -> not expired");

  /* format_slot_name: NULL out and zero size are no-ops */
  popup_controller_format_slot_name(0, NULL, 10);  /* must not crash */
  buf[0] = 'X';
  popup_controller_format_slot_name(0, buf, 0);    /* zero size: no write */
  TEST_ASSERT(buf[0] == 'X', "zero-size buffer untouched");

  TEST_PASS("popup_controller: defensive checks don't crash");
}


/* Test runner */
typedef struct
{
  const char *name;
  int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] =
  {
    {"init_zeroes_state",                   test_init_zeroes_state},
    {"find_free_slot_empty_partial_full",   test_find_free_slot_empty_partial_full},
    {"open_slot_marks_active",              test_open_slot_marks_active},
    {"close_slot_clears_state",             test_close_slot_clears_state},
    {"clear_resets_all_slots",              test_clear_resets_all_slots},
    {"timeout_check_within_interval",       test_timeout_check_within_interval},
    {"timeout_check_after_interval",        test_timeout_check_after_interval},
    {"slot_expired_decision",               test_slot_expired_decision},
    {"message_valid_combinations",          test_message_valid_combinations},
    {"format_slot_name_basic_and_wrap",     test_format_slot_name_basic_and_wrap},
    {"defensive_null_and_oob",              test_defensive_null_and_oob},
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
