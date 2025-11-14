/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
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
#ifndef __XASTIR_OBJECT_UTILS_H
#define __XASTIR_OBJECT_UTILS_H

#include <stddef.h>

extern void format_course_speed(char *dst, size_t dst_size, char *course_str, char *speed_str, int *course, int *speed);
extern void format_altitude(char *dst, size_t dst_size, char *altitude_str);
extern void format_zulu_time(char *dst, size_t dst_size);
extern void format_area_color_from_numeric(char * dst, size_t dst_size, unsigned int color);
extern unsigned int area_color_from_string(char *color_string);
extern void format_area_color_from_dialog(char *dst, size_t dst_size, char *color, int bright);
extern void format_area_corridor(char *dst, size_t dst_size, unsigned int type, unsigned int width);
extern void format_probability_ring_data(char *dst, size_t dst_size, char *pmin,
                                         char *pmax);

#endif
