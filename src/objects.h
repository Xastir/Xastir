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

#ifndef XASTIR_OBJECTS_H
#define XASTIR_OBJECTS_H

#include <stdint.h>

// --------------------------------------------------------------------

extern void move_station_time(DataRow *p_curr, DataRow *p_time);
extern int valid_object(char *name);
extern int valid_item(char *name);
extern void check_and_transmit_objects_items(time_t time);
extern int Create_object_item_tx_string(DataRow *p_station, char *line, int line_length);
extern DataRow *construct_object_item_data_row(char *name,
                                        char *lat_str, char *lon_str,
                                        char obj_group, char obj_symbol,
                                        char *comment,
                                        char *course,
                                        char *speed,
                                        char *altitude,
                                        int area_object,
                                        int area_type,
                                        int area_filled,
                                        char *area_color,
                                        char *lat_offset_str,
                                        char *lon_offset_str,
                                        char *corridor,
                                        int signpost_object,
                                        char *signpost_str,
                                        int df_object,
                                        int omni_df,
                                        int beam_df,
                                        char *df_shgd,
                                        char *bearing,
                                        char *NRQ,
                                        int prob_circles,
                                        char *prob_min,
                                        char *prob_max,
                                        int is_object,
                                        int killed);
extern void destroy_object_item_data_row(DataRow *theDataRow);

#endif /* XASTIR_OBJECTS_H */


