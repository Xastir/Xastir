/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025 The Xastir Group
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
 * Test program for object_utils.c functions
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "tests/test_framework.h"

/* forward declarations (so we don't need to include object_utils.h */
void format_course_speed(char *dst, size_t dst_size, char *course_str, char *speed_str, int *course, int *speed);

/* test cases for format_course_speed */

int test_format_course_speed_basic(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "090","005", &course, &speed);

  TEST_ASSERT_STR_EQ("090/005",course_speed_string, "Valid course and speed should be CSE/SPD");
  TEST_ASSERT(course == 90, "Integer course should be 90");
  TEST_ASSERT(speed == 5, "Integer speed should be 5");
  TEST_PASS("format_course_speed with valid input for both");
}

int test_format_course_speed_bad_course(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "390","005", &course, &speed);

  TEST_ASSERT_STR_EQ(".../005",course_speed_string, "Valid course and speed should be CSE/SPD, invalid course should be ...");
  TEST_ASSERT(course == 0, "Integer course should be 00");
  TEST_ASSERT(speed == 5, "Integer speed should be 5");
  TEST_PASS("format_course_speed with bad input for course");
}

int test_format_course_speed_null_course(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "","005", &course, &speed);

  TEST_ASSERT_STR_EQ(".../005",course_speed_string, "Valid course and speed should be CSE/SPD, null course should be ...");
  TEST_ASSERT(course == 0, "Integer course should be 00");
  TEST_ASSERT(speed == 5, "Integer speed should be 5");
  TEST_PASS("format_course_speed with null input for course");
}

int test_format_course_speed_bad_speed(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "090","1005", &course, &speed);

  TEST_ASSERT_STR_EQ("090/...",course_speed_string, "Valid course and speed should be CSE/SPD, invalid speed should be ...");
  TEST_ASSERT(course == 90, "Integer course should be 90");
  TEST_ASSERT(speed == 0, "Integer speed should be 0");
  TEST_PASS("format_course_speed with bad input for speed");
}

int test_format_course_speed_null_speed(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "090","", &course, &speed);

  TEST_ASSERT_STR_EQ("090/...",course_speed_string, "Valid course and speed should be CSE/SPD, missing speed should be ...");
  TEST_ASSERT(course == 90, "Integer course should be 90");
  TEST_ASSERT(speed == 0, "Integer speed should be 0");
  TEST_PASS("format_course_speed with null input for speed");
}

int test_format_course_speed_null_inputs(void)
{
  char course_speed_string[8];
  int speed,course;
  format_course_speed(course_speed_string,sizeof(course_speed_string),
                      "","", &course, &speed);

  TEST_ASSERT_STR_EQ("",course_speed_string, "Valid course and speed should be CSE/SPD, missing both should be empty string");
  TEST_ASSERT(course == 0, "Integer course should be 90");
  TEST_ASSERT(speed == 0, "Integer speed should be 0");
  TEST_PASS("format_course_speed with null input for both");
}

/* Test runner */
typedef struct {
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] = {
    /* format_course_speed tests */
    {"format_course_speed_basic", test_format_course_speed_basic},
    {"format_course_speed_bad_course", test_format_course_speed_bad_course},
    {"format_course_speed_null_course", test_format_course_speed_null_course},
    {"format_course_speed_bad_speed", test_format_course_speed_bad_speed},
    {"format_course_speed_null_speed", test_format_course_speed_null_speed},
    {"format_course_speed_null_inputs", test_format_course_speed_null_inputs},
    {NULL,NULL}
  };

  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <test name>\n", argv[0]);
    fprintf(stderr, "Available tests: \n");
    for (int i = 0; tests[i].name != NULL; i++)
    {
      fprintf(stderr, "  %s\n", tests[i].name);
    }
    return 1;
  }


  const char *test_name = argv[1];

  /* Run the requested test */
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
