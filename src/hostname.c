/*
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2003  The Xastir Group
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

#include "xastir.h"
#include "main.h"
#include "lang.h"


#ifndef __LCLINT__
  #ifdef __solaris__
    jmp_buf ret_place;
  #else
    static  sigjmp_buf ret_place;       /* Jump address if alarm */
  #endif
#endif // __LCLINT__





/*************************************************************************/
/* Time out on connect                                                   */
/* In case there is a problem in getting the hostname or connecting      */
/* (see  setjmp below).                                                  */
/*************************************************************************/

static void host_time_out( /*@unused@*/ int sig) {
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

char *host_lookup(char *host, char *ip, int time) {
    /*struct sockaddr_in address;*/
    char **names, **addrs;
    static struct hostent *hostinfo;

#ifdef __linux__
    sighandler_t previous_loc;
#else
    void *previous_loc;
#endif

    pid_t host_pid;
    int status;
    char ip_addr[400];
    char temp[40];
    int fp[2];
    char buf[401];
    int ip_found;
    int first;
    int tm;
    int ips;
    int i;
    char ttemp[60];
    int wait_host;

    if (debug_level & 1024)
        fprintf(stderr,"Start Host lookup\n");

    memset(ip_addr,(int)'\0',sizeof(ip_addr));
    memset(buf,(int)'\0',sizeof(buf));

    busy_cursor(appshell);

    if (debug_level & 1024)
        fprintf(stderr,"Creating pipe\n");

    if (pipe(fp)==0) {          // Create a pipe for communication

        host_pid = fork();      // Fork off a child process

        if (debug_level & 1024)
            fprintf(stderr,"Host fork\n");

        if (host_pid!=-1) {     // If the fork was successful

//---------------------------------------------------------------------------------------
            if (host_pid==0) {  // We're in the child process

                // Close the end of the pipe we don't need here

                if (debug_level & 1024)
                    fprintf(stderr,"Child closing read end of pipe\n");

                close(fp[0]);   // Read end of the pipe

                if (debug_level & 1024)
                    fprintf(stderr,"Set alarm \n");

                previous_loc = signal(SIGALRM, host_time_out);

                // Set up to jump here if we time out on SIGALRM
                if (sigsetjmp(ret_place,-1)!=0) {

                    // Turn off the alarm
                    (void)alarm(0);

                    // Reset the SIGALRM handler to its previous value
                    (void)signal(SIGALRM, previous_loc);

                    // Return net connection time out
                    strcpy(ip_addr,"TIMEOUT");
                    (void)write(fp[1],ip_addr,strlen(ip_addr));

                    if (debug_level & 1024)
                        fprintf(stderr,"Child closing write end of pipe\n");

                    close(fp[1]);   // All done writing to the pipe
                    exit(EXIT_FAILURE);
                }
                (void)alarm(time);  // Start the timer

                // Make the call that may time out if no response from DNS
                /*hostinfo = gethostbyname2(host,AF_INET); some systems don't have this*/
                hostinfo = gethostbyname(host);

                // If we get to here, we haven't timed out
                // and we have an answer to process.

                // Turn off the alarm
                (void)alarm(0);
                // Reset the SIGALRM handler to its previous value
                (void)signal(SIGALRM,previous_loc);

                if (hostinfo) {
                    names = hostinfo -> h_aliases;
                    ip_found=0;

                    /* look at all names */
                    ips=0;
                    first=0;
                    if (!*names)
                        first=1;

                    while (*names || first) {
                        if (hostinfo -> h_addrtype == AF_INET) {
                            ip_found=1;
                            addrs = hostinfo -> h_addr_list;
                            while (*addrs) {
                                xastir_snprintf(temp, sizeof(temp), "%s",
                                        inet_ntoa(*(struct in_addr *)*addrs));

                                if (debug_level & 1024)
                                    fprintf(stderr,"IP [%s]\n",temp);

                                if (strlen(temp)>7) {
                                    /* IP found */
                                    if((strlen(ip_addr)+strlen(temp))<sizeof(ip_addr)) {
                                        if (ips>0)
                                            strcat(ip_addr," ");

                                        strcat(ip_addr,temp);
                                        ips++;
                                    }
                                }
                                addrs++;
                            }
                        }
                        if (first) {
                            *names=NULL;
                            first=0;
                        } else
                            names++;
                    }
                    if (ip_found==0) {
                        /* Host not found */
                        strcpy(ip_addr,"NOIP");

                        if (debug_level & 1024)
                            fprintf(stderr,"Host NO IP");

                    }
                }
                else {
                    /* Host not found */
                    strcpy(ip_addr,"NOHOST");

                    if (debug_level & 1024)
                        fprintf(stderr,"NO HOST\n");

                }
                if (debug_level & 1024)
                    fprintf(stderr,"Clear alarm 1\n");

                if (debug_level & 1024)
                    fprintf(stderr,"Clear alarm 2\n");

                (void)write(fp[1],ip_addr,strlen(ip_addr));

                if (debug_level & 1024)
                    fprintf(stderr,"Child closing write end of pipe\n");

                close(fp[1]);   // All done writing to the pipe
                exit(EXIT_FAILURE);

            }   // End of child process
//---------------------------------------------------------------------------------------
            else {
                // We're in the parent process at this point

                // Close the end of the pipe we don't need here

                if (debug_level & 1024)
                    fprintf(stderr,"Parent closing write end of pipe\n");

                close(fp[1]);   // Write end of the pipe

                tm=1;
                wait_host=1;
                while (wait_host!=-1) {
                    xastir_snprintf(ttemp, sizeof(ttemp), langcode("BBARSTA031"), tm++);
                    statusline(ttemp,1);        // Looking up hostname...

                    for (i=0; i < 60 && wait_host!=-1; i++) {
                        wait_host=waitpid(host_pid,&status,WNOHANG);
                        /* update display while waiting */
//                        XmUpdateDisplay(XtParent(da));
                        //usleep(500);
                        sched_yield();
                    }
                }
                (void)read(fp[0],buf,sizeof(buf)-1);
                strcpy(ip,buf);

                if (debug_level & 1024)
                    fprintf(stderr,"Parent closing read end of pipe\n");

                close(fp[0]);   // Close the read end of the pipe
            }
        }
        else {  // We didn't fork
            // Close both ends of the pipe to make
            // sure we've cleaned up properly
            close(fp[0]);
            close(fp[1]);
        }
    }
    else {  // No pipe created
    }
    return(ip);
}


