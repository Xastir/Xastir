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

#include <Xm/XmAll.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

//Needed for Solaris 2.5
#include <netinet/in.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <setjmp.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <errno.h>

#include "xastir.h"
#include "main.h"
#include "lang.h"

// Must be last include file
#include "leak_detection.h"
#include "forked_getaddrinfo.h"

#ifndef HAVE_SIGHANDLER_T
  #ifdef HAVE_SIG_T
    typedef sig_t sighandler_t;
  #else
    typedef void (*sighandler_t)(int);
  #endif
#endif


#ifndef __LCLINT__
  #ifndef HAVE_SIGJMP_BUF
    jmp_buf ret_place;
  #else // HAVE_SIGJMP_BUF
    static  sigjmp_buf ret_place;       /* Jump address if alarm */
  #endif    // HAVE_SIGJMP_BUF
#endif // __LCLINT__





/*************************************************************************/
/* Time out on connect                                                   */
/* In case there is a problem in getting the hostname or connecting      */
/* (see  setjmp below).                                                  */
/*************************************************************************/

static void host_time_out( int UNUSED(sig) )
{
#ifndef __LCLINT__
  siglongjmp(ret_place,0);
#endif // __LCLINT__
}





/*************************************************************************/
/* do a nice host lookup (don't thread!!)                                */
/*                                                                       */
/* host: name to lookup                                                  */
/* ip: buffer for ip's must be 400 bytes at least                        */
/* time: time in seconds to wait                                         */
/*                                                                       */
/* return the ip or ip's of the host name                                */
/* or these strings:                                                     */
/* NOHOST  for no host by that name found                                */
/* NOIP    for host found but no ip address available                    */
/* TIMEOUT for time exceeded                                             */
/*************************************************************************/

int forked_getaddrinfo(const char *hostname, const char *servname, const struct addrinfo *hints, struct addrinfo **resout, int time)
{

#ifdef RETSIGTYPE
  RETSIGTYPE * previous_loc;
#else
#error RETSIGTYPE not defined
#endif

  pid_t host_pid;
  int status;
  int fp[2];
  int tm;
  int i;
  int rc = EAI_FAIL;
  char ttemp[60];
  int wait_host;
  struct addrinfo *res0;
  struct addrinfo *res;

  if (debug_level & 1024)
  {
    fprintf(stderr,"Start Host lookup\n");
  }

  busy_cursor(appshell);

  if (debug_level & 1024)
  {
    fprintf(stderr,"Creating pipe\n");
  }

  // Create a pipe for communication
  if (pipe(fp)!=0)
  {
    fprintf(stderr, "Error creating pipe for hostname lookup: %s\n",
            strerror(errno));
    return rc;
  }

  host_pid = fork();      // Fork off a child process

  if (debug_level & 1024)
  {
    fprintf(stderr,"Host fork\n");
  }

  if (host_pid!=-1)       // If the fork was successful
  {

//---------------------------------------------------------------------------------------
    if (host_pid==0)    // We're in the child process
    {


      // Go back to default signal handler instead of
      // calling restart() on SIGHUP
      (void) signal(SIGHUP,SIG_DFL);


      // Change the name of the new child process.  So far
      // this only works for "ps" listings, not for "top".
      // This code only works on Linux.  For BSD use
      // setproctitle(3), NetBSD can use setprogname(2).
#ifdef __linux__
      init_set_proc_title(my_argc, my_argv, my_envp);
      set_proc_title("%s", "hostname lookup (xastir)");
      //fprintf(stderr,"DEBUG: %s\n", Argv[0]);
#endif  // __linux__


      // Close the end of the pipe we don't need here

      if (debug_level & 1024)
      {
        fprintf(stderr,"Child closing read end of pipe\n");
      }

      close(fp[0]);   // Read end of the pipe

      if (debug_level & 1024)
      {
        fprintf(stderr,"Set alarm \n");
      }

#ifdef RETSIGTYPE
      previous_loc = (RETSIGTYPE *)signal(SIGALRM, host_time_out);
#else
      previous_loc = signal(SIGALRM, host_time_out);
#endif

      // Set up to jump here if we time out on SIGALRM
      if (sigsetjmp(ret_place,-1)!=0)
      {

        // Turn off the alarm
        (void)alarm(0);

        // Reset the SIGALRM handler to its previous value
        (void)signal(SIGALRM, (sighandler_t)previous_loc);

        // Return net connection time out
        rc = FAI_TIMEOUT;

        if (write(fp[1],&rc, sizeof(int)) == -1)
        {
          // Write error. Do nothing as we did prior
          // to checking the return value.
        }

        if (debug_level & 1024)
        {
          fprintf(stderr,"Child closing write end of pipe\n");
        }

        close(fp[1]);   // All done writing to the pipe
        exit(EXIT_FAILURE); // Exit from child process
      }
      (void)alarm(time);  // Start the timer

      // Make the call that may time out if no response from DNS
      /*hostinfo = gethostbyname2(host,AF_INET); some systems don't have this*/
      rc = getaddrinfo(hostname, servname, hints, &res0);

      // If we get to here, we haven't timed out
      // and we have an answer to process.

      // Turn off the alarm
      (void)alarm(0);
      // Reset the SIGALRM handler to its previous value
      (void)signal(SIGALRM, (sighandler_t)previous_loc);

      if (write(fp[1], &rc, sizeof(int)) == -1)    // Send status
      {
        // Write error. Do nothing as we did prior
        // to checking the return value.
      }

      for (res = res0; res; res = res->ai_next)
      {
        // Signal that an entry is coming.
        rc = 1;

        if (write(fp[1], &rc, sizeof(int)) == -1)
        {
          // Write error. Do nothing as we did prior
          // to checking the return value.
        }

        // Send  entry
        if (write(fp[1], res, sizeof(struct addrinfo)) == -1)
        {
          // Write error. Do nothing as we did prior
          // to checking the return value.
        }

        if (write(fp[1], res->ai_addr, res->ai_addrlen) == -1)
        {
          // Write error. Do nothing as we did prior
          // to checking the return value.
        }

      }
      // Signal that there is nothing left
      rc = 0;

      if (write(fp[1], &rc, sizeof(int)) == -1)
      {
        // Write error. Do nothing as we did prior
        // to checking the return value.
      }

      if (debug_level & 1024)
      {
        fprintf(stderr,"Child closing write end of pipe\n");
      }

      freeaddrinfo(res0);
      close(fp[1]);   // All done writing to the pipe
      exit(EXIT_FAILURE); // Exit from child process

    }   // End of child process
//---------------------------------------------------------------------------------------
    else
    {
      // We're in the parent process at this point

      // Close the end of the pipe we don't need here

      if (debug_level & 1024)
      {
        fprintf(stderr,"Parent closing write end of pipe\n");
      }

      close(fp[1]);   // Write end of the pipe

      tm=1;
      wait_host=1;
      while (wait_host!=-1)
      {
        xastir_snprintf(ttemp, sizeof(ttemp), langcode("BBARSTA031"), tm++);
        statusline(ttemp,1);        // Looking up hostname...

        for (i=0; i < 60 && wait_host!=-1; i++)
        {
          wait_host=waitpid(host_pid,&status,WNOHANG);
          /* update display while waiting */
//                        XmUpdateDisplay(XtParent(da));
          //usleep(500);
          sched_yield();
        }
      }
      // Get the return code
      if (read(fp[0],&rc,sizeof(int)) == -1)
      {
        // Read error. Do nothing as we did prior
        // to checking the return value.
      }

      if(rc!=0)
      {
        close(fp[0]);
        return rc;
      }

      if (read(fp[0], &status, sizeof(int)) == -1)
      {
        // Read error. Do nothing as we did prior
        // to checking the return value.
      }

      while(status)
      {
        *resout = (struct addrinfo*) malloc(sizeof(struct addrinfo));
        res = *resout;

        if (read(fp[0], res, sizeof(struct addrinfo)) == -1)
        {
          // Read error. Do nothing as we did prior
          // to checking the return value.
        }

        res->ai_addr = (struct sockaddr*) malloc(res->ai_addrlen);

        if (read(fp[0], res->ai_addr, res->ai_addrlen) == -1)
        {
          // Read error. Do nothing as we did prior
          // to checking the return value.
        }

        res->ai_canonname = NULL;
        resout = &res->ai_next;

        // See if there is another
        if (read(fp[0], &status, sizeof(int)) == -1)
        {
          // Read error. Do nothing as we did prior
          // to checking the return value.
        }
      }
      *resout = NULL;

      if (debug_level & 1024)
      {
        fprintf(stderr,"Parent closing read end of pipe\n");
      }

      close(fp[0]);   // Close the read end of the pipe
    }
  }
  else    // We didn't fork
  {
    // Close both ends of the pipe to make
    // sure we've cleaned up properly
    close(fp[0]);
    close(fp[1]);
  }
  return rc;
}

void forked_freeaddrinfo(struct addrinfo *ai)
{
  struct addrinfo *next = ai;
  struct addrinfo *current = ai;

  while(next)
  {
    current = next;
    next = current->ai_next;

    free(current->ai_addr);
    free(current);
  }
}


