/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
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

#include <poll.h>

#include "xastir.h"

// Must be last include file
#include "leak_detection.h"

// Atttempt to send to one of the addresses, waiting for 10 seconds
// for (hopefully) a response. Returns 1 on success or 0 if we didn't
// get a response. (Any response is considered a success)
int try_exchange(struct addrinfo *addr, char *buffer, int UNUSED(buflen) )
{
  int sockfd, n;
  socklen_t length;
  struct sockaddr_storage from;
  struct pollfd polls;

  sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
  if (sockfd < 0)
  {
    fprintf(stderr, "socket error: %s\n", strerror(errno));
    return(0);
  }

  n = sendto(sockfd, buffer, (size_t)strlen(buffer), 0, addr->ai_addr,
             addr->ai_addrlen);
  if (n < 0)
  {
    fprintf(stderr, "Sendto error %s\n", strerror(errno));
    close(sockfd);
    return(0);
  }

  polls.fd = sockfd;
  polls.events = POLLIN;

  // wait for up to 10 seconds for a response.
  n = poll(&polls, 1, 10 * 1000);
  if(n == 0)
  {
    fprintf(stderr, "Timeout waiting for response\n");
    close(sockfd);
    return 0;
  }
  else if (n < 0 )
  {
    fprintf(stderr, "poll() returned an error: %s\n", strerror(errno));
    close(sockfd);
    return 0;
  }

  // Response should be waiting, get it.
  n = recvfrom(sockfd, buffer, 256, 0, (struct sockaddr *)&from, &length);
  if (n < 0)
  {
    fprintf(stderr, "recvfrom: %s\n", strerror(errno));
    close(sockfd);
    return(0);
  }

  close(sockfd);
  return 1;
}

#ifndef AI_DEFAULT
  #define AI_DEFAULT (AI_V4MAPPED|AI_ADDRCONFIG)
#endif

// Loop through the possible addresses for hostname (probably IPv6 and IPv4)
// Tries until we are successful (get a reponse) or we run out of addresses
int exchange_packet(char *hostname, char *port, char *buffer, int buflen)
{
  struct addrinfo hints, *res, *r;
  int error;
  int success = 0;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_DEFAULT;

  error = getaddrinfo(hostname, port, &hints, &res);
  if (error)
  {
    fprintf(stderr, "Error: Unable to lookup addresses for host %s port %s\n",
            hostname, port);
    return 1;
  }

  r = res;
  while(!success && r)
  {
    success = try_exchange(r, buffer, buflen);
    r = r->ai_next;
    if(!success && r)
    {
      fprintf(stderr, "Trying next address to send to\n");
    }
  }

  freeaddrinfo(res);
  return success;
}

// Send a UDP packet to a UDP listening port.  This allows scripts
// and other programs to inject packets into Xastir via UDP
// protocol.
// Inputs:
//      hostname    (argv[1])
//      port        (argv[2])
//      callsign    (argv[3])
//      passcode    (argv[4])
//      optional flags:  -identify
//                       -to_rf
//                       -to_inet
//      APRS Packet (argv[5])
// Returns:
//      0: Message sent, ack received
//      1: Error condition
//
//
//
int main(int argc, char *argv[])
{
  char buffer[512];
  char callsign[10];
  char extra[100];
  int passcode;
  char message[256];
  int ii, success;


  if (argc < 6)
  {
    fprintf(stderr,
            "\nUsage: xastir_udp_client server port call passcode -identify\n");
    fprintf(stderr,
            "       xastir_udp_client server port call passcode [-to_rf] [-to_inet] \"APRS Packet\"\n");

    fprintf(stderr,
            "\nExample: xastir_udp_client localhost 2023 ab7cd 1234 \"APRS packet goes here\"\n");
    return(1);
  }

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
  for (ii = 5; ii < argc; ii++)
  {
    if (strstr(argv[ii], "-identify"))
    {
//fprintf(stderr,"Found -identify\n");
      strncat(extra, ",-identify", sizeof(extra)-strlen(extra)-1);
    }
    else if (strstr(argv[ii], "-to_rf"))
    {
//fprintf(stderr,"Found -to_rf\n");
      strncat(extra, ",-to_rf", sizeof(extra)-strlen(extra)-1);
    }
    else if (strstr(argv[ii], "-to_inet"))
    {
//fprintf(stderr,"Found -to_inet\n");
      strncat(extra, ",-to_inet", sizeof(extra)-strlen(extra)-1);
    }
  }


//    fprintf(stdout, "Please enter the message: ");

  // Fetch message portion from the end of the command-line
  snprintf(message, sizeof(message), "%s", argv[argc-1]);
  message[sizeof(message)-1] = '\0';  // Terminate it

  if (message[0] == '\0') // Empty message
  {
    return(1);
  }

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

  success = exchange_packet(argv[1], argv[2], buffer, 256);
  if(!success)
  {
    fprintf(stdout, "No response received.\n");
    return(1);
  }
  fprintf(stdout,"Received: %s\n", buffer);

  if (strncmp(buffer, "NACK", 4) == 0)
  {
//fprintf(stderr,"returning 1\n");
    return(1);  // Received a NACK
  }
  else if (strncmp(buffer, "ACK", 3) == 0)
  {
//fprintf(stderr,"returning 0\n");
    return(0);  // Received an ACK
  }
  else
  {
//fprintf(stderr,"returning 1\n");
    return(1);  // Received something other than ACK or NACK
  }
}


