/* -*- c-basic-indent: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2002  The Xastir Group
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



// We may want to call or schedule somehow that refresh_image gets
// called, so that new alerts will show up in a timely manner.  Same for
// expiring alerts:  Need to refresh the map.
// alert_redraw_on_update will cause refresh_image to get called.
// alert_add_entry sets it.  Expiring an alert should set it as well.


//
// Changes Dale Huguely would like to see:
// Shapefile weather alerts.
// What are the ? and - at the beginning?  Also there seems to be no
// parsing of the data such as issue time - expiration time or who
// sent it.  Need to at least be able to see the stuff after the
// curly brace on the screen.  Need the first 6 chars from the "from
// Call" and the first 3 chars after the curly brace for a new query
// operation that Dale has planned.
//


//
// In the alert structure, flags[] is size 16.  Only the first two
// positions in the array are currently used.
//
// alert_entry.flags[0]
//          ?  Initial state or ready-to-recompute state
//          -   Expired between 1 sec and 1 hour
//          Y   Active alert within viewport
//          N   Active alert outside viewport
//
// alert_entry.flags[1]
//          DATA_VIA_TNC
//          DATA_VIA_LOCAL
//
// alert_entry.alert_tag    alert_entry.alert_level
//         CANCL                C
//         TEST                 T
//         WARN                 R
//         WATCH                Y
//         ADVIS                B
//         Other                G
//         Unset                ?
//
//
//
// Global alert_tag string contains three characters for each alert.
// These contain the first two characters of the title, and then 0
// or 1 for the source of the alert (DATA_VIA_TNC or
// DATA_VIA_LOCAL).  The first two characters are used to add on to
// the end of the path ("/usr/local/xastir/Counties/") to come up
// with the State subdirectory to search for this map file
// ("/usr/local/xastir/Counties/MS").  See maps.c:load_alert_maps().
//
// When converting to using Shapefile maps, we could either save the
// shapefile designator in this same global alert_tag string, or we
// could compute it on the fly from the data.  Perhaps we could
// compute it on the fly if the variable in the struct was empty,
// then fill it in after the first search so that we didn't have to
// look it up again for later display.
//
// Stuff from Dale, paraphrased by Curt:
//
// The clue to which shapefile to use is in the 4th char (which
// is the first following an '_')
//
// ICTSVS>APRS::NWS-ADVIS:120145z,SEVERE_WEATHER,KS_Z091, {C14AA
// TSASVR>APRS::NWS-WARN :120230z,SVRTSM,OK_C113,  OSAGE COUNTY {C16AA
//
// C = county file (c_??????)
// A = County Warning File (w_?????)
// Z = Zone File (z_????? or mz_??????)
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
// LZKNPW>APRS::NWS-ADVIS:221000z,ARZ003>007-012>016-021> ;025-030>034-037>047-052>057-062>069{LLSAA
//
// The trick is to step thru the data base contained in the
// shapefiles to determine what areas to light up.  In the above
// example read ">" as "thru" and "-" as "and".
//
// More from Dale:
// It occurs to me you might need some insight into what shapefile to
// look
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
// I am running out of time - Need to meet Gerry at the EOC in 30 min- but 
// that should be all there is to it- I will send you a complete list of 
// marine zones later today- I think there are no more than 13 or 14 to 
// search through - better that the 54 "states"- could be hard coded. 


// Found on the AWIPS web pages so far:
// AWIPS Counties          C
// County Warning Areas    A
// Zone Forecast Areas     Z
// Coastal Marine Zones    Z
// Offshore Marine Zones   Z
// High Seas Marine Zones  Z    (Says "Not for operational use")
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
#endif
#include <stdlib.h>
#include <stdio.h>

#include <assert.h>
#include <ctype.h>

#ifdef  HAVE_LOCALE_H
#include <locale.h>
#endif

#ifdef  HAVE_LIBINTL_H
#include <libintl.h>
#define _(x)        gettext(x)
#else
#define _(x)        (x)
#endif

#include <Xm/XmAll.h>

#include "alert.h"
#include "xastir.h"
#include "util.h"


alert_entry *alert_list = NULL;
int alert_list_count = 0;
static int alert_max_count = 0;
char *alert_tag = NULL;
static int alert_tag_size = 0;
int alert_redraw_on_update = 0;





//
// normal_title()
//
// Function to convert "County Warning Area" to "CWA" in a string.
// Called from alert_match() and alert_update_list() functions.
//
void normal_title(char *incoming_title, char *outgoing_title) {
    char *c_ptr;


    if (debug_level & 1)
        printf("normal_title: Incoming: %s\n",incoming_title);

    strncpy(outgoing_title, incoming_title, 32);
    outgoing_title[32] = '\0';
    if ((c_ptr = strstr(outgoing_title, "County Warning Area ")) && c_ptr == outgoing_title) {
        c_ptr = &outgoing_title[strlen("County Warning Area ")]; // Find end of text
        strcpy(outgoing_title, "CWA");  // Add "CWA" to output string instead
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

    if (debug_level & 1)
        printf("normal_title: Outgoing: %s\n",outgoing_title);
}





//
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

    printf("Alert counts: %d/%d\n", alert_list_count, alert_max_count);
    for (i = 0; i < alert_list_count; i++) {
        strncpy(title, alert_list[i].title, 99);
        title[99] = '\0';
        for (c_ptr = &title[strlen(title)-1]; *c_ptr == ' '; c_ptr--)
            *c_ptr = '\0';

        printf("Alert:%4d%c,%9s>%9s, Tag: %c%20s, Activity: %9s, Expiration: %lu, Title: %s\n", i,
                alert_list[i].flags[0], alert_list[i].from,
                alert_list[i].to, alert_list[i].alert_level,
                alert_list[i].alert_tag, alert_list[i].activity,
                (unsigned long)(alert_list[i].expiration), title);
    }
}





//
// alert_add_entry()
//
// This function adds a new alert to our alert list.
// Returns address of entry in alert_list or NULL.
// Called from alert_build_list() function.
//
/*@null@*/ static alert_entry *alert_add_entry(alert_entry *entry) {
    alert_entry *ptr;
    int i;


    if (debug_level & 1)
        printf("alert_add_entry\n");

    // Skip NWS_SOLAR and -NoActivationExpected alerts, they don't
    // interest us.
    if (strcmp(entry->to, "NWS-SOLAR") == 0) {
printf("NWS-SOLAR, skipping\n");
        return (NULL);
    }
    if (strcasecmp(entry->title, "-NoActivationExpected") == 0) {
printf("NoActivationExpected, skipping\n");
        return (NULL);
    }

    // Allocate more space if we're at our maximum already.
    // Allocate space for 10 more alerts.
    if (alert_list_count == alert_max_count) {
        ptr = realloc(alert_list, (alert_max_count+10)*sizeof(alert_entry));
        if (ptr) {
            alert_list = ptr;
            alert_max_count += 10;
        }
    }

if (entry->expiration < time(NULL))
printf("Expired, current: %lu, alert: %lu\n", time(NULL), entry->expiration );

    // Check for non-zero alert title, non-expired alert time
    if (entry->title[0] != '\0' && entry->expiration >= time(NULL)) {

        // Schedule a screen update 'cuz we have a new alert
        alert_redraw_on_update = redraw_on_new_data = 1;

        // Scan for an empty entry, fill it in if found
        for (i = 0; i < alert_list_count; i++) {
            if (alert_list[i].title[0] == '\0') {   // If alert entry is empty
                memcpy(&alert_list[i], entry, sizeof(alert_entry)); // Use it
printf("Found empty entry, filling it\n");
                return ( &alert_list[i]);
            }
        }

        // Else fill in the entry at the end and bump up the count
        if (alert_list_count < alert_max_count) {
printf("Adding new entry\n");
            memcpy(&alert_list[alert_list_count], entry, sizeof(alert_entry));
            return (&alert_list[alert_list_count++]);
        }
    }

    // The title was empty or the alert has already expired
printf("Title empty or alert expired, skipping\n");
    return (NULL);
}





//
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


    if (debug_level & 1)
        printf("alert_match\n");
 
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

    for (i = 0; i < alert_list_count; i++) {

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

        if ((match_level < ALERT_FROM || strcmp(alert_list[i].from, alert->from) == 0) &&
                (match_level < ALERT_TO   || strcasecmp(alert_list[i].to, alert->to) == 0) &&
                (match_level < ALERT_TAG  || strcmp(alert_list[i].alert_tag, alert->alert_tag) == 0) &&
                (title_m[0] && (strncasecmp(title_e, title_m, strlen(title_m)) == 0 ||
                strcasecmp(title_m, filename) == 0 || strcasecmp(alert_f, title_e) == 0 ||
                (alert_f[0] && filename[0] && strcasecmp(alert_f, filename) == 0)))) {
            return (&alert_list[i]);
        }
    }
    return (NULL);
}





//
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


    if (debug_level & 1)
        printf("alert_update_list\n");

    // Find the matching alert in alert_list, copy updated
    // parameters from new alert into existing alert_list entry.
    if ((ptr = alert_match(alert, match_level))) {
        if (!ptr->filename[0]) {    // We found a match!  Fill it in.
            strncpy(ptr->filename, alert->filename, 32);
            strncpy(ptr->title, alert->title, 32);
            ptr->filename[32] = ptr->title[32] = '\0';
            ptr->top_boundary = alert->top_boundary;
            ptr->left_boundary = alert->left_boundary;
            ptr->bottom_boundary = alert->bottom_boundary;
            ptr->right_boundary = alert->right_boundary;
        }
        ptr->flags[0] = alert->flags[0];

        // Shorten title
        normal_title(alert->title, title_e);

        // Force the string to be terminated
        title_e[32] = title_m[32] = '\0';

        // Interate through the entire alert_list, checking flags
        for (i = 0; i < alert_list_count; i++) {

            // If flag was '?' or has changed, update the alert
            if ((alert_list[i].flags[0] == '?' || alert_list[i].flags[0] != ptr->flags[0])) {

                // Shorten the title.  Title_m will be the shortened
                // title.
                normal_title(alert_list[i].title, title_m);

                if (strcmp(title_e, title_m) == 0) {

                    // Update parameters
                    if (!alert_list[i].filename[0]) {
                        strncpy(alert_list[i].filename, alert->filename, 32);
                        alert_list[i].filename[32] = '\0';
                        alert_list[i].top_boundary = alert->top_boundary;
                        alert_list[i].left_boundary = alert->left_boundary;
                        alert_list[i].bottom_boundary = alert->bottom_boundary;
                        alert_list[i].right_boundary = alert->right_boundary;
                        strncpy(alert_list[i].title, alert->title, 32);
                        alert_list[i].title[32] = '\0';
                    }
                    alert_list[i].flags[0] = alert->flags[0];
                }
            }
        }
    }
}





//
// alert_active()
//
// Here's where we get rid of expired alerts in the list.
// Called from alert_compare(), alert_display_request(),
// alert_on_screen(), alert_build_list(), and alert_message_scan()
// functions.  Also called from maps.c:load_alert_maps() function.
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


    if (debug_level & 1)
        printf("alert_active\n");

    (void)time(&now);

    if ((a_ptr = alert_match(alert, match_level))) {
        if (a_ptr->expiration >= now) {
            for (level = 0; a_ptr->alert_level != l_list[level] && level < (int)sizeof(l_list); level++);
        }
        else if (a_ptr->expiration < (now - 3600)) {    // More than an hour past the expiration,
            a_ptr->title[0] = '\0';                     // so delete it from list by clearing
                                                        // out the title.
        }
        else if (a_ptr->flags[0] == '?') {  // Expired between 1sec and 1hr and found '?'
            a_ptr->flags[0] = '-';
 
            // Schedule a screen update 'cuz we have an expired alert
            alert_redraw_on_update = redraw_on_new_data = 1;
        }
    }
    return (level);
}





//
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

    if (a_entry->flags[0] == 'Y' && b_entry->flags[0] != 'Y')
        return (-1);

    if (a_entry->flags[0] != 'Y' && b_entry->flags[0] == 'Y')
        return (1);

    if (a_entry->flags[0] == '?' && b_entry->flags[0] == 'N')
        return (-1);

    if (a_entry->flags[0] == 'N' && b_entry->flags[0] == '?')
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





//
// alert_sort_active()
//
// This sorts the alert_list so that the active items are at the
// beginning.
// Called from maps.c:load_alert_maps() function.
//
void alert_sort_active(void) {


    if (debug_level & 1)
        printf("alert_sort_active\n");

    qsort(alert_list, (size_t)alert_list_count, sizeof(alert_entry), alert_compare);
    for (; alert_list_count && alert_list[alert_list_count-1].title[0] == '\0'; alert_list_count--);
}





//
// alert_display_request()
//
// Function which checks whether an alert should be displayed.
// Called from maps.c:load_alert_maps() function.
//
int alert_display_request(void) {
    int i, alert_count;
    static int last_alert_count;


    if (debug_level & 1)
        printf("alert_display_request\n");

    // Iterate through entire alert_list
    for (i = 0, alert_count = 0; i < alert_list_count; i++) {

        // If it's an active alert (not expired), and flags == 'Y'
        // (meaning it is within our viewport), set the flag to '?'.
        if (alert_active(&alert_list[i], ALERT_ALL) && (alert_list[i].flags[0] == 'Y' ||
                alert_list[i].flags[0] == '?')) {
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





//
// alert_on_screen()
//
// Returns a count of active weather alerts in the list which are
// within our viewport.
// Called from main.c:UpdateTime() function.
//
int alert_on_screen(void) {
    int i, alert_count;


    if (debug_level & 1)
        printf("alert_on_screen\n");

    for (i = 0, alert_count = 0; i < alert_list_count; i++) {
        if (alert_active(&alert_list[i], ALERT_ALL)
                && alert_list[i].flags[0] == 'Y') {
            alert_count++;
        }
    }

    return (alert_count);
}





//
// alert_build_list()
//
// This function builds alert_entry structs from message entries that
// contain NWS alert messages.
//
// Called from alert_message_scan() function.
//
// The original form is this:
//
//     :NWS-WARN :092010z,THUNDER_STORM,AR_ASHLEY,{S9JbA
//      activity            alert_tag   title (may be up to 5 more)
//
// The state or NWS area name is the AR above (in "AR_ASHLEY").
// We file the AR_ASHLEY map under /usr/local/xastir/Counties/AR/
//
// Here are some real examples captured over the 'net:
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
//              activity          alert_tag     title   title   title   title   title
//
static void alert_build_list(Message *fill) {
    alert_entry entry[6], *list_ptr;    // We might need up to six structs to split
                                        // up a message into individual map areas.
    int i, j;
    char *ptr;
    DataRow *p_station;


    if (debug_level & 1)
        printf("alert_build_list\n");

    if (fill->active == RECORD_ACTIVE) {
        memset(entry, 0, sizeof(entry));
        (void)sscanf(fill->message_line, "%20[^,],%20[^,],%32[^,],%32[^,],%32[^,],%32[^,],%32[^,],%32[^,]",
           entry[0].activity, entry[0].alert_tag, entry[0].title, entry[1].title,
           entry[2].title, entry[3].title, entry[4].title, entry[5].title);

        entry[0].activity[20] = entry[0].alert_tag[20] = '\0';

        if (!isdigit((int)entry[0].activity[0]) && entry[0].activity[0] != '-') {
            for (j = 5; j >= 0; j--) {
                strcpy(entry[j].title, entry[j-1].title);
            }
            strcpy(entry[0].title, entry[0].alert_tag);
            strcpy(entry[0].alert_tag, entry[0].activity);
        }

        entry[0].expiration = time_from_aprsstring(entry[0].activity);
 
        // flags[0] specifies whether it's onscreen or not
        memset(entry[0].flags, (int)'?', sizeof(entry[0].flags));
        p_station = NULL;

        // flags[1] specifies source of the alert DATA_VIA_TNC or
        // DATA_VIA_LOCAL
        if (search_station_name(&p_station,fill->from_call_sign,1))
            entry[0].flags[1] = p_station->data_via;


        // Set up each of up to six structs with data
        for (i = 0; i < 6 && entry[i].title[0]; i++) {

            // Terminate title string for each of six structs
            entry[i].title[32] = '\0';
printf("Title: %s\n",entry[i].title);
 
            while ((ptr = strpbrk(entry[i].title, " ")))
                memmove(ptr, ptr+1, strlen(ptr)+1);

            if ((ptr = strpbrk(entry[i].title, "}>=!:/*+;"))) {
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

            if (entry[i].title[0] == '\0')
                continue;

            // Fill in other struct data from first struct, except
            // for title.
            strcpy(entry[i].activity, entry[0].activity);
            strcpy(entry[i].alert_tag, entry[0].alert_tag);
            strcpy(entry[i].from, fill->from_call_sign);
            strcpy(entry[i].to, fill->call_sign);
            entry[i].expiration = entry[0].expiration;
            memcpy(entry[i].flags, entry[0].flags, sizeof(entry[0].flags));

            if (strstr(entry[i].alert_tag, "CANCL") || strstr(entry[i].to, "CANCL"))
                entry[i].alert_level = 'C';

            else if (!strncmp(entry[i].alert_tag, "TEST", 4) || strstr(entry[i].to, "TEST"))
                entry[i].alert_level = 'T';

            else if (strstr(entry[i].alert_tag, "WARN") || strstr(entry[i].to, "WARN"))
                entry[i].alert_level = 'R';

            else if (strstr(entry[i].alert_tag, "WATCH") || strstr(entry[i].to, "WATCH"))
                entry[i].alert_level = 'Y';

            else if (strstr(entry[i].alert_tag, "ADVIS") || strstr(entry[i].to, "ADVIS"))
                entry[i].alert_level = 'B';

            else
                entry[i].alert_level = 'G';

            if ((list_ptr = alert_match(&entry[i], ALERT_ALL))) {
                list_ptr->expiration = entry[i].expiration;
                strcpy(list_ptr->activity, entry[i].activity);
            } else {
                (void)alert_add_entry(&entry[i]);
            }

            if (alert_active(&entry[i], ALERT_ALL)) {
                // Empty "if" body here?????  LCLINT caught this.
            }
        }
        fill->active = RECORD_CLOSED;
    }
}





//
// alert_message_scan()
//
// This function scans the message list to find new alerts.  It adds
// non-expired alerts to our alert list via the alert_build_list()
// function.  It also builds the alert_tag string which contains
// three characters per alert, and the on-screen status of each.
//
// Called from db.c:decode_message() function when a new alert is
// received, and from maps.c:load_alert_maps() function.
//
// It returns the string length of the global alert_tag variable.
// Divide this by 3 and we know the number of alerts that we have.
//
int alert_message_scan(void) {
    char *a_ptr;
    int i, j;


    if (debug_level & 1)
        printf("alert_message_scan\n");

    // This is the shorthand way we keep track of which state's we just got alerts for.
    // Looks like "CO?MO?ID?". The 3rd position is to indicate if the alert came via local or
    // from the TNC. If there is an unresolved alert then a message is sent to console.
    // This is a text string with 3 chars per alert.  We allocate 21
    // chars here initially, enough for seven alerts.
    if (!alert_tag) {
        alert_tag = malloc(21);
        alert_tag_size = 21;
    }

    // Mark active/inactive alerts as such
    for (i = 0; i < alert_list_count; i++)
        (void)alert_active(&alert_list[i], ALERT_ALL);

    // Scan the message list for alerts, add new ones to our alert list.
    // This calls alert_build_list() function for each message found.
    mscan_file(MESSAGE_NWS, alert_build_list);

    // Blank out the alert tag
    *alert_tag = '\0';

    // Rebuild the alert tag string for the current alerts
    for (j = 0; j < alert_list_count; j++) {

        if (alert_list[j].flags[0] == '?') {    // On-screen status not known yet

            // Look through global alert_tag string looking for a
            // match.
            for (i = 0; i < (int)strlen(alert_tag); i += 3) {
                // If first 2 chars of title match
                if (strncmp(&alert_tag[i], alert_list[j].title, 2) == 0) {
                    if (alert_list[j].flags[1]==DATA_VIA_TNC || alert_list[j].flags[1]==DATA_VIA_LOCAL) {
                        // Set the 3rd char to DATA_VIA_TNC or DATA_VIA_LOCAL
                        alert_tag[i+2] = alert_list[j].flags[1];
                    }
                    break;
                }
            }

            // If global alert_tag string isn't long enough,
            // allocate some more space.
            if (i == (int)strlen(alert_tag)) {
                if (i+4 >= alert_tag_size) {
                    a_ptr = realloc(alert_tag, (size_t)(alert_tag_size+21) );
                    if (a_ptr) {
                        alert_tag = a_ptr;
                        alert_tag_size += 21;
                    }
                }
                // Add new 3-character string to end of global alert_tag
                if (i+4 < alert_tag_size) {
                    a_ptr = &alert_tag[i];
                    if (alert_list[j].title[0] && alert_list[j].title[1]) {
                        *a_ptr++ = alert_list[j].title[0];
                        *a_ptr++ = alert_list[j].title[1];
                        *a_ptr++ = alert_list[j].flags[1];
                    }
                    // Terminate the alert_tag string
                    *a_ptr = '\0';
                }
            }
        }
    }
    return ( (int)strlen(alert_tag) );
}


