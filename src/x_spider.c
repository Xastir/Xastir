/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003  The Xastir Group
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
//   Starts up the server.
//   Connects to it with a socket.
//   Sends a special string so the server knows which one is the
//     controlling (master) socket.
//   All packets received/transmitted by Xastir also get sent to the
//     server socket if enabled.
//
// x_spider:
//   Accept client socket connections.
//   Spawn a new process for each one.
//   Each process talks to the listening server using two pipes.
//   Authenticate each client in the normal manner.
//   Accept data from any socket, send the data out all ports.
//   If the "master" Xastir sends a shutdown packet, all connections
//     will be dropped and the server will exit.
//   x_spider uses select() calls to multiplex multiple pipes,
//     listening socket, and socket to the "master" Xastir.
//   
// This makes the design of the server rather simple:  It needs to
// authenticate clients and it needs to parse the shutdown message
// from the "master" socket.  Other than that it just needs to
// re-transmit anything heard one one socket out to all of the other
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
#include <pthread.h>
#include <termios.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <netinet/in.h>     // Moved ahead of inet.h as reports of some *BSD's not
                            // including this as they should.
#include <arpa/inet.h>
#include <netinet/tcp.h>    // Needed for TCP_NODELAY setsockopt() (disabling Nagle algorithm)

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif  // HAVE_NETDB_H

#include <sys/types.h>
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

//#include <Xm/XmAll.h>
//#include <X11/cursorfont.h>

//#include "xastir.h"

#ifndef SIGRET
#define SIGRET  void
#endif  // SIGRET





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





// Write "n" bytes to a descriptor.  Use in place of write() when fd
// is a stream socket.  This routine is from rom "Unix Network
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
void str_echo(int sockfd) {
    int n;  
    char line[MAXLINE];

    for ( ; ; ) {
        n = readline(sockfd, line, MAXLINE);
        if (n == 0) {
            return; // Connection terminated
        }
        else if (n < 0) {
            fprintf(stderr,"str_echo: Readline error");
        }

        if (writen(sockfd, line, n) != n) {
            fprintf(stderr,"str_echo: Writen error");
        }
    }
}





typedef struct _pipe_object {
    int to_child[2];
    int to_parent[2];
    struct _pipe_object *next;
} pipe_object;





// This TCP server provides a listening socket.  When a client
// connects, the server forks off a separate process to handle it
// and goes back to listening for new connects.  The basic concept
// here is from "Unix Network Programming".
//
// We need to create two pipes between this server and all of its
// forked children.  We also need to identify which one is the
// master socket connection (back to the Xastir that started up
// x_spider), keep track of that, and not reset it if another client
// comes online and claims to be the master.  If we get control
// commands from the master, service them.
//
// Anything that comes in from a client that's not a registration or
// a control packet, repeat it to all of the other connected
// clients, including the control channel.
//

int main(int argc, char *argv[]) {
    int sockfd, newsockfd, clilen, childpid;
    struct sockaddr_in cli_addr, serv_addr;
//    int masterfd = -1;
    pipe_object *pipe_head = NULL;
    pipe_object *p;

    
    // Open a TCP listening socket
    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr,"x_spider: Can't open socket for listening.");
    }

    // Bind our local address so that the client can send to us.
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(SERV_TCP_PORT);

    if (bind(sockfd,
            (struct sockaddr *)&serv_addr,
            sizeof(serv_addr)) < 0) {
        fprintf(stderr,"x_spider: Can't bind local address.");
    }

    listen(sockfd, 5);

    // Infinite loop
    for ( ; ; ) {

// We need to make the "accept" call non-blocking so that we can
// keep servicing all of the pipes from the children.  If any pipe
// has data, check whether it's a registration from the master or a
// control packet.  If not, send the data out each client
// connection.  Each child will take care of normal APRS
// authentication.  If the client doesn't authenticate, close the
// socket and exit from the child process.  Notify the main process
// as well?

        // Wait for a connection from a client process.  This is a
        // concurrent server (allows multiple concurrent
        // connections).

        clilen = sizeof(cli_addr);

        newsockfd = accept(sockfd,
                        (struct sockaddr *)&cli_addr,
                        &clilen);

        if (newsockfd < 0) {
            fprintf(stderr,"x_spider: Accept error.");
        }

        // Allocate a new pipe before we fork.
        p = (pipe_object *)malloc(sizeof(pipe_object));
        if (p == NULL) {
            fprintf(stderr,"x_spider: Couldn't malloc pipe_object\n");
        }

        // Link it into the head of the chain.
        p->next = pipe_head;
        pipe_head = p;

        if (pipe(p->to_child) < 0 || pipe(p->to_parent) < 0) {
            fprintf(stderr,"x_spider: Can't create pipes\n");
        }
        
        if ( (childpid = fork()) < 0) {
            fprintf(stderr,"x_spider: Fork error.");
        }
        else if (childpid == 0) {
            //
            // child process
            //
            // New naming system so that we don't have to remember
            // the longer name:
            int pipe_to_parent = p->to_parent[1];
            int pipe_from_parent = p->to_child[0];

            close(sockfd);      // Close original socket.  Child
                                // doesn't need the listening
                                // socket.
            close(p->to_child[1]);  // Close write end of pipe
            close(p->to_parent[0]); // Close read end of pipe

str_echo(newsockfd);    // Process the request

            // Clean up and exit
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
/*
void Fork_server(void) {
    int childpid;

 
    if ( (childpid = fork()) < 0) {
        fprintf(stderr,"Fork_server: Fork error.");
    }
    else if (childpid == 0) {
        //
        // Child process
        //

        // Go into an infinite loop here which restarts the
        // listening process whenever it dies.
        //
        while (1) {
            fprintf(stderr,"Starting Server...\n");
            Server();
            fprintf(stderr,"Server process died.\n");
        }
    }
    //
    // Parent process
    //

    // We don't need to do anything here except return back to the
    // calling routine.
}
*/


