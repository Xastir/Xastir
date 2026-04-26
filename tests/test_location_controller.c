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
 * Unit tests for location_controller.c — the presentation-free kernel
 * extracted from location_gui.c.
 *
 * No stubs file needed: location_controller.c uses only standard C
 * library functions (stdio, string, stdlib).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

#include "tests/test_framework.h"
#include "location_controller.h"


/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/*
 * Write lines to a temp file and return the path via out_path.
 * Returns 1 on success, 0 on failure.
 */
static int write_temp_file(const char *path, const char * const *lines, int n)
{
  FILE *f;
  int i;

  f = fopen(path, "w");
  if (f == NULL)
  {
    return 0;
  }

  for (i = 0; i < n; i++)
  {
    fprintf(f, "%s\n", lines[i]);
  }

  fclose(f);
  return 1;
}


/* ------------------------------------------------------------------ */
/* Tests: lifecycle                                                     */
/* ------------------------------------------------------------------ */

int test_init_zeroes_state(void)
{
  location_controller_t lc;

  memset(&lc, 0x5a, sizeof(lc));

  location_controller_init(&lc, "/some/path", "/some/path-tmp");

  TEST_ASSERT(strcmp(lc.path, "/some/path") == 0,
              "init stores the path");
  TEST_ASSERT(strcmp(lc.tmp_path, "/some/path-tmp") == 0,
              "init stores the tmp_path");

  TEST_PASS("location_controller_init: stores paths");
}

int test_init_null_paths_are_empty(void)
{
  location_controller_t lc;

  memset(&lc, 0x5a, sizeof(lc));

  location_controller_init(&lc, NULL, NULL);

  TEST_ASSERT(lc.path[0] == '\0',    "NULL path -> empty string");
  TEST_ASSERT(lc.tmp_path[0] == '\0',"NULL tmp_path -> empty string");

  TEST_PASS("location_controller_init: NULL paths stored as empty");
}


/* ------------------------------------------------------------------ */
/* Tests: name_valid                                                    */
/* ------------------------------------------------------------------ */

int test_name_valid_basic(void)
{
  TEST_ASSERT(location_controller_name_valid("Home") == 1,
              "normal name is valid");
  TEST_ASSERT(location_controller_name_valid("A") == 1,
              "single-char name is valid");

  /* Build a name exactly LOCATION_NAME_MAX chars long — should be invalid
     (the rule is len < LOCATION_NAME_MAX, i.e. strictly shorter). */
  char long_name[LOCATION_NAME_MAX + 1];
  memset(long_name, 'x', LOCATION_NAME_MAX);
  long_name[LOCATION_NAME_MAX] = '\0';
  TEST_ASSERT(location_controller_name_valid(long_name) == 0,
              "name of exactly NAME_MAX chars is invalid");

  /* One char shorter than the limit */
  long_name[LOCATION_NAME_MAX - 1] = '\0';
  TEST_ASSERT(location_controller_name_valid(long_name) == 1,
              "name of NAME_MAX-1 chars is valid");

  TEST_PASS("location_controller_name_valid: basic cases");
}

int test_name_valid_empty_and_null(void)
{
  TEST_ASSERT(location_controller_name_valid("") == 0,
              "empty string is invalid");
  TEST_ASSERT(location_controller_name_valid(NULL) == 0,
              "NULL is invalid");

  TEST_PASS("location_controller_name_valid: empty/NULL rejected");
}


/* ------------------------------------------------------------------ */
/* Tests: parse_line                                                    */
/* ------------------------------------------------------------------ */

int test_parse_line_valid(void)
{
  char line[256];
  location_entry_t e;

  strncpy(line, "Home|4800.00N 01600.00E 100", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';

  TEST_ASSERT(location_controller_parse_line(line, &e) == 1,
              "valid line parses successfully");
  TEST_ASSERT_STR_EQ("Home",       e.name, "name extracted");
  TEST_ASSERT_STR_EQ("4800.00N",   e.lat,  "lat extracted");
  TEST_ASSERT_STR_EQ("01600.00E",  e.lon,  "lon extracted");
  TEST_ASSERT_STR_EQ("100",        e.zoom, "zoom extracted");

  TEST_PASS("location_controller_parse_line: valid line");
}

int test_parse_line_no_pipe(void)
{
  char line[256];
  location_entry_t e;

  strncpy(line, "NoSeparatorHere 4800.00N 01600.00E 100", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';

  TEST_ASSERT(location_controller_parse_line(line, &e) == 0,
              "line without '|' fails");

  TEST_PASS("location_controller_parse_line: no pipe fails");
}

int test_parse_line_too_short(void)
{
  char line[256];
  location_entry_t e;

  strncpy(line, "x|y z", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';

  /* 5 chars: shorter than minimum-meaningful 9 */
  TEST_ASSERT(location_controller_parse_line(line, &e) == 0,
              "line shorter than 9 chars fails");

  TEST_PASS("location_controller_parse_line: too short fails");
}

int test_parse_line_bad_position_format(void)
{
  char line[256];
  location_entry_t e;

  /* Position part has only 2 tokens instead of 3 */
  strncpy(line, "Home|4800.00N 01600.00E", sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';

  TEST_ASSERT(location_controller_parse_line(line, &e) == 0,
              "position missing zoom field fails");

  TEST_PASS("location_controller_parse_line: bad position format fails");
}

int test_parse_line_null_args(void)
{
  location_entry_t e;
  char line[256];

  strncpy(line, "Home|4800.00N 01600.00E 100", sizeof(line) - 1);

  TEST_ASSERT(location_controller_parse_line(NULL, &e) == 0,
              "NULL line fails");
  TEST_ASSERT(location_controller_parse_line(line, NULL) == 0,
              "NULL out fails");

  TEST_PASS("location_controller_parse_line: NULL args handled");
}


/* ------------------------------------------------------------------ */
/* Tests: file-backed operations (use /tmp scratch files)              */
/* ------------------------------------------------------------------ */

#define TMP_SYS  "/tmp/xastir_test_locations.sys"
#define TMP_SYSX "/tmp/xastir_test_locations.sys-tmp"

int test_name_exists_found_and_not_found(void)
{
  const char *lines[] = {
    "Home|4800.00N 01600.00E 100",
    "Work|5100.00N 01000.00E 200"
  };
  location_controller_t lc;

  if (!write_temp_file(TMP_SYS, lines, 2))
  {
    fprintf(stderr, "SKIP: cannot write temp file\n");
    return 0;
  }

  location_controller_init(&lc, TMP_SYS, TMP_SYSX);

  TEST_ASSERT(location_controller_name_exists(&lc, "Home") == 1,
              "existing name 'Home' found");
  TEST_ASSERT(location_controller_name_exists(&lc, "Work") == 1,
              "existing name 'Work' found");
  TEST_ASSERT(location_controller_name_exists(&lc, "Nonexistent") == 0,
              "absent name returns 0");

  unlink(TMP_SYS);
  TEST_PASS("location_controller_name_exists: found and not-found");
}

int test_name_exists_empty_file(void)
{
  const char *lines[] = {NULL};
  location_controller_t lc;

  if (!write_temp_file(TMP_SYS, lines, 0))
  {
    fprintf(stderr, "SKIP: cannot write temp file\n");
    return 0;
  }

  location_controller_init(&lc, TMP_SYS, TMP_SYSX);

  TEST_ASSERT(location_controller_name_exists(&lc, "Home") == 0,
              "empty file: no name found");

  unlink(TMP_SYS);
  TEST_PASS("location_controller_name_exists: empty file");
}

int test_find_returns_correct_entry(void)
{
  const char *lines[] = {
    "Home|4800.00N 01600.00E 100",
    "Work|5100.00N 01000.00E 200"
  };
  location_controller_t lc;
  location_entry_t e;

  if (!write_temp_file(TMP_SYS, lines, 2))
  {
    fprintf(stderr, "SKIP: cannot write temp file\n");
    return 0;
  }

  location_controller_init(&lc, TMP_SYS, TMP_SYSX);

  TEST_ASSERT(location_controller_find(&lc, "Work", &e) == 1,
              "find 'Work' succeeds");
  TEST_ASSERT_STR_EQ("Work",      e.name, "name correct");
  TEST_ASSERT_STR_EQ("5100.00N",  e.lat,  "lat correct");
  TEST_ASSERT_STR_EQ("01000.00E", e.lon,  "lon correct");
  TEST_ASSERT_STR_EQ("200",       e.zoom, "zoom correct");

  TEST_ASSERT(location_controller_find(&lc, "Missing", &e) == 0,
              "find absent name returns 0");

  unlink(TMP_SYS);
  TEST_PASS("location_controller_find: returns correct entry");
}

int test_add_appends_entry(void)
{
  const char *seed[] = { "Home|4800.00N 01600.00E 100" };
  location_controller_t lc;
  location_entry_t e;

  if (!write_temp_file(TMP_SYS, seed, 1))
  {
    fprintf(stderr, "SKIP: cannot write temp file\n");
    return 0;
  }

  location_controller_init(&lc, TMP_SYS, TMP_SYSX);

  TEST_ASSERT(location_controller_add(&lc, "Work", "5100.00N", "01000.00E", "200") == 1,
              "add returns 1 on success");

  /* Verify by finding the newly added entry */
  TEST_ASSERT(location_controller_find(&lc, "Work", &e) == 1,
              "added entry can be found");
  TEST_ASSERT_STR_EQ("Work",      e.name, "added name correct");
  TEST_ASSERT_STR_EQ("5100.00N",  e.lat,  "added lat correct");
  TEST_ASSERT_STR_EQ("01000.00E", e.lon,  "added lon correct");
  TEST_ASSERT_STR_EQ("200",       e.zoom, "added zoom correct");

  /* Original entry still present */
  TEST_ASSERT(location_controller_find(&lc, "Home", &e) == 1,
              "original entry still present after add");

  unlink(TMP_SYS);
  TEST_PASS("location_controller_add: appends new entry");
}

int test_delete_removes_entry(void)
{
  const char *lines[] = {
    "Home|4800.00N 01600.00E 100",
    "Work|5100.00N 01000.00E 200",
    "Cabin|4500.00N 01200.00E 50"
  };
  location_controller_t lc;
  location_entry_t e;

  if (!write_temp_file(TMP_SYS, lines, 3))
  {
    fprintf(stderr, "SKIP: cannot write temp file\n");
    return 0;
  }

  location_controller_init(&lc, TMP_SYS, TMP_SYSX);

  TEST_ASSERT(location_controller_delete(&lc, "Work") == 1,
              "delete returns 1");

  /* Deleted entry no longer present */
  TEST_ASSERT(location_controller_find(&lc, "Work", &e) == 0,
              "deleted entry not found");

  /* Others still present */
  TEST_ASSERT(location_controller_find(&lc, "Home",  &e) == 1,
              "Home still present after deleting Work");
  TEST_ASSERT(location_controller_find(&lc, "Cabin", &e) == 1,
              "Cabin still present after deleting Work");

  unlink(TMP_SYS);
  TEST_PASS("location_controller_delete: removes only the named entry");
}

int test_delete_nonexistent_is_noop(void)
{
  const char *lines[] = { "Home|4800.00N 01600.00E 100" };
  location_controller_t lc;
  location_entry_t e;

  if (!write_temp_file(TMP_SYS, lines, 1))
  {
    fprintf(stderr, "SKIP: cannot write temp file\n");
    return 0;
  }

  location_controller_init(&lc, TMP_SYS, TMP_SYSX);

  TEST_ASSERT(location_controller_delete(&lc, "NoSuchPlace") == 1,
              "delete of absent name returns 1 (file rewritten but unchanged)");

  /* Original still present */
  TEST_ASSERT(location_controller_find(&lc, "Home", &e) == 1,
              "Home still present after no-op delete");

  unlink(TMP_SYS);
  TEST_PASS("location_controller_delete: absent name is a no-op");
}


/* ------------------------------------------------------------------ */
/* Tests: defensive / NULL handling                                     */
/* ------------------------------------------------------------------ */

int test_defensive_null(void)
{
  location_entry_t e;

  /* init with NULL controller */
  location_controller_init(NULL, "/x", "/y");  /* must not crash */

  /* All file-backed functions with NULL controller */
  TEST_ASSERT(location_controller_name_exists(NULL, "x") == 0,
              "name_exists: NULL lc -> 0");
  TEST_ASSERT(location_controller_find(NULL, "x", &e) == 0,
              "find: NULL lc -> 0");
  TEST_ASSERT(location_controller_add(NULL, "x", "a", "b", "c") == 0,
              "add: NULL lc -> 0");
  TEST_ASSERT(location_controller_delete(NULL, "x") == 0,
              "delete: NULL lc -> 0");

  /* NULL name args */
  {
    location_controller_t lc;
    location_controller_init(&lc, TMP_SYS, TMP_SYSX);

    TEST_ASSERT(location_controller_name_exists(&lc, NULL) == 0,
                "name_exists: NULL name -> 0");
    TEST_ASSERT(location_controller_find(&lc, NULL, &e) == 0,
                "find: NULL name -> 0");
    TEST_ASSERT(location_controller_find(&lc, "x", NULL) == 0,
                "find: NULL out -> 0");
    TEST_ASSERT(location_controller_add(&lc, NULL, "a", "b", "c") == 0,
                "add: NULL name -> 0");
    TEST_ASSERT(location_controller_delete(&lc, NULL) == 0,
                "delete: NULL name -> 0");
  }

  TEST_PASS("location_controller: defensive NULL handling");
}


/* ------------------------------------------------------------------ */
/* Test runner                                                          */
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
    {"init_zeroes_state",                  test_init_zeroes_state},
    {"init_null_paths_are_empty",          test_init_null_paths_are_empty},
    {"name_valid_basic",                   test_name_valid_basic},
    {"name_valid_empty_and_null",          test_name_valid_empty_and_null},
    {"parse_line_valid",                   test_parse_line_valid},
    {"parse_line_no_pipe",                 test_parse_line_no_pipe},
    {"parse_line_too_short",               test_parse_line_too_short},
    {"parse_line_bad_position_format",     test_parse_line_bad_position_format},
    {"parse_line_null_args",               test_parse_line_null_args},
    {"name_exists_found_and_not_found",    test_name_exists_found_and_not_found},
    {"name_exists_empty_file",             test_name_exists_empty_file},
    {"find_returns_correct_entry",         test_find_returns_correct_entry},
    {"add_appends_entry",                  test_add_appends_entry},
    {"delete_removes_entry",               test_delete_removes_entry},
    {"delete_nonexistent_is_noop",         test_delete_nonexistent_is_noop},
    {"defensive_null",                     test_defensive_null},
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
