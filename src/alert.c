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
// Function to convert "County Warning Area" text to "CWA"
// Called from alert_match() and alert_update_list() functions.
//
void normal_title(char *incoming_title, char *outgoing_title) {
    char *c_ptr;

    strncpy(outgoing_title, incoming_title, 32);
    outgoing_title[32] = '\0';
    if ((c_ptr = strstr(outgoing_title, "County Warning Area ")) && c_ptr == outgoing_title) {
        c_ptr = &outgoing_title[strlen("County Warning Area ")];
        strcpy(outgoing_title, "CWA");
        strncat(outgoing_title, c_ptr, 32-3); // total max length - strlen("CWA")
        outgoing_title[32] = '\0';
    }
    while ((c_ptr = strstr(outgoing_title, ". ")))
        memmove(c_ptr, c_ptr+2, strlen(c_ptr)+1);

    if ((c_ptr = strpbrk(outgoing_title, " >")))
        *c_ptr = '\0';

    while ((c_ptr = strpbrk(outgoing_title, "_.-}!")))
        memmove(c_ptr, c_ptr+1, strlen(c_ptr)+1);

    outgoing_title[8] = '\0';
}





//
// Debug routine.  Currently attached to the Test() function in
// main.c, but the button in the file menu is normally grey'ed out.
// This function prints the weather alerts to the xterm.
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
                alert_list[i].flags[0], alert_list[i].from, alert_list[i].to, alert_list[i].alert_level,
                alert_list[i].alert_tag, alert_list[i].activity, (unsigned long)(alert_list[i].expiration), title);
    }
}





//
// Called from alert_build_list()
// This function add a new alert to the list.
//
/*@null@*/ static alert_entry *alert_add_entry(alert_entry *entry) {
    alert_entry *ptr;
    int i;

    if (strcmp(entry->to, "NWS-SOLAR") == 0)
        return (NULL);
    if (strcasecmp(entry->title, "-NoActivationExpected") == 0)
        return (NULL);

    if (alert_list_count == alert_max_count) {
        ptr = realloc(alert_list, (alert_max_count+10)*sizeof(alert_entry));
        if (ptr) {
            alert_list = ptr;
            alert_max_count += 10;
        }
    }
    if (entry->title[0] != '\0' && entry->expiration >= time(NULL)) {

        // Schedule a screen update 'cuz we have a new alert
        alert_redraw_on_update = redraw_on_new_data = 1;

        for (i = 0; i < alert_list_count; i++)
        if (alert_list[i].title[0] == '\0') {
            memcpy(&alert_list[i], entry, sizeof(alert_entry));
            return ( &alert_list[i]);
        }
        if (alert_list_count < alert_max_count) {
            memcpy(&alert_list[alert_list_count], entry, sizeof(alert_entry));
            return (&alert_list[alert_list_count++]);
        }
    }
    return (NULL);
}





//
// Called from alert_build_list(), alert_update_list(), and
// alert_active().
//
static alert_entry *alert_match(alert_entry *alert, alert_match_level match_level) {
    int i;
    char *ptr, title_e[33], title_m[33], alert_f[33], filename[33];

    normal_title(alert->title, title_e);
    strncpy(filename, alert->filename, 32);
    title_e[32] = title_m[32] = alert_f[32] = filename[32] = '\0';
    if ((ptr = strpbrk(filename, ".")))
        *ptr = '\0';

    if ((ptr = strrchr(filename, '/'))) {
        ptr++;
        memmove(filename, ptr, strlen(ptr)+1);
    }
    while ((ptr = strpbrk(filename, "_ -")))
        memmove(ptr, ptr+1, strlen(ptr)+1);

    for (i = 0; i < alert_list_count; i++) {
        normal_title(alert_list[i].title, title_m);
        strncpy(alert_f, alert_list[i].filename, 32);
        alert_f[32] = '\0';
        if ((ptr = strpbrk(alert_f, ".")))
            *ptr = '\0';

        if ((ptr = strrchr(alert_f, '/'))) {
            ptr++;
            memmove(alert_f, ptr, strlen(ptr)+1);
        }
        while ((ptr = strpbrk(alert_f, "_ -")))
            memmove(ptr, ptr+1, strlen(ptr)+1);

        if ((match_level < ALERT_FROM || strcmp(alert_list[i].from, alert->from) == 0) &&
        (match_level < ALERT_TO   || strcasecmp(alert_list[i].to, alert->to) == 0) &&
        (match_level < ALERT_TAG  || strcmp(alert_list[i].alert_tag, alert->alert_tag) == 0) &&
        (title_m[0] && (strncasecmp(title_e, title_m, strlen(title_m)) == 0 ||
                strcasecmp(title_m, filename) == 0 || strcasecmp(alert_f, title_e) == 0 ||
                (alert_f[0] && filename[0] && strcasecmp(alert_f, filename) == 0))))

            return (&alert_list[i]);
    }
    return (NULL);
}





//
// Called from maps.c:load_alert_maps() (both copies of it, compiled
// in with different map support).
//
void alert_update_list(alert_entry *alert, alert_match_level match_level) {
    alert_entry *ptr;
    int i;
    char title_e[33], title_m[33];

    if ((ptr = alert_match(alert, match_level))) {
        if (!ptr->filename[0]) {
            strncpy(ptr->filename, alert->filename, 32);
            strncpy(ptr->title, alert->title, 32);
            ptr->filename[32] = ptr->title[32] = '\0';
            ptr->top_boundary = alert->top_boundary;
            ptr->left_boundary = alert->left_boundary;
            ptr->bottom_boundary = alert->bottom_boundary;
            ptr->right_boundary = alert->right_boundary;
        }
        ptr->flags[0] = alert->flags[0];
        normal_title(alert->title, title_e);
        title_e[32] = title_m[32] = '\0';
        for (i = 0; i < alert_list_count; i++) {
            if ((alert_list[i].flags[0] == '?' || alert_list[i].flags[0] != ptr->flags[0])) {
                normal_title(alert_list[i].title, title_m);
                if (strcmp(title_e, title_m) == 0) {
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
// Here's where we mark the expired alerts in the list.
// Called from alert_compare(), alert_display_request(),
// alert_on_screen(), alert_build_list(), alert_message_scan().
// Also called from maps.c:load_alert_maps() functions (both of
// them).
//
int alert_active(alert_entry *alert, alert_match_level match_level) {
    alert_entry *a_ptr;
    char l_list[] = {"?RYBTGC"};
    int level = 0;
    time_t now;

    (void)time(&now);
    if ((a_ptr = alert_match(alert, match_level))) {
        if (a_ptr->expiration >= now)
            for (level = 0; a_ptr->alert_level != l_list[level] && level < (int)sizeof(l_list); level++);

        else if (a_ptr->expiration < (now - 3600)) {    // More than an hour past the expiration,
            a_ptr->title[0] = '\0';                     // so delete it from list.
        }
        else if (a_ptr->flags[0] == '?')
            a_ptr->flags[0] = '-';
    }
    return (level);
}





//
// Used in qsort as the compare function in alert_sort_active()
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
        if (a_active - b_active)
            return (a_active - b_active);
    } else if (a_active)
        return (-1);
    else if (b_active)
        return (1);

    return (strcmp(a_entry->title, b_entry->title));
}





//
// Called from maps.c:load_alert_maps() functions (both of them).
//
void alert_sort_active(void) {
    qsort(alert_list, (size_t)alert_list_count, sizeof(alert_entry), alert_compare);
    for (; alert_list_count && alert_list[alert_list_count-1].title[0] == '\0'; alert_list_count--);
}





//
// Called from maps.c:load_alert_maps() functions (both of them).
//
int alert_display_request(void) {
    int i, alert_count;
    static int last_alert_count;

    for (i = 0, alert_count = 0; i < alert_list_count; i++)
        if (alert_active(&alert_list[i], ALERT_ALL) && (alert_list[i].flags[0] == 'Y' ||
                alert_list[i].flags[0] == '?'))
            alert_count++;

    if (alert_count != last_alert_count) {
        last_alert_count = alert_count;
        return ((int)TRUE);
    }

    return ((int)FALSE);
}





//
// Called from main.c:UpdateTime() function.
// Returns a count of active weather alerts in the list
//
int alert_on_screen(void) {
    int i, alert_count;

    for (i = 0, alert_count = 0; i < alert_list_count; i++)
        if (alert_active(&alert_list[i], ALERT_ALL) && alert_list[i].flags[0] == 'Y')
            alert_count++;

    return (alert_count);
}





//
// Called from alert_message_scan().
//
// This function builds alert_entry structs from message entries that
// contain NWS alert messages.  The original form is this:
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

    if (fill->active == RECORD_ACTIVE) {
    memset(entry, 0, sizeof(entry));
    (void)sscanf(fill->message_line, "%20[^,],%20[^,],%32[^,],%32[^,],%32[^,],%32[^,],%32[^,],%32[^,]",
           entry[0].activity, entry[0].alert_tag, entry[0].title, entry[1].title,
           entry[2].title, entry[3].title, entry[4].title, entry[5].title);

    entry[0].activity[20] = entry[0].alert_tag[20] = '\0';
    if (!isdigit((int)entry[0].activity[0]) && entry[0].activity[0] != '-') {
        for (j = 5; j >= 0; j--)
          strcpy(entry[j].title, entry[j-1].title);
        strcpy(entry[0].title, entry[0].alert_tag);
        strcpy(entry[0].alert_tag, entry[0].activity);
    }
    entry[0].expiration = time_from_aprsstring(entry[0].activity);
    memset(entry[0].flags, (int)'?', sizeof(entry[0].flags));
    p_station = NULL;
    if (search_station_name(&p_station,fill->from_call_sign,1))
        entry[0].flags[1] = p_station->data_via;

    for (i = 0; i < 6 && entry[i].title[0]; i++) {
        entry[i].title[32] = '\0';
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
        } else
            (void)alert_add_entry(&entry[i]);

        if (alert_active(&entry[i], ALERT_ALL)) {
            // Empty "if" body here?????  LCLINT caught this.
        }
    }
    fill->active = RECORD_CLOSED;
    }
}





//
// Called from db.c:decode_message() function when a new alert is
// received, and from maps.c:load_alert_maps() functions (both of
// them).
//
int alert_message_scan(void) {
    char *a_ptr;
    int i, j;

    // This is the shorthand way we keep track of which state's we just got alerts for.
    // Looks like "CO?MO?ID?". The 3rd position is to indicate if the alert came via local or
    // from the TNC. If there is an unresolved alert then a message is sent to console.
    if (!alert_tag) {
        alert_tag = malloc(21);
        alert_tag_size = 21;
    }

    for (i = 0; i < alert_list_count; i++)
        (void)alert_active(&alert_list[i], ALERT_ALL);

    mscan_file(MESSAGE_NWS, alert_build_list);
    *alert_tag = '\0';
    for (j = 0; j < alert_list_count; j++) {
        if (alert_list[j].flags[0] == '?') {
            for (i = 0; i < (int)strlen(alert_tag); i += 3) {
                if (strncmp(&alert_tag[i], alert_list[j].title, 2) == 0) {
                    if (alert_list[j].flags[1]==DATA_VIA_TNC || alert_list[j].flags[1]==DATA_VIA_LOCAL)
                        alert_tag[i+2] = alert_list[j].flags[1];

                    break;
                }
            }
            if (i == (int)strlen(alert_tag)) {
                if (i+4 >= alert_tag_size) {
                    a_ptr = realloc(alert_tag, (size_t)(alert_tag_size+21) );
                    if (a_ptr) {
                        alert_tag = a_ptr;
                        alert_tag_size += 21;
                    }
                }
                if (i+4 < alert_tag_size) {
                    a_ptr = &alert_tag[i];
                    if (alert_list[j].title[0] && alert_list[j].title[1]) {
                        *a_ptr++ = alert_list[j].title[0];
                        *a_ptr++ = alert_list[j].title[1];
                        *a_ptr++ = alert_list[j].flags[1];
                    }
                    *a_ptr = '\0';
                }
            }
        }
    }
    return ( (int)strlen(alert_tag) );
}


