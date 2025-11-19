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

extern void pad_item_name(char *name, size_t name_size);
extern void format_course_speed(char *dst, size_t dst_size, char *course_str, char *speed_str, int *course, int *speed);
extern void format_altitude(char *dst, size_t dst_size, char *altitude_str);
extern void format_zulu_time(char *dst, size_t dst_size);
extern void format_area_color_from_numeric(char * dst, size_t dst_size, unsigned int color);
extern unsigned int area_color_from_string(char *color_string);
extern void format_area_color_from_dialog(char *dst, size_t dst_size, char *color, int bright);
extern void format_area_corridor(char *dst, size_t dst_size, unsigned int type, unsigned int width);
extern void format_signpost(char *dst, size_t dst_size, char *signpost);
extern void format_probability_ring_data(char *dst, size_t dst_size, char *pmin,
                                         char *pmax);
extern void prepend_rng_phg(char *dst, size_t dst_size, char *power_gain);
extern void format_area_object_item_packet(char *dst, size_t dst_size,
                                    char *name, char object_group,
                                    char object_symbol, char *time, char *lat_str,
                                    char *lon_str, int area_type,
                                    char *area_color,
                                    int lat_offset, int lon_offset,
                                    char *speed_course, char *corridor,
                                    char *altitude, int course, int speed,
                                    int is_object, int compressed);

extern void format_signpost_object_item_packet(char *dst, size_t dst_size,
                                        char *name, char object_group,
                                        char object_symbol, char *time,
                                        char * lat_str, char *lon_str,
                                        char *speed_course,
                                        char *altitude,
                                        char *signpost,
                                        int course, int speed,
                                        int is_object, int compressed);
extern void format_omni_df_object_item_packet(char *dst, size_t dst_size,
                                       char *name,
                                       char object_group, char object_symbol,
                                       char *time,
                                       char *lat_str, char *lon_str,
                                       char *signal_gain,
                                       char *speed_course,
                                       char *altitude,
                                       int course, int speed,
                                       int is_object, int compressed);
extern void format_beam_df_object_item_packet(char *dst, size_t dst_size,
                                       char *name,
                                       char object_group, char object_symbol,
                                       char *time,
                                       char *lat_str, char *lon_str,
                                       char *bearing_string,
                                       char *NRQ,
                                       char *speed_course,
                                       char *altitude,
                                       int course, int speed,
                                       int is_object, int compressed);
extern void format_normal_object_item_packet(char *dst, size_t dst_size,
                                      char *name,
                                      char object_group, char object_symbol,
                                      char *time,
                                      char *lat_str, char *lon_str,
                                      char *speed_course,
                                      char *altitude,
                                      int course, int speed,
                                      int is_object, int compressed);
extern int reformat_killed_object_item_packet(char *dst, size_t dst_size,
                                              int is_object, int is_active);
#endif
