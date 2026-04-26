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
 * Unit tests for view_message_controller.c — the presentation-free
 * kernel extracted from view_message_gui.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tests/test_framework.h"
#include "view_message_controller.h"


/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static view_message_controller_t make_vc(int range, int limit,
                                         int ptype, int mine_only)
{
  view_message_controller_t vc;
  vc.range            = range;
  vc.message_limit    = limit;
  vc.packet_data_type = ptype;
  vc.mine_only        = mine_only;
  return vc;
}


/* ------------------------------------------------------------------ */
/* view_message_controller_init                                        */
/* ------------------------------------------------------------------ */

int test_init_default_values(void)
{
  view_message_controller_t vc;
  memset(&vc, 0x5a, sizeof(vc));  /* fill with garbage */

  view_message_controller_init(&vc);

  TEST_ASSERT(vc.range == 0,              "init: range defaults to 0");
  TEST_ASSERT(vc.message_limit == 10000,  "init: message_limit defaults to 10000");
  TEST_ASSERT(vc.packet_data_type == VM_SOURCE_ALL, "init: packet_data_type defaults to ALL");
  TEST_ASSERT(vc.mine_only == 0,          "init: mine_only defaults to 0");

  TEST_PASS("view_message_controller_init: default values correct");
}

int test_init_null_safe(void)
{
  view_message_controller_init(NULL);  /* must not crash */
  TEST_PASS("view_message_controller_init: NULL is a safe no-op");
}


/* ------------------------------------------------------------------ */
/* view_message_strip_ssid                                             */
/* ------------------------------------------------------------------ */

int test_strip_ssid_with_ssid(void)
{
  char dst[16];
  view_message_strip_ssid("KD9XYZ-9", dst, sizeof(dst));
  TEST_ASSERT(strcmp(dst, "KD9XYZ") == 0, "strip_ssid: SSID suffix removed");
  TEST_PASS("view_message_strip_ssid: strip SSID suffix");
}

int test_strip_ssid_without_ssid(void)
{
  char dst[16];
  view_message_strip_ssid("KD9XYZ", dst, sizeof(dst));
  TEST_ASSERT(strcmp(dst, "KD9XYZ") == 0, "strip_ssid: no-SSID call unchanged");
  TEST_PASS("view_message_strip_ssid: no SSID present");
}

int test_strip_ssid_null_src(void)
{
  char dst[16];
  dst[0] = 'X';  /* poison */
  view_message_strip_ssid(NULL, dst, sizeof(dst));
  TEST_ASSERT(dst[0] == '\0', "strip_ssid: NULL src gives empty dst");
  TEST_PASS("view_message_strip_ssid: NULL src");
}

int test_strip_ssid_exact_minus(void)
{
  char dst[16];
  view_message_strip_ssid("W1AW-1", dst, sizeof(dst));
  TEST_ASSERT(strcmp(dst, "W1AW") == 0, "strip_ssid: single-digit SSID stripped");
  TEST_PASS("view_message_strip_ssid: single-digit SSID");
}


/* ------------------------------------------------------------------ */
/* view_message_controller_should_display                              */
/* ------------------------------------------------------------------ */

int test_should_display_range_zero_allows_any_distance(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_ALL, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX", 9999, "K1XX") == 1,
              "range=0: very far message still shown");
  TEST_PASS("should_display: range=0 allows any distance");
}

int test_should_display_range_excludes_far(void)
{
  view_message_controller_t vc = make_vc(50, 10000, VM_SOURCE_ALL, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX", 51, "K1XX") == 0,
              "distance > range → not displayed");
  TEST_PASS("should_display: range filter excludes far message");
}

int test_should_display_range_includes_near(void)
{
  view_message_controller_t vc = make_vc(50, 10000, VM_SOURCE_ALL, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX", 50, "K1XX") == 1,
              "distance == range → shown");
  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX", 10, "K1XX") == 1,
              "distance < range → shown");
  TEST_PASS("should_display: range filter passes near messages");
}

int test_should_display_tnc_only_drops_net(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_TNC, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'I', "W1AW", "K1XX", 0, "K1XX") == 0,
              "TNC-only filter: 'I' source dropped");
  TEST_ASSERT(view_message_controller_should_display(&vc, 'L', "W1AW", "K1XX", 0, "K1XX") == 0,
              "TNC-only filter: 'L' source dropped");
  TEST_PASS("should_display: TNC-only filter drops NET sources");
}

int test_should_display_tnc_only_passes_tnc(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_TNC, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX", 0, "K1XX") == 1,
              "TNC-only filter: 'T' source passes");
  TEST_PASS("should_display: TNC-only filter passes TNC source");
}

int test_should_display_net_only_drops_tnc(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_NET, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX", 0, "K1XX") == 0,
              "NET-only filter: 'T' source dropped");
  TEST_PASS("should_display: NET-only filter drops TNC source");
}

int test_should_display_net_only_passes_net(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_NET, 0);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'I', "W1AW", "K1XX", 0, "K1XX") == 1,
              "NET-only filter: 'I' source passes");
  TEST_PASS("should_display: NET-only filter passes NET source");
}

int test_should_display_mine_only_matching_call_sign(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_ALL, 1);

  /* call_sign contains base callsign */
  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "K1XX", "W1AW", 9999, "K1XX") == 1,
              "mine_only: call_sign matches base callsign");
  TEST_PASS("should_display: mine_only passes matching call_sign");
}

int test_should_display_mine_only_matching_from_call(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_ALL, 1);

  /* from_call contains base callsign */
  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "K1XX-9", 9999, "K1XX") == 1,
              "mine_only: from_call contains base callsign");
  TEST_PASS("should_display: mine_only passes matching from_call");
}

int test_should_display_mine_only_no_match(void)
{
  view_message_controller_t vc = make_vc(0, 10000, VM_SOURCE_ALL, 1);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "W1AW", "N0CALL", 9999, "K1XX") == 0,
              "mine_only: neither call matches → dropped");
  TEST_PASS("should_display: mine_only drops unrelated messages");
}

int test_should_display_mine_only_ignores_range(void)
{
  /* mine_only=1 should bypass the range check */
  view_message_controller_t vc = make_vc(10, 10000, VM_SOURCE_ALL, 1);

  TEST_ASSERT(view_message_controller_should_display(&vc, 'T', "K1XX", "W1AW", 9999, "K1XX") == 1,
              "mine_only: very far message shown when mine_only set");
  TEST_PASS("should_display: mine_only bypasses range filter");
}

int test_should_display_null_vc(void)
{
  TEST_ASSERT(view_message_controller_should_display(NULL, 'T', "W1AW", "K1XX", 0, "K1XX") == 0,
              "NULL vc → returns 0 without crashing");
  TEST_PASS("should_display: NULL vc is safe");
}


/* ------------------------------------------------------------------ */
/* view_message_format_record                                          */
/* ------------------------------------------------------------------ */

int test_format_record_basic(void)
{
  char buf[256];
  int  rc;

  rc = view_message_format_record("K1XX", "W1AW", "001", 'M',
                                  "Hello world",
                                  "Seq", "via",
                                  buf, sizeof(buf));

  TEST_ASSERT(rc == 0, "format_record: returns 0 on success");
  TEST_ASSERT(strstr(buf, "K1XX") != NULL,    "format_record: from_call present");
  TEST_ASSERT(strstr(buf, "W1AW") != NULL,    "format_record: call_sign present");
  TEST_ASSERT(strstr(buf, "001") != NULL,     "format_record: seq present");
  TEST_ASSERT(strstr(buf, "Hello world") != NULL, "format_record: message present");
  TEST_ASSERT(buf[strlen(buf)-1] == '\n',     "format_record: ends with newline");

  TEST_PASS("view_message_format_record: basic formatting");
}

int test_format_record_null_args(void)
{
  char buf[256];

  TEST_ASSERT(view_message_format_record(NULL, "W1AW", "001", 'M',
                                         "msg", "Seq", "via",
                                         buf, sizeof(buf)) == -1,
              "format_record: NULL from_call → -1");
  TEST_ASSERT(view_message_format_record("K1XX", NULL, "001", 'M',
                                         "msg", "Seq", "via",
                                         buf, sizeof(buf)) == -1,
              "format_record: NULL call_sign → -1");
  TEST_ASSERT(view_message_format_record("K1XX", "W1AW", "001", 'M',
                                         "msg", "Seq", "via",
                                         NULL, sizeof(buf)) == -1,
              "format_record: NULL out → -1");
  TEST_ASSERT(view_message_format_record("K1XX", "W1AW", "001", 'M',
                                         "msg", "Seq", "via",
                                         buf, 0) == -1,
              "format_record: outsz=0 → -1");

  TEST_PASS("view_message_format_record: NULL arg handling");
}


/* ------------------------------------------------------------------ */
/* view_message_format_line                                            */
/* ------------------------------------------------------------------ */

int test_format_line_normal(void)
{
  char buf[512];
  int  rc;

  rc = view_message_format_line('T', "W1AW", "K1XX", "Test message",
                                "Broadcast", buf, sizeof(buf));

  TEST_ASSERT(rc == 0,                             "format_line: returns 0");
  TEST_ASSERT(strstr(buf, "K1XX") != NULL,         "format_line: from_call present");
  TEST_ASSERT(strstr(buf, "W1AW") != NULL,         "format_line: call_sign present");
  TEST_ASSERT(strstr(buf, "Test message") != NULL, "format_line: message present");
  TEST_ASSERT(strstr(buf, "via:T") != NULL,        "format_line: via:from present");
  TEST_ASSERT(buf[strlen(buf)-1] == '\n',          "format_line: ends with newline");

  TEST_PASS("view_message_format_line: normal message");
}

int test_format_line_broadcast_java(void)
{
  char buf[512];

  view_message_format_line('I', "java-client", "K1XX", "Broadcast msg",
                           "Broadcast", buf, sizeof(buf));

  TEST_ASSERT(strstr(buf, "Broadcast") != NULL,  "format_line: broadcast label used for java");
  TEST_ASSERT(strstr(buf, "via:") == NULL,       "format_line: no via: in broadcast format");
  TEST_PASS("view_message_format_line: java broadcast");
}

int test_format_line_broadcast_user(void)
{
  char buf[512];

  view_message_format_line('I', "USER", "K1XX", "User broadcast",
                           "Broadcast", buf, sizeof(buf));

  TEST_ASSERT(strstr(buf, "Broadcast") != NULL, "format_line: broadcast label used for USER");
  TEST_PASS("view_message_format_line: USER broadcast");
}

int test_format_line_broadcast_null_label(void)
{
  char buf[512];

  /* broadcast_label==NULL → fall back to raw call_sign */
  view_message_format_line('I', "java-client", "K1XX", "Msg",
                           NULL, buf, sizeof(buf));

  TEST_ASSERT(strstr(buf, "java-client") != NULL, "format_line: raw call_sign used when label NULL");
  TEST_PASS("view_message_format_line: broadcast with NULL label uses raw call_sign");
}

int test_format_line_long_message_split(void)
{
  /* Build a 100-character message; should be split at char 95. */
  char long_msg[101];
  char buf[512];

  memset(long_msg, 'A', 95);
  memset(long_msg + 95, 'B', 5);
  long_msg[100] = '\0';

  view_message_format_line('T', "W1AW", "K1XX", long_msg,
                           "Broadcast", buf, sizeof(buf));

  /* The remainder ("BBBBB") should appear after a "\n\t" */
  TEST_ASSERT(strstr(buf, "\n\t") != NULL,       "format_line: long message contains split");
  TEST_ASSERT(strstr(buf, "BBBBB") != NULL,      "format_line: overflow chars present after split");
  TEST_PASS("view_message_format_line: long message split at char 95");
}

int test_format_line_null_args(void)
{
  char buf[512];

  TEST_ASSERT(view_message_format_line('T', NULL, "K1XX", "msg",
                                       "B", buf, sizeof(buf)) == -1,
              "format_line: NULL call_sign → -1");
  TEST_ASSERT(view_message_format_line('T', "W1AW", "K1XX", "msg",
                                       "B", NULL, 256) == -1,
              "format_line: NULL out → -1");
  TEST_ASSERT(view_message_format_line('T', "W1AW", "K1XX", "msg",
                                       "B", buf, 0) == -1,
              "format_line: outsz=0 → -1");

  TEST_PASS("view_message_format_line: NULL arg handling");
}


/* ------------------------------------------------------------------ */
/* Test dispatcher                                                     */
/* ------------------------------------------------------------------ */

typedef int (*test_fn)(void);

typedef struct
{
  const char *name;
  test_fn     fn;
} test_entry_t;

static const test_entry_t tests[] =
{
  /* init */
  { "init_default_values",                    test_init_default_values },
  { "init_null_safe",                         test_init_null_safe },
  /* strip_ssid */
  { "strip_ssid_with_ssid",                   test_strip_ssid_with_ssid },
  { "strip_ssid_without_ssid",                test_strip_ssid_without_ssid },
  { "strip_ssid_null_src",                    test_strip_ssid_null_src },
  { "strip_ssid_exact_minus",                 test_strip_ssid_exact_minus },
  /* should_display */
  { "should_display_range_zero_allows_any",   test_should_display_range_zero_allows_any_distance },
  { "should_display_range_excludes_far",      test_should_display_range_excludes_far },
  { "should_display_range_includes_near",     test_should_display_range_includes_near },
  { "should_display_tnc_only_drops_net",      test_should_display_tnc_only_drops_net },
  { "should_display_tnc_only_passes_tnc",     test_should_display_tnc_only_passes_tnc },
  { "should_display_net_only_drops_tnc",      test_should_display_net_only_drops_tnc },
  { "should_display_net_only_passes_net",     test_should_display_net_only_passes_net },
  { "should_display_mine_only_match_call",    test_should_display_mine_only_matching_call_sign },
  { "should_display_mine_only_match_from",    test_should_display_mine_only_matching_from_call },
  { "should_display_mine_only_no_match",      test_should_display_mine_only_no_match },
  { "should_display_mine_only_ignores_range", test_should_display_mine_only_ignores_range },
  { "should_display_null_vc",                 test_should_display_null_vc },
  /* format_record */
  { "format_record_basic",                    test_format_record_basic },
  { "format_record_null_args",                test_format_record_null_args },
  /* format_line */
  { "format_line_normal",                     test_format_line_normal },
  { "format_line_broadcast_java",             test_format_line_broadcast_java },
  { "format_line_broadcast_user",             test_format_line_broadcast_user },
  { "format_line_broadcast_null_label",       test_format_line_broadcast_null_label },
  { "format_line_long_message_split",         test_format_line_long_message_split },
  { "format_line_null_args",                  test_format_line_null_args },
};

#define N_TESTS ((int)(sizeof(tests) / sizeof(tests[0])))

int main(int argc, char *argv[])
{
  int i;

  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <test_name>\n", argv[0]);
    fprintf(stderr, "Available tests:\n");
    for (i = 0; i < N_TESTS; i++)
    {
      fprintf(stderr, "  %s\n", tests[i].name);
    }
    return 1;
  }

  for (i = 0; i < N_TESTS; i++)
  {
    if (strcmp(argv[1], tests[i].name) == 0)
    {
      return tests[i].fn();
    }
  }

  fprintf(stderr, "Unknown test: %s\n", argv[1]);
  return 1;
}
