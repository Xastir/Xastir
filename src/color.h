/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2019 The Xastir Group
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

#ifndef XASTIR_COLOR_H
#define XASTIR_COLOR_H

#define MAX_COLORS 81

#define MAX_COLORNAME 40
typedef struct
{
  char colorname[MAX_COLORNAME];
  XColor color;
} color_load;

typedef enum
{
  NOT_TRUE_NOR_DIRECT,
  RGB_565,
  RGB_555,
  RGB_888,
  RGB_OTHER
} Pixel_Format;
extern Pixel_Format visual_type;
extern int visual_depth;

/* from color.c */
extern int load_color_file(void);
extern Pixel GetPixelByName(Widget w, char *colorname);
extern void setup_visual_info(Display* dpy, int scr);
extern void pack_pixel_bits(unsigned short r, unsigned short g, unsigned short b, unsigned long* pixel);

#endif /* XASTIR_COLOR_H */

