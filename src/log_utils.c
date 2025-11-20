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

#if HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif // HAVE_SYS_TIME_H
#include <time.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include "xastir.h"
#include "main.h"
#include "globals.h"
#include "util.h"
#include "db_funcs.h"
#include "xa_config.h"
#include "snprintf.h"

#include "maps.h" // for fill_in_new_alert_entries prototype

#define MAX_LOGFILE_SIZE 2048000


char *fetch_file_line(FILE *f, char *line)
{
  char cin;
  int pos;


  pos = 0;
  line[0] = '\0';

  while (!feof(f))
  {

    // Read one character at a time
    if (fread(&cin,1,1,f) == 1)
    {

      if (pos < MAX_LINE_SIZE)
      {
        if (cin != (char)13)    // CR
        {
          line[pos++] = cin;
        }
      }

      if (cin == (char)10)   // Found LF as EOL char
      {
        line[pos++] = '\0'; // Always add a terminating zero after last char
        pos = 0;          // start next line
        return(line);
      }
    }
  }

  // Must be end of file
  line[pos] = '\0';
  return(line);
}


// used by log_data
void rotate_file(char *file, int max_keep )
{
  int i;
  char file_a[MAX_FILENAME];
  char file_b[MAX_FILENAME];
  struct stat file_status;

  if (debug_level & 1)
  {
    fprintf(stderr, "Rotating: %s. Will keep %d \n", file, max_keep);
  }

  for(i=max_keep; i>=1; i--)
  {

    if (debug_level & 1)
    {
      fprintf(stderr, "rotate: %s : %s\n", file_b, file_a);
    }

    xastir_snprintf(file_a,sizeof(file_a),"%s.%d",file,i);
    xastir_snprintf(file_b,sizeof(file_b),"%s.%d",file,i-1);

    unlink (file_a);
    if (stat(file_a, &file_status) == 0)
    {
      // We got good status.  That means it didn't get deleted!
      fprintf(stderr,
              "Couldn't delete file '%s': %s",
              file_a,strerror(errno));
      return;
    }

    // Rename previous to next
    //
    // Check whether file_b exists
    if (stat(file_b, &file_status) == 0)
    {
      if (!S_ISREG(file_status.st_mode))
      {
        fprintf(stderr,
                "Couldn't stat %s\n",
                file_b);
        break;
      }
      if ( rename (file_b, file_a) )
      {
        fprintf(stderr,
                "Couldn't rename %s to %s, cancelling log_rotate()\n",
                file_b,
                file_a);
        return;
      }
    }

  }

  if (debug_level & 1)
  {
    fprintf(stderr, "rotate: %s : %s\n", file, file_a);
  }

  if ( rename (file, file_a) )
  {
    fprintf(stderr,
            "Couldn't rename %s to %s, cancelling log_rotate()\n",
            file,
            file_a);
  }

  return;
}




// Restore weather alerts so that we have a clear picture of the
// current state.  Check timestamps on the file.  If relatively
// current, read the file in.
//
void load_wx_alerts_from_log_working_sub(time_t time_now, char *filename)
{
  time_t file_timestamp;
  int file_age;
  int expire_limit;   // In seconds
  char line[MAX_LINE_SIZE+1];
  FILE *f;


  expire_limit = 60 * 60 * 24 * 15;   // 15 days
//    expire_limit = 60 * 60 * 24 * 1;   // 1 day

  file_timestamp = file_time(filename);

  if (file_timestamp == -1)
  {
//        fprintf(stderr,"File %s doesn't exist\n", filename);
    return;
  }

  file_age = time_now - file_timestamp;

  if ( file_age > expire_limit)
  {
//        fprintf(stderr,"Old file: %s, skipping...\n", filename);
    return;
  }

//    fprintf(stderr,"File is current: %s\n", filename);

  // Read the file in, as it exists and is relatively new.

  // Check timestamps before each log line and skip those that are
  // old.  Lines in the file should look about like this:
  //
  // # 1157027319  Thu Aug 31 05:28:39 PDT 2006
  // OUNSWO>APRS::SKYOUN   :OUN Check For Activation VCEAB
  // # 1157027319  Thu Aug 31 05:28:39 PDT 2006
  // LZKFFS>APRS::NWS_ADVIS:311324z,FLOOD,ARC67-147 V1PAA
  //
  // We could try to use the regular read_file and read_file_ptr
  // scheme for reading in the log file, but we'd have to modify
  // it in two ways:  We need to keep from bringing up the
  // interfaces so we'd need to set a flag when done reading and
  // then start them, plus we'd need to have it check the
  // timestamps and skip old ones.  Instead we'll do it all on our
  // own here so that we can control everything ourselves.

  f = fopen(filename, "r");
  if (!f)
  {
    fprintf(stderr,"Wx Alert log file could not be opened for reading\n");
    return;
  }

  while (!feof(f))    // Read until end of file
  {

    (void)fetch_file_line(f, line);

restart_sync:

    if (line[0] == '\0')
    {
      // Empty line found, skip
    }

    else if (line[0] == '#')   // Timestamp line, check the date
    {
      time_t line_stamp;
      int line_age;

      if (strlen(line) < 3)   // Line is too short, skip
      {
        goto restart_sync;
      }

      line_stamp = atoi(&line[2]);
      line_age = time_now - line_stamp;
//fprintf(stderr, "Age: %d\t", line_age);

      if ( line_age < expire_limit)
      {
        // Age is good, read next line and process it

        (void)fetch_file_line(f, line);

        if (line[0] != '#')   // It's a packet, not a timestamp line
        {
//fprintf(stderr,"%s\n",line);
          decode_ax25_line(line,'F',-1, 1);   // Decode the packet
        }
        else
        {
          goto restart_sync;
        }
      }
    }
  }
  if (feof(f))   // Close file if at the end
  {
    (void)fclose(f);
  }
}





// Restore weather alerts so that we have a clear picture of the
// current state.  Do this before we start the interfaces.  Only
// reload if the log files datestamps are relatively current.
//
// Check timestamps on each file in turn.  If relatively
// current, read them in the correct order:
// wx_alert.log.3
// wx_alert.log.2
// wx_alert.log.1
// wx.alert.log
//
void load_wx_alerts_from_log(void)
{
  time_t time_now;
  char filename[MAX_FILENAME];
  char logfile_path[MAX_VALUE];

  get_user_base_dir(LOGFILE_WX_ALERT,logfile_path, sizeof(logfile_path));

  time_now = sec_now();

  fprintf(stderr,"*** Reading WX Alert log files\n");

  // wx_alert.log.3
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s.3",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  // wx_alert.log.2
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s.2",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  // wx_alert.log.1
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s.1",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  // wx_alert.log
  xastir_snprintf(filename,
                  sizeof(filename),
                  "%s",
                  logfile_path );
  load_wx_alerts_from_log_working_sub(time_now, filename);

  fill_in_new_alert_entries();

  fprintf(stderr,"*** Done with WX Alert log files\n");
}





// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void log_data(char *file, char *line)
{
  FILE *f;
  struct stat file_status;
  int reset_setuid = 0 ;


  // Check for "# Tickle" first, don't log it if found.
  // It's an idle string designed to keep the socket active.
  if ( (strncasecmp(line, "#Tickle", 7)==0)
       || (strncasecmp(line, "# Tickle", 8) == 0) )
  {
    return;
  }
  else
  {
    char temp[200];
    char timestring[100+1];
    struct tm *time_now;
    time_t secs_now;

    // Fetch the current date/time string
//        get_timestamp(timestring);
    secs_now=sec_now();

    time_now = localtime(&secs_now);

    (void)strftime(timestring,100,"%a %b %d %H:%M:%S %Z %Y",time_now);

    xastir_snprintf(temp,
                    sizeof(temp),
                    "# %ld  %s",
                    (unsigned long)secs_now,
                    timestring);

    // Change back to the base directory

// This call corrupts the "file" variable.  Commented it out as we
// don't appear to need it anyway.  The complete root-anchored
// path/filename are passed to us in the "file" parameter.
//
//        chdir(get_user_base_dir(""));

    // check size and rotate if too big

    if (stat(file, &file_status)==0)
    {

//            if (debug_level & 1) {
//                fprintf(stderr, "log_data(): logfile size: %ld \n",(long) file_status.st_size);
//            }

      if ((file_status.st_size + strlen(temp) + strlen(line) )> MAX_LOGFILE_SIZE)
      {
        if (debug_level & 1)
        {
          fprintf(stderr, "log_data(): calling rotate_file()\n");
        }

        rotate_file(file,3);
      }

    }
    else
    {
      // ENOENT is ok -- we make the file below
      if (errno != ENOENT )
      {
        fprintf(stderr,"Couldn't stat log file '%s': %s\n",
                file,strerror(errno));
        return;
      }

    }

    if (getuid() != geteuid())
    {
      reset_setuid=1;
      DISABLE_SETUID_PRIVILEGE;
    }

    f=fopen(file,"a");
    if (f!=NULL)
    {
      fprintf(f,"%s\n", temp); // Write the timestamp line
      fprintf(f,"%s\n",line);  // Write the data line
      (void)fclose(f);
    }
    else
    {
      fprintf(stderr,"Couldn't open file for appending: %s\n", file);
    }

    if(reset_setuid)
    {
      ENABLE_SETUID_PRIVILEGE;
    }
  }
}

