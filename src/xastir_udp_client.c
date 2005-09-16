/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2003-2005  The Xastir Group
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



#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <string.h>

#include <netinet/in.h>     // Moved ahead of inet.h as reports of some *BSD's not
                            // including this as they should.
#include <arpa/inet.h>
//#include <netinet/tcp.h>    // Needed for TCP_NODELAY setsockopt() (disabling Nagle algorithm)

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif  // HAVE_NETDB_H

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>


// Must be last include file
#include "leak_detection.h"





// Send a UDP packet to a UDP listening port.  This allows scripts
// and other programs to inject packets into Xastir via UDP
// protocol.
// Inputs:
//      hostname    (argv[1])
//      port        (argv[2])
//      callsign    (argv[3])
//      passcode    (argv[4])
//      optional flags:  -identify -to_rf -to_inet (not implemented yet)
//      message     (argv[5])
// Returns:
//      0: Message sent, ack received
//      1: Error condition
//
int main(int argc, char *argv[]) {
    int sockfd, length, n;
    struct sockaddr_in server, from;
    struct hostent *hostinfo;
    char buffer[512];
    char callsign[10];
    char extra[100];
    int passcode;
    char message[256];
    int ii;


    if (argc < 6) {
        fprintf(stderr,
            "Usage: server port call passcode [-identify] \"message\"\n");
        fprintf(stderr,
            "Example: xastir_udp_client localhost 2023 ab7cd 1234 \"APRS packet\"\n");
        return(1);
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "socket\n");
        return(1);
    }

    memset(&server, 0, sizeof(struct sockaddr_in));

    server.sin_family = AF_INET;

    hostinfo = gethostbyname(argv[1]);
    if (hostinfo == NULL) {
        fprintf(stderr, "Unknown host\n");
        return(1);
    }

//    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    memcpy(&server.sin_addr, hostinfo->h_addr, hostinfo->h_length);

    server.sin_port = htons(atoi(argv[2]));

    length = sizeof(struct sockaddr_in);

    // Fetch the callsign
    snprintf(callsign, sizeof(callsign), "%s", argv[3]);
    callsign[sizeof(callsign)-1] = '\0';  // Terminate it

    // Fetch the passcode
    passcode = atoi(argv[4]);

    // Check for optional flags here:
    //      -identify
    //      -to_rf
    //      -to_inet
    //
    extra[0] = '\0';
    for (ii = 5; ii < argc; ii++) {
        if (strstr(argv[ii], "-identify")) {
//fprintf(stderr,"Found -identify\n");
            strncat(extra, ",-identify", 10);
        }
        else if (strstr(argv[ii], "-to_rf")) {
//fprintf(stderr,"Found -to_rf\n");
            strncat(extra, ",-to_rf", 7);
        }
        else if (strstr(argv[ii], "-to_inet")) {
//fprintf(stderr,"Found -to_inet\n");
            strncat(extra, ",-to_inet", 9);
        }
    }


//    fprintf(stdout, "Please enter the message: ");

    // Fetch message portion from the end of the command-line
    snprintf(message, sizeof(message), "%s", argv[argc-1]);
    message[sizeof(message)-1] = '\0';  // Terminate it

    if (message[0] == '\0') // Empty message
        return(1);

    memset(buffer, 0, 256);
//    fgets(buffer, 255, stdin);

    snprintf(buffer,
        sizeof(buffer),
        "%s,%d%s\n%s\n",
        callsign,
        passcode,
        extra,
        message);

//fprintf(stderr, "%s", buffer);

    n = sendto(sockfd,
        buffer,
        (size_t)strlen(buffer),
        0,
        (struct sockaddr *)&server,
        length);
    if (n < 0) {
        fprintf(stderr, "Sendto\n");
        return(1);
    }

    n = recvfrom(sockfd,
        buffer,
        256,
        0,
        (struct sockaddr *)&from,
        &length);
    if (n < 0) {
        fprintf(stderr, "recvfrom\n");
        return(1);
    }

    fprintf(stdout,"Received: %s\n", buffer);

    if (strncmp(buffer, "NACK", 4) == 0) {
//fprintf(stderr,"returning 1\n");
        return(1);  // Received a NACK
    }
    else if (strncmp(buffer, "ACK", 3) == 0) {
//fprintf(stderr,"returning 0\n");
        return(0);  // Received an ACK
    }
    else {
//fprintf(stderr,"returning 1\n");
        return(1);  // Received something other than ACK or NACK
    }
}


