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


#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stdint.h>
#include <ctype.h>
#include <math.h>

#if TIME_WITH_SYS_TIME
  #include <sys/time.h>
  #include <time.h>
#else   // TIME_WITH_SYS_TIME
  #if HAVE_SYS_TIME_H
    #include <sys/time.h>
  #else  // HAVE_SYS_TIME_H
    #include <time.h>
  #endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include "xastir.h"
#include "draw_symbols.h"
#include "main.h"
#include "xa_config.h"
#include "maps.h"
#include "interface.h"
#include "objects.h"

void move_station_time(DataRow *p_curr, DataRow *p_time);

#include <Xm/XmAll.h>
#include <X11/cursorfont.h>

// Must be last include file
#include "leak_detection.h"


extern XmFontList fontlist1;    // Menu/System fontlist

// lesstif (at least as of version 0.94 in 2008), doesn't
// have full implementation of combo boxes.
#ifndef USE_COMBO_BOX
  #if (XmVERSION >= 2 && !defined(LESSTIF_VERSION))
    #define USE_COMBO_BOX 1
  #endif
#endif  // USE_COMBO_BOX


// ---------------------------- object -------------------------------
Widget object_dialog = (Widget)NULL;
Widget df_object_dialog = (Widget)NULL;
Widget object_name_data,
       object_lat_data_deg, object_lat_data_min, object_lat_data_ns,
       object_lon_data_deg, object_lon_data_min, object_lon_data_ew,
       object_group_data, object_symbol_data, object_icon,
       object_comment_data, ob_frame, ob_group, ob_symbol,
       ob_option_frame,
       signpost_frame, area_frame, area_toggle, signpost_toggle,
       df_bearing_toggle, map_view_toggle, probabilities_toggle,
       ob_bearing_data, frameomni, framebeam,
       ob_speed, ob_speed_data, ob_course, ob_course_data,
       ob_comment,
       ob_altitude, ob_altitude_data, signpost_data,
       probability_data_min, probability_data_max,
       open_filled_toggle, ob_lat_offset_data, ob_lon_offset_data,
       ob_corridor, ob_corridor_data, ob_corridor_miles,
       omni_antenna_toggle, beam_antenna_toggle;
Pixmap Ob_icon0, Ob_icon;
void Set_Del_Object(Widget w, XtPointer clientData, XtPointer calldata);
// Array to hold predefined objects to display on Create/Move popup menu.
predefinedObject predefinedObjects[MAX_NUMBER_OF_PREDEFINED_OBJECTS];
void Populate_predefined_objects(predefinedObject *predefinedObjects);
int number_of_predefined_objects;
char predefined_object_definition_filename[256] = "predefined_SAR.sys";
int predefined_menu_from_file = 0;

int Area_object_enabled = 0;
int Map_View_object_enabled = 0;
int Area_type = 0;
char Area_color[3] = "/0";
int Area_bright = 0;
int Area_filled = 0;
int Signpost_object_enabled = 0;
int Probability_circles_enabled = 0;
int DF_object_enabled = 0;
int Omni_antenna_enabled = 0;
int Beam_antenna_enabled = 0;
char object_shgd[5] = "0000\0";
char object_NRQ[4] = "960\0";
XtPointer global_parameter1 = (XtPointer)NULL;
XtPointer global_parameter2 = (XtPointer)NULL;

int polygon_last_x = -1;        // Draw CAD Objects functions
int polygon_last_y = -1;        // Draw CAD Objects functions

int doing_move_operation = 0;

Widget draw_CAD_objects_dialog = (Widget)NULL;
Widget cad_dialog = (Widget)NULL;
Widget cad_label_data,
       cad_comment_data,
       cad_probability_data,
       cad_line_style_data;
// Values entered in the cad_dialog
int draw_CAD_objects_flag = 0;
void Draw_All_CAD_Objects(Widget w);
void Save_CAD_Objects_to_file(void);
Widget cad_erase_dialog;
Widget list_of_existing_CAD_objects = (Widget)NULL;
Widget cad_list_dialog = (Widget)NULL;
Widget list_of_existing_CAD_objects_edit = (Widget)NULL;
void Format_area_for_output(double *area_km2, char *area_description, int sizeof_area_description);
void Update_CAD_objects_list_dialog(void);
void CAD_object_set_raw_probability(CADRow *object_ptr, float probability, int as_percent);
int CAD_draw_objects = TRUE;
int CAD_show_label = TRUE;
int CAD_show_raw_probability = TRUE;
int CAD_show_comment = TRUE;
int CAD_show_area = TRUE;
void Draw_CAD_Objects_erase_dialog_close(Widget w, XtPointer clientData, XtPointer callData);
void Draw_CAD_Objects_list_dialog_close(Widget w, XtPointer clientData, XtPointer callData);
#ifndef USE_COMBO_BOX
  int clsd_value;  // replacement value for cad line type combo box
#endif // !USE_COMBO_BOX


////////////////////////////////////////////////////////////////////////////////////////////////////

// Init values for Objects dialog
char last_object[9+1];
char last_obj_grp;
char last_obj_sym;
char last_obj_overlay;
char last_obj_comment[34+1];



/////////////////////////////////////////////////////////////////////////



/*
 *  Check for a valid object name
 */
int valid_object(char *name)
{
  int len, i;

  // max 9 printable ASCII characters, case sensitive   [APRS
  // Reference]
  len = (int)strlen(name);
  if (len > 9 || len == 0)
  {
    return(0);  // wrong size
  }

  for (i=0; i<len; i++)
    if (!isprint((int)name[i]))
    {
      return(0);  // not printable
    }

  return(1);
}





/*
 *  Check for a valid item name (3-9 chars, any printable ASCII
 *  except '!' or '_')
 */
int valid_item(char *name)
{
  int len, i;

  // min 3, max 9 printable ASCII characters, case sensitive
  // [APRS Reference]
  len = (int)strlen(name);
  if (len > 9 || len < 3)
  {
    return(0);  // Wrong size
  }

  for (i=0; i<len; i++)
  {
    if (!isprint((int)name[i]))
    {
      return(0);                  // Not printable
    }
    if ( (name[i] == '!') || (name[i] == '_') )
    {
      return(0);                  // Contains '!' or '_'
    }
  }

  return(1);
}





/*
 *  Clear out object/item history log file
 */
void Object_History_Clear( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  char *file;
  FILE *f;
  char temp_file_path[MAX_VALUE];

  file = get_user_base_dir("config/object.log", temp_file_path, sizeof(temp_file_path));

  f=fopen(file,"w");
  if (f!=NULL)
  {
    (void)fclose(f);

    if (debug_level & 1)
    {
      fprintf(stderr,"Clearing Object/Item history file...\n");
    }
  }
  else
  {
    fprintf(stderr,"Couldn't open file for writing: %s\n", file);
  }
}





/*
 *  Re-read object/item history log file
 */
void Object_History_Refresh( Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{

  // Reload saved objects and items from previous runs.
  reload_object_item();
}





// We have a lot of code duplication between Setup_object_data,
// Setup_item_data, and Create_object_item_tx_string.
//
// Make sure to look at the "transmit_compressed_objects_items"
// variable
// to decide whether to send a compressed packet.
/*
 *  Create the transmit string for Objects/Items.
 *  Input is a DataRow struct, output is both an integer that says
 *  whether it completed successfully, and a char* that has the
 *  output tx string in it.
 *
 *  Returns 0 if there's a problem.
 */
int Create_object_item_tx_string(DataRow *p_station, char *line, int line_length)
{
  int i, done;
  char lat_str[MAX_LAT];
  char lon_str[MAX_LONG];
  char comment[43+1];                 // max 43 characters of comment
  char comment2[43+1];
  char time[7+1];
  struct tm *day_time;
  time_t sec;
  char complete_area_color[3];
  int complete_area_type;
  int lat_offset, lon_offset;
  char complete_corridor[6];
  char altitude[10];
  char speed_course[8];
  int speed;
  int course;
  int temp;
  long temp2;
  char signpost[6];
  int bearing;
  char tempstr[50];
  char object_group;
  char object_symbol;
  int killed = 0;


  (void)remove_trailing_spaces(p_station->call_sign);
  //(void)to_upper(p_station->call_sign);     Not per spec.  Don't
  //use this.

  if ((p_station->flag & ST_OBJECT) != 0)     // We have an object
  {
    if (!valid_object(p_station->call_sign))
    {
      line[0] = '\0';
      return(0);
    }
  }
  else if ((p_station->flag & ST_ITEM) != 0)      // We have an item
  {
    xastir_snprintf(tempstr,
                    sizeof(tempstr),
                    "%s",
                    p_station->call_sign);

    if (strlen(tempstr) == 1)   // Add two spaces (to make 3 minimum chars)
    {
      strcpy(p_station->call_sign, tempstr);
      p_station->call_sign[sizeof(p_station->call_sign)-1] = '\0';  // Terminate string
      strcat(p_station->call_sign, "  ");
      p_station->call_sign[sizeof(p_station->call_sign)-1] = '\0';  // Terminate string
    }
    else if (strlen(tempstr) == 2)   // Add one space (to make 3 minimum chars)
    {
      strcpy(p_station->call_sign, tempstr);
      p_station->call_sign[sizeof(p_station->call_sign)-1] = '\0';  // Terminate string
      strcat(p_station->call_sign, " ");
      p_station->call_sign[sizeof(p_station->call_sign)-1] = '\0';  // Terminate string
    }

    if (!valid_item(p_station->call_sign))
    {
      line[0] = '\0';
      return(0);
    }
  }
  else    // Not an item or an object, what are we doing here!
  {
    line[0] = '\0';
    return(0);
  }

  // Lat/lon are in Xastir coordinates, so we need to convert
  // them to APRS string format here.
  convert_lat_l2s(p_station->coord_lat, lat_str, sizeof(lat_str), CONVERT_LP_NOSP);
  convert_lon_l2s(p_station->coord_lon, lon_str, sizeof(lon_str), CONVERT_LP_NOSP);

  // Check for an overlay character.  Replace the group character
  // (table char) with the overlay if present.
  if (p_station->aprs_symbol.special_overlay != '\0')
  {
    // Overlay character found
    object_group = p_station->aprs_symbol.special_overlay;
    if ( (object_group >= '0' && object_group <= '9')
         || (object_group >= 'A' && object_group <= 'Z') )
    {
      // Valid overlay character, use what we have
    }
    else
    {
      // Bad overlay character, throw it away
      object_group = '\\';
    }
  }
  else      // No overlay character
  {
    object_group = p_station->aprs_symbol.aprs_type;
  }

  object_symbol = p_station->aprs_symbol.aprs_symbol;

  // In this case we grab only the first comment field (if it
  // exists) for the object/item
  if ( (p_station->comment_data != NULL)
       && (p_station->comment_data->text_ptr != NULL) )
  {
    xastir_snprintf(comment,
                    sizeof(comment),
                    "%s",
                    p_station->comment_data->text_ptr);
  }
  else
  {
    comment[0] = '\0';  // Empty string
  }

  if ( (p_station->probability_min[0] != '\0')
       || (p_station->probability_max[0] != '\0') )
  {

    if (p_station->probability_max[0] == '\0')
    {
      // Only have probability_min
      strcpy(comment2, "Pmin");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, p_station->probability_min);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, ",");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, comment);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
    }
    else if (p_station->probability_min[0] == '\0')
    {
      // Only have probability_max
      strcpy(comment2, "Pmax");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, p_station->probability_max);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, ",");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, comment);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
    }
    else    // Have both
    {
      strcpy(comment2, "Pmin");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, p_station->probability_min);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, ",Pmax");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, p_station->probability_max);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, ",");
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
      strcat(comment2, comment);
      comment2[sizeof(comment2)-1] = '\0';  // Terminate string
    }
    xastir_snprintf(comment,sizeof(comment), "%s", comment2);
  }


  // Put RNG or PHG at the beginning of the comment
  strcpy(comment2, p_station->power_gain);
  comment2[sizeof(comment2)-1] = '\0';  // Terminate string
  strcat(comment2, comment);
  comment2[sizeof(comment2)-1] = '\0';  // Terminate string

  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  comment2);


  (void)remove_trailing_spaces(comment);


  // This is for objects only, not items.  Uses current time but
  // should use the transmitted time from the DataRow struct.
  // Which time field in the struct would that be?  Have to find
  // out
  // from the extract_?? code.
  if ((p_station->flag & ST_OBJECT) != 0)
  {
    sec = sec_now();
    day_time = gmtime(&sec);
    xastir_snprintf(time,
                    sizeof(time),
                    "%02d%02d%02dz",
                    day_time->tm_mday,
                    day_time->tm_hour,
                    day_time->tm_min);
  }


// Handle Generic Options


  // Speed/Course Fields
  xastir_snprintf(speed_course, sizeof(speed_course), ".../"); // Start with invalid-data string
  course = 0;
  if (strlen(p_station->course) != 0)      // Course was entered
  {
    // Need to check for 1 to three digits only, and 001-360
    // degrees)
    temp = atoi(p_station->course);
    if ( (temp >= 1) && (temp <= 360) )
    {
      xastir_snprintf(speed_course, sizeof(speed_course), "%03d/",temp);
      course = temp;
    }
    else if (temp == 0)     // Spec says 001 to 360 degrees...
    {
      xastir_snprintf(speed_course, sizeof(speed_course), "360/");
    }
  }
  speed = 0;
  if (strlen(p_station->speed) != 0)   // Speed was entered (we only handle knots currently)
  {
    // Need to check for 1 to three digits, no alpha characters
    temp = atoi(p_station->speed);
    if ( (temp >= 0) && (temp <= 999) )
    {
      long x_long, y_lat;

      xastir_snprintf(tempstr, sizeof(tempstr), "%03d",temp);
      strncat(speed_course,
              tempstr,
              sizeof(speed_course) - 1 - strlen(speed_course));
      speed = temp;

      // Speed is non-zero.  Compute the current dead-reckoned
      // position and use that instead.
      compute_current_DR_position(p_station,
                                  &x_long,
                                  &y_lat);

      // Lat/lon are in Xastir coordinates, so we need to
      // convert them to APRS string format here.
      //
      convert_lat_l2s(y_lat,
                      lat_str,
                      sizeof(lat_str),
                      CONVERT_LP_NOSP);

      convert_lon_l2s(x_long,
                      lon_str,
                      sizeof(lon_str),
                      CONVERT_LP_NOSP);

//fprintf(stderr,"\t%s  %s\n", lat_str, lon_str);

    }
    else
    {
      strncat(speed_course,
              "...",
              sizeof(speed_course) - 1 - strlen(speed_course));
    }
  }
  else    // No speed entered, blank it out
  {
    strncat(speed_course,
            "...",
            sizeof(speed_course) - 1 - strlen(speed_course));
  }
  if ( (speed_course[0] == '.') && (speed_course[4] == '.') )
  {
    speed_course[0] = '\0'; // No speed or course entered, so blank it
  }
  if (p_station->aprs_symbol.area_object.type != AREA_NONE)    // It's an area object
  {
    speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
  }

  // Altitude Field
  altitude[0] = '\0'; // Start with empty string
  if (strlen(p_station->altitude) != 0)     // Altitude was entered (we only handle feet currently)
  {
    // Need to check for all digits, and 1 to 6 digits
    if (isdigit((int)p_station->altitude[0]))
    {
      // Must convert from meters to feet before transmitting
      temp2 = (int)( (atof(p_station->altitude) / 0.3048) + 0.5);
      if ( (temp2 >= 0) && (temp2 <= 99999l) )
      {
        char temp_alt[20];
        xastir_snprintf(temp_alt, sizeof(temp_alt), "/A=%06ld",temp2);
        memcpy(altitude, temp_alt, sizeof(altitude) - 1);
        altitude[sizeof(altitude)-1] = '\0';  // Terminate string
      }
    }
  }


// Handle Specific Options


  // Area Objects
  if (p_station->aprs_symbol.area_object.type != AREA_NONE)   // It's an area object
  {

    // Note that transmitted color consists of two characters,
    // from "/0" to "15"
    xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02d", p_station->aprs_symbol.area_object.color);
    if (complete_area_color[0] == '0')
    {
      complete_area_color[0] = '/';
    }

    complete_area_type = p_station->aprs_symbol.area_object.type;

    lat_offset = p_station->aprs_symbol.area_object.sqrt_lat_off;
    lon_offset = p_station->aprs_symbol.area_object.sqrt_lon_off;

    // Corridor
    complete_corridor[0] = '\0';
    if ( (complete_area_type == 1) || (complete_area_type == 6))
    {
      if (p_station->aprs_symbol.area_object.corridor_width > 0)
      {
        char temp_corridor[10];
        xastir_snprintf(temp_corridor, sizeof(temp_corridor), "{%d}",
                        p_station->aprs_symbol.area_object.corridor_width);
        memcpy(complete_corridor, temp_corridor, sizeof(complete_corridor) - 1);
        complete_corridor[sizeof(complete_corridor)-1] = '\0';  // Terminate string
      }
    }

    if ((p_station->flag & ST_OBJECT) != 0)     // It's an object
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ";%-9s*%s%s%1d%02d%2s%02d%s%s%s",
                        p_station->call_sign,
                        time,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        complete_area_type,
                        lat_offset,
                        complete_area_color,
                        lon_offset,
                        speed_course,
                        complete_corridor,
                        altitude);

      }
      else    // Non-compressed posit object
      {

        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%1d%02d%2s%02d%s%s%s",
                        p_station->call_sign,
                        time,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        complete_area_type,
                        lat_offset,
                        complete_area_color,
                        lon_offset,
                        speed_course,
                        complete_corridor,
                        altitude);
      }
    }
    else        // It's an item
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ")%s!%s%1d%02d%2s%02d%s%s%s",
                        p_station->call_sign,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        complete_area_type,
                        lat_offset,
                        complete_area_color,
                        lon_offset,
                        speed_course,
                        complete_corridor,
                        altitude);
      }
      else    // Non-compressed item
      {

        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%1d%02d%2s%02d%s%s%s",
                        p_station->call_sign,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        complete_area_type,
                        lat_offset,
                        complete_area_color,
                        lon_offset,
                        speed_course,
                        complete_corridor,
                        altitude);
      }
    }
  }

  else if ( (p_station->aprs_symbol.aprs_type == '\\') // We have a signpost object
            && (p_station->aprs_symbol.aprs_symbol == 'm' ) )
  {
    if (strlen(p_station->signpost) > 0)
    {
      char temp_sign[10];
      xastir_snprintf(temp_sign, sizeof(temp_sign), "{%s}", p_station->signpost);
      memcpy(signpost, temp_sign, sizeof(signpost));
      signpost[sizeof(signpost)-1] = '\0';  // Terminate string
    }
    else    // No signpost data entered, blank it out
    {
      signpost[0] = '\0';
    }
    if ((p_station->flag & ST_OBJECT) != 0)     // It's an object
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ";%-9s*%s%s%s%s",
                        p_station->call_sign,
                        time,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        altitude,
                        signpost);
      }
      else    // Non-compressed posit object
      {

        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s%s",
                        p_station->call_sign,
                        time,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        speed_course,
                        altitude,
                        signpost);
      }
    }
    else    // It's an item
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ")%s!%s%s%s",
                        p_station->call_sign,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        altitude,
                        signpost);
      }
      else    // Non-compressed item
      {

        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s%s",
                        p_station->call_sign,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        speed_course,
                        altitude,
                        signpost);
      }
    }
  }

  else if (p_station->signal_gain[0] != '\0')   // Must be an Omni-DF object/item
  {

    if ((p_station->flag & ST_OBJECT) != 0)     // It's an object
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ";%-9s*%s%s%s/%s%s",
                        p_station->call_sign,
                        time,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        p_station->signal_gain,
                        speed_course,
                        altitude);
      }
      else    // Non-compressed posit object
      {

        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%s%s",
                        p_station->call_sign,
                        time,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        p_station->signal_gain,
                        speed_course,
                        altitude);
      }
    }
    else    // It's an item
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ")%s!%s%s/%s%s",
                        p_station->call_sign,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        p_station->signal_gain,
                        speed_course,
                        altitude);
      }
      else    // Non-compressed item
      {

        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%s%s",
                        p_station->call_sign,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        p_station->signal_gain,
                        speed_course,
                        altitude);
      }
    }
  }
  else if (p_station->NRQ[0] != 0)    // It's a Beam Heading DFS object/item
  {

    if (strlen(speed_course) != 7)
      xastir_snprintf(speed_course,
                      sizeof(speed_course),
                      "000/000");

    bearing = atoi(p_station->bearing);
    if ( (bearing < 1) || (bearing > 360) )
    {
      bearing = 360;
    }

    if ((p_station->flag & ST_OBJECT) != 0)     // It's an object
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ";%-9s*%s%s/%03i/%s%s",
                        p_station->call_sign,
                        time,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        bearing,
                        p_station->NRQ,
                        altitude);
      }
      else    // Non-compressed posit object
      {

        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%03i/%s%s",
                        p_station->call_sign,
                        time,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        speed_course,
                        bearing,
                        p_station->NRQ,
                        altitude);
      }
    }
    else    // It's an item
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ")%s!%s/%03i/%s%s",
                        p_station->call_sign,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        bearing,
                        p_station->NRQ,
                        altitude);
      }
      else    // Non-compressed item
      {

        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%03i/%s%s",
                        p_station->call_sign,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        speed_course,
                        bearing,
                        p_station->NRQ,
                        altitude);
      }
    }
  }

  else    // Else it's a normal object/item
  {

    if ((p_station->flag & ST_OBJECT) != 0)     // It's an object
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ";%-9s*%s%s%s",
                        p_station->call_sign,
                        time,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        altitude);
      }
      else    // Non-compressed posit object
      {
        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s",
                        p_station->call_sign,
                        time,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        speed_course,
                        altitude);
      }
    }
    else    // It's an item
    {

      if (transmit_compressed_objects_items)
      {
        char temp_group = object_group;
        long x_long, y_lat;

        // If we have a numeric overlay, we need to convert
        // it to 'a-j' for compressed objects.
        if (temp_group >= '0' && temp_group <= '9')
        {
          temp_group = temp_group + 'a';
        }

        if (speed == 0)
        {
          x_long = p_station->coord_lon;
          y_lat  = p_station->coord_lat;
        }
        else
        {
          // Speed is non-zero.  Compute the current
          // dead-reckoned position and use that instead.
          compute_current_DR_position(p_station,
                                      &x_long,
                                      &y_lat);
        }

        // We need higher precision lat/lon strings than
        // those created above.
        convert_lat_l2s(y_lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
        convert_lon_l2s(x_long, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);

        xastir_snprintf(line, line_length, ")%s!%s%s",
                        p_station->call_sign,
                        compress_posit(lat_str,
                                       temp_group,
                                       lon_str,
                                       object_symbol,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank
                        altitude);
      }
      else    // Non-compressed item
      {
        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s",
                        p_station->call_sign,
                        lat_str,
                        object_group,
                        lon_str,
                        object_symbol,
                        speed_course,
                        altitude);
      }
    }
  }

  // If it's a "killed" object, change '*' to an '_'
  if ((p_station->flag & ST_OBJECT) != 0)                 // It's an object
  {
    if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE)     // It's been killed
    {
      line[10] = '_';
      killed++;
    }
  }
  // If it's a "killed" item, change '!' to an '_'
  else                                                    // It's an item
  {
    if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE)     // It's been killed
    {
      killed++;
      done = 0;
      i = 0;
      while ( (!done) && (i < 11) )
      {
        if (line[i] == '!')
        {
          line[i] = '_';          // mark as deleted object
          done++;                 // Exit from loop
        }
        i++;
      }
    }
  }

  // Check whether we need to stop transmitting particular killed
  // object/items now.
  if (killed)
  {
    // Check whether we should decrement the object_retransmit
    // counter so that we will eventually stop sending this
    // object/item.
    if (p_station->object_retransmit == 0)
    {
      // We shouldn't be transmitting this killed object/item
      // anymore.  We're already done transmitting it.

//fprintf(stderr, "Done transmitting this object: %s,  %d\n",
//p_station->call_sign,
//p_station->object_retransmit);

      return(0);
    }

    // Check whether the timeout has been set yet on this killed
    // object/item.  If not, change it from -1 (continuous
    // transmit of non-killed objects) to
    // MAX_KILLED_OBJECT_RETRANSMIT.
    if (p_station->object_retransmit <= -1)
    {

//fprintf(stderr, "Killed object %s, setting retries, %d -> %d\n",
//p_station->call_sign,
//p_station->object_retransmit,
//MAX_KILLED_OBJECT_RETRANSMIT - 1);

      if ((MAX_KILLED_OBJECT_RETRANSMIT - 1) < 0)
      {
        p_station->object_retransmit = 0;
        return(0);  // No retransmits desired
      }
      else
      {
        p_station->object_retransmit = MAX_KILLED_OBJECT_RETRANSMIT - 1;
      }
    }
    else
    {
      // Decrement the timeout if it is a positive number.
      if (p_station->object_retransmit > 0)
      {

//fprintf(stderr, "Killed object %s, decrementing retries, %d ->
//%d\n",
//p_station->call_sign,
//p_station->object_retransmit,
//p_station->object_retransmit - 1);

        p_station->object_retransmit--;
      }
    }
  }

  // We need to tack the comment on the end, but need to make
  // sure we don't go over the maximum length for an object/item.
  if (strlen(comment) != 0)
  {
    temp = 0;
    if ((p_station->flag & ST_OBJECT) != 0)
    {
      while ( (strlen(line) < 80) && (temp < (int)strlen(comment)) )
      {
        //fprintf(stderr,"temp: %d->%d\t%c\n", temp,
        //strlen(line), comment[temp]);
        line[strlen(line) + 1] = '\0';
        line[strlen(line)] = comment[temp++];
      }
    }
    else    // It's an item
    {
      while ( (strlen(line) < (64 + strlen(p_station->call_sign))) && (temp < (int)strlen(comment)) )
      {
        //fprintf(stderr,"temp: %d->%d\t%c\n", temp,
        //strlen(line), comment[temp]);
        line[strlen(line) + 1] = '\0';
        line[strlen(line)] = comment[temp++];
      }
    }
  }

  //fprintf(stderr,"line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for objects/items.

  return(1);
}





// check_and_transmit_objects_items
//
// This function checks the last_transmit_time for each
// locally-owned object/item.  If it has been at least the
// transmit_time_increment since the last transmit, the increment is
// doubled and the object/item transmitted.
//
// Killed objects/items are transmitted for
// MAX_KILLED_OBJECT_RETRANSMIT times and then transmitting of those
// objects ceases.
//
// This would be a good place to implement auto-expiration of
// objects that's been discussed on the mailing lists.
//
// This function depends on the local loopback that is in
// interface.c.  If we don't hear & decode our own packets, we won't
// have our own objects/items in our list.
//
// We need to check DataRow objects for ST_OBJECT or ST_ITEM types
// that were transmitted by our callsign & SSID.  We might also need
// to modify the remove_time() and check_station_remove functions in
// order not to delete our own objects/items too quickly.
//
// insert_time/remove_time/next_station_time/prev_station_time
//
// It would be nice if the create/modify object dialog and this
// routine went
// through the same functions to create the transmitted packets:
//      objects.c:Setup_object_data
//      objects.c:Setup_item_data
// Unfortunately those routines snag their data directly from the
// dialog.
// In order to make them use the same code we'd have to separate out
// the
// fetch-from-dialog code from the create-transmit-packet code.
//
// This is what aprsDOS does, from Bob's APRS.TXT file:  "a
// fundamental precept is that old data is less important than new
// data."  "Each new packet is transmitted immediately, then 20
// seconds later.  After every transmission, the period is doubled.
// After 20 minutes only six packets have been transmitted.  From
// then on the rate remains at 10 minutes times the number of
// digipeater hops you are using."
// Actually, talking to Bob, he's used a period of 15 seconds as his
// base unit.  We now do the same using the OBJECT_CHECK_RATE define
// to set the initial timing.

//
// Added these to database.h:DataRow struct:
// time_t last_transmit_time;          // Time we last transmitted
// an object/item.  Used to
//                                     // implement decaying
//                                     transmit time algorithm
// short transmit_time_increment;      // Seconds to add to transmit
// next time around.  Used
//                                     // to implement decaying
//                                     transmit time algorithm
//
// The earlier code here transmitted objects/items at a specified
// rate.  This can cause large transmissions every OBJECT_rate
// seconds, as all objects/items are transmitted at once.  With the
// new code, the objects/items may be spaced a bit from each other
// time-wise, plus they are transmitted less and less often with
// each transmission until they hit the max interval specified by
// the "Object/Item TX Interval" slider.  When they hit that max
// interval, they are transmitted at the constant interval until
// killed.  When they are killed, they are transmitted for
// MAX_KILLED_OBJECT_RETRANSMIT iterations using the decaying
// algorithm, then transmissions cease.
//
void check_and_transmit_objects_items(time_t time)
{
  DataRow *p_station;     // pointer to station data
  char line[256];
  int first = 1;  // Used to output debug message only once
  int increment;


  // Time to re-transmit objects/items?
  // Check every OBJECT_CHECK_RATE seconds - 20%.  No faster else
  // we'll be running through the station list too often and
  // wasting cycles.
  if (time < (last_object_check + (int)(4.0 * OBJECT_CHECK_RATE/5.0 + 1.0) ) )
  {
    return;
  }

//fprintf(stderr,"check_and_transmit_objects_items\n");

  // Set up timer for next go-around
  last_object_check = time;

  if (debug_level & 1)
  {
    fprintf(stderr,"Checking whether to retransmit any objects/items\n");
  }

// We could speed things up quite a bit here by either keeping a
// separate list of our own objects/items, or going through the list
// of stations by time instead of by name (If by time, only check
// backwards from the current time by the max transmit interval plus
// some increment.  Watch out for the user changing the slider).

  for (p_station = n_first; p_station != NULL; p_station = p_station->n_next)
  {

    //fprintf(stderr,"%s\t%s\n",p_station->call_sign,p_station->origin);

    // If station is owned by me (Exact match includes SSID)
//        if (is_my_call(p_station->origin,1)) {
    // and it's an object or item
    if ((p_station->flag & (ST_OBJECT|ST_ITEM)) && is_my_object_item(p_station))
    {

      long x_long_save, y_lat_save;

      // If dead-reckoning, we need to send out a new
      // position for this object instead of just
      // overwriting the old position, which will cause
      // the track to skip.  Here we save the old position
      // away so we can save it back to the record later.
      //
      x_long_save = p_station->coord_lon;
      y_lat_save = p_station->coord_lat;

      if (debug_level & 1)
      {
        fprintf(stderr,
                "Found a locally-owned object or item: %s\n",
                p_station->call_sign);
      }

      // Call the DR function to compute a new lat/long
      // and change the object's lat/long to match so that
      // we move the object along each time we transmit
      // it.
      //
// WE7U
// Here we should log the new position to file if it's not done
// automatically.
      //
      if (p_station->speed[0] != '\0')
      {
        long x_long, y_lat;

        compute_current_DR_position(p_station, &x_long, &y_lat);

        // Put the new position into the record
        // temporarily so that we can
        p_station->coord_lon = x_long;
        p_station->coord_lat = y_lat;
      }

      // Keep the timestamp current on my own
      // objects/items so they don't expire.
      p_station->sec_heard = sec_now();
      move_station_time(p_station,NULL);

// Implementing sped-up transmission of new objects, regular
// transmission of old objects (decaying algorithm).  We'll do this
// by keeping a last_transmit_time variable and a
// transmit_time_increment with each DataRow struct.  If the
// last_transmit_time is older than the transmit_time_increment, we
// transmit the object and double the increment variable, until we
// hit the OBJECT_rate limit for the increment.  This will make
// newer objects/items transmit more often, and will also space out
// the transmissions of old objects so they're not transmitted all
// at once in a group.  Each time a new object/item is created that
// is owned by us, it needs to have it's timer set to 20 (seconds).
// If an object/item is touched, it needs to again be set to 20
// seconds.
///////////////////////////////////

// Run through the station list.

// Transmit any objects/items that have equalled or gone past
// (last_transmit_time + transmit_time_increment).  Update the
// last_transmit_time to current time.
//
// Double the transmit_time_increment.  If it has gone beyond
// OBJECT_rate, set it to OBJECT_rate instead.
//
///////////////////////////////////


      // Check for the case where the timing slider has
      // been reduced and the expire time is too long.
      // Reset it to the current max expire time so that
      // it'll get transmitted more quickly.
      if (p_station->transmit_time_increment > OBJECT_rate)
      {
        p_station->transmit_time_increment = OBJECT_rate;
      }


      increment = p_station->transmit_time_increment;

      if ( ( p_station->last_transmit_time + increment) <= time )
      {
        // We should transmit this object/item as it has
        // hit its transmit interval.

        float randomize;
        int one_fifth_increment;
        int new_increment;


        if (first && !object_tx_disable)      // "Transmitting objects/items"
        {
          statusline(langcode("BBARSTA042"),1);
          first = 0;
        }

        // Set up the new doubling increment
        increment = increment * 2;
        if (increment > OBJECT_rate)
        {
          increment = OBJECT_rate;
        }

        // Randomize the distribution a bit, so that all
        // objects are not transmitted at the same time.
        // Allow the random number to vary over 20%
        // (one-fifth) of the newly computed increment.
        one_fifth_increment = (int)((increment / 5) + 0.5);
//fprintf(stderr,"one_fifth_increment: %d\n", one_fifth_increment);

        // Scale the random number from 0.0 to 1.0.
        // Must convert at least one of the numbers to a
        // float else randomize will be zero every time.
        randomize = rand() / (float)RAND_MAX;
//fprintf(stderr,"randomize: %f\n", randomize);

        // Scale it to the range we want (0% to 20% of
        // the interval)
        randomize = randomize * one_fifth_increment;
//fprintf(stderr,"scaled randomize: %f\n", randomize);

        // Subtract it from the increment, use
        // poor-man's rounding to turn the random number
        // into an int (so we get the full range).
        new_increment = increment - (int)(randomize + 0.5);
        p_station->transmit_time_increment = (short)new_increment;

//fprintf(stderr,"check_and_transmit_objects_items():Setting
//tx_increment to %d:%s\n",
//    new_increment,
//    p_station->call_sign);

        // Set the last transmit time into the object.
        // Keep this based off the time the object was
        // last created/modified/deleted, so that we
        // don't end up with a bunch of them transmitted
        // together.
        p_station->last_transmit_time = p_station->last_transmit_time + new_increment;

        // Here we need to re-assemble and re-transmit
        // the object or item
        // Check whether it is a "live" or "killed"
        // object and vary the
        // number of retransmits accordingly.  Actually
        // we should be able
        // to keep retransmitting "killed" objects until
        // they expire out of
        // our station queue with no problems.  If
        // someone wants to ressurect
        // the object we'll get new info into our struct
        // and this function will
        // ignore that object from then on, unless we
        // again snatch control of
        // the object.

        // if signpost, area object, df object, or
        // generic object
        // check p_station->APRS_Symbol->aprs_type:
        //   APRS_OBJECT
        //   APRS_ITEM
        //   APRS_DF (looks like I didn't use this one
        //   when I implemented DF objects)

        //   Whether area, df, signpost.
        // Check ->signpost for signpost data.  Check
        // ->df_color also.

        // call_sign, sec_heard, coord_lon, coord_lat,
        // packet_time, origin,
        // aprs_symbol, pos_time, altitude, speed,
        // course, bearing, NRQ,
        // power_gain, signal_gain, signpost,
        // station_time, station_time_type,
        // comments, df_color
        if (Create_object_item_tx_string(p_station, line, sizeof(line)) )
        {

          // Restore the original lat/long before we
          // transmit the (possibly) new position.
          //
          p_station->coord_lon = x_long_save;
          p_station->coord_lat = y_lat_save;

//fprintf(stderr,"Transmitting: %s\n",line);
          // Attempt to transmit the object/item again
          if (object_tx_disable || transmit_disable)      // Send to loopback only
          {
            output_my_data(line,-1,0,1,0,NULL); // Local loopback only, not igating
          }
          else   // Send to all active tx-enabled interfaces
          {
            output_my_data(line,-1,0,0,0,NULL); // Transmit/loopback object data, not igating
          }
        }
        else
        {
//fprintf(stderr,"Create_object_item_tx_string returned a 0\n");
          // Don't transmit it.
        }
      }
      else    // Not time to transmit it yet
      {
//fprintf(stderr,"Not time to TX yet:
//%s\t%s\t",p_station->call_sign,p_station->origin);
//fprintf(stderr, "%ld secs to go\n", p_station->last_transmit_time
//+ increment - time );
      }
    }
  }
//fprintf(stderr,"Exiting check_and_transmit_objects_items\n");
}



//////////////////// Draw CAD Objects Functions ////////////////////



//#define CAD_DEBUG

// Allocate a new vertice along the polygon.  If the vertice is very
// close to the first vertice, ask the operator if they wish to
// close the polygon.  If closing, ask for a raw probability?
//
// As each vertice is allocated, write it out to file?  We'd then
// need to edit the file and comment vertices out if we're deleting
// vertices in memory.  We could also write out an entire object
// when we select "Close Polygon".
//
void CAD_vertice_allocate(long latitude, long longitude)
{

#ifdef CAD_DEBUG
  fprintf(stderr,"Allocating a new vertice\n");
#endif

  // Check whether a line segment will cross another?

  // We use the CAD_list_head variable, as it will be pointing to
  // the top of the list, where the current object we're working
  // on will be placed.  Check whether that pointer is NULL
  // though, just in case.
  if (CAD_list_head)     // We have at least one object defined
  {
    VerticeRow *p_new;

    // Allocate area to hold the vertice
    p_new = (VerticeRow *)malloc(sizeof(VerticeRow));

    if (!p_new)
    {
      fprintf(stderr,"Couldn't allocate memory in CAD_vertice_allocate()\n");
      return;
    }

    p_new->latitude = latitude;
    p_new->longitude = longitude;

    // Link it in at the top of the vertice chain.
    p_new->next = CAD_list_head->start;
    CAD_list_head->start = p_new;
  }

  // Call redraw_symbols outside this function, as
  // verticies may be allocated both when loading lots of them from a file
  // and when the user is drawing objects in the user interface
  // Reload symbols/tracks/CAD objects
  //redraw_symbols(da);
}





// Allocate a struct for a new object and add one vertice to it.
// When do we name it and place the label?  Assign probability to
// it?  We should keep a pointer to the current polygon we're
// working on, so that we can modify it easily as we draw.
// Actually, it'll be pointed to by CAD_list_head, so we already
// have it!
//
// As each object is allocated, write it out to file?
//
// Compute a default label of date/time?
//
void CAD_object_allocate(long latitude, long longitude)
{
  CADRow *p_new;

#ifdef CAD_DEBUG
  fprintf(stderr,"Allocating a new CAD object\n");
#endif

  // Allocate memory and link it to the top of the singly-linked
  // list of CADRow objects.
  p_new = (CADRow *)malloc(sizeof(CADRow));

  if (!p_new)
  {
    fprintf(stderr,"Couldn't allocate memory in CAD_object_allocate()\n");
    return;
  }

  // Fill in default values
  p_new->creation_time = sec_now();
  p_new->start = NULL;
  p_new->line_color = colors[0x27];
  p_new->line_type = 2;  // LineOnOffDash;
  p_new->line_width = 4;
  p_new->computed_area = 0;
  CAD_object_set_raw_probability(p_new,0.0,FALSE);
  p_new->label_latitude = 0l;
  p_new->label_longitude = 0l;
  p_new->label[0] = '\0';
  p_new->comment[0] = '\0';

  // Allocate area to hold the first vertice

#ifdef CAD_DEBUG
  fprintf(stderr,"Allocating a new vertice\n");
#endif

  p_new->start = (VerticeRow *)malloc(sizeof(VerticeRow));
  if (!p_new->start)
  {
    fprintf(stderr,"Couldn't allocate memory in CAD_object_allocate(2)\n");
    free(p_new);
    return;
  }

  p_new->start->next = NULL;
  p_new->start->latitude = latitude;
  p_new->start->longitude = longitude;

  // Hook it into the linked list of objects
  p_new->next = CAD_list_head;
  CAD_list_head = p_new;

  /*
  //
  // Note:  It was too confusing to have these two dialogs close and
  // get redrawn when we click on the first vertice.  The net result
  // is that we may have two dialogs move on top of the drawing area
  // to the spot we're trying to draw.  Commented out this section due
  // to that.  We'll get the two dialogs updated when we click on
  // either the DONE or CANCEL button on the Close Polygon dialog.
  //
      // Here we update the erase cad objects dialog if it is up on
      // the screen.  We get rid of it and re-establish it, which will
      // usually make the dialog move, but this is better than having
      // it be out-of-date.
      //
      if (cad_erase_dialog != NULL) {
          Draw_CAD_Objects_erase_dialog_close(da, NULL, NULL);
          Draw_CAD_Objects_erase_dialog(da, NULL, NULL);
      }

      // Here we update the edit cad objects dialog by getting rid of
      // it and then re-establishing it if it is active when we start.
      // This will usually make the dialog move, but it's better than
      // having it be out-of-date.
      //
      if (cad_list_dialog!=NULL) {
          // Update the Edit CAD Objects list
          Draw_CAD_Objects_list_dialog_close(da, NULL, NULL);
          Draw_CAD_Objects_list_dialog(da, NULL, NULL);
      }
  */
}





// Delete all vertices associated with a CAD object and free the
// memory.  We really should pass a pointer to the object here
// instead of a vertice, and set the start pointer to NULL when
// done.
//
void CAD_vertice_delete_all(VerticeRow *v)
{
  VerticeRow *tmp;

  // Call CAD_vertice_delete() for each vertice, then unlink this
  // CAD object from the linked list and free its memory.

  // Iterate through each vertice, deleting as we go
  while (v != NULL)
  {
    tmp = v;
    v = v->next;
    free(tmp);

#ifdef CAD_DEBUG
    fprintf(stderr,"Free'ing a vertice\n");
#endif

  }
}





// Delete _all_ CAD objects and all associated vertices.  Loop
// through the entire list of CAD objects, calling
// CAD_vertice_delete_all() and then free'ing the CAD object.  When
// done, set the start pointer to NULL.
//
// We also need to wipe the persistent CAD object file.
//
void CAD_object_delete_all(void)
{
  CADRow *p = CAD_list_head;
  CADRow *tmp;

  while (p != NULL)
  {
    VerticeRow *v = p->start;

    // Remove all of the vertices
    if (v != NULL)
    {

      // Delete/free the vertices
      CAD_vertice_delete_all(v);
    }

    // Remove the object and free its memory
    tmp = p;
    p = p->next;
    free(tmp);

#ifdef CAD_DEBUG
    fprintf(stderr,"Free'ing an object\n");
#endif

  }

  // Zero the CAD linked list head
  CAD_list_head = NULL;
}





// Remove a vertice, thereby joining two segments into one?
//
// Recompute the raw probability if need be, or make it an invalid
// value so that we know we need to recompute it.
//
//void CAD_vertice_delete(CADrow *object) {
//    VerticeRow *v = object->start;

// Unlink the vertice from the linked list and free its memory.
// Allow removing a vertice in the middle or end of a chain.  If
// removing the vertice turns the polygon into an open polygon,
// alert the user of that fact and ask if they wish to close it.
//}





/* Test to see if a CAD object of the name (label) provided exists.
   Parameter: label, the label text to be checked.
   Returns 0 if no CAD object with a name matching the
   provided name is found.
   Returns 1 if a CAD object with a name matching the
   provided name is found.  */
int exists_CAD_object_by_label(char *label)
{
  CADRow *object_pointer = CAD_list_head;
  int result = 0;  // function return value
  int done = 0;    // flag to stop loop when a match is found
  while (object_pointer != NULL && done==0)
  {
    if (strcmp(object_pointer->label,label)==0)
    {
      // a matching name was found
      result = 1;
      done = 1;
    }
    object_pointer = object_pointer->next;
  }
  return result;
}





/* Counts to see how many CAD objects of the name (label) provided exist.
   Parameter: label, the label text to be checked.
   Returns 0 if no CAD object with a name matching the
   provided name is found.
   Returns count of the number of CAD objects with a matching label if
   one or more is found.  */
int count_CAD_object_with_matching_label(char *label)
{
  CADRow *object_pointer = CAD_list_head;
  int result = 0;
  while (object_pointer != NULL)
  {
    // iterate through all CAD objects
    if (strcmp(object_pointer->label,label)==0)
    {
      // a matching name was found
      result++;
      object_pointer = object_pointer->next;
    }
  }
  return result;
}





/* Delete one CAD object and all of its vertices. */
void CAD_object_delete(CADRow *object)
{
  CADRow *all_objects_ptr = CAD_list_head;
  CADRow *previous_object_ptr = CAD_list_head;
  VerticeRow *v = object->start;
  int done = 0;

#ifdef CAD_DEBUG
  fprintf(stderr,"Deleting CAD object %s\n",object->label);
#endif
  // check to see if the object we were given was the first object
  if (object==all_objects_ptr)
  {
#ifdef CAD_DEBUG
    fprintf(stderr,"Deleting first CAD object %s\n",object->label);
#endif
    CAD_vertice_delete_all(v); // Frees the memory also

    // Unlink the object from the chain and free the memory.
    CAD_list_head = object->next;  // Unlink
    free(object);   // Free the object memory
  }
  else
  {
#ifdef CAD_DEBUG
    fprintf(stderr,"Deleting other than first CAD object %s\n",object->label);
#endif
    // walk through the list and delete the object when found
    while (all_objects_ptr != NULL && done==0)
    {
      if (object==all_objects_ptr)
      {
        v = object->start;
        CAD_vertice_delete_all(v);
        previous_object_ptr->next = object->next;
        free(object);
        done = 1;
      }
      else
      {
        all_objects_ptr = all_objects_ptr->next;
      }
    }
  }
}





// Split an existing CAD object into two objects.  Can we trigger
// this by drawing a line across a closed polygon?
void CAD_object_split_existing(void)
{
}

// Join two existing polygons into one larger polygon.
void CAD_object_join_two(void)
{
}

// Move an entire CAD object, with all it's vertices, somewhere
// else.  Move the label along with it as well.
void CAD_object_move(void)
{
}





// Determine if a CAD object is a closed polygon.
//
// Takes a pointer to a CAD object as an argument.
// Returns 1 if the object is closed.
// Returns 0 if the object is not closed.
//
int is_CAD_object_open(CADRow *cad_object)
{
  VerticeRow *vertex_pointer;
  int vertex_count = 0;
  int result = 1;
  int atleast_one_different = 0;
  long start_lat, start_long;
  long stop_lat, stop_long;

  vertex_pointer = cad_object->start;
  if (vertex_pointer!=NULL)
  {
    // greater than zero points, get first point.
    start_lat = vertex_pointer->latitude;
    start_long = vertex_pointer->longitude;
    stop_lat = vertex_pointer->latitude;
    stop_long = vertex_pointer->longitude;
    vertex_pointer = vertex_pointer->next;
    while (vertex_pointer != NULL)
    {
      //greater than one point, get current point.
      stop_lat = vertex_pointer->latitude;
      stop_long = vertex_pointer->longitude;
      if (stop_lat!=start_lat || stop_long!=start_long)
      {
        atleast_one_different = 1;
      }
      vertex_pointer = vertex_pointer->next;
      vertex_count++;
    }
    if (vertex_count>2 && start_lat==stop_lat && start_long==stop_long && atleast_one_different > 0)
    {
      // more than two points, and they aren't in the same place
      result = 0;
    }
  }
  return result;
}





// Compute the area enclosed by a CAD object.  Check that it is a
// closed, non-intersecting polygon first.
//
double CAD_object_compute_area(CADRow *CAD_list_head)
{
  VerticeRow *tmp;
  double area;
  char temp_course[20];
  // Walk the linked list, computing the area of the
  // polygon.  Greene's Theorem is how we can compute the area of
  // a polygon using the vertices.  We could also compute whether
  // we're going clockwise or counter-clockwise around the polygon
  // using Greene's Theorem.  In fact I think we do that for
  // Shapefile hole polygons.  Remember that here we're walking
  // around the vertices backwards due to the ordering of the
  // list.  Shouldn't matter for our purposes though.
  //
  area = 0.0;
  tmp = CAD_list_head->start;
  if (is_CAD_object_open(CAD_list_head)==0)
  {
    // Only compute the area if CAD object is a closed polygon,
    // that is, not an open polygon.
    while (tmp->next != NULL)
    {
      double dx0, dy0, dx1, dy1;

      // Because lat/long units can vary drastically w.r.t. real
      // units, we need to multiply the terms by the real units in
      // order to get real area.

      // Compute real distances from a fixed point.  Convert to
      // the current measurement units.  We'll use the starting
      // vertice as our fixed point.
      //
      dx0 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              CAD_list_head->start->latitude,
              tmp->longitude,
              temp_course,
              sizeof(temp_course));

      if (tmp->longitude < CAD_list_head->start->longitude)
      {
        dx0 = -dx0;
      }

      dy0 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              tmp->latitude,
              CAD_list_head->start->longitude,
              temp_course,
              sizeof(temp_course));

      if (tmp->latitude < CAD_list_head->start->latitude)
      {
        dx0 = -dx0;
      }

      dx1 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              CAD_list_head->start->latitude,
              tmp->next->longitude,
              temp_course,
              sizeof(temp_course));

      if (tmp->next->longitude < CAD_list_head->start->longitude)
      {
        dx0 = -dx0;
      }

      dy1 = calc_distance_course(
              CAD_list_head->start->latitude,
              CAD_list_head->start->longitude,
              tmp->next->latitude,
              CAD_list_head->start->longitude,
              temp_course,
              sizeof(temp_course));

      // Add the minus signs back in, if any
      if (tmp->longitude < CAD_list_head->start->longitude)
      {
        dx0 = -dx0;
      }
      if (tmp->latitude < CAD_list_head->start->latitude)
      {
        dy0 = -dy0;
      }
      if (tmp->next->longitude < CAD_list_head->start->longitude)
      {
        dx1 = -dx1;
      }
      if (tmp->next->latitude < CAD_list_head->start->latitude)
      {
        dy1 = -dy1;
      }

      // Greene's Theorem:  Summation of the following, then
      // divide by two:
      //
      // A = X Y    - X   Y
      //  i   i i+1    i+1 i
      //
      area += (dx0 * dy1) - (dx1 * dy0);

      tmp = tmp->next;
    }
    area = 0.5 * area;
  }

  if (area < 0.0)
  {
    area = -area;
  }

//fprintf(stderr,"Square nautical miles: %f\n", area);

  return area;

}





// Allocate a label for an object, and place it according to the
// user's requests.  Keep track of where from the origin to place
// the label, font to use, color, etc.
void CAD_object_allocate_label(void)
{
}





// Set the probability for an object.  We should probably allocate
// the raw probability to small "buckets" within the closed polygon.
// This will allow us to split/join polygons later without messing
// up the probablity assigned to each area originally.  Check that
// it is a closed polygon first.
// if as_percent==TRUE, then probability is treated as a percent
// (expected to be a value between 0 and 100).
// otherwise, then probability is treated as a probability
// (expected to be a value between 0 and 1).
//
void CAD_object_set_raw_probability(CADRow *object_ptr, float probability, int as_percent)
{
  // initial implementation just assigns a single raw probability to the whole polygon.
  // internal storage is as a probability between 0 and 1
  // users will usually want to manipulate this as a percent (between 0 and 100)
  // thus the get and set functions are aware of both internal storage and
  // the user's request and return an appropriately scaled value.
  if (as_percent==TRUE)
  {
    // convert from a percent to a probability between 0 and 1
    object_ptr->raw_probability = (probability/100.00);
  }
  else
  {
    // treat as in internal storage form
    object_ptr->raw_probability = probability;
  }
}





// Get the raw probability for an object.  Sum up the raw
// probability "buckets" contained within the closed polygon.  Check
// that it _is_ a closed polygon first.
//
float CAD_object_get_raw_probability(CADRow *object_ptr, int as_percent)
{
  float result = 0.0;
  // not checking yet for closure
  if (object_ptr != NULL)
  {
    // initial implementation returns just the single raw probability
    result = object_ptr->raw_probability;
    if (as_percent > 0)
    {
      // raw probability is a probability between 0 an 1,
      // this may be desired as a percent.
      result = result * 100;
    }
  }
#ifdef CAD_DEBUG
  fprintf(stderr,"Getting Probability: %01.5f\n",result);
#endif
  return result;
}





void CAD_object_set_line_width(void)
{
}





void CAD_object_set_color(void)
{
}





void CAD_object_set_linetype(void)
{
}





// Used to break a line segment into two.  Can then move the vertice
// if needed.  Recompute the raw probability if need be, or make it
// an invalid value so that we know we need to recompute it.
void CAD_vertice_insert_new(void)
{
  // Check whether a line segment will cross another?
}





// Move an existing vertice.  Recompute the raw probability if need
// be, or make it an invalid value so that we know we need to
// recompute it.
void CAD_vertice_move(void)
{
  // Check whether a line segment will cross another?
}





// Set the location for drawing the label of an area to the center
// of the area.  Takes a pointer to a CAD object as a parameter.
// Sets the label_latitude and label_longitude attributes of the CAD
// object to the center of the region described by the vertices of
// the object.
//
void CAD_object_set_label_at_centroid(CADRow *CAD_object)
{
  // *** current implementation approximates the center as the
  // average of the largest and smallest of each of latitude
  // and longitude rather than correctly computing the centroid,
  // that is, it places the label at the centroid of a bounding
  // box for the area.  ***
  // We can't use a simple x=sum(x)/n, y=sum(y)/n as the
  // points on the outline shouldn't be weighted equally.
  // Ideal would be to place the label at the central point within
  // the area itslef, apparently this is a hard prbolem.
  // alternative would be to use the centroid, which like the
  // average of maximum and minimum values may lie outside of
  // the area.
  VerticeRow *vertex_pointer;
  long min_lat, min_long;
  long max_lat, max_long;
  // Walk the linked list and compute the centroid of the bounding box.
  vertex_pointer = CAD_object->start;
  min_lat = 0.0;
  min_long = 0.0;

  // Set the latitude and longitude of the label to the
  // centroid of the bounding box.
  // Start by setting lat and long of label to first point.
  CAD_object->label_latitude = vertex_pointer->latitude;
  CAD_object->label_longitude = vertex_pointer->longitude;
  if (vertex_pointer != NULL)
  {
    // Iterate through the vertices and calculate the center x and y position
    // based on an average of the largest and smallest latitudes and longitudes.
    min_lat = vertex_pointer->latitude;
    min_long = vertex_pointer->longitude;
    max_lat = vertex_pointer->latitude;
    max_long = vertex_pointer->longitude;
    while (vertex_pointer != NULL)
    {
      if (vertex_pointer->next != NULL)
      {
        if (vertex_pointer->longitude < min_long )
        {
          min_long = vertex_pointer->longitude;
        }
        if (vertex_pointer->latitude < min_lat )
        {
          min_lat = vertex_pointer->latitude;
        }
        if (vertex_pointer->longitude > max_long )
        {
          max_long = vertex_pointer->longitude;
        }
        if (vertex_pointer->latitude > max_lat )
        {
          max_lat = vertex_pointer->latitude;
        }
      }
      vertex_pointer = vertex_pointer->next;
    }
    CAD_object->label_latitude = (max_lat + min_lat)/2.0;
    CAD_object->label_longitude = (max_long + min_long)/2.0;
  }
}





// This is the callback for the CAD objects parameters dialog.  It
// takes the values entered in the dialog and stores them in the
// most recently created object.
//
void Set_CAD_object_parameters (Widget widget,
                                XtPointer clientData,
                                XtPointer calldata)
{

  float probability = 0.0;
  CADRow *target_object = NULL;
  int cb_selected;
  // need to find out object to edit from clientData rather than
  // using the first object in list as the one to edit.
  //target_object = CAD_list_head;
  target_object = (CADRow *)clientData;

  // set label, comment, and probability for area
  xastir_snprintf(target_object->label,
                  sizeof(target_object->label),
                  "%s", XmTextGetString(cad_label_data)
                 );
  xastir_snprintf(target_object->comment,
                  sizeof(target_object->comment),
                  "%s", XmTextGetString(cad_comment_data)
                 );
  // Is more error checking needed?  atof appears to correctly handle
  // empty input, reasonable probability values, and text (0.00).
  // User side probabilities are expressed as percent.
  probability = atof(XmTextGetString(cad_probability_data));
  CAD_object_set_raw_probability(target_object, probability, TRUE);

  // Use the selected line type, default is dashed
  cb_selected = FALSE;

#ifdef USE_COMBO_BOX
  XtVaGetValues(cad_line_style_data,
                XmNselectedPosition,
                &cb_selected,
                NULL);
#else
  cb_selected = clsd_value;
#endif // USE_COMBO_BOX        

  if (cb_selected)
  {
    target_object->line_type = cb_selected;
  }
  else
  {
    target_object->line_type = 2; // LineOnOffDash
  }

  if (cad_list_dialog)
  {
    Update_CAD_objects_list_dialog();
  }

  // close object_parameters dialog
  XtPopdown(cad_dialog);
  XtDestroyWidget(cad_dialog);
  cad_dialog = (Widget)NULL;

  Save_CAD_Objects_to_file();
  // Reload symbols/tracks/CAD objects so that object name will show on map.
  redraw_symbols(da);

  // Here we update the erase cad objects dialog if it is up on
  // the screen.  We get rid of it and re-establish it, which will
  // usually make the dialog move, but this is better than having
  // it be out-of-date.
  //
  if (cad_erase_dialog != NULL)
  {
    Draw_CAD_Objects_erase_dialog_close(widget,clientData,calldata);
    Draw_CAD_Objects_erase_dialog(widget,clientData,calldata);
  }

  // Here we update the edit cad objects dialog by getting rid of
  // it and then re-establishing it if it is active when we start.
  // This will usually make the dialog move, but it's better than
  // having it be out-of-date.
  //
  if (cad_list_dialog!=NULL)
  {
    // Update the Edit CAD Objects list
    Draw_CAD_Objects_list_dialog_close(widget, clientData, calldata);
    Draw_CAD_Objects_list_dialog(widget, clientData, calldata);
  }
}





// Update the list of existing CAD objects on the cad list dialog to
// reflect the current list of objects.
//
void Update_CAD_objects_list_dialog()
{
  CADRow *object_ptr = CAD_list_head;
  int counter = 1;
  XmString cb_item;

  if (list_of_existing_CAD_objects_edit!=NULL && cad_list_dialog)
  {
    XmListDeleteAllItems(list_of_existing_CAD_objects_edit);

    // iterate through list of objects to populate scrolled list
    while (object_ptr != NULL)
    {

      //  If no label, use the string "<Empty Label>" instead
      if (object_ptr->label[0] == '\0')
      {
        cb_item = XmStringCreateLtoR("<Empty Label>", XmFONTLIST_DEFAULT_TAG);
      }
      else
      {
        cb_item = XmStringCreateLtoR(object_ptr->label, XmFONTLIST_DEFAULT_TAG);
      }

      XmListAddItem(list_of_existing_CAD_objects_edit,
                    cb_item,
                    counter);
      counter++;
      XmStringFree(cb_item);
      object_ptr = object_ptr->next;
    }
  }
}





void close_object_params_dialog(Widget widget, XtPointer clientData, XtPointer calldata)
{
  XtPopdown(cad_dialog);
  XtDestroyWidget(cad_dialog);
  cad_dialog = (Widget)NULL;

  // Here we update the erase cad objects dialog if it is up on
  // the screen.  We get rid of it and re-establish it, which will
  // usually make the dialog move, but this is better than having
  // it be out-of-date.
  //
  if (cad_erase_dialog != NULL)
  {
    Draw_CAD_Objects_erase_dialog_close(widget,clientData,calldata);
    Draw_CAD_Objects_erase_dialog(widget,clientData,calldata);
  }

  // Here we update the edit cad objects dialog by getting rid of
  // it and then re-establishing it if it is active when we start.
  // This will usually make the dialog move, but it's better than
  // having it be out-of-date.
  //
  if (cad_list_dialog!=NULL)
  {
    // Update the Edit CAD Objects list
    Draw_CAD_Objects_list_dialog_close(widget, clientData, calldata);
    Draw_CAD_Objects_list_dialog(widget, clientData, calldata);
  }
}


#ifndef USE_COMBO_BOX
void clsd_menuCallback(Widget widget, XtPointer ptr, XtPointer callData)
{
  //XmPushButtonCallbackStruct *data = (XmPushButtonCallbackStruct *)callData;
  XtPointer userData;

  XtVaGetValues(widget, XmNuserData, &userData, NULL);

  //clsd_menu is zero based, cad_line_style_data constants are one based.
  clsd_value = (int)userData + 1;
  if (debug_level & 1)
  {
    fprintf(stderr,"Selected value on cad line type pulldown: %d\n",clsd_value);
  }
}
#endif  // !USE_COMBO_BOX



// Create a dialog to obtain information about a newly created CAD
// object from the user.  Values of probability, name, and comment
// are initially blank.  Takes as a parameter a string describing
// the area of the object.  There is a single button with a callback
// to Set_CAD_object_parameters, which stores values from the dialog
// in the object's struct.  Should be generalized to allow editing
// of a pre-existing CAD object (except for the name).  Parameter
// should be a pointer to the object.
//
void Set_CAD_object_parameters_dialog(char *area_description, CADRow *CAD_object)
{
  Widget cad_pane, cad_form,
         cad_label,
         cad_comment,
         cad_probability,
         cad_line_style,
         button_done,
         button_cancel;
  char   probability_string[5];
  int i;  // loop counters
  //XmString cb_item;  // used to create picklist of line styles
  XmString cb_items[3];
#ifndef USE_COMBO_BOX
  Widget clsd_menuPane;
  Widget clsd_button;
  Widget clsd_buttons[3];
  Widget clsd_menu;
  char buf[18];
  int x;
  Arg args[12]; // available for XtSetArguments
#endif // !USE_COMBO_BOX
  Widget clsd_widget;


  if (cad_dialog)
  {
    (void)XRaiseWindow(XtDisplay(cad_dialog), XtWindow(cad_dialog));
  }
  else
  {

    // Area Object"
    cad_dialog = XtVaCreatePopupShell(langcode("CADPUD001"),
                                      xmDialogShellWidgetClass,
                                      appshell,
                                      XmNdeleteResponse,          XmDESTROY,
                                      XmNdefaultPosition,         FALSE,
                                      XmNfontList, fontlist1,
                                      NULL);

    cad_pane = XtVaCreateWidget("Set_Del_Object pane",
                                xmPanedWindowWidgetClass,
                                cad_dialog,
                                MY_FOREGROUND_COLOR,
                                MY_BACKGROUND_COLOR,
                                NULL);

    cad_form =  XtVaCreateWidget("Set_Del_Object ob_form",
                                 xmFormWidgetClass,
                                 cad_pane,
                                 XmNfractionBase,            2,
                                 XmNautoUnmanage,            FALSE,
                                 XmNshadowThickness,         1,
                                 MY_FOREGROUND_COLOR,
                                 MY_BACKGROUND_COLOR,
                                 NULL);
    // Area of polygon, already scaled and internationalized.
    cad_label = XtVaCreateManagedWidget(area_description,
                                        xmLabelWidgetClass,
                                        cad_form,
                                        XmNtopAttachment,           XmATTACH_FORM,
                                        XmNtopOffset,               10,
                                        XmNbottomAttachment,        XmATTACH_NONE,
                                        XmNleftAttachment,          XmATTACH_FORM,
                                        XmNleftOffset,              10,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);
    // "Area Label:"
    cad_label = XtVaCreateManagedWidget(langcode("CADPUD002"),
                                        xmLabelWidgetClass,
                                        cad_form,
                                        XmNtopAttachment,           XmATTACH_FORM,
                                        XmNtopOffset,               50,
                                        XmNbottomAttachment,        XmATTACH_NONE,
                                        XmNleftAttachment,          XmATTACH_FORM,
                                        XmNleftOffset,              10,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);
    // label text field
    cad_label_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                     xmTextFieldWidgetClass,
                     cad_form,
                     XmNeditable,                TRUE,
                     XmNcursorPositionVisible,   TRUE,
                     XmNsensitive,               TRUE,
                     XmNshadowThickness,         1,
                     XmNcolumns,                 20,
                     XmNmaxLength,               CAD_LABEL_MAX_SIZE - 1,
                     XmNtopAttachment,           XmATTACH_FORM,
                     XmNtopOffset,               50,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_WIDGET,
                     XmNleftWidget,              cad_label,
                     XmNrightAttachment,         XmATTACH_NONE,
                     XmNbackground,              colors[0x0f],
                     XmNfontList, fontlist1,
                     NULL);
    // "Comment"
    cad_comment = XtVaCreateManagedWidget(langcode("CADPUD003"),
                                          xmLabelWidgetClass,
                                          cad_form,
                                          XmNtopAttachment,           XmATTACH_FORM,
                                          XmNtopOffset,               90,
                                          XmNbottomAttachment,        XmATTACH_NONE,
                                          XmNleftAttachment,          XmATTACH_FORM,
                                          XmNleftOffset,              10,
                                          XmNrightAttachment,         XmATTACH_NONE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);
    // comment text field
    cad_comment_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                       xmTextFieldWidgetClass,
                       cad_form,
                       XmNeditable,                TRUE,
                       XmNcursorPositionVisible,   TRUE,
                       XmNsensitive,               TRUE,
                       XmNshadowThickness,         1,
                       XmNcolumns,                 40,
                       XmNmaxLength,               CAD_COMMENT_MAX_SIZE - 1,
                       XmNtopAttachment,           XmATTACH_FORM,
                       XmNtopOffset,               90,
                       XmNbottomAttachment,        XmATTACH_NONE,
                       XmNleftAttachment,          XmATTACH_WIDGET,
                       XmNleftWidget,              cad_comment,
                       XmNrightAttachment,         XmATTACH_NONE,
                       XmNbackground,              colors[0x0f],
                       XmNfontList, fontlist1,
                       NULL);
    // "Probability (as %)"
    cad_probability = XtVaCreateManagedWidget(langcode("CADPUD004"),
                      xmLabelWidgetClass,
                      cad_form,
                      XmNtopAttachment,           XmATTACH_FORM,
                      XmNtopOffset,               130,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNleftAttachment,          XmATTACH_FORM,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);
    // probability field
    cad_probability_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                           xmTextFieldWidgetClass,
                           cad_form,
                           XmNeditable,                TRUE,
                           XmNcursorPositionVisible,   TRUE,
                           XmNsensitive,               TRUE,
                           XmNshadowThickness,         1,
                           XmNcolumns,                 5,
                           XmNmaxLength,               5,
                           XmNtopAttachment,           XmATTACH_FORM,
                           XmNtopOffset,               130,
                           XmNbottomAttachment,        XmATTACH_NONE,
                           XmNleftAttachment,          XmATTACH_WIDGET,
                           XmNleftWidget,              cad_probability,
                           XmNrightAttachment,         XmATTACH_NONE,
                           XmNbackground,              colors[0x0f],
                           XmNfontList, fontlist1,
                           NULL);
    // Boundary Line Type
    cad_line_style = XtVaCreateManagedWidget("Line Type:",
                     xmLabelWidgetClass,
                     cad_form,
                     XmNtopAttachment,           XmATTACH_WIDGET,
                     XmNtopWidget,               cad_probability_data,
                     XmNtopOffset,               5,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_FORM,
                     XmNleftOffset,              10,
                     XmNrightAttachment,         XmATTACH_NONE,
                     MY_FOREGROUND_COLOR,
                     MY_BACKGROUND_COLOR,
                     XmNfontList, fontlist1,
                     NULL);

    // lesstif as of 0.95 in 2008 doesn't fully support combo boxes
    //
    // Need to replace combo boxes with a pull down menu when lesstif is used.
    // See xpdf's  XPDFViewer.cc/XPDFViewer.h for an example.
    //cb_items = (XmString *) XtMalloc ( sizeof (XmString) * 4 );
    // Solid
    cb_items[0] = XmStringCreateLtoR( langcode("CADPUD012"), XmFONTLIST_DEFAULT_TAG);
    // Dashed
    cb_items[1] = XmStringCreateLtoR( langcode("CADPUD013"), XmFONTLIST_DEFAULT_TAG);
    // Double Dash
    cb_items[2] = XmStringCreateLtoR( langcode("CADPUD014"), XmFONTLIST_DEFAULT_TAG);

    clsd_widget = cad_line_style_data;

#ifdef USE_COMBO_BOX
    // Combo box to pick line style
    cad_line_style_data = XtVaCreateManagedWidget("select line style",
                          xmComboBoxWidgetClass,
                          cad_form,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               cad_probability_data,
                          XmNtopOffset,               5,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              cad_line_style,
                          XmNleftOffset,              10,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNnavigationType,          XmTAB_GROUP,
                          XmNcomboBoxType,            XmDROP_DOWN_LIST,
                          XmNpositionMode,            XmONE_BASED,
                          XmNvisibleItemCount,        3,
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          XmNfontList, fontlist1,
                          NULL);
    XmComboBoxAddItem(cad_line_style_data,cb_items[0],1,1);
    XmComboBoxAddItem(cad_line_style_data,cb_items[1],2,1);
    XmComboBoxAddItem(cad_line_style_data,cb_items[2],3,1);

    clsd_widget = cad_line_style_data;
#else
    // menu replacement for combo box when using lesstif
    x = 0;
    XtSetArg(args[x], XmNmarginWidth, 0);
    ++x;
    XtSetArg(args[x], XmNmarginHeight, 0);
    ++x;
    XtSetArg(args[x], XmNfontList, fontlist1);
    ++x;
    clsd_menuPane = XmCreatePulldownMenu(cad_form,"sddd_menuPane", args, x);
    //sddd_menu is zero based, constants for database types are one based.
    //sddd_value is set to match constants in callback.
    for (i=0; i<3; i++)
    {
      x = 0;
      XtSetArg(args[x], XmNlabelString, cb_items[i]);
      x++;
      XtSetArg(args[x], XmNuserData, (XtPointer)i);
      x++;
      XtSetArg(args[x], XmNfontList, fontlist1);
      ++x;
      sprintf(buf,"button%d",i);
      clsd_button = XmCreatePushButton(clsd_menuPane, buf, args, x);
      XtManageChild(clsd_button);
      XtAddCallback(clsd_button, XmNactivateCallback, clsd_menuCallback, Set_CAD_object_parameters_dialog);
      clsd_buttons[i] = clsd_button;
    }
    x = 0;
    XtSetArg(args[x], XmNleftAttachment, XmATTACH_WIDGET);
    ++x;
    XtSetArg(args[x], XmNleftWidget, cad_line_style);
    ++x;
    XtSetArg(args[x], XmNtopAttachment, XmATTACH_WIDGET);
    ++x;
    XtSetArg(args[x], XmNtopWidget, cad_probability_data);
    ++x;
    XtSetArg(args[x], XmNmarginWidth, 0);
    ++x;
    XtSetArg(args[x], XmNmarginHeight, 0);
    ++x;
    XtSetArg(args[x], XmNtopOffset, 5);
    ++x;
    XtSetArg(args[x], XmNleftOffset, 10);
    ++x;
    XtSetArg(args[x], XmNsubMenuId, clsd_menuPane);
    ++x;
    XtSetArg(args[x], XmNfontList, fontlist1);
    ++x;
    clsd_menu = XmCreateOptionMenu(cad_form, "sddd_Menu", args, x);
    XtManageChild(clsd_menu);
    clsd_value = 2;   // set a default value (line on off dash)
    clsd_widget = clsd_menu;
#endif  // USE_COMBO_BOX
    // free up space from combo box strings
    for (i=0; i<3; i++)
    {
      XmStringFree(cb_items[i]);
    }


    // "OK"
    button_done = XtVaCreateManagedWidget(langcode("CADPUD005"),
                                          xmPushButtonGadgetClass,
                                          cad_form,
                                          XmNtopAttachment,     XmATTACH_WIDGET,
                                          XmNtopWidget,         clsd_widget,
                                          XmNtopOffset,         5,
                                          XmNbottomAttachment,  XmATTACH_FORM,
                                          XmNbottomOffset,      5,
                                          XmNleftAttachment,    XmATTACH_POSITION,
                                          XmNleftPosition,      0,
                                          XmNrightAttachment,   XmATTACH_POSITION,
                                          XmNrightPosition,     1,
                                          XmNnavigationType,    XmTAB_GROUP,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

    // "Cancel"
    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),
                                            xmPushButtonGadgetClass,
                                            cad_form,
                                            XmNtopAttachment,     XmATTACH_WIDGET,
                                            XmNtopWidget,         clsd_widget,
                                            XmNtopOffset,         5,
                                            XmNbottomAttachment,  XmATTACH_FORM,
                                            XmNbottomOffset,      5,
                                            XmNleftAttachment,    XmATTACH_POSITION,
                                            XmNleftPosition,      1,
                                            XmNrightAttachment,   XmATTACH_POSITION,
                                            XmNrightPosition,     2,
                                            XmNnavigationType,    XmTAB_GROUP,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);


    // callback depends on whether this is a new or old object
    //XtAddCallback(button_done, XmNactivateCallback, Set_CAD_object_parameters, Set_CAD_object_parameters_dialog);

    if (CAD_object!=NULL)
    {
      XtAddCallback(button_done, XmNactivateCallback, Set_CAD_object_parameters, (XtPointer *)CAD_object);
    }
    else
    {
      // called to get information for a newly created cad object
      // pass pointer to the head of the list, which contains
      // the most recently created cad object.
      XtAddCallback(button_done, XmNactivateCallback, Set_CAD_object_parameters, CAD_list_head);
    }

    XtAddCallback(button_cancel, XmNactivateCallback, close_object_params_dialog, NULL);

    pos_dialog(cad_dialog);
    XmInternAtom(XtDisplay(cad_dialog),"WM_DELETE_WINDOW", FALSE);

    XtManageChild(cad_form);
    XtManageChild(cad_pane);

    XtPopup(cad_dialog,XtGrabNone);
  } // end if ! caddialog

  if (CAD_object!=NULL)
  {
    XmString tempSelection;

    // given an existing object, fill form with its information
    XmTextFieldSetString(cad_label_data,CAD_object->label);
    XmTextFieldSetString(cad_comment_data,CAD_object->comment);
    xastir_snprintf(probability_string,
                    sizeof(probability_string),
                    "%01.2f",
                    CAD_object_get_raw_probability(CAD_object,1));
    XmTextFieldSetString(cad_probability_data,probability_string);

    switch(CAD_object->line_type)
    {

      case 1: // Solid
#ifndef USE_COMBO_BOX
        i = 0;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD012"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;

      case 2: // Dashed
#ifndef USE_COMBO_BOX
        i = 1;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD013"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;

      case 3: // Double Dash
#ifndef USE_COMBO_BOX
        i = 2;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD014"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;

      default:
#ifndef USE_COMBO_BOX
        i = 1;
#endif // !USE_COMBO_BOX
        tempSelection = XmStringCreateLtoR( langcode("CADPUD013"),
                                            XmFONTLIST_DEFAULT_TAG);
        break;
    }
#ifdef USE_COMBO_BOX
    XmComboBoxSelectItem(cad_line_style_data, tempSelection);
#else
    clsd_value = i+1;
    //clsd_menu is zero based, line types are one based.
    //clsd_value matches line types (1-3).
    XtVaSetValues(clsd_menu, XmNmenuHistory, clsd_buttons[i], NULL);
#endif // USE_COMBO_BOX
    XmStringFree(tempSelection);
  }
}





static Cursor cs_CAD = (Cursor)NULL;

void free_cs_CAD(void)
{
  XFreeCursor(XtDisplay(da), cs_CAD);
  cs_CAD = (Cursor)NULL;
}

// This is the callback for the Draw togglebutton
//
void Draw_CAD_Objects_mode( Widget UNUSED(widget),
                            XtPointer UNUSED(clientData),
                            XtPointer callData)
{

  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;


  if(state->set)
  {
    draw_CAD_objects_flag = 1;

    // Create the "pencil" cursor so we know what mode we're in.
    //
    if(!cs_CAD)
    {
      cs_CAD=XCreateFontCursor(XtDisplay(da),XC_pencil);
      atexit(free_cs_CAD);
    }

    // enable the close polygon button on an open CAD menu
    if (CAD_close_polygon_menu_item)
    {
      XtSetSensitive(CAD_close_polygon_menu_item,TRUE);
    }

    (void)XDefineCursor(XtDisplay(da),XtWindow(da),cs_CAD);
    (void)XFlush(XtDisplay(da));

    draw_CAD_objects_flag = 1;
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position
  }
  else
  {
    draw_CAD_objects_flag = 0;
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position

    Save_CAD_Objects_to_file();

    // Remove the special "pencil" cursor.
    (void)XUndefineCursor(XtDisplay(da),XtWindow(da));
    (void)XFlush(XtDisplay(da));

    // disable the close polygon button on an open CAD menu.
    if (CAD_close_polygon_menu_item)
    {
      XtSetSensitive(CAD_close_polygon_menu_item,FALSE);
    }
  }
}





// Called when we complete a new CAD object.  Save the object to
// disk so that we can recover in the case of a crash or power
// failure.  Save any old file to a backup file.  Perhaps write them
// to numbered backup files so that we keep several on-hand?
//
void Save_CAD_Objects_to_file(void)
{
  FILE *f;
  char *file;
  CADRow *object_ptr = CAD_list_head;
  char temp_file_path[MAX_VALUE];

  fprintf(stderr,"Saving CAD objects to file\n");

  // Save in ~/.xastir/config/CAD_object.log
  file = get_user_base_dir("config/CAD_object.log", temp_file_path, sizeof(temp_file_path));
  f = fopen(file,"w+");

  if (f == NULL)
  {
    fprintf(stderr,
            "Couldn't open config/CAD_object.log file for writing!\n");
    return;
  }

  while (object_ptr != NULL)
  {
    VerticeRow *vertice = object_ptr->start;

    // Write out the main object info:
    fprintf(f,"\nCAD_Object\n");
    fprintf(f,"creation_time:   %lu\n",(unsigned long)object_ptr->creation_time);
    fprintf(f,"line_color:      %d\n",object_ptr->line_color);
    fprintf(f,"line_type:       %d\n",object_ptr->line_type);
    fprintf(f,"line_width:      %d\n",object_ptr->line_width);
    fprintf(f,"computed_area:   %f\n",object_ptr->computed_area);
    fprintf(f,"raw_probability: %f\n",CAD_object_get_raw_probability(object_ptr,TRUE));
    fprintf(f,"label_latitude:  %lu\n",object_ptr->label_latitude);
    fprintf(f,"label_longitude: %lu\n",object_ptr->label_longitude);
    fprintf(f,"label: %s\n",object_ptr->label);
    if (strlen(object_ptr->comment)>1)
    {
      fprintf(f,"comment: %s\n",object_ptr->comment);
    }
    else
    {
      fprintf(f,"comment: NULL\n");
    }

    // Iterate through the vertices:
    while (vertice != NULL)
    {

      fprintf(f,"Vertice: %lu %lu\n",
              vertice->latitude,
              vertice->longitude);

      vertice = vertice->next;
    }
    object_ptr = object_ptr->next;
  }
  (void)fclose(f);
}





// Called by main() when we start Xastir.  Restores CAD objects
// created in earlier Xastir sessions.
//
void Restore_CAD_Objects_from_file(void)
{
  FILE *f;
  char *file;
  char line[MAX_FILENAME];
  char temp_file_path[MAX_VALUE];
#ifdef CAD_DEBUG
  fprintf(stderr,"Restoring CAD objects from file\n");
#endif

  // Restore from ~/.xastir/config/CAD_object.log
  file = get_user_base_dir("config/CAD_object.log", temp_file_path, sizeof(temp_file_path));
  f = fopen(file,"r");

  if (f == NULL)
  {
#ifdef CAD_DEBUG
    fprintf(stderr,
            "Couldn't open config/CAD_object.log file for reading!\n");
#endif
    return;
  }

  while (!feof (f))
  {
    (void)get_line(f, line, MAX_FILENAME);
    if (strncasecmp(line,"CAD_Object",10) == 0)
    {
      // Found a new CAD Object declaration!

      //fprintf(stderr,"Found CAD_Object\n");

      // Malloc a new object, add it to the linked list, start
      // filling in the fields.
      //
      // This gives us a default object with one vertice.  We
      // can replace all of the fields in it as we parse them.
      CAD_object_allocate(0l, 0l);

      // Remove the one vertice from the newly allocated
      // object so that we don't end up with one too many
      // vertices when all done.
      CAD_vertice_delete_all(CAD_list_head->start);
      CAD_list_head->start = NULL;

    }
    else if (strncasecmp(line,"creation_time:",14) == 0)
    {
      //fprintf(stderr,"Found creation_time:\n");
      unsigned long temp_time;
      if (1 != sscanf(line+15, "%lu",&temp_time))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [creation_time]\n");
      }
      CAD_list_head->creation_time=(time_t)temp_time;
    }
    else if (strncasecmp(line,"line_color:",11) == 0)
    {
      //fprintf(stderr,"Found line_color:\n");
      if (1 != sscanf(line+12,"%d",
                      &CAD_list_head->line_color))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [line_color]\n");
      }
    }
    else if (strncasecmp(line,"line_type:",10) == 0)
    {
      //fprintf(stderr,"Found line_type:\n");
      if (1 != sscanf(line+11,"%d",
                      &CAD_list_head->line_type))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [line_type]\n");
      }
    }
    else if (strncasecmp(line,"line_width:",11) == 0)
    {
      //fprintf(stderr,"Found line_width:\n");
      if (1 != sscanf(line+12,"%d",
                      &CAD_list_head->line_width))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [line_width]\n");
      }
    }
    else if (strncasecmp(line,"computed_area:",14) == 0)
    {
      //fprintf(stderr,"Found computed_area:\n");
      if (1 != sscanf(line+15,"%f",
                      &CAD_list_head->computed_area))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [computed_area]\n");
      }
    }
    else if (strncasecmp(line,"raw_probability:",16) == 0)
    {
      //fprintf(stderr,"Found raw_probability:\n");
      if (1 != sscanf(line+17,"%f",
                      &CAD_list_head->raw_probability))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [raw_probability]\n");
      }
      else
      {
        // External storage is as percent, need to make sure that this
        // fits the expected internal storage format.
        // Thus take given value and store using method that knows
        // how to handle percents
        CAD_object_set_raw_probability(CAD_list_head,CAD_list_head->raw_probability,TRUE);
      }
    }
    else if (strncasecmp(line,"label_latitude:",15) == 0)
    {
      //fprintf(stderr,"Found label_latitude:\n");
      if (1 != sscanf(line+16,"%lu",
                      (unsigned long *)&CAD_list_head->label_latitude))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [label_latitude]\n");
      }
    }
    else if (strncasecmp(line,"label_longitude:",16) == 0)
    {
      //fprintf(stderr,"Found label_longitude:\n");
      if (1 != sscanf(line+17,"%lu",
                      (unsigned long *)&CAD_list_head->label_longitude))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [label_longitude]\n");
      }
    }
    else if (strncasecmp(line,"label:",6) == 0)
    {
      //fprintf(stderr,"Found label:\n");
      xastir_snprintf(CAD_list_head->label,
                      sizeof(CAD_list_head->label),
                      "%s",
                      line+7);
    }
    else if (strncasecmp(line,"comment:",8) == 0)
    {
      //fprintf(stderr,"Found comment:\n");
      xastir_snprintf(CAD_list_head->comment,
                      sizeof(CAD_list_head->comment),
                      "%s",
                      line+9);

      if (strcmp(CAD_list_head->comment,"NULL")==0)
      {
        xastir_snprintf(CAD_list_head->comment,
                        sizeof(CAD_list_head->comment),
                        "%c",
                        '\0'
                       );
      }
    }
    else if (strncasecmp(line,"Vertice:",8) == 0)
    {
      long latitude, longitude;

      //fprintf(stderr,"Found Vertice:\n");
      if (2 != sscanf(line+9,"%lu %lu",
                      (unsigned long *)&latitude,
                      (unsigned long *)&longitude))
      {
        fprintf(stderr,"Restore_CAD_Objects_from_file:sscanf parsing error [vertex]\n");
      }
      CAD_vertice_allocate(latitude,longitude);
    }
    else
    {
      // Else not recognized, do nothing with it!
      //fprintf(stderr,"Found unrecognized line\n");
    }
  }
  (void)fclose(f);
  // Reload symbols/tracks/CAD objects to draw the loaded objects
  redraw_symbols(da);
}





// popdown and destroy the cad_erase_dialog.
//
void Draw_CAD_Objects_erase_dialog_close ( Widget UNUSED(w),
    XtPointer UNUSED(clientData),
    XtPointer UNUSED(callData) )
{

  if (cad_erase_dialog!=NULL)
  {
    // close cad_erase_dialog
    XtPopdown(cad_erase_dialog);
    XtDestroyWidget(cad_erase_dialog);
    cad_erase_dialog = (Widget)NULL;
  }

}





// Call back for delete selected button on
// Draw_CAD_Objects_erase_dialog.  Iterates through the list of
// selected CAD objects and deletes them.
//
void Draw_CAD_Objects_erase_selected ( Widget w,
                                       XtPointer clientData,
                                       XtPointer callData)
{
  int itemCount;       // number of items in list of CAD objects.
  XmString *listItems; // names of CAD objects on list
  char *cadName;       // the text name of a CAD object
  Position x;          // position on list
  char *selectedName;  // the text name of a selected CAD object
  CADRow *object_ptr = CAD_list_head;  // pointer to the linked list of CAD objects
  int done = 0;        // has a cad object with a name matching the current selection been found

  // For more than a few objects this loop/save/redraw will need to move
  // off to a separate thread.

  XtVaGetValues(list_of_existing_CAD_objects,
                XmNitemCount,&itemCount,
                XmNitems,&listItems,
                NULL);
  // iterate through list and delete each first object with a name matching
  // those that are selected on the list.
  //
  // *** Note: If names are not unique the results may not be what the user expects.
  // The first match to a selection will be deleted, not necessarily the selection.
  //
  for (x=1; x<=itemCount; x++)
  {

    if (done)
    {
      break;
    }

    if (XmListPosSelected(list_of_existing_CAD_objects,x))
    {
      int no_label = 0;

      XmStringGetLtoR(listItems[(x-1)],XmFONTLIST_DEFAULT_TAG,&selectedName);

      // Check for our own definition of no label for the CAD
      // objects, which is "<Empty Label>"
      if (strcmp(selectedName,"<Empty Label>") == 0)
      {
        no_label++;
      }

      object_ptr = CAD_list_head;
      done = 0;

      while (object_ptr != NULL && done == 0)
      {

        cadName = object_ptr->label;

        if (strcmp(cadName,selectedName)==0
            || ( (cadName == NULL || cadName[0] == '\0') && no_label) )
        {
          // delete CAD object matching the selected name
          CAD_object_delete(object_ptr);
          done = 1;
        }
        else
        {
          object_ptr = object_ptr->next;
        }
      }
    }
  }

  Draw_CAD_Objects_erase_dialog_close(w,clientData,callData);

  // Save the altered list to file.
  Save_CAD_Objects_to_file();
  // Reload symbols/tracks/CAD objects
  redraw_symbols(da);

  // Here we update the edit cad objects dialog by getting rid of it and
  // then re-establishing it if it is active when we start.  This will
  // usually make the dialog move, but it's better than having it be
  // out-of-date.
  //
  if (cad_list_dialog!=NULL)
  {
    // Update the Edit CAD Objects list
    Draw_CAD_Objects_list_dialog_close(w, clientData, callData);
    Draw_CAD_Objects_list_dialog(w, clientData, callData);
  }
}





// Callback for delete CAD objects menu option.  Dialog to allow
// users to delete all CAD objects or select individual CAD objects
// to delete.
//
void Draw_CAD_Objects_erase_dialog( Widget UNUSED(w),
                                    XtPointer UNUSED(clientData),
                                    XtPointer UNUSED(callData) )
{

  Widget cad_erase_pane, cad_erase_form, cad_erase_label,
         button_delete_all, button_delete_selected, button_cancel;
  Arg al[100];       /* Arg List */
  unsigned int ac;   /* Arg Count */
  CADRow *object_ptr = CAD_list_head;
  int counter = 1;
  XmString cb_item;

  if (cad_erase_dialog)
  {
    (void)XRaiseWindow(XtDisplay(cad_erase_dialog), XtWindow(cad_erase_dialog));
  }
  else
  {

    // Delete CAD Objects
    cad_erase_dialog = XtVaCreatePopupShell("Delete CAD Objects",
                                            xmDialogShellWidgetClass,
                                            appshell,
                                            XmNdeleteResponse,          XmDESTROY,
                                            XmNdefaultPosition,         FALSE,
                                            XmNfontList, fontlist1,
                                            NULL);

    cad_erase_pane = XtVaCreateWidget("CAD erase Object pane",
                                      xmPanedWindowWidgetClass,
                                      cad_erase_dialog,
                                      MY_FOREGROUND_COLOR,
                                      MY_BACKGROUND_COLOR,
                                      NULL);

    cad_erase_form =  XtVaCreateWidget("Cad erase Object form",
                                       xmFormWidgetClass,
                                       cad_erase_pane,
                                       XmNfractionBase,            3,
                                       XmNautoUnmanage,            FALSE,
                                       XmNshadowThickness,         1,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       NULL);

    // heading: Delete CAD Objects
    cad_erase_label = XtVaCreateManagedWidget(langcode("CADPUD009"),
                      xmLabelWidgetClass,
                      cad_erase_form,
                      XmNtopAttachment,           XmATTACH_FORM,
                      XmNtopOffset,               10,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNleftAttachment,          XmATTACH_FORM,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);

    // *** need to handle the special case of no CAD objects ? ***

    // scrolled pick list to allow selection of current objects
    /*set args for list */
    ac=0;
    XtSetArg(al[ac], XmNvisibleItemCount, 11);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmEXTENDED_SELECT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNtopWidget, cad_erase_label);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE);
    ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNrightOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNleftOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
    ac++;
    //XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, colors[0x0f]);
    ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1);
    ac++;

    list_of_existing_CAD_objects = XmCreateScrolledList(cad_erase_form,
                                   "CAD objects for deletion scrolled list",
                                   al,
                                   ac);
    // make sure list is empty
    XmListDeleteAllItems(list_of_existing_CAD_objects);

    // iterate through list of objects to populate scrolled list
    while (object_ptr != NULL)
    {

      //  If no label, use the string "<Empty Label>" instead
      if (object_ptr->label[0] == '\0')
      {
        cb_item = XmStringCreateLtoR("<Empty Label>", XmFONTLIST_DEFAULT_TAG);
      }
      else
      {
        cb_item = XmStringCreateLtoR(object_ptr->label, XmFONTLIST_DEFAULT_TAG);
      }


      XmListAddItem(list_of_existing_CAD_objects,
                    cb_item,
                    counter);
      counter++;
      XmStringFree(cb_item);
      object_ptr = object_ptr->next;
    }

    // "Delete All"
    button_delete_all = XtVaCreateManagedWidget(langcode("CADPUD010"),
                        xmPushButtonGadgetClass,
                        cad_erase_form,
                        XmNtopAttachment,     XmATTACH_WIDGET,
                        XmNtopWidget,         list_of_existing_CAD_objects,
                        XmNtopOffset,         5,
                        XmNbottomAttachment,  XmATTACH_FORM,
                        XmNbottomOffset,      5,
                        XmNleftAttachment,    XmATTACH_FORM,
                        XmNleftOffset,        5,
                        XmNnavigationType,    XmTAB_GROUP,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        XmNfontList, fontlist1,
                        NULL);
    XtAddCallback(button_delete_all, XmNactivateCallback, Draw_CAD_Objects_erase, Draw_CAD_Objects_erase_dialog);

    // "Delete Selected"
    button_delete_selected = XtVaCreateManagedWidget(langcode("CADPUD011"),
                             xmPushButtonGadgetClass,
                             cad_erase_form,
                             XmNtopAttachment,     XmATTACH_WIDGET,
                             XmNtopWidget,         list_of_existing_CAD_objects,
                             XmNtopOffset,         5,
                             XmNbottomAttachment,  XmATTACH_FORM,
                             XmNbottomOffset,      5,
                             XmNleftAttachment,    XmATTACH_WIDGET,
                             XmNleftWidget,        button_delete_all,
                             XmNleftOffset,        10,
                             XmNnavigationType,    XmTAB_GROUP,
                             MY_FOREGROUND_COLOR,
                             MY_BACKGROUND_COLOR,
                             XmNfontList, fontlist1,
                             NULL);
    XtAddCallback(button_delete_selected, XmNactivateCallback, Draw_CAD_Objects_erase_selected, Draw_CAD_Objects_erase_dialog);

    // "Cancel"
    button_cancel = XtVaCreateManagedWidget(langcode("CADPUD008"),
                                            xmPushButtonGadgetClass,
                                            cad_erase_form,
                                            XmNtopAttachment,     XmATTACH_WIDGET,
                                            XmNtopWidget,         list_of_existing_CAD_objects,
                                            XmNtopOffset,         5,
                                            XmNbottomAttachment,  XmATTACH_FORM,
                                            XmNbottomOffset,      5,
                                            XmNleftAttachment,    XmATTACH_WIDGET,
                                            XmNleftWidget,        button_delete_selected,
                                            XmNleftOffset,        10,
                                            XmNnavigationType,    XmTAB_GROUP,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);
    XtAddCallback(button_cancel, XmNactivateCallback, Draw_CAD_Objects_erase_dialog_close, Draw_CAD_Objects_erase_dialog);
    pos_dialog(cad_erase_dialog);
    XmInternAtom(XtDisplay(cad_erase_dialog),"WM_DELETE_WINDOW", FALSE);

    XtManageChild(cad_erase_form);
    XtManageChild(list_of_existing_CAD_objects);
    XtManageChild(cad_erase_pane);

    XtPopup(cad_erase_dialog,XtGrabNone);
  }
}





// popdown and destroy the cad_list_dialog
//
void Draw_CAD_Objects_list_dialog_close ( Widget UNUSED(w),
    XtPointer UNUSED(clientData),
    XtPointer UNUSED(callData) )
{

  if (cad_list_dialog!=NULL)
  {
    // close cad_list_dialog
    XtPopdown(cad_list_dialog);
    XtDestroyWidget(cad_list_dialog);
    cad_list_dialog = (Widget)NULL;
  }

}





// Show details for selected CAD object.  Callback for the show/edit
// details button on the Draw_CAD_Objects_list dialog.
//
void Show_selected_CAD_object_details ( Widget UNUSED(w),
                                        XtPointer UNUSED(clientData),
                                        XtPointer UNUSED(callData) )
{

  static int sizeof_area_description = 200;
  int itemCount;       // number of items in list of CAD objects.
  XmString *listItems; // names of CAD objects on list
  char *cadName;       // the text name of a CAD object
  Position x;          // position on list
  char *selectedName;  // the text name of a selected CAD object
  CADRow *object_ptr = CAD_list_head;  // pointer to the linked list of CAD objects
  int done = 0;        // has a cad object with a name matching the current selection been found
  double area;
  char area_description[sizeof_area_description];
  xastir_snprintf(area_description, sizeof_area_description, "Area");

  if (cad_list_dialog!=NULL)
  {
    // get the selected object
    XtVaGetValues(list_of_existing_CAD_objects_edit,
                  XmNitemCount,&itemCount,
                  XmNitems,&listItems,
                  NULL);
    // iterate through list and find each object with a name
    // matching one selected on the list.
    //
    // *** Note: If names are not unique the results may not be what the user expects.
    // The first match to a selection will be used, not necessarily the selection.
    //
    for (x=1; x<=itemCount; x++)
    {
      if (XmListPosSelected(list_of_existing_CAD_objects_edit,x))
      {
        int no_label = 0;

        XmStringGetLtoR(listItems[(x-1)],XmFONTLIST_DEFAULT_TAG,&selectedName);

        // Check for our own definition of no label for the CAD
        // objects, which is "<Empty Label>"
        if (strcmp(selectedName,"<Empty Label>") == 0)
        {
          no_label++;
        }

        object_ptr = CAD_list_head;
        done = 0;

        while (object_ptr != NULL && done == 0)
        {

          cadName = object_ptr->label;

          if (strcmp(cadName,selectedName)==0
              || ( (cadName == NULL || cadName[0] == '\0') && no_label) )
          {

            // get the area for the CAD object matching the selected name
            // and format it as a localized string.
            area = object_ptr->computed_area;
            Format_area_for_output(&area, area_description,sizeof_area_description);
            // open the CAD object details dialog for the matching CAD object
            Set_CAD_object_parameters_dialog(area_description,object_ptr);
            done = 1;
          }
          else
          {
            object_ptr = object_ptr->next;
          }
        }
      }
    }

    // leave the list dialog open
  }
}





// Callback for edit CAD objects menu option.  Dialog to allow users
// to select individual CAD objects in order to edit their metadata.
//
void Draw_CAD_Objects_list_dialog( Widget UNUSED(w),
                                   XtPointer UNUSED(clientData),
                                   XtPointer UNUSED(callData) )
{

  Widget cad_list_pane, cad_list_form, cad_list_label,
         button_list_selected, button_close;
  Arg al[100];       /* Arg List */
  unsigned int ac;   /* Arg Count */
  CADRow *object_ptr = CAD_list_head;
  int counter = 1;
  XmString cb_item;

  if (cad_list_dialog)
  {
    (void)XRaiseWindow(XtDisplay(cad_list_dialog), XtWindow(cad_list_dialog));
  }
  else
  {

    // List CAD Objects
    cad_list_dialog = XtVaCreatePopupShell("List CAD Objects",
                                           xmDialogShellWidgetClass,
                                           appshell,
                                           XmNdeleteResponse,          XmDESTROY,
                                           XmNdefaultPosition,         FALSE,
                                           XmNfontList, fontlist1,
                                           NULL);

    cad_list_pane = XtVaCreateWidget("CAD list Object pane",
                                     xmPanedWindowWidgetClass,
                                     cad_list_dialog,
                                     MY_FOREGROUND_COLOR,
                                     MY_BACKGROUND_COLOR,
                                     XmNfontList, fontlist1,
                                     NULL);

    cad_list_form =  XtVaCreateWidget("Cad list Object form",
                                      xmFormWidgetClass,
                                      cad_list_pane,
                                      XmNfractionBase,            3,
                                      XmNautoUnmanage,            FALSE,
                                      XmNshadowThickness,         1,
                                      MY_FOREGROUND_COLOR,
                                      MY_BACKGROUND_COLOR,
                                      XmNfontList, fontlist1,
                                      NULL);

    // heading: CAD Objects
    cad_list_label = XtVaCreateManagedWidget(langcode("CADPUD006"),
                     xmLabelWidgetClass,
                     cad_list_form,
                     XmNtopAttachment,           XmATTACH_FORM,
                     XmNtopOffset,               10,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_FORM,
                     XmNleftOffset,              10,
                     XmNrightAttachment,         XmATTACH_NONE,
                     MY_FOREGROUND_COLOR,
                     MY_BACKGROUND_COLOR,
                     XmNfontList, fontlist1,
                     NULL);

    // *** need to handle the special case of no CAD objects ? ***

    // scrolled pick list to allow selection of current objects
    /*set args for list */
    ac=0;
    XtSetArg(al[ac], XmNvisibleItemCount, 11);
    ac++;
    XtSetArg(al[ac], XmNtraversalOn, TRUE);
    ac++;
    XtSetArg(al[ac], XmNshadowThickness, 3);
    ac++;
    XtSetArg(al[ac], XmNselectionPolicy, XmSINGLE_SELECT);
    ac++;
    XtSetArg(al[ac], XmNscrollBarPlacement, XmBOTTOM_RIGHT);
    ac++;
    XtSetArg(al[ac], XmNtopAttachment, XmATTACH_WIDGET);
    ac++;
    XtSetArg(al[ac], XmNtopWidget, cad_list_label);
    ac++;
    XtSetArg(al[ac], XmNtopOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_NONE);
    ac++;
    XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNrightOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);
    ac++;
    XtSetArg(al[ac], XmNleftOffset, 5);
    ac++;
    XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
    ac++;
    //XtSetArg(al[ac], XmNbackground, MY_BG_COLOR); ac++;
    XtSetArg(al[ac], XmNbackground, colors[0x0f]);
    ac++;
    XtSetArg(al[ac], XmNfontList, fontlist1);
    ac++;

    list_of_existing_CAD_objects_edit = XmCreateScrolledList(cad_list_form,
                                        "CAD objects for deletion scrolled list",
                                        al,
                                        ac);
    // make sure list is empty
    XmListDeleteAllItems(list_of_existing_CAD_objects_edit);

    // iterate through list of objects to populate scrolled list
    while (object_ptr != NULL)
    {

      //  If no label, use the string "<Empty Label>" instead
      if (object_ptr->label[0] == '\0')
      {
        cb_item = XmStringCreateLtoR("<Empty Label>", XmFONTLIST_DEFAULT_TAG);
      }
      else
      {
        cb_item = XmStringCreateLtoR(object_ptr->label, XmFONTLIST_DEFAULT_TAG);
      }

      XmListAddItem(list_of_existing_CAD_objects_edit,
                    cb_item,
                    counter);
      counter++;
      XmStringFree(cb_item);
      object_ptr = object_ptr->next;
    }

    // "Show/edit details"
    button_list_selected = XtVaCreateManagedWidget(langcode("CADPUD007"),
                           xmPushButtonGadgetClass,
                           cad_list_form,
                           XmNtopAttachment,     XmATTACH_WIDGET,
                           XmNtopWidget,         list_of_existing_CAD_objects_edit,
                           XmNtopOffset,         5,
                           XmNbottomAttachment,  XmATTACH_FORM,
                           XmNbottomOffset,      5,
                           XmNleftAttachment,    XmATTACH_FORM,
                           XmNleftOffset,        10,
                           XmNnavigationType,    XmTAB_GROUP,
                           MY_FOREGROUND_COLOR,
                           MY_BACKGROUND_COLOR,
                           XmNfontList, fontlist1,
                           NULL);
    XtAddCallback(button_list_selected, XmNactivateCallback, Show_selected_CAD_object_details, Draw_CAD_Objects_list_dialog);

    // "Close"
    button_close = XtVaCreateManagedWidget(langcode("UNIOP00003"),
                                           xmPushButtonGadgetClass,
                                           cad_list_form,
                                           XmNtopAttachment,     XmATTACH_WIDGET,
                                           XmNtopWidget,         list_of_existing_CAD_objects_edit,
                                           XmNtopOffset,         5,
                                           XmNbottomAttachment,  XmATTACH_FORM,
                                           XmNbottomOffset,      5,
                                           XmNleftAttachment,    XmATTACH_WIDGET,
                                           XmNleftWidget,        button_list_selected,
                                           XmNleftOffset,        10,
                                           XmNnavigationType,    XmTAB_GROUP,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);
    XtAddCallback(button_close, XmNactivateCallback, Draw_CAD_Objects_list_dialog_close, Draw_CAD_Objects_erase_dialog);
    pos_dialog(cad_list_dialog);
    XmInternAtom(XtDisplay(cad_list_dialog),"WM_DELETE_WINDOW", FALSE);

    XtManageChild(cad_list_form);
    XtManageChild(list_of_existing_CAD_objects_edit);
    XtManageChild(cad_list_pane);

    XtPopup(cad_list_dialog,XtGrabNone);
  }
}





// Free the object and vertice lists then do a screen update.
// callback from delete all button on cad_erase_dialog.
//
void Draw_CAD_Objects_erase( Widget w,
                             XtPointer clientData,
                             XtPointer callData)
{

  // if we were called from the cad_erase_dialog, make sure it is closed properly
  if (cad_erase_dialog)
  {
    Draw_CAD_Objects_erase_dialog_close(w,clientData,callData);
  }

  CAD_object_delete_all();
  polygon_last_x = -1;    // Invalid position
  polygon_last_y = -1;    // Invalid position

  // Save the empty list out to file
  Save_CAD_Objects_to_file();

  // Reload symbols/tracks/CAD objects
  redraw_symbols(da);
}





// Formats an area as a string in english (square miles) or metric units
// (square kilometers). Switches to square feet or square meters if the
// area is less than 0.1 of the units.
//
// Parameters:
//     area: an area in square kilometers.
//     area_description: area reformatted as a localized text string.
//     sizeof_area_description: array length of area_description.
//
void Format_area_for_output(double *area_km2, char *area_description, int sizeof_area_description)
{
  double area;
  // Format it for output and dump it out.  We're using square terms, so
  // apply the conversion factor twice to convert from square kilometers
  // to the units of interest. The result here is squared meters or
  // squared feet.
//fprintf(stderr,"Square km: %f\n", *area_km2);

  area = *area_km2 * 1000.0 * 1000.0 * cvt_m2len * cvt_m2len;

  // We could be measuring a very small or a very large object.
  // In the case of very small, convert it to square feet or
  // square meters.

  if (english_units)   // Square feet
  {
//fprintf(stderr,"Square feet: %f\n", area);

    if (area < 2787840.0)   // Switch at 0.5 miles squared
    {
      // Smaller area: Output in feet squared
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("POPUPMA053") );   // ft
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
    else
    {
      // Larger area: Output in miles squared
      area = area / 27878400.0;
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("POPUPMA055") );   // mi
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
  }
  else    // Square meters
  {
//fprintf(stderr,"Square meters: %f\n", area);

    if (area < 100000.0)   // Switch at 0.1 km squared
    {
      // Smaller area: Output in meters squared
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("POPUPMA054") );   // meters
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
    else
    {
      // Larger ara: Output in kilometers squared
      area = area / 1000000.0;
      xastir_snprintf(area_description,
                      sizeof_area_description,
                      "A:%0.2f %s %s",
                      area,
                      langcode("POPUPMA052"),     // sq
                      langcode("UNIOP00005") );   // km
      //popup_message_always(langcode("POPUPMA020"),area_description);
    }
  }
}





// Add an ending vertice that is the same as the starting vertice.
// Best not to use the screen coordinates we captured first, as the
// user may have zoomed or panned since then.  Better to copy the
// first vertice that we recorded in our linked list.
//
// Compute the area of the closed polygon.  Write it out to STDERR,
// the computed_area field in the Object, and to a dialog that pops
// up on the screen.
//
void Draw_CAD_Objects_close_polygon( Widget UNUSED(widget),
                                     XtPointer UNUSED(clientData),
                                     XtPointer UNUSED(callData) )
{
  static int sizeof_area_description = 200;
  VerticeRow *tmp;
  double area;
  int n;
  //char temp_course[20];
  char area_description[sizeof_area_description];
  xastir_snprintf(area_description, sizeof_area_description, "Area");

  // Check whether we're currently working on a polygon.  If not,
  // get out of here.
  if (polygon_last_x == -1 || polygon_last_y == -1)
  {

    // Tell the code that we're starting a new polygon by wiping
    // out the first position.
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position

    return;
  }

  // Find the last vertice in the linked list.  That will be the
  // first vertice we recorded for the object.

  // Check for at least three vertices.  We don't need to check
  // that the first/last point are equal:  We force it below by
  // copying the first vertice to the last.
  //
  n = 0;
  if (CAD_list_head != NULL)
  {

    // Walk the linked list.  Stop at the last record.
    tmp = CAD_list_head->start;
    if (tmp != NULL)
    {
      n++;
      while (tmp->next != NULL)
      {
        tmp = tmp->next;
        n++;
      }
      if (n > 2)
      {
        // We have more than a point or a line, therefore
        // can copy the first point to the last, closing the
        // polygon.
        CAD_vertice_allocate(tmp->latitude, tmp->longitude);
      }
    }
  }
  // Reload symbols/tracks/CAD objects and redraw the polygon
  redraw_symbols(da);

#ifdef CAD_DEBUG
  fprintf(stderr,"Points in closed polygon: n = %d\n",n);
#endif

  if (n < 3)
  {
    // Not enough points to compute an area.

    // Tell the code that we're starting a new polygon by wiping
    // out the first position.
    polygon_last_x = -1;    // Invalid position
    polygon_last_y = -1;    // Invalid position

    return;
  }
  area =  CAD_object_compute_area(CAD_list_head);

  // Save it in the object.  Convert nautical square miles to
  // square kilometers because that's what "Format_area_for_output"
  // requires.
  area = area * 3.429903999977917; // Now in km squared
//fprintf(stderr,"SQUARE KM: %f\n", area);
  CAD_list_head->computed_area = area;

  Format_area_for_output(&area, area_description, sizeof_area_description);

#ifdef CAD_DEBUG
  // Also write the area to stderr
  fprintf(stderr,"New CAD object %s\n",area_description);
#endif

  // Tell the code that we're starting a new polygon by wiping out
  // the first position.
  polygon_last_x = -1;    // Invalid position
  polygon_last_y = -1;    // Invalid position

  CAD_object_set_label_at_centroid(CAD_list_head);
  // CAD object vertices are ready, needs associated data
  // obtain label, comment, and probability for this polygon
  // from user through a dialog.
  Set_CAD_object_parameters_dialog(area_description,NULL);
}





// Function called by UpdateTime when doing screen refresh.  Draws
// all CAD objects onto the screen again.
//
void Draw_All_CAD_Objects(Widget w)
{
  CADRow *object_ptr = CAD_list_head;
  long x_long, y_lat;
  long x_offset, y_offset;
  float probability;
  char probability_string[8];
  VerticeRow *vertice;
  double area;
  int actual_line_type = LineOnOffDash;
  static int sizeof_area_description = 50; // define here as local static to limit size of display on map
  // independent of size as shown on form
  char area_description[sizeof_area_description];
  char dash[2];

  // Start at CAD_list_head, iterate through entire linked list,
  // drawing as we go.  Respect the line
  // width/line_color/line_type variables for each object.

//fprintf(stderr,"Drawing CAD objects\n");
  if (CAD_draw_objects==TRUE)
  {
    while (object_ptr != NULL)
    {
      probability = CAD_object_get_raw_probability(object_ptr,1);
      xastir_snprintf(probability_string,
                      sizeof(probability_string),
                      "%01.1f%%",
                      probability);

      // find point at which to draw label and other descriptive text
      x_long = object_ptr->label_longitude;
      y_lat = object_ptr->label_latitude;
#ifdef CAD_DEBUG
      fprintf(stderr,"Drawing object %s\n", (object_ptr->label) ? object_ptr->label : "NULL" );
#endif
      //fprintf(stderr,"Lat: %d\n", y_lat);
      //fprintf(stderr,"Long: %d\n", x_long);
//            if ((x_long+10>=0) && (x_long-10<=129600000l)) {      // 360 deg

//                if ((y_lat+10>=0) && (y_lat-10<=64800000l)) {     // 180 deg

      if ((x_long>NW_corner_longitude) && (x_long<SE_corner_longitude))
      {

        if ((y_lat>NW_corner_latitude) && (y_lat<SE_corner_latitude))
        {

          // ok to draw label and assocated data, point is on screen
          x_offset=((x_long-NW_corner_longitude)/scale_x)-(10);
          y_offset=((y_lat -NW_corner_latitude) /scale_y)-(10);
          // ****** ?? use -10 or point ??
          x_offset=((x_long-NW_corner_longitude)/scale_x);
          y_offset=((y_lat -NW_corner_latitude) /scale_y);

          if (((int)strlen(object_ptr->label)>0) & (CAD_show_label==TRUE))
          {
            // Draw Label
            // 0x08 is background color
            // 0x40 is foreground color (yellow)
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,object_ptr->label,0x08,0x40,strlen(object_ptr->label));

            x_offset=x_offset+12;
            y_offset=y_offset+15;
          }

          if (CAD_show_raw_probability==TRUE)
          {
            // draw probability
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,probability_string,0x08,0x40,strlen(probability_string));
            y_offset=y_offset+15;
          }

          if ((CAD_show_comment==TRUE) & ((int)strlen(object_ptr->comment)>0))
          {
            // draw comment
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,object_ptr->comment,0x08,0x40,strlen(object_ptr->comment));
            y_offset=y_offset+15;
          }


          if (CAD_show_area==TRUE)
          {
            area = object_ptr->computed_area;
            Format_area_for_output(&area, area_description, sizeof_area_description);
            draw_nice_string(w,pixmap_final,letter_style,x_offset,y_offset,area_description,0x08,0x40,strlen(area_description));
            y_offset=y_offset+15;
          }

        }
      }
//                }
//            }

      // Iterate through the vertices and draw the lines
      vertice = object_ptr->start;

      switch (object_ptr->line_type)
      {

        case 1:
          actual_line_type = LineSolid;
          break;

        case 2:
          actual_line_type = LineOnOffDash;
          dash[0] = dash[1]  = 8;
          break;

        case 3:
          actual_line_type = LineDoubleDash;
          dash[0] = dash[1]  = 16;
          break;

        default:
          actual_line_type = LineOnOffDash;
          dash[0] = dash[1]  = 8;
          break;
      }

      // Set up line color/width/type here
      (void)XSetLineAttributes (XtDisplay (da),
                                gc_tint,
                                object_ptr->line_width,
                                actual_line_type,
                                CapButt,
                                JoinMiter);

      if (object_ptr->line_type  != 1)
      {
        (void)XSetDashes (XtDisplay (da),
                          gc_tint,
                          0,       // dash offset
                          dash,    // dash list[]
                          2);      // elements in dash lista
      }

      (void)XSetForeground (XtDisplay (da),
                            gc_tint,
                            object_ptr->line_color);

      (void)XSetFunction (XtDisplay (da),
                          gc_tint,
                          GXxor);

      while (vertice != NULL)
      {
        if (vertice->next != NULL)
        {
          // Use the draw_vector function from maps.c
          draw_vector(w,
                      vertice->longitude,
                      vertice->latitude,
                      vertice->next->longitude,
                      vertice->next->latitude,
                      gc_tint,
                      pixmap_final,
                      0);
        }
        vertice = vertice->next;
      }
      object_ptr = object_ptr->next;
    }
  }
}



/////////////////////////////////////////   Object Dialog   //////////////////////////////////////////



/*
 *  Destroy Object Dialog Popup Window
 */
void Object_destroy_shell( Widget widget, XtPointer clientData, XtPointer callData)
{
  Widget shell = (Widget) clientData;
  XtPopdown(shell);
  (void)XFreePixmap(XtDisplay(appshell),Ob_icon0);
  (void)XFreePixmap(XtDisplay(appshell),Ob_icon);
  XtDestroyWidget(shell);
  object_dialog = (Widget)NULL;

  // Take down the symbol selection dialog as well (if it's up)
  if (select_symbol_dialog)
  {
    Select_symbol_destroy_shell( widget, select_symbol_dialog, callData);
  }

  // NULL out the dialog field in the global struct used for
  // Coordinate Calculator.  Prevents segfaults if the calculator is
  // still up and trying to write to us.
  coordinate_calc_array.calling_dialog = NULL;
}





// Snag the latest DR'ed position for an object/item and create the
// types of strings that we need for Setup_object_data() and
// Setup_item_data() for APRS and Base-91 compressed formats.
//
void fetch_current_DR_strings(DataRow *p_station, char *lat_str,
                              char *lon_str, char *ext_lat_str, char *ext_lon_str)
{

  long x_long, y_lat;


  // Fetch the latest position for a DR'ed object.  Returns the
  // values in two longs, x_long and y_lat.
  compute_current_DR_position(p_station, &x_long, &y_lat);

  // Normal precision for APRS format
  convert_lat_l2s(y_lat,
                  lat_str,
                  MAX_LAT,
                  CONVERT_LP_NOSP);

  convert_lon_l2s(x_long,
                  lon_str,
                  MAX_LONG,
                  CONVERT_LP_NOSP);

  // Extended precision for Base-91 compressed format
  convert_lat_l2s(y_lat,
                  ext_lat_str,
                  MAX_LAT,
                  CONVERT_HP_NOSP);

  convert_lon_l2s(x_long,
                  ext_lon_str,
                  MAX_LONG,
                  CONVERT_HP_NOSP);
}





// Convert this eventually to populate a DataRow struct, then call
// data.c:Create_object_item_tx_string().  Right now we have a lot
// of code duplication between Setup_object_data, Setup_item_data,
// and Create_object_item_tx_string.
//
// Make sure to look at the "transmit_compressed_objects_items" variable
// to decide whether to send a compressed packet.
/*
 *  Setup APRS Information Field for Objects
 */
int Setup_object_data(char *line, int line_length, DataRow *p_station)
{
  char lat_str[MAX_LAT];
  char lon_str[MAX_LONG];
  char ext_lat_str[20];   // Extended precision for base91 compression
  char ext_lon_str[20];   // Extended precision for base91 compression
  char comment[43+1];                 // max 43 characters of comment
  char time[7+1];
  struct tm *day_time;
  time_t sec;
  char complete_area_color[3];
  int complete_area_type;
  int lat_offset, lon_offset;
  char complete_corridor[6];
  char altitude[10];
  char speed_course[8];
  int speed;
  int course;
  int temp;
  long temp2;
  float temp3;
  char signpost[6];
  char prob_min[20+1];
  char prob_max[20+1];
  int bearing;
  char *temp_ptr;
  char *temp_ptr2;
  int DR = 0; // Dead-reckoning flag
  int comment_field_used = 0;   // Amount of comment field used up


  //fprintf(stderr,"Setup_object_data\n");

  // If speed for the original object was non-zero, then we need
  // to snag the updated DR'ed position for the object.  Snag the
  // current DR position and build the strings we need // for
  // position transmit.
  //
  if (p_station != NULL)
  {

    speed = atoi(p_station->speed);

    if (speed > 0 && !doing_move_operation)
    {

      fetch_current_DR_strings(p_station,
                               lat_str,
                               lon_str,
                               ext_lat_str,
                               ext_lon_str);
      DR++;   // Set the dead-reckoning flag
    }

    // Keep the time current for our own objects.
    p_station->sec_heard = sec_now();
    move_station_time(p_station,NULL);
  }

  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(line,
                  line_length,
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(line);
  //(void)to_upper(line);      Not per spec.  Don't use this.
  if (!valid_object(line))
  {
    return(0);
  }

  // Copy object name into "last_object"
  xastir_snprintf(last_object,
                  sizeof(last_object),
                  "%s",
                  line);


  if (!DR)    // We're not doing dead-reckoning, so proceed
  {

    temp_ptr = XmTextFieldGetString(object_lat_data_ns);
    if((char)toupper((int)temp_ptr[0]) == 'S')
    {
      line[0] = 'S';
    }
    else
    {
      line[0] = 'N';
    }
    XtFree(temp_ptr);

    // Check latitude for out-of-bounds
    temp_ptr = XmTextFieldGetString(object_lat_data_deg);
    temp = atoi(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp > 90) || (temp < 0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lat_data_min);
    temp3 = atof(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
    {
      return(0);
    }
    if ( (temp == 90) && (temp3 != 0.0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lat_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lat_data_min);
    xastir_snprintf(lat_str, sizeof(lat_str), "%02d%05.2f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//            atof(temp_ptr2) + 0.001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(object_lat_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lat_data_min);
    xastir_snprintf(ext_lat_str, sizeof(ext_lat_str), "%02d%05.3f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.0001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//            atof(temp_ptr2) + 0.0001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(object_lon_data_ew);
    xastir_snprintf(line,
                    line_length,
                    "%s",
                    temp_ptr);
    XtFree(temp_ptr);

    if((char)toupper((int)line[0]) == 'E')
    {
      line[0] = 'E';
    }
    else
    {
      line[0] = 'W';
    }

    // Check longitude for out-of-bounds
    temp_ptr = XmTextFieldGetString(object_lon_data_deg);
    temp = atoi(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp > 180) || (temp < 0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lon_data_min);
    temp3 = atof(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
    {
      return(0);
    }

    if ( (temp == 180) && (temp3 != 0.0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lon_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lon_data_min);
    xastir_snprintf(lon_str, sizeof(lon_str), "%03d%05.2f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//            atof(temp_ptr2) + 0.001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(object_lon_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lon_data_min);
    xastir_snprintf(ext_lon_str, sizeof(ext_lon_str), "%03d%05.3f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.0001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//            atof(temp_ptr2) + 0.0001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);
  }


  temp_ptr = XmTextFieldGetString(object_group_data);
  xastir_snprintf(line,
                  line_length,
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  last_obj_grp = line[0];
  if(isalpha((int)last_obj_grp))
  {
    last_obj_grp = toupper((int)line[0]);  // todo: toupper in dialog
  }

  // Check for overlay character
  if (last_obj_grp != '/' && last_obj_grp != '\\')
  {
    // Found an overlay character.  Check that it's within the
    // proper range
    if ( (last_obj_grp >= '0' && last_obj_grp <= '9')
         || (last_obj_grp >= 'A' && last_obj_grp <= 'Z') )
    {
      last_obj_overlay = last_obj_grp;
      last_obj_grp = '\\';
    }
    else
    {
      last_obj_overlay = '\0';
    }
  }
  else
  {
    last_obj_overlay = '\0';
  }

  temp_ptr = XmTextFieldGetString(object_symbol_data);
  xastir_snprintf(line,
                  line_length,
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  last_obj_sym = line[0];

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(comment);
  //fprintf(stderr,"Comment Field was: %s\n",comment);

  sec = sec_now();
  day_time = gmtime(&sec);
  xastir_snprintf(time, sizeof(time), "%02d%02d%02dz", day_time->tm_mday, day_time->tm_hour,
                  day_time->tm_min);

  // Handle Generic Options

  // Speed/Course Fields
  temp_ptr = XmTextFieldGetString(ob_course_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  xastir_snprintf(speed_course, sizeof(speed_course), "%s", ".../");
  course = 0;
  if (strlen(line) != 0)      // Course was entered
  {
    // Need to check for 1 to three digits only, and 001-360 degrees)
    temp = atoi(line);
    if ( (temp >= 1) && (temp <= 360) )
    {
      xastir_snprintf(speed_course, sizeof(speed_course), "%03d/", temp);
      course = temp;
    }
    else if (temp == 0)       // Spec says 001 to 360 degrees...
    {
      xastir_snprintf(speed_course, sizeof(speed_course), "%s", "360/");
    }
  }
  temp_ptr = XmTextFieldGetString(ob_speed_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  speed = 0;
  if (strlen(line) != 0)   // Speed was entered (we only handle knots currently)
  {
    // Need to check for 1 to three digits, no alpha characters
    temp = atoi(line);
    if ( (temp >= 0) && (temp <= 999) )
    {
      xastir_snprintf(line, line_length, "%03d", temp);
      strncat(speed_course,
              line,
              sizeof(speed_course) - 1 - strlen(speed_course));
      speed = temp;
    }
    else
    {
      strncat(speed_course,
              "...",
              sizeof(speed_course) - 1 - strlen(speed_course));
    }
  }
  else    // No speed entered, blank it out
  {
    strncat(speed_course,
            "...",
            sizeof(speed_course) - 1 - strlen(speed_course));
  }
  if ( (speed_course[0] == '.') && (speed_course[4] == '.') )
  {
    speed_course[0] = '\0'; // No speed or course entered, so blank it
  }
  if (Area_object_enabled)
  {
    speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
  }

  // Altitude Field
  temp_ptr = XmTextFieldGetString(ob_altitude_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  //fprintf(stderr,"Altitude entered: %s\n", line);
  altitude[0] = '\0'; // Start with empty string
  if (strlen(line) != 0)     // Altitude was entered (we only handle feet currently)
  {
    // Need to check for all digits, and 1 to 6 digits
    if (isdigit((int)line[0]))
    {
      temp2 = atoi(line);
      if ( (temp2 >= 0) && (temp2 <= 999999) )
      {
        char temp_alt[20];
        xastir_snprintf(temp_alt, sizeof(temp_alt), "/A=%06ld", temp2);
        memcpy(altitude, temp_alt, sizeof(altitude));
        altitude[sizeof(altitude)-1] = '\0';  // Terminate string
        //fprintf(stderr,"Altitude string: %s\n",altitude);
      }
    }
  }

  // Handle Specific Options

  // Area Objects
  if (Area_object_enabled)
  {
    //fprintf(stderr,"Area_bright: %d\n", Area_bright);
    //fprintf(stderr,"Area_filled: %d\n", Area_filled);
    if (Area_bright)    // Bright color
    {
      xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%2s", Area_color);
    }
    else                  // Dim color
    {
      xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02.0f",
                      (float)(atoi(&Area_color[1]) + 8) );

      if ( (Area_color[1] == '0') || (Area_color[1] == '1') )
      {
        complete_area_color[0] = '/';
      }
    }
    if ( (Area_filled) && (Area_type != 1) && (Area_type != 6) )
    {
      complete_area_type = Area_type + 5;
    }
    else      // Can't fill in a line
    {
      complete_area_type = Area_type;
    }
    temp_ptr = XmTextFieldGetString(ob_lat_offset_data);
    xastir_snprintf(line, line_length, "%s", temp_ptr);
    XtFree(temp_ptr);

    lat_offset = sqrt(atof(line));
    if (lat_offset > 99)
    {
      lat_offset = 99;
    }
    //fprintf(stderr,"Line: %s\tlat_offset: %d\n", line, lat_offset);

    temp_ptr = XmTextFieldGetString(ob_lon_offset_data);
    xastir_snprintf(line, line_length, "%s", temp_ptr);
    XtFree(temp_ptr);

    lon_offset = sqrt(atof(line));
    if (lon_offset > 99)
    {
      lon_offset = 99;
    }
    //fprintf(stderr,"Line: %s\tlon_offset: %d\n", line, lon_offset);

    //fprintf(stderr,"Corridor Field: %s\n", XmTextFieldGetString(ob_corridor_data) );
    // Corridor
    complete_corridor[0] = '\0';
    if ( (Area_type == 1) || (Area_type == 6) )
    {

      temp_ptr = XmTextFieldGetString(ob_corridor_data);
      xastir_snprintf(line, line_length, "%s", temp_ptr);
      XtFree(temp_ptr);

      if (strlen(line) != 0)      // We have a line and some corridor data
      {
        // Need to check for 1 to three digits only
        temp = atoi(line);
        if ( (temp > 0) && (temp <= 999) )
        {
          xastir_snprintf(complete_corridor, sizeof(complete_corridor), "{%d}", temp);
          //fprintf(stderr,"%s\n",complete_corridor);
        }
      }
    }

    //fprintf(stderr,"Complete_corridor: %s\n", complete_corridor);

    if (transmit_compressed_objects_items)
    {
      char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


      // Need to handle the conversion of numeric overlay
      // chars to "a-j" here.
      if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
      {
        temp_overlay = last_obj_overlay + 'a';
      }

      xastir_snprintf(line, line_length, ";%-9s*%s%s%1d%02d%2s%02d%s%s%s",
                      last_object,
                      time,
                      compress_posit(ext_lat_str,
                                     (temp_overlay) ? temp_overlay : last_obj_grp,
                                     ext_lon_str,
                                     last_obj_sym,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank in this case
                      complete_area_type, // In comment field
                      lat_offset,         // In comment field
                      complete_area_color,// In comment field
                      lon_offset,         // In comment field
                      speed_course,       // In comment field
                      complete_corridor,  // In comment field
                      altitude);          // In comment field

      comment_field_used = strlen(line) - 31;

    }
    else
    {

      xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%1d%02d%2s%02d%s%s%s",
                      last_object,
                      time,
                      lat_str,
                      last_obj_grp,
                      lon_str,
                      last_obj_sym,
                      complete_area_type, // In comment field
                      lat_offset,         // In comment field
                      complete_area_color,// In comment field
                      lon_offset,         // In comment field
                      speed_course,       // In comment field
                      complete_corridor,  // In comment field
                      altitude);          // In comment field

      comment_field_used = strlen(line) - 37;
    }

    //fprintf(stderr,"String is: %s\n", line);

  }
  else if (Signpost_object_enabled)
  {
    temp_ptr = XmTextFieldGetString(signpost_data);
    xastir_snprintf(line, line_length, "%s", temp_ptr);
    XtFree(temp_ptr);

    //fprintf(stderr,"Signpost entered: %s\n", line);
    if (strlen(line) != 0)     // Signpost data was entered
    {
      // Need to check for between one and three characters
      temp = strlen(line);
      if ( (temp >= 0) && (temp <= 3) )
      {
        xastir_snprintf(signpost, sizeof(signpost), "{%s}", line);
      }
      else
      {
        signpost[0] = '\0';
      }
    }
    else      // No signpost data entered, blank it out
    {
      signpost[0] = '\0';
    }


    if (transmit_compressed_objects_items)
    {
      char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


      // Need to handle the conversion of numeric overlay
      // chars to "a-j" here.
      if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
      {
        temp_overlay = last_obj_overlay + 'a';
      }

      xastir_snprintf(line, line_length, ";%-9s*%s%s%s%s",
                      last_object,
                      time,
                      compress_posit(ext_lat_str,
                                     (temp_overlay) ? temp_overlay : last_obj_grp,
                                     ext_lon_str,
                                     last_obj_sym,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank in this case
                      altitude,
                      signpost);

      comment_field_used = strlen(line) - 31;

    }
    else
    {

      xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s%s%s",
                      last_object,
                      time,
                      lat_str,
                      last_obj_grp,
                      lon_str,
                      last_obj_sym,
                      speed_course,
                      altitude,
                      signpost);

      comment_field_used = strlen(line) - 37;
    }

  }
  else if (DF_object_enabled)       // A DF'ing object of some type
  {
    if (Omni_antenna_enabled)
    {

      if (transmit_compressed_objects_items)
      {
        char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


        // Need to handle the conversion of numeric overlay
        // chars to "a-j" here.
        if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
        {
          temp_overlay = last_obj_overlay + 'a';
        }

        xastir_snprintf(line, line_length, ";%-9s*%s%sDFS%s/%s%s",
                        last_object,
                        time,
                        compress_posit(ext_lat_str,
                                       (temp_overlay) ? temp_overlay : last_obj_grp,
                                       ext_lon_str,
                                       last_obj_sym,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank in this case
                        object_shgd,
                        speed_course,
                        altitude);

        comment_field_used = strlen(line) - 31;

      }
      else
      {

        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%cDFS%s/%s%s",
                        last_object,
                        time,
                        lat_str,
                        last_obj_grp,
                        lon_str,
                        last_obj_sym,
                        object_shgd,
                        speed_course,
                        altitude);

        comment_field_used = strlen(line) - 37;
      }

    }
    else      // Beam Heading DFS object
    {
      if (strlen(speed_course) != 7)
        xastir_snprintf(speed_course,
                        sizeof(speed_course),
                        "000/000");

      temp_ptr = XmTextFieldGetString(ob_bearing_data);
      bearing = atoi(temp_ptr);
      XtFree(temp_ptr);

      if ( (bearing < 1) || (bearing > 360) )
      {
        bearing = 360;
      }

      if (transmit_compressed_objects_items)
      {
        char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


        // Need to handle the conversion of numeric overlay
        // chars to "a-j" here.
        if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
        {
          temp_overlay = last_obj_overlay + 'a';
        }

        xastir_snprintf(line, line_length, ";%-9s*%s%s/%03i/%s%s",
                        last_object,
                        time,
                        compress_posit(ext_lat_str,
                                       (temp_overlay) ? temp_overlay : last_obj_grp,
                                       ext_lon_str,
                                       last_obj_sym,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank in this case
                        bearing,
                        object_NRQ,
                        altitude);

        comment_field_used = strlen(line) - 31;

      }
      else
      {


        xastir_snprintf(line, line_length, ";%-9s*%s%s%c%s%c%s/%03i/%s%s",
                        last_object,
                        time,
                        lat_str,
                        last_obj_grp,
                        lon_str,
                        last_obj_sym,
                        speed_course,
                        bearing,
                        object_NRQ,
                        altitude);

        comment_field_used = strlen(line) - 37;
      }

    }
  }
  else      // Else it's a normal object
  {

    prob_min[0] = '\0';
    prob_max[0] = '\0';

    if (Probability_circles_enabled)
    {

      temp_ptr = XmTextFieldGetString(probability_data_min);
      xastir_snprintf(line, line_length, "%s", temp_ptr);
      XtFree(temp_ptr);

      //fprintf(stderr,"Probability min circle entered: %s\n", line);
      if (strlen(line) != 0)     // Probability circle data was entered
      {
        xastir_snprintf(prob_min, sizeof(prob_min), "Pmin%s,", line);
      }

      temp_ptr = XmTextFieldGetString(probability_data_max);
      xastir_snprintf(line, line_length, "%s", temp_ptr);
      XtFree(temp_ptr);

      //fprintf(stderr,"Probability max circle entered: %s\n", line);
      if (strlen(line) != 0)     // Probability circle data was entered
      {
        xastir_snprintf(prob_max, sizeof(prob_max), "Pmax%s,", line);
      }
    }

    if (transmit_compressed_objects_items)
    {
      char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


      // Need to handle the conversion of numeric overlay
      // chars to "a-j" here.
      if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
      {
        temp_overlay = last_obj_overlay + 'a';
      }

      xastir_snprintf(line,
                      line_length,
                      ";%-9s*%s%s%s%s%s",
                      last_object,
                      time,
                      compress_posit(ext_lat_str,
                                     (temp_overlay) ? temp_overlay : last_obj_grp,
                                     ext_lon_str,
                                     last_obj_sym,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank in this case
                      altitude,
                      prob_min,
                      prob_max);

      comment_field_used = strlen(line) - 31;

    }
    else
    {
      xastir_snprintf(line,
                      line_length,
                      ";%-9s*%s%s%c%s%c%s%s%s%s",
                      last_object,
                      time,
                      lat_str,
                      last_obj_overlay ? last_obj_overlay : last_obj_grp,
                      lon_str,
                      last_obj_sym,
                      speed_course,
                      altitude,
                      prob_min,
                      prob_max);

      comment_field_used = strlen(line) - 37;

    }
  }


  // We need to tack the comment on the end, but need to make
  // sure we don't go over the maximum length for an object.  The
  // maximum comment field is 43 chars.  "comment_field_used"
  // tells us how many chars of that field we've used up already
  // with other parameters before adding the comment chars to the
  // end.
  //
  //fprintf(stderr,"Comment: %s\n",comment);
  if (strlen(comment) != 0)
  {

    if (comment[0] == '}')
    {
      // May be a multipoint polygon string at the start of
      // the comment field.  Add a space before this special
      // character as multipoints have to start with " }" to
      // be valid.
      line[strlen(line) + 1] = '\0';
      line[strlen(line)] = ' ';

      comment_field_used++;
    }

    temp = 0;
//        while ( (strlen(line) < 80) && (temp < (int)strlen(comment)) ) {
    while ((comment_field_used < 43) && (temp < (int)strlen(comment)) )
    {
      //fprintf(stderr,"temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
      line[strlen(line) + 1] = '\0';
      line[strlen(line)] = comment[temp++];
      comment_field_used++;
    }
  }
  //fprintf(stderr,"line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for objects.

  return(1);
}





// Convert this eventually to populate a DataRow struct, then call
// data.c:Create_object_item_tx_string().  Right now we have a lot
// of code duplication between Setup_object_data, Setup_item_data,
// and Create_object_item_tx_string.
//
// Make sure to look at the "transmit_compressed_objects_items" variable
// to decide whether to send a compressed packet.
/*
 *  Setup APRS Information Field for Items
 */
int Setup_item_data(char *line, int line_length, DataRow *p_station)
{
  char lat_str[MAX_LAT];
  char lon_str[MAX_LONG];
  char ext_lat_str[20];   // Extended precision for base91 compression
  char ext_lon_str[20];   // Extended precision for base91 compression
  char comment[43+1];                 // max 43 characters of comment
  char complete_area_color[3];
  int complete_area_type;
  int lat_offset, lon_offset;
  char complete_corridor[6];
  char altitude[10];
  char speed_course[8];
  int speed;
  int course;
  int temp;
  long temp2;
  float temp3;
  char tempstr[MAX_CALLSIGN+1];
  char signpost[6];
  char prob_min[20+1];
  char prob_max[20+1];
  int bearing;
  char *temp_ptr;
  char *temp_ptr2;
  int DR = 0; // Dead-reckoning flag
  int comment_field_used = 0;   // Amount of comment field used up


  // If speed for the original object was non-zero, then we need
  // to snag the updated DR'ed position for the object.  Snag the
  // current DR position and build the strings we need // for
  // position transmit.
  //
  if (p_station != NULL)
  {

    speed = atoi(p_station->speed);

    if (speed > 0 && !doing_move_operation)
    {

      fetch_current_DR_strings(p_station,
                               lat_str,
                               lon_str,
                               ext_lat_str,
                               ext_lon_str);
      DR++;   // Set the dead-reckoning flag
    }

    // Keep the time current for our own items.
    p_station->sec_heard = sec_now();
    move_station_time(p_station,NULL);
  }

  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(line);
  //(void)to_upper(line);     Not per spec.  Don't use this.

  xastir_snprintf(tempstr,
                  sizeof(tempstr),
                  "%s",
                  line);

  if (strlen(line) == 1)  // Add two spaces (to make 3 minimum chars)
  {
    xastir_snprintf(line, line_length, "%s  ", tempstr);
  }

  else if (strlen(line) == 2) // Add one space (to make 3 minimum chars)
  {
    xastir_snprintf(line, line_length, "%s ", tempstr);
  }

  if (!valid_item(line))
  {
    return(0);
  }

  xastir_snprintf(last_object,
                  sizeof(last_object),
                  "%s",
                  line);


  if (!DR)    // We're not doing dead-reckoning, so proceed
  {

    temp_ptr = XmTextFieldGetString(object_lat_data_ns);
    if((char)toupper((int)temp_ptr[0]) == 'S')
    {
      line[0] = 'S';
    }
    else
    {
      line[0] = 'N';
    }
    XtFree(temp_ptr);

    // Check latitude for out-of-bounds
    temp_ptr = XmTextFieldGetString(object_lat_data_deg);
    temp = atoi(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp > 90) || (temp < 0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lat_data_min);
    temp3 = atof(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
    {
      return(0);
    }

    if ( (temp == 90) && (temp3 != 0.0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lat_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lat_data_min);
    xastir_snprintf(lat_str, sizeof(lat_str), "%02d%05.2f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//                atof(temp_ptr2) + 0.001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(object_lat_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lat_data_min);
    xastir_snprintf(ext_lat_str, sizeof(ext_lat_str), "%02d%05.3f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.0001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//                atof(temp_ptr2) + 0.0001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(object_lon_data_ew);
    if((char)toupper((int)temp_ptr[0]) == 'E')
    {
      line[0] = 'E';
    }
    else
    {
      line[0] = 'W';
    }
    XtFree(temp_ptr);

    // Check longitude for out-of-bounds
    temp_ptr = XmTextFieldGetString(object_lon_data_deg);
    temp = atoi(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp > 180) || (temp < 0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lon_data_min);
    temp3 = atof(temp_ptr);
    XtFree(temp_ptr);

    if ( (temp3 >= 60.0) || (temp3 < 0.0) )
    {
      return(0);
    }

    if ( (temp == 180) && (temp3 != 0.0) )
    {
      return(0);
    }

    temp_ptr = XmTextFieldGetString(object_lon_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lon_data_min);
    xastir_snprintf(lon_str, sizeof(lon_str), "%03d%05.2f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//                atof(temp_ptr2) + 0.001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);

    temp_ptr = XmTextFieldGetString(object_lon_data_deg);
    temp_ptr2 = XmTextFieldGetString(object_lon_data_min);
    xastir_snprintf(ext_lon_str, sizeof(ext_lon_str), "%03d%05.3f%c",
                    atoi(temp_ptr),
// An attempt was made to round here, adding 0.0001 to the minutes
// value.  Problems arise if it goes above 59 minutes as the degrees
// value would need to bump up also.  This then gets into problems
// at 90.0 degrees.  The correct method would be to convert it to
// decimal at a higher precision and then convert it back to DD
// MM.MM format.
//                atof(temp_ptr2) + 0.0001,
                    atof(temp_ptr2),
                    line[0]);
    XtFree(temp_ptr);
    XtFree(temp_ptr2);
  }


  temp_ptr = XmTextFieldGetString(object_group_data);
  last_obj_grp = temp_ptr[0];
  if(isalpha((int)last_obj_grp))
  {
    last_obj_grp = toupper((int)temp_ptr[0]);  // todo: toupper in dialog
  }
  XtFree(temp_ptr);

  // Check for overlay character
  if (last_obj_grp != '/' && last_obj_grp != '\\')
  {
    // Found an overlay character.  Check that it's within the
    // proper range
    if ( (last_obj_grp >= '0' && last_obj_grp <= '9')
         || (last_obj_grp >= 'A' && last_obj_grp <= 'Z') )
    {
      last_obj_overlay = last_obj_grp;
      last_obj_grp = '\\';
    }
    else
    {
      last_obj_overlay = '\0';
    }
  }
  else
  {
    last_obj_overlay = '\0';
  }

  temp_ptr = XmTextFieldGetString(object_symbol_data);
  last_obj_sym = temp_ptr[0];
  XtFree(temp_ptr);

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(comment);

  // Handle Generic Options

  // Speed/Course Fields
  temp_ptr = XmTextFieldGetString(ob_course_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  sprintf(speed_course,".../");   // Start with invalid-data string
  course = 0;
  if (strlen(line) != 0)      // Course was entered
  {
    // Need to check for 1 to three digits only, and 001-360 degrees)
    temp = atoi(line);
    if ( (temp >= 1) && (temp <= 360) )
    {
      xastir_snprintf(speed_course, sizeof(speed_course), "%03d/", temp);
      course = temp;
    }
    else if (temp == 0)       // Spec says 001 to 360 degrees...
    {
      sprintf(speed_course, "360/");
    }
  }
  temp_ptr = XmTextFieldGetString(ob_speed_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  speed = 0;
  if (strlen(line) != 0)   // Speed was entered (we only handle knots currently)
  {
    // Need to check for 1 to three digits, no alpha characters
    temp = atoi(line);
    if ( (temp >= 0) && (temp <= 999) )
    {
      xastir_snprintf(line, line_length, "%03d", temp);
      strncat(speed_course,
              line,
              sizeof(speed_course) - 1 - strlen(speed_course));
      speed = temp;
    }
    else
    {
      strncat(speed_course,
              "...",
              sizeof(speed_course) - 1 - strlen(speed_course));
    }
  }
  else      // No speed entered, blank it out
  {
    strncat(speed_course,
            "...",
            sizeof(speed_course) - 1 - strlen(speed_course));
  }
  if ( (speed_course[0] == '.') && (speed_course[4] == '.') )
  {
    speed_course[0] = '\0'; // No speed or course entered, so blank it
  }

  if (Area_object_enabled)
  {
    speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
  }

  // Altitude Field
  temp_ptr = XmTextFieldGetString(ob_altitude_data);
  xastir_snprintf(line, line_length, "%s", temp_ptr);
  XtFree(temp_ptr);

  //fprintf(stderr,"Altitude entered: %s\n", line);
  altitude[0] = '\0'; // Start with empty string
  if (strlen(line) != 0)     // Altitude was entered (we only handle feet currently)
  {
    // Need to check for all digits, and 1 to 6 digits
    if (isdigit((int)line[0]))
    {
      temp2 = atoi(line);
      if ((temp2 >= 0) && (temp2 <= 999999))
      {
        char temp_alt[20];
        xastir_snprintf(temp_alt, sizeof(temp_alt), "/A=%06ld",temp2);
        memcpy(altitude, temp_alt, sizeof(altitude));
        altitude[sizeof(altitude)-1] = '\0';  // Terminate string
      }
    }
  }

  // Handle Specific Options

  // Area Items
  if (Area_object_enabled)
  {
    if (Area_bright)    // Bright color
    {
      xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%2s",
                      Area_color);
    }
    else                  // Dim color
    {
      xastir_snprintf(complete_area_color, sizeof(complete_area_color), "%02.0f",
                      (float)(atoi(&Area_color[1]) + 8) );
      if ((Area_color[1] == '0') || (Area_color[1] == '1'))
      {
        complete_area_color[0] = '/';
      }
    }
    if ( (Area_filled) && (Area_type != 1) && (Area_type != 6) )
    {
      complete_area_type = Area_type + 5;
    }
    else      // Can't fill in a line
    {
      complete_area_type = Area_type;
    }
    temp_ptr = XmTextFieldGetString(ob_lat_offset_data);
    xastir_snprintf(line, line_length, "%s", temp_ptr);
    XtFree(temp_ptr);

    lat_offset = sqrt(atof(line));
    if (lat_offset > 99)
    {
      lat_offset = 99;
    }
    //fprintf(stderr,"Line: %s\tlat_offset: %d\n", line, lat_offset);
    temp_ptr = XmTextFieldGetString(ob_lon_offset_data);
    xastir_snprintf(line, line_length, "%s", temp_ptr);
    XtFree(temp_ptr);

    lon_offset = sqrt(atof(line));
    if (lon_offset > 99)
    {
      lon_offset = 99;
    }
    //fprintf(stderr,"Line: %s\tlon_offset: %d\n", line, lon_offset);

    // Corridor
    complete_corridor[0] = '\0';
    if ( (Area_type == 1) || (Area_type == 6) )
    {

      temp_ptr = XmTextFieldGetString(ob_corridor_data);
      xastir_snprintf(line, line_length, "%s", temp_ptr);
      XtFree(temp_ptr);

      if (strlen(line) != 0)      // We have a line and some corridor data
      {
        // Need to check for 1 to three digits only
        temp = atoi(line);
        if ( (temp > 0) && (temp <= 999) )
        {
          xastir_snprintf(complete_corridor, sizeof(complete_corridor), "{%d}", temp);
          //fprintf(stderr,"%s\n",complete_corridor);
        }
      }
    }

    if (transmit_compressed_objects_items)
    {
      char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


      // Need to handle the conversion of numeric overlay
      // chars to "a-j" here.
      if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
      {
        temp_overlay = last_obj_overlay + 'a';
      }

      xastir_snprintf(line, line_length, ")%s!%s%1d%02d%2s%02d%s%s%s",
                      last_object,
                      compress_posit(ext_lat_str,
                                     (temp_overlay) ? temp_overlay : last_obj_grp,
                                     ext_lon_str,
                                     last_obj_sym,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank in this case
                      complete_area_type,
                      lat_offset,
                      complete_area_color,
                      lon_offset,
                      speed_course,
                      complete_corridor,
                      altitude);

      comment_field_used = strlen(line) - 15 - strlen(last_object);

    }
    else
    {

      xastir_snprintf(line, line_length, ")%s!%s%c%s%c%1d%02d%2s%02d%s%s%s",
                      last_object,
                      lat_str,
                      last_obj_grp,
                      lon_str,
                      last_obj_sym,
                      complete_area_type,
                      lat_offset,
                      complete_area_color,
                      lon_offset,
                      speed_course,
                      complete_corridor,
                      altitude);

      comment_field_used = strlen(line) - 21 - strlen(last_object);
    }

  }
  else if (Signpost_object_enabled)
  {
    temp_ptr = XmTextFieldGetString(signpost_data);
    xastir_snprintf(line, line_length, "%s", temp_ptr);
    XtFree(temp_ptr);

    //fprintf(stderr,"Signpost entered: %s\n", line);
    if (strlen(line) != 0)     // Signpost data was entered
    {
      // Need to check for between one and three characters
      temp = strlen(line);
      if ( (temp >= 0) && (temp <= 3) )
      {
        xastir_snprintf(signpost, sizeof(signpost), "{%s}", line);
      }
      else
      {
        signpost[0] = '\0';
      }
    }
    else      // No signpost data entered, blank it out
    {
      signpost[0] = '\0';
    }

    if (transmit_compressed_objects_items)
    {
      char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


      // Need to handle the conversion of numeric overlay
      // chars to "a-j" here.
      if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
      {
        temp_overlay = last_obj_overlay + 'a';
      }

      xastir_snprintf(line, line_length, ")%s!%s%s%s",
                      last_object,
                      compress_posit(ext_lat_str,
                                     (temp_overlay) ? temp_overlay : last_obj_grp,
                                     ext_lon_str,
                                     last_obj_sym,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank in this case
                      altitude,
                      signpost);

      comment_field_used = strlen(line) - 15 - strlen(last_object);

    }
    else
    {

      xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s%s%s",
                      last_object,
                      lat_str,
                      last_obj_grp,
                      lon_str,
                      last_obj_sym,
                      speed_course,
                      altitude,
                      signpost);

      comment_field_used = strlen(line) - 21 - strlen(last_object);
    }

  }
  else if (DF_object_enabled)       // A DF'ing item of some type
  {
    if (Omni_antenna_enabled)
    {

      if (transmit_compressed_objects_items)
      {
        char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


        // Need to handle the conversion of numeric overlay
        // chars to "a-j" here.
        if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
        {
          temp_overlay = last_obj_overlay + 'a';
        }

        xastir_snprintf(line, line_length, ")%s!%sDFS%s/%s%s",
                        last_object,
                        compress_posit(ext_lat_str,
                                       (temp_overlay) ? temp_overlay : last_obj_grp,
                                       ext_lon_str,
                                       last_obj_sym,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank in this case
                        object_shgd,
                        speed_course,
                        altitude);

        comment_field_used = strlen(line) - 15 - strlen(last_object);

      }
      else
      {

        xastir_snprintf(line, line_length, ")%s!%s%c%s%cDFS%s/%s%s",
                        last_object,
                        lat_str,
                        last_obj_grp,
                        lon_str,
                        last_obj_sym,
                        object_shgd,
                        speed_course,
                        altitude);

        comment_field_used = strlen(line) - 21 - strlen(last_object);

      }

    }
    else      // Beam Heading DFS item
    {
      if (strlen(speed_course) != 7)
        xastir_snprintf(speed_course,
                        sizeof(speed_course),
                        "000/000");

      temp_ptr = XmTextFieldGetString(ob_bearing_data);
      bearing = atoi(temp_ptr);
      XtFree(temp_ptr);

      if ( (bearing < 1) || (bearing > 360) )
      {
        bearing = 360;
      }

      if (transmit_compressed_objects_items)
      {
        char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


        // Need to handle the conversion of numeric overlay
        // chars to "a-j" here.
        if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
        {
          temp_overlay = last_obj_overlay + 'a';
        }

        xastir_snprintf(line, line_length, ")%s!%s/%03i/%s%s",
                        last_object,
                        compress_posit(ext_lat_str,
                                       (temp_overlay) ? temp_overlay : last_obj_grp,
                                       ext_lon_str,
                                       last_obj_sym,
                                       course,
                                       speed,  // In knots
                                       ""),    // PHG, must be blank in this case
                        bearing,
                        object_NRQ,
                        altitude);

        comment_field_used = strlen(line) - 15 - strlen(last_object);

      }
      else
      {

        xastir_snprintf(line, line_length, ")%s!%s%c%s%c%s/%03i/%s%s",
                        last_object,
                        lat_str,
                        last_obj_grp,
                        lon_str,
                        last_obj_sym,
                        speed_course,
                        bearing,
                        object_NRQ,
                        altitude);

        comment_field_used = strlen(line) - 21 - strlen(last_object);
      }

    }
  }
  else      // Else it's a normal item
  {

    prob_min[0] = '\0';
    prob_max[0] = '\0';

    if (Probability_circles_enabled)
    {

      temp_ptr = XmTextFieldGetString(probability_data_min);
      xastir_snprintf(line, line_length, "%s", temp_ptr);
      XtFree(temp_ptr);

      //fprintf(stderr,"Probability min circle entered: %s\n",
      //line);
      if (strlen(line) != 0)     // Probability circle data was entered
      {
        xastir_snprintf(prob_min, sizeof(prob_min), "Pmin%s,", line);
      }

      temp_ptr = XmTextFieldGetString(probability_data_max);
      xastir_snprintf(line, line_length, "%s", temp_ptr);
      XtFree(temp_ptr);

      //fprintf(stderr,"Probability max circle entered: %s\n",
      //line);
      if (strlen(line) != 0)     // Probability circle data was entered
      {
        xastir_snprintf(prob_max, sizeof(prob_max), "Pmax%s,", line);
      }
    }


    if (transmit_compressed_objects_items)
    {
      char temp_overlay = last_obj_overlay;


// Need to compute "csT" at some point and add it to the object.
// Until we do that we'll have no course/speed/altitude.  Looks like
// we can have course/speed or altitude, but not both.  Must have to
// add the "/A=000123" stuff to the end if we want both.
//
// If we have course and/or speed, use course/speed csT bytes.  If
// no course/speed but we have altitude, use altitude csT bytes.  We
// can cheat right now and just always use course/speed, adding
// altitude with the altitude extension.  Not as efficient, but it
// gets the job done.
//
// Later we should change compress_posit() to accept an altitude
// parameter, and have it decide which csT set of bytes to add, and
// whether to add altitude as an uncompressed extension if
// necessary.


      // Need to handle the conversion of numeric overlay
      // chars to "a-j" here.
      if (last_obj_overlay >= '0' && last_obj_overlay <= '9')
      {
        temp_overlay = last_obj_overlay + 'a';
      }

      xastir_snprintf(line,
                      line_length,
                      ")%s!%s%s%s%s",
                      last_object,
                      compress_posit(ext_lat_str,
                                     (temp_overlay) ? temp_overlay : last_obj_grp,
                                     ext_lon_str,
                                     last_obj_sym,
                                     course,
                                     speed,  // In knots
                                     ""),    // PHG, must be blank in this case
                      altitude,
                      prob_min,
                      prob_max);

      comment_field_used = strlen(line) - 15 - strlen(last_object);

    }
    else
    {
      xastir_snprintf(line,
                      line_length,
                      ")%s!%s%c%s%c%s%s%s%s",
                      last_object,
                      lat_str,
                      last_obj_overlay ? last_obj_overlay : last_obj_grp,
                      lon_str,
                      last_obj_sym,
                      speed_course,
                      altitude,
                      prob_min,
                      prob_max);

      comment_field_used = strlen(line) - 21 - strlen(last_object);

    }
  }


  // We need to tack the comment on the end, but need to make
  // sure we don't go over the maximum length for an item.
  //fprintf(stderr,"Comment: %s\n",comment);
  if (strlen(comment) != 0)
  {

    if (comment[0] == '}')
    {
      // May be a multipoint polygon string at the start of
      // the comment field.  Add a space before this special
      // character as multipoints have to start with " }" to
      // be valid.
      line[strlen(line) + 1] = '\0';
      line[strlen(line)] = ' ';
      comment_field_used++;
    }

    temp = 0;
//        while ( (strlen(line) < (64 + strlen(last_object))) && (temp < (int)strlen(comment)) ) {
    while ( (comment_field_used < 43) && (temp < (int)strlen(comment)) )
    {
      //fprintf(stderr,"temp: %d->%d\t%c\n", temp, strlen(line), comment[temp]);
      line[strlen(line) + 1] = '\0';
      line[strlen(line)] = comment[temp++];
      comment_field_used++;
    }
  }
  //fprintf(stderr,"line: %s\n",line);

// NOTE:  Compressed mode will be shorter still.  Account
// for that when compressed mode is implemented for items.

  return(1);
}






/*
 *  Set an Object
 */
void Object_change_data_set(Widget widget, XtPointer clientData, XtPointer UNUSED(callData) )
{
  char line[43+1+40];                 // ???
  DataRow *p_station = global_parameter1;

  //fprintf(stderr,"Object_change_data_set\n");

  // p_station will be NULL if the object is new.
  if (Setup_object_data(line, sizeof(line), p_station))
  {

    // Update this object in our save file
    log_object_item(line,0,last_object);

    // Set up the timer properly for the decaying algorithm
    if (p_station != NULL)
    {
      p_station->transmit_time_increment = OBJECT_CHECK_RATE;
      p_station->last_transmit_time = sec_now();

      // Keep the time current for our own objects.
      p_station->sec_heard = sec_now();
      move_station_time(p_station,NULL);

//            p_station->last_modified_time = sec_now(); // For dead-reckoning
//fprintf(stderr,"Object_change_data_set(): Setting transmit increment to %d\n", OBJECT_CHECK_RATE);
    }

    if (object_tx_disable || transmit_disable)
    {
      output_my_data(line,-1,0,1,0,NULL);  // Local loopback only, not igating
    }
    else
    {
      output_my_data(line,-1,0,0,0,NULL);  // Transmit/loopback object data, not igating
    }

    sched_yield();                  // Wait for transmitted data to get processed
    Object_destroy_shell(widget,clientData,NULL);
    // Getting a segfault here on a Move operation, so just
    // comment it out.  A redraw will occur shortly anyway.
    //redraw_symbols(da);
    (void)XCopyArea(XtDisplay(da),
                    pixmap_final,
                    XtWindow(da),
                    gc,
                    0,
                    0,
                    (unsigned int)screen_width,
                    (unsigned int)screen_height,
                    0,
                    0);
  }
  else
  {
    // error message
    popup_message_always(langcode("POPEM00022"),langcode("POPEM00027"));
  }
}





/*
 *  Set an Item
 */
void Item_change_data_set(Widget widget, XtPointer clientData, XtPointer UNUSED(callData) )
{
  char line[43+1+40];                 // ???
  DataRow *p_station = global_parameter1;


  if (Setup_item_data(line,sizeof(line), p_station))
  {

    // Update this item in our save file
    log_object_item(line,0,last_object);

    // Set up the timer properly for the decaying algorithm
    if (p_station != NULL)
    {
      p_station->transmit_time_increment = OBJECT_CHECK_RATE;
      p_station->last_transmit_time = sec_now();

      // Keep the time current for our own items.
      p_station->sec_heard = sec_now();
      move_station_time(p_station,NULL);

//            p_station->last_modified_time = sec_now(); // For dead-reckoning
//fprintf(stderr,"Item_change_data_set(): Setting transmit increment to %d\n", OBJECT_CHECK_RATE);
    }

    if (object_tx_disable || transmit_disable)
    {
      output_my_data(line,-1,0,1,0,NULL);  // Local loopback only, not igating
    }
    else
    {
      output_my_data(line,-1,0,0,0,NULL);  // Transmit/loopback item data, not igating
    }

    sched_yield();                  // Wait for transmitted data to get processed
    Object_destroy_shell(widget,clientData,NULL);
    // Getting a segfault here on a Move operation, so just
    // comment it out.  A redraw will occur shortly anyway.
    //redraw_symbols(da);
    (void)XCopyArea(XtDisplay(da),
                    pixmap_final,
                    XtWindow(da),
                    gc,
                    0,
                    0,
                    (unsigned int)screen_width,
                    (unsigned int)screen_height,
                    0,
                    0);
  }
  else
  {
    // error message
    popup_message_always(langcode("POPEM00022"),langcode("POPEM00027"));
  }
}





// Check the name of a new Object.  If it already exists in our
// database, warn the user.  Confirmation dialog to continue?
//
void Object_confirm_data_set(Widget widget, XtPointer clientData, XtPointer callData)
{
  char *temp_ptr;
  char line[MAX_CALLSIGN+1];
  DataRow *p_station;


  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(line,
                  sizeof(line),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_leading_spaces(line);
  (void)remove_trailing_spaces(line);

  //fprintf(stderr,"Object name:%s\n", line);

  // We have the name now.  Check it against our database of
  // stations/objects/items.  Do an exact match.
  //
  if (search_station_name(&p_station,line,1)
      && (p_station->flag & ST_ACTIVE))
  {
    // Found a live object with that name.  Don't allow Object
    // creation.  Bring up a warning message instead.
    popup_message_always(langcode("POPEM00035"), langcode("POPEM00038"));
  }
  else
  {

    // Not found.  Allow the Object to be created.
    Object_change_data_set(widget, clientData, callData);
  }
}





// Check the name of a new Item.  If it already exists in our
// database, warn the user.  Confirmation dialog to continue?
//
void Item_confirm_data_set(Widget widget, XtPointer clientData, XtPointer callData)
{
  char *temp_ptr;
  char line[MAX_CALLSIGN+1];
  DataRow *p_station;


  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(line,
                  sizeof(line),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_leading_spaces(line);
  (void)remove_trailing_spaces(line);

  //fprintf(stderr,"Item name:%s\n", line);

  // We have the name now.  Check it against our database of
  // stations/objects/items.  Do an exact match.
  //
  if (search_station_name(&p_station,line,1)
      && (p_station->flag & ST_ACTIVE))
  {
    // Found a live object with that name.  Don't allow Object
    // creation.  Bring up a warning message instead.
    popup_message_always(langcode("POPEM00035"), langcode("POPEM00038"));
  }
  else
  {

    // Not found.  Allow the Item to be created.
    Item_change_data_set(widget, clientData, callData);
  }
}





/*
 *  Delete an Object
 */
void Object_change_data_del(Widget widget, XtPointer clientData, XtPointer UNUSED(callData) )
{
  char line[43+1+40];                 // ???
  DataRow *p_station = global_parameter1;


  if (Setup_object_data(line, sizeof(line), p_station))
  {

    line[10] = '_';                         // mark as deleted object

    // Update this object in our save file, then comment all
    // instances out in the file.
    log_object_item(line,1,last_object);

    // Set up the timer properly for the decaying algorithm
    if (p_station != NULL)
    {
      p_station->transmit_time_increment = OBJECT_CHECK_RATE;
      p_station->last_transmit_time = sec_now();
//            p_station->last_modified_time = sec_now(); // For dead-reckoning
//fprintf(stderr,"Object_change_data_del(): Setting transmit increment to %d\n", OBJECT_CHECK_RATE);
    }

    if (object_tx_disable || transmit_disable)
    {
      output_my_data(line,-1,0,1,0,NULL);  // Local loopback only, not igating
    }
    else
    {
      output_my_data(line,-1,0,0,0,NULL);  // Transmit object data, not igating
    }

    Object_destroy_shell(widget,clientData,NULL);
  }
}





/*
 *  Delete an Item
 */
void Item_change_data_del(Widget widget, XtPointer clientData, XtPointer UNUSED(callData) )
{
  char line[43+1+40];                 // ???
  int i, done;
  DataRow *p_station = global_parameter1;


  if (Setup_item_data(line,sizeof(line), p_station))
  {

    done = 0;
    i = 0;
    while ( (!done) && (i < 11) )
    {
      if (line[i] == '!')
      {
        line[i] = '_';          // mark as deleted object
        done++;                 // Exit from loop
      }
      i++;
    }

    // Update this item in our save file, then comment all
    // instances out in the file.
    log_object_item(line,1,last_object);

    // Set up the timer properly for the decaying algorithm
    if (p_station != NULL)
    {
      p_station->transmit_time_increment = OBJECT_CHECK_RATE;
      p_station->last_transmit_time = sec_now();
//            p_station->last_modified_time = sec_now(); // For dead-reckoning
//fprintf(stderr,"Item_change_data_del(): Setting transmit increment to %d\n", OBJECT_CHECK_RATE);
    }

    if (object_tx_disable || transmit_disable)
    {
      output_my_data(line,-1,0,1,0,NULL);  // Local loopback only, not igating
    }
    else
    {
      output_my_data(line,-1,0,0,0,NULL);  // Transmit item data, not igating
    }

    Object_destroy_shell(widget,clientData,NULL);
  }
}





/*
 *  Select a symbol graphically
 */
void Ob_change_symbol(Widget widget, XtPointer clientData, XtPointer callData)
{
  //fprintf(stderr,"Trying to change a symbol\n");
  symbol_change_requested_from = 2;   // Tell Select_symbol who to return the data to
  Select_symbol(widget, clientData, callData);
}





/*
 *  Update symbol picture for changed symbol or table
 */
void updateObjectPictureCallback(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData) )
{
  char table, overlay;
  char symb, group;
  char *temp_ptr;


  XtVaSetValues(object_icon, XmNlabelPixmap, Ob_icon0, NULL);         // clear old icon
  XtManageChild(object_icon);

  temp_ptr = XmTextFieldGetString(object_group_data);
  group = temp_ptr[0];
  XtFree(temp_ptr);

  temp_ptr = XmTextFieldGetString(object_symbol_data);
  symb  = temp_ptr[0];
  XtFree(temp_ptr);

  if (group == '/' || group == '\\')
  {
    // No overlay character
    table   = group;
    overlay = ' ';
  }
  else
  {
    // Found overlay character.  Check that it's a valid
    // overlay.
    if ( (group >= '0' && group <= '9')
         || (group >= 'A' && group <= 'Z') )
    {
      // Valid overlay character
      table   = '\\';
      overlay = group;
    }
    else
    {
      // Bad overlay character
      table = '\\';
      overlay = ' ';
    }
  }
  symbol(object_icon,0,table,symb,overlay,Ob_icon,0,0,0,' ');         // create icon

  XtVaSetValues(object_icon,XmNlabelPixmap,Ob_icon,NULL);             // draw new icon
  XtManageChild(object_icon);
}





// Handler for "Signpost" toggle button
void Signpost_object_toggle( Widget widget, XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
  char temp_data[40];
  char comment[43+1];     // max 43 characters of comment
  char signpost_name[10];
  char *temp_ptr;


  // Save name and comment fields temporarily
  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(signpost_name,
                  sizeof(signpost_name),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(signpost_name);

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(comment);


  if(state->set)
  {
    Signpost_object_enabled = 1;
    Area_object_enabled = 0;
    DF_object_enabled = 0;
    Map_View_object_enabled = 0;
    Probability_circles_enabled = 0;

    //fprintf(stderr,"Signpost Objects are ENABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );

    XmToggleButtonSetState(area_toggle, FALSE, FALSE);
    XmToggleButtonSetState(df_bearing_toggle, FALSE, FALSE);
    XmToggleButtonSetState(map_view_toggle, FALSE, FALSE);
    XmToggleButtonSetState(probabilities_toggle, FALSE, FALSE);

    temp_data[0] = '\\';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = 'm';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,FALSE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }
  else
  {
    Signpost_object_enabled = 0;

    //fprintf(stderr,"Signpost Objects are DISABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,TRUE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }

  // Restore name and comment fields
  XmTextFieldSetString(object_name_data,signpost_name);
  XmTextFieldSetString(object_comment_data,comment);
}





// Handler for "Probability Circles" toggle button
void Probability_circle_toggle( Widget widget, XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
  char temp_data[40];
  char comment[43+1];     // max 43 characters of comment
  char signpost_name[10];
  char *temp_ptr;


  // Save name and comment fields temporarily
  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(signpost_name,
                  sizeof(signpost_name),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(signpost_name);

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(comment);


  if(state->set)
  {
    Signpost_object_enabled = 0;
    Area_object_enabled = 0;
    DF_object_enabled = 0;
    Map_View_object_enabled = 0;
    Probability_circles_enabled = 1;

    //fprintf(stderr,"Probability Circles are ENABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );

    XmToggleButtonSetState(area_toggle, FALSE, FALSE);
    XmToggleButtonSetState(df_bearing_toggle, FALSE, FALSE);
    XmToggleButtonSetState(map_view_toggle, FALSE, FALSE);
    XmToggleButtonSetState(signpost_toggle, FALSE, FALSE);

    // Set to hiker symbol by default, but can be changed by
    // user to something else.
    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '[';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }
  else
  {
    Probability_circles_enabled = 0;

    //fprintf(stderr,"Signpost Objects are DISABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,TRUE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }

  // Restore name and comment fields
  XmTextFieldSetString(object_name_data,signpost_name);
  XmTextFieldSetString(object_comment_data,comment);
}





// Handler for "Enable Area Type" toggle button
void  Area_object_toggle( Widget widget, XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
  char temp_data[40];
  char comment[43+1];     // max 43 characters of comment
  char signpost_name[10];
  char *temp_ptr;


  // Save name and comment fields temporarily
  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(signpost_name,
                  sizeof(signpost_name),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(signpost_name);

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(comment);


  if(state->set)
  {
    Area_object_enabled = 1;
    Signpost_object_enabled = 0;
    DF_object_enabled = 0;
    Map_View_object_enabled = 0;
    Probability_circles_enabled = 0;

    //fprintf(stderr,"Area Objects are ENABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    XmToggleButtonSetState(signpost_toggle, FALSE, FALSE);
    XmToggleButtonSetState(df_bearing_toggle, FALSE, FALSE);
    XmToggleButtonSetState(map_view_toggle, FALSE, FALSE);
    XmToggleButtonSetState(probabilities_toggle, FALSE, FALSE);

    XtSetSensitive(ob_speed,FALSE);
    XtSetSensitive(ob_speed_data,FALSE);
    XtSetSensitive(ob_course,FALSE);
    XtSetSensitive(ob_course_data,FALSE);

    temp_data[0] = '\\';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = 'l';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,FALSE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }
  else
  {
    Area_object_enabled = 0;

    //fprintf(stderr,"Area Objects are DISABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    XtSetSensitive(ob_speed,TRUE);
    XtSetSensitive(ob_speed_data,TRUE);
    XtSetSensitive(ob_course,TRUE);
    XtSetSensitive(ob_course_data,TRUE);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,TRUE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }

  // Restore name and comment fields
  XmTextFieldSetString(object_name_data,signpost_name);
  XmTextFieldSetString(object_comment_data,comment);
}





// Handler for "DF Bearing Object" toggle button
void  DF_bearing_object_toggle( Widget widget, XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
  char temp_data[40];
  char comment[43+1];     // max 43 characters of comment
  char signpost_name[10];
  char *temp_ptr;


  // Save name and comment fields temporarily
  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(signpost_name,
                  sizeof(signpost_name),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(signpost_name);

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);
  (void)remove_trailing_spaces(comment);


  if(state->set)
  {
    Area_object_enabled = 0;
    Signpost_object_enabled = 0;
    DF_object_enabled = 1;
    Map_View_object_enabled = 0;
    Probability_circles_enabled = 0;

    //fprintf(stderr,"DF Objects are ENABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    XmToggleButtonSetState(signpost_toggle, FALSE, FALSE);
    XmToggleButtonSetState(area_toggle, FALSE, FALSE);
    XmToggleButtonSetState(map_view_toggle, FALSE, FALSE);
    XmToggleButtonSetState(probabilities_toggle, FALSE, FALSE);

    XtSetSensitive(ob_speed,TRUE);
    XtSetSensitive(ob_speed_data,TRUE);
    XtSetSensitive(ob_course,TRUE);
    XtSetSensitive(ob_course_data,TRUE);
    XtSetSensitive(frameomni,FALSE);
    XtSetSensitive(framebeam,FALSE);
    Omni_antenna_enabled = 0;
    Beam_antenna_enabled = 0;

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '\\';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,FALSE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }
  else
  {
    DF_object_enabled = 0;

    //fprintf(stderr,"DF Objects are DISABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,TRUE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }

  // Restore name and comment fields
  XmTextFieldSetString(object_name_data,signpost_name);
  XmTextFieldSetString(object_comment_data,comment);
}





// Handler for "Map View Object" toggle button
void  Map_View_object_toggle( Widget widget, XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;
  char temp_data[40];
  char comment[43+1];     // max 43 characters of comment
  char signpost_name[10];
  char *temp_ptr;


  // Save name and comment fields temporarily
  temp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(signpost_name,
                  sizeof(signpost_name),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);

  (void)remove_trailing_spaces(signpost_name);

  temp_ptr = XmTextFieldGetString(object_comment_data);
  xastir_snprintf(comment,
                  sizeof(comment),
                  "%s",
                  temp_ptr);
  XtFree(temp_ptr);
  (void)remove_trailing_spaces(comment);


  if(state->set)
  {
    // Make a bunch of the fields insensitive that we don't use
    // here.

    Area_object_enabled = 0;
    Signpost_object_enabled = 0;
    DF_object_enabled = 0;
    Map_View_object_enabled = 1;
    Probability_circles_enabled = 0;

    //fprintf(stderr,"Map View Objects are ENABLED\n");


// Make a bunch of the fields insensitive that we don't use here?


    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );

    XmToggleButtonSetState(signpost_toggle, FALSE, FALSE);
    XmToggleButtonSetState(area_toggle, FALSE, FALSE);
    XmToggleButtonSetState(df_bearing_toggle, FALSE, FALSE);
    XmToggleButtonSetState(probabilities_toggle, FALSE, FALSE);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = 'E'; // Eyeball symbol
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,FALSE);
    XtSetSensitive(ob_option_frame,FALSE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }
  else
  {
    Map_View_object_enabled = 0;

    //fprintf(stderr,"Map View Objects are DISABLED\n");

    // Call Set_Del_Object again, causing it to redraw with the new options.
    //Set_Del_Object( widget, clientData, callData );
    Set_Del_Object( widget, global_parameter1, global_parameter2 );


    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_group_data,temp_data);

    temp_data[0] = '/';
    temp_data[1] = '\0';
    XmTextFieldSetString(object_symbol_data,temp_data);

    XtSetSensitive(ob_frame,TRUE);
    XtSetSensitive(ob_option_frame,TRUE);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);
  }

  // Restore name and comment fields
  XmTextFieldSetString(object_name_data,signpost_name);

  // Don't want to restore the comment if it is a Map View object,
  // as Set_Del_Object() changes that field in that case.
  if (!Map_View_object_enabled)
  {
    XmTextFieldSetString(object_comment_data,comment);
  }
}





/* Area object type radio buttons */
void Area_type_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    Area_type = atoi(which);    // Set to shape desired
    if ( (Area_type == 1) || (Area_type == 6) )     // If either line type
    {
      //fprintf(stderr,"Line type: %d\n", Area_type);
      XtSetSensitive(ob_corridor,TRUE);
      XtSetSensitive(ob_corridor_data,TRUE);
      XtSetSensitive(ob_corridor_miles,TRUE);
      XtSetSensitive(open_filled_toggle,FALSE);
    }
    else    // Not line type
    {
      //fprintf(stderr,"Not line type: %d\n", Area_type);
      XtSetSensitive(ob_corridor,FALSE);
      XtSetSensitive(ob_corridor_data,FALSE);
      XtSetSensitive(ob_corridor_miles,FALSE);
      XtSetSensitive(open_filled_toggle,TRUE);
    }
  }
  else
  {
    Area_type = 0;  // Open circle
    //fprintf(stderr,"Type zero\n");
  }
  //fprintf(stderr,"Area type: %d\n", Area_type);
}





/* Area object color radio buttons */
void Area_color_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    Area_color[0] = which[0];   // Set to color desired.
    Area_color[1] = which[1];
    Area_color[2] = '\0';
  }
  else
  {
    Area_color[0] = '/';
    Area_color[1] = '0';    // Black
    Area_color[2] = '\0';
  }
  //fprintf(stderr,"Area color: %s\n", Area_color);
}





/* Area bright color enable button */
void Area_bright_dim_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    Area_bright = atoi(which);
    //fprintf(stderr,"Bright colors are ENABLED: %d\n", Area_bright);
  }
  else
  {
    Area_bright = 0;
    //fprintf(stderr,"Bright colors are DISABLED: %d\n", Area_bright);
  }
}





/* Area filled enable button */
void Area_open_filled_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    Area_filled = atoi(which);
    //fprintf(stderr,"Filled shapes ENABLED: %d\n", Area_filled);
  }
  else
  {
    Area_filled = 0;
    //fprintf(stderr,"Filled shapes DISABLED: %d\n", Area_filled);
  }
}





// Handler for "Omni Antenna" toggle button
void  Omni_antenna_toggle( Widget UNUSED(widget), XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    //fprintf(stderr,"Omni Antenna ENABLED\n");
    XmToggleButtonSetState(beam_antenna_toggle, FALSE, FALSE);
    XtSetSensitive(frameomni,TRUE);
    XtSetSensitive(framebeam,FALSE);
    Omni_antenna_enabled = 1;
    Beam_antenna_enabled = 0;
  }
  else
  {
    //fprintf(stderr,"Omni Antenna DISABLED\n");
    XtSetSensitive(frameomni,FALSE);
    Omni_antenna_enabled = 0;
  }
}





// Handler for "Beam Antenna" toggle button
void  Beam_antenna_toggle( Widget UNUSED(widget), XtPointer UNUSED(clientData), XtPointer callData)
{
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    //fprintf(stderr,"Beam Antenna ENABLED\n");
    XmToggleButtonSetState(omni_antenna_toggle, FALSE, FALSE);
    XtSetSensitive(frameomni,FALSE);
    XtSetSensitive(framebeam,TRUE);
    Omni_antenna_enabled = 0;
    Beam_antenna_enabled = 1;
  }
  else
  {
    //fprintf(stderr,"Beam Antenna DISABLED\n");
    XtSetSensitive(framebeam,FALSE);
    Beam_antenna_enabled = 0;
  }
}





/* Object signal radio buttons */
void Ob_signal_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    object_shgd[0] = which[0];  // Set to signal quality heard
  }
  else
  {
    object_shgd[0] = '0';       // 0 (Signal not heard)
  }
  object_shgd[4] = '\0';

  //fprintf(stderr,"SHGD: %s\n",object_shgd);
}





/* Object height radio buttons */
void Ob_height_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    object_shgd[1] = which[0];  // Set to height desired
  }
  else
  {
    object_shgd[1] = '0';       // 0 (10ft HAAT)
  }
  object_shgd[4] = '\0';

  //fprintf(stderr,"SHGD: %s\n",object_shgd);
}





/* Object gain radio buttons */
void Ob_gain_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    object_shgd[2] = which[0];  // Set to antenna gain desired
  }
  else
  {
    object_shgd[2] = '0';       // 0dB gain
  }
  object_shgd[4] = '\0';

  //fprintf(stderr,"SHGD: %s\n",object_shgd);
}





/* Object directivity radio buttons */
void Ob_directivity_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    object_shgd[3] = which[0];  // Set to antenna pattern desired
  }
  else
  {
    object_shgd[3] = '0';       // Omni-directional pattern
  }
  object_shgd[4] = '\0';

  //fprintf(stderr,"SHGD: %s\n",object_shgd);
}





/* Object beamwidth radio buttons */
void Ob_width_toggle( Widget UNUSED(widget), XtPointer clientData, XtPointer callData)
{
  char *which = (char *)clientData;
  XmToggleButtonCallbackStruct *state = (XmToggleButtonCallbackStruct *)callData;

  if(state->set)
  {
    object_NRQ[2] = which[0];  // Set to antenna beamwidth desired
  }
  else
  {
    object_NRQ[2] = '0';       // Beamwidth = "Useless"
  }
  object_NRQ[3] = '\0';

  //fprintf(stderr,"NRQ: %s\n", object_NRQ);
}





/* populate predefined object (SAR) struct */
void Populate_predefined_objects(predefinedObject *predefinedObjects)
{
  // The number of objects you are defining below must be
  // exactly equal to number_of_predefined_objects
  // and less than MAX_NUMBER_OF_PREDEFINED_OBJECTS.
  // using counter j for this seems inelegant **

  // A set of predefined SAR objects are hardcoded and used by default
  // other sets of predefined objects (SAR in km, public service event,
  // and user defined objects) can be loaded from a file.
  //
  // Detailed instructions for the format of the files can be found in
  // the two example files provided: predefined_SAR.sys and
  // predefined_EVENT.sys
  char predefined_object_definition_file[263];
  int read_file_ok = 0;
  int line_max_length = 255;
  int object_read_ok = 0;
  char line[line_max_length];
  char *value;
  char *variable;
  FILE *fp_file;
  int j = 0;
  char error_correct_location[300];
  char predef_obj_path[MAX_VALUE];
#ifdef OBJECT_DEF_FILE_USER_BASE
  char temp_file_path[MAX_VALUE];
#endif


  xastir_snprintf(line,sizeof(line),"%s","\0");
  xastir_snprintf(predefined_object_definition_file,sizeof(predefined_object_definition_file),"config/%s",predefined_object_definition_filename);

  get_user_base_dir(predefined_object_definition_file, predef_obj_path,
                    sizeof(predef_obj_path));

  number_of_predefined_objects = 0;

  if (predefined_menu_from_file == 1 )
  {
    // Check to see if a file containing predefined object definitions
    // exists, if it does, open it and try to read the definitions
    // if this fails, use the hardcoded SAR default instead.
//        fprintf(stderr,"Checking for predefined objects menu file\n");
#ifdef OBJECT_DEF_FILE_USER_BASE
    if (filethere(predef_obj_path))
    {
      fp_file = fopen(predef_obj_path,"r");
#else   // OBJECT_DEF_FILE_USER_BASE
    if (filethere(get_data_base_dir(predefined_object_definition_file)))
    {
      fp_file = fopen(get_data_base_dir(predefined_object_definition_file),"r");
#endif  // OBJECT_DEF_FILE_USER_BASE

      xastir_snprintf(error_correct_location,
                      sizeof(error_correct_location),
                      "Loading from %s/%s \n",
#ifdef OBJECT_DEF_FILE_USER_BASE
                      get_user_base_dir("config", temp_file_path, sizeof(temp_file_path)),
#else   // OBJECT_DEF_FILE_USER_BASE
                      get_data_base_dir("config"),
#endif  // OBJECT_DEF_FILE_USER_BASE
                      predefined_object_definition_file);
      fprintf(stderr, "%s", error_correct_location);
      while (!feof(fp_file))
      {
        // read lines to end of file
        (void)get_line(fp_file, line, line_max_length);
        // ignore blank lines and lines starting with #
        if ((strncmp("#",line,1)!=0) && (strlen(line) > 2))
        {
          // if line starts "NAME" begin an object
          // next five lines should be PAGE, SYMBOL, DATA, MENU, HIDDENCHILD
          // NAME, PAGE, SYMBOL, MENU, and HIDDENCHILD are required.
          // HIDDENCHILD must come last (it is being used to identify the
          // end of one object).
          // split line into variable/value pairs on tab
          // See predefined_SAR.sys and predefined_event.sys for more details.
          variable = strtok((char *)&line,"\t");
          if (strcmp("NAME",variable)==0)
          {
            value = strtok(NULL,"\t\r\n");
            if (value != NULL)
            {
              xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call), "%s", value);
              // by default, set data to an empty string, allowing DATA to be ommitted
              xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
              object_read_ok ++;
            }
          }
          if (strcmp("PAGE",variable)==0)
          {
            value = strtok(NULL,"\t\r\n");
            if (value != NULL)
            {
              xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page), "%s", value);
              object_read_ok ++;
            }
          }
          if (strcmp("SYMBOL",variable)==0)
          {
            value = strtok(NULL,"\t\r\n");
            if (value != NULL)
            {
              xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol), "%s", value);
              object_read_ok ++;
            }
          }
          if (strcmp("DATA",variable)==0)
          {
            value = strtok(NULL,"\t\r\n");
            if (value == NULL || strcmp(value,"NULL")==0)
            {
              xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
            }
            else
            {
              xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data), "%s", value);
            }
          }
          if (strcmp("MENU",variable)==0)
          {
            value = strtok(NULL,"\t\r\n");
            if (value != NULL)
            {
              xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call), "%s", value);
              object_read_ok ++;
            }
          }
          if (strcmp("HIDDENCHILD",variable)==0)
          {
            value = strtok(NULL,"\t\r\n");
            if (strcmp(value,"YES")==0)
            {
              predefinedObjects[j].show_on_menu = 0;
              predefinedObjects[j].index_of_child = j - 1;
              predefinedObjects[j].index = j;
            }
            else
            {
              predefinedObjects[j].show_on_menu = 1;
              predefinedObjects[j].index_of_child = -1;
              predefinedObjects[j].index = j;
            }
            if (object_read_ok == 4)
            {
              // All elements for an object were read correctly.
              // Begin filling next element in array.
              j++;
              // Read of at least one object was successful,
              // don't display default hardcoded menu items.
              read_file_ok = 1;
              // Reset value counter for next object.
              object_read_ok = 0;
            }
            else
            {
              // Something was missing or HIDDENCHILD was out of order.
              // Don't increment array (overwrite a partly filled entry).
              fprintf(stderr,"Error in reading predefined object menu file:\nAn object is not correctly defined.\n");
            }
          }
        }
      } // end while !feof()
      fclose(fp_file);
      if (read_file_ok==0)
      {
        fprintf(stderr,"Error in reading predefined objects menu file:\nNo valid objects found.\n");
      }
    }
    else
    {

      fprintf(stderr,"Error: Predefined objects menu file not found.\n");

      xastir_snprintf(error_correct_location,
                      sizeof(error_correct_location),
                      "File should be in %s\n",
#ifdef OBJECT_DEF_FILE_USER_BASE
                      get_user_base_dir("config", temp_file_path, sizeof(temp_file_path)));
#else   // OBJECT_DEF_FILE_USER_BASE
                      get_data_base_dir("config"));
#endif  // OBJECT_DEF_FILE_USER_BASE
      fprintf(stderr, "%s", error_correct_location);
    }
  }

  if (read_file_ok==0)
  {
    // file read failed or was not requested, display default SAR menu

    // command post
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"ICP");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"/");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"c");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"ICP: Command Post");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

    // Staging area
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"Staging");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"S");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"0");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"Staging");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

    // Initial Planning Point
    // set up to draw as two objects with different probability circles
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"IPP_");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"/");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"/");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data)," Pmin0.75,Pmax1.0");
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"[not shown]");
    // show on menu = 0 will hide this entry on menu
    predefinedObjects[j].show_on_menu = 0;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"IPP");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"/");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"/");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data)," Pmin0.25,Pmax0.5");
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"IPP: InitialPlanningPoint");
    predefinedObjects[j].show_on_menu = 1;
    // index of child j - 1 will add additional callback to IPP_
    predefinedObjects[j].index_of_child = j - 1;
    predefinedObjects[j].index = j;
    j++;

    // Point last seen
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"PLS");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"/");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"/");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"PLS: Point Last Seen");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;


    // Last known point
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"LKP");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"/");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),".");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"LKP: Last Known Point");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

    // Base
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"Base");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"B");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"0");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"Base");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

    // Helibase (helicopter support base)
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"Helibase");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"H");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"0");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"Helibase");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

    // Helispot  (helicopter landing spot)
    // Heli- will be created as Heli-1, Heli-2, Heli-3, etc.
    // terminal - on a call is a magic character. see Create_SAR+Object.
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"Heli-");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"/");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"/");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"Heli-n: Helispot");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

    // Camp
    xastir_snprintf(predefinedObjects[j].call,sizeof(predefinedObjects[j].call),"Camp");
    xastir_snprintf(predefinedObjects[j].page,sizeof(predefinedObjects[j].page),"C");
    xastir_snprintf(predefinedObjects[j].symbol,sizeof(predefinedObjects[j].symbol),"0");
    xastir_snprintf(predefinedObjects[j].data,sizeof(predefinedObjects[j].data),"%c",'\0');
    xastir_snprintf(predefinedObjects[j].menu_call,sizeof(predefinedObjects[j].menu_call),"Camp");
    predefinedObjects[j].show_on_menu = 1;
    predefinedObjects[j].index_of_child = -1;
    predefinedObjects[j].index = j;
    j++;

  }

  // Could read additional entries from a file here.
  // The total number of entries should be left fairly small
  // to prevent the menu from becoming too large and unweildy.

  number_of_predefined_objects = j;
  if (number_of_predefined_objects>MAX_NUMBER_OF_PREDEFINED_OBJECTS)
  {
    // need beter handling of this - we will allready have run
    // past the end of the array if we have reached here.
    number_of_predefined_objects=MAX_NUMBER_OF_PREDEFINED_OBJECTS;
  }
}





/* Create a predefined SAR/Public Event object
   Create an object of the specified type at the current mouse position
   without a dialog.
   Current undesirable behavior: If an object of the same name exists,
   takes control of that object and moves it to the current mouse position.

   clientData is pointer to an integer representing the index of a
   predefined object in the predefinedObjects array
   */
void Create_SAR_Object(Widget UNUSED(w), XtPointer clientData, XtPointer UNUSED(calldata) )
{
  Dimension width, height;
  char call[MAX_CALLSIGN+1];
  long x_lat,x_lon;
  char origin[MAX_CALLSIGN+1];  // mycall
  char data[MAX_LINE_SIZE];
  char page[2];
  // reserve space for probability circle as well as symbol /Pmin0.25,Pmax0.5,
  char symbol_plus[PREDEFINED_OBJECT_DATA_LENGTH];
  char symbol[2];
  char c_lon[10];
  char c_lat[10];
  char time[7];
  intptr_t i;
  DataRow *p_station;
  int done = 0;
  int iterations_left = 1000; // Max iterations of while loop below
  int extra_num = 1;
  char orig_call[MAX_CALLSIGN+1];


  // set some defaults in case of a non-matched value
  xastir_snprintf(page,sizeof(page),"/");
  xastir_snprintf(symbol,sizeof(symbol),"/");
  xastir_snprintf(call, sizeof(call), "Marker");

  //for (i=0;i<number_of_predefined_objects;i++) {
  //   if (strcmp((char *)clientData,predefinedObjects[i].call)==0) {
  i = (intptr_t)clientData;
  if (i > -1)
  {
    if (i <= number_of_predefined_objects)
    {
      xastir_snprintf(page,sizeof(page), "%s", predefinedObjects[i].page);
      xastir_snprintf(symbol,sizeof(symbol), "%s", predefinedObjects[i].symbol);
      xastir_snprintf(call, sizeof(call), "%s", predefinedObjects[i].call);
      xastir_snprintf(symbol_plus, sizeof(symbol_plus), "%s%s",symbol,predefinedObjects[i].data);
    }
  }

  // Get mouse position.
  XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
  x_lon = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
  x_lat = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
  if(debug_level & 1)
    fprintf(stderr, "Creating symbol %s %s at: %lu %lu with calldata: [%li]\n",
            page,
            symbol,
            x_lat,
            x_lon,
            (intptr_t)clientData);

  // CONVERT_LP_NOSP      = DDMM.MMN
  convert_lat_l2s(x_lat, (char *)c_lat, sizeof(c_lat), CONVERT_LP_NOSP);
  convert_lon_l2s(x_lon, (char *)c_lon, sizeof(c_lon), CONVERT_LP_NOSP);


  // Save "call" away in "orig_call" so that we can use it again
  // and again as we try to come up with a unique name for the
  // object.
  //
  xastir_snprintf(orig_call,
                  sizeof(orig_call),
                  "%s",
                  call);

  // '-' is a magic character.
  //
  // If the last character in call is a "-", the symbol is expected
  // to be a numeric series starting with call-1, so change call to
  // call-1.  This lets us describe Heli- and create Heli-1, Heli-2
  // and similar series.  Storing call to orig_call before appending
  // the number should allow the sequence to increment normally.
  if ((int)'-'==(int)*(call+(strlen(call)-1)))
  {
    // make sure that we don't write past the end of call
    if (strlen(call)<MAX_CALLSIGN)
    {
      strncat(call,"1",sizeof(call)-strlen(call)-1);
    }
  }
  // Check object names against our station database until we find
  // a unique name or a killed object name we can use.
  //
  while (!done && iterations_left)
  {
    char num_string[10];

    // Create object as owned by own station, or take control of
    // object if it has another owner.  Taking control of a
    // received object is probably not a desirable behavior.

    if (!search_station_name(&p_station,call,1))
    {
      //
      // No match found with the original name, so the name
      // for our object is ok to use.  Get out of the while
      // loop and create the object.
      //
      done++;
      continue;   // Next loop iteration (Exit the while loop)
    }


    // If we get to here, a station with this name exists.  We
    // have a pointer to it with p_station.
    //
    // If object or item and killed, use the old name to create
    // a new object.
    //
    // If not killed or not object/item, pick a new name by
    // adding digits onto the end of the old name until we don't
    // have a name collision or we find an old object or item by
    // that name that has been killed.


    // Check whether object or item.  If so, check whether killed.
    if ((p_station->flag & (ST_OBJECT | ST_ITEM)) != 0)   // It's an object or item
    {

      // Check whether object/item has been killed already
      if ((p_station->flag & ST_ACTIVE) != ST_ACTIVE)
      {
        //
        // The object or item has been killed.  Ok to use
        // this object name.  Get out of the while loop and
        // create the object.
        //
        done++;
        continue;   // Next loop iteration (Exit the while loop)
      }
    }


// If we get to this point we have an object name that matches
// another in our database.  We must come up with a new name.  We
// add digits to the end of the original name until we get one that
// works for us.



    /*
            // If my_callsign (Exact match includes SSID)
    //        if (is_my_call(p_station->origin,1)) {
            if (is_my_object_item(p_station)) {

                // The previous object with the same name is owned by
                // me.
                //   a) MOVE the EXISTING object (default), perhaps with the option
                //      to clear the track.  Clearing the track would only take
                //      effect on our local map screen, not on everyone else's.
                //   b) RENAME the NEW object, perhaps tacking a number
                //   onto the end until we get to an unused name.
                //   c) CANCEL request


    fprintf(stderr, "Object with same name exists, owned by me\n");

    // Pop up a new dialog with the various options on it.  Save our
    // state here so that we can create the object in the callbacks for
    // the next dialog.
    //
    // Code goes here...


            else {
                // The previous object with the same name is NOT owned
                // by me.
                //   a) ADOPT the existing object and MOVE it (a very
                //      poor idea).
                //      Same track-clearing as option 1a.
                //   b) RENAME the NEW object (default), perhaps tacking
                //   a number onto the end until we get to an unused
                //   name.
                //   c) CANCEL request


    fprintf(stderr, "Object with same name exists, owned by %s\n", p_station->origin);

    // Pop up a new dialog with the various options on it.  Save our
    // state here so that we can create the object in the callbacks for
    // the next dialog.
    //
    // Code goes here...


            }
    */


    extra_num++;

    // Append extra_num to the object name (starts at "2"), try
    // again to see if it is unique.
    //
    // Note: Converting to float only to use width specifiers
    // properly and quiet a compiler warning.
    xastir_snprintf(num_string, sizeof(num_string), "%2.0f", (float)extra_num);
    strcpy(call, orig_call);
    call[sizeof(call)-1] = '\0';  // Terminate string
    strcat(call, num_string);
    call[sizeof(call)-1] = '\0';  // Terminate string
// ****** Bug ********
// need to check length of call - if it has gone over 9 characters only
// the first 9 will be treated as unique, thus FirstAid11 will become FirstAid1
// and become new position for existing FirstAid1.
// MAX_CALLSIGN is the constraining global.
// In that case, need to fail gracefully and throw an error message.
    iterations_left--;

  }   // End of while loop


  if (iterations_left == 0)
  {
// Pop up a message stating that we couldn't find an empty name in
// 1000 iterations.  Call popup_message_always()

    fprintf(stderr, "No more iterations left\n");

  }


  xastir_snprintf(origin,
                  sizeof(origin),
                  "%s", my_callsign);
  xastir_snprintf(time, sizeof(time), "%02d%02d%02d",
                  get_hours(),
                  get_minutes(),
                  get_seconds() );
  // Prepare APRS data string using latitude and longitude from mouse click location
  // and page, symbol, and any additional data from the prepared object.
  xastir_snprintf(data,
                  sizeof(data),
                  ";%-9s*%sh%s%s%s%s",
                  call,
                  time,
                  c_lat,
                  page,
                  c_lon,
                  symbol_plus);

//fprintf(stderr,"Packet:%s\n", data);

  log_object_item(data,0,last_object);

// *********** New objects not being displayed on map untill restart


  if (object_tx_disable || transmit_disable)
  {
    output_my_data(data,-1,0,1,0,NULL);  // Local loopback only, not igating
  }
  else
  {
    output_my_data(data,-1,0,0,0,NULL);  // Transmit/loopback object data, not igating
  }
}





// Fill in fields from an existing object/item or create the proper
// transmit string for a new object/item from these fields.
/*
 *  Setup Object/Item Dialog
 *  clientData = pointer to object struct, if it's a modify or a move operation,
 *  else it's NULL.
 *  If calldata = 2, then we're doing a move object operation.  We want in that
 *  case to fill in the new values for lat/long and make them take effect.
 *  If calldata = 1, then we've dropped through Station_info/Station_data
 *  on the way to Modify->Object.
 *  Need to put the tests for the different types of objects
 *  at the top of this function, then the dialog will build properly for
 *  the type of object initially.
 */
void Set_Del_Object( Widget w, XtPointer clientData, XtPointer calldata)
{
  Dimension width, height;
  long lat,lon;
  char lat_str[MAX_LAT];
  char lon_str[MAX_LONG];
  static Widget ob_pane, ob_form,
         ob_name,ob_latlon_frame,ob_latlon_form,
         ob_lat, ob_lat_deg, ob_lat_min,
         ob_lon, ob_lon_deg, ob_lon_min, ob_lon_ew,
         ob_form1,
         signpost_form,
         signpost_label,
         probability_frame,probability_form,
         probability_label_min, probability_label_max,
         ob_option_form,
         area_form,
         bright_dim_toggle,
         shape_box,toption1,toption2,toption3,toption4,toption5,
         color_box,coption1,coption2,coption3,coption4,coption5,coption6,coption7,coption8,
         formomni,
         signal_box,soption0,soption1,soption2,soption3,soption4,soption5,soption6,soption7,soption8,soption9,
         height_box,hoption0,hoption1,hoption2,hoption3,hoption4,hoption5,hoption6,hoption7,hoption8,hoption9,
         gain_box,goption0,goption1,goption2,goption3,goption4,goption5,goption6,goption7,goption8,goption9,
         directivity_box,doption0,doption1,doption2,doption3,doption4,doption5,doption6,doption7,doption8,
         formbeam,
         width_box,woption0,woption1,woption2,woption3,woption4,woption5,woption6,woption7,woption8,woption9,
         ob_bearing,
         ob_lat_offset,ob_lon_offset,
         ob_sep, ob_button_set,ob_button_del,ob_button_cancel,it_button_set,
         ob_button_symbol,
         compute_button;
  char temp_data[40];
  Atom delw;
  DataRow *p_station = (DataRow *)clientData;
  Arg al[50];         /* Arg List */
  unsigned int ac;    /* Arg Count */
  long x,y;


  /*
      if (p_station != NULL)
          fprintf(stderr,"Have a pointer to an object.  ");
      else
          fprintf(stderr,"No pointer, new object?       ");
      if (calldata != NULL) {
          if (strcmp(calldata,"2") == 0)
              fprintf(stderr,"Set_Del_Object: calldata: 2.  Move object.\n");
          else if (strcmp(calldata,"1") == 0)
              fprintf(stderr,"Set_Del_Object: calldata: 1.  Modify object.\n");
          else if (strcmp(calldata,"0") == 0)
              fprintf(stderr,"Set_Del_Object: calldata: 0.  New object.\n");
          else
              fprintf(stderr,"Set_Del_Object: calldata: invalid.  New object.\n");
      }
  */

  // Save the data so that other routines can access it.  Some of the
  // callbacks can only handle one parameter, and we need two.
  if (p_station != NULL)
  {
    global_parameter1 = clientData;
  }
  else
  {
    global_parameter1 = NULL;
  }
  global_parameter2 = calldata;



  // This function can be invoked from the mouse menus or by being called
  // directly by other routines.  We look at the p_station pointer to decide
  // how we were called.
  //
  if (p_station != NULL)    // We were called from the Modify_object()
  {
    // or Move() functions
    //fprintf(stderr,"Got a pointer!\n");
    lon = p_station->coord_lon;     // Fill in values from the original object
    lat = p_station->coord_lat;
  }
  else
  {
    // We were called from the "Create New Object" mouse menu or
    // by the "Move" option get default position for object, the
    // position we have clicked at.  For the special case of a
    // Map View object, we instead want the screen center for
    // this new object.
    //
    if (Map_View_object_enabled)
    {
      // Get center of screen
      lon = center_longitude;
      lat = center_latitude;
    }
    else
    {
      // Get mouse position
      XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);
      lon = center_longitude - ((width *scale_x)/2) + (menu_x*scale_x);
      lat = center_latitude  - ((height*scale_y)/2) + (menu_y*scale_y);
    }
  }


  // If the object dialog is up, we need to kill it and draw a new
  // one so that we have the correct values filled in.
  if (object_dialog)
  {
    Object_destroy_shell( w, object_dialog, NULL);
  }


  // Check for the three "Special" types of objects we deal with and set
  // the global variables for them here.  This will result in the correct
  // type of dialog being drawn for each type of object.
// Question:  What about for Modify->Object where we're trying to change
// the type of the object?

  if (p_station != NULL)
  {
    /*
            if (calldata != NULL) {
                if (strcmp(calldata,"2") == 0)
                    fprintf(stderr,"Set_Del_Object: calldata: 2.  Move object.\n");
                else if (strcmp(calldata,"1") == 0)
                    fprintf(stderr,"Set_Del_Object: calldata: 1.  Modify object.\n");
                else if (strcmp(calldata,"0") == 0)
                    fprintf(stderr,"Set_Del_Object: calldata: 0.  New object.\n");
                else
                    fprintf(stderr,"Set_Del_Object: calldata: invalid.  New object.\n");
            }
    */

    // Check to see whether we should even be here at all!
    if ( !(p_station->flag & ST_OBJECT)
         && !(p_station->flag & ST_ITEM))    // Not an object or item
    {
      //fprintf(stderr,"flag: %i\n", (int)p_station->flag);
      popup_message_always(langcode("POPEM00022"),
                           langcode("POPEM00043") ); // "Not an Object/Item!"
      return;
    }

    // Set to known defaults first
    Area_object_enabled = 0;
    Signpost_object_enabled = 0;
    DF_object_enabled = 0;
    Map_View_object_enabled = 0;
    Probability_circles_enabled = 0;

    if (p_station->aprs_symbol.area_object.type != AREA_NONE)   // Found an area object
    {
      Area_object_enabled = 1;
    }
    else if ( (p_station->aprs_symbol.aprs_symbol == 'm') // Found a signpost object
              && (p_station->aprs_symbol.aprs_type == '\\') )
    {
      Signpost_object_enabled = 1;
    }
    else if ( (p_station->aprs_symbol.aprs_symbol == '\\') // Found a DF object
              && (p_station->aprs_symbol.aprs_type == '/')
              && ((strlen(p_station->signal_gain) == 7) // That has data associated with it
                  || (strlen(p_station->bearing) == 3)
                  || (strlen(p_station->NRQ) == 3) ) )
    {
      DF_object_enabled = 1;
    }
    else if ( (p_station->aprs_symbol.aprs_symbol == 'E') // Found a Map View object
              && (p_station->aprs_symbol.aprs_type == '/')
              && (strstr(p_station->power_gain,"RNG") != 0) )   // Has a range value
    {

      //fprintf(stderr,"Found a range\n");
      Map_View_object_enabled = 1;
    }

    else if (p_station->probability_min[0] != '\0'      // Found some data
             || p_station->probability_max[0] != '\0')   // Found some data
    {
      Probability_circles_enabled = 1;
    }
  }

  //fprintf(stderr,"Area:Signpost:DF  %i:%i:%i\n",Area_object_enabled,Signpost_object_enabled,DF_object_enabled);

// Ok.  The stage is now set to draw the proper type of dialog for the
// type of object we're interested in currently.


  if(object_dialog)           // it is already open
  {
    (void)XRaiseWindow(XtDisplay(object_dialog), XtWindow(object_dialog));
  }
  else                        // create new popup window
  {
    object_dialog = XtVaCreatePopupShell(langcode("POPUPOB001"),
                                         xmDialogShellWidgetClass,   appshell,
                                         XmNdeleteResponse,          XmDESTROY,
                                         XmNdefaultPosition,         FALSE,
                                         XmNfontList, fontlist1,
                                         NULL);

    ob_pane = XtVaCreateWidget("Set_Del_Object pane",
                               xmPanedWindowWidgetClass,
                               object_dialog,
                               MY_FOREGROUND_COLOR,
                               MY_BACKGROUND_COLOR,
                               XmNfontList, fontlist1,
                               NULL);

    ob_form =  XtVaCreateWidget("Set_Del_Object ob_form",
                                xmFormWidgetClass,
                                ob_pane,
                                XmNfractionBase,            3,
                                XmNautoUnmanage,            FALSE,
                                XmNshadowThickness,         1,
                                MY_FOREGROUND_COLOR,
                                MY_BACKGROUND_COLOR,
                                XmNfontList, fontlist1,
                                NULL);

    // "Name"
    ob_name = XtVaCreateManagedWidget(langcode("POPUPOB002"),
                                      xmLabelWidgetClass,
                                      ob_form,
                                      XmNtopAttachment,           XmATTACH_FORM,
                                      XmNtopOffset,               10,
                                      XmNbottomAttachment,        XmATTACH_NONE,
                                      XmNleftAttachment,          XmATTACH_FORM,
                                      XmNleftOffset,              10,
                                      XmNrightAttachment,         XmATTACH_NONE,
                                      MY_FOREGROUND_COLOR,
                                      MY_BACKGROUND_COLOR,
                                      XmNfontList, fontlist1,
                                      NULL);
    // object name
    object_name_data = XtVaCreateManagedWidget("Set_Del_Object name_data",
                       xmTextFieldWidgetClass,
                       ob_form,
                       XmNeditable,                TRUE,
                       XmNcursorPositionVisible,   TRUE,
                       XmNsensitive,               TRUE,
                       XmNshadowThickness,         1,
                       XmNcolumns,                 9,
                       XmNmaxLength,               9,
                       XmNtopAttachment,           XmATTACH_FORM,
                       XmNtopOffset,               5,
                       XmNbottomAttachment,        XmATTACH_NONE,
                       XmNleftAttachment,          XmATTACH_WIDGET,
                       XmNleftWidget,              ob_name,
                       XmNrightAttachment,         XmATTACH_NONE,
                       XmNbackground,              colors[0x0f],
                       XmNfontList, fontlist1,
                       NULL);


//----- Frame for table / symbol
    ob_frame = XtVaCreateManagedWidget("Set_Del_Object ob_frame",
                                       xmFrameWidgetClass,
                                       ob_form,
                                       XmNtopAttachment,           XmATTACH_WIDGET,
                                       XmNtopWidget,               object_name_data,
                                       XmNtopOffset,               10,
                                       XmNbottomAttachment,        XmATTACH_NONE,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNleftOffset,              10,
                                       XmNrightAttachment,         XmATTACH_NONE,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    // "Station Symbol"
    // ob_ts
    (void)XtVaCreateManagedWidget(langcode("WPUPCFS009"),
                                  xmLabelWidgetClass,
                                  ob_frame,
                                  XmNchildType,               XmFRAME_TITLE_CHILD,
                                  MY_FOREGROUND_COLOR,
                                  MY_BACKGROUND_COLOR,
                                  XmNfontList, fontlist1,
                                  NULL);

    ob_form1 =  XtVaCreateWidget("Set_Del_Object form1",
                                 xmFormWidgetClass,
                                 ob_frame,
                                 XmNfractionBase,            5,
                                 MY_FOREGROUND_COLOR,
                                 MY_BACKGROUND_COLOR,
                                 XmNfontList, fontlist1,
                                 NULL);

    // "Group/overlay"
    ob_group = XtVaCreateManagedWidget(langcode("WPUPCFS010"),
                                       xmLabelWidgetClass,
                                       ob_form1,
                                       XmNtopAttachment,           XmATTACH_FORM,
                                       XmNtopOffset,               8,
                                       XmNbottomAttachment,        XmATTACH_FORM,
                                       XmNbottomOffset,            10,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNleftOffset,              10,
                                       XmNrightAttachment,         XmATTACH_NONE,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    // table
    object_group_data = XtVaCreateManagedWidget("Set_Del_Object group",
                        xmTextFieldWidgetClass,
                        ob_form1,
                        XmNeditable,                TRUE,
                        XmNcursorPositionVisible,   FALSE,
                        XmNsensitive,               TRUE,
                        XmNshadowThickness,         1,
                        XmNcolumns,                 1,
                        XmNmaxLength,               1,
                        XmNtopOffset,               3,
                        XmNbackground,              colors[0x0f],
                        XmNleftAttachment,          XmATTACH_WIDGET,
                        XmNleftWidget,              ob_group,
                        XmNleftOffset,              5,
                        XmNtopAttachment,           XmATTACH_FORM,
                        XmNbottomAttachment,        XmATTACH_NONE,
                        XmNrightAttachment,         XmATTACH_NONE,
                        XmNfontList, fontlist1,
                        NULL);

    // "Symbol"
    ob_symbol = XtVaCreateManagedWidget(langcode("WPUPCFS011"),
                                        xmLabelWidgetClass,
                                        ob_form1,
                                        XmNtopAttachment,           XmATTACH_FORM,
                                        XmNtopOffset,               8,
                                        XmNbottomAttachment,        XmATTACH_NONE,
                                        XmNleftAttachment,          XmATTACH_WIDGET,
                                        XmNleftWidget,              object_group_data,
                                        XmNleftOffset,              20,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);
    // symbol
    object_symbol_data = XtVaCreateManagedWidget("Set_Del_Object symbol",
                         xmTextFieldWidgetClass,
                         ob_form1,
                         XmNeditable,                TRUE,
                         XmNcursorPositionVisible,   FALSE,
                         XmNsensitive,               TRUE,
                         XmNshadowThickness,         1,
                         XmNcolumns,                 1,
                         XmNmaxLength,               1,
                         XmNtopOffset,               3,
                         XmNbackground,              colors[0x0f],
                         XmNleftAttachment,          XmATTACH_WIDGET,
                         XmNleftWidget,              ob_symbol,
                         XmNleftOffset,              5,
                         XmNtopAttachment,           XmATTACH_FORM,
                         XmNbottomAttachment,        XmATTACH_NONE,
                         XmNrightAttachment,         XmATTACH_NONE,
                         XmNfontList, fontlist1,
                         NULL);

    // icon
    Ob_icon0 = XCreatePixmap(XtDisplay(appshell),
                             RootWindowOfScreen(XtScreen(appshell)),
                             20,
                             20,
                             DefaultDepthOfScreen(XtScreen(appshell)));
    Ob_icon  = XCreatePixmap(XtDisplay(appshell),
                             RootWindowOfScreen(XtScreen(appshell)),
                             20,
                             20,
                             DefaultDepthOfScreen(XtScreen(appshell)));
    object_icon = XtVaCreateManagedWidget("Set_Del_Object icon",
                                          xmLabelWidgetClass,
                                          ob_form1,
                                          XmNlabelType,               XmPIXMAP,
                                          XmNlabelPixmap,             Ob_icon,
                                          XmNleftAttachment,          XmATTACH_WIDGET,
                                          XmNleftWidget,              object_symbol_data,
                                          XmNleftOffset,              15,
                                          XmNtopAttachment,           XmATTACH_FORM,
                                          XmNtopOffset,               8,
                                          XmNbottomAttachment,        XmATTACH_NONE,
                                          XmNrightAttachment,         XmATTACH_NONE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

    ob_button_symbol = XtVaCreateManagedWidget(langcode("WPUPCFS028"),
                       xmPushButtonGadgetClass,
                       ob_form1,
                       XmNtopAttachment,           XmATTACH_FORM,
                       XmNtopOffset,               2,
                       XmNbottomAttachment,        XmATTACH_NONE,
                       XmNleftAttachment,          XmATTACH_WIDGET,
                       XmNleftWidget,              object_icon,
                       XmNleftOffset,              5,
                       XmNrightAttachment,         XmATTACH_FORM,
                       XmNrightOffset,             5,
                       XmNnavigationType,          XmTAB_GROUP,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
                       NULL);
    XtAddCallback(ob_button_symbol, XmNactivateCallback, Ob_change_symbol, object_dialog);

//----- Frame for Lat/Long
    ob_latlon_frame = XtVaCreateManagedWidget("Set_Del_Object ob_latlon_frame",
                      xmFrameWidgetClass,
                      ob_form,
                      XmNtopAttachment,           XmATTACH_FORM,
                      XmNtopOffset,               10,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNleftAttachment,          XmATTACH_WIDGET,
                      XmNleftWidget,              ob_frame,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_FORM,
                      XmNrightOffset,             10,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      NULL);

    // "Location"
    //ob_latlon_ts
    (void)XtVaCreateManagedWidget(langcode("POPUPOB028"),
                                  xmLabelWidgetClass,
                                  ob_latlon_frame,
                                  XmNchildType,               XmFRAME_TITLE_CHILD,
                                  MY_FOREGROUND_COLOR,
                                  MY_BACKGROUND_COLOR,
                                  XmNfontList, fontlist1,
                                  NULL);

    ob_latlon_form =  XtVaCreateWidget("Set_Del_Object ob_latlon_form",
                                       xmFormWidgetClass,
                                       ob_latlon_frame,
                                       XmNfractionBase,            5,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);

    // "LAT"
    ob_lat = XtVaCreateManagedWidget(langcode("WPUPCFS003"),
                                     xmLabelWidgetClass,
                                     ob_latlon_form,
                                     XmNtopAttachment,           XmATTACH_FORM,
                                     XmNtopOffset,               10,
                                     XmNbottomAttachment,        XmATTACH_NONE,
                                     XmNleftAttachment,          XmATTACH_FORM,
                                     XmNleftOffset,              15,
                                     XmNrightAttachment,         XmATTACH_NONE,
                                     MY_FOREGROUND_COLOR,
                                     MY_BACKGROUND_COLOR,
                                     XmNfontList, fontlist1,
                                     NULL);
    // lat deg
    object_lat_data_deg = XtVaCreateManagedWidget("Set_Del_Object lat_deg",
                          xmTextFieldWidgetClass,
                          ob_latlon_form,
                          XmNeditable,                TRUE,
                          XmNcursorPositionVisible,   TRUE,
                          XmNsensitive,               TRUE,
                          XmNshadowThickness,         1,
                          XmNcolumns,                 3,
                          XmNmaxLength,               2,
                          XmNtopOffset,               5,
                          XmNtopAttachment,           XmATTACH_FORM,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              ob_lat,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNbackground,              colors[0x0f],
                          XmNfontList, fontlist1,
                          NULL);
    // "deg"
    ob_lat_deg = XtVaCreateManagedWidget(langcode("WPUPCFS004"),
                                         xmLabelWidgetClass,
                                         ob_latlon_form,
                                         XmNtopAttachment,           XmATTACH_FORM,
                                         XmNtopOffset,               10,
                                         XmNbottomAttachment,        XmATTACH_NONE,
                                         XmNleftAttachment,          XmATTACH_WIDGET,
                                         XmNleftWidget,              object_lat_data_deg,
                                         XmNrightAttachment,         XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
    // lat min
    object_lat_data_min = XtVaCreateManagedWidget("Set_Del_Object lat_min",
                          xmTextFieldWidgetClass,
                          ob_latlon_form,
                          XmNeditable,                TRUE,
                          XmNcursorPositionVisible,   TRUE,
                          XmNsensitive,               TRUE,
                          XmNshadowThickness,         1,
                          XmNcolumns,                 6,
                          XmNmaxLength,               6,
                          XmNtopOffset,               5,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              ob_lat_deg,
                          XmNleftOffset,              10,
                          XmNtopAttachment,           XmATTACH_FORM,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNbackground,              colors[0x0f],
                          XmNfontList, fontlist1,
                          NULL);
    // "min"
    ob_lat_min = XtVaCreateManagedWidget(langcode("WPUPCFS005"),
                                         xmLabelWidgetClass,
                                         ob_latlon_form,
                                         XmNtopAttachment,           XmATTACH_FORM,
                                         XmNtopOffset,               10,
                                         XmNbottomAttachment,        XmATTACH_NONE,
                                         XmNleftAttachment,          XmATTACH_WIDGET,
                                         XmNleftWidget,              object_lat_data_min,
                                         XmNrightAttachment,         XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
    // N/S
    object_lat_data_ns = XtVaCreateManagedWidget("Set_Del_Object lat_ns",
                         xmTextFieldWidgetClass,
                         ob_latlon_form,
                         XmNeditable,                TRUE,
                         XmNcursorPositionVisible,   FALSE,
                         XmNsensitive,               TRUE,
                         XmNshadowThickness,         1,
                         XmNcolumns,                 1,
                         XmNmaxLength,               1,
                         XmNtopOffset,               5,
                         XmNleftAttachment,          XmATTACH_WIDGET,
                         XmNleftWidget,              ob_lat_min,
                         XmNleftOffset,              10,
                         XmNtopAttachment,           XmATTACH_FORM,
                         XmNbottomAttachment,        XmATTACH_NONE,
                         XmNrightAttachment,         XmATTACH_NONE,
                         XmNbackground,              colors[0x0f],
                         XmNfontList, fontlist1,
                         NULL);
    // "(N/S)"
    // ob_lat_ns
    (void)XtVaCreateManagedWidget(langcode("WPUPCFS006"),
                                  xmLabelWidgetClass,
                                  ob_latlon_form,
                                  XmNtopAttachment,           XmATTACH_FORM,
                                  XmNtopOffset,               10,
                                  XmNbottomAttachment,        XmATTACH_NONE,
                                  XmNleftAttachment,          XmATTACH_WIDGET,
                                  XmNleftWidget,              object_lat_data_ns,
                                  XmNrightAttachment,         XmATTACH_NONE,
                                  MY_FOREGROUND_COLOR,
                                  MY_BACKGROUND_COLOR,
                                  XmNfontList, fontlist1,
                                  NULL);

    // "LONG"
    ob_lon = XtVaCreateManagedWidget(langcode("WPUPCFS007"),
                                     xmLabelWidgetClass,
                                     ob_latlon_form,
                                     XmNtopAttachment,           XmATTACH_WIDGET,
                                     XmNtopWidget,               ob_lat,
                                     XmNtopOffset,               20,
                                     XmNbottomAttachment,        XmATTACH_NONE,
                                     XmNleftAttachment,          XmATTACH_FORM,
                                     XmNleftOffset,              10,
                                     XmNrightAttachment,         XmATTACH_NONE,
                                     MY_FOREGROUND_COLOR,
                                     MY_BACKGROUND_COLOR,
                                     XmNfontList, fontlist1,
                                     NULL);
    // long
    object_lon_data_deg = XtVaCreateManagedWidget("Set_Del_Object long_deg",
                          xmTextFieldWidgetClass,
                          ob_latlon_form,
                          XmNeditable,                TRUE,
                          XmNcursorPositionVisible,   TRUE,
                          XmNsensitive,               TRUE,
                          XmNshadowThickness,         1,
                          XmNcolumns,                 3,
                          XmNmaxLength,               3,
                          XmNtopOffset,               14,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               ob_lat,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              ob_lon,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNbackground,              colors[0x0f],
                          XmNfontList, fontlist1,
                          NULL);
    // "deg"
    ob_lon_deg = XtVaCreateManagedWidget(langcode("WPUPCFS004"),
                                         xmLabelWidgetClass,
                                         ob_latlon_form,
                                         XmNtopAttachment,           XmATTACH_WIDGET,
                                         XmNtopWidget,               ob_lat,
                                         XmNtopOffset,               20,
                                         XmNbottomAttachment,        XmATTACH_NONE,
                                         XmNleftAttachment,          XmATTACH_WIDGET,
                                         XmNleftWidget,              object_lon_data_deg,
                                         XmNrightAttachment,         XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
    // min
    object_lon_data_min = XtVaCreateManagedWidget("Set_Del_Object long_min",
                          xmTextFieldWidgetClass,
                          ob_latlon_form,
                          XmNeditable,                TRUE,
                          XmNcursorPositionVisible,   TRUE,
                          XmNsensitive,               TRUE,
                          XmNshadowThickness,         1,
                          XmNcolumns,                 6,
                          XmNmaxLength,               6,
                          XmNtopOffset,               14,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              ob_lon_deg,
                          XmNleftOffset,              10,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               ob_lat,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNbackground,              colors[0x0f],
                          XmNfontList, fontlist1,
                          NULL);
    // "min"
    ob_lon_min = XtVaCreateManagedWidget(langcode("WPUPCFS005"),
                                         xmLabelWidgetClass,
                                         ob_latlon_form,
                                         XmNtopAttachment,           XmATTACH_WIDGET,
                                         XmNtopWidget,               ob_lat,
                                         XmNtopOffset,               20,
                                         XmNbottomAttachment,        XmATTACH_NONE,
                                         XmNleftAttachment,          XmATTACH_WIDGET,
                                         XmNleftWidget,              object_lon_data_min,
                                         XmNrightAttachment,         XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
    // E/W
    object_lon_data_ew = XtVaCreateManagedWidget("Set_Del_Object long_ew",
                         xmTextFieldWidgetClass,
                         ob_latlon_form,
                         XmNeditable,                TRUE,
                         XmNcursorPositionVisible,   FALSE,
                         XmNsensitive,               TRUE,
                         XmNshadowThickness,         1,
                         XmNcolumns,                 1,
                         XmNmaxLength,               1,
                         XmNtopOffset,               14,
                         XmNleftAttachment,          XmATTACH_WIDGET,
                         XmNleftWidget,              ob_lon_min,
                         XmNleftOffset,              10,
                         XmNtopAttachment,           XmATTACH_WIDGET,
                         XmNtopWidget,               ob_lat,
                         XmNbottomAttachment,        XmATTACH_NONE,
                         XmNrightAttachment,         XmATTACH_NONE,
                         XmNbackground,              colors[0x0f],
                         XmNfontList, fontlist1,
                         NULL);
    // "(E/W)"
    ob_lon_ew = XtVaCreateManagedWidget(langcode("WPUPCFS008"),
                                        xmLabelWidgetClass,
                                        ob_latlon_form,
                                        XmNtopAttachment,           XmATTACH_WIDGET,
                                        XmNtopWidget,               ob_lat,
                                        XmNtopOffset,               20,
                                        XmNbottomAttachment,        XmATTACH_NONE,
                                        XmNleftAttachment,          XmATTACH_WIDGET,
                                        XmNleftWidget,              object_lon_data_ew,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);

    compute_button = XtVaCreateManagedWidget(langcode("COORD002"),
                     xmPushButtonGadgetClass,
                     ob_latlon_form,
                     XmNtopAttachment,           XmATTACH_WIDGET,
                     XmNtopWidget,               ob_lat,
                     XmNtopOffset,               20,
                     XmNbottomAttachment,        XmATTACH_NONE,
                     XmNleftAttachment,          XmATTACH_WIDGET,
                     XmNleftWidget,              ob_lon_ew,
                     XmNleftOffset,              10,
                     XmNrightAttachment,         XmATTACH_NONE,
                     XmNnavigationType,          XmTAB_GROUP,
                     MY_FOREGROUND_COLOR,
                     MY_BACKGROUND_COLOR,
                     XmNfontList, fontlist1,
                     NULL);

    // Fill in the pointers to our input textfields so that the coordinate
    // calculator can fiddle with them.
    coordinate_calc_array.calling_dialog = object_dialog;
    coordinate_calc_array.input_lat_deg = object_lat_data_deg;
    coordinate_calc_array.input_lat_min = object_lat_data_min;
    coordinate_calc_array.input_lat_dir = object_lat_data_ns;
    coordinate_calc_array.input_lon_deg = object_lon_data_deg;
    coordinate_calc_array.input_lon_min = object_lon_data_min;
    coordinate_calc_array.input_lon_dir = object_lon_data_ew;
//        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, ob_latlon_form);
//        XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, "Set_Del_Object");
    XtAddCallback(compute_button, XmNactivateCallback, Coordinate_calc, langcode("POPUPOB001"));

//----- Frame for generic options
    ob_option_frame = XtVaCreateManagedWidget("Set_Del_Object ob_option_frame",
                      xmFrameWidgetClass,
                      ob_form,
                      XmNtopAttachment,           XmATTACH_WIDGET,
                      XmNtopWidget,               ob_frame,
                      XmNtopOffset,               10,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNleftAttachment,          XmATTACH_FORM,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      XmNrightOffset,             10,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);
    // "Generic Options"
    // ob_option_ts
    (void)XtVaCreateManagedWidget(langcode("POPUPOB027"),
                                  xmLabelWidgetClass,
                                  ob_option_frame,
                                  XmNchildType,               XmFRAME_TITLE_CHILD,
                                  MY_FOREGROUND_COLOR,
                                  MY_BACKGROUND_COLOR,
                                  XmNfontList, fontlist1,
                                  NULL);

    ob_option_form =  XtVaCreateWidget("Set_Del_Object ob_option_form",
                                       xmFormWidgetClass,
                                       ob_option_frame,
                                       XmNfractionBase,            5,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);

    // "Speed"
    ob_speed = XtVaCreateManagedWidget(langcode("POPUPOB036"),
                                       xmLabelWidgetClass,
                                       ob_option_form,
                                       XmNtopAttachment,           XmATTACH_FORM,
                                       XmNtopOffset,               8,
                                       XmNbottomAttachment,        XmATTACH_FORM,
                                       XmNbottomOffset,            10,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNleftOffset,              5,
                                       XmNrightAttachment,         XmATTACH_NONE,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);

    ob_speed_data = XtVaCreateManagedWidget("Set_Del_Object ob_speed_data",
                                            xmTextFieldWidgetClass,
                                            ob_option_form,
                                            XmNeditable,                TRUE,
                                            XmNcursorPositionVisible,   TRUE,
                                            XmNsensitive,               TRUE,
                                            XmNshadowThickness,         1,
                                            XmNcolumns,                 3,
                                            XmNmaxLength,               3,
                                            XmNtopOffset,               3,
                                            XmNbackground,              colors[0x0f],
                                            XmNleftAttachment,          XmATTACH_WIDGET,
                                            XmNleftWidget,              ob_speed,
                                            XmNtopAttachment,           XmATTACH_FORM,
                                            XmNbottomAttachment,        XmATTACH_FORM,
                                            XmNrightAttachment,         XmATTACH_NONE,
                                            XmNfontList, fontlist1,
                                            NULL);

    // "Course"
    ob_course = XtVaCreateManagedWidget(langcode("POPUPOB037"),
                                        xmLabelWidgetClass,
                                        ob_option_form,
                                        XmNtopAttachment,           XmATTACH_FORM,
                                        XmNtopOffset,               8,
                                        XmNbottomAttachment,        XmATTACH_FORM,
                                        XmNbottomOffset,            10,
                                        XmNleftAttachment,          XmATTACH_WIDGET,
                                        XmNleftWidget,              ob_speed_data,
                                        XmNleftOffset,              10,
                                        XmNrightAttachment,         XmATTACH_NONE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);

    ob_course_data = XtVaCreateManagedWidget("Set_Del_Object ob_course_data",
                     xmTextFieldWidgetClass,
                     ob_option_form,
                     XmNeditable,                TRUE,
                     XmNcursorPositionVisible,   TRUE,
                     XmNsensitive,               TRUE,
                     XmNshadowThickness,         1,
                     XmNcolumns,                 3,
                     XmNmaxLength,               3,
                     XmNtopOffset,               3,
                     XmNbackground,              colors[0x0f],
                     XmNleftAttachment,          XmATTACH_WIDGET,
                     XmNleftWidget,              ob_course,
                     XmNtopAttachment,           XmATTACH_FORM,
                     XmNbottomAttachment,        XmATTACH_FORM,
                     XmNrightAttachment,         XmATTACH_NONE,
                     XmNfontList, fontlist1,
                     NULL);

    // "Altitude"
    ob_altitude = XtVaCreateManagedWidget(langcode("POPUPOB035"),
                                          xmLabelWidgetClass,
                                          ob_option_form,
                                          XmNtopAttachment,           XmATTACH_FORM,
                                          XmNtopOffset,               8,
                                          XmNbottomAttachment,        XmATTACH_FORM,
                                          XmNbottomOffset,            10,
                                          XmNleftAttachment,          XmATTACH_WIDGET,
                                          XmNleftWidget,              ob_course_data,
                                          XmNleftOffset,              10,
                                          XmNrightAttachment,         XmATTACH_NONE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);

    ob_altitude_data = XtVaCreateManagedWidget("Set_Del_Object ob_altitude_data",
                       xmTextFieldWidgetClass,
                       ob_option_form,
                       XmNeditable,                TRUE,
                       XmNcursorPositionVisible,   TRUE,
                       XmNsensitive,               TRUE,
                       XmNshadowThickness,         1,
                       XmNcolumns,                 6,
                       XmNmaxLength,               6,
                       XmNtopOffset,               3,
                       XmNbackground,              colors[0x0f],
                       XmNleftAttachment,          XmATTACH_WIDGET,
                       XmNleftWidget,              ob_altitude,
                       XmNtopAttachment,           XmATTACH_FORM,
                       XmNbottomAttachment,        XmATTACH_FORM,
                       XmNrightAttachment,         XmATTACH_NONE,
                       XmNfontList, fontlist1,
                       NULL);


//----- Comment Field
    // "Comment:"
    ob_comment = XtVaCreateManagedWidget(langcode("WPUPCFS017"),
                                         xmLabelWidgetClass,
                                         ob_form,
                                         XmNtopAttachment,           XmATTACH_WIDGET,
                                         XmNtopWidget,               ob_option_frame,
                                         XmNtopOffset,               10,
                                         XmNbottomAttachment,        XmATTACH_NONE,
                                         XmNleftAttachment,          XmATTACH_FORM,
                                         XmNleftOffset,              10,
                                         XmNrightAttachment,         XmATTACH_NONE,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);

    object_comment_data = XtVaCreateManagedWidget("Set_Del_Object comment",
                          xmTextFieldWidgetClass,
                          ob_form,
                          XmNeditable,                TRUE,
                          XmNcursorPositionVisible,   TRUE,
                          XmNsensitive,               TRUE,
                          XmNshadowThickness,         1,
                          XmNcolumns,                 43,     // max 43 without Data Extension
                          XmNmaxLength,               43,
                          XmNtopOffset,               6,
                          XmNbackground,              colors[0x0f],
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              ob_comment,
                          XmNleftOffset,              5,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               ob_option_frame,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNfontList, fontlist1,
                          NULL);


    // "Probability Circles"
    probabilities_toggle = XtVaCreateManagedWidget(langcode("POPUPOB047"),
                           xmToggleButtonGadgetClass,
                           ob_form,
                           XmNtopAttachment,           XmATTACH_WIDGET,
                           XmNtopWidget,               ob_latlon_frame,
                           XmNtopOffset,               2,
                           XmNbottomAttachment,        XmATTACH_NONE,
                           XmNbottomOffset,            0,
                           XmNleftAttachment,          XmATTACH_WIDGET,
                           XmNleftWidget,              ob_option_frame,
                           XmNleftOffset,              10,
                           XmNrightAttachment,         XmATTACH_NONE,
                           MY_FOREGROUND_COLOR,
                           MY_BACKGROUND_COLOR,
                           XmNfontList, fontlist1,
                           NULL);
    XtAddCallback(probabilities_toggle,XmNvalueChangedCallback,Probability_circle_toggle,(XtPointer)p_station);


    // "Signpost Enable"
    signpost_toggle = XtVaCreateManagedWidget(langcode("POPUPOB029"),
                      xmToggleButtonGadgetClass,
                      ob_form,
                      XmNtopAttachment,           XmATTACH_WIDGET,
                      XmNtopWidget,               probabilities_toggle,
                      XmNtopOffset,               0,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNbottomOffset,            0,
                      XmNleftAttachment,          XmATTACH_WIDGET,
                      XmNleftWidget,              ob_option_frame,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);
    XtAddCallback(signpost_toggle,XmNvalueChangedCallback,Signpost_object_toggle,(XtPointer)p_station);


    // "Area Enable"
    area_toggle = XtVaCreateManagedWidget(langcode("POPUPOB008"),
                                          xmToggleButtonGadgetClass,
                                          ob_form,
                                          XmNtopAttachment,           XmATTACH_WIDGET,
                                          XmNtopWidget,               signpost_toggle,
                                          XmNtopOffset,               0,
                                          XmNbottomAttachment,        XmATTACH_NONE,
                                          XmNbottomOffset,            0,
                                          XmNleftAttachment,          XmATTACH_WIDGET,
                                          XmNleftWidget,              ob_option_frame,
                                          XmNleftOffset,              10,
                                          XmNrightAttachment,         XmATTACH_NONE,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtAddCallback(area_toggle,XmNvalueChangedCallback,Area_object_toggle,(XtPointer)p_station);



    // "Area Enable"
    df_bearing_toggle = XtVaCreateManagedWidget(langcode("POPUPOB038"),
                        xmToggleButtonGadgetClass,
                        ob_form,
                        XmNtopAttachment,           XmATTACH_WIDGET,
                        XmNtopWidget,               area_toggle,
                        XmNtopOffset,               0,
                        XmNbottomAttachment,        XmATTACH_NONE,
                        XmNbottomOffset,            0,
                        XmNleftAttachment,          XmATTACH_WIDGET,
                        XmNleftWidget,              ob_option_frame,
                        XmNleftOffset,              10,
                        XmNrightAttachment,         XmATTACH_NONE,
                        MY_FOREGROUND_COLOR,
                        MY_BACKGROUND_COLOR,
                        XmNfontList, fontlist1,
                        NULL);
    XtAddCallback(df_bearing_toggle,XmNvalueChangedCallback,DF_bearing_object_toggle,(XtPointer)p_station);



    // "Map View Object"
    map_view_toggle = XtVaCreateManagedWidget(langcode("POPUPOB048"),
                      xmToggleButtonGadgetClass,
                      ob_form,
                      XmNtopAttachment,           XmATTACH_WIDGET,
                      XmNtopWidget,               df_bearing_toggle,
                      XmNtopOffset,               0,
                      XmNbottomAttachment,        XmATTACH_NONE,
                      XmNbottomOffset,            0,
                      XmNleftAttachment,          XmATTACH_WIDGET,
                      XmNleftWidget,              ob_option_frame,
                      XmNleftOffset,              10,
                      XmNrightAttachment,         XmATTACH_NONE,
                      MY_FOREGROUND_COLOR,
                      MY_BACKGROUND_COLOR,
                      XmNfontList, fontlist1,
                      NULL);
    XtAddCallback(map_view_toggle,XmNvalueChangedCallback,Map_View_object_toggle,(XtPointer)p_station);



//----- Frame for Probability Circles info
    if (Probability_circles_enabled)
    {

      //fprintf(stderr,"Drawing probability circle data\n");

      probability_frame = XtVaCreateManagedWidget("Set_Del_Object probability_frame",
                          xmFrameWidgetClass,
                          ob_form,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               map_view_toggle,
                          XmNtopOffset,               0,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNleftAttachment,          XmATTACH_FORM,
                          XmNleftOffset,              10,
                          XmNrightAttachment,         XmATTACH_FORM,
                          XmNrightOffset,             10,
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          XmNfontList, fontlist1,
                          NULL);

      // "Probability Circles"
      // probability_ts
      (void)XtVaCreateManagedWidget(langcode("POPUPOB047"),
                                    xmLabelWidgetClass,
                                    probability_frame,
                                    XmNchildType,               XmFRAME_TITLE_CHILD,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    XmNfontList, fontlist1,
                                    NULL);

      probability_form =  XtVaCreateWidget("Set_Del_Object probability_form",
                                           xmFormWidgetClass,
                                           probability_frame,
                                           XmNfractionBase,            5,
                                           XmNautoUnmanage,            FALSE,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);

      // "Min (mi):"
      probability_label_min = XtVaCreateManagedWidget(langcode("POPUPOB049"),
                              xmLabelWidgetClass,
                              probability_form,
                              XmNtopAttachment,           XmATTACH_FORM,
                              XmNtopOffset,               8,
                              XmNbottomAttachment,        XmATTACH_NONE,
                              XmNleftAttachment,          XmATTACH_FORM,
                              XmNleftOffset,              10,
                              XmNrightAttachment,         XmATTACH_NONE,
                              MY_FOREGROUND_COLOR,
                              MY_BACKGROUND_COLOR,
                              XmNfontList, fontlist1,
                              NULL);

      probability_data_min = XtVaCreateManagedWidget("Set_Del_Object probability_data_min",
                             xmTextFieldWidgetClass,
                             probability_form,
                             XmNeditable,                TRUE,
                             XmNcursorPositionVisible,   TRUE,
                             XmNsensitive,               TRUE,
                             XmNshadowThickness,         1,
                             XmNcolumns,                 10,
                             XmNmaxLength,               10,
                             XmNtopOffset,               3,
                             XmNbackground,              colors[0x0f],
                             XmNleftAttachment,          XmATTACH_WIDGET,
                             XmNleftWidget,              probability_label_min,
                             XmNtopAttachment,           XmATTACH_FORM,
                             XmNbottomAttachment,        XmATTACH_NONE,
                             XmNrightAttachment,         XmATTACH_NONE,
                             XmNfontList, fontlist1,
                             NULL);

      // "Max (mi):"
      probability_label_max = XtVaCreateManagedWidget(langcode("POPUPOB050"),
                              xmLabelWidgetClass,
                              probability_form,
                              XmNtopAttachment,           XmATTACH_WIDGET,
                              XmNtopWidget,               probability_label_min,
                              XmNtopOffset,               8,
                              XmNbottomAttachment,        XmATTACH_NONE,
                              XmNbottomOffset,            10,
                              XmNleftAttachment,          XmATTACH_FORM,
                              XmNleftOffset,              10,
                              XmNrightAttachment,         XmATTACH_NONE,
                              MY_FOREGROUND_COLOR,
                              MY_BACKGROUND_COLOR,
                              XmNfontList, fontlist1,
                              NULL);

      probability_data_max = XtVaCreateManagedWidget("Set_Del_Object probability_data_max",
                             xmTextFieldWidgetClass,
                             probability_form,
                             XmNeditable,                TRUE,
                             XmNcursorPositionVisible,   TRUE,
                             XmNsensitive,               TRUE,
                             XmNshadowThickness,         1,
                             XmNcolumns,                 10,
                             XmNmaxLength,               10,
                             XmNtopOffset,               3,
                             XmNbackground,              colors[0x0f],
                             XmNleftAttachment,          XmATTACH_WIDGET,
                             XmNleftWidget,              probability_label_max,
                             XmNtopAttachment,           XmATTACH_WIDGET,
                             XmNtopWidget,               probability_label_min,
                             XmNbottomAttachment,        XmATTACH_NONE,
                             XmNrightAttachment,         XmATTACH_NONE,
                             XmNfontList, fontlist1,
                             NULL);



      ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep",
                                       xmSeparatorGadgetClass,
                                       ob_form,
                                       XmNorientation,             XmHORIZONTAL,
                                       XmNtopAttachment,           XmATTACH_WIDGET,
                                       XmNtopWidget,               probability_frame,
                                       XmNtopOffset,               14,
                                       XmNbottomAttachment,        XmATTACH_NONE,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNrightAttachment,         XmATTACH_FORM,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    }



//----- Frame for signpost info
    else if (Signpost_object_enabled)
    {

      //fprintf(stderr,"Drawing signpost data\n");

      signpost_frame = XtVaCreateManagedWidget("Set_Del_Object signpost_frame",
                       xmFrameWidgetClass,
                       ob_form,
                       XmNtopAttachment,           XmATTACH_WIDGET,
                       XmNtopWidget,               map_view_toggle,
                       XmNtopOffset,               0,
                       XmNbottomAttachment,        XmATTACH_NONE,
                       XmNleftAttachment,          XmATTACH_FORM,
                       XmNleftOffset,              10,
                       XmNrightAttachment,         XmATTACH_FORM,
                       XmNrightOffset,             10,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       NULL);

      // "Signpost"
      // signpost_ts
      (void)XtVaCreateManagedWidget(langcode("POPUPOB031"),
                                    xmLabelWidgetClass,
                                    signpost_frame,
                                    XmNchildType,               XmFRAME_TITLE_CHILD,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    XmNfontList, fontlist1,
                                    NULL);

      signpost_form =  XtVaCreateWidget("Set_Del_Object signpost_form",
                                        xmFormWidgetClass,
                                        signpost_frame,
                                        XmNfractionBase,            5,
                                        XmNautoUnmanage,            FALSE,
                                        MY_FOREGROUND_COLOR,
                                        MY_BACKGROUND_COLOR,
                                        XmNfontList, fontlist1,
                                        NULL);

      // "Signpost Data"
      signpost_label = XtVaCreateManagedWidget(langcode("POPUPOB030"),
                       xmLabelWidgetClass,
                       signpost_form,
                       XmNtopAttachment,           XmATTACH_FORM,
                       XmNtopOffset,               8,
                       XmNbottomAttachment,        XmATTACH_FORM,
                       XmNbottomOffset,            10,
                       XmNleftAttachment,          XmATTACH_FORM,
                       XmNleftOffset,              10,
                       XmNrightAttachment,         XmATTACH_NONE,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
                       NULL);

      signpost_data = XtVaCreateManagedWidget("Set_Del_Object signpost_data",
                                              xmTextFieldWidgetClass,
                                              signpost_form,
                                              XmNeditable,                TRUE,
                                              XmNcursorPositionVisible,   TRUE,
                                              XmNsensitive,               TRUE,
                                              XmNshadowThickness,         1,
                                              XmNcolumns,                 3,
                                              XmNmaxLength,               3,
                                              XmNtopOffset,               3,
                                              XmNbackground,              colors[0x0f],
                                              XmNleftAttachment,          XmATTACH_WIDGET,
                                              XmNleftWidget,              signpost_label,
                                              XmNtopAttachment,           XmATTACH_FORM,
                                              XmNbottomAttachment,        XmATTACH_FORM,
                                              XmNrightAttachment,         XmATTACH_NONE,
                                              XmNfontList, fontlist1,
                                              NULL);

      ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep",
                                       xmSeparatorGadgetClass,
                                       ob_form,
                                       XmNorientation,             XmHORIZONTAL,
                                       XmNtopAttachment,           XmATTACH_WIDGET,
                                       XmNtopWidget,               signpost_frame,
                                       XmNtopOffset,               14,
                                       XmNbottomAttachment,        XmATTACH_NONE,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNrightAttachment,         XmATTACH_FORM,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    }



//----- Frame for area info
    else if (Area_object_enabled)
    {

      //fprintf(stderr,"Drawing Area data\n");

      area_frame = XtVaCreateManagedWidget("Set_Del_Object area_frame",
                                           xmFrameWidgetClass,
                                           ob_form,
                                           XmNtopAttachment,           XmATTACH_WIDGET,
                                           XmNtopWidget,               map_view_toggle,
                                           XmNtopOffset,               0,
                                           XmNbottomAttachment,        XmATTACH_NONE,
                                           XmNleftAttachment,          XmATTACH_FORM,
                                           XmNleftOffset,              10,
                                           XmNrightAttachment,         XmATTACH_FORM,
                                           XmNrightOffset,             10,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           NULL);

      // "Area Options"
      // area_ts
      (void)XtVaCreateManagedWidget(langcode("POPUPOB007"),
                                    xmLabelWidgetClass,
                                    area_frame,
                                    XmNchildType,               XmFRAME_TITLE_CHILD,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    XmNfontList, fontlist1,
                                    NULL);

      area_form =  XtVaCreateWidget("Set_Del_Object area_form",
                                    xmFormWidgetClass,
                                    area_frame,
                                    XmNfractionBase,            5,
                                    XmNautoUnmanage,            FALSE,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    NULL);

      // "Bright Color Enable"
      bright_dim_toggle = XtVaCreateManagedWidget(langcode("POPUPOB009"),
                          xmToggleButtonGadgetClass,
                          area_form,
                          XmNtopAttachment,           XmATTACH_FORM,
                          XmNtopOffset,               8,
                          XmNbottomAttachment,        XmATTACH_NONE,
                          XmNleftAttachment,          XmATTACH_FORM,
                          XmNleftOffset,              10,
                          XmNrightAttachment,         XmATTACH_NONE,
                          XmNset,                     FALSE,   // Select default to be OFF
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          XmNfontList, fontlist1,
                          NULL);
      XtAddCallback(bright_dim_toggle,XmNvalueChangedCallback,Area_bright_dim_toggle,"1");
      Area_bright = 0;    // Set to default each time dialog is created

      // "Color-Fill Enable"
      open_filled_toggle = XtVaCreateManagedWidget(langcode("POPUPOB010"),
                           xmToggleButtonGadgetClass,
                           area_form,
                           XmNtopAttachment,           XmATTACH_FORM,
                           XmNtopOffset,               8,
                           XmNbottomAttachment,        XmATTACH_NONE,
                           XmNleftAttachment,          XmATTACH_WIDGET,
                           XmNleftWidget,              bright_dim_toggle,
                           XmNleftOffset,              10,
                           XmNrightAttachment,         XmATTACH_NONE,
                           XmNset,                     FALSE,   // Select default to be OFF
                           MY_FOREGROUND_COLOR,
                           MY_BACKGROUND_COLOR,
                           XmNfontList, fontlist1,
                           NULL);
      XtAddCallback(open_filled_toggle,XmNvalueChangedCallback,Area_open_filled_toggle,"1");
      Area_filled = 0;    // Set to default each time dialog is created


// Shape of object
      ac = 0;
      XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
      ac++;
      XtSetArg(al[ac], XmNbackground, MY_BG_COLOR);
      ac++;

      shape_box = XmCreateRadioBox(area_form,
                                   "Set_Del_Object Shape Options box",
                                   al,
                                   ac);

      XtVaSetValues(shape_box,
                    XmNpacking, XmPACK_TIGHT,
                    XmNorientation, XmHORIZONTAL,
                    XmNtopAttachment,XmATTACH_WIDGET,
                    XmNtopWidget,  bright_dim_toggle,
                    XmNbottomAttachment,XmATTACH_NONE,
                    XmNleftAttachment, XmATTACH_FORM,
                    XmNleftOffset, 5,
                    XmNrightAttachment,XmATTACH_FORM,
                    XmNrightOffset, 5,
                    XmNnumColumns,1,
                    NULL);


      // "Circle"
      toption1 = XtVaCreateManagedWidget(langcode("POPUPOB011"),
                                         xmToggleButtonGadgetClass,
                                         shape_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(toption1,XmNvalueChangedCallback,Area_type_toggle,"0");

      // "Line-Right '/'"
      toption2 = XtVaCreateManagedWidget(langcode("POPUPOB013"),
                                         xmToggleButtonGadgetClass,
                                         shape_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(toption2,XmNvalueChangedCallback,Area_type_toggle,"1");

      // "Line-Left '\'
      toption3 = XtVaCreateManagedWidget(langcode("POPUPOB012"),
                                         xmToggleButtonGadgetClass,
                                         shape_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(toption3,XmNvalueChangedCallback,Area_type_toggle,"6");

      // "Triangle"
      toption4 = XtVaCreateManagedWidget(langcode("POPUPOB014"),
                                         xmToggleButtonGadgetClass,
                                         shape_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(toption4,XmNvalueChangedCallback,Area_type_toggle,"3");

      // "Rectangle"
      toption5 = XtVaCreateManagedWidget(langcode("POPUPOB015"),
                                         xmToggleButtonGadgetClass,
                                         shape_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(toption5,XmNvalueChangedCallback,Area_type_toggle,"4");


// Color of object
      color_box = XmCreateRadioBox(area_form,
                                   "Set_Del_Object Color Options box",
                                   al,
                                   ac);

      XtVaSetValues(color_box,
                    XmNpacking, XmPACK_TIGHT,
                    XmNorientation, XmHORIZONTAL,
                    XmNtopAttachment,XmATTACH_WIDGET,
                    XmNtopWidget, shape_box,
                    XmNbottomAttachment,XmATTACH_NONE,
                    XmNleftAttachment, XmATTACH_FORM,
                    XmNleftOffset, 5,
                    XmNrightAttachment,XmATTACH_FORM,
                    XmNrightOffset, 5,
                    NULL);


      XtVaSetValues(color_box,
                    XmNnumColumns,4,
                    NULL);

      // "Black"
      coption1 = XtVaCreateManagedWidget(langcode("POPUPOB016"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption1,XmNvalueChangedCallback,Area_color_toggle,"/0");

      // "Blue"
      coption2 = XtVaCreateManagedWidget(langcode("POPUPOB017"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption2,XmNvalueChangedCallback,Area_color_toggle,"/1");

      // "Green"
      coption3 = XtVaCreateManagedWidget(langcode("POPUPOB018"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption3,XmNvalueChangedCallback,Area_color_toggle,"/2");

      // "Cyan"
      coption4 = XtVaCreateManagedWidget(langcode("POPUPOB019"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption4,XmNvalueChangedCallback,Area_color_toggle,"/3");

      // "Red"
      coption5 = XtVaCreateManagedWidget(langcode("POPUPOB020"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption5,XmNvalueChangedCallback,Area_color_toggle,"/4");

      // "Violet"
      coption6 = XtVaCreateManagedWidget(langcode("POPUPOB021"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption6,XmNvalueChangedCallback,Area_color_toggle,"/5");

      // "Yellow"
      coption7 = XtVaCreateManagedWidget(langcode("POPUPOB022"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption7,XmNvalueChangedCallback,Area_color_toggle,"/6");

      // "Grey"
      coption8 = XtVaCreateManagedWidget(langcode("POPUPOB023"),
                                         xmToggleButtonGadgetClass,
                                         color_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(coption8,XmNvalueChangedCallback,Area_color_toggle,"/7");

// Latitude offset
      // "Offset Up"
      ob_lat_offset = XtVaCreateManagedWidget(langcode("POPUPOB024"),
                                              xmLabelWidgetClass,
                                              area_form,
                                              XmNtopAttachment,           XmATTACH_WIDGET,
                                              XmNtopWidget,               color_box,
                                              XmNtopOffset,               10,
                                              XmNbottomAttachment,        XmATTACH_FORM,
                                              XmNbottomOffset,            10,
                                              XmNleftAttachment,          XmATTACH_FORM,
                                              XmNleftOffset,              10,
                                              XmNrightAttachment,         XmATTACH_NONE,
                                              MY_FOREGROUND_COLOR,
                                              MY_BACKGROUND_COLOR,
                                              XmNfontList, fontlist1,
                                              NULL);

      ob_lat_offset_data = XtVaCreateManagedWidget("Set_Del_Object lat offset",
                           xmTextFieldWidgetClass,
                           area_form,
                           XmNeditable,                TRUE,
                           XmNcursorPositionVisible,   TRUE,
                           XmNsensitive,               TRUE,
                           XmNshadowThickness,         1,
                           XmNcolumns,                 4,
                           XmNmaxLength,               4,
                           XmNtopOffset,               5,
                           XmNbackground,              colors[0x0f],
                           XmNleftAttachment,          XmATTACH_WIDGET,
                           XmNleftWidget,              ob_lat_offset,
                           XmNtopAttachment,           XmATTACH_WIDGET,
                           XmNtopWidget,               color_box,
                           XmNbottomAttachment,        XmATTACH_FORM,
                           XmNrightAttachment,         XmATTACH_NONE,
                           XmNfontList, fontlist1,
                           NULL);

// Longitude offset
      // "Offset Left (except for '/')"
      ob_lon_offset = XtVaCreateManagedWidget(langcode("POPUPOB025"),
                                              xmLabelWidgetClass,
                                              area_form,
                                              XmNtopAttachment,           XmATTACH_WIDGET,
                                              XmNtopWidget,               color_box,
                                              XmNtopOffset,               10,
                                              XmNbottomAttachment,        XmATTACH_FORM,
                                              XmNbottomOffset,            10,
                                              XmNleftAttachment,          XmATTACH_WIDGET,
                                              XmNleftWidget,              ob_lat_offset_data,
                                              XmNleftOffset,              10,
                                              XmNrightAttachment,         XmATTACH_NONE,
                                              MY_FOREGROUND_COLOR,
                                              MY_BACKGROUND_COLOR,
                                              XmNfontList, fontlist1,
                                              NULL);

      ob_lon_offset_data = XtVaCreateManagedWidget("Set_Del_Object long offset",
                           xmTextFieldWidgetClass,
                           area_form,
                           XmNeditable,                TRUE,
                           XmNcursorPositionVisible,   TRUE,
                           XmNsensitive,               TRUE,
                           XmNshadowThickness,         1,
                           XmNcolumns,                 4,
                           XmNmaxLength,               4,
                           XmNtopOffset,               5,
                           XmNbackground,              colors[0x0f],
                           XmNleftAttachment,          XmATTACH_WIDGET,
                           XmNleftWidget,              ob_lon_offset,
                           XmNtopAttachment,           XmATTACH_WIDGET,
                           XmNtopWidget,               color_box,
                           XmNbottomAttachment,        XmATTACH_FORM,
                           XmNrightAttachment,         XmATTACH_NONE,
                           XmNfontList, fontlist1,
                           NULL);

      // "Corridor (Lines only)"
      ob_corridor = XtVaCreateManagedWidget(langcode("POPUPOB026"),
                                            xmLabelWidgetClass,
                                            area_form,
                                            XmNtopAttachment,           XmATTACH_WIDGET,
                                            XmNtopWidget,               color_box,
                                            XmNtopOffset,               10,
                                            XmNbottomAttachment,        XmATTACH_FORM,
                                            XmNbottomOffset,            10,
                                            XmNleftAttachment,          XmATTACH_WIDGET,
                                            XmNleftWidget,              ob_lon_offset_data,
                                            XmNleftOffset,              10,
                                            XmNrightAttachment,         XmATTACH_NONE,
                                            MY_FOREGROUND_COLOR,
                                            MY_BACKGROUND_COLOR,
                                            XmNfontList, fontlist1,
                                            NULL);

      ob_corridor_data = XtVaCreateManagedWidget("Set_Del_Object lat offset",
                         xmTextFieldWidgetClass,
                         area_form,
                         XmNeditable,                TRUE,
                         XmNcursorPositionVisible,   TRUE,
                         XmNsensitive,               TRUE,
                         XmNshadowThickness,         1,
                         XmNcolumns,                 3,
                         XmNmaxLength,               3,
                         XmNtopOffset,               5,
                         XmNbackground,              colors[0x0f],
                         XmNleftAttachment,          XmATTACH_WIDGET,
                         XmNleftWidget,              ob_corridor,
                         XmNtopAttachment,           XmATTACH_WIDGET,
                         XmNtopWidget,               color_box,
                         XmNbottomAttachment,        XmATTACH_FORM,
                         XmNrightAttachment,         XmATTACH_NONE,
                         XmNfontList, fontlist1,
                         NULL);

      // "Miles"
      ob_corridor_miles = XtVaCreateManagedWidget(langcode("UNIOP00004"),
                          xmLabelWidgetClass,
                          area_form,
                          XmNtopAttachment,           XmATTACH_WIDGET,
                          XmNtopWidget,               color_box,
                          XmNtopOffset,               10,
                          XmNbottomAttachment,        XmATTACH_FORM,
                          XmNbottomOffset,            10,
                          XmNleftAttachment,          XmATTACH_WIDGET,
                          XmNleftWidget,              ob_corridor_data,
                          XmNrightAttachment,         XmATTACH_NONE,
                          MY_FOREGROUND_COLOR,
                          MY_BACKGROUND_COLOR,
                          XmNfontList, fontlist1,
                          NULL);
      XtSetSensitive(ob_corridor,FALSE);
      XtSetSensitive(ob_corridor_data,FALSE);
      XtSetSensitive(ob_corridor_miles,FALSE);


      ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep",
                                       xmSeparatorGadgetClass,
                                       ob_form,
                                       XmNorientation,             XmHORIZONTAL,
                                       XmNtopAttachment,           XmATTACH_WIDGET,
                                       XmNtopWidget,               area_frame,
                                       XmNtopOffset,               14,
                                       XmNbottomAttachment,        XmATTACH_NONE,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNrightAttachment,         XmATTACH_FORM,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    }



//----- Frame for DF-omni info
    else if (DF_object_enabled)
    {

      //fprintf(stderr,"Drawing DF data\n");

      // "Omni Antenna"
      omni_antenna_toggle = XtVaCreateManagedWidget(langcode("POPUPOB041"),
                            xmToggleButtonGadgetClass,
                            ob_form,
                            XmNtopAttachment,           XmATTACH_WIDGET,
                            XmNtopWidget,               signpost_toggle,
                            XmNtopOffset,               0,
                            XmNbottomAttachment,        XmATTACH_NONE,
                            XmNbottomOffset,            0,
                            XmNleftAttachment,          XmATTACH_WIDGET,
                            XmNleftWidget,              area_toggle,
                            XmNleftOffset,              10,
                            XmNrightAttachment,         XmATTACH_NONE,
                            MY_FOREGROUND_COLOR,
                            MY_BACKGROUND_COLOR,
                            XmNfontList, fontlist1,
                            NULL);
      XtAddCallback(omni_antenna_toggle,XmNvalueChangedCallback,Omni_antenna_toggle,(XtPointer)p_station);


      // "Beam Antenna"
      beam_antenna_toggle = XtVaCreateManagedWidget(langcode("POPUPOB042"),
                            xmToggleButtonGadgetClass,
                            ob_form,
                            XmNtopAttachment,           XmATTACH_WIDGET,
                            XmNtopWidget,               omni_antenna_toggle,
                            XmNtopOffset,               0,
                            XmNbottomAttachment,        XmATTACH_NONE,
                            XmNbottomOffset,            0,
                            XmNleftAttachment,          XmATTACH_WIDGET,
                            XmNleftWidget,              area_toggle,
                            XmNleftOffset,              10,
                            XmNrightAttachment,         XmATTACH_NONE,
                            MY_FOREGROUND_COLOR,
                            MY_BACKGROUND_COLOR,
                            XmNfontList, fontlist1,
                            NULL);
      XtAddCallback(beam_antenna_toggle,XmNvalueChangedCallback,Beam_antenna_toggle,(XtPointer)p_station);


      frameomni = XtVaCreateManagedWidget("Set_Del_Object frameomni",
                                          xmFrameWidgetClass,
                                          ob_form,
                                          XmNtopAttachment,           XmATTACH_WIDGET,
                                          XmNtopWidget,               map_view_toggle,
                                          XmNtopOffset,               0,
                                          XmNbottomAttachment,        XmATTACH_NONE,
                                          XmNleftAttachment,          XmATTACH_FORM,
                                          XmNleftOffset,              10,
                                          XmNrightAttachment,         XmATTACH_FORM,
                                          XmNrightOffset,             10,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          NULL);

      // omnilabel
      (void)XtVaCreateManagedWidget(langcode("POPUPOB039"),
                                    xmLabelWidgetClass,
                                    frameomni,
                                    XmNchildType,               XmFRAME_TITLE_CHILD,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    XmNfontList, fontlist1,
                                    NULL);

      formomni =  XtVaCreateWidget("Set_Del_Object formomni",
                                   xmFormWidgetClass,
                                   frameomni,
                                   XmNfractionBase,            5,
                                   MY_FOREGROUND_COLOR,
                                   MY_BACKGROUND_COLOR,
                                   XmNfontList, fontlist1,
                                   NULL);


      // Power
      ac = 0;
      XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
      ac++;
      XtSetArg(al[ac], XmNbackground, MY_BG_COLOR);
      ac++;

      signal_box = XmCreateRadioBox(formomni,
                                    "Set_Del_Object Power Radio Box",
                                    al,
                                    ac);

      XtVaSetValues(signal_box,
                    XmNpacking,               XmPACK_TIGHT,
                    XmNorientation,           XmHORIZONTAL,
                    XmNtopAttachment,         XmATTACH_FORM,
                    XmNtopOffset,             5,
                    XmNbottomAttachment,      XmATTACH_NONE,
                    XmNleftAttachment,        XmATTACH_FORM,
                    XmNleftOffset,            5,
                    XmNrightAttachment,       XmATTACH_FORM,
                    XmNrightOffset,           5,
                    XmNnumColumns,            11,
                    NULL);

      // No signal detected what-so-ever
      soption0 = XtVaCreateManagedWidget("0",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption0,XmNvalueChangedCallback,Ob_signal_toggle,"0");

      // Detectible signal (Maybe)
      soption1 = XtVaCreateManagedWidget("1",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption1,XmNvalueChangedCallback,Ob_signal_toggle,"1");

      // Detectible signal (certain but not copyable)
      soption2 = XtVaCreateManagedWidget("2",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption2,XmNvalueChangedCallback,Ob_signal_toggle,"2");

      // Weak signal marginally readable
      soption3 = XtVaCreateManagedWidget("3",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption3,XmNvalueChangedCallback,Ob_signal_toggle,"3");

      // Noisy but copyable
      soption4 = XtVaCreateManagedWidget("4",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption4,XmNvalueChangedCallback,Ob_signal_toggle,"4");

      // Some noise but easy to copy
      soption5 = XtVaCreateManagedWidget("5",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption5,XmNvalueChangedCallback,Ob_signal_toggle,"5");

      // Good signal with detectible noise
      soption6 = XtVaCreateManagedWidget("6",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption6,XmNvalueChangedCallback,Ob_signal_toggle,"6");

      // Near full-quieting signal
      soption7 = XtVaCreateManagedWidget("7",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption7,XmNvalueChangedCallback,Ob_signal_toggle,"7");

      // Dead full-quieting signal, no noise detectible
      soption8 = XtVaCreateManagedWidget("8",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption8,XmNvalueChangedCallback,Ob_signal_toggle,"8");

      // Extremely strong signal "pins the meter"
      soption9 = XtVaCreateManagedWidget("9",
                                         xmToggleButtonGadgetClass,
                                         signal_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(soption9,XmNvalueChangedCallback,Ob_signal_toggle,"9");


      // Height
      height_box = XmCreateRadioBox(formomni,
                                    "Set_Del_Object Height Radio Box",
                                    al,
                                    ac);

      XtVaSetValues(height_box,
                    XmNpacking, XmPACK_TIGHT,
                    XmNorientation, XmHORIZONTAL,
                    XmNtopAttachment,XmATTACH_WIDGET,
                    XmNtopWidget,signal_box,
                    XmNbottomAttachment,XmATTACH_NONE,
                    XmNleftAttachment, XmATTACH_FORM,
                    XmNleftOffset, 5,
                    XmNrightAttachment,XmATTACH_FORM,
                    XmNrightOffset, 5,
                    XmNnumColumns,10,
                    NULL);


      // 10 Feet
      hoption0 = XtVaCreateManagedWidget(
                   (english_units) ? "10ft" : "3m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption0,XmNvalueChangedCallback,Ob_height_toggle,"0");

      // 20 Feet
      hoption1 = XtVaCreateManagedWidget(
                   (english_units) ? "20ft" : "6m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption1,XmNvalueChangedCallback,Ob_height_toggle,"1");

      // 40 Feet
      hoption2 = XtVaCreateManagedWidget(
                   (english_units) ? "40ft" : "12m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption2,XmNvalueChangedCallback,Ob_height_toggle,"2");

      // 80 Feet
      hoption3 = XtVaCreateManagedWidget(
                   (english_units) ? "80ft" : "24m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption3,XmNvalueChangedCallback,Ob_height_toggle,"3");

      // 160 Feet
      hoption4 = XtVaCreateManagedWidget(
                   (english_units) ? "160ft" : "49m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption4,XmNvalueChangedCallback,Ob_height_toggle,"4");

      // 320 Feet
      hoption5 = XtVaCreateManagedWidget(
                   (english_units) ? "320ft" : "98m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption5,XmNvalueChangedCallback,Ob_height_toggle,"5");

      // 640 Feet
      hoption6 = XtVaCreateManagedWidget(
                   (english_units) ? "640ft" : "195m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption6,XmNvalueChangedCallback,Ob_height_toggle,"6");

      // 1280 Feet
      hoption7 = XtVaCreateManagedWidget(
                   (english_units) ? "1280ft" : "390m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption7,XmNvalueChangedCallback,Ob_height_toggle,"7");

      // 2560 Feet
      hoption8 = XtVaCreateManagedWidget(
                   (english_units) ? "2560ft" : "780m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption8,XmNvalueChangedCallback,Ob_height_toggle,"8");

      // 5120 Feet
      hoption9 = XtVaCreateManagedWidget(
                   (english_units) ? "5120ft" : "1561m",
                   xmToggleButtonGadgetClass,
                   height_box,
                   MY_FOREGROUND_COLOR,
                   MY_BACKGROUND_COLOR,
                   XmNfontList, fontlist1,
                   NULL);
      XtAddCallback(hoption9,XmNvalueChangedCallback,Ob_height_toggle,"9");


      // Gain
      gain_box = XmCreateRadioBox(formomni,
                                  "Set_Del_Object Gain Radio Box",
                                  al,
                                  ac);

      XtVaSetValues(gain_box,
                    XmNpacking, XmPACK_TIGHT,
                    XmNorientation, XmHORIZONTAL,
                    XmNtopAttachment,XmATTACH_WIDGET,
                    XmNtopWidget,height_box,
                    XmNbottomAttachment,XmATTACH_NONE,
                    XmNleftAttachment, XmATTACH_FORM,
                    XmNleftOffset, 5,
                    XmNrightAttachment,XmATTACH_FORM,
                    XmNrightOffset, 5,
                    XmNnumColumns,10,
                    NULL);


      // 0 dB
      goption0 = XtVaCreateManagedWidget("0dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption0,XmNvalueChangedCallback,Ob_gain_toggle,"0");

      // 1 dB
      goption1 = XtVaCreateManagedWidget("1dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption1,XmNvalueChangedCallback,Ob_gain_toggle,"1");

      // 2 dB
      goption2 = XtVaCreateManagedWidget("2dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption2,XmNvalueChangedCallback,Ob_gain_toggle,"2");

      // 3 dB
      goption3 = XtVaCreateManagedWidget("3dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption3,XmNvalueChangedCallback,Ob_gain_toggle,"3");

      // 4 dB
      goption4 = XtVaCreateManagedWidget("4dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption4,XmNvalueChangedCallback,Ob_gain_toggle,"4");

      // 5 dB
      goption5 = XtVaCreateManagedWidget("5dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption5,XmNvalueChangedCallback,Ob_gain_toggle,"5");

      // 6 dB
      goption6 = XtVaCreateManagedWidget("6dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption6,XmNvalueChangedCallback,Ob_gain_toggle,"6");

      // 7 dB
      goption7 = XtVaCreateManagedWidget("7dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption7,XmNvalueChangedCallback,Ob_gain_toggle,"7");

      // 8 dB
      goption8 = XtVaCreateManagedWidget("8dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption8,XmNvalueChangedCallback,Ob_gain_toggle,"8");

      // 9 dB
      goption9 = XtVaCreateManagedWidget("9dB",
                                         xmToggleButtonGadgetClass,
                                         gain_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(goption9,XmNvalueChangedCallback,Ob_gain_toggle,"9");


      // Gain
      directivity_box = XmCreateRadioBox(formomni,
                                         "Set_Del_Object Directivity Radio Box",
                                         al,
                                         ac);

      XtVaSetValues(directivity_box,
                    XmNpacking, XmPACK_TIGHT,
                    XmNorientation, XmHORIZONTAL,
                    XmNtopAttachment,XmATTACH_WIDGET,
                    XmNtopWidget,gain_box,
                    XmNbottomAttachment,XmATTACH_NONE,
                    XmNleftAttachment, XmATTACH_FORM,
                    XmNleftOffset, 5,
                    XmNrightAttachment,XmATTACH_FORM,
                    XmNrightOffset, 5,
                    XmNnumColumns,10,
                    NULL);


      // Omni-directional
      doption0 = XtVaCreateManagedWidget(langcode("WPUPCFS016"),
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption0,XmNvalueChangedCallback,Ob_directivity_toggle,"0");

      // 45 NE
      doption1 = XtVaCreateManagedWidget("45\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption1,XmNvalueChangedCallback,Ob_directivity_toggle,"1");

      // 90 E
      doption2 = XtVaCreateManagedWidget("90\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption2,XmNvalueChangedCallback,Ob_directivity_toggle,"2");

      // 135 SE
      doption3 = XtVaCreateManagedWidget("135\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption3,XmNvalueChangedCallback,Ob_directivity_toggle,"3");

      // 180 S
      doption4 = XtVaCreateManagedWidget("180\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption4,XmNvalueChangedCallback,Ob_directivity_toggle,"4");

      // 225 SW
      doption5 = XtVaCreateManagedWidget("225\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption5,XmNvalueChangedCallback,Ob_directivity_toggle,"5");

      // 270 W
      doption6 = XtVaCreateManagedWidget("270\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption6,XmNvalueChangedCallback,Ob_directivity_toggle,"6");

      // 315 NW
      doption7 = XtVaCreateManagedWidget("315\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption7,XmNvalueChangedCallback,Ob_directivity_toggle,"7");

      // 360 N
      doption8 = XtVaCreateManagedWidget("360\xB0",
                                         xmToggleButtonGadgetClass,
                                         directivity_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(doption8,XmNvalueChangedCallback,Ob_directivity_toggle,"8");


//----- Frame for DF-beam info
      framebeam = XtVaCreateManagedWidget("Set_Del_Object framebeam",
                                          xmFrameWidgetClass,
                                          ob_form,
                                          XmNtopAttachment,           XmATTACH_WIDGET,
                                          XmNtopWidget,               frameomni,
                                          XmNtopOffset,               10,
                                          XmNbottomAttachment,        XmATTACH_NONE,
                                          XmNleftAttachment,          XmATTACH_FORM,
                                          XmNleftOffset,              10,
                                          XmNrightAttachment,         XmATTACH_FORM,
                                          XmNrightOffset,             10,
                                          MY_FOREGROUND_COLOR,
                                          MY_BACKGROUND_COLOR,
                                          NULL);

      // beamlabel
      (void)XtVaCreateManagedWidget(langcode("POPUPOB040"),
                                    xmLabelWidgetClass,
                                    framebeam,
                                    XmNchildType,               XmFRAME_TITLE_CHILD,
                                    MY_FOREGROUND_COLOR,
                                    MY_BACKGROUND_COLOR,
                                    XmNfontList, fontlist1,
                                    NULL);

      formbeam =  XtVaCreateWidget("Set_Del_Object formbeam",
                                   xmFormWidgetClass,
                                   framebeam,
                                   XmNfractionBase,            5,
                                   MY_FOREGROUND_COLOR,
                                   MY_BACKGROUND_COLOR,
                                   NULL);


      // Beam width
      ac = 0;
      XtSetArg(al[ac], XmNforeground, MY_FG_COLOR);
      ac++;
      XtSetArg(al[ac], XmNbackground, MY_BG_COLOR);
      ac++;

      width_box = XmCreateRadioBox(formbeam,
                                   "Set_Del_Object Width Box",
                                   al,
                                   ac);

      XtVaSetValues(width_box,
                    XmNpacking,               XmPACK_TIGHT,
                    XmNorientation,           XmHORIZONTAL,
                    XmNtopAttachment,         XmATTACH_FORM,
                    XmNtopOffset,             5,
                    XmNbottomAttachment,      XmATTACH_NONE,
                    XmNleftAttachment,        XmATTACH_FORM,
                    XmNleftOffset,            5,
                    XmNrightAttachment,       XmATTACH_FORM,
                    XmNrightOffset,           5,
                    XmNnumColumns,            11,
                    NULL);

      // Useless
      woption0 = XtVaCreateManagedWidget(langcode("POPUPOB043"),
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption0,XmNvalueChangedCallback,Ob_width_toggle,"0");

      // < 240 Degrees
      woption1 = XtVaCreateManagedWidget("<240\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption1,XmNvalueChangedCallback,Ob_width_toggle,"1");

      // < 120 Degrees
      woption2 = XtVaCreateManagedWidget("<120\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption2,XmNvalueChangedCallback,Ob_width_toggle,"2");

      // < 64 Degrees
      woption3 = XtVaCreateManagedWidget("<64\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption3,XmNvalueChangedCallback,Ob_width_toggle,"3");

      // < 32 Degrees
      woption4 = XtVaCreateManagedWidget("<32\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption4,XmNvalueChangedCallback,Ob_width_toggle,"4");

      // < 16 Degrees
      woption5 = XtVaCreateManagedWidget("<16\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption5,XmNvalueChangedCallback,Ob_width_toggle,"5");

      // < 8 Degrees
      woption6 = XtVaCreateManagedWidget("<8\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption6,XmNvalueChangedCallback,Ob_width_toggle,"6");

      // < 4 Degrees
      woption7 = XtVaCreateManagedWidget("<4\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption7,XmNvalueChangedCallback,Ob_width_toggle,"7");

      // < 2 Degrees
      woption8 = XtVaCreateManagedWidget("<2\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption8,XmNvalueChangedCallback,Ob_width_toggle,"8");

      // < 1 Degrees
      woption9 = XtVaCreateManagedWidget("<1\xB0",
                                         xmToggleButtonGadgetClass,
                                         width_box,
                                         MY_FOREGROUND_COLOR,
                                         MY_BACKGROUND_COLOR,
                                         XmNfontList, fontlist1,
                                         NULL);
      XtAddCallback(woption9,XmNvalueChangedCallback,Ob_width_toggle,"9");


      // "Bearing"
      ob_bearing = XtVaCreateManagedWidget(langcode("POPUPOB046"),
                                           xmLabelWidgetClass,
                                           formbeam,
                                           XmNtopAttachment,           XmATTACH_WIDGET,
                                           XmNtopWidget,               width_box,
                                           XmNtopOffset,               10,
                                           XmNbottomAttachment,        XmATTACH_NONE,
                                           XmNleftAttachment,          XmATTACH_FORM,
                                           XmNleftOffset,              10,
                                           XmNrightAttachment,         XmATTACH_NONE,
                                           MY_FOREGROUND_COLOR,
                                           MY_BACKGROUND_COLOR,
                                           XmNfontList, fontlist1,
                                           NULL);
      // Bearing data
      ob_bearing_data = XtVaCreateManagedWidget("Set_Del_Object ob_bearing_data",
                        xmTextFieldWidgetClass,
                        formbeam,
                        XmNeditable,                TRUE,
                        XmNcursorPositionVisible,   TRUE,
                        XmNsensitive,               TRUE,
                        XmNshadowThickness,         1,
                        XmNcolumns,                 9,
                        XmNmaxLength,               9,
                        XmNtopAttachment,           XmATTACH_WIDGET,
                        XmNtopWidget,               width_box,
                        XmNtopOffset,               5,
                        XmNbottomAttachment,        XmATTACH_NONE,
                        XmNleftAttachment,          XmATTACH_WIDGET,
                        XmNleftWidget,              ob_bearing,
                        XmNrightAttachment,         XmATTACH_NONE,
                        XmNbackground,              colors[0x0f],
                        XmNfontList, fontlist1,
                        NULL);


      XtSetSensitive(frameomni,FALSE);
      XtSetSensitive(framebeam,FALSE);
      Omni_antenna_enabled = 0;
      Beam_antenna_enabled = 0;


      ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep",
                                       xmSeparatorGadgetClass,
                                       ob_form,
                                       XmNorientation,             XmHORIZONTAL,
                                       XmNtopAttachment,           XmATTACH_WIDGET,
                                       XmNtopWidget,               framebeam,
                                       XmNtopOffset,               14,
                                       XmNbottomAttachment,        XmATTACH_NONE,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNrightAttachment,         XmATTACH_FORM,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    }
// End of DF-specific widgets



//----- No Special options selected.  We need a widget here for the next widget to attach to.
    if (!DF_object_enabled
        && !Area_object_enabled
        && !Signpost_object_enabled
        && !Probability_circles_enabled)
    {

      //fprintf(stderr,"No special object types\n");

      ob_sep = XtVaCreateManagedWidget("Set_Del_Object ob_sep",
                                       xmSeparatorGadgetClass,
                                       ob_form,
                                       XmNorientation,             XmHORIZONTAL,
                                       XmNtopAttachment,           XmATTACH_WIDGET,
                                       XmNtopWidget,               map_view_toggle,
                                       XmNtopOffset,               0,
                                       XmNbottomAttachment,        XmATTACH_NONE,
                                       XmNleftAttachment,          XmATTACH_FORM,
                                       XmNrightAttachment,         XmATTACH_FORM,
                                       MY_FOREGROUND_COLOR,
                                       MY_BACKGROUND_COLOR,
                                       XmNfontList, fontlist1,
                                       NULL);
    }



//----- Buttons
    if (p_station != NULL)    // We were called from the Modify_object() or Move function
    {

      // Change the buttons/callbacks based on whether we're dealing with an item or an object
      if ((p_station->flag & ST_ITEM) != 0)       // Modifying an Item
      {
        // Here we need Modify Item/Delete Item/Cancel buttons
        ob_button_set = XtVaCreateManagedWidget(langcode("POPUPOB034"),
                                                xmPushButtonGadgetClass,
                                                ob_form,
                                                XmNtopAttachment,           XmATTACH_WIDGET,
                                                XmNtopWidget,               ob_sep,
                                                XmNtopOffset,               5,
                                                XmNbottomAttachment,        XmATTACH_FORM,
                                                XmNbottomOffset,            5,
                                                XmNleftAttachment,          XmATTACH_POSITION,
                                                XmNleftPosition,            0,
                                                XmNrightAttachment,         XmATTACH_POSITION,
                                                XmNrightPosition,           1,
                                                XmNnavigationType,          XmTAB_GROUP,
                                                MY_FOREGROUND_COLOR,
                                                MY_BACKGROUND_COLOR,
                                                XmNfontList, fontlist1,
                                                NULL);
        XtAddCallback(ob_button_set, XmNactivateCallback, Item_change_data_set, object_dialog);

        // Check whether we own this item
        if (strcasecmp(p_station->origin,my_callsign)==0)
        {

          // We own this item, set up the "Delete"
          // button.
          ob_button_del = XtVaCreateManagedWidget(langcode("POPUPOB033"),
                                                  xmPushButtonGadgetClass,
                                                  ob_form,
                                                  XmNtopAttachment,           XmATTACH_WIDGET,
                                                  XmNtopWidget,               ob_sep,
                                                  XmNtopOffset,               5,
                                                  XmNbottomAttachment,        XmATTACH_FORM,
                                                  XmNbottomOffset,            5,
                                                  XmNleftAttachment,          XmATTACH_POSITION,
                                                  XmNleftPosition,            1,
                                                  XmNrightAttachment,         XmATTACH_POSITION,
                                                  XmNrightPosition,           2,
                                                  XmNnavigationType,          XmTAB_GROUP,
                                                  MY_FOREGROUND_COLOR,
                                                  MY_BACKGROUND_COLOR,
                                                  XmNfontList, fontlist1,
                                                  NULL);
          XtAddCallback(ob_button_del, XmNactivateCallback, Item_change_data_del, object_dialog);
        }
        else
        {

          // Somebody else owns this item, set up the
          // "Adopt" button.
          ob_button_del = XtVaCreateManagedWidget(langcode("POPUPOB045"),
                                                  xmPushButtonGadgetClass,
                                                  ob_form,
                                                  XmNtopAttachment,           XmATTACH_WIDGET,
                                                  XmNtopWidget,               ob_sep,
                                                  XmNtopOffset,               5,
                                                  XmNbottomAttachment,        XmATTACH_FORM,
                                                  XmNbottomOffset,            5,
                                                  XmNleftAttachment,          XmATTACH_POSITION,
                                                  XmNleftPosition,            1,
                                                  XmNrightAttachment,         XmATTACH_POSITION,
                                                  XmNrightPosition,           2,
                                                  XmNnavigationType,          XmTAB_GROUP,
                                                  MY_FOREGROUND_COLOR,
                                                  MY_BACKGROUND_COLOR,
                                                  XmNfontList, fontlist1,
                                                  NULL);
          XtAddCallback(ob_button_del, XmNactivateCallback, Item_change_data_set, object_dialog);
        }
      }
      else    // Modifying an Object
      {
        // Here we need Modify Object/Delete Object/Cancel buttons
        ob_button_set = XtVaCreateManagedWidget(langcode("POPUPOB005"),
                                                xmPushButtonGadgetClass,
                                                ob_form,
                                                XmNtopAttachment,           XmATTACH_WIDGET,
                                                XmNtopWidget,               ob_sep,
                                                XmNtopOffset,               5,
                                                XmNbottomAttachment,        XmATTACH_FORM,
                                                XmNbottomOffset,            5,
                                                XmNleftAttachment,          XmATTACH_POSITION,
                                                XmNleftPosition,            0,
                                                XmNrightAttachment,         XmATTACH_POSITION,
                                                XmNrightPosition,           1,
                                                XmNnavigationType,          XmTAB_GROUP,
                                                MY_FOREGROUND_COLOR,
                                                MY_BACKGROUND_COLOR,
                                                XmNfontList, fontlist1,
                                                NULL);
        XtAddCallback(ob_button_set, XmNactivateCallback, Object_change_data_set, object_dialog);

        // Check whether we own this Object
        if (strcasecmp(p_station->origin,my_callsign)==0)
        {

          // We own this object, set up the "Delete"
          // button.
          ob_button_del = XtVaCreateManagedWidget(langcode("POPUPOB004"),
                                                  xmPushButtonGadgetClass,
                                                  ob_form,
                                                  XmNtopAttachment,           XmATTACH_WIDGET,
                                                  XmNtopWidget,               ob_sep,
                                                  XmNtopOffset,               5,
                                                  XmNbottomAttachment,        XmATTACH_FORM,
                                                  XmNbottomOffset,            5,
                                                  XmNleftAttachment,          XmATTACH_POSITION,
                                                  XmNleftPosition,            1,
                                                  XmNrightAttachment,         XmATTACH_POSITION,
                                                  XmNrightPosition,           2,
                                                  XmNnavigationType,          XmTAB_GROUP,
                                                  MY_FOREGROUND_COLOR,
                                                  MY_BACKGROUND_COLOR,
                                                  XmNfontList, fontlist1,
                                                  NULL);
          XtAddCallback(ob_button_del, XmNactivateCallback, Object_change_data_del, object_dialog);
        }
        else
        {

          // Somebody else owns this object, set up the
          // "Adopt" button.
          ob_button_del = XtVaCreateManagedWidget(langcode("POPUPOB044"),
                                                  xmPushButtonGadgetClass,
                                                  ob_form,
                                                  XmNtopAttachment,           XmATTACH_WIDGET,
                                                  XmNtopWidget,               ob_sep,
                                                  XmNtopOffset,               5,
                                                  XmNbottomAttachment,        XmATTACH_FORM,
                                                  XmNbottomOffset,            5,
                                                  XmNleftAttachment,          XmATTACH_POSITION,
                                                  XmNleftPosition,            1,
                                                  XmNrightAttachment,         XmATTACH_POSITION,
                                                  XmNrightPosition,           2,
                                                  XmNnavigationType,          XmTAB_GROUP,
                                                  MY_FOREGROUND_COLOR,
                                                  MY_BACKGROUND_COLOR,
                                                  XmNfontList, fontlist1,
                                                  NULL);
          XtAddCallback(ob_button_del, XmNactivateCallback, Object_change_data_set, object_dialog);
        }
      }
    }
    else    // We were called from Create->Object mouse menu
    {
      ob_button_set = XtVaCreateManagedWidget(langcode("POPUPOB003"),xmPushButtonGadgetClass, ob_form,
                                              XmNtopAttachment,           XmATTACH_WIDGET,
                                              XmNtopWidget,               ob_sep,
                                              XmNtopOffset,               5,
                                              XmNbottomAttachment,        XmATTACH_FORM,
                                              XmNbottomOffset,            5,
                                              XmNleftAttachment,          XmATTACH_POSITION,
                                              XmNleftPosition,            0,
                                              XmNrightAttachment,         XmATTACH_POSITION,
                                              XmNrightPosition,           1,
                                              XmNnavigationType,          XmTAB_GROUP,
                                              MY_FOREGROUND_COLOR,
                                              MY_BACKGROUND_COLOR,
                                              XmNfontList, fontlist1,
                                              NULL);

      it_button_set = XtVaCreateManagedWidget(langcode("POPUPOB006"),xmPushButtonGadgetClass, ob_form,
                                              XmNtopAttachment,           XmATTACH_WIDGET,
                                              XmNtopWidget,               ob_sep,
                                              XmNtopOffset,               5,
                                              XmNbottomAttachment,        XmATTACH_FORM,
                                              XmNbottomOffset,            5,
                                              XmNleftAttachment,          XmATTACH_POSITION,
                                              XmNleftPosition,            1,
                                              XmNrightAttachment,         XmATTACH_POSITION,
                                              XmNrightPosition,           2,
                                              XmNnavigationType,          XmTAB_GROUP,
                                              MY_FOREGROUND_COLOR,
                                              MY_BACKGROUND_COLOR,
                                              XmNfontList, fontlist1,
                                              NULL);


      // Changed to different callback routines here which
      // check the new object/item name against our internal
      // database then call
      // Object_change_data_set/Item_change_data_set if all
      // ok.  If a conflict (object/item already exists), do a
      // popup_message() instead or bring up a confirmation
      // dialog before creating the object/item.
      //
      //XtAddCallback(ob_button_set,
      //    XmNactivateCallback,
      //    Object_change_data_set,
      //    object_dialog);
      //XtAddCallback(it_button_set,
      //    XmNactivateCallback,
      //    Item_change_data_set,
      //    object_dialog);
      XtAddCallback(ob_button_set,
                    XmNactivateCallback,
                    Object_confirm_data_set,
                    object_dialog);
      XtAddCallback(it_button_set,
                    XmNactivateCallback,
                    Item_confirm_data_set,
                    object_dialog);
    }

    ob_button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"),xmPushButtonGadgetClass, ob_form,
                       XmNtopAttachment,           XmATTACH_WIDGET,
                       XmNtopWidget,               ob_sep,
                       XmNtopOffset,               5,
                       XmNbottomAttachment,        XmATTACH_FORM,
                       XmNbottomOffset,            5,
                       XmNleftAttachment,          XmATTACH_POSITION,
                       XmNleftPosition,            2,
                       XmNrightAttachment,         XmATTACH_POSITION,
                       XmNrightPosition,           3,
                       XmNnavigationType,          XmTAB_GROUP,
                       MY_FOREGROUND_COLOR,
                       MY_BACKGROUND_COLOR,
                       XmNfontList, fontlist1,
                       NULL);
    XtAddCallback(ob_button_cancel, XmNactivateCallback, Object_destroy_shell,   object_dialog);


    // Set ToggleButtons for the current state.  Don't notify the callback
    // functions associated with them or we'll be in an infinite loop.
    if (Area_object_enabled)
    {
      XmToggleButtonSetState(area_toggle, TRUE, FALSE);
    }
    if (Signpost_object_enabled)
    {
      XmToggleButtonSetState(signpost_toggle, TRUE, FALSE);
    }
    if (DF_object_enabled)
    {
      XmToggleButtonSetState(df_bearing_toggle, TRUE, FALSE);
    }
    if (Map_View_object_enabled)
    {
      XmToggleButtonSetState(map_view_toggle, TRUE, FALSE);
    }
    if (Probability_circles_enabled)
    {
      XmToggleButtonSetState(probabilities_toggle, TRUE, FALSE);
    }


// Fill in current data if object already exists
    if (p_station != NULL)    // We were called from the Modify_object() or Move functions
    {

      // Don't allow changing types if the object is already created.
      // Already tried allowing it, and it causes other problems.
      XtSetSensitive(area_toggle, FALSE);
      XtSetSensitive(signpost_toggle, FALSE);
      XtSetSensitive(df_bearing_toggle, FALSE);
      XtSetSensitive(map_view_toggle, FALSE);
      XtSetSensitive(probabilities_toggle, FALSE);

      XmTextFieldSetString(object_name_data,p_station->call_sign);
      // Need to make the above field non-editable 'cuz we're trying to modify
      // _parameters_ of the object and the name has to stay the same in order
      // to do this.  Change the name and we'll be creating a new object instead
      // of modifying an old one.
      XtSetSensitive(object_name_data, FALSE);
      // Would be nice to change the colors too

      // Check for overlay character
      if (p_station->aprs_symbol.special_overlay)
      {
        // Found an overlay character
        temp_data[0] = p_station->aprs_symbol.special_overlay;
      }
      else    // No overlay character
      {
        temp_data[0] = p_station->aprs_symbol.aprs_type;
      }
      temp_data[1] = '\0';
      XmTextFieldSetString(object_group_data,temp_data);

      temp_data[0] = p_station->aprs_symbol.aprs_symbol;
      temp_data[1] = '\0';
      XmTextFieldSetString(object_symbol_data,temp_data);

      // We only check the first possible comment string in
      // the record
      //if (strlen(p_station->comments) > 0)
      if (Map_View_object_enabled)
      {

        if ( (p_station->comment_data != NULL)
             && (p_station->comment_data->text_ptr != NULL) )
        {
          char temp[100];

          xastir_snprintf(temp,
                          sizeof(temp),
                          "%s%s",
                          p_station->power_gain,
                          p_station->comment_data->text_ptr);
          XmTextFieldSetString(object_comment_data,temp);
        }
        else
        {
          XmTextFieldSetString(object_comment_data,p_station->power_gain);
        }
      }

      else if ( (p_station->comment_data != NULL)
                && (p_station->comment_data->text_ptr != NULL) )
      {
        XmTextFieldSetString(object_comment_data,p_station->comment_data->text_ptr);
      }

      else
      {
        XmTextFieldSetString(object_comment_data,"");
      }


//            if ( (p_station->aprs_symbol.area_object.type != AREA_NONE) // Found an area object
//                    && Area_object_enabled ) {
      if (Area_object_enabled)
      {

        XtSetSensitive(ob_frame,FALSE);
        XtSetSensitive(area_frame,TRUE);

        switch (p_station->aprs_symbol.area_object.type)
        {
          case (1):   // Line '/'
            XmToggleButtonSetState(toption2, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
            if (p_station->aprs_symbol.area_object.corridor_width > 0)
              xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                              p_station->aprs_symbol.area_object.corridor_width );
            else
            {
              temp_data[0] = '\0';  // Empty string
            }

            XmTextFieldSetString( ob_corridor_data, temp_data );
            break;
          case (6):   // Line '\'
            XmToggleButtonGadgetSetState(toption3, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
            if (p_station->aprs_symbol.area_object.corridor_width > 0)
              xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                              p_station->aprs_symbol.area_object.corridor_width );
            else
            {
              temp_data[0] = '\0';  // Empty string
            }

            XmTextFieldSetString( ob_corridor_data, temp_data );
            break;
          case (3):   // Open Triangle
            XmToggleButtonGadgetSetState(toption4, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
            break;
          case (4):   // Open Rectangle
            XmToggleButtonGadgetSetState(toption5, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
            break;
          case (5):   // Filled Circle
            XmToggleButtonGadgetSetState(toption1, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, TRUE, TRUE);
            break;
          case (8):   // Filled Triangle
            XmToggleButtonGadgetSetState(toption4, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, TRUE, TRUE);
            break;
          case (9):   // Filled Rectangle
            XmToggleButtonGadgetSetState(toption5, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, TRUE, TRUE);
            break;
          case (0):   // Open Circle
          default:
            XmToggleButtonGadgetSetState(toption1, TRUE, TRUE);
            XmToggleButtonGadgetSetState(open_filled_toggle, FALSE, TRUE);
            break;
        }

        switch (p_station->aprs_symbol.area_object.color)
        {
          case (1):   // Blue Bright
            XmToggleButtonGadgetSetState(coption2, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (2):   // Green Bright
            XmToggleButtonGadgetSetState(coption3, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (3):   // Cyan Bright
            XmToggleButtonGadgetSetState(coption4, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (4):   // Red Bright
            XmToggleButtonGadgetSetState(coption5, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (5):   // Violet Bright
            XmToggleButtonGadgetSetState(coption6, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (6):   // Yellow Bright
            XmToggleButtonGadgetSetState(coption7, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (7):   // Gray Bright
            XmToggleButtonGadgetSetState(coption8, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
          case (8):   // Black Dim
            XmToggleButtonGadgetSetState(coption1, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (9):   // Blue Dim
            XmToggleButtonGadgetSetState(coption2, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (10):  // Green Dim
            XmToggleButtonGadgetSetState(coption3, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (11):  // Cyan Dim
            XmToggleButtonGadgetSetState(coption4, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (12):  // Red Dim
            XmToggleButtonGadgetSetState(coption5, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (13):  // Violet Dim
            XmToggleButtonGadgetSetState(coption6, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (14):  // Yellow Dim
            XmToggleButtonGadgetSetState(coption7, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (15):  // Gray Dim
            XmToggleButtonGadgetSetState(coption8, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);
            break;
          case (0):   // Black Bright
          default:
            XmToggleButtonGadgetSetState(coption1, TRUE, TRUE);
            XmToggleButtonGadgetSetState(bright_dim_toggle, TRUE, TRUE);
            break;
        }

        xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                        p_station->aprs_symbol.area_object.sqrt_lat_off
                        * p_station->aprs_symbol.area_object.sqrt_lat_off );

        XmTextFieldSetString( ob_lat_offset_data, temp_data );

        xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                        p_station->aprs_symbol.area_object.sqrt_lon_off
                        * p_station->aprs_symbol.area_object.sqrt_lon_off );

        XmTextFieldSetString( ob_lon_offset_data, temp_data );

      }   // Done with filling in Area Objects

      else    // Signpost/Probability/Normal Object
      {

        // Handle Generic Options (common to Signpost/Normal Objects)
        if (strlen(p_station->speed) != 0)
        {
          xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                          (int)(atof(p_station->speed) + 0.5) );

          XmTextFieldSetString( ob_speed_data, temp_data );
        }
        else
        {
          XmTextFieldSetString( ob_speed_data, "" );
        }

        if (strlen(p_station->course) != 0)
        {
          XmTextFieldSetString( ob_course_data, p_station->course);
        }
        else
        {
          XmTextFieldSetString( ob_course_data, "" );
        }

//                if ( (p_station->aprs_symbol.aprs_symbol == 'm')   // Found a signpost object
//                        && (p_station->aprs_symbol.aprs_type == '\\')
//                        && Signpost_object_enabled) {
        if (Signpost_object_enabled)
        {
          XtSetSensitive(ob_frame,FALSE);
          XtSetSensitive(signpost_frame,TRUE);
          XmTextFieldSetString( signpost_data, p_station->signpost);
        }   // Done with filling in Signpost Objects


        if (Probability_circles_enabled)
        {
          // Fetch the min/max fields from the object data and
          // write that data into the input fields.
          XmTextFieldSetString( probability_data_min, p_station->probability_min );
          XmTextFieldSetString( probability_data_max, p_station->probability_max );
        }


//                else if ( (p_station->aprs_symbol.aprs_type == '/') // Found a DF object
//                        && (p_station->aprs_symbol.aprs_symbol == '\\' )) {
        if (DF_object_enabled)
        {
          XtSetSensitive(ob_frame,FALSE);
          //fprintf(stderr,"Found a DF object\n");

          // Decide if it was an omni-DF object or a beam heading object
          if (p_station->NRQ[0] == '\0')      // Must be an omni-DF object
          {
            //fprintf(stderr,"omni-DF\n");
            //fprintf(stderr,"Signal_gain: %s\n", p_station->signal_gain);

            XmToggleButtonSetState(omni_antenna_toggle, TRUE, TRUE);

            // Set the received signal quality toggle
            switch (p_station->signal_gain[3])
            {
              case ('1'):   // 1
                XmToggleButtonGadgetSetState(soption1, TRUE, TRUE);
                break;
              case ('2'):   // 2
                XmToggleButtonGadgetSetState(soption2, TRUE, TRUE);
                break;
              case ('3'):   // 3
                XmToggleButtonGadgetSetState(soption3, TRUE, TRUE);
                break;
              case ('4'):   // 4
                XmToggleButtonGadgetSetState(soption4, TRUE, TRUE);
                break;
              case ('5'):   // 5
                XmToggleButtonGadgetSetState(soption5, TRUE, TRUE);
                break;
              case ('6'):   // 6
                XmToggleButtonGadgetSetState(soption6, TRUE, TRUE);
                break;
              case ('7'):   // 7
                XmToggleButtonGadgetSetState(soption7, TRUE, TRUE);
                break;
              case ('8'):   // 8
                XmToggleButtonGadgetSetState(soption8, TRUE, TRUE);
                break;
              case ('9'):   // 9
                XmToggleButtonGadgetSetState(soption9, TRUE, TRUE);
                break;
              case ('0'):   // 0
              default:
                XmToggleButtonGadgetSetState(soption0, TRUE, TRUE);
                break;
            }

            // Set the HAAT toggle
            switch (p_station->signal_gain[4])
            {
              case ('1'):   // 20ft
                XmToggleButtonGadgetSetState(hoption1, TRUE, TRUE);
                break;
              case ('2'):   // 40ft
                XmToggleButtonGadgetSetState(hoption2, TRUE, TRUE);
                break;
              case ('3'):   // 80ft
                XmToggleButtonGadgetSetState(hoption3, TRUE, TRUE);
                break;
              case ('4'):   // 160ft
                XmToggleButtonGadgetSetState(hoption4, TRUE, TRUE);
                break;
              case ('5'):   // 320ft
                XmToggleButtonGadgetSetState(hoption5, TRUE, TRUE);
                break;
              case ('6'):   // 640ft
                XmToggleButtonGadgetSetState(hoption6, TRUE, TRUE);
                break;
              case ('7'):   // 1280ft
                XmToggleButtonGadgetSetState(hoption7, TRUE, TRUE);
                break;
              case ('8'):   // 2560ft
                XmToggleButtonGadgetSetState(hoption8, TRUE, TRUE);
                break;
              case ('9'):   // 5120ft
                XmToggleButtonGadgetSetState(hoption9, TRUE, TRUE);
                break;
              case ('0'):   // 10ft
              default:
                XmToggleButtonGadgetSetState(hoption0, TRUE, TRUE);
                break;
            }

            // Set the antenna gain toggle
            switch (p_station->signal_gain[5])
            {
              case ('1'):   // 1dB
                XmToggleButtonGadgetSetState(goption1, TRUE, TRUE);
                break;
              case ('2'):   // 2dB
                XmToggleButtonGadgetSetState(goption2, TRUE, TRUE);
                break;
              case ('3'):   // 3dB
                XmToggleButtonGadgetSetState(goption3, TRUE, TRUE);
                break;
              case ('4'):   // 4dB
                XmToggleButtonGadgetSetState(goption4, TRUE, TRUE);
                break;
              case ('5'):   // 5dB
                XmToggleButtonGadgetSetState(goption5, TRUE, TRUE);
                break;
              case ('6'):   // 6dB
                XmToggleButtonGadgetSetState(goption6, TRUE, TRUE);
                break;
              case ('7'):   // 7dB
                XmToggleButtonGadgetSetState(goption7, TRUE, TRUE);
                break;
              case ('8'):   // 8dB
                XmToggleButtonGadgetSetState(goption8, TRUE, TRUE);
                break;
              case ('9'):   // 9dB
                XmToggleButtonGadgetSetState(goption9, TRUE, TRUE);
                break;
              case ('0'):   // 0dB
              default:
                XmToggleButtonGadgetSetState(goption0, TRUE, TRUE);
                break;
            }

            // Set the antenna directivity toggle
            switch (p_station->signal_gain[6])
            {
              case ('1'):   // 45
                XmToggleButtonGadgetSetState(doption1, TRUE, TRUE);
                break;
              case ('2'):   // 90
                XmToggleButtonGadgetSetState(doption2, TRUE, TRUE);
                break;
              case ('3'):   // 135
                XmToggleButtonGadgetSetState(doption3, TRUE, TRUE);
                break;
              case ('4'):   // 180
                XmToggleButtonGadgetSetState(doption4, TRUE, TRUE);
                break;
              case ('5'):   // 225
                XmToggleButtonGadgetSetState(doption5, TRUE, TRUE);
                break;
              case ('6'):   // 270
                XmToggleButtonGadgetSetState(doption6, TRUE, TRUE);
                break;
              case ('7'):   // 315
                XmToggleButtonGadgetSetState(doption7, TRUE, TRUE);
                break;
              case ('8'):   // 360
                XmToggleButtonGadgetSetState(doption8, TRUE, TRUE);
                break;
              case ('0'):   // Omni
              default:
                XmToggleButtonGadgetSetState(doption0, TRUE, TRUE);
                break;
            }
          }
          else    // Must be a beam-heading object
          {
            //fprintf(stderr,"beam-heading\n");

            XmToggleButtonSetState(beam_antenna_toggle, TRUE, TRUE);

            XmTextFieldSetString(ob_bearing_data, p_station->bearing);

            switch (p_station->NRQ[2])
            {
              case ('1'):   // 240°
                XmToggleButtonGadgetSetState(woption1, TRUE, TRUE);
                break;
              case ('2'):   // 120°
                XmToggleButtonGadgetSetState(woption2, TRUE, TRUE);
                break;
              case ('3'):   // 64°
                XmToggleButtonGadgetSetState(woption3, TRUE, TRUE);
                break;
              case ('4'):   // 32°
                XmToggleButtonGadgetSetState(woption4, TRUE, TRUE);
                break;
              case ('5'):   // 16°
                XmToggleButtonGadgetSetState(woption5, TRUE, TRUE);
                break;
              case ('6'):   // 8°
                XmToggleButtonGadgetSetState(woption6, TRUE, TRUE);
                break;
              case ('7'):   // 4°
                XmToggleButtonGadgetSetState(woption7, TRUE, TRUE);
                break;
              case ('8'):   // 2°
                XmToggleButtonGadgetSetState(woption8, TRUE, TRUE);
                break;
              case ('9'):   // 1°
                XmToggleButtonGadgetSetState(woption9, TRUE, TRUE);
                break;
              case ('0'):   // Useless
              default:
                XmToggleButtonGadgetSetState(woption0, TRUE, TRUE);
                break;
            }
          }
        }
        else      // Found a normal object
        {
          // Nothing needed in this block currently
        } // Done with filling in Normal objects

      } // Done with filling in Signpost, DF, or Normal Objects

      // Handle Generic Options (common to all)
      // Convert altitude from meters to feet
      if (strlen(p_station->altitude) != 0)
      {
        xastir_snprintf(temp_data, sizeof(temp_data), "%d",
                        (int)((atof(p_station->altitude) / 0.3048) + 0.5) );

        XmTextFieldSetString( ob_altitude_data, temp_data );
      }
      else
      {
        XmTextFieldSetString( ob_altitude_data, "" );
      }
    }

// Else we're creating a new object from scratch:  p_station == NULL
    else
    {
      if (Area_object_enabled)
      {
        XmToggleButtonGadgetSetState(coption1, TRUE, TRUE);             // Black
        XmToggleButtonGadgetSetState(bright_dim_toggle, FALSE, TRUE);   // Dim
        XmToggleButtonGadgetSetState(toption1, TRUE, TRUE);             // Circle
      }

      XmTextFieldSetString(object_name_data,"");


      // Set the symbol type based on the global variables
      if (Area_object_enabled)
      {
        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = 'l';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,FALSE);


      }
      else if (Signpost_object_enabled)
      {
        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = 'm';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XmTextFieldSetString( signpost_data, "" );

        XtSetSensitive(ob_frame,FALSE);


      }
      else if (Probability_circles_enabled)
      {
        temp_data[0] = '/';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '[';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XmTextFieldSetString( probability_data_min, "" );
        XmTextFieldSetString( probability_data_max, "" );


      }
      else if (DF_object_enabled)
      {
        temp_data[0] = '/';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '\\';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        // Defaults Omni-DF
        XmToggleButtonGadgetSetState(soption0, TRUE, TRUE); // Nothing heard
        XmToggleButtonGadgetSetState(hoption1, TRUE, TRUE); // 20 feet HAAT
        XmToggleButtonGadgetSetState(goption3, TRUE, TRUE); // 3dB gain antenna
        XmToggleButtonGadgetSetState(doption0, TRUE, TRUE); // No directivity

        // Defaults Beam-DF
        XmToggleButtonGadgetSetState(woption5, TRUE, TRUE); // 16 degree beamwidth

        XtSetSensitive(ob_frame,FALSE);


      }
      else if (Map_View_object_enabled)
      {
        temp_data[0] = '/';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = 'E';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);

        XtSetSensitive(ob_frame,FALSE);
        XtSetSensitive(ob_option_frame,FALSE);


      }
      else      // Normal object/item
      {
        temp_data[0] = '/';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_group_data,temp_data);

        temp_data[0] = '/';
        temp_data[1] = '\0';
        XmTextFieldSetString(object_symbol_data,temp_data);
      }

      // Compute the range for the Map View object and store
      // it in the comment field.
      if (Map_View_object_enabled)
      {
        double top_range, left_range, max_range;
        char range[10];
        Dimension width, height;


        // Get the display parameters
        XtVaGetValues(da,XmNwidth, &width,XmNheight, &height, NULL);

        // Find distance from center to top of screen.
        // top_range = distance from center_longitude,
        // center_latitude to center_longitude,
        // center_latitude-((height*scale_y)/2).
        top_range = calc_distance(center_latitude,
                                  center_longitude,
                                  NW_corner_latitude,
                                  center_longitude);
//fprintf(stderr," top_range:%1.0f meters\n", top_range);
        // Find distance from center to left of screen.
        // left_range = distance from center_longitude,
        // center_latitude to
        // center_longitude-((width*scale_x)/2),
        // center_latitude.
        left_range = calc_distance(center_latitude,
                                   center_longitude,
                                   center_latitude,
                                   NW_corner_longitude);
//fprintf(stderr,"left_range:%1.0f meters\n", left_range);

        // Compute greater of the two.  This is our range in
        // meters.
        if (top_range > left_range)
        {
          max_range = top_range;
        }
        else
        {
          max_range = left_range;
        }

        // Convert from meters to miles
        max_range = max_range / 1000.0;  // kilometers
        max_range = max_range * 0.62137; // miles

        // Restrict it to four digits.
        if (max_range > 9999.0)
        {
          max_range = 9999.0;
        }

//fprintf(stderr,"Range:%04d miles\n", (int)(max_range + 0.5));

        xastir_snprintf(range,
                        sizeof(range),
                        "RNG%04d",
                        (int)(max_range + 0.5)); // Poor man's rounding

        XmTextFieldSetString(object_comment_data, range);
      }
      else
      {
        XmTextFieldSetString(object_comment_data,"");
      }
    }



    if (strcmp(calldata,"2") != 0)    // Normal Modify->Object or Create->Object behavior
    {
      // Fill in original lat/lon values
      convert_lat_l2s(lat, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
      substr(temp_data,lat_str,2);
      XmTextFieldSetString(object_lat_data_deg,temp_data);
      substr(temp_data,lat_str+2,6);
      XmTextFieldSetString(object_lat_data_min,temp_data);
      substr(temp_data,lat_str+8,1);
      XmTextFieldSetString(object_lat_data_ns,temp_data);

      convert_lon_l2s(lon, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);
      substr(temp_data,lon_str,3);
      XmTextFieldSetString(object_lon_data_deg,temp_data);
      substr(temp_data,lon_str+3,6);
      XmTextFieldSetString(object_lon_data_min,temp_data);
      substr(temp_data,lon_str+9,1);
      XmTextFieldSetString(object_lon_data_ew,temp_data);
    }

    else    // We're in the middle of moving an object, calldata was "2"
    {
      // Fill in new lat/long values
      //fprintf(stderr,"Here we will fill in the new lat/long values\n");
      x = (center_longitude - ((screen_width  * scale_x)/2) + (input_x * scale_x));
      y = (center_latitude  - ((screen_height * scale_y)/2) + (input_y * scale_y));
      if (x < 0)
      {
        x = 0l;  // 180°W
      }

      if (x > 129600000l)
      {
        x = 129600000l;  // 180°E
      }

      if (y < 0)
      {
        y = 0l;  //  90°N
      }

      if (y > 64800000l)
      {
        y = 64800000l;  //  90°S
      }

      convert_lat_l2s(y, lat_str, sizeof(lat_str), CONVERT_HP_NOSP);
      substr(temp_data,lat_str,2);
      XmTextFieldSetString(object_lat_data_deg,temp_data);
      substr(temp_data,lat_str+2,6);
      XmTextFieldSetString(object_lat_data_min,temp_data);
      substr(temp_data,lat_str+8,1);
      XmTextFieldSetString(object_lat_data_ns,temp_data);

      convert_lon_l2s(x, lon_str, sizeof(lon_str), CONVERT_HP_NOSP);
      substr(temp_data,lon_str,3);
      XmTextFieldSetString(object_lon_data_deg,temp_data);
      substr(temp_data,lon_str+3,6);
      XmTextFieldSetString(object_lon_data_min,temp_data);
      substr(temp_data,lon_str+9,1);
      XmTextFieldSetString(object_lon_data_ew,temp_data);
    }

    XtAddCallback(object_group_data, XmNvalueChangedCallback, updateObjectPictureCallback, object_dialog);
    XtAddCallback(object_symbol_data, XmNvalueChangedCallback, updateObjectPictureCallback, object_dialog);

    // update symbol picture
    (void)updateObjectPictureCallback((Widget)NULL,(XtPointer)NULL,(XtPointer)NULL);

    pos_dialog(object_dialog);

    delw = XmInternAtom(XtDisplay(object_dialog),"WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(object_dialog, delw, Object_destroy_shell, (XtPointer)object_dialog);

    if (Signpost_object_enabled)
    {
      XtManageChild(signpost_form);
    }
    else if (Probability_circles_enabled)
    {
      XtManageChild(probability_form);
    }
    else if (Area_object_enabled)
    {
      XtManageChild(shape_box);
      XtManageChild(color_box);
      XtManageChild(area_form);
    }
    else if (DF_object_enabled)
    {
      XtManageChild(signal_box);
      XtManageChild(height_box);
      XtManageChild(gain_box);
      XtManageChild(directivity_box);
      XtManageChild(width_box);
      XtManageChild(formomni);
      XtManageChild(formbeam);
    }

    XtManageChild(ob_latlon_form);
    XtManageChild(ob_option_form);
    XtManageChild(ob_form1);
    XtManageChild(ob_form);
    XtManageChild(ob_pane);

    XtPopup(object_dialog,XtGrabNone);
    fix_dialog_size(object_dialog);         // don't allow a resize

    // Move focus to the Cancel button.  This appears to highlight t
    // button fine, but we're not able to hit the <Enter> key to
    // have that default function happen.  Note:  We _can_ hit the
    // <SPACE> key, and that activates the option.
//        XmUpdateDisplay(object_dialog);
    XmProcessTraversal(ob_button_cancel, XmTRAVERSE_CURRENT);
  }

  // This will cause the move to happen quickly without any button presses.
  if (calldata != NULL)   // If we're doing a "move" operation
  {
    if (strcmp(calldata,"2") == 0)
    {
      if ((p_station->flag & ST_ITEM) != 0)       // Moving an Item
      {
//fprintf(stderr,"Calling Item_change_data_set()\n");
        Item_change_data_set(w,object_dialog,object_dialog);    // Move it now!
//fprintf(stderr,"Done with call\n");
      }
      else                                        // Moving an Object
      {
//fprintf(stderr,"Calling Object_change_data_set()\n");
        Object_change_data_set(w,object_dialog,object_dialog);  // Move it now!
//fprintf(stderr,"Done with call\n");
      }
    }
  }

}   // End of Set_Del_Object





/*
 *  Change data for current object
 *  If calldata = 2, we're doing a move object operation.  Pass the
 *  value on through to Set_Del_Object.
 */
void Modify_object( Widget w, XtPointer clientData, XtPointer calldata)
{
  DataRow *p_station = clientData;

  //if (calldata != NULL)
  //    fprintf(stderr,"Modify_object:  calldata:  %s\n", (char *)calldata);

  //fprintf(stderr,"Object Name: %s\n", p_station->call_sign);

  // Only move the object if it is our callsign, else force the
  // user to adopt the object first, then move it.
  //
  // Not exact match (this one is useful for testing)
////    if (!is_my_call(p_station->origin,0)) {
//    if (!is_my_object_item(p_station)) {
  //
  // Exact match includes SSID (this one is for production code)
//    if (!is_my_call(p_station->origin,1)) {
  if (!is_my_object_item(p_station))
  {

    // It's not from my callsign
    if (strncmp(calldata,"2",1) == 0)
    {

      // We're doing a Move Object operation
      //fprintf(stderr,"Modify_object:  Object not owned by
      //me!\n");
      popup_message_always(langcode("POPEM00035"),
                           langcode("POPEM00042"));
      return;
    }
  }

  Set_Del_Object( w, p_station, calldata );
}





//
// Disown function called by object/item decode routines.
// If an object/item is received that matches something in our
// object.log file, we immediately cease to transmit that object and
// we mark each line containing that object in our log file with a
// hash mark ('#').  This comments out that object so that the next
// time we reboot, we won't start transmitting it again.

// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void disown_object_item(char *call_sign, char *new_owner)
{
  char file[200];
  char file_temp[200];
  FILE *f;
  FILE *f_temp;
  char line[300];
  char name[15];
  int ret;


//fprintf(stderr,"disown_object_item, object: %s, new_owner: %s\n",
//    call_sign,
//    new_owner);


  // If it's my call in the new_owner field, then I must have just
  // deleted the object and am transmitting a killed object for
  // it.  If it's not my call, someone else has assumed control of
  // the object.
  //
  // Comment out any references to the object in the log file so
  // that we don't start retransmitting it on a restart.

  if (is_my_call(new_owner,1))   // Exact match includes SSID
  {
    //fprintf(stderr,"Commenting out %s in object.log\n",
    //call_sign);
  }
  else
  {
    fprintf(stderr,"Disowning '%s': '%s' is taking over control of it.\n",
            call_sign, new_owner);
  }

  get_user_base_dir("config/object.log", file, sizeof(file));

  get_user_base_dir("config/object-temp.log", file_temp, sizeof(file_temp));

  //fprintf(stderr,"%s\t%s\n",file,file_temp);

  // Our own internal function from util.c
  ret = copy_file(file, file_temp);
  if (ret)
  {
    fprintf(stderr,"\n\nCouldn't create temp file %s!\n\n\n",
            file_temp);
    return;
  }

  // Open the temp file and write to the original file, with hash
  // marks in front of the appropriate lines.
  f_temp=fopen(file_temp,"r");
  f=fopen(file,"w");

  if (f == NULL)
  {
    fprintf(stderr,"Couldn't open %s\n",file);
    return;
  }
  if (f_temp == NULL)
  {
    fprintf(stderr,"Couldn't open %s\n",file_temp);
    return;
  }

  // Read lines from the temp file and write them to the standard
  // file, modifying them as necessary.
  while (fgets(line, 300, f_temp) != NULL)
  {

    // Need to check that the length matches for both!  Best way
    // is to parse the object/item name out of the string and
    // then do a normal string compare between the two.

    if (line[0] == ';')         // Object
    {
      substr(name,&line[1],9);
      name[9] = '\0';
      remove_trailing_spaces(name);
    }

    else if (line[0] == ')')    // Item
    {
      int i;

      // 3-9 char name
      for (i = 1; i <= 9; i++)
      {
        if (line[i] == '!' || line[i] == '_')
        {
          name[i-1] = '\0';
          break;
        }
        name[i-1] = line[i];
      }
      name[9] = '\0';  // In case we never saw '!' || '_'

      // Don't remove trailing spaces for Items, else we won't
      // get a match.
    }

    else if (line[1] == ';')    // Commented out Object
    {
      substr(name,&line[2],10);
      name[9] = '\0';
      remove_trailing_spaces(name);

    }

    else if (line[1] == ')')    // Commented out Item
    {
      int i;

      // 3-9 char name
      for (i = 2; i <= 10; i++)
      {
        if (line[i] == '!' || line[i] == '_')
        {
          name[i-1] = '\0';
          break;
        }
        name[i-1] = line[i];
      }
      name[9] = '\0';  // In case we never saw '!' || '_'

      // Don't remove trailing spaces for Items, else we won't
      // get a match.
    }


    //fprintf(stderr,"'%s'\t'%s'\n", name, call_sign);

    if (valid_object(name))
    {

      if ( strcmp(name,call_sign) == 0 )
      {
        // Match.  Comment it out in the file unless it's
        // already commented out.
        if (line[0] != '#')
        {
          fprintf(f,"#%s",line);
          //fprintf(stderr,"#%s",line);
        }
        else
        {
          fprintf(f,"%s",line);
          //fprintf(stderr,"%s",line);
        }
      }
      else
      {
        // No match.  Copy the line verbatim unless it's
        // just a
        // blank line.
        if (line[0] != '\n')
        {
          fprintf(f,"%s",line);
          //fprintf(stderr,"%s",line);
        }
      }
    }
  }
  fclose(f);
  fclose(f_temp);
}





//
// Logging function called by object/item create/modify routines.
// We log each object/item as one line in a file.
//
// We need to check for objects of the same name in the file,
// deleting lines that have the same name, and adding new records to
// the end.  Actually  BAD IDEA!  We want to keep the history of the
// object so that we can trace its movements later.
//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
// Change this function so that deleted objects/items get disowned
// instead (commented out in the file so that they're not
// transmitted again after a restart).  See disown_object_item().
//
void log_object_item(char *line, int disable_object, char *object_name)
{
  char file[MAX_VALUE];
  FILE *f;

  get_user_base_dir("config/object.log", file, sizeof(file));

  f=fopen(file,"a");
  if (f!=NULL)
  {
    fprintf(f,"%s\n",line);
    (void)fclose(f);

    if (debug_level & 1)
    {
      fprintf(stderr,"Saving object/item to file: %s",line);
    }

    // Comment out all instances of the object/item in the log
    // file.  This will make sure that the object is not
    // retransmitted again when Xastir is restarted.
    if (disable_object)
    {
      disown_object_item(object_name, my_callsign);
    }

  }
  else
  {
    fprintf(stderr,"Couldn't open file for appending: %s\n", file);
  }
}





//
// Function to load saved objects and items back into Xastir.  This
// is called on startup.  This implements persistent objects/items
// across Xastir restarts.
//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
// This appears to skip the loading of killed objects/items.  We may
// instead want to begin transmitting them again until they time
// out, then mark them with a '#' in the log file at that point.
//
void reload_object_item(void)
{
  char file[MAX_VALUE];
  FILE *f;
  char line[300+1];
  char line2[350];
  int save_state;


  get_user_base_dir("config/object.log", file, sizeof(file));

  f=fopen(file,"r");
  if (f!=NULL)
  {

    // Turn off duplicate point checking (need this in order to
    // work with SAR objects).  Save state so that we don't mess
    // it up.
    save_state = skip_dupe_checking;
    skip_dupe_checking++;

    while (fgets(line, 300, f) != NULL)
    {

      if (debug_level & 1)
      {
        fprintf(stderr,"Loading object/item from file: %s",line);
      }

      if (line[0] != '#')     // Skip comment lines
      {
        xastir_snprintf(line2,
                        sizeof(line2),
                        "%s>%s:%s",
                        my_callsign,
                        VERSIONFRM,
                        line);

        // Decode this packet.  This will put it into our
        // station database and cause it to be transmitted
        // at
        // regular intervals.  Port is set to -1 here.
        decode_ax25_line( line2, DATA_VIA_LOCAL, -1, 1);

// Right about here we could do a lookup for the object/item
// matching the name and change the timing on it.  This could serve
// to spread the transmit timing out a bit so that all objects/items
// are not transmitted together.  Another easier option would be to
// change the routine which chooses when to transmit, having it
// randomize the numbers a bit each time.  I chose the second
// option.

      }
    }
    (void)fclose(f);

    // Restore the skip_dupe_checking state
    skip_dupe_checking = save_state;

    // Update the screen
    redraw_symbols(da);
    (void)XCopyArea(XtDisplay(da),
                    pixmap_final,
                    XtWindow(da),
                    gc,
                    0,
                    0,
                    screen_width,
                    screen_height,
                    0,
                    0);
  }
  else
  {
    if (debug_level & 1)
    {
      fprintf(stderr,"Couldn't open file for reading: %s\n", file);
    }
  }

  // Start transmitting these objects in about 30 seconds.
  // Prevent transmission of objects until sometime after we're
  // done with our initial load.
  last_object_check = sec_now() + 30;
}


