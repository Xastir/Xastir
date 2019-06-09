// Modification for Xastir CVS purposes
//
// Portions Copyright (C) 2000-2019 The Xastir Group
//
//
// End of modification



/*************************************************************************/
/*                                                                       */
/*                Centre for Speech Technology Research                  */
/*                     University of Edinburgh, UK                       */
/*                        Copyright (c) 1999                             */
/*                        All Rights Reserved.                           */
/*                                                                       */
/*  Permission is hereby granted, free of charge, to use and distribute  */
/*  this software and its documentation without restriction, including   */
/*  without limitation the rights to use, copy, modify, merge, publish,  */
/*  distribute, sublicense, and/or sell copies of this work, and to      */
/*  permit persons to whom this work is furnished to do so, subject to   */
/*  the following conditions:                                            */
/*   1. The code must retain the above copyright notice, this list of    */
/*      conditions and the following disclaimer.                         */
/*   2. Any modifications must be clearly marked as such.                */
/*   3. Original authors' names are not deleted.                         */
/*   4. The authors' names are not used to endorse or promote products   */
/*      derived from this software without specific prior written        */
/*      permission.                                                      */
/*                                                                       */
/*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
/*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
/*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
/*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
/*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
/*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
/*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
/*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
/*  THIS SOFTWARE.                                                       */
/*                                                                       */
/*************************************************************************/
/*             Author :  Alan W Black (awb@cstr.ed.ac.uk)                */
/*             Date   :  March 1999                                      */
/*-----------------------------------------------------------------------*/
/*                                                                       */
/* Client end of Festival server API in C designed specifically for      */
/* Galaxy Communicator use though might be of use for other things       */
/*                                                                       */
/* This is a standalone C client, no other Festival or Speech Tools      */
/* libraries need be link with this.  Thus is very small.                */
/*                                                                       */
/* Compile with (plus socket libraries if required)                      */
/*    cc -o festival_client -DSTANDALONE festival_client.c               */
/*                                                                       */
/* Run as                                                                */
/*    festival_client -text "hello there" -o hello.snd                   */
/*                                                                       */
/*                                                                       */
/* This is provided as an example, it is quite limited in what it does   */
/* but is functional compiling without -DSTANDALONE gives you a simple   */
/* API                                                                   */
/*                                                                       */
/*************************************************************************/
/*                                                                       */
/* Heavily modified and Hacked together to provide a general purpose     */
/* interface for use in XASTIR by:                                       */
/*                                                                       */
/*    Ken Koster    N7IPB    03/24/2001                                  */
/*                                                                       */
/*                                                                       */
/*  More comments added and 'do' loop that waited for 'ok' response      */
/*  removed.  Also cleaned up escape processing                          */
/*                                                                       */
/*                  N7IPB    04/04/2001                                  */
/*  Test for errno result from 'read' operation and re-opening of        */
/*  connection if it gets closed.                                        */
/*                  N7IPB    04/08/2001                                  */
/*                                                                       */
/*=======================================================================*/

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#include "xastir.h"
#include "festival.h"
#include "snprintf.h"

// Must be last include file
#include "leak_detection.h"



FT_Info *info = NULL;

static char last_speech_text[8000];
static time_t last_speech_time = (time_t)0;
static time_t festival_connect_attempt_time = (time_t)0;





// Set up default struct
//
void festival_default_info(void)
{

  if (info == NULL)   // First time through
  {

    // Malloc storage for the struct
    info = (FT_Info *)malloc(1 * sizeof(FT_Info));

    // Fill in the struct
    if (info != NULL)
    {
      info->server_host = FESTIVAL_DEFAULT_SERVER_HOST;
      info->server_port = FESTIVAL_DEFAULT_SERVER_PORT;
      info->text_mode = FESTIVAL_DEFAULT_TEXT_MODE;
      info->server_fd = -1;
    }
    else    // Couldn't allocate memory
    {
      fprintf(stderr,"festival_default_info: Couldn't malloc\n");
    }
  }

  return;
}





// Returns a FD to a remote server
//
static int festival_socket_open(const char *host, int port)
{
  struct sockaddr_in serv_addr;
  struct hostent *serverhost;
  int fd;


  // Delay at least 60 seconds between each socket attempt
  if ( (festival_connect_attempt_time + 60) > sec_now() )
  {
    //fprintf(stderr,"Not time yet\n");
    return(-1);
  }
  festival_connect_attempt_time = sec_now();

  if ((fd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    fprintf(stderr,"festival_client: can't get socket\n");
    return(-1);
  }

  memset(&serv_addr, 0, sizeof(serv_addr));

  if ((int)(serv_addr.sin_addr.s_addr = inet_addr(host)) == -1)
  {
    /* its a name rather than an ipnum */
    serverhost = gethostbyname(host);
    if (serverhost == (struct hostent *)0)
    {
      fprintf(stderr,"festival_client: gethostbyname failed\n");
      return(-1);
    }
    memmove(&serv_addr.sin_addr,serverhost->h_addr, (size_t)serverhost->h_length);
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
  {
    fprintf(stderr,"festival_client: connect to server failed\n");
    (void)close(fd);    // Close the socket
    return(-1);
  }
  return(fd);
}





/***********************************************************************/
/* Public Functions to this API                                        */
/***********************************************************************/



// Close socket to server
//
int festivalClose(void)
{

  //fprintf(stderr,"festivalClose()\n");

  if (info != NULL)      // We have a struct allocated
  {

    // Check whether we have a socket open
    if (info->server_fd != -1)
    {
      fprintf(stderr,"Closing Festival socket\n");
      (void)close(info->server_fd);   // Close the socket
      info->server_fd = -1;   // Just to be safe
    }

    // Free the struct, zero the pointer.  The struct will get
    // re-created/re-initialized later when we re-open the
    // festival connection.
    free(info);
    info = NULL;
  }

  return(0);
}





// Open socket to server.  Close the connection if one is already
// open.
//
int festivalOpen(void)
{


  //fprintf(stderr,"festivalOpen()\n");

  festival_default_info();

  // Check whether we have a record to work with
  if (info == NULL)
  {
    return(-1);
  }

  // Check whether we already have a socket open (or think we do)
  if (info->server_fd != -1)      // We have a socket open
  {
    (void)festivalClose();      // Close it, free struct
    usleep(50000);              // 50ms wait
  }

  info->server_fd = festival_socket_open(info->server_host, info->server_port);

  if (info->server_fd == -1)      // Error occured opening socket
  {
    //fprintf(stderr,"festivalOpen: Error opening socket\n");
    (void)festivalClose();      // Close, free struct
    usleep(50000);              // 50ms wait
    return(-1);
  }

  return(0);
}





void festivalStringToSpeech(char *text)
{
  FILE *fd;
  char *p;
  char ack[4];
  int n;
  int tmp = 0;
  int ret;


  //fprintf(stderr,"festivalStringToSpeech()\n");

  // If we don't have a struct allocated
  if (info == 0)
  {
    if (festivalOpen() == -1)   // Allocate struct, open socket
    {
      //fprintf(stderr,"festivalStringToSpeech: Couldn't open socket to Festival\n");
      return;
    }
  }

  if (info == 0)      // If socket is still not open
  {
    return;
  }

  if (info->server_fd == -1)
  {
    fprintf(stderr,"festival_client: server connection unopened\n");
    (void)festivalClose();
    return;
  }

  fd = fdopen(dup(info->server_fd),"wb");

  if (fd == NULL)
  {
    fprintf(stderr,"Couldn't create duplicate socket\n");
    (void)festivalClose();
    return;
  }

  /*
  **  Send the mode commands to festival
  */
  ret = fprintf(fd,"(audio_mode `async)\n(SayText \"\n");

  if (ret == 0 || ret == -1)
  {
    fprintf(stderr,"Couldn't send mode commands to festival\n");
    (void)fclose(fd);
    (void)festivalClose();
    return;
  }

  /*
  **  Copy text over to server, escaping any quotes
  */
  for (p=text; p && (*p != '\0'); p++)
  {
    if ((*p == '"') || (*p == '\\'))
    {

      if (putc('\\',fd) == EOF)   // Error writing to socket
      {
        fprintf(stderr,"Error writing to socket\n");
        (void)fclose(fd);
        (void)festivalClose();
        return;
      }
    }
    else
    {
      /*
      ** Then convert any embedded '-' into the word 'dash'
      ** This could cause problems with spoken text from
      ** Weather alerts or messages.  We'll deal with that
      ** later if necessary.  Making this a separate function
      ** is probably the thing to do.
      */
      if (*p == '-' )
      {

        ret = fprintf(fd,",dash,");

        if (ret == 0 || ret == -1)
        {
          fprintf(stderr,"Error writing to socket\n");
          (void)fclose(fd);
          (void)festivalClose();
          return;
        }
      }
      else
      {

        if (putc(*p,fd) == EOF)     // Error writing to socket
        {
          fprintf(stderr,"Error writing to socket\n");
          (void)fclose(fd);
          (void)festivalClose();
          return;
        }
      }
    }
  }
  /*
  ** Complete the command to xastir, close the quotes and
  ** set the mode to 'fundamental'
  */
  ret = fprintf(fd,"\" \"%s\")\n",info->text_mode);

  if (ret == 0 || ret == -1)
  {
    fprintf(stderr,"Error writing to socket\n");
    (void)fclose(fd);
    (void)festivalClose();
    return;
  }

  /*
  ** Close the duplicate port we used for writing
  */
  (void)fclose(fd);
  /*
  ** Read back info from server
  **
  ** We don't really care what we get back.  If it's an error
  ** We're just going to continue on anyway so I've removed the
  ** check here for the 'OK' response.  We still check for an ERror
  ** but we just print the fact we got it and continue on
  **
  ** This could probably use some work,  I need to study the
  ** festival docs a bit more,  this could block and it could
  ** also have more than 3bytes that need to be read.  It doesn't
  ** appear to matter but should be checked into.
  */
  for (n=0; n < 3; )
  {
    if ( ( tmp = read(info->server_fd,ack+n,3-n)) != -1 )
    {
      n = n + tmp;
    }
    else
    {
      if (debug_level & 2)
      {
        fprintf(stderr,"Error reading festival ACK - %s\n",strerror(errno));
      }
      n = 3;
      if (errno == ECONNRESET)
      {

        fprintf(stderr,"Connection reset\n");
        info = 0;

        (void)festivalClose();

        if (festivalOpen() == -1)
        {
          fprintf(stderr,"festivalStringToSpeech2: Couldn't open socket to Festival\n");
          return;
        }

      }
    }
  }

  /*
  ** Null terminate the string
  */
  ack[3] = '\0';
  if (strcmp(ack,"ER\n") == 0)      /* server got an error */
  {
    fprintf(stderr,"festival_client: server returned error\n");
  }
  return;
}





int SayText(char *text)
{

  if (debug_level & 2)
  {
    fprintf(stderr,"SayText: %s\n",text);
  }

  // Check whether the last text was the same and it hasn't been
  // enough time between them (30 seconds).  We include our speech
  // system test string here so that we don't have to wait 30
  // seconds between Test button-presses.
  if ( (strcmp(last_speech_text,text) == 0) // Strings match
       && (strcmp(text,SPEECH_TEST_STRING) != 0)
       && (last_speech_time + 30 > sec_now()) )
  {

    /*
        fprintf(stderr,
            "Same text, skipping speech: %d seconds, %s\n",
            (int)(sec_now() - last_speech_time),
            text);
    */

    return(1);
  }

  //fprintf(stderr,"Speaking: %s\n",text);

  xastir_snprintf(last_speech_text,
                  sizeof(last_speech_text),
                  "%s",
                  text);
  last_speech_time = sec_now();

  festivalStringToSpeech(text);

  return(0);
}





int SayTextInit(void)
{

  if (festivalOpen() == -1)
  {
    fprintf(stderr,"SayText: Couldn't open socket to Festival\n");
  }

  last_speech_text[0] = '\0';
  last_speech_time = (time_t)0;

  return(0);
}


