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

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "xastir.h"
#include "main.h"

// Must be last include file
#include "leak_detection.h"





pid_t play_sound(char *sound_cmd, char *soundfile)
{
  pid_t sound_pid;
  char command[600];

  sound_pid=0;
  if (strlen(sound_cmd)>3 && strlen(soundfile)>1)
  {
    if (last_sound_pid==0)
    {

      // Create a new process to run in
      sound_pid = fork();
      if (sound_pid!=-1)
      {
        if(sound_pid==0)
        {
// This is the child process


          // Go back to default signal handler instead of
          // calling restart() on SIGHUP
          (void) signal(SIGHUP,SIG_DFL);


          // Change the name of the new child process.  So
          // far this only works for "ps" listings, not
          // for "top".  This code only works on Linux.
          // For BSD use setproctitle(3), NetBSD can use
          // setprogname(2).
#ifdef __linux__
          init_set_proc_title(my_argc, my_argv, my_envp);
          set_proc_title("%s", "festival process (xastir)");
          //fprintf(stderr,"DEBUG: %s\n", Argv[0]);
#endif  // __linux__


          xastir_snprintf(command,
                          sizeof(command),
                          "%s %s/%s",
                          sound_cmd,
                          SOUND_DIR,
                          soundfile);

          if (system(command) != 0) {}  // We don't care whether it succeeded
          exit(0);    // Exits only this process, not Xastir itself
        }
        else
        {
// This is the parent process
          last_sound_pid=sound_pid;
        }
      }
      else
      {
        fprintf(stderr,"Error! trying to play sound\n");
      }
    }
    else
    {
      sound_pid=last_sound_pid;
      /*fprintf(stderr,"Sound already running\n");*/
    }
  }
  return(sound_pid);
}





int sound_done(void)
{
  int done;
  int *status;

  status=NULL;
  done=0;
  if(last_sound_pid!=0)
  {
    if(waitpid(last_sound_pid,status,WNOHANG)==last_sound_pid)
    {
      done=1;
      last_sound_pid=0;
    }
  }
  return(done);
}


