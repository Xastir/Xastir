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
 * Test program for CAD object deletion in cad_objects.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <Xm/XmAll.h>

#include "tests/test_framework.h"
#include "database.h"
#include "db_funcs.h"
#include "cad_objects.h"

// Not part of a public header, only used internally by
// Draw_CAD_Objects_erase_selected(). Declared here so we can test
// it directly.
extern void CAD_object_delete(CADRow *object);

static int object_count(void)
{
  int n = 0;
  CADRow *p = CAD_list_head;
  while (p != NULL)
  {
    n++;
    p = p->next;
  }
  return(n);
}

static int vertice_count(CADRow *object)
{
  int n = 0;
  VerticeRow *v = object->start;
  while (v != NULL)
  {
    n++;
    v = v->next;
  }
  return(n);
}

static void reset_state(void)
{
  while (CAD_list_head != NULL)
  {
    CAD_object_delete(CAD_list_head);
  }
  polygon_last_x = -1;
  polygon_last_y = -1;
}

// Reproduces the bug in issue #4: draw a polygon and close it,
// start a second polygon but leave it open, then delete that
// in-progress polygon and start a third one.  Before the fix, the
// third polygon's first vertice was appended onto the first
// (closed) polygon instead of starting a new object.
int test_delete_in_progress_polygon_resets_draw_state(void)
{
  reset_state();

  // Draw and close polygon one.
  CAD_object_allocate(1, 1);
  CAD_vertice_allocate(2, 2);
  CAD_vertice_allocate(3, 3);
  polygon_last_x = -1;    // Mimics Draw_CAD_Objects_close_polygon()
  polygon_last_y = -1;

  TEST_ASSERT(object_count() == 1, "one closed polygon should exist");

  // Start polygon two, but leave it open (still mid-draw).
  CAD_object_allocate(10, 10);
  polygon_last_x = 100;
  polygon_last_y = 100;
  CAD_vertice_allocate(20, 20);

  TEST_ASSERT(object_count() == 2, "second, in-progress polygon should exist");
  TEST_ASSERT(vertice_count(CAD_list_head) == 2, "in-progress polygon should have two vertices");

  // Delete the in-progress polygon (matches
  // Draw_CAD_Objects_erase_selected() deleting the unlabeled,
  // still-open object from the CAD object list).
  CAD_object_delete(CAD_list_head);

  TEST_ASSERT(object_count() == 1, "in-progress polygon should be gone");
  TEST_ASSERT(polygon_last_x == -1, "draw state x should be reset after deleting in-progress polygon");
  TEST_ASSERT(polygon_last_y == -1, "draw state y should be reset after deleting in-progress polygon");

  // Start polygon three.  Since polygon_last_x/y are back to -1,
  // this must go through CAD_object_allocate() and create a new
  // object rather than appending to polygon one via
  // CAD_vertice_allocate().
  CAD_object_allocate(30, 30);
  polygon_last_x = 300;
  polygon_last_y = 300;

  TEST_ASSERT(object_count() == 2, "third polygon should be its own object");
  TEST_ASSERT(vertice_count(CAD_list_head) == 1, "third polygon should start with a single vertice");
  TEST_ASSERT(vertice_count(CAD_list_head->next) == 3, "first polygon's vertices should be untouched");

  reset_state();
  TEST_PASS("CAD_object_delete: deleting the in-progress polygon resets draw state");
}

// Deleting an object other than the one currently being drawn must
// not disturb the in-progress polygon's draw state.
int test_delete_other_object_does_not_reset_draw_state(void)
{
  reset_state();

  // Polygon one, closed.
  CAD_object_allocate(1, 1);
  CAD_vertice_allocate(2, 2);
  polygon_last_x = -1;
  polygon_last_y = -1;

  // Polygon two, still open (this is the one at the head).
  CAD_object_allocate(10, 10);
  polygon_last_x = 100;
  polygon_last_y = 100;
  CAD_vertice_allocate(20, 20);

  TEST_ASSERT(object_count() == 2, "two objects should exist");

  // Delete polygon one (not the head, not the one being drawn).
  CAD_object_delete(CAD_list_head->next);

  TEST_ASSERT(object_count() == 1, "unrelated polygon should be gone");
  TEST_ASSERT(polygon_last_x == 100, "draw state x should be untouched");
  TEST_ASSERT(polygon_last_y == 100, "draw state y should be untouched");
  TEST_ASSERT(vertice_count(CAD_list_head) == 2, "in-progress polygon should be unaffected");

  reset_state();
  TEST_PASS("CAD_object_delete: deleting an unrelated object leaves draw state alone");
}

// Deleting a closed polygon while nothing is being drawn should
// leave the (already inactive) draw state alone.
int test_delete_closed_polygon_while_idle(void)
{
  reset_state();

  CAD_object_allocate(1, 1);
  CAD_vertice_allocate(2, 2);
  polygon_last_x = -1;
  polygon_last_y = -1;

  CAD_object_delete(CAD_list_head);

  TEST_ASSERT(object_count() == 0, "list should be empty");
  TEST_ASSERT(polygon_last_x == -1, "draw state x should remain idle");
  TEST_ASSERT(polygon_last_y == -1, "draw state y should remain idle");

  TEST_PASS("CAD_object_delete: deleting a closed polygon while idle is a no-op on draw state");
}

typedef struct
{
  const char *name;
  int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] =
  {
    {"delete_in_progress_polygon_resets_draw_state", test_delete_in_progress_polygon_resets_draw_state},
    {"delete_other_object_does_not_reset_draw_state", test_delete_other_object_does_not_reset_draw_state},
    {"delete_closed_polygon_while_idle", test_delete_closed_polygon_while_idle},
    {NULL, NULL}
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
