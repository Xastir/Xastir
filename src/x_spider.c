/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003-2006  The Xastir Group
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



//
// The idea here:  Have Xastir spawn off a separate server (via a
// system() call?) which can provide listening sockets for other
// APRS clients to connect to.  This allows other clients to share
// Xastir's TNC ports.  Forking Xastir directly and running the
// server code doesn't work out well:  The new process ends up with
// the large Xastir memory image.  We need a separate app with a
// small memory image for the server.  It might spawn off quite a
// few processes, and we need it to be small and fast.
//
// The initial goal here is to take one box that is RF-rich (running
// one or more TNC's), and have the ability to let other APRS
// clients (Xastir or otherwise) to share the RF and/or internet
// connections of the "master" Xastir session.  This could be useful
// in an EOC (Emergency Operations Center), a large SAR mission with
// multiple computers, or simply to allow one to connect to their
// home Xastir session from work and gain the use of the RF ports.
// My use:  On a SAR mission, using wireless networking and laptops
// to let everyone see the same picture, multiplexing to one Xastir
// session running a TNC.  As usual, I'm sure other users will
// figure out new and inventive uses for it.
//
// Yes, this does pose some security risks to the license of the
// operator.  I wouldn't recommend having an open port on the
// internet to allow people to use your RF ports.  Of course with
// the authentication stuff that'll be in here, it's as secure as
// the igating stuff that we currently have for APRS.
//
// I thought about using IPC Messaging or FIFO's (named pipes) in
// order to make the connection from this server back to the
// "master" Xastir session.  I looked at the lack of support for
// them in Cygwin and decided to use sockets instead.  We'll set up
// a special registration for Xastir so that this server code knows
// which port is the "master" port (controlling port).
//
// Xastir will end up with a togglebutton to enable the server:
//   Starts up x_spider.
//   Connects to x_spider with a socket.
//   Sends a special string so x_spider knows which one is the
//     controlling (master) socket.
//   All packets received/transmitted by Xastir on any port also get
//     sent to x_spider.
//
// x_spider:
//   Accepts client socket connections.
//   Spawns a new process for each one.
//   Each new process talks to the main x_spider via two pipes.
//   Authenticate each connecting client in the normal manner.
//   Accept data from any socket, echo data out _all_ sockets.
//   If the "master" Xastir sends a shutdown packet, all connections
//     are dropped and x_spider and all it's children will exit.
//   x_spider uses select() calls to multiplex all pipes and the
//     listening socket.  It shouldn't use up much CPU as it'll be
//     in the blocking select call until it has data to process.
//   x_spider's children should also wait in blocking calls until
//     there is data to process.
//   
// This makes the design of the server rather simple:  It needs to
// authenticate clients and it needs to parse the shutdown message
// from the "master" socket.  Other than that it just needs to
// re-transmit anything heard on one socket to all of the other
// connected sockets.
//
// Xastir itself will have to change a bit in order to add the
// togglebutton, to send anything transmit/received to the special
// socket, and to send the registration and shutdown strings to the
// server at the appropriate times.  Not earth-shaking changes, but
// changes nonetheless.
//
// Most of this code is adapted directly from W. Richard Steven's
// book:  "Unix Network Programming".  Highly recommended book, as
// are the other books by him that I own.  I was sorry to hear of
// his passing.
//
// Name for this?  Spider, centipede, millipede, octopus,
// multiplexer, repeater.


//#include "config.h"
#include "x_spider.h"
#include "snprintf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>

#include <termios.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>

#include <netinet/in.h>     // Moved ahead of inet.h as reports of some *BSD's not
                            // including this as they should.
#include <arpa/inet.h>
#include <netinet/tcp.h>    // Needed for TCP_NODELAY setsockopt() (disabling Nagle algorithm)

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif  // HAVE_NETDB_H

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else   // TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else  // HAVE_SYS_TIME_H
#  include <time.h>
# endif // HAVE_SYS_TIME_H
#endif  // TIME_WITH_SYS_TIME

#include <errno.h>

#ifdef  HAVE_LOCALE_H
#include <locale.h>
#endif  // HAVE_LOCALE_H

#ifdef  HAVE_LIBINTL_H
#include <libintl.h>
#define _(x)        gettext(x)
#else   // HAVE_LIBINTL_H
#define _(x)        (x)
#endif  // HAVE_LIBINTL_H


#ifndef SIGRET
#define SIGRET  void
#endif  // SIGRET

// Must be last include file
#include "leak_detection.h"



// Define this if you wish to use this as a standalone program
// instead of as a function in another program.
//
//#define STANDALONE_PROGRAM



// These are from util.h/util.c.  We can't include util.h here
// because it causes other problems.
extern short checkHash(char *theCall, short theHash);
extern void get_timestamp(char *timestring);
extern void split_string( char *data, char *cptr[], int max );

// From database.h
extern char my_callsign[];


typedef struct _pipe_object {
    int to_child[2];
    int to_parent[2];
    char callsign[20];
    int authenticated;
    struct _pipe_object *next;
} pipe_object;


pipe_object *pipe_head = NULL;
//int master_fd = -1; // Start with an invalid value

pipe_object *xastir_tcp_pipe = NULL;
pipe_object *xastir_udp_pipe = NULL;

// TCP server pipes to/from Xastir proper
int pipe_xastir_to_tcp_server = -1;
int pipe_tcp_server_to_xastir = -1;

// UDP server pipes to/from Xastir proper
int pipe_xastir_to_udp_server = -1; // (not currently used)
int pipe_udp_server_to_xastir = -1;





/*
// Read "n" bytes from a descriptor.  Use in place of read() when fd
// is a stream socket.  This routine is from "Unix Network
// Programming".
//
// This routine is not used currently.
//
int readn(register int fd, register char *ptr, register int nbytes) {
    int nleft, nread;

    nleft = nbytes;
    while (nleft > 0) {
        nread = read(fd, ptr, nleft);
        if (nread < 0) {
            return(nread);  // Error, return < 0
        }
        else if (nread == 0) {
            break;  // EOF
        }

        nleft -= nread;
        ptr += nread;
    }
    return(nbytes - nleft); // Return >= 0
}
*/





// Write "n" bytes to a descriptor.  Use in place of write() when fd
// is a stream socket.  This routine is from "Unix Network
// Programming".
//
int writen(register int fd, register char *ptr, register int nbytes) {
    int nleft, nwritten;

    nleft = nbytes;
    while (nleft > 0) {
        nwritten = write(fd,  ptr, nleft);
        if (nwritten <= 0) {
            return(nwritten);   // Error
        }

        nleft -= nwritten;
        ptr += nwritten;
    }

//    fprintf(stderr,
//        "writen: %d bytes written, %d bytes left to write\n",
//        nleft,
//        nbytes - nleft);

    return(nbytes - nleft);
}





// Read a line from a descriptor.  Read the line one byte at a time,
// looking for the newline.  We store the newline in the buffer,
// then follow it with a null (the same as fgets(3)).  We return the
// number of characters up to, but not including, the null (the same
// as strlen(3));  This routine is from "Unix Network Programming".
//
int readline(register int fd, register char *ptr, register int maxlen) {
    int n, rc;
    char c;

    for (n = 1; n < maxlen; n++) {
        if ( (rc = read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if (c == '\n') {
                break;
            }
        }
        else if (rc == 0) {
            if (n == 1) {
                return(0);  // EOF, no data read
            }
            else {
                break;      // EOF, some data was read
            }
        }
        else {
            return(-1);     // Error
        }
    }
    *ptr = 0;
    return(n);
}





#define MAXLINE 512


// Read a stream socket one line at a time, and write each line back
// to the sender.  Return when the connection is terminated.  This
// routine is from "Unix Network Programming".
//
/*
void str_echo(int sockfd) {
    int n;  
    char line[MAXLINE];

    for ( ; ; ) {
        n = readline(sockfd, line, MAXLINE);
        if (n == 0) {
            return; // Connection terminated
        }
        if (n < 0) {
            fprintf(stderr,"str_echo: No data to read\n");
        }

        if (writen(sockfd, line, n) != n) {
            fprintf(stderr,"str_echo: Writen error\n");
        }
    }
}
*/





// Read a stream socket one line at a time, and write each line back
// to the sender.  Return when the connection is terminated.  This
// routine is from "Unix Network Programming".
//
void str_echo2(int sockfd, int pipe_from_parent, int pipe_to_parent) {
    int n;  
    char line[MAXLINE];


    // Set socket to be non-blocking.
    //
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr,"str_echo2: Couldn't set socket non-blocking\n");
    }

    // Set read-end of pipe to be non-blocking.
    //
    if (fcntl(pipe_from_parent, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr,"str_echo2: Couldn't set read-end of pipe_from_parent non-blocking\n");
    }


    //Send our callsign to spider clients as "#callsign" much like APRS-IS sends "# javaAPRS"
    // # xastir 1.5.1 callsign:<mycall>
    sprintf(line,"# Welcome to Xastir's server port, callsign: %s\r\n",my_callsign);
    writen(sockfd,line,strlen(line));

    // Infinite loop
    for ( ; ; ) {

        //
        // Read data from socket, write to pipe (parent)
        //
        if (!sockfd)    // Socket is closed
            return; // Connection terminated
 
        n = readline(sockfd, line, MAXLINE);
        if (n == 0) {
            return; // Connection terminated
        }
        if (n < 0) {
            //fprintf(stderr,"str_echo2: Readline error: %d\n",errno);
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This is normal if we have no data to read
                //fprintf(stderr,"EAGAIN or EWOULDBLOCK\n");
            }
            else {  // Non-normal error.  Report it.
                fprintf(stderr,"str_echo2: Readline error socket: %d\n",errno);
//close(sockfd);
return;
            }
        }
        else {  // We received some data.  Send it down the pipe.
//            fprintf(stderr,"str_echo2: %s\n",line);
            if (writen(pipe_to_parent, line, n) != n) {
                fprintf(stderr,"str_echo2: Writen error socket: %d\n",errno);
//close(sockfd);
return;
            }
        }


        //
        // Read data from pipe (parent), write to socket
        //
        if (!pipe_from_parent)
            exit(0);
 
        n = readline(pipe_from_parent, line, MAXLINE);
        if (n == 0) {
            exit(0);    // Connection terminated
        }
        if (n < 0) {
            //fprintf(stderr,"str_echo2: Readline error: %d\n",errno);
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This is normal if we have no data to read
                //fprintf(stderr,"EAGAIN or EWOULDBLOCK\n");
            }
            else {  // Non-normal error.  Report it.
                fprintf(stderr,"str_echo2: Readline error pipe: %d\n",errno);
//close(pipe_from_parent);
return;
            }
        }
        else {  // We received some data.  Send it down the socket.
//            fprintf(stderr,"str_echo2: %s\n",line);

            if (writen(sockfd, line, n) != n) {
                fprintf(stderr,"str_echo2: Writen error pipe: %d\n",errno);
//close(pipe_from_parent);
return;
            }
        }

        // Slow the loop down to prevent excessive CPU.
        usleep(10000); // 10ms
    }
}





// Function which checks the incoming pipes to see if there's any
// data.  If there is, checks to see if it is a control packet or a
// registration packet from the master.  If not, echoes the data out
// all outgoing pipes.
// 
// If we get a shutdown from the verified master, send a "1" as the
// return value, which will shut down the server.  Otherwise send a
// "0" return value.
//
// Incoming registration data:  Record only the master socket.  All
// others should be handled by the child processes, and they should
// not pass the registration info down to us.  Same for control
// packets.  Actually, the child process handling the master socket
// could skip notifying us as well, and just pass down the control
// packets if the master sent any (like the shutdown packet).  If we
// lost the connection between Xastir and x_spider, we might not
// have a clean way of shutting down the server in that case though.
// Might be better to record it down here, and if the pipes ever
// closed, we shut down x_spider and all the child processes.
//
int pipe_check(char *client_address) {
    pipe_object *p = pipe_head;
    int n;  
    char line[MAXLINE];


    // Need a select here with a timeout?  Also need a method of
    // revising the read bits we send to select.  Should we revise
    // them every time through the loop, or set a variable in the
    // main() routine whenever a new connect comes in.  What about
    // connects that go away?  We need a way to free up the pipes
    // and sockets in that case, and revise the select bits again.

//    select();

//fprintf(stderr,"pipe_check()\n");

    // All of the read ends of the pipes have been set non-blocking
    // by this point.

    // Check all the pipes in the linked list looking for something
    // to read.
    while (p != NULL) {
//        fprintf(stderr,"Running through pipes\n");

        //
        // Read data from pipe, write to all pipes except the one
        // who sent it.
        //
        n = readline(p->to_parent[0], line, MAXLINE);
        if (n == 0) {
            char timestring[101];
            pipe_object *q = pipe_head;

            get_timestamp(timestring);

            if (p->authenticated) { 
                fprintf(stderr,
                    "%s X_spider session terminated, callsign: %s, address: %s\n",
                    timestring,
                    p->callsign,
                    client_address);
            }
            else {
                fprintf(stderr,
                    "%s X_spider session terminated, unauthenticated user, address %s\n",
                    timestring,
                    client_address);
            }

            // Close the pipe
            close(p->to_child[1]);
            close(p->to_parent[0]);

            // Unlink this record from our list
            if (q == p) {   // Beginning of list?
                pipe_head = q->next;
            }
            else {
                while (q->next != p && q != NULL)
                    q = q->next;    
                if (q != NULL)
                    q->next = p->next;
            }
            free(p);    // Free the malloc'd memory.

            wait(NULL); // Reap the status of the dead process
        }
        else if (n < 0) {
            //fprintf(stderr,"pipe_check: Readline error: %d\n",errno);
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // This is normal if we have no data to read
                //fprintf(stderr,"EAGAIN or EWOULDBLOCK\n");
            }
            else {  // Non-normal error.  Report it.
                fprintf(stderr,"pipe_check: Readline error: %d\n",errno);
            }
        }
        else {  // We received some data.  Send it down all of the
                // pipes except the one that sent it.

            pipe_object *q;


// Check for an authentication string.  If the pipe has not been
// authenticated, we don't allow it to send anything to the upstream
// server.  It's probably ok to send it to downstream connections
// though.

            // Check for "user" "pass" string.
            // "user WE7U-13 pass XXXX vers XASTIR 1.3.3"
            if (strstr(line,"user") && strstr(line,"pass")) {
                char line2[MAXLINE];
                char *callsign;
                char *passcode_str;
                short passcode;
                char *space;

                // We have a string with user/pass in it, but they
                // can be anywhere along the line.  We'll have to
                // search for each piece.

//fprintf(stderr,"x_spider:Found an authentication string\n");

                // Copy the line
                xastir_snprintf(line2, sizeof(line2), "%s", line);

                // Add white space to the end.
                strncat(line2,
                    "                                    ",
                    sizeof(line2) - strlen(line2));

                // Find the "user" string position
                callsign = strstr(line2,"user");

                if (callsign == NULL)
                    continue;

                // Fast-forward past the "user" word.
                callsign += 4;

                // Skip past any additional spaces that might be
                // present between "user" and callsign.
                while (callsign[0] == ' ' && callsign[0] != '\0') {
                    callsign += 1;
                }

                if (callsign[0] == '\0')
                    continue;

                // We should now be pointing at the beginning of the
                // callsign.

                // Find the space after the callsign
                space = strstr(callsign," ");

                if (space == NULL)
                    continue;

                // Terminate the callsign string
                space[0] = '\0';

                // Snag the passcode string 

                // Find the "pass" string
                passcode_str = strstr(&space[1],"pass");

                if (passcode_str == NULL)
                    continue;

                // Fast-forward past the "pass" word.
                passcode_str = passcode_str + 4;

                // Skip past any additional spaces that might be
                // present between "pass" and the passcode.
                while (passcode_str[0] == ' ' && passcode_str[0] != '\0') {
                    passcode_str += 1;
                }

                if (passcode_str[0] == '\0')
                    continue;

                // Find the space after the passcode
                space = strstr(&passcode_str[0]," ");

                if (space == NULL)
                    continue;

                // Terminate the passcode string
                space[0] = '\0';

                passcode = atoi(passcode_str);

//fprintf(stderr,"x_spider: user:.%s., pass:%d\n", callsign, passcode);

                if (checkHash(callsign, passcode)) {
                    // Authenticate the pipe.  It is now allowed to send
                    // to the upstream server.
                    //fprintf(stderr,
                    //    "x_spider: Authenticated user %s\n",
                    //    callsign);
                    p->authenticated = 1;
                    xastir_snprintf(p->callsign,
                        sizeof(p->callsign),
                        "%s",
                        callsign);
                    p->callsign[19] = '\0';
                }
                else {
                    fprintf(stderr,
                        "X_spider: Bad authentication, user %s, pass %d\n",
                        callsign,
                        passcode);
                    fprintf(stderr,
                        "Line: %s\n",
                        line);
                }
            }

            q = pipe_head;

            while (q != NULL) {
//                fprintf(stderr,"pipe_check: %s\n",line);

                if (q != p) {
                    if (writen(q->to_child[1], line, n) != n) {
                        fprintf(stderr,"pipe_check: Writen error1: %d\n",errno);
                    }
                }
                q = q->next;
            }

            // Here we send it to Xastir itself.  We use a couple of
            // global variables just like channel_data() does, but
            // we don't have to protect them in the same manner as
            // we only have one process on each end.
            //

// Send it down the pipe to Xastir's main thread.  Knock off any
// carriage return that might be present.  We only want a linefeed
// on the end.
            if (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) {
                line[n-1] = '\0';
                n--;
            }
            if (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) {
                line[n-1] = '\0';
                n--;
            }
            // Add the linefeed on the end
            strncat(line,"\n",1);
            n++;

// Only send to upstream server if this client has authenticated.
            if (p->authenticated) {

//fprintf(stderr,"Data available, sending to server\n");
//fprintf(stderr,"\t%s\n",line);

                if (writen(pipe_tcp_server_to_xastir, line, n) != n) {
                    fprintf(stderr, "pipe_check: Writen error2: %d\n", errno);
                }
            }
        }

        p = p->next;
    }


    // Check the pipe from Xastir's main thread to see if it is
    // sending us any data
    n = readline(pipe_xastir_to_tcp_server, line, MAXLINE);
    if (n == 0) {
        exit(0); // Connection terminated
    }
    if (n < 0) {
        //fprintf(stderr,"pipe_check: Readline error: %d\n",errno);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // This is normal if we have no data to read
            //fprintf(stderr,"EAGAIN or EWOULDBLOCK\n");
        }
        else {  // Non-normal error.  Report it.
            fprintf(stderr,"pipe_check: Readline error: %d\n",errno);
        }
    }
    else {  // We received some data.  Send it down all of the
            // pipes.
// Also send it down the socket.
        pipe_object *q = pipe_head;

        // The internet protocol for sending lines is "\r\n".  Knock
        // off any line-end characters that might be present, then
        // add a "\r\n" combo on the end.
        //
        if (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) {
            line[n-1] = '\0';
            n--;
        }
        if (n > 0 && (line[n-1] == '\r' || line[n-1] == '\n')) {
            line[n-1] = '\0';
            n--;
        }
        // Add carriage-return/linefeed onto the end
        strncat(line, "\r\n", 2);
        n = n+2;

        while (q != NULL) {
//          fprintf(stderr,"pipe_check: %s\n",line);

            if (writen(q->to_child[1], line, n) != n) {
                fprintf(stderr,"pipe_check: Writen error1: %d\n",errno);
            }
            q = q->next;
        }
    }

    return(0);
}





// The below two functions init_set_proc_title() and
// set_proc_title() are from:
// http://lightconsulting.com/~thalakan/process-title-notes.html
// They seems to work fine on Linux, but they only change the "ps"
// listings, not the top listings.  I don't know why yet.

// Here's another good web page for Linux:
// http://www.uwsg.iu.edu/hypermail/linux/kernel/0006.1/0610.html 

/* Globals */
static char **Argv = ((void *)0);
#ifdef __linux__
extern char *__progname, *__progname_full;
#endif
static char *LastArgv = ((void *)0);



void init_set_proc_title(int argc, char *argv[], char *envp[]) {
    int i, envpsize;
    extern char **environ;
    char **p;

    for(i = envpsize = 0; envp[i] != NULL; i++)
        envpsize += strlen(envp[i]) + 1;
  
    if((p = (char **) malloc((i + 1) * sizeof(char *))) != NULL ) {
        environ = p;

    for(i = 0; envp[i] != NULL; i++) {
        if((environ[i] = malloc(strlen(envp[i]) + 1)) != NULL)
            xastir_snprintf(environ[i],
                sizeof(environ[i]),
                "%s",
                envp[i]);
        }
    
        environ[i] = NULL;
    }

    Argv = argv;
  
    for(i = 0; envp[i] != NULL; i++) {
        if((LastArgv + 1) == envp[i]) // Not sure if this conditional is needed
        LastArgv = envp[i] + strlen(envp[i]);
    }
#ifdef __linux__
    // Pretty sure you don't need this either
    __progname = strdup("xastir");
    __progname_full = strdup(argv[0]);
#endif
}





void set_proc_title(char *fmt,...) {
    va_list msg;
    static char statbuf[8192];
    char *p;
    int i,maxlen = (LastArgv - Argv[0]) - 2;

    //fprintf(stderr,"DEBUG: maxlen: %i\n", maxlen);

    va_start(msg,fmt);

    memset(statbuf, 0, sizeof(statbuf));
    xastir_vsnprintf(statbuf, sizeof(statbuf), fmt, msg);

    va_end(msg);

    i = strlen(statbuf);

    xastir_snprintf(Argv[0], maxlen, "%s", statbuf);
    p = &Argv[0][i];
  
    while(p < LastArgv)
        *p++ = '\0';
    Argv[1] = ((void *)0) ;
}





// This TCP server provides a listening socket.  When a client
// connects, the server forks off a separate process to handle it
// and goes back to listening for new connects.  The initial code
// framework here is from the book:  "Unix Network Programming".
//
// Create two pipes between this server and each child process.
// Identify and record which socket is the master socket connection
// (back to the Xastir session that started up x_spider).  Set this
// variable once, don't change it if another client connects and
// claims to be the master.  If we get control commands from the
// master, service them.
//
// If anything that comes in from a client that's not a registration
// or a control packet, repeat it to all of the other connected
// clients, including sending it to the controlling Xastir socket
// (which is also a data channel).
//
// We need to make the "accept" call non-blocking so that we can
// keep servicing all of the pipes from the children.  If any pipe
// has data, check whether it's a registration from the master or a
// control packet.  If not, send the data out each client
// connection.  Each child will take care of normal APRS
// authentication.  If the client doesn't authenticate, close the
// socket and exit from the child process.  Notify the main process
// as well?
//
#ifdef STANDALONE_PROGRAM
int main(int argc, char *argv[]) {
#else   // !STANDALONE_PROGRAM 
void TCP_Server(int argc, char *argv[], char *envp[]) {
#endif  // STANDALONE_PROGRAM

    int sockfd, newsockfd, childpid;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;
    pipe_object *p;
    int sendbuff;
    int pipe_to_parent; /* symbolic names to reduce confusion */
    int pipe_from_parent;
    char timestring[101];

    
    // Open a TCP listening socket
    //
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr,"x_spider: Can't open socket for listening\n");
        exit(1);
    }

    // Set the new socket to be non-blocking.
    //
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr,"x_spider: Couldn't set socket non-blocking\n");
    }

    // Set up to reuse the port number (good for debug so we can
    // restart the server quickly against the same port).
    //
    sendbuff = 1;
    if (setsockopt(sockfd,
            SOL_SOCKET,
            SO_REUSEADDR,
            (char *)&sendbuff,
            sizeof(sendbuff)) < 0) {
        fprintf(stderr,"x_spider: Couldn't set socket REUSEADDR\n");
    }

    // Bind our local address so that the client can send to us.
    //
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_TCP_PORT);

    if (bind(sockfd,
            (struct sockaddr *)&serv_addr,
            sizeof(serv_addr)) < 0) {
        fprintf(stderr,"x_spider: Can't bind local address\n");
        exit(1);
    }

    // Set up to listen.  We allow up to five backlog connections
    // (unserviced connects that get put on a queue until we can
    // service them).
    //
    listen(sockfd, 5);

    // Infinite loop
    //
    for ( ; ; ) {
        int flag;


        // Look for a connection from a client process.  This is a
        // concurrent server (allows multiple concurrent
        // connections).

        clilen = (socklen_t)sizeof(cli_addr);

        // "accept" is the call where we wait for a connection.  We
        // made the socket non-blocking above so that we pop out of
        // it with an EAGAIN if we don't have an incoming socket
        // connection.  This lets us check all of our pipe
        // connections for incoming data periodically.
        //
        newsockfd = accept(sockfd,
                        (struct sockaddr *)&cli_addr,
                        &clilen);

        if (newsockfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {

                // We returned from the non-blocking accept but with
                // no incoming socket connection.  Check the pipe
                // queues for incoming data.
                //
                if (pipe_check(inet_ntoa(cli_addr.sin_addr)) == -1) {

                    // We received a shutdown command from the
                    // master socket connection.
                    exit(0);
                }
                goto finis;
            }
            else if (newsockfd < 0) {

                // Some error happened in accept().  Skip the rest
                // of the loop.
                //
                fprintf(stderr,"x_spider: Accept error: %d\n", errno);
                goto finis;
            }
        }

        // Else we returned from the accept with an incoming
        // connection.  Service it.
        //
        // Allocate a new pipe before we fork.
        //
        p = (pipe_object *)malloc(sizeof(pipe_object));
        if (p == NULL) {
            fprintf(stderr,"x_spider: Couldn't malloc pipe_object\n");
            close(newsockfd);
            goto finis;
        }

        // We haven't authenticated this user client yet.
        p->authenticated = 0;
        p->callsign[0] = '\0';

        get_timestamp(timestring);

        fprintf(stderr,"%s X_spider client connected from address %s\n",
            timestring,
            inet_ntoa(cli_addr.sin_addr));
 
        // Link it into the head of the chain.
        //
        p->next = pipe_head;
        pipe_head = p;

        if (pipe(p->to_child) < 0 || pipe(p->to_parent) < 0) {
            fprintf(stderr,"x_spider: Can't create pipes\n");
            free(p);    // Free the malloc'd memory.
            close(newsockfd);
            goto finis;
        }

        flag = 1;

        // Turn on the socket keepalive option
        (void)setsockopt(newsockfd,  SOL_SOCKET, SO_KEEPALIVE, (char *) &flag, sizeof(int));

        // Disable the Nagle algorithm (speeds things up)
        (void)setsockopt(newsockfd, IPPROTO_TCP,  TCP_NODELAY, (char *) &flag, sizeof(int));

        if ( (childpid = fork()) < 0) {
            //
            // Problem forking.  Clean up and continue loop.
            //

            fprintf(stderr,"x_spider: Fork error\n");
            // Close pipes
            close(p->to_child[0]);
            close(p->to_child[1]);
            close(p->to_parent[0]);
            close(p->to_parent[1]);
            free(p);    // Free the malloc'd memory.
            close(newsockfd);
            goto finis;
        }
        else if (childpid == 0) {
            //
            // child process
            //


            // Go back to default signal handler instead of calling
            // restart() on SIGHUP
            (void) signal(SIGHUP,SIG_DFL);


/*
            fprintf(stderr,
                "Client address: %s\n",
                inet_ntoa(cli_addr.sin_addr));
*/

            // Change the name of the new child process.  So far
            // this only works for "ps" listings, not for "top".
            // This code only works on Linux.  For BSD use
            // setproctitle(3), NetBSD can use setprogname(2).
#ifdef __linux__
            init_set_proc_title(argc, argv, envp);
            set_proc_title("%s%s %s",
                "x-spider client @",
                inet_ntoa(cli_addr.sin_addr),
                "(xastir)");
            //fprintf(stderr,"DEBUG: %s\n", Argv[0]);
#endif  // __linux__

// It'd be very cool here to include the IP address of the remote
// client on the "ps" line, and include the callsign of the
// connecting client once the client authenticates.  Both of these
// are do-able.


            // New naming system so that we don't have to remember
            // the longer name:
            //
            pipe_to_parent = p->to_parent[1];
            pipe_from_parent = p->to_child[0];

            close(sockfd);      // Close original socket.  Child
                                // doesn't need the listening
                                // socket.
            close(p->to_child[1]);  // Close write end of pipe
            close(p->to_parent[0]); // Close read end of pipe

//            str_echo(newsockfd);    // Process the request
            str_echo2(newsockfd,
                pipe_from_parent,
                pipe_to_parent);
 

            // Clean up and exit
            //
            close(pipe_to_parent);
            close(pipe_from_parent);
            exit(0);

        }
        //
        // Parent process
        //

        close(newsockfd);
        close(p->to_parent[1]); // Close write end of pipe
        close(p->to_child[0]);  // Close read end of pipe

        // Set read-end of pipe to be non-blocking.
        //
        if (fcntl(p->to_parent[0], F_SETFL, O_NONBLOCK) < 0) {
            fprintf(stderr,"x_spider: Couldn't set read-end of pipe_to_parent non-blocking\n");
        }

finis:
        // Need a delay so that we don't use too much CPU, at least
        // for debug.  Put the delay into the select() call in the
        // pipe_check() function once we get to that stage of
        // coding.
        //
        usleep(10000); // 10ms
    }
}





// Send a nack back to the xastir_udp_client program 
void send_udp_nack(int sock, struct sockaddr_in from, int fromlen) {
    int n;

    n = sendto(sock,
        "NACK", // Negative Acknowledgment
        5,
        0,
        (struct sockaddr *)&from,
        fromlen);
    if (n < 0) {
        fprintf(stderr, "Error: sendto");
    }
}
 




// Create a UDP listening port.  This allows scripts and other
// programs to inject packets into Xastir via UDP protocol.
//
void UDP_Server(int argc, char *argv[], char *envp[]) {
    int sock, length, fromlen, n1, n2;
    struct sockaddr_in server;
    struct sockaddr_in from;
    char buf[1024];
    char buf2[512];
    char *callsign;
    short passcode;
    char *cptr[10];
    char *message = NULL;
    char message2[1024];
    int send_to_inet;
    int send_to_rf;


    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        fprintf(stderr, "Error: Opening socket");
        return;
    }

    length = sizeof(server);
    memset(&server, 0, length);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(SERV_UDP_PORT);

    if (bind(sock, (struct sockaddr *)&server, length) < 0) {
        fprintf(stderr, "Error: Binding");
        return;
    }

    fromlen = sizeof(struct sockaddr_in);

    while (1) {
        n1 = recvfrom(sock,
            buf,
            1024,
            0,
            (struct sockaddr *)&from, &fromlen);
        if (n1 < 0) {
            fprintf(stderr, "Error: recvfrom");
        }
        else if (n1 == 0) {
            continue;
        }
        else {
            buf[n1] = '\0';    // Terminate the buffer
        }

fprintf(stderr, "Received datagram: %s", buf);


        send_to_inet = 0;
        send_to_rf = 0;


        //
        // Authenticate the packet.  First line should contain:
        //
        //      callsign,passcode,[TO_RF],[TO_INET]
        //
        // The second line should contain the APRS packet
        //

        // Copy the entire buffer so that we can modify it
        xastir_snprintf(buf2, sizeof(buf2), "%s", buf);
        split_string(buf2, cptr, 10);

        if (cptr[0] == NULL || cptr[0][0] == '\0') {    // callsign
            send_udp_nack(sock, from, fromlen);
            continue;
        }

        callsign = cptr[0];

        if (cptr[1] == NULL || cptr[1][0] == '\0') {    // passcode
            send_udp_nack(sock, from, fromlen);
            continue;
        }

        passcode = atoi(cptr[1]);

fprintf(stderr,"x_spider udp:  user:%s  pass:%d\n", callsign, passcode);

        if (checkHash(callsign, passcode)) {
            // Authenticate the pipe.  It is now allowed to send
            // to the upstream server.
            //fprintf(stderr,
            //    "x_spider: Authenticated user %s\n",
            //    callsign);
        }
        else {
            fprintf(stderr,
                "X_spider: Bad authentication, user %s, pass %d\n",
                callsign,
                passcode);
            fprintf(stderr,
                "UDP Packet: %s\n",
                buf);
            send_udp_nack(sock, from, fromlen);
            continue;
        }


        // Here's where we would look for the optional flags in the
        // first line.  Here we implement these flags:
        //      -identify
        //      -to_rf
        //      -to_inet


        // Look for the "-identify" flag in the UDP packet
        //
        if (strstr(buf, "-identify")) {
            
            // Send the callsign back to the xastir_udp_client
            // program
            n1 = sendto(sock,
                my_callsign,
                strlen(my_callsign)+1,
                0,
                (struct sockaddr *)&from,
                fromlen);
            if (n1 < 0) {
                fprintf(stderr, "Error: sendto");
            }
            continue;
        }


        // Look for the "-to_inet" flag in the UDP packet
        //
        if (strstr(buf, "-to_inet")) {
//fprintf(stderr,"Sending to INET\n");
            send_to_inet++;
        }


        // Look for the "-to_rf" flag in the UDP packet
        //
        if (strstr(buf, "-to_rf")) {
//fprintf(stderr,"Sending to local RF\n");
            send_to_rf++;
        }


        // Now snag the text message from the second line using the
        // original buffer.  Look for the first '\n' character which
        // is just before the text message itself.
        message = strchr(buf, '\n');
        message++;  // Point to the first char after the '\n'

        if (message == NULL || message[0] == '\0') {
//fprintf(stderr,"Empty message field\n");
            send_udp_nack(sock, from, fromlen);
            continue;
        }

//fprintf(stderr,"Message:  %s", message);

        xastir_snprintf(message2,
            sizeof(message2),
            "%s%s%s",
            (send_to_inet) ? "TO_INET," : "",
            (send_to_rf) ? "TO_RF," : "",
            message);

//fprintf(stderr,"Message2: %s", message2);



//
//
// NOTE:
// Should we refuse to send the message on if "callsign" and the
// FROM callsign in the packet don't match?
//
// Should we change to third-party format if "my_callsign" and the
// FROM callsign in the packet don't match?
//
// Require all three callsigns to match?
//
//



        n1 = strlen(message);
        n2 = strlen(message2);


        // Send to Xastir udp pipe
        //
//fprintf(stderr,"Sending to Xastir itself\n");
        if (writen(pipe_udp_server_to_xastir, message2, n2) != n2) {
            fprintf(stderr,"UDP_Server: Writen error1: %d\n", errno);
        }

        // Send to the x_spider TCP server, so it can go to all
        // connected TCP clients
//fprintf(stderr,"Sending to TCP clients\n");
        if (writen(pipe_xastir_to_tcp_server, message, n1) != n1) {
            fprintf(stderr, "UDP_Server: Writen error2: %d\n", errno);
        }
 
        // Send an ACK back to the xastir_udp_client program 
        n1 = sendto(sock,
            "ACK",  // Acknowledgment.  Good UDP packet.
            4,
            0,
            (struct sockaddr *)&from,
            fromlen);
        if (n1 < 0) {
            fprintf(stderr, "Error: sendto");
        }
    }
}





// Function used to start a separate process for the server.  This
// way the server can be running concurrently with the main part of
// Xastir.
//
// Turns out that with a "fork", the memory image of the server was
// too large.  Might try it with a thread instead before abandoning
// that method altogether.  It would be nice to have this be more
// integrated with Xastir, instead of having to have a socket to
// communicate between Xastir and the server.
//
#ifndef STANDALONE_PROGRAM
int Fork_TCP_server(int argc, char *argv[], char *envp[]) {
    int childpid;


    // Allocate a pipe before we fork.
    //
    xastir_tcp_pipe = (pipe_object *)malloc(sizeof(pipe_object));
    if (xastir_tcp_pipe == NULL) {
        fprintf(stderr,"x_spider: Couldn't malloc pipe_object\n");
        return(0);
    }

    if (pipe(xastir_tcp_pipe->to_child) < 0 || pipe(xastir_tcp_pipe->to_parent) < 0) {
        fprintf(stderr,"x_spider: Can't create pipes\n");
        free(xastir_tcp_pipe);    // Free the malloc'd memory.
        return(0);
    }
 
    xastir_tcp_pipe->authenticated = 1;
    xastir_tcp_pipe->callsign[0] = '\0';
 
    if ( (childpid = fork()) < 0) {
        fprintf(stderr,"Fork_TCP_server: Fork error\n");

        // Close pipes
        close(xastir_tcp_pipe->to_child[0]);
        close(xastir_tcp_pipe->to_child[1]);
        close(xastir_tcp_pipe->to_parent[0]);
        close(xastir_tcp_pipe->to_parent[1]);
        free(xastir_tcp_pipe);    // Free the malloc'd memory.
        return(0);
    }
    else if (childpid == 0) {
        //
        // Child process
        //


        // Go back to default signal handler instead of calling
        // restart() on SIGHUP
        (void) signal(SIGHUP,SIG_DFL);


        // Change the name of the new child process.  So far this
        // only works for "ps" listings, not for "top".  This code
        // only works on Linux.  For BSD use setproctitle(3), NetBSD
        // can use setprogname(2).
#ifdef __linux__
        init_set_proc_title(argc, argv, envp);
        set_proc_title("%s", "x-spider TCP daemon (xastir)");
        //fprintf(stderr,"DEBUG: %s\n", Argv[0]);
#endif  // __linux__
 

        close(xastir_tcp_pipe->to_child[1]);  // Close write end of pipe
        close(xastir_tcp_pipe->to_parent[0]); // Close read end of pipe

        // Assign the global variables
        pipe_tcp_server_to_xastir = xastir_tcp_pipe->to_parent[1];
        pipe_xastir_to_tcp_server = xastir_tcp_pipe->to_child[0];

        // Set read-end of pipe to be non-blocking.
        //
        if (fcntl(pipe_xastir_to_tcp_server, F_SETFL, O_NONBLOCK) < 0) {
            fprintf(stderr,"x_spider: Couldn't set read-end of pipe_xastir_to_tcp_server non-blocking\n");
        }

        // Go into an infinite loop here which restarts the
        // listening process whenever it dies.
        //
//        while (1) {
//            fprintf(stderr,"Starting TCP_Server...\n");

            TCP_Server(argc, argv, envp);
 
//            fprintf(stderr,"TCP_Server process died.\n");
//        }
    }
    //
    // Parent process
    //

    close(xastir_tcp_pipe->to_parent[1]); // Close write end of pipe
    close(xastir_tcp_pipe->to_child[0]);  // Close read end of pipe

    // Assign the global variables so that Xastir itself will know
    // how to talk to the pipes
    pipe_tcp_server_to_xastir = xastir_tcp_pipe->to_parent[0];
    pipe_xastir_to_tcp_server = xastir_tcp_pipe->to_child[1];

    // Set read-end of pipe to be non-blocking.
    //
    if (fcntl(pipe_tcp_server_to_xastir, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr,"x_spider: Couldn't set read-end of pipe_tcp_server_to_xastir non-blocking\n");
    }

//    // Set write-end of pipe to be non-blocking.
//    //
//    if (fcntl(pipe_xastir_to_tcp_server, F_SETFL, O_NONBLOCK) < 0) {
//        fprintf(stderr,"x_spider: Couldn't set read-end of pipe_xastir_to_tcp_server non-blocking\n");
//    }

    // We don't need to do anything here except return back to the
    // calling routine with the PID of the new server process, so
    // that it can request the server and all it's children to quit
    // when Xastir quits or segfaults.
    return(childpid);   // Really the parent PID in this case
}
#endif  // STANDALONE_PROGRAM





int Fork_UDP_server(int argc, char *argv[], char *envp[]) {
    int childpid;


    // Allocate a pipe before we fork.
    //
    xastir_udp_pipe = (pipe_object *)malloc(sizeof(pipe_object));
    if (xastir_udp_pipe == NULL) {
        fprintf(stderr,"x_spider: Couldn't malloc pipe_object\n");
        return(0);
    }

    if (pipe(xastir_udp_pipe->to_child) < 0 || pipe(xastir_udp_pipe->to_parent) < 0) {
        fprintf(stderr,"x_spider: Can't create pipes\n");
        free(xastir_udp_pipe);    // Free the malloc'd memory.
        return(0);
    }
 
    xastir_udp_pipe->authenticated = 1;
    xastir_udp_pipe->callsign[0] = '\0';
 
    if ( (childpid = fork()) < 0) {
        fprintf(stderr,"Fork_UDP_server: Fork error\n");

        // Close pipes
        close(xastir_udp_pipe->to_child[0]);
        close(xastir_udp_pipe->to_child[1]);
        close(xastir_udp_pipe->to_parent[0]);
        close(xastir_udp_pipe->to_parent[1]);
        free(xastir_udp_pipe);    // Free the malloc'd memory.
        return(0);
    }
    else if (childpid == 0) {
        //
        // Child process
        //


        // Go back to default signal handler instead of calling
        // restart() on SIGHUP
        (void) signal(SIGHUP,SIG_DFL);


        // Change the name of the new child process.  So far this
        // only works for "ps" listings, not for "top".  This code
        // only works on Linux.  For BSD use setproctitle(3), NetBSD
        // can use setprogname(2).
#ifdef __linux__
        init_set_proc_title(argc, argv, envp);
        set_proc_title("%s", "x-spider UDP daemon (xastir)");
        //fprintf(stderr,"DEBUG: %s\n", Argv[0]);
#endif  // __linux__
 

        close(xastir_udp_pipe->to_child[1]);  // Close write end of pipe
        close(xastir_udp_pipe->to_parent[0]); // Close read end of pipe

        // Assign the global variables
        pipe_udp_server_to_xastir = xastir_udp_pipe->to_parent[1];
        pipe_xastir_to_udp_server = xastir_udp_pipe->to_child[0];

        // Set read-end of pipe to be non-blocking.
        //
//        if (fcntl(pipe_xastir_to_udp_server, F_SETFL, O_NONBLOCK) < 0) {
//            fprintf(stderr,
//                "x_spider: Couldn't set read-end of pipe_xastir_to_udp_server non-blocking\n");
//        }

        // Go into an infinite loop here which restarts the
        // listening process whenever it dies.
        //
//        while (1) {
//            fprintf(stderr,"Starting UDP_Server...\n");

            UDP_Server(argc, argv, envp);
 
            fprintf(stderr,"UDP_Server process died.\n");
//        }
    }
    //
    // Parent process
    //

    close(xastir_udp_pipe->to_parent[1]); // Close write end of pipe
    close(xastir_udp_pipe->to_child[0]);  // Close read end of pipe

    // Assign the global variables so that Xastir itself will know
    // how to talk to the pipes
    pipe_udp_server_to_xastir = xastir_udp_pipe->to_parent[0];
    pipe_xastir_to_udp_server = xastir_udp_pipe->to_child[1];


    // Set read-end of pipe to be non-blocking.
    //
    if (fcntl(pipe_udp_server_to_xastir, F_SETFL, O_NONBLOCK) < 0) {
        fprintf(stderr,"x_spider: Couldn't set read-end of pipe_udp_server_to_xastir non-blocking\n");
    }

//    // Set write-end of pipe to be non-blocking.
//    //
//    if (fcntl(pipe_xastir_to_udp_server, F_SETFL, O_NONBLOCK) < 0) {
//        fprintf(stderr,"x_spider: Couldn't set read-end of pipe_xastir_to_udp_server non-blocking\n");
//    }


    // We don't need to do anything here except return back to the
    // calling routine with the PID of the new server process, so
    // that it can request the server and all it's children to quit
    // when Xastir quits or segfaults.
    return(childpid);   // Really the parent PID in this case
}

 
