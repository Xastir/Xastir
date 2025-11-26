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

#include "xastir.h"
#include "main.h"
#include "db_funcs.h"
#include "xa_config.h"
#include "interface.h"
#include "objects.h"
#include "object_utils.h"
#include "dr_utils.h"

void move_station_time(DataRow *p_curr, DataRow *p_time);

// forward declaration of a function present in db.c but not advertised by
// any of the headers we include.
void init_station(DataRow *p_station);
int delete_comments_and_status(DataRow *fill);
void add_comment(DataRow *p_station, char *comment_string);

// Must be last include file
#include "leak_detection.h"




// ---------------------------- object -------------------------------



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
  char lat_str[MAX_LAT];
  char lon_str[MAX_LONG];
  char comment[43+1];                 // max 43 characters of comment
  char time[7+1];
  char complete_area_color[3];
  int complete_area_type;
  int lat_offset, lon_offset;
  char complete_corridor[6];
  char altitude[10];
  char speed_course[8];
  int speed;
  int course;
  char signpost[6];
  char object_group;
  char object_symbol;
  int killed = 0;

  long x_long = p_station->coord_lon;
  long y_lat = p_station->coord_lat;;

  (void)remove_trailing_spaces(p_station->call_sign);

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
    pad_item_name(p_station->call_sign, sizeof(p_station->call_sign));

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

  // If the object or item has an associated speed, use the dead-reckoned
  // position instead of the one in p_station.
  if (strlen(p_station->speed) != 0)
  {
    int temp = atoi(p_station->speed);
    if ( (temp >=0) && (temp <= 999))
    {
      compute_current_DR_position(p_station,&x_long,&y_lat);
    }
  }

  // Lat/lon are in Xastir coordinates, so we need to convert
  // them to APRS string format here.
  // Need low-precision if uncompressed, high precision if compressed
  convert_lat_l2s(y_lat, lat_str, sizeof(lat_str),
          (transmit_compressed_objects_items)?CONVERT_HP_NOSP:CONVERT_LP_NOSP);
  convert_lon_l2s(x_long, lon_str, sizeof(lon_str),
          (transmit_compressed_objects_items)?CONVERT_HP_NOSP:CONVERT_LP_NOSP);

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

  format_probability_ring_data(comment,sizeof(comment), p_station->probability_min, p_station->probability_max);

  prepend_rng_phg(comment,sizeof(comment),p_station->power_gain);

  (void)remove_trailing_spaces(comment);


  // This is for objects only, not items.  Uses current time but
  // should use the transmitted time from the DataRow struct.
  // Which time field in the struct would that be?  Have to find
  // out
  // from the extract_?? code.
  if ((p_station->flag & ST_OBJECT) != 0)
  {
    format_zulu_time(time,sizeof(time));
  }


// Handle Generic Options

  // Speed/Course Fields

  if (p_station->aprs_symbol.area_object.type != AREA_NONE)    // It's an area object
  {
    speed_course[0] = '\0'; // Course/Speed not allowed if Area Object
    course=0;
    speed=0;
  }
  else
  {
    format_course_speed(speed_course,sizeof(speed_course),p_station->course,p_station->speed,&course,&speed);
  }

  // Altitude Field
  format_altitude(altitude, sizeof(altitude), p_station->altitude);

// Handle Specific Options
  // Area Objects
  if (p_station->aprs_symbol.area_object.type != AREA_NONE)   // It's an area object
  {

    format_area_color_from_numeric(complete_area_color,
                                   sizeof(complete_area_color),
                                   p_station->aprs_symbol.area_object.color);

    complete_area_type = p_station->aprs_symbol.area_object.type;

    lat_offset = p_station->aprs_symbol.area_object.sqrt_lat_off;
    lon_offset = p_station->aprs_symbol.area_object.sqrt_lon_off;

    // Corridor
    format_area_corridor(complete_corridor, sizeof(complete_corridor),
                         complete_area_type,
                         p_station->aprs_symbol.area_object.corridor_width);

    format_area_object_item_packet(line, line_length,
                                   p_station->call_sign,
                                   object_group, object_symbol,
                                   time,
                                   lat_str, lon_str,
                                   complete_area_type,
                                   complete_area_color,
                                   lat_offset, lon_offset,
                                   speed_course, complete_corridor,
                                   altitude, course, speed,
                                   (p_station->flag & ST_OBJECT),
                                   transmit_compressed_objects_items);
  }

  else if ( (p_station->aprs_symbol.aprs_type == '\\') // We have a signpost object
            && (p_station->aprs_symbol.aprs_symbol == 'm' ) )
  {
    format_signpost(signpost,sizeof(signpost),p_station->signpost);

    format_signpost_object_item_packet(line, line_length,
                                       p_station->call_sign,
                                       object_group,object_symbol,
                                       time,
                                       lat_str, lon_str,
                                       speed_course,
                                       altitude,
                                       signpost,
                                       course,speed,
                                       (p_station->flag&ST_OBJECT),
                                       transmit_compressed_objects_items);
  }

  else if (p_station->signal_gain[0] != '\0')   // Must be an Omni-DF object/item
  {
    format_omni_df_object_item_packet(line, line_length,
                                      p_station->call_sign,
                                      object_group, object_symbol,
                                      time,
                                      lat_str, lon_str,
                                      p_station->signal_gain,
                                      speed_course,
                                      altitude,
                                      course, speed,
                                      (p_station->flag & ST_OBJECT),
                                      transmit_compressed_objects_items);
  }
  else if (p_station->NRQ[0] != 0)    // It's a Beam Heading DFS object/item
  {
    format_beam_df_object_item_packet(line, line_length,
                                      p_station->call_sign,
                                      object_group, object_symbol,
                                      time,
                                      lat_str, lon_str,
                                      p_station->bearing,
                                      p_station->NRQ,
                                      speed_course,
                                      altitude,
                                      course,speed,
                                      (p_station->flag & ST_OBJECT),
                                      transmit_compressed_objects_items);
  }

  else    // Else it's a normal object/item
  {
    format_normal_object_item_packet(line, line_length,
                                     p_station->call_sign,
                                     object_group, object_symbol,
                                     time,
                                     lat_str, lon_str,
                                     speed_course,
                                     altitude,
                                     course, speed,
                                     (p_station->flag & ST_OBJECT),
                                     transmit_compressed_objects_items);
  }

  // If it's a "killed" object, change '*' to an '_'
  killed=reformat_killed_object_item_packet(line, line_length,
                                            (p_station->flag & ST_OBJECT),
                                            (p_station->flag & ST_ACTIVE));

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

      return(0);
    }

    // Check whether the timeout has been set yet on this killed
    // object/item.  If not, change it from -1 (continuous
    // transmit of non-killed objects) to
    // MAX_KILLED_OBJECT_RETRANSMIT.
    if (p_station->object_retransmit <= -1)
    {

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
        p_station->object_retransmit--;
      }
    }
  }

  // We need to tack the comment on the end, but need to make
  // sure we don't go over the maximum length for an object/item.
  append_comment_to_object_item_packet(line, line_length,
                                       comment,
                                       p_station->call_sign,
                                       (p_station->flag & ST_OBJECT));

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

    // If station is owned by me (Exact match includes SSID)
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

        // Scale the random number from 0.0 to 1.0.
        // Must convert at least one of the numbers to a
        // float else randomize will be zero every time.
        randomize = rand() / (float)RAND_MAX;

        // Scale it to the range we want (0% to 20% of
        // the interval)
        randomize = randomize * one_fifth_increment;

        // Subtract it from the increment, use
        // poor-man's rounding to turn the random number
        // into an int (so we get the full range).
        new_increment = increment - (int)(randomize + 0.5);
        p_station->transmit_time_increment = (short)new_increment;

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
          // Don't transmit it.
        }
      }
      else    // Not time to transmit it yet
      {
      }
    }
  }
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


  // If it's my call in the new_owner field, then I must have just
  // deleted the object and am transmitting a killed object for
  // it.  If it's not my call, someone else has assumed control of
  // the object.
  //
  // Comment out any references to the object in the log file so
  // that we don't start retransmitting it on a restart.

  if (is_my_call(new_owner,1))   // Exact match includes SSID
  {
  }
  else
  {
    fprintf(stderr,"Disowning '%s': '%s' is taking over control of it.\n",
            call_sign, new_owner);
  }

  get_user_base_dir("config/object.log", file, sizeof(file));

  get_user_base_dir("config/object-temp.log", file_temp, sizeof(file_temp));

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

    if (valid_object(name))
    {

      if ( strcmp(name,call_sign) == 0 )
      {
        // Match.  Comment it out in the file unless it's
        // already commented out.
        if (line[0] != '#')
        {
          fprintf(f,"#%s",line);
        }
        else
        {
          fprintf(f,"%s",line);
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


/*
  This function exists to take all the strings that are obtained
  directly from the Object/Item create/modify dialog box and create a
  stand-alone DataRow structure that can be read by
  Create_object_item_tx_string to craft a correctly formatted object
  or item packet.

  Some of the character strings in the argument list will be null
  strings, either because the user left them blank or because they
  don't apply to the object type being created.

  The integer types here are all booleans that reflect the settings of
  toggle buttons in the dialog.

  This function will return a null pointer if the name string is empty.  It
  will abort with a fatal error if the memory allocation call fails.

  It is ASSUMED that the caller will have done all required sanity
  checking on the name and latitude/longitude strings.  We do only very
  basic checking here, mostly to avoid overrunning buffers.

  In order to get a valid pointer return, at least name, lat/lon_str,
  obj_group, and obj_symbol must be provided, in which case the object is
  just a simple static object.

  Parameters:
      name:   the object or item name with trailing spaces deleted
      lat_str,lon_str:  latitude and longitude in DDMM.MM[M]H/DDDMM.MM[M]H
                        format.  They will be converted to Xastir coordinates
                        before storage in the data row.
      obj_group, obj_symbol: The group and symbol taken from the
                              dialog box.  if obj_group is neither '/'
                              nor '\' then it must be a valid overlay
                              character, the upper case letters A-Z or
                              digits 0-9.  The actual group stored in
                              the record as the object symbol group
                              will be '\' and the group given will be
                              stored instead in the overlay field.
                              If the overlay character is invalid, it will be
                              ignored and we'll just use '\' as the group.
      comment:  A comment string up to 43 characters long.  May be null.
      course, speed:          course in degrees, speed in knots.  May be null.
                              must be no more than three digits.
      altitude:               altitude in feet.
                              While the dialog prompts the user for altitude
                              in feet, it gets converted into meters for
                              storage in the DataRow comments per
                              comments in database.h and as handled by
                              functions in db.c.
                              Create_object_item_tx_string converts back
                              to feet for transmit.  Eep.
      area_object, area_type, area_filled:
                              If area_object is nonzero, we are doing an
                              area object and the following flags define
                              the type (0-15) and whether or not it's filled.
     area_color:              two character color string /0 through 15.
     lat_offset_str, lon_offset_str:
                              latitude and longitude offset values in 1/100
                              of a degree.  The actual value stored will be
                              the square root of this number.  May be null.
     corridor:                optional three digit corridor used only by
                              area object types 1 and 6 (lines left and right)
     signpost_object:         if nonzero, we're creating a signpost object.
     signpost_str:            character string up to three characters to
                              be displayed as signpost text.  May be null.
     df_object:               if nonzero, this is a DF report object.
     omni_df:                 if nonzero, it's an omnidirectional report and
                              we need the SHGD string
     beam_df:                 if nonzero, we're a beam reading DF report
                              and need bearing and NRQ.
     df_shgd:                 signal quality, etc. for omni DF
     bearing:                 beam DF bearing
     NRQ:                     beam width, etc. for beam df object
     prob_circles:            if nonzero, we're a probability circles object,
                              and expect prob_min and/or prob_max to be
                              non-null.
     prob_min, prob_max:      min and max radii of probability circles.  May
                              be null (if both are null, this winds up being
                              just another ordinary object)
     is_object:               if nonzero, set this record's flag to have the
                              ST_OBJECT bit set.  Otherwise, set ST_ITEM.
     killed:                  if nonzero, unset the ST_ACTIVE flag.  Otherwise,
                              set the ST_ACTIVE flag.

    THIS FUNCTION ALLOCATES DATA THAT MUST BE FREED.
    DO NOT USE THE STATION DELETE FUNCTIONS IN DB.C BECAUSE THEY PRESUME
    THE RECORD IS LINKED INTO THE LINKED LISTS, AND THESE WILL NOT BE.

    This function is in objects.c and not db.c because it is intended ONLY
    as a function for preparing a fake record to be used in object packet
    creation, which will then be discarded after the string is created.
*/
DataRow *construct_object_item_data_row(char *name,
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
                                        int killed)
{
  DataRow *theDataRow = NULL;
  // if we have these three strings and they're not null, go ahead and try
  // to do the thing.  We are ASSUMING the caller has already taken care
  // of sanity checking them.
  if (name!=NULL && lat_str != NULL && lon_str != NULL &&
      strlen(name) != 0 && strlen(lat_str) != 0 && strlen(lon_str) != 0)
  {
    theDataRow = (DataRow *)calloc(1,sizeof(DataRow));
    CHECKMALLOC(theDataRow);
    init_station(theDataRow); // populate with defaults

    //
    // Generic data for all object types
    //

    // Truncate name if necessary
    if (strlen(name) > sizeof(theDataRow->call_sign)-1)
    {
      name[sizeof(theDataRow->call_sign)-1] = '\0';
    }
    strncpy(theDataRow->call_sign,name,sizeof(theDataRow->call_sign)-1);

    theDataRow->coord_lat = convert_lat_s2l(lat_str);
    theDataRow->coord_lon = convert_lon_s2l(lon_str);
    theDataRow->flag |= (is_object)?(ST_OBJECT):(ST_ITEM);

    if (killed)
      theDataRow->flag &= ~ST_ACTIVE;
    else
      theDataRow->flag |= ST_ACTIVE;

    if (obj_group == '/' || obj_group == '\\')
    {
      theDataRow->aprs_symbol.aprs_type=obj_group;
    }
    else
    {
      theDataRow->aprs_symbol.aprs_type='\\';
      if ((obj_group >= 'A' && obj_group <= 'Z')
          || (obj_group >= '0' && obj_group <= '9'))
      {
        theDataRow->aprs_symbol.special_overlay = obj_group;
      }
      else
      {
        theDataRow->aprs_symbol.special_overlay = '\0';
      }
    }
    theDataRow->aprs_symbol.aprs_symbol = obj_symbol;
    if (course && strlen(course) >= 1 && strlen(course)<=3 && atoi(course)>0 && atoi(course) <=360)
    {
      xastir_snprintf(theDataRow->course,sizeof(theDataRow->course),"%03d",atoi(course));
    }
    if (speed && strlen(speed) >= 1 && strlen(speed)<=3 )
    {
      xastir_snprintf(theDataRow->speed,sizeof(theDataRow->speed),"%3d",atoi(speed));
    }
    if (altitude && strlen(altitude) > 0)
    {
      long alt_in_feet=atoi(altitude);
      if (alt_in_feet >=0 && alt_in_feet <= 999999)
      {
        double alt_in_meters=atof(altitude)*0.3048;
        xastir_snprintf(theDataRow->altitude,sizeof(theDataRow->altitude),"%.2f",alt_in_meters);
      }
    }
    if (comment && strlen(comment)>0)
    {
      add_comment(theDataRow,comment);
    }

    // Specific data for fancier object types
    if (area_object)
    {

      // Enforce correct symbol.  Must be '\l', no overlay.
      theDataRow->aprs_symbol.aprs_type='\\';
      theDataRow->aprs_symbol.aprs_symbol='l';
      theDataRow->aprs_symbol.special_overlay = '\0';

      // Area objects are not allowed to have course/speed, clobber those
      // if they were given
      theDataRow->speed[0] = '\0';
      theDataRow->course[0] = '\0';

      if (area_filled && area_type != 1 && area_type != 6)
      {
        theDataRow->aprs_symbol.area_object.type = area_type+5;
      }
      else
      {
              theDataRow->aprs_symbol.area_object.type = area_type;
      }
      // here we assume that the area color has already been processed
      // as far as correcting it for dim/bright by the caller, and that it
      // is provided us *exactly* as it needs to be
      theDataRow->aprs_symbol.area_object.color = area_color_from_string(area_color);

      // The dialog asks the user to input the lat/lon offsets in 1/100 degree,
      // but the value is actually stored as the integer part of the square
      // root of the value input, because that's what's actually transmitted
      // per spec.
      if (lat_offset_str && strlen(lat_offset_str) != 0)
      {
        int lat_offset;
        lat_offset = sqrt(atof(lat_offset_str));
        if (lat_offset > 99)
          lat_offset = 99;
        theDataRow->aprs_symbol.area_object.sqrt_lat_off = lat_offset;
      }
      if (lon_offset_str && strlen(lon_offset_str) != 0)
      {
        int lon_offset;
        lon_offset = sqrt(atof(lon_offset_str));
        if (lon_offset > 99)
          lon_offset = 99;
        theDataRow->aprs_symbol.area_object.sqrt_lon_off = lon_offset;
      }
      // only line left and line right get this set:
      if (corridor && strlen(corridor) != 0 &&
          (area_type == AREA_LINE_LEFT || area_type == AREA_LINE_RIGHT))
      {
        unsigned int cwidth = atoi(corridor);
        if (cwidth >0 && cwidth < 999)
        {
          theDataRow->aprs_symbol.area_object.corridor_width = cwidth;
        }
      }
    }
    else if (signpost_object)
    {
      // Enforce correct symbol.  Must be '\m', no overlay.
      theDataRow->aprs_symbol.aprs_type='\\';
      theDataRow->aprs_symbol.aprs_symbol='m';
      theDataRow->aprs_symbol.special_overlay = '\0';

      if (signpost_str && strlen(signpost_str) >0 && strlen(signpost_str) <= 3)
      {
        xastir_snprintf(theDataRow->signpost,sizeof(theDataRow->signpost),"%s",signpost_str);
      }
    }
    else if (df_object)
    {
      // Enforce correct symbol.  Must be '/\', no overlay.
      theDataRow->aprs_symbol.aprs_type='/';
      theDataRow->aprs_symbol.aprs_symbol='\\';
      theDataRow->aprs_symbol.special_overlay = '\0';
      if (omni_df)
      {
        if (df_shgd && strlen(df_shgd) == 4)
        {
          xastir_snprintf(theDataRow->signal_gain,sizeof(theDataRow->signal_gain),"DFS%s",df_shgd);
        }
      }
    }
  }
  return theDataRow;
}
//
// this function is sufficient to deallocate a DataRow structure as created
// by the previous function, which can only set the comment field.  The
// comment field is a dynamically allocated string pointed to by a pointer
// stored in the DataRow, so has to be deallocated before freeing the row
// structure itself.  This is much simpler than the function station_del
// in db.c, which must look up the record in a linked list, deallocate lots
// of dynamic pieces, then call a function to unlink the record and then
// deallocate it.  Don't call that one, because we aren't in the linked list
// in the first place.
//
void destroy_object_item_data_row(DataRow *theDataRow)
{
  (void) delete_comments_and_status(theDataRow);
  free(theDataRow);
}
