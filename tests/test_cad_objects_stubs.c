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
 * Stub implementations for symbols referenced by cad_objects.o and
 * util.o but not exercised by the unit tests.
 *
 * These stubs allow us to link with the real cad_objects.o and
 * util.o for testing without pulling in the entire Xastir codebase.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <Xm/XmAll.h>
#include "globals.h"
#include "database.h"
#include "tests/test_framework.h"

STUB_IMPL(ll_to_utm_ups);
STUB_IMPL(utm_ups_to_ll);
STUB_IMPL(search_station_name);

char *langcode(char *code)
{
  (void)code;
  return("");
}

char *get_user_base_dir(char *dir, char *dest, size_t dest_size)
{
  (void)dir;
  if (dest != NULL && dest_size > 0)
  {
    dest[0] = '\0';
  }
  return(dest);
}

void draw_nice_string(Widget w, Pixmap where, int style, long x, long y,
                       char *text, int bgcolor, int fgcolor, int length)
{
  (void)w;
  (void)where;
  (void)style;
  (void)x;
  (void)y;
  (void)text;
  (void)bgcolor;
  (void)fgcolor;
  (void)length;
}

void draw_vector(Widget w, unsigned long x1, unsigned long y1,
                  unsigned long x2, unsigned long y2, GC gc,
                  Pixmap which_pixmap, int skip_duplicates)
{
  (void)w;
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
  (void)gc;
  (void)which_pixmap;
  (void)skip_duplicates;
}

void pos_dialog(Widget w)
{
  (void)w;
}

void redraw_symbols(Widget w)
{
  (void)w;
}

// global variables referenced but unused:
int debug_level=0;
Widget appshell;
Widget CAD_close_polygon_menu_item;
Widget da;
Pixel colors[256];
double cvt_m2len;
int english_units;
int letter_style;
GC gc_tint;
Pixmap pixmap_final;
XmFontList fontlist1;
long NW_corner_latitude, NW_corner_longitude;
long SE_corner_latitude, SE_corner_longitude;
long scale_x, scale_y;
CADRow *CAD_list_head = NULL;
