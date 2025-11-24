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

#include "database.h"
#include "objects.h"

int test_constructor_null_everything(void)
{
  DataRow *theDataRow;
  theDataRow=construct_object_item_data_row("","","",  // name, lat/lon
                                            '/','/',    // group, symbol
                                            "",         //comment
                                            "","",      //course, speed
                                            "",         //altitude
                                            0,0,0,      //area, type, filled
                                            "",         // area color
                                            "","",      // offsets
                                            "",         // corridor
                                            0,          // signpost
                                            "",         // signpost string
                                            0, 0, 0,    // df, omni, beam
                                            "",         // shgd
                                            "",         //bearing
                                            "",         // NRQ
                                            0,          // prob circles
                                            "","",      // prob min, max
                                            1,          // is_object
                                            0);         // killed
  TEST_ASSERT(theDataRow == NULL, "Constructor returns null appropriately");
  TEST_PASS("construct_object_item_data_row");
}

/* Test runner */
typedef struct {
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] = {
    {"constructor_null_everything",test_constructor_null_everything},
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
