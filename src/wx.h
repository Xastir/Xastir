/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000,2001,2002  The Xastir Group
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


#ifndef __XASTIR_WX_H
#define __XASTIR_WX_H

#include "db.h"

extern void fill_wx_data(void);

extern Widget GetTopShell(Widget w);
extern void pos_dialog(Widget w);
extern char wx_station_type[];

/* from wx.c */
extern char wx_dew_point[10];
extern char wx_dew_point_on;
extern char wx_high_wind[10];
extern char wx_high_wind_on;
extern char wx_wind_chill[10];
extern char wx_wind_chill_on;
extern char wx_baro_inHg[10];
extern char wx_baro_inHg_on;
extern char wx_three_hour_baro[10];
extern char wx_three_hour_baro_on;
extern char wx_hi_temp[10];
extern char wx_hi_temp_on;
extern char wx_low_temp[10];
extern char wx_low_temp_on;
extern char wx_heat_index[10];
extern char wx_heat_index_on;
extern char wx_station_type[];


/* from wx.c */
extern time_t wx_tx_data1(char *st);
extern void wx_decode(unsigned char *wx_line, int port);
extern void fill_wx_data(void);
extern void clear_rain_data(void);
extern void tx_raw_wx_data(void);
extern void clear_local_wx_data(void);
extern void wx_last_data_check(void);
extern void wx_fill_data(int from, int type, unsigned char *data, DataRow *fill);
extern void decode_U2000_L(int from, unsigned char *data, WeatherRow *weather);
extern void decode_U2000_P(int from, unsigned char *data, WeatherRow *weather);
extern void decode_Peet_Bros(int from, unsigned char *data, WeatherRow *weather, int type);
extern void cycle_weather(void);


/* wx_gui.c */
extern void wx_alert_update_list(void);

extern void WX_station(Widget w, XtPointer clientData, XtPointer callData);

#endif
