/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <Xm/XmAll.h>

#include "xa_config.h"
#include "interface.h"
#include "xastir.h"
#include "main.h"
#include "util.h"
#include "bulletin_gui.h"
#include "list_gui.h"
#include "messages.h"
#include "draw_symbols.h"
#include "maps.h"
#include "track_gui.h"
#include "geo.h"

#define CONFIG_FILE      "config/xastir.cnf"
#define CONFIG_FILE_BAK1 "config/xastir.cnf.1"
#define CONFIG_FILE_BAK2 "config/xastir.cnf.2"
#define CONFIG_FILE_BAK3 "config/xastir.cnf.3"
#define CONFIG_FILE_TMP  "config/xastir.tmp"

#define MAX_VALUE 300





void store_string(FILE * fout, char *option, char *value) {

//    if (debug_level & 1)
//        fprintf(stderr,"Store String Start\n");

    fprintf (fout, "%s:%s\n", option, value);

//    if (debug_level & 1)
//        fprintf(stderr,"Store String Stop\n");

}





void store_char(FILE * fout, char *option, char value) {
    char value_o[2];

    value_o[0] = value;
    value_o[1] = '\0';
    store_string (fout, option, value_o);
}





void store_int(FILE * fout, char *option, int value) {
    char value_o[MAX_VALUE];

    sprintf (value_o, "%d", value);
    store_string (fout, option, value_o);
}





void store_long (FILE * fout, char *option, long value) {
    char value_o[MAX_VALUE];

    sprintf (value_o, "%ld", value);
    store_string (fout, option, value_o);
}





FILE * fin;

void input_close(void)
{
    if(fin != NULL)
        (void)fclose(fin);
    fin = NULL;
}





/*
    This function will read the configuration file (xastir.cnf) until it finds
    the requested option. When the requested option is found it will return
    the value of that option.
    The return value of the function will be 1 if the option is found and 0
    if the option is not found.
*/
int get_string(char *option, char *value) {
    char config_file[MAX_VALUE];
    char config_line[256];
    char *option_in;
    char *value_in;
    short option_found;

    option_found = 0;

    if (fin == NULL) {
        strcpy (config_file, get_user_base_dir (CONFIG_FILE));
        // filecreate() refuses to create a new file if it already
        // exists.
        (void)filecreate(config_file);
        fin = fopen (config_file, "r");
    }

    if (fin != NULL) {
        (void)fseek(fin, 0, SEEK_SET);
        while ((fgets (&config_line[0], 256, fin) != NULL) && !option_found) {
            option_in = strtok (config_line, " \t:\r\n");
            if (strcmp (option_in, option) == 0) {
                option_found = 1;
                value_in = strtok (NULL, "\t\r\n");
                if (value_in == NULL)
                    value = "";
                else
                    strcpy (value, value_in);
            }
        }
    }
    else
        fprintf(stderr,"Couldn't open file: %s\n", config_file);

    return (option_found);
}





int get_char(char *option, char *value) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o);
    if (ret)
        *value = value_o[0];

    return (ret);
}





// Snags an int and checks whether it is within the correct range.
// If not, it assigns a default value.
int get_int(char *option, int *value, int low, int high, int def) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o);
    if (ret && (atoi(value_o) >= low) && (atoi(value_o) <= high) )
        *value = atoi (value_o);
    else {
        fprintf(stderr,"Found out-of-range or non-existent value (%d) for %s in config file, changing to %d\n",
            atoi(value_o),
            option,
            def);
        *value = def;
    }
    return (ret);
}





// Snags a long and checks whether it is within the correct range.
// If not, it assigns a default value.
int get_long(char *option, long *value, long low, long high, long def) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o);
    if (ret && (atol(value_o) >= low) && (atol(value_o) <= high) )
        *value = atol (value_o);
    else {
        fprintf(stderr,"Found out-of-range or non-existent value (%ld) for %s in config file, changing to %ld\n",
            atol(value_o),
            option,
            def);
        *value = def;
    }
    return (ret);
}





char *get_user_base_dir(char *dir) {
    static char base[MAX_VALUE];
    char *env_ptr;

    strcpy (base, ((env_ptr = getenv ("XASTIR_USER_BASE")) != NULL) ? env_ptr : user_dir);

    if (base[strlen (base) - 1] != '/')
        strcat (base, "/");

    strcat (base, ".xastir/");
    return strcat (base, dir);
}





char *get_data_base_dir(char *dir) {
    static char base[MAX_VALUE];
    char *env_ptr;

    // Snag this variable from the environment if it exists there,
    // else grab it from the define in config.h
    strcpy (base, ((env_ptr = getenv ("XASTIR_DATA_BASE")) != NULL) ? env_ptr : XASTIR_DATA_BASE);
    if (base[strlen (base) - 1] != '/')
        strcat (base, "/");

    return strcat (base, dir);
}





// Care should be taken here to make sure that no out-of-range data
// is saved, as it will mess up Xastir startup from that point on.
// Also: Config file should be owned by the user, and not by root.
// If chmod 4755 is done on the executable, then the config file ends
// up being owned by root from then on.
//
// Yea, I could have made this nicer by algorithmically figuring out
// the backup filenames and rotating among .1 through .9.  Perhaps
// next go-around!  Just having multiples is a big win, in case some
// of them get blown away.
//
// Another step that needs to be made is to restore config settings
// for the cases where Xastir comes up with a nonexistent or empty
// xastir.cnf file.  If the backups exist, we should copy them
// across.
//
void save_data(void)  {
    int i;
    char name_temp[20];
    char name[50];
    FILE * fout;
    char config_file[MAX_VALUE];
    char config_file_bak1[MAX_VALUE];
    char config_file_bak2[MAX_VALUE];
    char config_file_bak3[MAX_VALUE];
    struct stat file_status;


//    if (debug_level & 1)
//        fprintf(stderr,"Store String Start\n");

    strcpy (config_file, get_user_base_dir (CONFIG_FILE));
    strcpy (config_file_bak1, get_user_base_dir (CONFIG_FILE_BAK1));
    strcpy (config_file_bak2, get_user_base_dir (CONFIG_FILE_BAK2));
    strcpy (config_file_bak3, get_user_base_dir (CONFIG_FILE_BAK3));

    // Remove the bak3 file
    unlink (config_file_bak3);
    if (stat(config_file_bak3, &file_status) == 0) {
        // We got good status.  That means it didn't get deleted!
        fprintf(stderr,
            "Couldn't delete backup file: %s, cancelling save_data()\n",
            config_file_bak3);
        return;
    }

    // Rename bak2 to bak3
    // NOTE: bak won't exist until a couple of saves have happened.
    //
    // Check whether bak2 exists
    if (stat(config_file_bak2, &file_status) == 0) {
        if (!S_ISREG(file_status.st_mode)) {
            fprintf(stderr,
                "Couldn't stat %s, cancelling save_data()\n",
                config_file_bak2);
            return;
        }
        if ( rename (config_file_bak2, config_file_bak3) ) {
            fprintf(stderr,
                "Couldn't rename %s to %s, cancelling save_data()\n",
                config_file_bak2,
                config_file_bak3);

            // Attempt to restore to previous state
            // Nothing to do here!  bak3 was deleted.
            return;
        }
    }

    // Rename bak1 to bak2
    // NOTE: bak won't exist until a couple of saves have happened.
    //
    // Check whether bak1 exists
    if (stat(config_file_bak1, &file_status) == 0) {
        if (!S_ISREG(file_status.st_mode)) {
            fprintf(stderr,
                "Couldn't stat %s, cancelling save_data()\n",
                config_file_bak1);
            return;
        }
        if ( rename (config_file_bak1, config_file_bak2) ) {
            fprintf(stderr,
                "Couldn't rename %s to %s, cancelling save_data()\n",
                config_file_bak1,
                config_file_bak2);

            // Attempt to restore to previous state
            rename (config_file_bak3, config_file_bak2);
            return;
        }
    }

    // Rename config to bak1
    // NOTE: config won't exist until the first save happens.
    //
    // Check whether config exists
    if (stat(config_file, &file_status) == 0) {
        if (!S_ISREG(file_status.st_mode)) {
            fprintf(stderr,
                "Couldn't stat %s, cancelling save_data()\n",
                config_file);
            return;
        }
        if ( rename (config_file, config_file_bak1) ) {
            fprintf(stderr,
                "Couldn't rename %s to %s, cancelling save_data()\n",
                config_file,
                config_file_bak1);

            // Attempt to restore to previous state
            rename (config_file_bak2, config_file_bak1);
            rename (config_file_bak3, config_file_bak2);
            return;
        }
    }

    // Now save to a new config file

    fout = fopen (config_file, "a");
    if (fout != NULL) {
        if (debug_level & 1)
            fprintf(stderr,"Save Data Start\n");

        /* language */
        store_string (fout, "LANGUAGE", lang_to_use);

        /* my data */
        store_string (fout, "STATION_CALLSIGN", my_callsign);

        store_string (fout, "STATION_LAT", my_lat);
        store_string (fout, "STATION_LONG", my_long);
        store_char (fout, "STATION_GROUP", my_group);
        store_char (fout, "STATION_SYMBOL", my_symbol);
        store_char (fout, "STATION_MESSAGE_TYPE", aprs_station_message_type);
        store_string (fout, "STATION_POWER", my_phg);
        store_string (fout, "STATION_COMMENTS", my_comment);
        if (debug_level & 1)
            fprintf(stderr,"Save Data 1\n");

        /* default values */
        store_long (fout, "SCREEN_WIDTH", screen_width);

        store_long (fout, "SCREEN_HEIGHT", screen_height);
        store_long (fout, "SCREEN_LAT", mid_y_lat_offset);
        store_long (fout, "SCREEN_LONG", mid_x_long_offset);

        store_int (fout, "COORDINATE_SYSTEM", coordinate_system);

        store_int (fout, "STATION_TRANSMIT_AMB", position_amb_chars);

        if (scale_y > 0)
            store_long (fout, "SCREEN_ZOOM", scale_y);
        else
            store_long (fout, "SCREEN_ZOOM", 1);

        store_int (fout, "MAP_BGCOLOR", map_background_color);

        store_int (fout, "MAP_DRAW_FILLED_COLORS", map_color_fill);

#if !defined(NO_GRAPHICS)
#if defined(HAVE_IMAGEMAGICK)
        sprintf (name, "%f", imagemagick_gamma_adjust);
        store_string(fout, "IMAGEMAGICK_GAMMA_ADJUST", name);
#endif  // HAVE_IMAGEMAGICK
        sprintf (name, "%f", raster_map_intensity);
        store_string(fout, "RASTER_MAP_INTENSITY", name);
#endif  // NO_GRAPHICS

        store_int (fout, "MAP_LETTERSTYLE", letter_style);
        store_int (fout, "MAP_ICONOUTLINESTYLE", icon_outline_style);
        store_int (fout, "MAP_WX_ALERT_STYLE", wx_alert_style);
        store_string(fout, "ALTNET_CALL", altnet_call);
        store_int(fout, "ALTNET", altnet);
        store_int(fout, "SKIP_DUPE_CHECK", skip_dupe_checking);
        store_string (fout, "AUTO_MAP_DIR", AUTO_MAP_DIR);
        store_string (fout, "ALERT_MAP_DIR", ALERT_MAP_DIR);
        store_string (fout, "SELECTED_MAP_DIR", SELECTED_MAP_DIR);
        store_string (fout, "SELECTED_MAP_DATA", SELECTED_MAP_DATA);
        store_string (fout, "MAP_INDEX_DATA", MAP_INDEX_DATA);
        store_string (fout, "SYMBOLS_DIR", SYMBOLS_DIR);
        store_string (fout, "SOUND_DIR", SOUND_DIR);
        store_string (fout, "GROUP_DATA_FILE", group_data_file);
        store_string (fout, "GNIS_FILE", locate_gnis_filename);
        store_string (fout, "GEOCODE_FILE", geocoder_map_filename);
        store_int (fout, "SHOW_FIND_TARGET", show_destination_mark);

        /* maps */
        store_int (fout, "MAPS_LONG_LAT_GRID", long_lat_grid);
        store_int (fout, "MAPS_LEVELS", map_color_levels);
        store_int (fout, "MAPS_LABELS", map_labels);
        store_int (fout, "MAPS_AUTO_MAPS", map_auto_maps);
        store_int (fout, "MAPS_AUTO_MAPS_SKIP_RASTER", auto_maps_skip_raster);
        store_int (fout, "MAPS_INDEX_ON_STARTUP", index_maps_on_startup);

        store_string (fout, "MAPS_LABEL_FONT_TINY", rotated_label_fontname[FONT_TINY]);
        store_string (fout, "MAPS_LABEL_FONT_SMALL", rotated_label_fontname[FONT_SMALL]);
        store_string (fout, "MAPS_LABEL_FONT_MEDIUM", rotated_label_fontname[FONT_MEDIUM]);
        store_string (fout, "MAPS_LABEL_FONT_LARGE", rotated_label_fontname[FONT_LARGE]);
        store_string (fout, "MAPS_LABEL_FONT_HUGE", rotated_label_fontname[FONT_HUGE]);
        store_string (fout, "MAPS_LABEL_FONT", rotated_label_fontname[FONT_DEFAULT]);
//N0VH
#if defined(HAVE_IMAGEMAGICK)
        store_int (fout, "TIGERMAP_TIMEOUT", tigermap_timeout);
        store_int (fout, "TIGERMAP_SHOW_GRID", tiger_show_grid);
        store_int (fout, "TIGERMAP_SHOW_COUNTIES", tiger_show_counties);
        store_int (fout, "TIGERMAP_SHOW_CITIES", tiger_show_cities);
        store_int (fout, "TIGERMAP_SHOW_PLACES", tiger_show_places);
        store_int (fout, "TIGERMAP_SHOW_MAJROADS", tiger_show_majroads);
        store_int (fout, "TIGERMAP_SHOW_STREETS", tiger_show_streets);
        store_int (fout, "TIGERMAP_SHOW_RAILROAD", tiger_show_railroad);
        store_int (fout, "TIGERMAP_SHOW_STATES", tiger_show_states);
        store_int (fout, "TIGERMAP_SHOW_INTERSTATE", tiger_show_interstate);
        store_int (fout, "TIGERMAP_SHOW_USHWY", tiger_show_ushwy);
        store_int (fout, "TIGERMAP_SHOW_STATEHWY", tiger_show_statehwy);
        store_int (fout, "TIGERMAP_SHOW_WATER", tiger_show_water);
        store_int (fout, "TIGERMAP_SHOW_LAKES", tiger_show_lakes);
        store_int (fout, "TIGERMAP_SHOW_MISC", tiger_show_misc);
#endif //HAVE_IMAGEMAGICK

        // filter values
        // NOT SAVED: Select_.none
        store_int (fout, "DISPLAY_MY_STATION",            Select_.mine);
        store_int (fout, "DISPLAY_TNC_STATIONS",          Select_.tnc);
        store_int (fout, "DISPLAY_TNC_DIRECT_STATIONS",   Select_.direct);
        store_int (fout, "DISPLAY_TNC_VIADIGI_STATIONS",  Select_.via_digi);
        store_int (fout, "DISPLAY_NET_STATIONS",          Select_.net);
        store_int (fout, "DISPLAY_TACTICAL_STATIONS",     Select_.tactical);
        store_int (fout, "DISPLAY_OLD_STATION_DATA",      Select_.old_data);
        store_int (fout, "DISPLAY_STATIONS",              Select_.stations);
        store_int (fout, "DISPLAY_FIXED_STATIONS",        Select_.fixed_stations);
        store_int (fout, "DISPLAY_MOVING_STATIONS",       Select_.moving_stations);
        store_int (fout, "DISPLAY_WEATHER_STATIONS",      Select_.weather_stations);
        store_int (fout, "DISPLAY_OBJECTS",               Select_.objects);
        store_int (fout, "DISPLAY_STATION_WX_OBJ",        Select_.weather_objects);
        store_int (fout, "DISPLAY_WATER_GAGE_OBJ",        Select_.gauge_objects);
        store_int (fout, "DISPLAY_OTHER_OBJECTS",         Select_.other_objects);

        // display values
        store_int (fout, "DISPLAY_CALLSIGN",              Display_.callsign);
        store_int (fout, "DISPLAY_SYMBOL",                Display_.symbol);
        store_int (fout, "DISPLAY_SYMBOL_ROTATE",         Display_.symbol_rotate);
        store_int (fout, "DISPLAY_STATION_PHG",           Display_.phg);
        store_int (fout, "DISPLAY_DEFAULT_PHG",           Display_.default_phg);
        store_int (fout, "DISPLAY_MOBILES_PHG",           Display_.phg_of_moving);
        store_int (fout, "DISPLAY_ALTITUDE",              Display_.altitude);
        store_int (fout, "DISPLAY_COURSE",                Display_.course);
        store_int (fout, "DISPLAY_SPEED",                 Display_.speed);
        store_int (fout, "DISPLAY_SPEED_SHORT",           Display_.speed_short);
        store_int (fout, "DISPLAY_DIST_COURSE",           Display_.dist_bearing);
        store_int (fout, "DISPLAY_WEATHER",               Display_.weather);
        store_int (fout, "DISPLAY_STATION_WX",            Display_.weather_text);
        store_int (fout, "DISPLAY_TEMP_ONLY",             Display_.temperature_only);
        store_int (fout, "DISPLAY_WIND_BARB",             Display_.wind_barb);
        store_int (fout, "DISPLAY_STATION_TRAILS",        Display_.trail);
        store_int (fout, "DISPLAY_LAST_HEARD",            Display_.last_heard);
        store_int (fout, "DISPLAY_POSITION_AMB",          Display_.ambiguity);
        store_int (fout, "DISPLAY_DF_INFO",               Display_.df_data);
        store_int (fout, "DISPLAY_DEAD_RECKONING_INFO",   Display_.dr_data);
        store_int (fout, "DISPLAY_DEAD_RECKONING_ARC",    Display_.dr_arc);
        store_int (fout, "DISPLAY_DEAD_RECKONING_COURSE", Display_.dr_course);
        store_int (fout, "DISPLAY_DEAD_RECKONING_SYMBOL", Display_.dr_symbol);


        store_int (fout, "DISPLAY_UNITS_ENGLISH",         units_english_metric);
        store_int (fout, "DISPLAY_DIST_BEAR_STATUS",      do_dbstatus);


        // Interface values
        store_int (fout, "DISABLE_TRANSMIT",      transmit_disable);
        store_int (fout, "DISABLE_POSIT_TX",      posit_tx_disable);
        store_int (fout, "DISABLE_OBJECT_TX",     object_tx_disable);


        for (i = 0; i < MAX_IFACE_DEVICES; i++) {
            sprintf (name_temp, "DEVICE%0d_", i);

            strcpy (name, name_temp);
            strcat (name, "TYPE");
            store_int (fout, name, devices[i].device_type);

            strcpy (name, name_temp);
            strcat (name, "NAME");
            store_string (fout, name, devices[i].device_name);

            strcpy (name, name_temp);
            strcat (name, "RADIO_PORT");
            store_string (fout, name, devices[i].radio_port);

            strcpy (name, name_temp);
            strcat (name, "INTERFACE_COMMENT");
            store_string (fout, name, devices[i].comment);

            strcpy (name, name_temp);
            strcat (name, "HOST");
            store_string (fout, name, devices[i].device_host_name);

            strcpy (name, name_temp);
            strcat (name, "PASSWD");
            store_string (fout, name, devices[i].device_host_pswd);

            strcpy (name, name_temp);
            strcat (name, "FILTER_PARAMS");
            store_string (fout, name, devices[i].device_host_filter_string);

            strcpy (name, name_temp);
            strcat (name, "UNPROTO1");
            store_string (fout, name, devices[i].unproto1);

            strcpy (name, name_temp);
            strcat (name, "UNPROTO2");
            store_string (fout, name, devices[i].unproto2);

            strcpy (name, name_temp);
            strcat (name, "UNPROTO3");
            store_string (fout, name, devices[i].unproto3);

            strcpy (name, name_temp);
            strcat (name, "UNPROTO_IGATE");
            store_string (fout, name, devices[i].unproto_igate);

            strcpy (name, name_temp);
            strcat (name, "TNC_UP_FILE");
            store_string (fout, name, devices[i].tnc_up_file);

            strcpy (name, name_temp);
            strcat (name, "TNC_DOWN_FILE");
            store_string (fout, name, devices[i].tnc_down_file);

            strcpy (name, name_temp);
            strcat (name, "TNC_TXDELAY");
            store_string (fout, name, devices[i].txdelay);

            strcpy (name, name_temp);
            strcat (name, "TNC_PERSISTENCE");
            store_string (fout, name, devices[i].persistence);

            strcpy (name, name_temp);
            strcat (name, "TNC_SLOTTIME");
            store_string (fout, name, devices[i].slottime);

            strcpy (name, name_temp);
            strcat (name, "TNC_TXTAIL");
            store_string (fout, name, devices[i].txtail);

            strcpy (name, name_temp);
            strcat (name, "TNC_FULLDUPLEX");
            store_int (fout, name, devices[i].fullduplex);

            strcpy (name, name_temp);
            strcat (name, "SPEED");
            store_int (fout, name, devices[i].sp);

            strcpy (name, name_temp);
            strcat (name, "STYLE");
            store_int (fout, name, devices[i].style);

            strcpy (name, name_temp);
            strcat (name, "IGATE_OPTION");
            store_int (fout, name, devices[i].igate_options);

            strcpy (name, name_temp);
            strcat (name, "TXMT");
            store_int (fout, name, devices[i].transmit_data);

            strcpy (name, name_temp);
            strcat (name, "RELAY_DIGIPEAT");
            store_int (fout, name, devices[i].relay_digipeat);

            strcpy (name, name_temp);
            strcat (name, "RECONN");
            store_int (fout, name, devices[i].reconnect);

            strcpy (name, name_temp);
            strcat (name, "ONSTARTUP");
            store_int (fout, name, devices[i].connect_on_startup);

            strcpy (name, name_temp);
            strcat(name, "GPSRETR");
            store_int (fout, name, devices[i].gps_retrieve);

            strcpy (name, name_temp);
            strcat (name, "SETTIME");
            store_int (fout, name, devices[i].set_time);
        }
        /* TNC */
        store_int (fout, "TNC_LOG_DATA", log_tnc_data);
        store_string (fout, "LOGFILE_TNC", LOGFILE_TNC);

        /* NET */
        store_int (fout, "NET_LOG_DATA", log_net_data);

        store_int (fout, "NET_RUN_AS_IGATE", operate_as_an_igate);
        store_int (fout, "NETWORK_WAITTIME", NETWORK_WAITTIME);

        // LOGGING
        store_int (fout, "LOG_IGATE", log_igate);
        store_int (fout, "LOG_WX", log_wx);
        store_string (fout, "LOGFILE_IGATE", LOGFILE_IGATE);
        store_string (fout, "LOGFILE_NET", LOGFILE_NET);
        store_string (fout, "LOGFILE_WX", LOGFILE_WX);

        // SNAPSHOTS
        store_int (fout, "SNAPSHOTS_ENABLED", snapshots_enabled);

        /* WX ALERTS */
        store_long (fout, "WX_ALERTS_REFRESH_TIME", (long)WX_ALERTS_REFRESH_TIME);

        /* GPS */
        store_long (fout, "GPS_TIME", (long)gps_time);    /* still needed */

        /* POSIT RATE */
        store_long (fout, "POSIT_RATE", (long)POSIT_rate);

        /* OBJECT RATE */
        store_long (fout, "OBJECT_RATE", (long)OBJECT_rate);

        /* UPDATE DR RATE */
        store_long (fout, "UPDATE_DR_RATE", (long)update_DR_rate);

        /* station broadcast type */
        store_int (fout, "BST_TYPE", output_station_type);

#ifdef TRANSMIT_RAW_WX
        store_int (fout, "BST_WX_RAW", transmit_raw_wx);
#endif  // TRANSMIT_RAW_WX

        store_int (fout, "BST_COMPRESSED_POSIT", transmit_compressed_posit);

        store_int (fout, "COMPRESSED_OBJECTS_ITEMS", transmit_compressed_objects_items);

        store_int (fout, "SMART_BEACONING", smart_beaconing);
        store_int (fout, "SB_TURN_MIN", sb_turn_min);
        store_int (fout, "SB_TURN_SLOPE", sb_turn_slope);
        store_int (fout, "SB_TURN_TIME", sb_turn_time);
        store_int (fout, "SB_POSIT_FAST", sb_posit_fast);
        store_int (fout, "SB_POSIT_SLOW", sb_posit_slow);
        store_int (fout, "SB_LOW_SPEED_LIMIT", sb_low_speed_limit);
        store_int (fout, "SB_HIGH_SPEED_LIMIT", sb_high_speed_limit);

        store_int (fout, "POP_UP_NEW_BULLETINS", pop_up_new_bulletins);
        store_int (fout, "VIEW_ZERO_DISTANCE_BULLETINS", view_zero_distance_bulletins);

        store_int (fout, "WARN_ABOUT_MOUSE_MODIFIERS", warn_about_mouse_modifiers);

        /* -dk7in- variable beacon interval */
        /*         mobile:   max  2 min */
        /*         fixed:    max 15 min  */
        if ((output_station_type >= 1) && (output_station_type <= 3))
            max_transmit_time = (time_t)120l;       /*  2 min @ mobile */
        else
            max_transmit_time = (time_t)900l;       /* 15 min @ fixed */

        /* Audio Alarms */
        store_string (fout, "SOUND_COMMAND", sound_command);

        store_int (fout, "SOUND_PLAY_ONS", sound_play_new_station);
        store_string (fout, "SOUND_ONS_FILE", sound_new_station);
        store_int (fout, "SOUND_PLAY_ONM", sound_play_new_message);
        store_string (fout, "SOUND_ONM_FILE", sound_new_message);

        store_int (fout, "SOUND_PLAY_PROX", sound_play_prox_message);
        store_string (fout, "SOUND_PROX_FILE", sound_prox_message);
        store_string (fout, "PROX_MIN", prox_min);
        store_string (fout, "PROX_MAX", prox_max);
        store_int (fout, "SOUND_PLAY_BAND", sound_play_band_open_message);
        store_string (fout, "SOUND_BAND_FILE", sound_band_open_message);
        store_string (fout, "BANDO_MIN", bando_min);
        store_string (fout, "BANDO_MAX", bando_max);
        store_int (fout, "SOUND_PLAY_WX_ALERT", sound_play_wx_alert_message);
        store_string (fout, "SOUND_WX_ALERT_FILE", sound_wx_alert_message);

#ifdef HAVE_FESTIVAL
            /* Festival speech settings */
        store_int (fout, "SPEAK_NEW_STATION",festival_speak_new_station);
        store_int (fout, "SPEAK_PROXIMITY_ALERT",festival_speak_proximity_alert);
        store_int (fout, "SPEAK_TRACKED_ALERT",festival_speak_tracked_proximity_alert);
        store_int (fout, "SPEAK_BAND_OPENING",festival_speak_band_opening);
        store_int (fout, "SPEAK_MESSAGE_ALERT",festival_speak_new_message_alert);
        store_int (fout, "SPEAK_MESSAGE_BODY",festival_speak_new_message_body);
        store_int (fout, "SPEAK_WEATHER_ALERT",festival_speak_new_weather_alert);
        store_int (fout, "SPEAK_ID",festival_speak_ID);
#endif  // HAVE_FESTIVAL
        store_int (fout, "ATV_SCREEN_ID", ATV_screen_ID);

        /* defaults */
        store_long (fout, "DEFAULT_STATION_OLD", (long)sec_old);

        store_long (fout, "DEFAULT_STATION_CLEAR", (long)sec_clear);
        store_long(fout, "DEFAULT_STATION_REMOVE", (long)sec_remove);
        store_string (fout, "HELP_DATA", HELP_FILE);

        store_string (fout, "MESSAGE_COUNTER", message_counter);

        store_string (fout, "AUTO_MSG_REPLY", auto_reply_message);
        store_int (fout, "DISPLAY_PACKET_TYPE", Display_packet_data_type);

        store_int (fout, "BULLETIN_RANGE", bulletin_range);
        store_int(fout,"VIEW_MESSAGE_RANGE",vm_range);
        store_int(fout,"VIEW_MESSAGE_LIMIT",view_message_limit);

        /* printer variables */
        store_int (fout, "PRINT_ROTATED", print_rotated);
        store_int (fout, "PRINT_AUTO_ROTATION", print_auto_rotation);
        store_int (fout, "PRINT_AUTO_SCALE", print_auto_scale);
        store_int (fout, "PRINT_IN_MONOCHROME", print_in_monochrome);
        store_int (fout, "PRINT_INVERT_COLORS", print_invert);

        /* Rain Gauge Type, set in the Serial Weather interface
            properties dialog, but really a global default */
        store_int (fout, "RAIN_GAUGE_TYPE", WX_rain_gauge_type);


        /* list attributes */
        for (i = 0; i < LST_NUM; i++) {
            sprintf (name_temp, "LIST%0d_", i);
            strcpy (name, name_temp);
            strcat (name, "H");
            store_int (fout, name, list_size_h[i]);
            strcpy (name, name_temp);
            strcat (name, "W");
            store_int (fout, name, list_size_w[i]);
        }

        store_int (fout, "TRACK_ME", track_me);
        store_int (fout, "MAP_CHOOSER_EXPAND_DIRS", map_chooser_expand_dirs);
        store_int (fout, "ST_DIRECT_TIMEOUT", st_direct_timeout);
        store_int (fout, "DEAD_RECKONING_TIMEOUT", dead_reckoning_timeout);
        store_int (fout, "SERIAL_CHAR_PACING", serial_char_pacing);
        store_int (fout, "TRAIL_SEGMENT_TIME", trail_segment_time);
        store_int (fout, "TRAIL_SEGMENT_DISTANCE", trail_segment_distance);
        store_int (fout, "RINO_DOWNLOAD_INTERVAL", RINO_download_interval);


        if (debug_level & 1)
            fprintf(stderr,"Save Data Stop\n");

        (void)fclose (fout);
    } else  {
        // Couldn't create new config (out of filespace?).
        fprintf(stderr,"Couldn't open config file for appending: %s\n", config_file);

        // Back out the changes done to the config and backup files.
        // 
        // Continue using original config file.
        if ( rename (config_file_bak1, config_file) ) {
            // Problem here, couldn't rename bak1 file to xastir.cnf
            fprintf(stderr,
                "Couldn't recover %s from %s file\n",
                config_file,
                config_file_bak1);
            return;
        }
        rename (config_file_bak2, config_file_bak1);
        rename (config_file_bak3, config_file_bak2);

        // We're short one file now (config_file_bak3);
    }
}





void load_data_or_default(void) {
    int i;
    char name_temp[20];
    char name[50];
    long temp;

    /* language */
    if (!get_string ("LANGUAGE", lang_to_use))
        strcpy (lang_to_use, "English");


    /* my data */
    if (!get_string ("STATION_CALLSIGN", my_callsign))
        strcpy (my_callsign, "NOCALL");

    if (!get_string ("STATION_LAT", my_lat))
        strcpy (my_lat, "0000.000N");
    if ( (my_lat[4] != '.')
            || (my_lat[8] != 'N' && my_lat[8] != 'S') ) {
        strcpy (my_lat, "0000.000N");
        fprintf(stderr,"Invalid Latitude, changing it to 0000.000N\n");
    }
    // convert old data to high prec
    temp = convert_lat_s2l (my_lat);
    convert_lat_l2s (temp, my_lat, sizeof(my_lat), CONVERT_HP_NOSP);


    if (!get_string ("STATION_LONG", my_long))
        strcpy (my_long, "00000.000W");
    if ( (my_long[5] != '.')
            || (my_long[9] != 'W' && my_long[9] != 'E') ) {
        strcpy (my_long, "00000.000W");
        fprintf(stderr,"Invalid Longitude, changing it to 00000.000W\n");
    }
    // convert old data to high prec
    temp = convert_lon_s2l (my_long);
    convert_lon_l2s (temp, my_long, sizeof(my_long), CONVERT_HP_NOSP);


    if (!get_int ("STATION_TRANSMIT_AMB", &position_amb_chars, 0, 4, 0))
        position_amb_chars = 0;

    if (!get_char ("STATION_GROUP", &my_group))
        my_group = '/';

    if (!get_char ("STATION_SYMBOL", &my_symbol))
        my_symbol = 'x';

    if (!get_char ("STATION_MESSAGE_TYPE", &aprs_station_message_type))
        aprs_station_message_type = '=';

    if (!get_string ("STATION_POWER", my_phg))
        strcpy (my_phg, "");

    if (!get_string ("STATION_COMMENTS", my_comment))
        sprintf (my_comment, "XASTIR-%s", XASTIR_SYSTEM);

    /* default values */
    if (!get_long ("SCREEN_WIDTH", &screen_width, 100l, 10000l, 640l))
        screen_width = 640;

    if (screen_width < 100)
        screen_width = 100;

    if (!get_long ("SCREEN_HEIGHT", &screen_height, 40l, 10000l, 480l))
        screen_height = 480;

    if (screen_height < 40)
        screen_height = 40;

    if (!get_long ("SCREEN_LAT", &mid_y_lat_offset, 0l, 64800000l, 32400000l))
        mid_y_lat_offset = 32400000l;

    if (!get_long ("SCREEN_LONG", &mid_x_long_offset, 0l, 129600000l, 64800000l))
        mid_x_long_offset = 64800000l;

    if (!get_int ("COORDINATE_SYSTEM", &coordinate_system, 0, 5, USE_DDMMMM))
        coordinate_system = USE_DDMMMM;

    if (!get_long ("SCREEN_ZOOM", &scale_y, 1l, 327680l, 327680l))
        scale_y = 327680;
    scale_x = get_x_scale(mid_x_long_offset,mid_y_lat_offset,scale_y);

    if (!get_int ("MAP_BGCOLOR", &map_background_color, 0, 11, 0))
        map_background_color = 0;

    if (!get_int ( "MAP_DRAW_FILLED_COLORS", &map_color_fill, 0, 1, 1) )
        map_color_fill = 1;

#if !defined(NO_GRAPHICS)
#if defined(HAVE_IMAGEMAGICK)
    if (!get_string("IMAGEMAGICK_GAMMA_ADJUST", name))
        imagemagick_gamma_adjust = 0.0;
    else
        sscanf(name, "%f", &imagemagick_gamma_adjust);
#endif  // HAVE_IMAGEMAGICK
    if (!get_string("RASTER_MAP_INTENSITY", name))
        raster_map_intensity = 1.0;
    else
        sscanf(name, "%f", &raster_map_intensity);
#endif  // NO_GRAPHICS

    if (!get_int ("MAP_LETTERSTYLE", &letter_style, 0, 2, 1))
        letter_style = 1;

    if (!get_int ("MAP_ICONOUTLINESTYLE", &icon_outline_style, 0, 3, 0))
        icon_outline_style = 0;

    if (!get_int ("MAP_WX_ALERT_STYLE", &wx_alert_style, 0, 1, 1))
        wx_alert_style = 1;

    if (!get_string("ALTNET_CALL", altnet_call))
        strcpy(altnet_call, "XASTIR");

    if (!get_int("ALTNET", &altnet, 0, 1, 0))
        altnet=0;

    if (!get_int("SKIP_DUPE_CHECK", &skip_dupe_checking, 0, 1, 0))
        skip_dupe_checking=0;

    if (!get_string ("AUTO_MAP_DIR", AUTO_MAP_DIR))
        strcpy (AUTO_MAP_DIR, get_data_base_dir ("maps"));

    if (!get_string ("ALERT_MAP_DIR", ALERT_MAP_DIR))
        strcpy (ALERT_MAP_DIR, get_data_base_dir ("Counties"));

    if (!get_string ("SELECTED_MAP_DIR", SELECTED_MAP_DIR))
        strcpy (SELECTED_MAP_DIR, get_data_base_dir ("maps"));

    if (!get_string ("SELECTED_MAP_DATA", SELECTED_MAP_DATA))
        strcpy (SELECTED_MAP_DATA, get_user_base_dir ("config/selected_maps.sys"));

    if (!get_string ("MAP_INDEX_DATA", MAP_INDEX_DATA))
        strcpy (MAP_INDEX_DATA, get_user_base_dir ("config/map_index.sys"));

    if (!get_string ("SYMBOLS_DIR", SYMBOLS_DIR))
        strcpy (SYMBOLS_DIR, get_data_base_dir ("symbols"));

    if (!get_string ("SOUND_DIR", SOUND_DIR))
        strcpy (SOUND_DIR, get_data_base_dir ("sounds"));

    if (!get_string ("GROUP_DATA_FILE", group_data_file))
        strcpy (group_data_file, get_user_base_dir ("config/groups"));

    if (!get_string ("GNIS_FILE", locate_gnis_filename))
        strcpy (locate_gnis_filename, get_data_base_dir ("GNIS/WA.gnis"));

    if (!get_string ("GEOCODE_FILE", geocoder_map_filename))
        strcpy (geocoder_map_filename, get_data_base_dir ("GNIS/geocode"));

    if (!get_int ("SHOW_FIND_TARGET", &show_destination_mark, 0, 1, 1))
        show_destination_mark = 1;

    /* maps */
    if (!get_int ("MAPS_LONG_LAT_GRID", &long_lat_grid, 0, 1, 1))
        long_lat_grid = 1;

    if (!get_int ("MAPS_LEVELS", &map_color_levels, 0, 1, 0))
        map_color_levels = 0;

    if (!get_int ("MAPS_LABELS", &map_labels, 0, 1, 1))
        map_labels = 1;

    if (!get_int ("MAPS_AUTO_MAPS", &map_auto_maps, 0, 1, 0))
        map_auto_maps = 0;

    if (!get_int ("MAPS_AUTO_MAPS_SKIP_RASTER", &auto_maps_skip_raster, 0, 1, 0))
        auto_maps_skip_raster = 0;

    if (!get_int ("MAPS_INDEX_ON_STARTUP", &index_maps_on_startup, 0, 1, 1))
      index_maps_on_startup = 1;

    if (!get_string ("MAPS_LABEL_FONT_TINY", rotated_label_fontname[FONT_TINY]))
        strcpy(rotated_label_fontname[FONT_TINY],"-adobe-helvetica-medium-o-normal--8-*-*-*-*-*-iso8859-1");
    if (!get_string ("MAPS_LABEL_FONT_SMALL", rotated_label_fontname[FONT_SMALL]))
        strcpy(rotated_label_fontname[FONT_SMALL],"-adobe-helvetica-medium-o-normal--10-*-*-*-*-*-iso8859-1");
    if (!get_string ("MAPS_LABEL_FONT_MEDIUM", rotated_label_fontname[FONT_MEDIUM]))
        strcpy(rotated_label_fontname[FONT_MEDIUM],"-adobe-helvetica-medium-o-normal--12-*-*-*-*-*-iso8859-1");
    if (!get_string ("MAPS_LABEL_FONT_LARGE", rotated_label_fontname[FONT_LARGE]))
        strcpy(rotated_label_fontname[FONT_LARGE],"-adobe-helvetica-medium-o-normal--14-*-*-*-*-*-iso8859-1");
    if (!get_string ("MAPS_LABEL_FONT_HUGE", rotated_label_fontname[FONT_HUGE]))
        strcpy(rotated_label_fontname[FONT_HUGE],"-adobe-helvetica-medium-o-normal--24-*-*-*-*-*-iso8859-1");
    if (!get_string ("MAPS_LABEL_FONT", rotated_label_fontname[FONT_DEFAULT]))
        strcpy(rotated_label_fontname[FONT_DEFAULT],"-adobe-helvetica-medium-o-normal--12-*-*-*-*-*-iso8859-1");
//N0VH
#if defined(HAVE_IMAGEMAGICK)
    if (!get_int ("TIGERMAP_TIMEOUT", &tigermap_timeout, 10, 120, 30))
        tigermap_timeout = 30;

    if (!get_int ("TIGERMAP_SHOW_GRID", &tiger_show_grid, 0, 1, 0))
        tiger_show_grid = 0;

    if (!get_int ("TIGERMAP_SHOW_COUNTIES", &tiger_show_counties, 0, 1, 1))
        tiger_show_counties = 1;

    if (!get_int ("TIGERMAP_SHOW_CITIES", &tiger_show_cities, 0, 1, 1))
        tiger_show_cities = 1;

    if (!get_int ("TIGERMAP_SHOW_PLACES", &tiger_show_places, 0, 1, 1))
        tiger_show_places = 1;

    if (!get_int ("TIGERMAP_SHOW_MAJROADS", &tiger_show_majroads, 0, 1, 1))
        tiger_show_majroads = 1;

    if (!get_int ("TIGERMAP_SHOW_STREETS", &tiger_show_streets, 0, 1, 0))
        tiger_show_streets = 0;

    if (!get_int ("TIGERMAP_SHOW_RAILROAD", &tiger_show_railroad, 0, 1, 1))
        tiger_show_railroad = 1;

    if (!get_int ("TIGERMAP_SHOW_STATES", &tiger_show_states, 0, 1, 0))
        tiger_show_states = 0;

    if (!get_int ("TIGERMAP_SHOW_INTERSTATE", &tiger_show_interstate, 0, 1, 1))
        tiger_show_interstate = 1;

    if (!get_int ("TIGERMAP_SHOW_USHWY", &tiger_show_ushwy, 0, 1, 1))
        tiger_show_ushwy = 1;

    if (!get_int ("TIGERMAP_SHOW_STATEHWY", &tiger_show_statehwy, 0, 1, 1))
        tiger_show_statehwy = 1;

    if (!get_int ("TIGERMAP_SHOW_WATER", &tiger_show_water, 0, 1, 1))
        tiger_show_water = 1;

    if (!get_int ("TIGERMAP_SHOW_LAKES", &tiger_show_lakes, 0, 1, 1))
        tiger_show_lakes = 1;

    if (!get_int ("TIGERMAP_SHOW_MISC", &tiger_show_misc, 0, 1, 1))
        tiger_show_misc = 1;

#endif //HAVE_IMAGEMAGICK

    // filter values
    // NOT SAVED: Select_.none
    if (!get_int ("DISPLAY_MY_STATION", &Select_.mine, 0, 1, 1))
        Select_.mine = 1;
    if (!get_int ("DISPLAY_TNC_STATIONS", &Select_.tnc, 0, 1, 1))
        Select_.tnc = 1;
    if (!get_int ("DISPLAY_TNC_DIRECT_STATIONS", &Select_.direct, 0, 1, 1))
        Select_.direct = 1;
    if (!get_int ("DISPLAY_TNC_VIADIGI_STATIONS", &Select_.via_digi, 0, 1, 1))
        Select_.via_digi = 1;
    if (!get_int ("DISPLAY_NET_STATIONS", &Select_.net, 0, 1, 1))
        Select_.net = 1;
    if (!get_int ("DISPLAY_TACTICAL_STATIONS", &Select_.tactical, 0, 1, 0))
        Select_.tactical = 0;
    if (!get_int ("DISPLAY_OLD_STATION_DATA", &Select_.old_data, 0, 1, 0))
        Select_.old_data = 0;
    if (!get_int ("DISPLAY_STATIONS", &Select_.stations, 0, 1, 1))
        Select_.stations = 1;
    if (!get_int ("DISPLAY_FIXED_STATIONS", &Select_.fixed_stations, 0, 1, 1))
        Select_.fixed_stations = 1;
    if (!get_int ("DISPLAY_MOVING_STATIONS", &Select_.moving_stations, 0, 1, 1))
        Select_.moving_stations = 1;
    if (!get_int ("DISPLAY_WEATHER_STATIONS", &Select_.weather_stations, 0, 1, 1))
        Select_.weather_stations = 1;
    if (!get_int ("DISPLAY_OBJECTS", &Select_.objects, 0, 1, 1))
        Select_.objects = 1;
    if (!get_int ("DISPLAY_STATION_WX_OBJ", &Select_.weather_objects, 0, 1, 1))
        Select_.weather_objects = 1;
    if (!get_int ("DISPLAY_WATER_GAGE_OBJ", &Select_.gauge_objects, 0, 1, 1))
        Select_.gauge_objects = 1;
    if (!get_int ("DISPLAY_OTHER_OBJECTS", &Select_.other_objects, 0, 1, 1))
        Select_.other_objects = 1;

    // display values
    if (!get_int ("DISPLAY_CALLSIGN", &Display_.callsign, 0, 1, 1))
        Display_.callsign = 1;

    if (!get_int ("DISPLAY_SYMBOL",        &Display_.symbol, 0, 1, 1))
        Display_.symbol = 1;
    if (!get_int ("DISPLAY_SYMBOL_ROTATE", &Display_.symbol_rotate, 0, 1, 1))
        Display_.symbol_rotate = 1;
    if (!get_int ("DISPLAY_STATION_PHG", &Display_.phg, 0, 1, 0))
        Display_.phg = 0;
    if (!get_int ("DISPLAY_DEFAULT_PHG", &Display_.default_phg, 0, 1, 0))
        Display_.default_phg = 0;
    if (!get_int ("DISPLAY_MOBILES_PHG", &Display_.phg_of_moving, 0, 1, 0))
        Display_.phg_of_moving = 0;
    if (!get_int ("DISPLAY_ALTITUDE", &Display_.altitude, 0, 1, 0))
        Display_.altitude = 0;
    if (!get_int ("DISPLAY_COURSE",      &Display_.course, 0, 1, 0))
        Display_.course = 0;
    if (!get_int ("DISPLAY_SPEED",       &Display_.speed, 0, 1, 0))
        Display_.speed       = 0;
    if (!get_int ("DISPLAY_SPEED_SHORT", &Display_.speed_short, 0, 1, 0))
        Display_.speed_short = 0;
    if (!get_int ("DISPLAY_DIST_COURSE", &Display_.dist_bearing, 0, 1, 0))
        Display_.dist_bearing = 0;
    if (!get_int ("DISPLAY_WEATHER",    &Display_.weather, 0, 1, 0))
        Display_.weather = 0;
    if (!get_int ("DISPLAY_STATION_WX", &Display_.weather_text, 0, 1, 0))
        Display_.weather_text     = 0;
    if (!get_int ("DISPLAY_TEMP_ONLY",  &Display_.temperature_only, 0, 1, 0))
        Display_.temperature_only = 0;
    if (!get_int ("DISPLAY_WIND_BARB",  &Display_.wind_barb, 0, 1, 0))
        Display_.wind_barb = 0;
    if (!get_int ("DISPLAY_STATION_TRAILS", &Display_.trail, 0, 1, 1))
        Display_.trail = 1;
    if (!get_int ("DISPLAY_LAST_HEARD", &Display_.last_heard, 0, 1, 0))
        Display_.last_heard = 0;
    if (!get_int ("DISPLAY_POSITION_AMB", &Display_.ambiguity, 0, 1, 0))
        Display_.ambiguity = 0;
    if (!get_int ("DISPLAY_DF_INFO", &Display_.df_data, 0, 1, 0))
        Display_.df_data = 0;
    if (!get_int ("DISPLAY_DEAD_RECKONING_INFO", &Display_.dr_data, 0, 1, 1))
        Display_.dr_data = 1;
    if (!get_int ("DISPLAY_DEAD_RECKONING_ARC", &Display_.dr_arc, 0, 1, 1))
        Display_.dr_arc = 1;
    if (!get_int ("DISPLAY_DEAD_RECKONING_COURSE", &Display_.dr_course, 0, 1, 1))
        Display_.dr_course = 1;
    if (!get_int ("DISPLAY_DEAD_RECKONING_SYMBOL", &Display_.dr_symbol, 0, 1, 1))
        Display_.dr_symbol = 1;



    if (!get_int ("DISPLAY_UNITS_ENGLISH", &units_english_metric, 0, 1, 0))
        units_english_metric = 0;

    if (!get_int ("DISPLAY_DIST_BEAR_STATUS", &do_dbstatus, 0, 1, 0))
        do_dbstatus = 0;

    if (!get_int ("DISABLE_TRANSMIT", &transmit_disable, 0, 1, 0))
        transmit_disable = 0;

    if (!get_int ("DISABLE_POSIT_TX", &posit_tx_disable, 0, 1, 0))
        posit_tx_disable = 0;

    if (!get_int ("DISABLE_OBJECT_TX", &object_tx_disable, 0, 1, 0))
        object_tx_disable = 0;


    for (i = 0; i < MAX_IFACE_DEVICES; i++) {
        sprintf (name_temp, "DEVICE%0d_", i);
        strcpy (name, name_temp);
        strcat (name, "TYPE");
        if (!get_int (name, &devices[i].device_type,0,MAX_IFACE_DEVICE_TYPES,DEVICE_NONE)) {
            devices[i].device_type = DEVICE_NONE;
        }
        strcpy (name, name_temp);
        strcat (name, "NAME");
        if (!get_string (name, devices[i].device_name))
            strcpy (devices[i].device_name, "");

        strcpy (name, name_temp);
        strcat (name, "RADIO_PORT");
        if (!get_string (name, devices[i].radio_port))
            strcpy (devices[i].radio_port, "0");

        strcpy (name, name_temp);
        strcat (name, "INTERFACE_COMMENT");
        if (!get_string (name, devices[i].comment))
            strcpy (devices[i].comment, "");

        strcpy (name, name_temp);
        strcat (name, "HOST");
        if (!get_string (name, devices[i].device_host_name))
            strcpy (devices[i].device_host_name, "");

        strcpy (name, name_temp);
        strcat (name, "PASSWD");
        if (!get_string (name, devices[i].device_host_pswd))
            strcpy (devices[i].device_host_pswd, "");

        strcpy (name, name_temp);
        strcat (name, "FILTER_PARAMS");
        if (!get_string (name, devices[i].device_host_filter_string))
            strcpy (devices[i].device_host_filter_string, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO1");
        if (!get_string (name, devices[i].unproto1))
            strcpy (devices[i].unproto1, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO2");
        if (!get_string (name, devices[i].unproto2))
            strcpy (devices[i].unproto2, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO3");
        if (!get_string (name, devices[i].unproto3))
            strcpy (devices[i].unproto3, "");

        strcpy (name, name_temp);
        strcat (name, "UNPROTO_IGATE");
        if (!get_string (name, devices[i].unproto_igate))
            strcpy (devices[i].unproto_igate, "");

        strcpy (name, name_temp);
        strcat (name, "TNC_UP_FILE");
        if (!get_string (name, devices[i].tnc_up_file))
            strcpy (devices[i].tnc_up_file, "");

        strcpy (name, name_temp);
        strcat (name, "TNC_DOWN_FILE");
        if (!get_string (name, devices[i].tnc_down_file))
            strcpy (devices[i].tnc_down_file, "");

        strcpy (name, name_temp);
        strcat (name, "TNC_TXDELAY");
        if (!get_string (name, devices[i].txdelay))
            strcpy (devices[i].txdelay, "40");

        strcpy (name, name_temp);
        strcat (name, "TNC_PERSISTENCE");
        if (!get_string (name, devices[i].persistence))
            strcpy (devices[i].persistence, "63");

        strcpy (name, name_temp);
        strcat (name, "TNC_SLOTTIME");
        if (!get_string (name, devices[i].slottime))
            strcpy (devices[i].slottime, "10");

        strcpy (name, name_temp);
        strcat (name, "TNC_TXTAIL");
        if (!get_string (name, devices[i].txtail))
            strcpy (devices[i].txtail, "30");

        strcpy (name, name_temp);
        strcat (name, "TNC_FULLDUPLEX");
        if (!get_int (name, &devices[i].fullduplex, 0, 1, 0))
            devices[i].fullduplex = 0;

        strcpy (name, name_temp);
        strcat (name, "SPEED");
        if (!get_int (name, &devices[i].sp,0,230400,0))
            devices[i].sp = 0;

        strcpy (name, name_temp);
        strcat (name, "STYLE");
        if (!get_int (name, &devices[i].style,0,2,0))
            devices[i].style = 0;

        strcpy (name, name_temp);
        strcat (name, "IGATE_OPTION");
        if (!get_int (name, &devices[i].igate_options,0,2,0))
            devices[i].igate_options = 0;

        strcpy (name, name_temp);
        strcat (name, "TXMT");
        if (!get_int (name, &devices[i].transmit_data,0,1,0))
            devices[i].transmit_data = 0;

        strcpy (name, name_temp);
        strcat (name, "RELAY_DIGIPEAT");
        if (!get_int (name, &devices[i].relay_digipeat,0,1,1))
            devices[i].relay_digipeat = 1;

        strcpy (name, name_temp);
        strcat (name, "RECONN");
        if (!get_int (name, &devices[i].reconnect,0,1,0))
            devices[i].reconnect = 0;

        strcpy (name, name_temp);
        strcat (name, "ONSTARTUP");
        if (!get_int (name, &devices[i].connect_on_startup,0,1,0))
            devices[i].connect_on_startup = 0;

                strcpy (name, name_temp);
                strcat (name, "GPSRETR");
                if (!get_int (name, &devices[i].gps_retrieve,0,255,DEFAULT_GPS_RETR))
                        devices[i].gps_retrieve = DEFAULT_GPS_RETR;

                strcpy (name, name_temp);
                strcat (name, "SETTIME");
                if (!get_int (name, &devices[i].set_time,0,1,0))
                        devices[i].set_time = 0;
    }

    /* TNC */
    if (!get_int ("TNC_LOG_DATA", &log_tnc_data,0,1,0))
        log_tnc_data = 0;

    if (!get_string ("LOGFILE_TNC", LOGFILE_TNC))
        strcpy (LOGFILE_TNC, get_user_base_dir ("logs/tnc.log"));

    /* NET */
    if (!get_int ("NET_LOG_DATA", &log_net_data,0,1,0))
        log_net_data = 0;

    if (!get_int ("NET_RUN_AS_IGATE", &operate_as_an_igate,0,2,0))
        operate_as_an_igate = 0;

    if (!get_int ("LOG_IGATE", &log_igate,0,1,0))
        log_igate = 0;

    if (!get_int ("NETWORK_WAITTIME", &NETWORK_WAITTIME,10,120,10))
        NETWORK_WAITTIME = 10;

    // LOGGING
    if (!get_int ("LOG_WX", &log_wx,0,1,0))
        log_wx = 0;

    if (!get_string ("LOGFILE_IGATE", LOGFILE_IGATE))
        strcpy (LOGFILE_IGATE, get_user_base_dir ("logs/igate.log"));

    if (!get_string ("LOGFILE_NET", LOGFILE_NET))
        strcpy (LOGFILE_NET, get_user_base_dir ("logs/net.log"));

    if (!get_string ("LOGFILE_WX", LOGFILE_WX))
        strcpy (LOGFILE_WX, get_user_base_dir ("logs/wx.log"));

    // SNAPSHOTS
    if (!get_int ("SNAPSHOTS_ENABLED", &snapshots_enabled,0,1,0))
        snapshots_enabled = 0;

    /* WX ALERTS */
    if (!get_long ("WX_ALERTS_REFRESH_TIME", (long *)&WX_ALERTS_REFRESH_TIME, 10l, 86400l, 60l))
        WX_ALERTS_REFRESH_TIME = (time_t)60l;

    /* gps */
    if (!get_long ("GPS_TIME", (long *)&gps_time, 1l, 86400l, 60l))
        gps_time = (time_t)60l;

    /* POSIT RATE */
    if (!get_long ("POSIT_RATE", (long *)&POSIT_rate, 1l, 86400l, 1800l))
        POSIT_rate = (time_t)30*60l;

    /* OBJECT RATE */
    if (!get_long ("OBJECT_RATE", (long *)&OBJECT_rate, 1l, 86400l, 1800l))
        OBJECT_rate = (time_t)30*60l;

    /* UPDATE DR RATE */
    if (!get_long ("UPDATE_DR_RATE", (long *)&update_DR_rate, 1l, 86400l, 30l))
        update_DR_rate = (time_t)30l;

    /* station broadcast type */
    if (!get_int ("BST_TYPE", &output_station_type,0,5,0))
        output_station_type = 0;

#ifdef TRANSMIT_RAW_WX
    /* raw wx transmit */
    if (!get_int ("BST_WX_RAW", &transmit_raw_wx,0,1,0))
        transmit_raw_wx = 0;
#endif  // TRANSMIT_RAW_WX

    /* compressed posit transmit */
    if (!get_int ("BST_COMPRESSED_POSIT", &transmit_compressed_posit,0,1,0))
        transmit_compressed_posit = 0;

    /* compressed objects/items transmit */
    if (!get_int ("COMPRESSED_OBJECTS_ITEMS", &transmit_compressed_objects_items,0,1,0))
        transmit_compressed_objects_items = 0;

    if (!get_int ("SMART_BEACONING", &smart_beaconing,0,1,1))
        smart_beaconing = 0;

    if (!get_int ("SB_TURN_MIN", &sb_turn_min,1,360,20))
        sb_turn_min = 20;

    if (!get_int ("SB_TURN_SLOPE", &sb_turn_slope,0,360,25))
        sb_turn_slope = 25;

    if (!get_int ("SB_TURN_TIME", &sb_turn_time,0,3600,5))
        sb_turn_time = 5;

    if (!get_int ("SB_POSIT_FAST", &sb_posit_fast,1,1440,60))
        sb_posit_fast = 60;

    if (!get_int ("SB_POSIT_SLOW", &sb_posit_slow,1,1440,30))
        sb_posit_slow = 30;

    if (!get_int ("SB_LOW_SPEED_LIMIT", &sb_low_speed_limit,0,999,2))
        sb_low_speed_limit = 2;

    if (!get_int ("SB_HIGH_SPEED_LIMIT", &sb_high_speed_limit,0,999,60))
        sb_high_speed_limit = 60;

    if (!get_int ("POP_UP_NEW_BULLETINS", &pop_up_new_bulletins,0,1,1))
        pop_up_new_bulletins = 1;

    if (!get_int ("VIEW_ZERO_DISTANCE_BULLETINS", &view_zero_distance_bulletins,0,1,1))
        view_zero_distance_bulletins = 1;

    if (!get_int ("WARN_ABOUT_MOUSE_MODIFIERS", &warn_about_mouse_modifiers,0,1,1))
        warn_about_mouse_modifiers = 1;


    /* Audio Alarms*/
    if (!get_string ("SOUND_COMMAND", sound_command))
        strcpy (sound_command, "vplay -q");

    if (!get_int ("SOUND_PLAY_ONS", &sound_play_new_station,0,1,0))
        sound_play_new_station = 0;

    if (!get_string ("SOUND_ONS_FILE", sound_new_station))
        strcpy (sound_new_station, "newstation.wav");

    if (!get_int ("SOUND_PLAY_ONM", &sound_play_new_message,0,1,0))
        sound_play_new_message = 0;

    if (!get_string ("SOUND_ONM_FILE", sound_new_message))
        strcpy (sound_new_message, "newmessage.wav");

    if (!get_int ("SOUND_PLAY_PROX", &sound_play_prox_message,0,1,0))
        sound_play_prox_message = 0;

    if (!get_string ("SOUND_PROX_FILE", sound_prox_message))
        strcpy (sound_prox_message, "proxwarn.wav");

    if (!get_string ("PROX_MIN", prox_min))
        strcpy (prox_min, "0.01");

    if (!get_string ("PROX_MAX", prox_max))
        strcpy (prox_max, "10");

    if (!get_int ("SOUND_PLAY_BAND", &sound_play_band_open_message,0,1,0))
        sound_play_band_open_message = 0;

    if (!get_string ("SOUND_BAND_FILE", sound_band_open_message))
        strcpy (sound_band_open_message, "bandopen.wav");

    if (!get_string ("BANDO_MIN", bando_min))
        strcpy (bando_min, "200");

    if (!get_string ("BANDO_MAX", bando_max))
        strcpy (bando_max, "2000");

    if (!get_int ("SOUND_PLAY_WX_ALERT", &sound_play_wx_alert_message,0,1,0))
        sound_play_wx_alert_message = 0;

    if (!get_string ("SOUND_WX_ALERT_FILE", sound_wx_alert_message))
        strcpy (sound_wx_alert_message, "thunder.wav");

#ifdef HAVE_FESTIVAL
    /* Festival Speech defaults */

    if (!get_int ("SPEAK_NEW_STATION",&festival_speak_new_station,0,1,0))
        festival_speak_new_station = 0;

    if (!get_int ("SPEAK_PROXIMITY_ALERT",&festival_speak_proximity_alert,0,1,0))
        festival_speak_proximity_alert = 0;

    if (!get_int ("SPEAK_TRACKED_ALERT",&festival_speak_tracked_proximity_alert,0,1,0))
        festival_speak_tracked_proximity_alert = 0;

    if (!get_int ("SPEAK_BAND_OPENING",&festival_speak_band_opening,0,1,0))
        festival_speak_band_opening = 0;
                                  
    if (!get_int ("SPEAK_MESSAGE_ALERT",&festival_speak_new_message_alert,0,1,0))
        festival_speak_new_message_alert = 0; 

    if (!get_int ("SPEAK_MESSAGE_BODY",&festival_speak_new_message_body,0,1,0))
        festival_speak_new_message_body = 0;

    if (!get_int ("SPEAK_WEATHER_ALERT",&festival_speak_new_weather_alert,0,1,0))
        festival_speak_new_weather_alert = 0; 

    if (!get_int ("SPEAK_ID",&festival_speak_ID,0,1,0))
        festival_speak_new_station = 0;
#endif  // HAVE_FESTIVAL
    if (!get_int ("ATV_SCREEN_ID",&ATV_screen_ID,0,1,0))
        ATV_screen_ID = 0; 
 
    /* defaults */
    if (!get_long ("DEFAULT_STATION_OLD", (long *)&sec_old, 1l, 604800l, 4800l))
        sec_old = (time_t)4800l;

    if (!get_long ("DEFAULT_STATION_CLEAR", (long *)&sec_clear, 1l, 604800l, 43200l))
        sec_clear = (time_t)43200l;

    if (!get_long("DEFAULT_STATION_REMOVE", (long *)&sec_remove, 1l, 604800*2, sec_clear*2)) {
        sec_remove = sec_clear*2;
    // In the interests of keeping the memory used by Xastir down, I'm
    // commenting out the below lines.  When hooked to one or more internet
    // links it's nice to expire stations from the database more quickly.
//        if (sec_remove < (time_t)(24*3600)) // If it's less than one day,
//            sec_remove = (time_t)(24*3600); // change to a one-day expire
    }

    if (!get_string ("MESSAGE_COUNTER", message_counter))
        strcpy (message_counter, "00");

    message_counter[2] = '\0';  // Terminate at 2 chars
    // Check that chars are within the correct ranges
    if (         (message_counter[0] < '0')
            ||   (message_counter[1] < '0')
            || ( (message_counter[0] > '9') && (message_counter[0] < 'A') )
            || ( (message_counter[1] > '9') && (message_counter[1] < 'A') )
            || ( (message_counter[0] > 'Z') && (message_counter[0] < 'a') )
            || ( (message_counter[1] > 'Z') && (message_counter[1] < 'a') )
            ||   (message_counter[0] > 'z')
            ||   (message_counter[1] > 'z') ) {
        message_counter[0] = '0';
        message_counter[1] = '0'; 
    }

    if (!get_string ("AUTO_MSG_REPLY", auto_reply_message))
        strcpy (auto_reply_message, "Autoreply- No one is at the keyboard");

    if (!get_int ("DISPLAY_PACKET_TYPE", &Display_packet_data_type,0,2,0))
        Display_packet_data_type = 0;

    if (!get_int ("BULLETIN_RANGE", &bulletin_range,0,99999,0))
        bulletin_range = 0;

    if(!get_int("VIEW_MESSAGE_RANGE", &vm_range,0,99999,0))
        vm_range=0;

    if(!get_int("VIEW_MESSAGE_LIMIT", &view_message_limit,0,99999,0))
        view_message_limit = 10000;


    /* printer variables */
    if (!get_int ("PRINT_ROTATED", &print_rotated,0,1,0))
        print_rotated = 0;

    if (!get_int ("PRINT_AUTO_ROTATION", &print_auto_rotation,0,1,1))
        print_auto_rotation = 1;

    if (!get_int ("PRINT_AUTO_SCALE", &print_auto_scale,0,1,1))
        print_auto_scale = 1;

    if (!get_int ("PRINT_IN_MONOCHROME", &print_in_monochrome,0,1,0))
        print_in_monochrome = 0;

    if (!get_int ("PRINT_INVERT_COLORS", &print_invert,0,1,0))
        print_invert = 0;

    if (!get_int ("RAIN_GAUGE_TYPE", &WX_rain_gauge_type,0,3,0))
        WX_rain_gauge_type = 0;     // No Correction


    /* list attributes */
    for (i = 0; i < LST_NUM; i++) {
        sprintf (name_temp, "LIST%0d_", i);
        strcpy (name, name_temp);
        strcat (name, "H");
        if (!get_int (name, &list_size_h[i],-1,8192,-1))
            list_size_h[i] = -1;

        strcpy (name, name_temp);
        strcat (name, "W");
        if (!get_int (name, &list_size_w[i],-1,8192,-1))
            list_size_w[i] = -1;

    }

    if (!get_int ("TRACK_ME", &track_me,0,1,0))
        track_me = 0;    // No tracking

    if (!get_int ("MAP_CHOOSER_EXPAND_DIRS", &map_chooser_expand_dirs,0,1,1))
        map_chooser_expand_dirs = 1;

    if (!get_int ("ST_DIRECT_TIMEOUT", &st_direct_timeout,1,60*60*24*30,60*60))
        st_direct_timeout = 60*60;    // One hour default

    if (!get_int ("DEAD_RECKONING_TIMEOUT", &dead_reckoning_timeout,1,60*60*24*30,60*10))
        dead_reckoning_timeout = 60*10;     // Ten minute default

    if (!get_int ("SERIAL_CHAR_PACING", &serial_char_pacing,0,50,1))
        serial_char_pacing = 1; // 1ms default

    if (!get_int ("TRAIL_SEGMENT_TIME", &trail_segment_time,0,12*60,45))
        trail_segment_time = 45; // 45 minutes default, 12 hours max

    if (!get_int ("TRAIL_SEGMENT_DISTANCE", &trail_segment_distance,0,45,1))
        trail_segment_distance = 1; // 1 degrees default

    if (!get_int ("RINO_DOWNLOAD_INTERVAL", &RINO_download_interval,0,30,0))
        RINO_download_interval = 0; // 0 minutes default (function disabled)

    input_close();
}


