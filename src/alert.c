/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2004  The Xastir Group
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



// alert_redraw_on_update will cause refresh_image to get called.
// alert_add_entry sets it.


// In the alert structure, flags[] is size 16.  Only the first two
// positions in the array are currently used.
//
// alert_entry.flags[0] // on_screen
//          ?  Initial state or ready-to-recompute state
//          -   Expired between 1 sec and 1 hour
//          Y   Active alert within viewport
//          N   Active alert outside viewport
//
// alert_entry.flags[1] // source
//          DATA_VIA_TNC
//          DATA_VIA_LOCAL
//
// alert_entry.to   alert_entry.alert_level
//     CANCL            C   // Colors of alerts?????
//     TEST             T   // Colors of alerts?????
//     WARN             R   // Colors of alerts?????
//     CIVIL            R   // Colors of alerts?????
//     WATCH            Y   // Colors of alerts?????
//     ADVIS            B   // Colors of alerts?????
//     Other            G   // Colors of alerts?????
//     Unset            ?
//
//
// Here's how Xastir breaks down an alert into an alert struct:
//
// SFONPW>APRS::NWS-ADVIS:191700z,WIND,CA_Z007,CA_Z065, ALAMEDA AND CON & NAPA COUNTY {JDIAA
// |----|       |-------| |-----| |--| |-----| |-----|                                 |-|
//   |              |        |     |      |       |                                     |
//  from            to       |     |    title   title                               issue_date
//                           |  alert_tag
//                        activity (expiration)
//
//
// The code should also handle the case where the packet looks like
// this (it's the same as the above packet except for no expiration
// date):
//
// SFONPW>APRS::NWS-ADVIS:WIND,CA_Z007,CA_Z065, ALAMEDA AND CON & NAPA COUNTY {JDIAA
//
//
// Expiration is then computed from the activity field.  Alert_level
// is computed from "to" and/or "alert_tag".
//
//
// Stuff from Dale, paraphrased by Curt:
//
// WATCH - weather of some type is possible or probable for a geographic 
// area- at present I cannot do watches because they can cover huge areas 
// with hundreds of counties across many states.  I have a prototye of a 
// polygon generator - but that is a whole other can of worms
// 
// WARN - warning - Severe or dangerous weather is occuring or is about to 
// occur in a geographical area.  This we do a pretty good job on output.
// 
// ADVIS - advisory - this can be trivial all the way to a tornado report.
// If a tornado warning is issued and another tornado sighting happens in 
// the same county/zone during the valid time of the first- the info is 
// transmitted as an advisory.  Most of the time is is updates for other 
// messages.
// 
// CANCL - cancelation- discussed in earlier e-mail
// 
// I would add CIVIL for terrorist  earthquake  catostrophic type stuff - 
// the D7 and D&)) have special alarms built in so that a message to 
// NWS-CIVIL would alert folks no matter what there filters are set for.
// 
//
// The clue to which shapefile to use is in the 4th char in the
// title (which is the first following an '_')
//
// ICTSVS>APRS::NWS-ADVIS:120145z,SEVERE_WEATHER,KS_Z091, {C14AA
// TSASVR>APRS::NWS-WARN :120230z,SVRTSM,OK_C113,  OSAGE COUNTY {C16AA
//
// C = county file         (c_mmddyy.dbf)
// A = County Warning File (w_mmddyy.dbf)
// Z = Zone File           (z_mmddyy.dbf or mz_mmddyy.dbf)
// F = Fire zone file      (fz_mmddyy.dbf)
// A = Canadian Area       (a_mmddyy.dbf)
// R = Canadian Region     (r_mmddyy.dbf)
//
// Alerts are comma-delimited on the air s.t. after the
// :NWS-?????: the first field is the time in zulu (or local
// with no 'z'), the 2nd is the warning type
// (severe_thunderstorm etc.), the 3rd and up to 7th are s.t.
// the first 2 letters are the state or marine zone (1st field
// in both the county and zone .dbf files) followed by an
// underline char '_', the area type as above (C, Z, or A),
// then a 3 digit numerical code derived from:
//
// Counties:  the fips code (4th field in the .dbf)
//
// Zones: the zone number (2nd field in the .dbf)
//
// Marine Zones: have proper code in 1st field with addition of '_' in correct place.
//
// CWA: 2nd field has cwa-, these are always "CW_A" plus the cwa
//
// You must ignore anything after a space in the alert.
//
// We will probably want to add the "issue time" to the alert record
// and parse that out if it's present.  Change the view dialog to
// show expiration time, issue time, who the alert is apparently
// from, and the stuff after the curly brace.  Some of that info
// will be useful soon in a finger command to the weather server.
//
// New compressed-mode weather alert packets:
//
// LZKNPW>APRS::NWS-ADVIS:221000z,WINTER_STORM,ARZ003>007-012>016-021>025-030>034-037>047-052>057-062>069{LLSAA
//
// The trick is to step thru the data base contained in the
// shapefiles to determine what areas to light up.  In the above
// example read ">" as "thru" and "-" as "and".
//
// More from Dale:
// It occurs to me you might need some insight into what shapefile to look
// through for a zone/county. The current shape files are c_22mr02, 
// z_16mr02, and mz21fe02.
//
// ICTSVS>APRS::NWS-ADVIS:120145z,SEVERE_WEATHER,KS_Z091, {C14AA
// would be in z_ file
//
// TSASVR>APRS::NWS-WARN :120230z,SVRTSM,OK_C113,  OSAGE COUNTY {C16AA
// would be in c_ file
//
// problem comes with marine warnings-
//
// AM,AN,GM,PZ,PK,PM,LS,LM,LH,LO,LE,SL will look like states, but will all 
// come from the mz file.
//
// so AM_Z686 looks like a state zone, but is a marine zone.
// Aprs Plus requires the exact file name to be specified - winaprs just 
// looks for a file in the nwsshape folder starting c_ z_ and mz.  Someone 
// in the middle of Kansas might not need the marine at all- but here I am 
// closer to marine zones than land. The fact there is an index file for 
// the shapes should help the speed in a lookup.
//
// More from Dale:
// The CWA areas themselves were included for just one product- generally
// called the "Hazardous Weather Outlook".  The idea was to be able to
// click on your region and get a synopsis as a Skywarn Heads-up.  In
// winaprs it turned out that you would get (unwanted) the CWA outline
// instead of some other data about a specific station.  It makes more
// sense to have three or 4 "home CWA'S" that are defined in the config
// file - and have a dialog box to view the Hazardous WX Outlook and
// watches and warnings just for that CWA.  One step futher- I assume there
// is something that trips alarms when a warning is received for a county
// or zone right around you - the cwa or cwa's of interest could be derived
// from that if it already exists.  A long way to say don't worry about CWA
// maps as far as watches/warnings.
// 
// I think the easy coding for determining which shapefile to use would
// look like;
// 
// char sevenCharStr[8];  // seven character string in warning or derived
// //                        from compressed string i. e. AL_Z001
// 
// if the 4th char == 'C' then use "c_shapefile"
// if the 4th char == 'Z';
//       if the first two char == 'AN' ||
//       if the first two char == 'AM' ||
//       if the first two char == 'GM' ||
//       If the first two char == 'PZ' ||
//       if the first two char == 'LH' ||
//       if the first two char == 'LO' ||
//       if the first two char == 'LS' ||          
//       if the first two char == 'SL' ||
//       if the first two char == 'LM' ||
//            then use the "mzshapefile"
//       else use the "z_shapefile"
//
// I am running out of time but that should be all there is to it- I
// will send you a complete list of marine zones later today- I
// think there are no more than 13 or 14 to search through - better
// that the 54 "states"- could be hard coded.

// We now  have fire weather alerts also.  From Dale:
// "Ok I think we can use this to solve a problem with the Fire
// Watches and Warnings taking over the map of someone not
// interested.  Roger,  I had to take Fire warnings off the regular
// aprs-is feeds and send it to firenet.us server only because of
// complaints of the maps getting cluttered-- with data most people
// didn't want. Even though the NWS sends "AZZ148" just as if it
// were to be found in the z_mmddyy.dbf (warning zone) type file,
// wxsvr knows it is coming out of a "Fire Weather" type product and
// can substitute "AZF148" .  Client software (read xastir &
// Ui-view) would know to look in the fireweather shapefile.  If
// someone doesn't have the fire shapefile loaded, it would just be
// ignored (I think)."


// Found on the AWIPS web pages so far:
// AWIPS Counties          C
// County Warning Areas    A
// Zone Forecast Areas     Z
// Coastal Marine Zones    Z
// Offshore Marine Zones   Z
// High Seas Marine Zones  Z    (Says "Not for operational use")
// Fire Weather Zones      FZ
//
// Don't forget about the Canadian Areas and Regions, which are
// created by Dale Huguley from Environment Canada data.
//
//
// AWIPS Counties:
// -----------------------------
// STATE        character   2
// CWA          character   9
// COUNTYNAME   character   24
// FIPS         character   5
// TIME_ZONE    character   2
// FE_AREA      character   2
// LON          numeric     10,5
// LAT          numeric     9,5
//
//
// County Warning Areas:
// -----------------------------
// WFO          character   3
// CWA          character   3
// LON          numeric     10,5
// LAT          numeric     9,5
//
//
// Zone Forecast Areas:
// -----------------------------
// STATE        character   2
// ZONE         character   3
// CWA          character   3
// NAME         character   254
// STATE_ZONE   character   5
// TIME_ZONE    character   2
// FE_AREA      character   2
// LON          numeric     10,5
// LAT          numeric     9,5
//
//
// Coastal and Offshore Marine Zones:
// ----------------------------------
// ID           character   6
// WFO          character   3
// NAME         character   250
// LON          numeric     10,5
// LAT          numeric     9,5
// WFO_AREA     character   200
//
//
// High Seas Marine Zones:
// -----------------------------
// WFO          character   3
// LON          numeric     10,5
// LAT          numeric     9,5
// HEADING      character   250



#include "config.h"

#ifdef  WITH_DMALLOC
#include <dmalloc.h>
#endif  // WITH_DMALLOC
#include <stdlib.h>
#include <stdio.h>

#include <assert.h>
#include <ctype.h>

#ifdef  HAVE_LOCALE_H
#include <locale.h>
#endif  // HAVE_LOCALE_H

#ifdef  HAVE_LIBINTL_H
#include <libintl.h>
#define _(x)        gettext(x)
#else   // HAVE_LIBINTL_H
#define _(x)        (x)
#endif  // HAVE_LIBINTL_H

#include <Xm/XmAll.h>

#include "alert.h"
#include "xastir.h"
#include "util.h"
#include "snprintf.h"
#include "wx.h"


alert_entry *alert_list = NULL;
static int alert_list_count = 0;           // Count of active alerts
int alert_max_count = 0;     // Alerts we've allocated space for
int alert_redraw_on_update = 0;


// How many alerts we add storage for each time we're short.
#define ALERT_COUNT_INCREMENT 25





// normal_title()
//
// Function to convert "County Warning Area" to "CWA" in a string.
// Called from alert_match() and alert_update_list() functions.
//
void normal_title(char *incoming_title, char *outgoing_title) {
    char *c_ptr;


//    if (debug_level & 2)
//        fprintf(stderr,"normal_title: Incoming: %s\n",incoming_title);

    strncpy(outgoing_title, incoming_title, 32);
    outgoing_title[32] = '\0';
    if ((c_ptr = strstr(outgoing_title, "County Warning Area ")) && c_ptr == outgoing_title) {
        c_ptr = &outgoing_title[strlen("County Warning Area ")]; // Find end of text
        strncpy(outgoing_title, "CWA",4);  // Add "CWA" to output string instead
        // Copy remaining portion of input string to the output string
        strncat(outgoing_title, c_ptr, 32-strlen("County Warning Area "));
        outgoing_title[32] = '\0';  // Make sure string is terminated
    }
    // Remove ". " strings ( .<space> )
    while ((c_ptr = strstr(outgoing_title, ". ")))
        memmove(c_ptr, c_ptr+2, strlen(c_ptr)+1);

    // Terminate string at '>' character
    if ((c_ptr = strpbrk(outgoing_title, " >")))
        *c_ptr = '\0';

    // Remove these characters from the string
    while ((c_ptr = strpbrk(outgoing_title, "_.-}!")))
        memmove(c_ptr, c_ptr+1, strlen(c_ptr)+1);

    // Truncate the string to eight characters always
    outgoing_title[8] = '\0';

//    if (debug_level & 2)
//        fprintf(stderr,"normal_title: Outgoing: %s\n",outgoing_title);
}





// alert_print_list()
//
// Debug routine.  Currently attached to the Test() function in
// main.c, but the button in the file menu is normally grey'ed out.
// This function prints the current weather alert list out to the
// xterm.
//
void alert_print_list(void) {
    int i;
    char title[100], *c_ptr;

    fprintf(stderr,"Alert counts: %d/%d\n", alert_list_count, alert_max_count);

    for (i = 0; i < alert_max_count; i++) {

        // Check whether it's an active alert
        if (alert_list[i].title[0] != '\0') {

            strncpy(title, alert_list[i].title, 99);
            title[99] = '\0';

            for (c_ptr = &title[strlen(title)-1]; *c_ptr == ' '; c_ptr--)
                *c_ptr = '\0';

// Alert: 10Y, MSPTOR> NWS-TEST, TAG: T  TORNDO, ACTIVITY: 191915z, Expiration: 1019234700, Title: WI_Z003
            fprintf(stderr,"Alert:%4d%c,%9s>%9s, Tag: %c%20s, Activity: %9s, Expiration: %lu, Title: %s\n",
                i,                                          // 10
                alert_list[i].flags[on_screen],             // Y
                alert_list[i].from,                         // MSPTOR
                alert_list[i].to,                           // NWS-TEST

                alert_list[i].alert_level,                  // T
                alert_list[i].alert_tag,                    // TORNDO

                alert_list[i].activity,                     // 191915z (expiration)

                (unsigned long)(alert_list[i].expiration),  // 1019234700

                title);                                     // WI_Z003
        }
    }
}





// Needed by alert_expire() below
static time_t last_alert_expire = 0;



// alert_expire()
//
// Delete stored alerts that have expired, by zeroing the title
// string.  This free's up the slot to be used by another alert, and
// makes sure that the expired alert doesn't get drawn or shown in
// the View->WX Alerts dialog.
//
// Returns the quantity of alerts that were just expired.
//
int alert_expire(void) {
    int ii;
    int expire_count = 0;

    // Check only every 60 seconds
    if ( (last_alert_expire + 60) > sec_now() ) {
        return(0);
    }
    last_alert_expire = sec_now();

    if (debug_level & 2)
        fprintf(stderr,"Checking for expired alerts...\n");

    // Delete stored alerts that have expired (zero the title string)
    for (ii = 0; ii < alert_max_count; ii++) {
        if ( (alert_list[ii].title[0] != '\0')
                && (alert_list[ii].expiration < time(NULL)) ) {
            if (debug_level & 2) {
                fprintf(stderr,
                    "alert_expire: Clearing slot %d, current: %lu, alert: %lu\n",
                    ii,
                    time(NULL),
                    alert_list[ii].expiration );
            }
            alert_list[ii].title[0] = '\0'; // Clear this alert
            alert_list_count--;

//WE7U
//fprintf(stderr,"alerts:%d\tmax:%d\n",alert_list_count,alert_max_count);

            expire_count++;
        }
    }

    // Cause a screen redraw if we expired some alerts
    if (expire_count) {
        // Schedule a screen update 'cuz we have a new alert
        alert_redraw_on_update = redraw_on_new_data = 2;
 
    }

    return(expire_count);
}





// alert_add_entry()
//
// This function adds a new alert to our alert list.
//
// Returns address of entry in alert_list or NULL.
// Called from alert_build_list() function.
//
/*@null@*/ static alert_entry *alert_add_entry(alert_entry *entry) {
    alert_entry *ptr;
    int i;


    if (debug_level & 2)
        fprintf(stderr,"alert_add_entry\n");

    // Skip NWS_SOLAR and -NoActivationExpected alerts, they don't
    // interest us.
    if ( (strcmp(entry->to, "NWS-SOLAR") == 0)
            || (strcmp(entry->to, "NWS_SOLAR") == 0) ) {
        if (debug_level & 2)
            fprintf(stderr,"NWS-SOLAR, skipping\n");
        return(NULL);
    }
    if (strcasecmp(entry->title, "-NoActivationExpected") == 0) {
        if (debug_level & 2)
            fprintf(stderr,"NoActivationExpected, skipping\n");
        return(NULL);
    }

    // Check whether this new alert has aleady expired.  If so,
    // don't add it.
    if (entry->expiration < time(NULL)) {
        if (debug_level & 2) {
            fprintf(stderr,
                "Newest Alert Expired->Clearing, current: %lu, alert: %lu\n",
                time(NULL),
                entry->expiration );
        }
        return(NULL);
    }

    // Allocate more space if we're at our maximum already.
    // Allocate space for ALERT_COUNT_INCREMENT more alerts.
    if (alert_list_count == alert_max_count) {
        ptr = realloc(alert_list, (alert_max_count+ALERT_COUNT_INCREMENT)*sizeof(alert_entry));
        if (ptr) {
            int ii;
            alert_list = ptr;

            // Zero out the new added entries
            for (ii = alert_max_count; ii < alert_max_count+ALERT_COUNT_INCREMENT; ii++) {
                alert_list[ii].title[0] = '\0';
                alert_list[ii].top_boundary = 0.0;
                alert_list[ii].left_boundary = 0.0;
                alert_list[ii].bottom_boundary = 0.0;
                alert_list[ii].right_boundary = 0.0;
                alert_list[ii].expiration = (time_t)0;
                alert_list[ii].activity[0] = '\0';
                alert_list[ii].alert_tag[0] = '\0';
                alert_list[ii].alert_level = '\0';
                alert_list[ii].from[0] = '\0';
                alert_list[ii].to[0] = '\0';
                alert_list[ii].flags[0] = '\0';
                alert_list[ii].filename[0] = '\0';
                alert_list[ii].index = 0;
                alert_list[ii].seq[0] = '\0';
                alert_list[ii].issue_date_time[0] = '\0';
                alert_list[ii].desc0[0] = '\0';
                alert_list[ii].desc1[0] = '\0';
                alert_list[ii].desc2[0] = '\0';
                alert_list[ii].desc3[0] = '\0';
            }
            alert_max_count += ALERT_COUNT_INCREMENT;

//fprintf(stderr, "        Max Alert Array: %d\n", alert_max_count);

        }
        else {
            fprintf(stderr,"Couldn't realloc alert_list\n");
        }

        if (debug_level & 2) {
            fprintf(stderr,"Allocated %d more slots for alerts, Max=%d\n",
                ALERT_COUNT_INCREMENT,
                alert_max_count);
        }
    }

    // Check for non-zero alert title, non-expired alert time in new
    // alert.
    if (entry->title[0] != '\0' && entry->expiration >= time(NULL)) {

        // Schedule a screen update 'cuz we have a new alert
        alert_redraw_on_update = redraw_on_new_data = 2;

        // Scan for an empty entry, fill it in if found
        for (i = 0; i < alert_max_count; i++) {
            if (alert_list[i].title[0] == '\0') {   // If alert entry is empty
                memcpy(&alert_list[i], entry, sizeof(alert_entry)); // Use it
                alert_list_count++;
//WE7U
//fprintf(stderr,"alerts:%d\tmax:%d\n",alert_list_count,alert_max_count);

                if (debug_level & 2) {
                    fprintf(stderr,"Found empty alert slot %d, filling it\n", i);
                }
                return( &alert_list[i]);
            }
        }
    }

    // If we got to here, the title was empty or the alert has
    // already expired?  Figure out why we might ever get here.
    if (debug_level & 2) {
        fprintf(stderr,"Exiting alert_add_entry() without actually adding the alert:\n");
        fprintf(stderr,"%s %s %lu\n",
            entry->to,
            entry->title,
            entry->expiration);
    }

    return(NULL);
}





// alert_match()
//
// Function used for matching on alerts.
// Returns address of matching entry in alert_list or NULL.
// Called from alert_build_list(), alert_update_list(), and
// alert_active() functions.
//
static alert_entry *alert_match(alert_entry *alert, alert_match_level match_level) {
    int i;
    char *ptr, title_e[33], title_m[33], alert_f[33], filename[33];


    if (debug_level & 2)
        fprintf(stderr,"alert_match\n");
 
    // Shorten the title
    normal_title(alert->title, title_e);

    strncpy(filename, alert->filename, 32);
    title_e[32] = title_m[32] = alert_f[32] = filename[32] = '\0';

    // Truncate at '.'
    if ((ptr = strpbrk(filename, ".")))
        *ptr = '\0';

    // Get rid of '/' characters
    if ((ptr = strrchr(filename, '/'))) {
        ptr++;
        memmove(filename, ptr, strlen(ptr)+1);
    }

    // Get rid of ' ', '_', or '-' characters
    while ((ptr = strpbrk(filename, "_ -")))
        memmove(ptr, ptr+1, strlen(ptr)+1);

    for (i = 0; i < alert_max_count; i++) {

        if (alert_list[i].title[0] == '\0') {
            // Found a blank alert list entry, skip it.
            continue;
        }

        // Shorten the title
        normal_title(alert_list[i].title, title_m);

        strncpy(alert_f, alert_list[i].filename, 32);
        alert_f[32] = '\0';

        // Truncate at '.'
        if ((ptr = strpbrk(alert_f, ".")))
            *ptr = '\0';

        // Get rid of '/' characters
        if ((ptr = strrchr(alert_f, '/'))) {
            ptr++;
            memmove(alert_f, ptr, strlen(ptr)+1);
        }

        // Get rid of ' ', '_', or '-' characters
        while ((ptr = strpbrk(alert_f, "_ -")))
            memmove(ptr, ptr+1, strlen(ptr)+1);

        // Look for non-cancelled alerts that match fairly closely
        if ((match_level < ALERT_FROM || strcmp(alert_list[i].from, alert->from) == 0) &&
                (match_level < ALERT_TO   || strcasecmp(alert_list[i].to, alert->to) == 0) &&
                (match_level < ALERT_TAG  || strcmp(alert_list[i].alert_tag, alert->alert_tag) == 0) &&
                (title_m[0] && (strncasecmp(title_e, title_m, strlen(title_m)) == 0 ||
                strcasecmp(title_m, filename) == 0 || strcasecmp(alert_f, title_e) == 0 ||
                (alert_f[0] && filename[0] && strcasecmp(alert_f, filename) == 0)))) {
            return (&alert_list[i]);
        }

        // Now check whether a new CANCL alert passed to us might match one
        // of the existing alerts.  We use a much looser match for this.
        if (    (alert->alert_level == 'C')         // Cancelled alert
                && (match_level < ALERT_FROM
                    || strcmp(alert_list[i].from, alert->from) == 0)

                && (match_level < ALERT_TAG
                    || strncasecmp(alert_list[i].alert_tag,
                            alert->alert_tag,
                            strlen(alert_list[i].alert_tag)) == 0
                    || strncasecmp(alert_list[i].alert_tag,
                            alert->alert_tag,
                            strlen(alert->alert_tag)) == 0)

                && (title_m[0] && (strncasecmp(title_e, title_m, strlen(title_m)) == 0
                    || strcasecmp(title_m, filename) == 0
                    || strcasecmp(alert_f, title_e) == 0))) {
            if (debug_level & 2)
                fprintf(stderr,"Found a cancellation: %s\t%s\n",title_e,title_m);

            return (&alert_list[i]);
        }


/*
// I've been told by Dale Huguely that this might occur:  A new
// alert that shouldn't match the cancelled alert.  Tabling this
// ammendment for now.  ;-)
*/

        // Now check whether a new alert passed to us might match a
        // cancelled existing alert.  We use a much looser match for
        // this.
        if (    (alert_list[i].alert_level == 'C')         // Cancelled alert
                && (match_level < ALERT_FROM
                    || strcmp(alert_list[i].from, alert->from) == 0)

                && (match_level < ALERT_TAG
                    || strncasecmp(alert_list[i].alert_tag,
                            alert->alert_tag,
                            strlen(alert_list[i].alert_tag)) == 0
                    || strncasecmp(alert_list[i].alert_tag,
                            alert->alert_tag,
                            strlen(alert->alert_tag)) == 0)

                && (title_m[0] && (strncasecmp(title_e, title_m, strlen(title_m)) == 0
                    || strcasecmp(title_m, filename) == 0
                    || strcasecmp(alert_f, title_e) == 0))) {
            if (debug_level & 2)
                fprintf(stderr,"New alert that matches a cancel: %s\t%s\n",title_e,title_m);

            return (&alert_list[i]);
        }
    }
    return (NULL);
}





// alert_update_list()
//
// Updates entry in alert_list from new matching alert.  Checks for
// matching entry first, else fills in blank entry.
// Called from maps.c:load_alert_maps() function.
//
void alert_update_list(alert_entry *alert, alert_match_level match_level) {
    alert_entry *ptr;
    int i;
    char title_e[33], title_m[33];


    if (debug_level & 2)
        fprintf(stderr,"alert_update_list\n");

    // Find the matching alert in alert_list, copy updated
    // parameters from new alert into existing alert_list entry.
    if ((ptr = alert_match(alert, match_level))) {
        if (!ptr->filename[0]) {    // We found a match!  Fill it in.
            strncpy(ptr->filename, alert->filename, sizeof(ptr->filename)-1);
            strncpy(ptr->title, alert->title, sizeof(ptr->title)-1);
            ptr->filename[sizeof(ptr->filename)-1] = ptr->title[sizeof(ptr->title)-1] = '\0';
            // Boundary markers now being used for shape lookup
            //ptr->top_boundary = alert->top_boundary;
            //ptr->left_boundary = alert->left_boundary;
            //ptr->bottom_boundary = alert->bottom_boundary;
            //ptr->right_boundary = alert->right_boundary;
        }
        ptr->flags[on_screen] = alert->flags[on_screen];

        // Shorten title
        normal_title(alert->title, title_e);

        // Force the string to be terminated
        title_e[sizeof(title_e)-1] = title_m[sizeof(title_m)-1] = '\0';

        // Interate through the entire alert_list, checking flags
        for (i = 0; i < alert_max_count; i++) {

            // If flag was '?' or has changed, update the alert
            if ((alert_list[i].flags[on_screen] == '?' || alert_list[i].flags[on_screen] != ptr->flags[on_screen])) {

                // Shorten the title.  Title_m will be the shortened
                // title.
                normal_title(alert_list[i].title, title_m);

                if (strcmp(title_e, title_m) == 0) {

                    // Update parameters
                    if (!alert_list[i].filename[0]) {
                        strncpy(alert_list[i].filename, alert->filename, sizeof(alert_list[0].filename)-1);
                        alert_list[i].filename[sizeof(alert_list[0].filename)-1] = '\0';
                        //alert_list[i].top_boundary = alert->top_boundary;
                        //alert_list[i].left_boundary = alert->left_boundary;
                        //alert_list[i].bottom_boundary = alert->bottom_boundary;
                        //alert_list[i].right_boundary = alert->right_boundary;
                        strncpy(alert_list[i].title, alert->title, 32);
                        alert_list[i].title[32] = '\0';
                    }
                    alert_list[i].flags[on_screen] = alert->flags[on_screen];
                }
            }
        }
    }
}





// alert_active()
//
// Here's where we get rid of expired alerts in the list.  Called
// from alert_compare(), alert_display_request(), alert_on_screen(),
// and alert_build_list() functions.  Also called from
// maps.c:load_alert_maps() function.
//
// Returns the alert level.
//
// Alert Match Levels:
// 0 = ?
// 1 = R
// 2 = Y
// 3 = B
// 4 = T
// 5 = G
// 6 = C
//
int alert_active(alert_entry *alert, alert_match_level match_level) {
    alert_entry *a_ptr;
    char l_list[] = {"?RYBTGC"};
    int level = 0;
    time_t now;


    if (debug_level & 2)
        fprintf(stderr,"alert_active\n");

    (void)time(&now);

    if ((a_ptr = alert_match(alert, match_level))) {
        if (a_ptr->expiration >= now) {
            for (level = 0; a_ptr->alert_level != l_list[level] && level < (int)sizeof(l_list); level++);
        }
        else if (a_ptr->expiration < (now - 3600)) {    // More than an hour past the expiration,
            a_ptr->title[0] = '\0';                     // so delete it from list by clearing
                                                        // out the title.
            // Knock the active count down by one
            alert_list_count--;

//WE7U
//fprintf(stderr,"alerts:%d\tmax:%d\n",alert_list_count,alert_max_count);

            // Schedule an update 'cuz we need to delete an expired
            // alert from the list.
            alert_redraw_on_update = redraw_on_new_data = 2;
        }
        else if (a_ptr->flags[on_screen] == '?') {  // Expired between 1sec and 1hr and found '?'
            a_ptr->flags[on_screen] = '-';
 
            // Schedule a screen update 'cuz we have an expired alert
            alert_redraw_on_update = redraw_on_new_data = 2;
        }
    }
    return (level);
}





// alert_compare()
//
// Used by qsort as the compare function in alert_sort_active()
// function below.
//
static int alert_compare(const void *a, const void *b) {
    alert_entry *a_entry = (alert_entry *)a;
    alert_entry *b_entry = (alert_entry *)b;
    int a_active, b_active;

    if (a_entry->title[0] && !b_entry->title[0])
        return (-1);

    if (!a_entry->title[0] && b_entry->title[0])
        return (1);

    if (a_entry->flags[on_screen] == 'Y' && b_entry->flags[on_screen] != 'Y')
        return (-1);

    if (a_entry->flags[on_screen] != 'Y' && b_entry->flags[on_screen] == 'Y')
        return (1);

    if (a_entry->flags[on_screen] == '?' && b_entry->flags[on_screen] == 'N')
        return (-1);

    if (a_entry->flags[on_screen] == 'N' && b_entry->flags[on_screen] == '?')
        return (1);

    a_active = alert_active(a_entry, ALERT_ALL);
    b_active = alert_active(b_entry, ALERT_ALL);
    if (a_active && b_active) {
        if (a_active - b_active) {
            return (a_active - b_active);
        }
    } else if (a_active) {
        return (-1);
    }
    else if (b_active) {
        return (1);
    }

    return (strcmp(a_entry->title, b_entry->title));
}





// alert_sort_active()
//
// This sorts the alert_list so that the active items are at the
// beginning.
// Called from maps.c:load_alert_maps() function.
//
void alert_sort_active(void) {


    if (debug_level & 2)
        fprintf(stderr,"alert_sort_active\n");

    qsort(alert_list, (size_t)alert_max_count, sizeof(alert_entry), alert_compare);
}





// alert_display_request()
//
// Function which checks whether an alert should be displayed.
// Called from maps.c:load_alert_maps() function.
//
int alert_display_request(void) {
    int i, alert_count;
    static int last_alert_count;


    if (debug_level & 2)
        fprintf(stderr,"alert_display_request\n");

    // Iterate through entire alert_list
    for (i = 0, alert_count = 0; i < alert_max_count; i++) {

        // If it's an active alert (not expired), and flags == 'Y'
        // (meaning it is within our viewport), or flags == '?' (indicating unmatched).
        if (alert_active(&alert_list[i], ALERT_ALL) && (alert_list[i].flags[on_screen] == 'Y' ||
                alert_list[i].flags[on_screen] == '?')) {
            alert_count++;
        }
    }

    // If we found any, return TRUE.
    if (alert_count != last_alert_count) {
        last_alert_count = alert_count;
        return ((int)TRUE);
    }

    return ((int)FALSE);
}





// alert_on_screen()
//
// Returns a count of active weather alerts in the list which are
// within our viewport.
// Called from main.c:UpdateTime() function.
//
int alert_on_screen(void) {
    int i, alert_count;


    if (debug_level & 2)
        fprintf(stderr,"alert_on_screen\n");

    for (i = 0, alert_count = 0; i < alert_max_count; i++) {
        if (alert_active(&alert_list[i], ALERT_ALL)
                && alert_list[i].flags[on_screen] == 'Y') {
            alert_count++;
        }
    }

    return (alert_count);
}





// alert_build_list()
//
// This function builds alert_entry structs from message entries that
// contain NWS alert messages.
//
// Called from alert_data_add() function.
//
//
// Here's how Xastir breaks down an alert into an alert struct:
//
// SFONPW>APRS::NWS-ADVIS:191700z,WIND,CA_Z007,CA_Z065, ALAMEDA AND CON & NAPA COUNTY {JDIAA
// |----|       |-------| |-----| |--| |-----| |-----|                                 |-|
//   |              |        |     |      |       |                                     |
//  from            to       |     |    title   title                               issue_date
//                           |  alert_tag
//                        activity (expiration)
//
//
// The code should also handle the case where the packet looks like
// this (same except no expiration date):
//
// SFONPW>APRS::NWS-ADVIS:WIND,CA_Z007,CA_Z065, ALAMEDA AND CON & NAPA COUNTY {JDIAA
//
// We also now have compressed NWS alerts, signified by NWS_ADVIS
// (underline instead of dash).
//
//
// Expiration is then computed from the activity field.  Alert_level
// is computed from "to" and/or "alert_tag".  There can be up to
// five titles in this original format.
//
// Here are some real examples captured over the 'net (may be quite old):
//
// TAEFFS>APRS::NWS-ADVIS:181830z,FLOOD,FL_C013,FL_C037,FL_C045,FL_C077, {HHEAA
// ICTFFS>APRS::NWS-ADVIS:180200z,FLOOD,KS_C035, {HEtAA
// JANFFS>APRS::NWS-ADVIS:180200z,FLOOD,MS_C049,MS_C079,MS_C089,MS_C099,MS_C121, {HEvAA
// DSMFFS>APRS::NWS-ADVIS:180500z,FLOOD,IA_Z086, {HHGAA
// EAXFFS>APRS::NWS-ADVIS:180500z,FLOOD,MO_Z023,MO_Z024,MO_Z028,MO_Z030,MO_Z031, {HHIAA
// SECIND>APRS::NWS-SOLAR:Flx134 A004 BK0001232.  PlnK0000232.Ep............Ee........ {HLaAA
// SHVFFS>APRS::NWS-ADVIS:181800z,FLOOD,TX_C005,TX_C073,TX_C347,TX_C365,TX_C401, {HF2AA
// FWDFFS>APRS::NWS-ADVIS:180200z,FLOOD,TX_C379,TX_C467, {HF5AA
// LCHFFS>APRS::NWS-ADVIS:180400z,FLOOD,LA_C003,LA_C079, {HIdAA
// GIDFFS>APRS::NWS-ADVIS:180200z,FLOOD,NE_C125, {H2uAA
// FWDSWO>APRS::NWS-ADVIS:181100z,SKY,CW_AFWD, -NO Activation Expected {HLqAA
// BGMWSW>APRS::NWS-ADVIS:180500z,WINTER_WEATHER,NY_Z015,NY_Z016,NY_Z017,NY_Z022,NY_Z023, {HKYAA
// AMAWSW>APRS::NWS-WARN :180400z,WINTER_STORM,OK_Z001,OK_Z002,TX_Z001,TX_Z002,TX_Z003, {HLGBA
//
//
// New compressed-mode weather alert packets:
//
// LZKNPW>APRS::NWS-ADVIS:221000z,WINTER_STORM,ARZ003>007-012>016-021>025-030>034-037>047-052>057-062>069{LLSAA
//
// or perhaps (leading zeroes removed):
//
// LZKNPW>APRS::NWS-ADVIS:221000z,WINTER_STORM,ARZ3>7-12>16-21>25-30>34-37>47-52>57-62>69{LLSAA
//
// This one's real:
// DVNFFS>APRS,qAO,WXSVR::NWS_ADVIS:022300z,FLOOD,IAC57-95-103-111-115-163-171-ILC1-67-71-131-MOC45 {2FsAA
//
// The trick is to step thru the data base contained in the
// shapefiles to determine what areas to light up.  In the above
// example read ">" as "thru" and "-" as "and".
//
//
// RIWWSW>APRS::NWS-WARN :191800z,WINTER_STORM,WY_Z014, GREEN MOUNTAINS {JBNBA
// RIWWSW>APRS::SKYRIW   :WINTER STORM WARNING CONTINUING TODAY {JBNBB
// RIWWSW>APRS::SKYRIW   :THROUGH SATURDAY {JBNBC
//
//
// We'll create and fill in "entry", then copy various "titles" into
// is such as "ID_C001", then insert that alert into the system.
//
void alert_build_list(Message *fill) {
    alert_entry entry, *list_ptr;
    char title[5][33];  // Storage place for zone/county titles
    int ii, jj;
    char *ptr;
    DataRow *p_station;
    int compressed_wx_packet = 0;
    char uncompressed_wx[10000];


    //fprintf(stderr,"Message_line:%s\n",fill->message_line);

    if (debug_level & 2) {
        fprintf(stderr,"alert_build_list:%s>%s:%s\n",
            fill->from_call_sign,
            fill->call_sign,
            fill->message_line);
    }

    // Empty this string first
    uncompressed_wx[0] = '\0';

    // Check for "SKY" text in the "call_sign" field.
    if (strncmp(fill->call_sign,"SKY",3) == 0) {
        // Special handling for SKY messages only.

        if (debug_level & 2)
            fprintf(stderr,"Sky Message: %s\n",fill->message_line);

        // Find a matching alert_record, check whether or not it is
        // expired.  If not, add this additional text into the
        // "desc[0123]" fields, in order.  Check that the
        // FROM callsign and the first four chars after the curly
        // brace match.  The next character specifies which message
        // block to fill in.  In order they should be:
        //
        // B = "desc0"
        // C = "desc1"
        // D = "desc2"
        // E = "desc3".
        //
        // A matching alert record would have the same "from" field
        // and the first four characters of the "seq" field would
        // match.
        //
        // Need to make this SKY data expire from the message list
        // somehow?
        // 
        // Remember to blank out these fields when we expire an
        // alert.  Check that all other fields are cleared in this
        // case as well.
        //

        // Run through the alert_list looking for a match to the
        // FROM and first four chars of SEQ
        for (ii = 0; ii < alert_max_count; ii++) {
            if ( (strcasecmp(alert_list[ii].from, fill->from_call_sign) == 0)
                    && ( strncmp(alert_list[ii].seq,fill->seq,4) == 0 ) ) {

                if (debug_level & 2)
                    fprintf(stderr,"%d:Found a matching alert to a SKY message:\t",ii);

                switch (fill->seq[4]) {
                    case 'B':
                        strncpy(alert_list[ii].desc0,fill->message_line,68);
                        alert_list[ii].desc0[67] = '\0';
                        if (debug_level & 2)
                            fprintf(stderr,"Wrote into desc0: %s\n",fill->message_line);
                        break;
                    case 'C':
                        strncpy(alert_list[ii].desc1,fill->message_line,68);
                        alert_list[ii].desc1[67] = '\0';
                        if (debug_level & 2)
                            fprintf(stderr,"Wrote into desc1: %s\n",fill->message_line);
                        break;
                    case 'D':
                        strncpy(alert_list[ii].desc2,fill->message_line,68);
                        alert_list[ii].desc2[67] = '\0';
                        if (debug_level & 2)
                            fprintf(stderr,"Wrote into desc2: %s\n",fill->message_line);
                        break;
                    case 'E':
                    default:
                        strncpy(alert_list[ii].desc3,fill->message_line,68);
                        alert_list[ii].desc3[67] = '\0';
                        if (debug_level & 2)
                            fprintf(stderr,"Wrote into desc3: %s\n",fill->message_line);
                        break;
                }
//                return; // All done with this sky message
            }
        }
        if (debug_level & 2)
            fprintf(stderr,"alert_build_list exit 1\n");
        return;
    }

    if (debug_level & 2)
        fprintf(stderr,"1\n");

    if (fill->active == RECORD_ACTIVE) {
        int ignore_title = 0;
#define MAX_SUB_ALERTS 5000
        char *title_ptr[MAX_SUB_ALERTS];


        if (debug_level & 2)
            fprintf(stderr,"2\n");

        memset(&entry, 0, sizeof(entry));

        // Zero the title strings.  We can have up to five alerts in
        // a non-compressed weather alert.
        title[0][0] = '\0';
        title[1][0] = '\0';
        title[2][0] = '\0';
        title[3][0] = '\0';
        title[4][0] = '\0';

        // This fills in the zone numbers (title) for uncompressed
        // alerts with up to five alerts per message.  This doesn't
        // handle filling in the title for compressed alerts though.
        (void)sscanf(fill->message_line,
            "%20[^,],%20[^,],%32[^,],%32[^,],%32[^,],%32[^,],%32[^,]",
            entry.activity,      // 191700z
            entry.alert_tag,     // WIND
            &title[0][0],        // CA_Z007
            &title[1][0],        // ...
            &title[2][0],        // ...
            &title[3][0],        // ...
            &title[4][0]);       // ...

        // Force a termination for each
        entry.activity[20]  = '\0';
        entry.alert_tag[20] = '\0';
        title[0][32]        = '\0';
        title[1][32]        = '\0';
        title[2][32]        = '\0';
        title[3][32]        = '\0';
        title[4][32]        = '\0';



        // Check for "NWS_" in the call_sign field.  Underline
        // signifies compressed alert format.  Dash signifies
        // non-compressed format.
        if (strncmp(fill->call_sign,"NWS_",4) == 0) {
            char compressed_wx[512];
            char *ptr;

/////////////////////////////////////////////////////////////////////
// Compressed weather alert special code
/////////////////////////////////////////////////////////////////////

            compressed_wx_packet++; // Set the flag

//fprintf(stderr, "Found compressed alert packet via NWS_!\n");

//fprintf(stderr,"Compressed Weather Alert:%s\n",fill->message_line);
//fprintf(stderr,"Compressed alerts are not fully implemented yet.\n");

            // Create a new weather alert for each of these and then
            // call this function on each one?  Seems like it might
            // work fine if we watch out for global variables.
            // Another method would be to create an incoming message
            // for each one and add it to the message queue, or just
            // a really long new message and add it to the queue,
            // in which case we'd exit from this routine as soon as
            // it was submitted.
            (void)sscanf(fill->message_line, "%20[^,],%20[^,],%255[^, ]",
                entry.activity,
                entry.alert_tag,
                compressed_wx);     // Stick the long string in here
            compressed_wx[255] = '\0';

//fprintf(stderr,"Line:%s\n",compressed_wx);

            // Snag alpha characters (should be three) at the start
            // of the string.  Use those until we hit more alpha
            // characters.  First two characters of each 3-letter
            // alpha group are the state, last character is the
            // zone/county/marine-zone indicator.

// Need to be very careful here to validate the letters/numbers, and
// to not run off the end of the string.  Need more code here to do
// this validation.


            // Scan through entire string
            ptr = compressed_wx;
            while (ptr < (compressed_wx + strlen(compressed_wx))) {
                char prefix[5];
                char suffix[4];
                char temp_suffix[4];
                char ending[4];


                // Snag the ALPHA portion
                strncpy(prefix,ptr,2);
                ptr += 2;
                prefix[2] = '_';
                prefix[3] = ptr[0];
                prefix[4] = '\0';   // Terminate the string
                ptr += 1;
                // prefix should now contain something like "MN_Z"

                // Snag the NUMERIC portion.  Note that the field
                // width can vary between 1 and 3.  The leading
                // zeroes have been removed.
                strncpy(temp_suffix,ptr,3);
                temp_suffix[3] = '\0';   // Terminate the string
                if (temp_suffix[1] == '-' || temp_suffix[1] == '>') {
                    temp_suffix[1] = '\0';
                    ptr += 1;
                }
                else if (temp_suffix[2] == '-' || temp_suffix[2] == '>') {
                    temp_suffix[2] = '\0';
                    ptr += 2;
                }
                else {
                    ptr += 3;
                }
                // temp_suffix should now contain something like
                // "039" or "45" or "2".  Add leading zeroes to give
                // "suffix" a length of 3.
                strncpy(suffix,"000",3);
                switch (strlen(temp_suffix)) {
                    case 1: // Copy one char across
                        suffix[2] = temp_suffix[0];
                        break;
                    case 2: // Copy two chars across
                        suffix[1] = temp_suffix[0];
                        suffix[2] = temp_suffix[1];
                        break;
                    case 3: // Copy all three chars across
                        strncpy(suffix,temp_suffix,3);
                        break;
                }
                // Make sure suffix is terminated properly
                suffix[3] = '\0';

// We have our first zone (of this loop) extracted!
//fprintf(stderr,"1Zone:%s%s\n",prefix,suffix);

                // Add it to our zone string.  In this case we know
                // that the lengths of the strings we're working
                // with are quite short.  Little danger of
                // overrunning our destination string.
                strcat(uncompressed_wx,",");
                strcat(uncompressed_wx,prefix); 
                strcat(uncompressed_wx,suffix);
                // Terminate it every time
                uncompressed_wx[9999] = '\0';

                // Here we keep looping until we hit another alpha
                // portion.  We need to look at the field separator
                // to determine whether we have another separate
                // field coming up or a range to enumerate.
                while ( (ptr < (compressed_wx + strlen(compressed_wx)))
                        && ( is_num_chr(ptr[1]) ) ) {

                    // Look for '>' or '-' character.  If former, we
                    // have a numeric sequence to ennumerate.  If the
                    // latter, we either have another zone number or
                    // another prefix coming up.
                    if (ptr[0] == '>') { // Numeric zone sequence
                        int start_number;
                        int end_number;
                        int kk;

                        ptr++;  // Skip past the '>' character

                        // Snag the NUMERIC portion
                        strncpy(ending,ptr,3);  
                        ending[3] = '\0';   // Terminate the string
                        if (is_num_chr(ending[0])) {
                            ptr += 1;
                            if (is_num_chr(ending[1])) {
                                ptr += 1;
                                if (is_num_chr(ending[2])) {
                                    ptr += 1;
                                }
                                else {
                                    ending[2] = '\0';
                                }
                            }
                            else {
                                ending[1] = '\0';
                            }
                        }
                        else {
                            // We have a problem, 'cuz we didn't
                            // find at least one number.  Packet is
                            // badly formatted.
                            return;
                        }
                        
                        // ending should now contain something like
                        // "046" or "35" or "2".
                        start_number = (int)atoi(suffix);
                        end_number = (int)atoi(ending);
                        for ( kk=start_number+1; kk<=end_number; kk++) {
                            xastir_snprintf(suffix,4,"%03d",kk);

//fprintf(stderr,"2Zone:%s%s\n",prefix,temp);

                            // And another zone... Ennumerate
                            // through the sequence, adding each
                            // new zone to our zone string.  In this
                            // case we know that the lengths of the
                            // strings we're working with are quite
                            // short.  Little danger of overrunning
                            // our destination string.
                            strcat(uncompressed_wx,",");
                            strcat(uncompressed_wx,prefix); 
                            strcat(uncompressed_wx,suffix);
                            // Terminate it every time
                            uncompressed_wx[9999] = '\0';
                        }
                    }
                    else if (ptr[0] == '-') {
                        // New zone number, not a numeric sequence.

                        ptr++;  // Skip past the '-' character
                        if ( is_num_chr(ptr[0]) ) {
                            // Found another number.  Use the prefix
                            // stored from last time.

                            // Snag the NUMERIC portion.  Note that the field
                            // width can vary between 1 and 3.  The leading
                            // zeroes have been removed.
                            strncpy(temp_suffix,ptr,3);
                            temp_suffix[3] = '\0';   // Terminate the string
                            if (temp_suffix[1] == '-' || temp_suffix[1] == '>') {
                                temp_suffix[1] = '\0';
                                ptr += 1;
                            }
                            else if (temp_suffix[2] == '-' || temp_suffix[2] == '>') {
                                temp_suffix[2] = '\0';
                                ptr += 2;
                            }
                            else {
                                ptr += 3;
                            }
                            // temp_suffix should now contain something like
                            // "039" or "45" or "2".  Add leading zeroes to give
                            // "suffix" a length of 3.
                            strncpy(suffix,"000",3);
                            switch (strlen(temp_suffix)) {
                                case 1: // Copy one char across
                                    suffix[2] = temp_suffix[0];
                                    break;
                                case 2: // Copy two chars across
                                    suffix[1] = temp_suffix[0];
                                    suffix[2] = temp_suffix[1];
                                                break;
                                case 3: // Copy all three chars across
                                    strncpy(suffix,temp_suffix,3);
                                    break;
                            }
                            // Make sure that suffix is terminated properly
                            suffix[3] = '\0';

//fprintf(stderr,"3Zone:%s%s\n",prefix,suffix);

                            // And another zone...
                            // Add it to our zone string.  In this
                            // case we know that the lengths of the
                            // strings we're working with are quite
                            // short.  Little danger of overrunning
                            // our destination string.
                            strcat(uncompressed_wx,",");
                            strcat(uncompressed_wx,prefix); 
                            strcat(uncompressed_wx,suffix);
                            // Terminate it every time
                            uncompressed_wx[9999] = '\0';
                        }
                        else {  // New prefix (not a number)
                            // Start at the top of the outer loop again
                        }
                    }
                }
                // Skip past '-' character, if any, so that we can
                // get to the next prefix
                // RZG:Added the ptr check, so we don't read a byte off the end
                if ( (ptr < (compressed_wx + strlen(compressed_wx))) && (ptr[0] == '-') ) {
                    ptr++;
                }
            }

            if (debug_level & 2)
                fprintf(stderr,"Uncompressed: %s\n",
                    uncompressed_wx);
        }
/////////////////////////////////////////////////////////////////////
// End of compressed weather alert special code
/////////////////////////////////////////////////////////////////////

        if (debug_level & 2)
            fprintf(stderr,"3\n");

       // Terminate the strings
        entry.activity[20] = entry.alert_tag[20] = '\0';

        // If the expire time is missing, shift fields to the right
        // by one field.  Evidently we can have an alert come across
        // that doesn't have an expire time.  The code shuffles the
        // titles to the next record before fixing up the title and
        // alert_tag for entry.
        if (!isdigit((int)entry.activity[0]) && entry.activity[0] != '-') {

            if (title[0][0] == '\0') {
                // No alerts in this message
            }

            // If it's a trashed packet, we may have entries here
            // that are too long.  Assure that we don't overwrite
            // the strings.
            for (jj = 4; jj > 0; jj--) {
                strncpy(&title[jj][0], &title[jj-1][0], 33);
                title[jj][32] = '\0';   // Force a terminator
            }
 
            strncpy(&title[0][0], entry.alert_tag, 33);
            title[0][32] = '\0';
 
            strncpy(entry.alert_tag, entry.activity, 21);
            entry.alert_tag[20] = '\0';
 
            // Shouldn't we clear out entry.activity in this
            // case???  We've determined it's not a date/time value.
            xastir_snprintf(entry.activity,sizeof(entry.activity),"------z");
            entry.expiration = sec_now() + (24 * 60 * 60);   // Add a day
        }
        else {
            // Compute expiration time_t from zulu time
            entry.expiration = time_from_aprsstring(entry.activity);
        }

        if (debug_level & 2)
            fprintf(stderr,"4\n");

        // Copy the sequence (which contains issue_date_time and
        // message sequence) into the record.
        xastir_snprintf(entry.seq,sizeof(entry.seq),"%s",fill->seq);

        if (debug_level & 2)
            fprintf(stderr,"5\n");

        // Now compute issue_date_time from the first three characters of
        // the sequence number:
        // 0-9   = 0-9
        // 10-35 = A-Z
        // 36-61 = a-z
        // The 3 characters are Day/Hour/Minute of the issue date time in
        // zulu time.
        if (strlen(fill->seq) == 5) {   // Looks ok so far
            // Could add another check to make sure that the first two
            // chars are a digit or a capital letter.
            char c;
            char date_time[10];
            char temp[3];


            date_time[0] = '\0';
            for ( ii = 0; ii < 3; ii++ ) {
                c = fill->seq[ii];   // Snag one character

                if (is_num_chr(c)) {    // Found numeric char
                    temp[0] = '0';
                    temp[1] = c;
                    temp[2] = '\0';
                }

                else if (c >= 'A' && c <= 'Z') {    // Found upper-case letter
                    // Need to take ord(c) - 55 to get the number
                    xastir_snprintf(temp,sizeof(temp),"%02d",(int)c - 55);
                }

                else if (c >= 'a' && c <= 'z') {    // Found lower-case letter
                    // Need to take ord(c) - 61 to get the number
                    xastir_snprintf(temp,sizeof(temp),"%02d",(int)c - 61);
                }

                strncat(date_time,temp,2);  // Concatenate the strings
            }
            strncat(date_time,"z",1);   // Add a 'z' on the end.

            if (debug_level & 2)
                fprintf(stderr,"Seq: %s,\tIssue_time: %s\n",fill->seq,date_time);

            xastir_snprintf(entry.issue_date_time,
                sizeof(entry.issue_date_time),
                "%s",
                date_time);
            //entry.issue_date_time = time_from_aprsstring(date_time);
        } else {
            xastir_snprintf(entry.issue_date_time,
                sizeof(entry.issue_date_time),
                "%s",
                "312359z");
        }
 
        if (debug_level & 2)
            fprintf(stderr,"6\n");

        // flags[0] specifies whether it's onscreen or not
        memset(entry.flags, (int)'?', sizeof(entry.flags));
        p_station = NULL;

        // flags[source] specifies source of the alert DATA_VIA_TNC or
        // DATA_VIA_LOCAL
        if (search_station_name(&p_station,fill->from_call_sign,1))
            entry.flags[source] = p_station->data_via;


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

 
        // Iterate through up to five uncompressed alerts, or
        // through the string of now-uncompressed "compressed"
        // alerts, creating an alert out of each.
        //
        if (compressed_wx_packet) { // Handle compressed packet.
            // Skip the first character of our uncompressed_wx
            // string, as it's a leading comma.  Snag out each
            // string in turn and use that as the title for a
            // weather alert.

            // Feed &uncompressed_wx[1] to split_string to fill in
            // an array with the various zone names.
            split_string(&uncompressed_wx[1],
                title_ptr,
                MAX_SUB_ALERTS);
        }
        else {  // Handle non-compressed packet
            // We have up to five alerts to process.

            // Set up an array of char pointers so that we can use
            // the same code for either compressed or uncompressed
            // weather alerts.
            title_ptr[0] = &title[0][0];
            title_ptr[1] = &title[1][0];
            title_ptr[2] = &title[2][0];
            title_ptr[3] = &title[3][0];
            title_ptr[4] = &title[4][0];
            title_ptr[5] = NULL;    // Make sure we terminate
        }

// We now have all of our titles pointed to by the title_ptr[]
// array.  Either type of alert can be processed identically now.


        // Try to create alerts out of each one.

        for (ii = 0; ii < MAX_SUB_ALERTS && title_ptr[ii]; ii++) {

            // Copy into our entry.title variable
            strncpy(entry.title, title_ptr[ii], sizeof(entry.title));

            // Terminate title string
            entry.title[sizeof(entry.title)-1] = '\0';
//fprintf(stderr,"Title: %s\n",entry.title);

            // This one removes spaces from the title.
            //while ((ptr = strpbrk(entry.title, " ")))
            //    memmove(ptr, ptr+1, strlen(ptr)+1);

            // Instead we should blank out the title and any
            // following alert titles if a space is encountered, as
            // we're to disregard anything after a space in the
            // information field.
            if (ignore_title)   // Blank out title if flag is set
                entry.title[0] = '\0';

            // If we found a space in a title, this signifies that
            // we hit the end of the current list of zones.
            if ( (ptr = strpbrk(entry.title, " ")) ) {
                ignore_title++;     // Set flag for following titles
                entry.title[0] = '\0';  // Blank out title
            }

            if ((ptr = strpbrk(entry.title, "}>=!:/*+;"))) {
                if (debug_level & 2) {
                    fprintf(stderr,
                        "Warning: Weird Weather Message: %ld:%s>%s:%s!\n",
                        (long)fill->sec_heard,
                        fill->from_call_sign,
                        fill->call_sign,
                        fill->message_line);
                }
                *ptr = '\0';
            }

            // Skip loop iterations if we don't have a title for an
            // entry
            if (entry.title[0] == '\0')
                continue;

            strncpy(entry.from, fill->from_call_sign, 10);
            entry.from[9] = '\0';
            strncpy(entry.to, fill->call_sign, 10);
            entry.to[9] = '\0';
            strncpy(entry.seq, fill->seq, 10);
            entry.seq[9] = '\0';

            // NWS_ADVIS or NWS_CANCL normally appear in the "to"
            // field.  ADVIS can appear in the alert_tag field on a
            // CANCL message though, and we want CANCL to have
            // priority.
            if (strstr(entry.alert_tag, "CANCL") || strstr(entry.to, "CANCL"))
                entry.alert_level = 'C';

            else if (!strncmp(entry.alert_tag, "TEST", 4) || strstr(entry.to, "TEST"))
                entry.alert_level = 'T';

            else if (strstr(entry.alert_tag, "WARN") || strstr(entry.to, "WARN"))
                entry.alert_level = 'R';

            else if (strstr(entry.alert_tag, "CIVIL") || strstr(entry.to, "CIVIL"))
                entry.alert_level = 'R';

            else if (strstr(entry.alert_tag, "WATCH") || strstr(entry.to, "WATCH"))
                entry.alert_level = 'Y';

            else if (strstr(entry.alert_tag, "ADVIS") || strstr(entry.to, "ADVIS"))
                entry.alert_level = 'B';

            else
                entry.alert_level = 'G';


            // Kludge for fire zones
            if (!strncmp(entry.alert_tag,"RED_FLAG",8))
            {
                // need to replace "Z" in the zone field with "F"
                if (entry.title[3] == 'Z')
                    entry.title[3] = 'F';
            }
            // Look for a similar alert

// We need some improvements here.  We compare these fields:
//
// from         SFONPW      SFONPW
// to           NWS-ADVIS   NWS-CANCL
// alert_tag    WIND        WIND_ADVIS_CANCEL
// title        CA_Z007     CA_Z007
//
// Of these, "from" and "title" should remain the same between an
// alert and a cancelled alert.  "to" and "alert_tag" change.  Since
// we're comparing all four fields, the cancels don't match any
// existing alerts.

            if ((list_ptr = alert_match(&entry, ALERT_ALL))) {

// We found a match!  We probably need to copy some more data across
// between the records:  seq, alert_tag, alert_level, from, to,
// issue_date_time, expiration?
// If it's a CANCL or CANCEL, we need to make sure the cancel
// packet's information is kept and the other's info is tossed, so
// that the alert doesn't get drawn anymore.

                // If we're not trying to replace a cancelled alert with
                // a new non-cancelled alert, go ahead and copy the
                // fields across.
                if ( (list_ptr->alert_level != 'C') // Stored alert is _not_ a CANCEL
                        || (entry.alert_level == 'C') ) { // Or new one _is_ a CANCEL
                    list_ptr->expiration = entry.expiration;
                    strncpy(list_ptr->activity, entry.activity, 21);
                    list_ptr->activity[20] = '\0';
                    strncpy(list_ptr->alert_tag, entry.alert_tag, 21);
                    list_ptr->alert_tag[20] = '\0';
                    list_ptr->alert_level = entry.alert_level;
                    strncpy(list_ptr->seq, entry.seq, 10);
                    list_ptr->seq[9] = '\0';
                    strncpy(list_ptr->from, entry.from, 10);
                    list_ptr->from[9] = '\0';
                    strncpy(list_ptr->to, entry.to, 10);
                    list_ptr->to[9] = '\0';
                    strncpy(list_ptr->issue_date_time, entry.issue_date_time, 10);
                    list_ptr->issue_date_time[9] = '\0';
                }
                else {
                    // Don't copy the info across, as we'd be making a
                    // cancelled alert active again if we did.
                }
            } else {    // No similar alert, add a new one to the list
                entry.index = -1;    // Haven't found it in a file yet
                (void)alert_add_entry(&entry);
            }

            if (alert_active(&entry, ALERT_ALL)) {
                // Empty "if" body here?????  LCLINT caught this.
            }

        }   // End of for loop


        // Signify that we're done processing the NWS message
        fill->active = RECORD_CLOSED;
    }

    if (debug_level & 2)
        fprintf(stderr,"alert_build_list exit 1\n");
}


