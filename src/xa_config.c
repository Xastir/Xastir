/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2009  The Xastir Group
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
#include "snprintf.h"
#include "objects.h"
#include "db_gis.h"

// Must be last include file
#include "leak_detection.h"



#define CONFIG_FILE      "config/xastir.cnf"
#define CONFIG_FILE_BAK1 "config/xastir.cnf.1"
#define CONFIG_FILE_BAK2 "config/xastir.cnf.2"
#define CONFIG_FILE_BAK3 "config/xastir.cnf.3"
#define CONFIG_FILE_BAK4 "config/xastir.cnf.4"
#define CONFIG_FILE_TMP  "config/xastir.cnf.tmp"

#define MAX_VALUE 300

//extern char xa_config_dir[];



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
    May return empty string in "value".
*/
int get_string(char *option, char *value, int value_size) {
    char config_file[MAX_VALUE];
    char config_line[256];
    char *option_in;
    char *value_in;
    short option_found;

    option_found = 0;

    if (fin == NULL) {

        xastir_snprintf(config_file,
            sizeof(config_file),
            "%s",
            get_user_base_dir(CONFIG_FILE));

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
                    xastir_snprintf(value,
                        value_size,
                        "%s",
                        value_in);
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

    ret = get_string (option, value_o, sizeof(value_o));
    if (ret)
        *value = value_o[0];

    return (ret);
}





// Snags an int and checks whether it is within the correct range.
// If not, it assigns a default value.  Returns the value.
int get_int(char *option, int low, int high, int def) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o, sizeof(value_o));
    if (ret && (atoi(value_o) >= low) && (atoi(value_o) <= high) ) {
        return(atoi (value_o));
    }

    if (!ret) {
//        fprintf(stderr,"xastir.cnf: %s not found, inserting default: %d\n",
//            option,
//            def);
        return(def);
    }

    fprintf(stderr,"xastir.cnf: %s out-of-range: %d, changing to default: %d\n",
        option,
        atoi(value_o),
        def);
    return(def);
}





// Snags a long and checks whether it is within the correct range.
// If not, it assigns a default value.  Returns the value.
long get_long(char *option, long low, long high, long def) {
    char value_o[MAX_VALUE];
    int ret;

    ret = get_string (option, value_o, sizeof(value_o));
    if (ret && (atol(value_o) >= low) && (atol(value_o) <= high) ) {
        return(atol(value_o));
    }

    if (!ret) {
//        fprintf(stderr,"xastir.cnf: %s not found, inserting default: %ld\n",
//            option,
//            def);
        return(def);
    }

    fprintf(stderr,
        "xastir.cnf: %s out-of-range: %ld, changing to default: %ld\n",
        option,
        atol(value_o),
        def);
    return(def);
}





char *get_user_base_dir(char *dir) {
    static char base[MAX_VALUE];
    char *env_ptr;

   // fprintf(stderr,"base: %s \nxa_config_dir: %s\n", base, xa_config_dir);

    switch (xa_config_dir[0]) {
    case '/':
	//have some path
        xastir_snprintf(base, sizeof(base), "%s",xa_config_dir);
        break; 

    case '\0' : 
        // build from scratch
        xastir_snprintf(base,
            sizeof(base),
            "%s",
            ((env_ptr = getenv ("XASTIR_USER_BASE")) != NULL) ? env_ptr : user_dir);

        if (base[strlen (base) - 1] != '/')
            strncat (base, "/", sizeof(base) - 1 - strlen(base));

        strncat (base, ".xastir/", sizeof(base) - 1 - strlen(base));
        break ;

    default: 
        // Unqualified path
        xastir_snprintf(base, sizeof(base), "%s",
            ((env_ptr = getenv ("PWD")) != NULL) ? env_ptr : user_dir);

       	if (base[strlen (base) - 1] != '/')
            strncat (base, "/", sizeof(base) - 1 - strlen(base));

        strncat (base, xa_config_dir, sizeof(base) - 1 - strlen(base));
    }
    
    if (base[strlen (base) - 1] != '/')
        strncat (base, "/", sizeof(base) - 1 - strlen(base));

    // Save base so we monkey around less later. 
    
    xastir_snprintf(xa_config_dir,sizeof(xa_config_dir),"%s", base);

    // Append dir and return 
    
    return strncat(base, dir, sizeof(base) - 1 - strlen(base));

}





char *get_data_base_dir(char *dir) {
    static char base[MAX_VALUE];
    char *env_ptr;

    // Snag this variable from the environment if it exists there,
    // else grab it from the define from the compile command-line
    // that should look like one of these:
    //
    // -DXASTIR_DATA_BASE=\"/opt/Xastir/share/xastir\"
    // -DXASTIR_DATA_BASE=\"/usr/local/share/xastir\"
    //
    xastir_snprintf(base,
        sizeof(base),
        "%s",
        ((env_ptr = getenv ("XASTIR_DATA_BASE")) != NULL) ? env_ptr : XASTIR_DATA_BASE);

    if (base[strlen (base) - 1] != '/')
        strncat(base, "/", sizeof(base) - 1 - strlen(base));

    return strncat(base, dir, sizeof(base) - 1 - strlen(base));
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
    char config_file_tmp[MAX_VALUE];
    char config_file[MAX_VALUE];
    char config_file_bak1[MAX_VALUE];
    char config_file_bak2[MAX_VALUE];
    char config_file_bak3[MAX_VALUE];
    char config_file_bak4[MAX_VALUE];
    struct stat file_status;


//    if (debug_level & 1)
//        fprintf(stderr,"Store String Start\n");

    // The new file we'll create
    xastir_snprintf(config_file_tmp,
        sizeof(config_file_tmp),
        "%s",
        get_user_base_dir(CONFIG_FILE_TMP));

    // Save to the new config file
    fout = fopen (config_file_tmp, "a");
    if (fout != NULL) {
//        Position x_return;
//        Position y_return;

 
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
        store_int (fout, "MY_TRAIL_DIFF_COLOR", my_trail_diff_color);
        if (debug_level & 1)
            fprintf(stderr,"Save Data 1\n");

        /* default values */
        store_long (fout, "SCREEN_WIDTH", screen_width);
        store_long (fout, "SCREEN_HEIGHT", screen_height);


/*
        // Get the X/Y offsets for the main window
        XtVaGetValues(appshell,
            XmNx, &x_return,
            XmNy, &y_return,
            NULL);

fprintf(stderr,"X:%d  y:%d\n", (int)x_return, (int)y_return);
        store_int (fout, "SCREEN_X_OFFSET", (int)x_return);
        store_int (fout, "SCREEN_Y_OFFSET", (int)y_return);
*/


        store_long (fout, "SCREEN_LAT", center_latitude);
        store_long (fout, "SCREEN_LONG", center_longitude);

        store_string(fout, "RELAY_DIGIPEAT_CALLS", relay_digipeater_calls);

        store_int (fout, "COORDINATE_SYSTEM", coordinate_system);

        store_int (fout, "STATION_TRANSMIT_AMB", position_amb_chars);

        if (scale_y > 0)
            store_long (fout, "SCREEN_ZOOM", scale_y);
        else
            store_long (fout, "SCREEN_ZOOM", 1);

        store_int (fout, "MAP_BGCOLOR", map_background_color);

        store_int (fout, "MAP_DRAW_FILLED_COLORS", map_color_fill);

#if !defined(NO_GRAPHICS)
#if defined(HAVE_MAGICK)
        sprintf (name, "%f", imagemagick_gamma_adjust);
        store_string(fout, "IMAGEMAGICK_GAMMA_ADJUST", name);
#endif  // HAVE_MAGICK
        sprintf (name, "%f", raster_map_intensity);
        store_string(fout, "RASTER_MAP_INTENSITY", name);
#endif  // NO_GRAPHICS

        store_string(fout, "PRINT_PROGRAM", printer_program);
        store_string(fout, "PREVIEWER_PROGRAM", previewer_program);

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
        store_int (fout, "MAPS_LABELED_GRID_BORDER", draw_labeled_grid_border);
        store_int (fout, "MAPS_LEVELS", map_color_levels);
        store_int (fout, "MAPS_LABELS", map_labels);
        store_int (fout, "MAPS_AUTO_MAPS", map_auto_maps);
        store_int (fout, "MAPS_AUTO_MAPS_SKIP_RASTER", auto_maps_skip_raster);
        store_int (fout, "MAPS_INDEX_ON_STARTUP", index_maps_on_startup);

        store_string (fout, "MAPS_LABEL_FONT_TINY", rotated_label_fontname[FONT_TINY]);
        store_string (fout, "MAPS_LABEL_FONT_SMALL", rotated_label_fontname[FONT_SMALL]);
        store_string (fout, "MAPS_LABEL_FONT_MEDIUM", rotated_label_fontname[FONT_MEDIUM]);
        // NOTE:  FONT_DEFAULT points to FONT_MEDIUM.
        store_string (fout, "MAPS_LABEL_FONT_LARGE", rotated_label_fontname[FONT_LARGE]);
        store_string (fout, "MAPS_LABEL_FONT_HUGE", rotated_label_fontname[FONT_HUGE]);
        store_string (fout, "MAPS_LABEL_FONT_BORDER", rotated_label_fontname[FONT_BORDER]);
        store_string (fout, "SYSTEM_FIXED_FONT", rotated_label_fontname[FONT_SYSTEM]);
        store_string (fout, "STATION_FONT", rotated_label_fontname[FONT_STATION]);
        store_string (fout, "ATV_ID_FONT", rotated_label_fontname[FONT_ATV_ID]);


//N0VH
#if defined(HAVE_MAGICK)
        store_int (fout, "NET_MAP_TIMEOUT", net_map_timeout);
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
#endif //HAVE_MAGICK

#ifdef HAVE_LIBGEOTIFF
        store_int (fout, "DRG_XOR_COLORS", DRG_XOR_colors);
        store_int (fout, "DRG_SHOW_COLORS_0", DRG_show_colors[0]);
        store_int (fout, "DRG_SHOW_COLORS_1", DRG_show_colors[1]);
        store_int (fout, "DRG_SHOW_COLORS_2", DRG_show_colors[2]);
        store_int (fout, "DRG_SHOW_COLORS_3", DRG_show_colors[3]);
        store_int (fout, "DRG_SHOW_COLORS_4", DRG_show_colors[4]);
        store_int (fout, "DRG_SHOW_COLORS_5", DRG_show_colors[5]);
        store_int (fout, "DRG_SHOW_COLORS_6", DRG_show_colors[6]);
        store_int (fout, "DRG_SHOW_COLORS_7", DRG_show_colors[7]);
        store_int (fout, "DRG_SHOW_COLORS_8", DRG_show_colors[8]);
        store_int (fout, "DRG_SHOW_COLORS_9", DRG_show_colors[9]);
        store_int (fout, "DRG_SHOW_COLORS_10", DRG_show_colors[10]);
        store_int (fout, "DRG_SHOW_COLORS_11", DRG_show_colors[11]);
        store_int (fout, "DRG_SHOW_COLORS_12", DRG_show_colors[12]);
#endif  // HAVE_LIBGEOTIFF

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
        store_int (fout, "DISPLAY_CWOP_WX_STATIONS",      Select_.CWOP_wx_stations);
        store_int (fout, "DISPLAY_OBJECTS",               Select_.objects);
        store_int (fout, "DISPLAY_STATION_WX_OBJ",        Select_.weather_objects);
        store_int (fout, "DISPLAY_WATER_GAGE_OBJ",        Select_.gauge_objects);
        store_int (fout, "DISPLAY_OTHER_OBJECTS",         Select_.other_objects);

        // display values
        store_int (fout, "DISPLAY_CALLSIGN",              Display_.callsign);
        store_int (fout, "DISPLAY_LABEL_ALL_TRACKPOINTS", Display_.label_all_trackpoints);
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


        store_int (fout, "DISPLAY_UNITS_ENGLISH",         english_units);
        store_int (fout, "DISPLAY_DIST_BEAR_STATUS",      do_dbstatus);


        // CAD Objects
        store_int (fout, "DISPLAY_CAD_OBJECT_LABEL",       CAD_show_label);
        store_int (fout, "DISPLAY_CAD_OBJECT_PROBABILITY", CAD_show_raw_probability);
        store_int (fout, "DISPLAY_CAD_OBJECT_COMMENT",     CAD_show_comment);
        store_int (fout, "DISPLAY_CAD_OBJECT_AREA",        CAD_show_area);


        // Interface values
        store_int (fout, "DISABLE_TRANSMIT",   transmit_disable);
        store_int (fout, "DISABLE_POSIT_TX",   posit_tx_disable);
        store_int (fout, "DISABLE_OBJECT_TX",  object_tx_disable);
        store_int (fout, "ENABLE_SERVER_PORT", enable_server_port);


        for (i = 0; i < MAX_IFACE_DEVICES; i++) {
            sprintf (name_temp, "DEVICE%0d_", i);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TYPE", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].device_type);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "NAME", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].device_name);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "RADIO_PORT", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].radio_port);

#ifdef HAVE_DB
            
            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "DATABASE_TYPE", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].database_type);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "DATABASE_SCHEMA_TYPE", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].database_schema_type);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "DATABASE_USERNAME", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].database_username);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "DATABASE_SCHEMA", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].database_schema);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "DATABASE_UNIX_SOCKET", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].database_unix_socket);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "QUERY_ON_STARTUP", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].query_on_startup);

#endif /* HAVE_DB */

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "INTERFACE_COMMENT", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].comment);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "HOST", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].device_host_name);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "PASSWD", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].device_host_pswd);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "FILTER_PARAMS", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].device_host_filter_string);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "UNPROTO1", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].unproto1);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "UNPROTO2", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].unproto2);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "UNPROTO3", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].unproto3);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "UNPROTO_IGATE", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].unproto_igate);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_UP_FILE", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].tnc_up_file);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_DOWN_FILE", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].tnc_down_file);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_TXDELAY", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].txdelay);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_PERSISTENCE", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].persistence);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_SLOTTIME", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].slottime);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_TXTAIL", sizeof(name) - 1 - strlen(name));
            store_string (fout, name, devices[i].txtail);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_FULLDUPLEX", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].fullduplex);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TNC_INIT_KISSMODE", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].init_kiss);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "SPEED", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].sp);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "STYLE", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].style);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "IGATE_OPTION", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].igate_options);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "TXMT", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].transmit_data);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "RELAY_DIGIPEAT", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].relay_digipeat);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "RECONN", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].reconnect);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "ONSTARTUP", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].connect_on_startup);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat(name, "GPSRETR", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, devices[i].gps_retrieve);

            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "SETTIME", sizeof(name) - 1 - strlen(name));
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
        store_int (fout, "LOG_MESSAGE", log_message_data);
        store_int (fout, "LOG_WX_ALERT", log_wx_alert_data);
        store_string (fout, "LOGFILE_IGATE", LOGFILE_IGATE);
        store_string (fout, "LOGFILE_NET", LOGFILE_NET);
        store_string (fout, "LOGFILE_WX", LOGFILE_WX);
        store_string (fout, "LOGFILE_MESSAGE", LOGFILE_MESSAGE);
        store_string (fout, "LOGFILE_WX_ALERT", LOGFILE_WX_ALERT);

        // SNAPSHOTS
        store_int (fout, "SNAPSHOTS_ENABLED", snapshots_enabled);

        // KML SNAPSHOTS
        store_int (fout, "KMLSNAPSHOTS_ENABLED", kmlsnapshots_enabled);

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
        store_int(fout,"PREDEF_MENU_LOAD",predefined_menu_from_file);
        store_string(fout,"PREDEF_MENU_FILE",predefined_object_definition_filename);

        store_int(fout, "READ_MESSAGES_PACKET_DATA_TYPE", Read_messages_packet_data_type);
        store_int(fout, "READ_MESSAGES_MINE_ONLY", Read_messages_mine_only);

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
            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "H", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, list_size_h[i]);
            xastir_snprintf(name,
                sizeof(name),
                "%s",
                name_temp);
            strncat (name, "W", sizeof(name) - 1 - strlen(name));
            store_int (fout, name, list_size_w[i]);
        }

        store_int (fout, "TRACK_ME", track_me);
        store_string (fout, "TRACKING_STATION_CALLSIGN", tracking_station_call);
        store_int (fout, "MAP_CHOOSER_EXPAND_DIRS", map_chooser_expand_dirs);
        store_int (fout, "ST_DIRECT_TIMEOUT", st_direct_timeout);
        store_int (fout, "DEAD_RECKONING_TIMEOUT", dead_reckoning_timeout);
        store_int (fout, "SERIAL_CHAR_PACING", serial_char_pacing);
        store_int (fout, "TRAIL_SEGMENT_TIME", trail_segment_time);
        store_int (fout, "TRAIL_SEGMENT_DISTANCE", trail_segment_distance);
        store_int (fout, "RINO_DOWNLOAD_INTERVAL", RINO_download_interval);
        store_int (fout, "SNAPSHOT_INTERVAL", snapshot_interval);


        if (debug_level & 1)
            fprintf(stderr,"Save Data Stop\n");

        // Close the config_file_tmp file, we're done writing it.
        (void)fclose (fout);


        xastir_snprintf(config_file,
            sizeof(config_file),
            "%s",
            get_user_base_dir(CONFIG_FILE));

        xastir_snprintf(config_file_bak1,
            sizeof(config_file_bak1),
            "%s",
            get_user_base_dir(CONFIG_FILE_BAK1));

        xastir_snprintf(config_file_bak2,
            sizeof(config_file_bak2),
            "%s",
            get_user_base_dir(CONFIG_FILE_BAK2));

        xastir_snprintf(config_file_bak3,
            sizeof(config_file_bak3),
            "%s",
            get_user_base_dir(CONFIG_FILE_BAK3));
 
        xastir_snprintf(config_file_bak4,
            sizeof(config_file_bak4),
            "%s",
            get_user_base_dir(CONFIG_FILE_BAK4));


        //
        // Rename the old config files so that we have a few
        // backups in case of corruption:
        //
        //   xastir.cnf.3   -> xastir.cnf.4
        //   xastir.cnf.2   -> xastir.cnf.3
        //   xastir.cnf.1   -> xastir.cnf.2
        //   xastir.cnf     -> xastir.cnf.1
        //   xastir.cnf.tmp -> xastir.cnf
        //

        // Rename bak3 to bak4
        // NOTE: bak3 won't exist until a few saves have happened.
        //
        // Check whether bak3 exists
        if (stat(config_file_bak3, &file_status) == 0) {
            if (!S_ISREG(file_status.st_mode)) {
                fprintf(stderr,
                    "Couldn't stat %s, cancelling save_data()\n",
                    config_file_bak3);
                return;
            }
            if ( rename (config_file_bak3, config_file_bak4) ) {
                fprintf(stderr,
                    "Couldn't rename %s to %s, cancelling save_data()\n",
                    config_file_bak3,
                    config_file_bak4);

                // Attempt to restore to previous state
                // Nothing to do here!
                return;
            }
        }

        // Rename bak2 to bak3
        // NOTE: bak2 won't exist until a few saves have happened.
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
                rename (config_file_bak4, config_file_bak3);
                // Nothing to do here!
                return;
            }
        }

        // Rename bak1 to bak2
        // NOTE: bak1 won't exist until a couple of saves have happened.
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
                rename (config_file_bak4, config_file_bak3);
                return;
            }
        }

// NOTE:  To minimize the possibility that we'll end up with a
// missing or corrupt config file, we actually should COPY
// config_file to config_file_bak1 here so that there's always a
// config file in place no matter what.  In the next block we can do
// the rename of config_file_tmp to config_file and not miss a beat.
// See "man 2 rename".

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
            //
            // Note "copy_file()" instead of "rename()".  This
            // assures that we always have a config file in place no
            // matter what errors might occur.
            //
            if ( copy_file(config_file, config_file_bak1) ) {
                fprintf(stderr,
                    "Couldn't copy %s to %s, cancelling save_data()\n",
                    config_file,
                    config_file_bak1);

                // Attempt to restore to previous state
                rename (config_file_bak2, config_file_bak1);
                rename (config_file_bak3, config_file_bak2);
                rename (config_file_bak4, config_file_bak3);
                return;
            }
        }

        // Rename config.tmp to xastir.cnf
        // NOTE: config won't exist until the first save happens.
        //
        // Check whether config.tmp exists
        if (stat(config_file_tmp, &file_status) == 0) {
            if (!S_ISREG(file_status.st_mode)) {
                fprintf(stderr,
                    "Couldn't stat %s, cancelling save_data()\n",
                    config_file_tmp);
                return;
            }
            if ( rename (config_file_tmp, config_file) ) {
                fprintf(stderr,
                    "Couldn't rename %s to %s, cancelling save_data()\n",
                    config_file_tmp,
                    config_file);

                // Attempt to restore to previous state
                rename (config_file_bak1, config_file);
                rename (config_file_bak2, config_file_bak1);
                rename (config_file_bak3, config_file_bak2);
                rename (config_file_bak4, config_file_bak3);
                return;
            }
        }
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
    }
}





void load_data_or_default(void) {
    int i;
    char name_temp[20];
    char name[50];
    long temp;

    /* language */
    if (!get_string ("LANGUAGE", lang_to_use, sizeof(lang_to_use))
            || lang_to_use[0] == '\0') {
        xastir_snprintf(lang_to_use,
            sizeof(lang_to_use),
            "English");
    }

    /* my data */
    if (!get_string ("STATION_CALLSIGN", my_callsign, sizeof(my_callsign))
            || my_callsign[0] == '\0') {
        xastir_snprintf(my_callsign,
            sizeof(my_callsign),
            "NOCALL");
    }

    if (!get_string ("STATION_LAT", my_lat, sizeof(my_lat))
            || my_lat[0] == '\0') {
        xastir_snprintf(my_lat,
            sizeof(my_lat),
            "0000.000N");
    }
    if ( (my_lat[4] != '.')
            || (my_lat[8] != 'N' && my_lat[8] != 'S') ) {
        xastir_snprintf(my_lat,
            sizeof(my_lat),
            "0000.000N");
        fprintf(stderr,"Invalid Latitude, changing it to 0000.000N\n");
    }
    // convert old data to high prec
    temp = convert_lat_s2l (my_lat);
    convert_lat_l2s (temp, my_lat, sizeof(my_lat), CONVERT_HP_NOSP);


    if (!get_string ("STATION_LONG", my_long, sizeof(my_long))
            || my_long[0] == '\0') {
        xastir_snprintf(my_long,
            sizeof(my_long),
            "00000.000W");
    }
    if ( (my_long[5] != '.')
            || (my_long[9] != 'W' && my_long[9] != 'E') ) {
        xastir_snprintf(my_long,
            sizeof(my_long),
            "00000.000W");
        fprintf(stderr,"Invalid Longitude, changing it to 00000.000W\n");
    }
    // convert old data to high prec
    temp = convert_lon_s2l (my_long);
    convert_lon_l2s (temp, my_long, sizeof(my_long), CONVERT_HP_NOSP);


    position_amb_chars = get_int ("STATION_TRANSMIT_AMB", 0, 4, 0);

    if (!get_char ("STATION_GROUP", &my_group))
        my_group = '/';

    if (!get_char ("STATION_SYMBOL", &my_symbol))
        my_symbol = 'x';

    if (!get_char ("STATION_MESSAGE_TYPE", &aprs_station_message_type))
        aprs_station_message_type = '=';

    // Empty string is ok here.
    if (!get_string ("STATION_POWER", my_phg, sizeof(my_phg)))
        my_phg[0] = '\0';

    if (!get_string ("STATION_COMMENTS", my_comment, sizeof(my_comment))
            || my_comment[0] == '\0') {
        sprintf (my_comment, "XASTIR-%s", XASTIR_SYSTEM);
    }
    /* replacing defined MY_TRAIL_DIFF_COLOR from db.c, default 0 matches 
       default value of MY_TRAIL_DIFF_COLOR to show all mycall-ssids in 
       the same color.  */
    my_trail_diff_color = get_int ("MY_TRAIL_DIFF_COLOR", 0, 1, 0);

    /* default values */
    screen_width = get_long ("SCREEN_WIDTH", 61l, 10000l, 590l);
    screen_height = get_long ("SCREEN_HEIGHT", 1l, 10000l, 420l);

//    screen_x_offset = (Position)get_int ("SCREEN_X_OFFSET", 0, 10000, 0);
//    screen_y_offset = (Position)get_int ("SCREEN_Y_OFFSET", 0, 10000, 0);


    center_latitude = get_long ("SCREEN_LAT", 0l, 64800000l, 32400000l);
    center_longitude = get_long ("SCREEN_LONG", 0l, 129600000l, 64800000l);

    // Empty string is ok here
    if (!get_string("RELAY_DIGIPEAT_CALLS", relay_digipeater_calls, sizeof(relay_digipeater_calls)))
        sprintf (relay_digipeater_calls, "WIDE1-1");
    // Make them all upper-case.
    (void)to_upper(relay_digipeater_calls);
    // And take out all spaces
    (void)remove_all_spaces(relay_digipeater_calls);

    coordinate_system = get_int ("COORDINATE_SYSTEM", 0, 5, USE_DDMMMM);

    scale_y = get_long ("SCREEN_ZOOM", 1l, 500000l, 327680l);

    scale_x = get_x_scale(center_longitude,center_latitude,scale_y);

    map_background_color = get_int ("MAP_BGCOLOR", 0, 11, 0);

    map_color_fill = get_int ( "MAP_DRAW_FILLED_COLORS", 0, 1, 1);

#if !defined(NO_GRAPHICS)
#if defined(HAVE_MAGICK)
    if (!get_string("IMAGEMAGICK_GAMMA_ADJUST", name, sizeof(name))
            || name[0] == '\0') {
        imagemagick_gamma_adjust = 0.0;
    }
    else {
        if (1 != sscanf(name, "%f", &imagemagick_gamma_adjust)) {
            fprintf(stderr,"load_data_or_default:sscanf parsing error\n");
        }
    }
#endif  // HAVE_MAGICK
    if (!get_string("RASTER_MAP_INTENSITY", name, sizeof(name))
            || name[0] == '\0') {
        raster_map_intensity = 1.0;
    }
    else {
        if (1 != sscanf(name, "%f", &raster_map_intensity)) {
            fprintf(stderr,"load_data_or_default:sscanf parsing error\n");
        }
    }
#endif  // NO_GRAPHICS

    if (!get_string ("PRINT_PROGRAM", printer_program, sizeof(printer_program))
            || printer_program[0] == '\0') {
        xastir_snprintf(printer_program,
            sizeof(printer_program),
            "%s",
#ifdef LPR_PATH
            // Path to LPR if defined
            LPR_PATH);
#else // LPR_PATH
            // Empty path
            "");
#endif // LPR_PATH
    }
    if (!get_string ("PREVIEWER_PROGRAM", previewer_program, sizeof(previewer_program))
            || previewer_program[0] == '\0') {
        xastir_snprintf(previewer_program,
            sizeof(previewer_program),
            "%s",
#ifdef GV_PATH
            // Path to GV if defined
            GV_PATH);
#else // GV_PATH
            // Empty path
            "");
#endif // GV_PATH
    }

    letter_style = get_int ("MAP_LETTERSTYLE", 0, 2, 1);

    icon_outline_style = get_int ("MAP_ICONOUTLINESTYLE", 0, 3, 0);

    wx_alert_style = get_int ("MAP_WX_ALERT_STYLE", 0, 1, 0);

    // Empty string is ok here
    if (!get_string("ALTNET_CALL", altnet_call, sizeof(altnet_call)))
        xastir_snprintf(altnet_call,
            sizeof(altnet_call),
            "XASTIR");

    altnet = get_int("ALTNET", 0, 1, 0);

    skip_dupe_checking = get_int("SKIP_DUPE_CHECK", 0, 1, 0);

    if (!get_string ("AUTO_MAP_DIR", AUTO_MAP_DIR, sizeof(AUTO_MAP_DIR))
            || AUTO_MAP_DIR[0] == '\0') {
        xastir_snprintf(AUTO_MAP_DIR,
            sizeof(AUTO_MAP_DIR),
            "%s",
            get_data_base_dir ("maps"));
    }

    if (!get_string ("ALERT_MAP_DIR", ALERT_MAP_DIR, sizeof(ALERT_MAP_DIR))
            || ALERT_MAP_DIR[0] == '\0') {
        xastir_snprintf(ALERT_MAP_DIR,
            sizeof(ALERT_MAP_DIR),
            "%s",
            get_data_base_dir ("Counties"));
    }

    if (!get_string ("SELECTED_MAP_DIR", SELECTED_MAP_DIR, sizeof(SELECTED_MAP_DIR))
            || SELECTED_MAP_DIR[0] == '\0') {
        xastir_snprintf(SELECTED_MAP_DIR,
            sizeof(SELECTED_MAP_DIR),
            "%s",
            get_data_base_dir ("maps"));
    }

    if (!get_string ("SELECTED_MAP_DATA", SELECTED_MAP_DATA, sizeof(SELECTED_MAP_DATA))
            || SELECTED_MAP_DATA[0] == '\0') {
        xastir_snprintf(SELECTED_MAP_DATA,
            sizeof(SELECTED_MAP_DATA),
            "%s",
            "config/selected_maps.sys");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), SELECTED_MAP_DATA, strlen(get_user_base_dir(""))) == 0)
         xastir_snprintf(SELECTED_MAP_DATA,
            sizeof(SELECTED_MAP_DATA),
            "%s",
            "config/selected_maps.sys");

    if (!get_string ("MAP_INDEX_DATA", MAP_INDEX_DATA, sizeof(MAP_INDEX_DATA))
            || MAP_INDEX_DATA[0] == '\0') {
        xastir_snprintf(MAP_INDEX_DATA,
            sizeof(MAP_INDEX_DATA),
            "%s",
            "config/map_index.sys");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), MAP_INDEX_DATA, strlen(get_user_base_dir(""))) == 0)
        xastir_snprintf(MAP_INDEX_DATA,
            sizeof(MAP_INDEX_DATA),
            "%s",
            "config/map_index.sys");
 
    if (!get_string ("SYMBOLS_DIR", SYMBOLS_DIR, sizeof(SYMBOLS_DIR))
            || SYMBOLS_DIR[0] == '\0') {
        xastir_snprintf(SYMBOLS_DIR,
            sizeof(SYMBOLS_DIR),
            "%s",
            get_data_base_dir ("symbols"));
    }

    if (!get_string ("SOUND_DIR", SOUND_DIR, sizeof(SOUND_DIR))
            || SOUND_DIR[0] == '\0') {
        xastir_snprintf(SOUND_DIR,
            sizeof(SOUND_DIR),
            "%s",
            get_data_base_dir ("sounds"));
    }

    if (!get_string ("GROUP_DATA_FILE", group_data_file, sizeof(group_data_file))
            || group_data_file[0] == '\0') {
        xastir_snprintf(group_data_file,
            sizeof(group_data_file),
            "%s",
            "config/groups");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), group_data_file, strlen(get_user_base_dir(""))) == 0)
        xastir_snprintf(group_data_file,
             sizeof(group_data_file),
            "%s",
            "config/groups");

    if (!get_string ("GNIS_FILE", locate_gnis_filename, sizeof(locate_gnis_filename))
            || locate_gnis_filename[0] == '\0') {
        xastir_snprintf(locate_gnis_filename,
            sizeof(locate_gnis_filename),
            "%s",
            get_data_base_dir ("GNIS/WA.gnis"));
    }

    if (!get_string ("GEOCODE_FILE", geocoder_map_filename, sizeof(geocoder_map_filename))
            || geocoder_map_filename[0] == '\0') {
        xastir_snprintf(geocoder_map_filename,
            sizeof(geocoder_map_filename),
            "%s",
            get_data_base_dir ("GNIS/geocode"));
    }

    show_destination_mark = get_int ("SHOW_FIND_TARGET", 0, 1, 1);

    /* maps */
    long_lat_grid = get_int ("MAPS_LONG_LAT_GRID", 0, 1, 1);

    draw_labeled_grid_border = get_int ("MAPS_LABELED_GRID_BORDER", 0, 1, 0);

    map_color_levels = get_int ("MAPS_LEVELS", 0, 1, 1);

    map_labels = get_int ("MAPS_LABELS", 0, 1, 1);

    map_auto_maps = get_int ("MAPS_AUTO_MAPS", 0, 1, 0);

    auto_maps_skip_raster = get_int ("MAPS_AUTO_MAPS_SKIP_RASTER", 0, 1, 0);

    index_maps_on_startup = get_int ("MAPS_INDEX_ON_STARTUP", 0, 1, 1);

    if (!get_string ("MAPS_LABEL_FONT_TINY", rotated_label_fontname[FONT_TINY], sizeof(rotated_label_fontname[FONT_TINY]))
            || rotated_label_fontname[FONT_TINY][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_TINY],
            sizeof(rotated_label_fontname[FONT_TINY]),
            "-adobe-helvetica-medium-r-normal--8-*-*-*-*-*-iso8859-1");
    }

    if (!get_string ("MAPS_LABEL_FONT_SMALL", rotated_label_fontname[FONT_SMALL], sizeof(rotated_label_fontname[FONT_SMALL]))
            || rotated_label_fontname[FONT_SMALL][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_SMALL],
            sizeof(rotated_label_fontname[FONT_SMALL]),
            "-adobe-helvetica-medium-r-normal--10-*-*-*-*-*-iso8859-1");
    }

    if (!get_string ("MAPS_LABEL_FONT_MEDIUM", rotated_label_fontname[FONT_MEDIUM], sizeof(rotated_label_fontname[FONT_MEDIUM]))
            || rotated_label_fontname[FONT_MEDIUM][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_MEDIUM],
            sizeof(rotated_label_fontname[FONT_MEDIUM]),
            "-adobe-helvetica-medium-r-normal--12-*-*-*-*-*-iso8859-1");
    }
    // NOTE:  FONT_DEFAULT points to FONT_MEDIUM

    if (!get_string ("MAPS_LABEL_FONT_LARGE", rotated_label_fontname[FONT_LARGE], sizeof(rotated_label_fontname[FONT_LARGE]))
            || rotated_label_fontname[FONT_LARGE][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_LARGE],
            sizeof(rotated_label_fontname[FONT_LARGE]),
            "-adobe-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1");
    }

    if (!get_string ("MAPS_LABEL_FONT_HUGE", rotated_label_fontname[FONT_HUGE], sizeof(rotated_label_fontname[FONT_HUGE]))
            || rotated_label_fontname[FONT_HUGE][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_HUGE],
            sizeof(rotated_label_fontname[FONT_HUGE]),
            "-adobe-helvetica-medium-r-normal--24-*-*-*-*-*-iso8859-1");
    }
    
    if (!get_string ("MAPS_LABEL_FONT_BORDER", rotated_label_fontname[FONT_BORDER], sizeof(rotated_label_fontname[FONT_BORDER]))
            || rotated_label_fontname[FONT_BORDER][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_BORDER],
            sizeof(rotated_label_fontname[FONT_BORDER]),
            "-adobe-helvetica-medium-r-normal--14-*-*-*-*-*-iso8859-1");
    }

    if (!get_string ("SYSTEM_FIXED_FONT", rotated_label_fontname[FONT_SYSTEM], sizeof(rotated_label_fontname[FONT_SYSTEM]))
            || rotated_label_fontname[FONT_SYSTEM][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_SYSTEM],
            sizeof(rotated_label_fontname[FONT_SYSTEM]),
            "fixed");
            // NOTE:  This same default font is hard-coded into
            // main.c, to be used for the case when the user enters
            // an invalid font (so the program won't crash).
    }

    if (!get_string ("STATION_FONT", rotated_label_fontname[FONT_STATION], sizeof(rotated_label_fontname[FONT_STATION]))
            || rotated_label_fontname[FONT_STATION][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_STATION],
            sizeof(rotated_label_fontname[FONT_STATION]),
            "fixed");
            // NOTE:  This same default font is hard-coded into
            // main.c, to be used for the case when the user enters
            // an invalid font (so the program won't crash).
    }

    if (!get_string ("ATV_ID_FONT", rotated_label_fontname[FONT_ATV_ID], sizeof(rotated_label_fontname[FONT_ATV_ID]))
            || rotated_label_fontname[FONT_ATV_ID][0] == '\0') {
        xastir_snprintf(rotated_label_fontname[FONT_ATV_ID],
            sizeof(rotated_label_fontname[FONT_ATV_ID]),
            "-*-helvetica-*-*-*-*-*-240-*-*-*-*-*-*");
    }


//N0VH
#if defined(HAVE_MAGICK)
    net_map_timeout = get_int ("NET_MAP_TIMEOUT", 10, 300, 120);

    tiger_show_grid = get_int ("TIGERMAP_SHOW_GRID", 0, 1, 0);
    tiger_show_counties = get_int ("TIGERMAP_SHOW_COUNTIES", 0, 1, 1);
    tiger_show_cities = get_int ("TIGERMAP_SHOW_CITIES", 0, 1, 1);
    tiger_show_places = get_int ("TIGERMAP_SHOW_PLACES", 0, 1, 1);
    tiger_show_majroads = get_int ("TIGERMAP_SHOW_MAJROADS", 0, 1, 1);
    tiger_show_streets = get_int ("TIGERMAP_SHOW_STREETS", 0, 1, 0);
    tiger_show_railroad = get_int ("TIGERMAP_SHOW_RAILROAD", 0, 1, 1);
    tiger_show_states = get_int ("TIGERMAP_SHOW_STATES", 0, 1, 0);
    tiger_show_interstate = get_int ("TIGERMAP_SHOW_INTERSTATE", 0, 1, 1);
    tiger_show_ushwy = get_int ("TIGERMAP_SHOW_USHWY", 0, 1, 1);
    tiger_show_statehwy = get_int ("TIGERMAP_SHOW_STATEHWY", 0, 1, 1);
    tiger_show_water = get_int ("TIGERMAP_SHOW_WATER", 0, 1, 1);
    tiger_show_lakes = get_int ("TIGERMAP_SHOW_LAKES", 0, 1, 1);
    tiger_show_misc = get_int ("TIGERMAP_SHOW_MISC", 0, 1, 1);
#endif //HAVE_MAGICK

#ifdef HAVE_LIBGEOTIFF
    DRG_XOR_colors = get_int ("DRG_XOR_COLORS", 0, 1, 0);
    DRG_show_colors[0] = get_int ("DRG_SHOW_COLORS_0", 0, 1, 1);
    DRG_show_colors[1] = get_int ("DRG_SHOW_COLORS_1", 0, 1, 1);
    DRG_show_colors[2] = get_int ("DRG_SHOW_COLORS_2", 0, 1, 1);
    DRG_show_colors[3] = get_int ("DRG_SHOW_COLORS_3", 0, 1, 1);
    DRG_show_colors[4] = get_int ("DRG_SHOW_COLORS_4", 0, 1, 1);
    DRG_show_colors[5] = get_int ("DRG_SHOW_COLORS_5", 0, 1, 1);
    DRG_show_colors[6] = get_int ("DRG_SHOW_COLORS_6", 0, 1, 1);
    DRG_show_colors[7] = get_int ("DRG_SHOW_COLORS_7", 0, 1, 1);
    DRG_show_colors[8] = get_int ("DRG_SHOW_COLORS_8", 0, 1, 1);
    DRG_show_colors[9] = get_int ("DRG_SHOW_COLORS_9", 0, 1, 1);
    DRG_show_colors[10] = get_int ("DRG_SHOW_COLORS_10", 0, 1, 1);
    DRG_show_colors[11] = get_int ("DRG_SHOW_COLORS_11", 0, 1, 1);
    DRG_show_colors[12] = get_int ("DRG_SHOW_COLORS_12", 0, 1, 1);
#endif  // HAVE_LIBGEOTIFF

    // filter values
    // NOT SAVED: Select_.none
    Select_.mine = get_int ("DISPLAY_MY_STATION", 0, 1, 1);
    Select_.tnc = get_int ("DISPLAY_TNC_STATIONS", 0, 1, 1);
    Select_.direct = get_int ("DISPLAY_TNC_DIRECT_STATIONS", 0, 1, 1);
    Select_.via_digi = get_int ("DISPLAY_TNC_VIADIGI_STATIONS", 0, 1, 1);
    Select_.net = get_int ("DISPLAY_NET_STATIONS", 0, 1, 1);
    Select_.tactical = get_int ("DISPLAY_TACTICAL_STATIONS", 0, 1, 0);
    Select_.old_data = get_int ("DISPLAY_OLD_STATION_DATA", 0, 1, 0);
    Select_.stations = get_int ("DISPLAY_STATIONS", 0, 1, 1);
    Select_.fixed_stations = get_int ("DISPLAY_FIXED_STATIONS", 0, 1, 1);
    Select_.moving_stations = get_int ("DISPLAY_MOVING_STATIONS", 0, 1, 1);
    Select_.weather_stations = get_int ("DISPLAY_WEATHER_STATIONS", 0, 1, 1);
    Select_.CWOP_wx_stations = get_int ("DISPLAY_CWOP_WX_STATIONS", 0, 1, 1);
    Select_.objects = get_int ("DISPLAY_OBJECTS", 0, 1, 1);
    Select_.weather_objects = get_int ("DISPLAY_STATION_WX_OBJ", 0, 1, 1);
    Select_.gauge_objects = get_int ("DISPLAY_WATER_GAGE_OBJ", 0, 1, 1);
    Select_.other_objects = get_int ("DISPLAY_OTHER_OBJECTS", 0, 1, 1);

    // display values
    Display_.callsign = get_int ("DISPLAY_CALLSIGN", 0, 1, 1);
    Display_.label_all_trackpoints = get_int ("DISPLAY_LABEL_ALL_TRACKPOINTS", 0, 1, 0);
    Display_.symbol = get_int ("DISPLAY_SYMBOL", 0, 1, 1);
    Display_.symbol_rotate = get_int ("DISPLAY_SYMBOL_ROTATE", 0, 1, 1);
    Display_.phg = get_int ("DISPLAY_STATION_PHG", 0, 1, 0);
    Display_.default_phg = get_int ("DISPLAY_DEFAULT_PHG", 0, 1, 0);
    Display_.phg_of_moving = get_int ("DISPLAY_MOBILES_PHG", 0, 1, 0);
    Display_.altitude = get_int ("DISPLAY_ALTITUDE", 0, 1, 0);
    Display_.course = get_int ("DISPLAY_COURSE", 0, 1, 0);
    Display_.speed = get_int ("DISPLAY_SPEED", 0, 1, 1);
    Display_.speed_short = get_int ("DISPLAY_SPEED_SHORT", 0, 1, 0);
    Display_.dist_bearing = get_int ("DISPLAY_DIST_COURSE", 0, 1, 0);
    Display_.weather = get_int ("DISPLAY_WEATHER", 0, 1, 1);
    Display_.weather_text = get_int ("DISPLAY_STATION_WX", 0, 1, 1);
    Display_.temperature_only = get_int ("DISPLAY_TEMP_ONLY", 0, 1, 0);
    Display_.wind_barb = get_int ("DISPLAY_WIND_BARB", 0, 1, 1);
    Display_.trail = get_int ("DISPLAY_STATION_TRAILS", 0, 1, 1);
    Display_.last_heard = get_int ("DISPLAY_LAST_HEARD", 0, 1, 0);
    Display_.ambiguity = get_int ("DISPLAY_POSITION_AMB", 0, 1, 1);
    Display_.df_data = get_int ("DISPLAY_DF_INFO", 0, 1, 1);
    Display_.dr_data = get_int ("DISPLAY_DEAD_RECKONING_INFO", 0, 1, 1);
    Display_.dr_arc = get_int ("DISPLAY_DEAD_RECKONING_ARC", 0, 1, 1);
    Display_.dr_course = get_int ("DISPLAY_DEAD_RECKONING_COURSE", 0, 1, 1);
    Display_.dr_symbol = get_int ("DISPLAY_DEAD_RECKONING_SYMBOL", 0, 1, 1);



    english_units = get_int ("DISPLAY_UNITS_ENGLISH", 0, 1, 0);
    do_dbstatus = get_int ("DISPLAY_DIST_BEAR_STATUS", 0, 1, 0);


    CAD_show_label = get_int ("DISPLAY_CAD_OBJECT_LABEL", 0, 1, 1);
    CAD_show_raw_probability = get_int ("DISPLAY_CAD_OBJECT_PROBABILITY", 0, 1, 1 );
    CAD_show_comment = get_int ("DISPLAY_CAD_OBJECT_COMMENT", 0, 1, 1 );
    CAD_show_area = get_int ("DISPLAY_CAD_OBJECT_AREA", 0, 1, 1 );


    transmit_disable = get_int ("DISABLE_TRANSMIT", 0, 1, 0);
    posit_tx_disable = get_int ("DISABLE_POSIT_TX", 0, 1, 0);
    object_tx_disable = get_int ("DISABLE_OBJECT_TX", 0, 1, 0);
    enable_server_port = get_int ("ENABLE_SERVER_PORT", 0, 1, 0);


    for (i = 0; i < MAX_IFACE_DEVICES; i++) {
        sprintf (name_temp, "DEVICE%0d_", i);
        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TYPE", sizeof(name) - 1 - strlen(name));
        devices[i].device_type = get_int (name, 0,MAX_IFACE_DEVICE_TYPES,DEVICE_NONE);
        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "NAME", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].device_name, sizeof(devices[i].device_name)))
            devices[i].device_name[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "RADIO_PORT", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].radio_port, sizeof(devices[i].radio_port)))
            xastir_snprintf(devices[i].radio_port,
                sizeof(devices[i].radio_port),
                "0");

#ifdef HAVE_DB        

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "DATABASE_TYPE", sizeof(name) - 1 - strlen(name));
        
        devices[i].database_type = get_int (name, 0,MAX_DB_TYPE,MAX_DB_TYPE);
        
        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "DATABASE_SCHEMA_TYPE", sizeof(name) - 1 - strlen(name));
        devices[i].database_schema_type = get_int (name, 0,MAX_XASTIR_SCHEMA,XASTIR_SCHEMA_SIMPLE);
        
        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "DATABASE_USERNAME", sizeof(name) - 1 - strlen(name));
        // default to xastir
        if (!get_string (name, devices[i].database_username, sizeof(devices[i].database_username)))
            xastir_snprintf(devices[i].database_username,
                sizeof(devices[i].database_username),
                "xastir");
        
        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "DATABASE_SCHEMA", sizeof(name) - 1 - strlen(name));
        // default to xastir
        if (!get_string (name, devices[i].database_schema, sizeof(devices[i].database_schema)))
            xastir_snprintf(devices[i].database_schema,
                sizeof(devices[i].database_schema),
                "xastir");

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "DATABASE_UNIX_SOCKET", sizeof(name) - 1 - strlen(name));
        // empty string is ok here
        if (!get_string (name, devices[i].database_unix_socket, sizeof(devices[i].database_unix_socket)))
            xastir_snprintf(devices[i].database_unix_socket,
                sizeof(devices[i].database_unix_socket),
                "0");

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "QUERY_ON_STARTUP", sizeof(name) - 1 - strlen(name));
        devices[i].query_on_startup = get_int (name, 0,1,0);

#endif /* HAVE_DB */

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "INTERFACE_COMMENT", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].comment, sizeof(devices[i].comment)))
            devices[i].comment[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "HOST", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].device_host_name, sizeof(devices[i].device_host_name)))
            devices[i].device_host_name[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "PASSWD", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].device_host_pswd, sizeof(devices[i].device_host_pswd)))
            devices[i].device_host_pswd[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "FILTER_PARAMS", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].device_host_filter_string, sizeof(devices[i].device_host_filter_string)))
            devices[i].device_host_filter_string[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "UNPROTO1", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].unproto1, sizeof(devices[i].unproto1)))
            devices[i].unproto1[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "UNPROTO2", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].unproto2, sizeof(devices[i].unproto2)))
            devices[i].unproto2[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "UNPROTO3", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].unproto3, sizeof(devices[i].unproto3)))
            devices[i].unproto3[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "UNPROTO_IGATE", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].unproto_igate, sizeof(devices[i].unproto_igate))
                || devices[i].unproto_igate[0] == '\0') {
            xastir_snprintf(devices[i].unproto_igate, sizeof(devices[i].unproto_igate), "WIDE2-1");
        }

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_UP_FILE", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].tnc_up_file, sizeof(devices[i].tnc_up_file)))
            devices[i].tnc_up_file[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_DOWN_FILE", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].tnc_down_file, sizeof(devices[i].tnc_down_file)))
            devices[i].tnc_down_file[0] = '\0';

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_TXDELAY", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].txdelay, sizeof(devices[i].txdelay)))
            xastir_snprintf(devices[i].txdelay,
                sizeof(devices[i].txdelay),
                "40");

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_PERSISTENCE", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].persistence, sizeof(devices[i].persistence)))
            xastir_snprintf(devices[i].persistence,
                sizeof(devices[i].persistence),
                "63");

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_SLOTTIME", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].slottime, sizeof(devices[i].slottime)))
            xastir_snprintf(devices[i].slottime,
                sizeof(devices[i].slottime),
                "10");

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_TXTAIL", sizeof(name) - 1 - strlen(name));
        // Empty string is ok here.
        if (!get_string (name, devices[i].txtail, sizeof(devices[i].txtail)))
            xastir_snprintf(devices[i].txtail,
                sizeof(devices[i].txtail),
                "30");

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_FULLDUPLEX", sizeof(name) - 1 - strlen(name));
        devices[i].fullduplex = get_int (name, 0, 1, 0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TNC_INIT_KISSMODE", sizeof(name) - 1 - strlen(name));
        devices[i].init_kiss = get_int (name, 0, 1, 0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "SPEED", sizeof(name) - 1 - strlen(name));
        devices[i].sp = get_int (name, 0,230400,0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "STYLE", sizeof(name) - 1 - strlen(name));
        devices[i].style = get_int (name, 0,2,0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "IGATE_OPTION", sizeof(name) - 1 - strlen(name));
        devices[i].igate_options = get_int (name, 0,2,0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "TXMT", sizeof(name) - 1 - strlen(name));
        devices[i].transmit_data = get_int (name, 0,1,0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "RELAY_DIGIPEAT", sizeof(name) - 1 - strlen(name));
        devices[i].relay_digipeat = get_int (name, 0,1,1);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "RECONN", sizeof(name) - 1 - strlen(name));
        devices[i].reconnect = get_int (name, 0,1,0);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "ONSTARTUP", sizeof(name) - 1 - strlen(name));
        devices[i].connect_on_startup = get_int (name, 0,1,0);

                xastir_snprintf(name,
                    sizeof(name),
                    "%s",
                    name_temp);
                strncat (name, "GPSRETR", sizeof(name) - 1 - strlen(name));
                devices[i].gps_retrieve = get_int (name, 0,255,DEFAULT_GPS_RETR);

                xastir_snprintf(name,
                    sizeof(name),
                    "%s",
                    name_temp);
                strncat (name, "SETTIME", sizeof(name) - 1 - strlen(name));
                devices[i].set_time = get_int (name, 0,1,0);
    }

    /* TNC */
    log_tnc_data = get_int ("TNC_LOG_DATA", 0,1,0);

    if (!get_string ("LOGFILE_TNC", LOGFILE_TNC, sizeof(LOGFILE_TNC))
            || LOGFILE_TNC[0] == '\0') {
        xastir_snprintf(LOGFILE_TNC,
            sizeof(LOGFILE_TNC),
            "%s",
            "logs/tnc.log");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), LOGFILE_TNC, strlen(get_user_base_dir(""))) == 0)
         xastir_snprintf(LOGFILE_TNC,
            sizeof(LOGFILE_TNC),
            "%s",
            "logs/tnc.log");

    /* NET */
    log_net_data = get_int ("NET_LOG_DATA", 0,1,0);

    operate_as_an_igate = get_int ("NET_RUN_AS_IGATE", 0,2,0);

    log_igate = get_int ("LOG_IGATE", 0,1,0);

    NETWORK_WAITTIME = get_int ("NETWORK_WAITTIME", 10,120,10);

    // LOGGING
    log_wx = get_int ("LOG_WX", 0,1,0);
    log_message_data = get_int ("LOG_MESSAGE", 0, 1, 0);
    log_wx_alert_data = get_int ("LOG_WX_ALERT", 0, 1, 0);

    if (!get_string ("LOGFILE_IGATE", LOGFILE_IGATE, sizeof(LOGFILE_IGATE))
            || LOGFILE_IGATE[0] == '\0') {
        xastir_snprintf(LOGFILE_IGATE,
            sizeof(LOGFILE_IGATE),
            "%s",
            "logs/igate.log");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), LOGFILE_IGATE, strlen(get_user_base_dir(""))) == 0)
         xastir_snprintf(LOGFILE_IGATE,
            sizeof(LOGFILE_IGATE),
            "%s",
            "logs/igate.log");

    if (!get_string ("LOGFILE_NET", LOGFILE_NET, sizeof(LOGFILE_NET))
            || LOGFILE_NET[0] == '\0') {
        xastir_snprintf(LOGFILE_NET,
            sizeof(LOGFILE_NET),
            "%s",
            "logs/net.log");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), LOGFILE_NET, strlen(get_user_base_dir(""))) == 0)
        xastir_snprintf(LOGFILE_NET,
            sizeof(LOGFILE_NET),
            "%s",
            "logs/net.log");
  
    if (!get_string ("LOGFILE_WX", LOGFILE_WX, sizeof(LOGFILE_WX))
            || LOGFILE_WX[0] == '\0') {
        xastir_snprintf(LOGFILE_WX,
            sizeof(LOGFILE_WX),
            "%s",
            "logs/wx.log");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), LOGFILE_WX, strlen(get_user_base_dir(""))) == 0)
        xastir_snprintf(LOGFILE_WX,
            sizeof(LOGFILE_WX),
            "%s",
            "logs/wx.log");

    if (!get_string ("LOGFILE_MESSAGE", LOGFILE_MESSAGE, sizeof(LOGFILE_MESSAGE))
            || LOGFILE_MESSAGE[0] == '\0') {
        xastir_snprintf(LOGFILE_MESSAGE,
            sizeof(LOGFILE_MESSAGE),
            "%s",
            "logs/message.log");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), LOGFILE_MESSAGE, strlen(get_user_base_dir(""))) == 0)
        xastir_snprintf(LOGFILE_MESSAGE,
            sizeof(LOGFILE_MESSAGE),
            "%s",
            "logs/message.log");
 
    if (!get_string ("LOGFILE_WX_ALERT", LOGFILE_WX_ALERT, sizeof(LOGFILE_WX_ALERT))
            || LOGFILE_WX_ALERT[0] == '\0') {
        xastir_snprintf(LOGFILE_WX_ALERT,
            sizeof(LOGFILE_WX_ALERT),
            "%s",
            "logs/wx_alert.log");
    }
    // Check for old complete path, change to new short path if a
    // match
    if (strncmp( get_user_base_dir(""), LOGFILE_WX_ALERT, strlen(get_user_base_dir(""))) == 0)
        xastir_snprintf(LOGFILE_WX_ALERT,
            sizeof(LOGFILE_WX_ALERT),
            "%s",
            "logs/wx_alert.log");
  
    // SNAPSHOTS
    snapshots_enabled = get_int ("SNAPSHOTS_ENABLED", 0,1,0);

    // KML SNAPSHOTS
    kmlsnapshots_enabled = get_int ("KMLSNAPSHOTS_ENABLED", 0,1,0);

    /* WX ALERTS */
    WX_ALERTS_REFRESH_TIME = (time_t) get_long ("WX_ALERTS_REFRESH_TIME", 10l, 86400l, 60l);

    /* gps */
    gps_time = (time_t) get_long ("GPS_TIME", 1l, 86400l, 60l);

    /* POSIT RATE */
    POSIT_rate = (time_t) get_long ("POSIT_RATE", 1l, 86400l, 30*60l);

    /* OBJECT RATE */
    OBJECT_rate = (time_t) get_long ("OBJECT_RATE", 1l, 86400l, 30*60l);

    /* UPDATE DR RATE */
    update_DR_rate = (time_t) get_long ("UPDATE_DR_RATE", 1l, 86400l, 30l);

    /* station broadcast type */
    output_station_type = get_int ("BST_TYPE", 0,5,0);

#ifdef TRANSMIT_RAW_WX
    /* raw wx transmit */
    transmit_raw_wx = get_int ("BST_WX_RAW", 0,1,0);
#endif  // TRANSMIT_RAW_WX

    /* compressed posit transmit */
    transmit_compressed_posit = get_int ("BST_COMPRESSED_POSIT", 0,1,0);

    /* compressed objects/items transmit */
    transmit_compressed_objects_items = get_int ("COMPRESSED_OBJECTS_ITEMS", 0,1,0);

    smart_beaconing = get_int ("SMART_BEACONING", 0,1,1);
    sb_turn_min = get_int ("SB_TURN_MIN", 1,360,20);
    sb_turn_slope = get_int ("SB_TURN_SLOPE", 0,360,25);
    sb_turn_time = get_int ("SB_TURN_TIME", 0,3600,5);
    sb_posit_fast = get_int ("SB_POSIT_FAST", 1,1440,60);
    sb_posit_slow = get_int ("SB_POSIT_SLOW", 1,1440,30);
    sb_low_speed_limit = get_int ("SB_LOW_SPEED_LIMIT", 0,999,2);
    sb_high_speed_limit = get_int ("SB_HIGH_SPEED_LIMIT", 0,999,60);

    pop_up_new_bulletins = get_int ("POP_UP_NEW_BULLETINS", 0,1,1);
    view_zero_distance_bulletins = get_int ("VIEW_ZERO_DISTANCE_BULLETINS", 0,1,1);

    warn_about_mouse_modifiers = get_int ("WARN_ABOUT_MOUSE_MODIFIERS", 0,1,1);


    /* Audio Alarms*/
    // Empty string is ok here.
    if (!get_string ("SOUND_COMMAND", sound_command, sizeof(sound_command)))
        xastir_snprintf(sound_command,
            sizeof(sound_command),
            "play");

    sound_play_new_station = get_int ("SOUND_PLAY_ONS", 0,1,0);

    if (!get_string ("SOUND_ONS_FILE", sound_new_station, sizeof(sound_new_station))
            || sound_new_station[0] == '\0') {
        xastir_snprintf(sound_new_station,
            sizeof(sound_new_station),
            "newstation.wav");
    }

    sound_play_new_message = get_int ("SOUND_PLAY_ONM", 0,1,0);

    if (!get_string ("SOUND_ONM_FILE", sound_new_message, sizeof(sound_new_message))
            || sound_new_message[0] == '\0') {
        xastir_snprintf(sound_new_message,
            sizeof(sound_new_message),
            "newmessage.wav");
    }

    sound_play_prox_message = get_int ("SOUND_PLAY_PROX", 0,1,0);

    if (!get_string ("SOUND_PROX_FILE", sound_prox_message, sizeof(sound_prox_message))
            || sound_prox_message[0] == '\0') {
        xastir_snprintf(sound_prox_message,
            sizeof(sound_prox_message),
            "proxwarn.wav");
    }

    if (!get_string ("PROX_MIN", prox_min, sizeof(prox_min))
            || prox_min[0] == '\0') {
        xastir_snprintf(prox_min,
            sizeof(prox_min),
            "0.01");
    }

    if (!get_string ("PROX_MAX", prox_max, sizeof(prox_max))
            || prox_max[0] == '\0') {
        xastir_snprintf(prox_max,
            sizeof(prox_max),
            "10");
    }

    sound_play_band_open_message = get_int ("SOUND_PLAY_BAND", 0,1,0);

    if (!get_string ("SOUND_BAND_FILE", sound_band_open_message, sizeof(sound_band_open_message))
            || sound_band_open_message[0] == '\0') {
        xastir_snprintf(sound_band_open_message,
            sizeof(sound_band_open_message),
            "bandopen.wav");
    }

    if (!get_string ("BANDO_MIN", bando_min, sizeof(bando_min))
            || bando_min[0] == '\0') {
        xastir_snprintf(bando_min,
            sizeof(bando_min),
            "200");
    }

    if (!get_string ("BANDO_MAX", bando_max, sizeof(bando_max))
            || bando_max[0] == '\0') {
        xastir_snprintf(bando_max,
            sizeof(bando_max),
            "2000");
    }

    sound_play_wx_alert_message = get_int ("SOUND_PLAY_WX_ALERT", 0,1,0);

    if (!get_string ("SOUND_WX_ALERT_FILE", sound_wx_alert_message, sizeof(sound_wx_alert_message))
            || sound_wx_alert_message[0] == '\0') {
        xastir_snprintf(sound_wx_alert_message,
            sizeof(sound_wx_alert_message),
            "thunder.wav");
    }

#ifdef HAVE_FESTIVAL
    /* Festival Speech defaults */
    festival_speak_new_station = get_int ("SPEAK_NEW_STATION",0,1,0);
    festival_speak_proximity_alert = get_int ("SPEAK_PROXIMITY_ALERT",0,1,0);
    festival_speak_tracked_proximity_alert = get_int ("SPEAK_TRACKED_ALERT",0,1,0);
    festival_speak_band_opening = get_int ("SPEAK_BAND_OPENING",0,1,0);
    festival_speak_new_message_alert = get_int ("SPEAK_MESSAGE_ALERT",0,1,0);
    festival_speak_new_message_body = get_int ("SPEAK_MESSAGE_BODY",0,1,0);
    festival_speak_new_weather_alert = get_int ("SPEAK_WEATHER_ALERT",0,1,0);
    festival_speak_new_station = get_int ("SPEAK_ID",0,1,0);
#endif  // HAVE_FESTIVAL

    ATV_screen_ID = get_int ("ATV_SCREEN_ID",0,1,0);
 
    /* defaults */
    sec_old = (time_t) get_long ("DEFAULT_STATION_OLD", 1l, 604800l, 4800l);

    sec_clear = (time_t) get_long ("DEFAULT_STATION_CLEAR", 1l, 604800l, 43200l);

    sec_remove = get_long("DEFAULT_STATION_REMOVE", 1l, 604800*2, sec_clear*2);

    if (!get_string ("MESSAGE_COUNTER", message_counter, sizeof(message_counter))
            || message_counter[0] == '\0') {
        xastir_snprintf(message_counter,
            sizeof(message_counter),
            "00");
    }

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

    if (!get_string ("AUTO_MSG_REPLY", auto_reply_message, sizeof(auto_reply_message))
        || auto_reply_message[0] == '\0') {
        xastir_snprintf(auto_reply_message,
            sizeof(auto_reply_message),
            "Autoreply- No one is at the keyboard");
    }

    Display_packet_data_type = get_int ("DISPLAY_PACKET_TYPE", 0,2,0);

    bulletin_range = get_int ("BULLETIN_RANGE", 0,99999,0);

    vm_range = get_int("VIEW_MESSAGE_RANGE", 0,99999,0);

    view_message_limit = get_int("VIEW_MESSAGE_LIMIT", 10000,99999,10000);
    predefined_menu_from_file = get_int("PREDEF_MENU_LOAD",0,1,0);
    if (!get_string ("PREDEF_MENU_FILE", predefined_object_definition_filename, sizeof(predefined_object_definition_filename))
            || predefined_object_definition_filename[0] == '\0') {
        xastir_snprintf(predefined_object_definition_filename,
            sizeof(predefined_object_definition_filename),
            "predefined_SAR.sys");
    }

    Read_messages_packet_data_type = get_int ("READ_MESSAGES_PACKET_DATA_TYPE", 0,2,0);
    Read_messages_mine_only = get_int ("READ_MESSAGES_MINE_ONLY", 0,1,0);

    /* printer variables */
    print_rotated = get_int ("PRINT_ROTATED", 0,1,0);
    print_auto_rotation = get_int ("PRINT_AUTO_ROTATION", 0,1,1);
    print_auto_scale = get_int ("PRINT_AUTO_SCALE", 0,1,1);
    print_in_monochrome = get_int ("PRINT_IN_MONOCHROME", 0,1,0);
    print_invert = get_int ("PRINT_INVERT_COLORS", 0,1,0);

    // 0 = no correction
    WX_rain_gauge_type = get_int ("RAIN_GAUGE_TYPE", 0,3,0);


    /* list attributes */
    for (i = 0; i < LST_NUM; i++) {
        sprintf (name_temp, "LIST%0d_", i);
        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "H", sizeof(name) - 1 - strlen(name));
        list_size_h[i] = get_int (name, -1,8192,-1);

        xastir_snprintf(name,
            sizeof(name),
            "%s",
            name_temp);
        strncat (name, "W", sizeof(name) - 1 - strlen(name));
        list_size_w[i] = get_int (name, -1,8192,-1);
    }

    // 0 = no tracking
    track_me = get_int ("TRACK_ME", 0,1,0);

//    store_string (fout, "TRACKING_STATION_CALLSIGN", tracking_station_call);
    if (!get_string ("TRACKING_STATION_CALLSIGN", tracking_station_call, sizeof(tracking_station_call))) {
        tracking_station_call[0] = '\0';
    }


    map_chooser_expand_dirs = get_int ("MAP_CHOOSER_EXPAND_DIRS", 0,1,1);

    // One hour default
    st_direct_timeout = get_int ("ST_DIRECT_TIMEOUT", 1,60*60*24*30,60*60);

    // Ten minute default
    dead_reckoning_timeout = get_int ("DEAD_RECKONING_TIMEOUT", 1,60*60*24*30,60*10);

    // 1ms default
    serial_char_pacing = get_int ("SERIAL_CHAR_PACING", 0,50,1);

    // 45 minutes default, 12 hours max
    trail_segment_time = get_int ("TRAIL_SEGMENT_TIME", 0,12*60,45);

    // 1 degree default
    trail_segment_distance = get_int ("TRAIL_SEGMENT_DISTANCE", 0,45,1);

    // 0 minutes default (function disabled)
    RINO_download_interval = get_int ("RINO_DOWNLOAD_INTERVAL", 0,30,0);

    // 5 minutes default
    snapshot_interval = get_int ("SNAPSHOT_INTERVAL", 1,30,5);

    input_close();
}


