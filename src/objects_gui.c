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



#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stdint.h>
#include <ctype.h>
#include <math.h>

#if HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif // HAVE_SYS_TIME_H
#include <time.h>

#include <Xm/XmAll.h>
#include <X11/cursorfont.h>

#include "xastir.h"
#include "draw_symbols.h"
#include "main.h"
#include "db_funcs.h"
#include "xa_config.h"
#include "interface.h"
#include "objects.h"
#include "objects_gui.h"
#include "object_utils.h"
#include "dr_utils.h"

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist

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

int doing_move_operation = 0;

// forward declare function to read object/item dialog box values
int Read_object_item_dialog_values(char *name, size_t name_size,
                                   char *lat_str, size_t lat_str_size,
                                   char *ext_lat_str, size_t ext_lat_str_size,
                                   char *lon_str, size_t lon_str_size,
                                   char *ext_lon_str, size_t ext_lon_str_size,
                                   char *obj_group, char *obj_symbol,
                                   char *comment, size_t comment_size,
                                   char *course, size_t course_size,
                                   char *speed, size_t speed_size,
                                   char *altitude, size_t altitude_size,
                                   int *area_object, int *area_type,
                                   int *area_filled,
                                   char *area_color, size_t area_color_size,
                                   char *lat_offset_str,
                                   size_t lat_offset_str_size,
                                   char *lon_offset_str,
                                   size_t lon_offset_str_size,
                                   char *corridor, size_t corridor_size,
                                   int *signpost_object,
                                   char *signpost_str, size_t signpost_str_size,
                                   int *df_object,
                                   int *omni_df,
                                   int *beam_df,
                                   char *df_shgd, size_t df_shgd_size,
                                   char *bearing, size_t bearing_size,
                                   char *NRQ, size_t NRQ_size,
                                   int *prob_circles,
                                   char *prob_min, size_t prob_min_size,
                                   char *prob_max, size_t prob_max_size
                                   );

////////////////////////////////////////////////////////////////////////////////////////////////////

// Init values for Objects dialog
char last_object[9+1];
char last_obj_grp;
char last_obj_sym;
char last_obj_overlay;

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





/*
 *  Setup APRS Information Field for Objects
 *
 * We do this by reading the dialog box, creating a DataRow from that
 * information, then formatting the packet exactly as we would do for
 * an existing object packet by calling Create_object_item_tx_string.
 *
 * If we are passed a p_station, that means the modify dialog was called
 * and we're creating a new packet for an existing object.  But we still
 * create a new DataRow for our new packet.  In this case, we discard
 * any position entered in the modify dialog box and replace it with
 * dead-reckoned position.  I'm not sure this is a desirable feature, but
 * it was exactly what Xastir did before my refactor.
 *
 */
int Setup_object_data(char *line, int line_length, DataRow *p_station)
{

  char name[MAX_CALLSIGN+1];
  char obj_group, obj_symbol;
  char lat_str[MAX_LAT];
  char lon_str[MAX_LONG];
  char ext_lat_str[20];
  char ext_lon_str[20];
  char comment[43+1];
  char course[MAX_COURSE+1];
  char speed[MAX_SPEED+1];
  char altitude[MAX_ALTITUDE];
  int area_object, area_type,area_filled;
  char area_color[3];
  char lat_offset_str[5];
  char lon_offset_str[5];
  char corridor[4];
  int signpost_object;
  char signpost_str[4];
  int df_object, omni_df, beam_df;
  char df_shgd[MAX_POWERGAIN+1];
  char bearing[MAX_COURSE+1];
  char NRQ[MAX_COURSE+1];
  int prob_circles;
  char prob_min[10+1];
  char prob_max[10+1];
  DataRow *theDataRow;

  int object_speed;

  if (Read_object_item_dialog_values(name, sizeof(name),
                                     lat_str, sizeof(lat_str),
                                     ext_lat_str, sizeof(ext_lat_str),
                                     lon_str, sizeof(lon_str),
                                     ext_lon_str, sizeof(ext_lon_str),
                                     &obj_group, &obj_symbol,
                                     comment, sizeof(comment),
                                     course, sizeof(course),
                                     speed, sizeof(speed),
                                     altitude, sizeof(altitude),
                                     &area_object, &area_type,
                                     &area_filled,
                                     area_color, sizeof(area_color),
                                     lat_offset_str, sizeof(lat_offset_str),
                                     lon_offset_str, sizeof(lon_offset_str),
                                     corridor, sizeof(corridor),
                                     &signpost_object,
                                     signpost_str, sizeof(signpost_str),
                                     &df_object, &omni_df, &beam_df,
                                     df_shgd, sizeof(df_shgd),
                                     bearing, sizeof(bearing),
                                     NRQ, sizeof(NRQ),
                                     &prob_circles,
                                     prob_min, sizeof(prob_min),
                                     prob_max, sizeof(prob_max)))
  {
    if (!valid_object(name))
    {
      return(0);
    }

    xastir_snprintf(last_object,sizeof(last_object),"%s",name);

    if (p_station != NULL)
    {
      object_speed = atoi(p_station->speed);
      if (object_speed > 0 && !doing_move_operation)
      {
        fetch_current_DR_strings(p_station,
                                 lat_str,
                                 lon_str,
                                 ext_lat_str,
                                 ext_lon_str);
      }
      // Keep the time current for our own objects.
      p_station->sec_heard = sec_now();
      move_station_time(p_station,NULL);
    }

    theDataRow=construct_object_item_data_row(name,
                                              ext_lat_str,
                                              ext_lon_str,
                                              obj_group, obj_symbol,
                                              comment,
                                              course,
                                              speed,
                                              altitude,
                                              area_object, area_type,
                                              area_filled,
                                              area_color,
                                              lat_offset_str,
                                              lon_offset_str,
                                              corridor,
                                              signpost_object,
                                              signpost_str,
                                              df_object, omni_df, beam_df,
                                              df_shgd,
                                              bearing, NRQ,
                                              prob_circles,
                                              prob_min, prob_max,
                                              1, 0);
    if (theDataRow)
    {
      (void)Create_object_item_tx_string(theDataRow, line,
                                         line_length);
      destroy_object_item_data_row(theDataRow);
    }
    else
    {
      // This can never happen, as the constructor will abort with a fatal
      // error if it can't allocate data.
      fprintf(stderr,"BOO!\n");
    }
    return(1);
  }
  else
  {
    return(0);
  }
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
  static Widget ob_pane, ob_scrollwindow, ob_form,
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

    ob_scrollwindow = XtVaCreateManagedWidget("scrollwindow",
                                              xmScrolledWindowWidgetClass,
                                              ob_pane,
                                              XmNscrollingPolicy, XmAUTOMATIC,
                                              NULL);

    ob_form =  XtVaCreateWidget("Set_Del_Object ob_form",
                                xmFormWidgetClass,
                                ob_scrollwindow,
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
                          XmNcolumns,                 37,     // max 43 without Data Extension
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

    resize_dialog(ob_form, object_dialog);

    XtPopup(object_dialog,XtGrabNone);

    // Move focus to the Cancel button.  This appears to highlight the
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


//
// This function was extracted from the original implementation of
// Setup_object_data.  It reads all the values out of the Object/Item
// create/modify dialog box and populates the buffers provided.
//
// Notes:
//    while most of these 'char *' arguments are intended to be char arrays
//    of appropriate size, obj_group and obj_symbols are pointers to a
//    'char' variable, not an array.  This is so they'll be passed by reference
//    and we can return values in them.
//
// Per the original implementation, we return both the low-precision and
// high-precision lat/lon strings.  The caller will decide which to use.
//
// Note that ALL char arrays passed in have their associated buffer size
// passed in with them, so we can always make sure we don't overrun them.
//
// Function returns 1 if all values read
// Function returns 0 if any value determined to be invalid.
//

int Read_object_item_dialog_values(char *name, size_t name_size,
                                   char *lat_str, size_t lat_str_size,
                                   char *ext_lat_str, size_t ext_lat_str_size,
                                   char *lon_str, size_t lon_str_size,
                                   char *ext_lon_str, size_t ext_lon_str_size,
                                   char *obj_group, char *obj_symbol,
                                   char *comment, size_t comment_size,
                                   char *course, size_t course_size,
                                   char *speed, size_t speed_size,
                                   char *altitude, size_t altitude_size,
                                   int *area_object, int *area_type,
                                   int *area_filled,
                                   char *area_color, size_t area_color_size,
                                   char *lat_offset_str,
                                   size_t lat_offset_str_size,
                                   char *lon_offset_str,
                                   size_t lon_offset_str_size,
                                   char *corridor, size_t corridor_size,
                                   int *signpost_object,
                                   char *signpost_str, size_t signpost_str_size,
                                   int *df_object,
                                   int *omni_df,
                                   int *beam_df,
                                   char *df_shgd, size_t df_shgd_size,
                                   char *bearing, size_t bearing_size,
                                   char *NRQ, size_t NRQ_size,
                                   int *prob_circles,
                                   char *prob_min, size_t prob_min_size,
                                   char *prob_max, size_t prob_max_size
                                   )
{
  char *tmp_ptr;
  // components of lat/lon
  char hemisphere;
  int degrees;
  double minutes;

  // Initialize all the things
  name[0] = lat_str[0] = ext_lat_str[0] = lon_str[0] = ext_lon_str[0] = '\0';
  *obj_group = *obj_symbol = '\0';
  comment[0] = course[0] = speed[0] = altitude[0] = '\0';
  *area_object = *area_type = *area_filled = '\0';
  area_color[0] = lat_offset_str[0] = lon_offset_str[0] = corridor[0] = '\0';
  *signpost_object = 0;
  signpost_str[0] = '\0';
  *df_object = *omni_df = *beam_df = 0;
  df_shgd[0] = bearing[0] = NRQ[0] = '\0';
  *prob_circles = 0;
  prob_min[0] = prob_max[0] = '\0';

  // ----------------------------------------------------------------
  // All the generic data fields
  // name
  tmp_ptr = XmTextFieldGetString(object_name_data);
  xastir_snprintf(name,name_size,"%s",tmp_ptr);
  XtFree(tmp_ptr);
  (void) remove_trailing_spaces(name);

  // assemble latitude
  tmp_ptr = XmTextFieldGetString(object_lat_data_ns);
  if ((char)toupper((int)(tmp_ptr[0])) == 'S')
    hemisphere='S';
  else
    hemisphere='N';
  XtFree(tmp_ptr);

  tmp_ptr = XmTextFieldGetString(object_lat_data_deg);
  degrees=atoi(tmp_ptr);
  XtFree(tmp_ptr);

  tmp_ptr = XmTextFieldGetString(object_lat_data_min);
  minutes=atof(tmp_ptr);
  XtFree(tmp_ptr);

  // Check latitude validity
  if ((degrees > 90) || (degrees < 0))
    return (0);
  if ((minutes >= 60.0) || (minutes < 0.0))
    return (0);

  if ((degrees == 90) && (minutes != 0.0))
    return (0);

  // Format both low and high precision versions.
  xastir_snprintf(lat_str,lat_str_size, "%02d%05.2f%c",
                  degrees, minutes, hemisphere);
  xastir_snprintf(ext_lat_str,ext_lat_str_size, "%02d%06.3f%c",
                  degrees, minutes, hemisphere);

  // assemble longitude
  tmp_ptr = XmTextFieldGetString(object_lon_data_ew);
  if ((char)toupper((int)(tmp_ptr[0])) == 'E')
    hemisphere='E';
  else
    hemisphere='W';
  XtFree(tmp_ptr);

  tmp_ptr = XmTextFieldGetString(object_lon_data_deg);
  degrees=atoi(tmp_ptr);
  XtFree(tmp_ptr);

  tmp_ptr = XmTextFieldGetString(object_lon_data_min);
  minutes=atof(tmp_ptr);
  XtFree(tmp_ptr);

  // Check longitude validity
  if ((degrees > 180) || (degrees < 0))
    return (0);
  if ((minutes >= 60.0) || (minutes < 0.0))
    return (0);

  if ((degrees == 180) && (minutes != 0.0))
    return (0);

  // Format both low and high precision versions.
  xastir_snprintf(lon_str,lon_str_size, "%03d%05.2f%c",
                  degrees, minutes, hemisphere);
  xastir_snprintf(ext_lon_str,ext_lon_str_size, "%03d%06.3f%c",
                  degrees, minutes, hemisphere);

  // Get symbol
  // We don't bother processing the group other than to upcase it.
  // Handling of whether this is an overlay character or not is taken care
  // of by construct_object_item_data_row.
  tmp_ptr = XmTextFieldGetString(object_group_data);
  if (isalpha((int)tmp_ptr[0]))
    *obj_group = toupper((int)(tmp_ptr[0]));
  else
    *obj_group = tmp_ptr[0];
  XtFree(tmp_ptr);

  tmp_ptr = XmTextFieldGetString(object_symbol_data);
  *obj_symbol = tmp_ptr[0];
  XtFree(tmp_ptr);

  // Get the comment
  tmp_ptr = XmTextFieldGetString(object_comment_data);
  if (tmp_ptr[0] == '}')
  {
    // this is a multiline comment, but thoseshould start with a space,
    // so add one
    xastir_snprintf(comment,comment_size," %s",tmp_ptr);
  }
  else
  {
    xastir_snprintf(comment,comment_size,"%s",tmp_ptr);
  }
  XtFree(tmp_ptr);

  (void)remove_trailing_spaces(comment);

  // course/speed
  tmp_ptr = XmTextFieldGetString(ob_course_data);
  if (strlen(tmp_ptr) != 0)
  {
    int tmp_int = atoi(tmp_ptr);
    tmp_int = (tmp_int==0)?360:tmp_int;
    if (tmp_int >=1 && tmp_int <= 360)
    {
      xastir_snprintf(course, course_size,"%03d",tmp_int);
    }
  }
  else
  {
    course[0]='\0';
  }
  XtFree(tmp_ptr);

  tmp_ptr = XmTextFieldGetString(ob_speed_data);
  if (strlen(tmp_ptr) != 0)
  {
    int tmp_int = atoi(tmp_ptr);
    if (tmp_int >=0 && tmp_int <= 999)
    {
      xastir_snprintf(speed, speed_size,"%3d",tmp_int);
    }
  }
  else
  {
    speed[0]='\0';
  }
  XtFree(tmp_ptr);

  // Altitude
  tmp_ptr = XmTextFieldGetString(ob_altitude_data);
  if (strlen(tmp_ptr) != 0)
  {
    if (isdigit((int)tmp_ptr[0]))
    {
      long tmp_int=atoi(tmp_ptr);
      if (tmp_int >= 0 && tmp_int <= 999999)
        xastir_snprintf(altitude,altitude_size,"%06ld",tmp_int);
    }
  }
  XtFree(tmp_ptr);

  // ----------------------------------------------------------------
  // Now handle fields that are only to be read under certain conditions:
  // ----------------------------------------------------------------

  if (Area_object_enabled)
  {
    *area_object=1;
    // ----------------------------------------------------------------
    // Area objects
    // The "Area_" variables are globals that are set by other callback
    // functions and represent values from radio and check buttons.
    // ----------------------------------------------------------------
    format_area_color_from_dialog(area_color,area_color_size,
                                  Area_color, Area_bright);
    *area_type = Area_type;
    *area_filled = Area_filled;

    // don't take the square root here, the function that processes this input
    // will do that.
    tmp_ptr = XmTextFieldGetString(ob_lat_offset_data);
    if (strlen(tmp_ptr) != 0)
    {
      xastir_snprintf(lat_offset_str,lat_offset_str_size,"%s",tmp_ptr);
    }
    XtFree(tmp_ptr);
    tmp_ptr = XmTextFieldGetString(ob_lon_offset_data);
    if (strlen(tmp_ptr) != 0)
    {
      xastir_snprintf(lon_offset_str,lon_offset_str_size,"%s",tmp_ptr);
    }
    XtFree(tmp_ptr);

    if (Area_type == 1 || Area_type == 6)
    {
      tmp_ptr = XmTextFieldGetString(ob_corridor_data);
      if (strlen(tmp_ptr) != 0)
      {
        int tmp_int = atoi(tmp_ptr);
        if (tmp_int >0 && tmp_int <= 999)
        {
          xastir_snprintf(corridor,corridor_size,"%d",tmp_int);
        }
      }
      XtFree(tmp_ptr);
    }
  }
  else if (Signpost_object_enabled)
  {
    *signpost_object = 1;
    tmp_ptr = XmTextFieldGetString(signpost_data);
    if (strlen(tmp_ptr) >= 0 && strlen(tmp_ptr) <= 3)
    {
      xastir_snprintf(signpost_str,signpost_str_size,"%s",tmp_ptr);
    }
    XtFree(tmp_ptr);
  }
  else if (DF_object_enabled)
  {
    // ----------------------------------------------------------------
    // DF objects
    // ----------------------------------------------------------------
    *df_object=1;

    if (Omni_antenna_enabled)
    {
      // The Omni_antenna_enabled variable is a global set by other callback
      // functions, and "object_shgd" is also set from callbacks using
      // values from the dialog box check- and radio-buttons.
      *omni_df=1;
      xastir_snprintf(df_shgd,df_shgd_size,"%s",object_shgd);
    }
    else // it's a beam-df
    {
      int tmp_int;
      // In this case, "object_NRQ" is a global being set by callbacks.
      *beam_df = 1;
      tmp_ptr = XmTextFieldGetString(ob_bearing_data);
      tmp_int = atoi(tmp_ptr);
      XtFree(tmp_ptr);

      if (tmp_int < 1 || tmp_int > 360)
        tmp_int = 360;
      xastir_snprintf(bearing,bearing_size,"%03d",tmp_int);

      xastir_snprintf(NRQ,NRQ_size,"%s",object_NRQ);
    }
  }
  else      // just a normal object, but could have probability circles
  {
    if (Probability_circles_enabled)
    {
      *prob_circles = 1;
      tmp_ptr = XmTextFieldGetString(probability_data_min);
      if (strlen(tmp_ptr)!=0)
      {
        xastir_snprintf(prob_min,prob_min_size,"%s",tmp_ptr);
      }
      XtFree(tmp_ptr);
      tmp_ptr = XmTextFieldGetString(probability_data_max);
      if (strlen(tmp_ptr)!=0)
      {
        xastir_snprintf(prob_max,prob_max_size,"%s",tmp_ptr);
      }
      XtFree(tmp_ptr);
    }
  }

  return(1);

}
