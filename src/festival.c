// Modification for Xastir CVS purposes
//
// $Id$
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
#include "festival.h"
#include <errno.h>
#include "xastir.h"

FT_Info *info;      



void festival_default_info()
{
    info = (FT_Info *)malloc(1 * sizeof(FT_Info));
    info->server_host = FESTIVAL_DEFAULT_SERVER_HOST;
    info->server_port = FESTIVAL_DEFAULT_SERVER_PORT;
    info->text_mode = FESTIVAL_DEFAULT_TEXT_MODE;
    info->server_fd = -1;
    
    return ;
}



static int festival_socket_open(const char *host, int port)
{   
    /* Return an FD to a remote server */
    struct sockaddr_in serv_addr;
    struct hostent *serverhost;
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd < 0)  
    {
    fprintf(stderr,"festival_client: can't get socket\n");
    return -1;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    if ((int)(serv_addr.sin_addr.s_addr = inet_addr(host)) == -1)
    {
    /* its a name rather than an ipnum */
    serverhost = gethostbyname(host);
    if (serverhost == (struct hostent *)0)
    {
        fprintf(stderr,"festival_client: gethostbyname failed\n");
        return -1;
    }
    memmove(&serv_addr.sin_addr,serverhost->h_addr, (size_t)serverhost->h_length);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
    {
    fprintf(stderr,"festival_client: connect to server failed\n");
    return -1;
    }
    return fd;
}



/***********************************************************************/
/* Public Functions to this API                                        */
/***********************************************************************/

void festivalOpen()
{
    /* Open socket to server */

    if (info == 0)
    festival_default_info();

    info->server_fd = 
    festival_socket_open(info->server_host, info->server_port);
    if (info->server_fd == -1)
    return;

    return;
}


void festivalStringToSpeech(char *text)
{
    FILE *fd;
    char *p;
    char ack[4];
    int n;
    int tmp = 0;

    if (info == 0)
    return;

    if (info->server_fd == -1)
    {
    fprintf(stderr,"festival_client: server connection unopened\n");
    return;
    }
    fd = fdopen(dup(info->server_fd),"wb");
    /*
    **  Send the mode commands to festival
    */
    fprintf(fd,"(audio_mode `async)\n(SayText \"\n");
    /* 
    **  Copy text over to server, escaping any quotes 
    */ 
    for (p=text; p && (*p != '\0'); p++)
      {
        if ((*p == '"') || (*p == '\\'))
          {
            (void)putc('\\',fd);
          } else {
            /* 
            ** Then convert any embedded '-' into the word 'dash'
            ** This could cause problems with spoken text from 
            ** Weather alerts or messages.  We'll deal with that 
            ** later if necessary.  Making this a separate function
            ** is probably the thing to do.
            */
            if (*p == '-' ) 
              {
                fprintf(fd,",dash,");
              } else {
                (void)putc(*p,fd);
              }
          }
      } 
    /*
    ** Complete the command to xastir, close the quotes and 
    ** set the mode to 'fundamental'
    */
    fprintf(fd,"\" \"%s\")\n",info->text_mode);
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
    for (n=0; n < 3; ) {
      if ( ( tmp = read(info->server_fd,ack+n,3-n)) != -1 ) {
        n = n + tmp;
      } else {
        if (debug_level & 2)    
            fprintf(stderr,"Error reading festival ACK - %s\n",strerror(errno));
        n = 3;
        if (errno == ECONNRESET) {
          info = 0;
          festivalOpen();
        } 
      }
    }
        
    /*
    ** Null terminate the string
    */
    ack[3] = '\0';
    if (strcmp(ack,"ER\n") == 0) {    /* server got an error */
      fprintf(stderr,"festival_client: server returned error\n");
    }
    return;
}



int festivalClose()
{
    if (info == 0)
    return 0;

    if (info->server_fd != -1)
    (void)close(info->server_fd);

    return 0;
}



int SayText(char *text)
{
    if (debug_level & 2)
        printf("SayText: %s\n",text);

    festivalStringToSpeech(text);
    return 0;
}



int SayTextInit()
{
       festival_default_info();
       festivalOpen();
        return 0;
}

