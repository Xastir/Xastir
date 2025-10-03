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

/* test_interface_helpers.c
 * This are some stub tests to verify that the test framework is working.
  * This file is planned to be used later for a planned refactoring of interface.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int test_build(void)
{
  /* This test just verifies that the test program compiled and linked successfully */
  printf("Testing that test program builds correctly...\n");
  printf("PASS: Build test\n");
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr, "Usage: %s <test_name>\n", argv[0]);
    fprintf(stderr, "Available tests:\n");
    fprintf(stderr, "  infrastructure - Basic infrastructure test\n");
    fprintf(stderr, "  build - Test program build verification\n");
    return 1;
  }

  const char *test = argv[1];

  if (strcmp(test, "build") == 0)
  {
    return test_build();
  }
  else
  {
    fprintf(stderr, "Unknown test: %s\n", test);
    return 1;
  }

  return 0;
}
