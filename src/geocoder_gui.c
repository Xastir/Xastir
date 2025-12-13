/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
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

#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "lang.h"
#include "db_gui.h"
#include "mutex_utils.h"
#include "geocoder.h"
#include "maps.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"

extern XmFontList fontlist1;    // Menu/System fontlist
static Atom delw;

// Dialog widgets
static Widget geocoder_dialog = (Widget)NULL;
static Widget query_text = (Widget)NULL;
static Widget country_combo = (Widget)NULL;
static Widget country_custom_text = (Widget)NULL;
static Widget results_list = (Widget)NULL;
static Widget status_label = (Widget)NULL;
static Widget attribution_label = (Widget)NULL;
static Widget goto_button = (Widget)NULL;
static Widget mark_button = (Widget)NULL;

// Dialog state
static xastir_mutex geocoder_dialog_lock;
static struct geocode_result_list current_results;

// Destination marking
long destination_coord_lat = 0;
long destination_coord_lon = 0;
int mark_destination = 0;
int show_destination_mark = 1;  // This used to be a checkbox in the GUI, now we handle it as a button

// Country code mapping (ISO 3166-1 alpha-2)
struct country_option {
    const char *label;
    const char *value;
};

static const struct country_option country_options[] = {
    {"None (Worldwide)", ""},
    {"United States", "us"},
    {"United Kingdom", "gb"},
    {"Canada", "ca"},
    {"Australia", "au"},
    {"Germany", "de"},
    {"France", "fr"},
    {"Spain", "es"},
    {"Italy", "it"},
    {"Netherlands", "nl"},
    {"Belgium", "be"},
    {"Switzerland", "ch"},
    {"Austria", "at"},
    {"Sweden", "se"},
    {"Norway", "no"},
    {"Denmark", "dk"},
    {"Finland", "fi"},
    {"Poland", "pl"},
    {"Czech Republic", "cz"},
    {"Japan", "jp"},
    {"China", "cn"},
    {"South Korea", "kr"},
    {"India", "in"},
    {"Brazil", "br"},
    {"Argentina", "ar"},
    {"Mexico", "mx"},
    {"Chile", "cl"},
    {"New Zealand", "nz"},
    {"South Africa", "za"},
    {"Russia", "ru"},
    {"Ukraine", "ua"},
    {"Turkey", "tr"},
    {"Greece", "gr"},
    {"Portugal", "pt"},
    {"Ireland", "ie"},
    {"Israel", "il"},
    {"Thailand", "th"},
    {"Indonesia", "id"},
    {"Philippines", "ph"},
    {"Singapore", "sg"},
    {"Malaysia", "my"},
    {"--- Custom Code ---", "custom"},  // Special marker for custom input
    {NULL, NULL}
};


void geocoder_gui_init(void)
{
    init_critical_section(&geocoder_dialog_lock);
    current_results.results = NULL;
    current_results.count = 0;
    current_results.capacity = 0;
}


static void free_current_results(void)
{
    if (current_results.results) {
        geocode_free_results(&current_results);
        current_results.results = NULL;
        current_results.count = 0;
        current_results.capacity = 0;
    }
}


static void update_status(const char *message)
{
    if (status_label) {
        XmString str = XmStringCreateLocalized((char *)message);
        XtVaSetValues(status_label, XmNlabelString, str, NULL);
        XmStringFree(str);
    }
}


static void populate_results_list(void)
{
    int i;
    XmString *items;
    char temp[512];
    
    if (!results_list) return;
    
    // Clear existing items
    XmListDeleteAllItems(results_list);
    
    if (current_results.count == 0) {
        update_status(langcode("GEOCODE011")); // No results found
        XtSetSensitive(goto_button, False);
        XtSetSensitive(mark_button, False);
        return;
    }
    
    // Allocate array for XmStrings
    items = (XmString *)malloc(sizeof(XmString) * current_results.count);
    if (!items) {
        update_status("Out of memory");
        return;
    }
    
    // Build list items
    for (i = 0; i < current_results.count; i++) {
        struct geocode_result *result = &current_results.results[i];

        if (result->display_name[0]) {
            xastir_snprintf(temp, sizeof(temp), "%s",
                           result->display_name);
        } else {
            char subtitle[512];
            geocode_format_subtitle(result, subtitle, sizeof(subtitle));
            xastir_snprintf(temp, sizeof(temp), "%s",
                           subtitle);
        }
        
        items[i] = XmStringCreateLocalized(temp);
    }
    
    // Add items to list
    XmListAddItems(results_list, items, current_results.count, 0);
    
    // Free XmStrings
    for (i = 0; i < current_results.count; i++) {
        XmStringFree(items[i]);
    }
    free(items);
    
    // Update status
    xastir_snprintf(temp, sizeof(temp), langcode("GEOCODE013"), current_results.count);
    update_status(temp);
    
    // Enable buttons
    XtSetSensitive(goto_button, True);
    XtSetSensitive(mark_button, True);
}


static void search_callback(Widget UNUSED(widget), XtPointer UNUSED(clientData), 
                           XtPointer UNUSED(callData))
{
    char *query;
    char *country_codes = NULL;
    int result;
    char error_msg[512];
    
    if (!query_text) return;
    
    // Get query text
    query = XmTextFieldGetString(query_text);
    if (!query || !query[0]) {
        update_status(langcode("GEOCODE024")); // Enter a location to search
        if (query) XtFree(query);
        return;
    }
    
    // Check if service is available
    if (!geocode_service_available(GEOCODE_SERVICE_NOMINATIM)) {
        update_status(langcode("GEOCODE023")); // Service not available
        XtFree(query);
        return;
    }
    
    // Get selected country filter
    if (country_combo) {
        XmString *selected_items;
        int selected_count;
        
        XtVaGetValues(country_combo,
                     XmNselectedItems, &selected_items,
                     XmNselectedItemCount, &selected_count,
                     NULL);
        
        fprintf(stderr, "DEBUG: selected_count=%d\n", selected_count);
        
        if (selected_count > 0) {
            // Find which item was selected by comparing XmStrings
            char *selected_text = NULL;
            XmStringGetLtoR(selected_items[0], XmFONTLIST_DEFAULT_TAG, &selected_text);
            
            fprintf(stderr, "DEBUG: selected_text='%s'\n", selected_text ? selected_text : "(null)");
            
            if (selected_text) {
                // Find matching option in our array
                int i;
                for (i = 0; country_options[i].label != NULL; i++) {
                    if (strcmp(selected_text, country_options[i].label) == 0) {
                        const char *selected_value = country_options[i].value;
                        fprintf(stderr, "DEBUG: matched option %d, value='%s'\n", i, selected_value);
                        
                        if (strcmp(selected_value, "custom") == 0) {
                            // Use custom text field
                            if (country_custom_text) {
                                char *custom = XmTextFieldGetString(country_custom_text);
                                if (custom && custom[0]) {
                                    country_codes = custom;
                                    fprintf(stderr, "DEBUG: Using custom code='%s'\n", country_codes);
                                }
                            }
                        } else if (selected_value[0] != '\0') {
                            // Non-empty value - use it
                            country_codes = (char *)selected_value;
                            fprintf(stderr, "DEBUG: Using country code='%s'\n", country_codes);
                        } else {
                            fprintf(stderr, "DEBUG: Empty value - worldwide search\n");
                        }
                        break;
                    }
                }
                XtFree(selected_text);
            }
        }
    }
    
    // Clear previous results
    free_current_results();
    
    // Update status
    update_status(langcode("GEOCODE010")); // Searching...
    XmUpdateDisplay(geocoder_dialog);
    
    // Perform search
    fprintf(stderr, "DEBUG: Calling geocode_search with query='%s', country='%s'\n", 
            query, country_codes ? country_codes : "(none)");
    result = geocode_search(GEOCODE_SERVICE_NOMINATIM,
                           query,
                           country_codes,
                           10,  // max results
                           &current_results);
    fprintf(stderr, "DEBUG: geocode_search returned %d, result count=%d\n", 
            result, current_results.count);
    
    XtFree(query);
    
    if (result < 0) {
        // Error occurred
        const char *error = geocode_get_error();
        fprintf(stderr, "DEBUG: Error from geocoder: %s\n", error ? error : "(null)");
        if (error) {
            xastir_snprintf(error_msg, sizeof(error_msg),
                           langcode("GEOCODE012"), error);
        } else {
            xastir_snprintf(error_msg, sizeof(error_msg),
                           langcode("GEOCODE012"), "Unknown error");
        }
        update_status(error_msg);
        return;
    }
    
    // Display results
    populate_results_list();
}


static void clear_callback(Widget UNUSED(widget), XtPointer UNUSED(clientData),
                          XtPointer UNUSED(callData))
{
    if (query_text) {
        XmTextFieldSetString(query_text, "");
    }
    
    free_current_results();
    
    if (results_list) {
        XmListDeleteAllItems(results_list);
    }
    
    update_status("");
    
    if (goto_button) XtSetSensitive(goto_button, False);
    if (mark_button) XtSetSensitive(mark_button, False);
}


static void goto_location(struct geocode_result *result, int mark_location)
{
    unsigned long coord_lat, coord_lon;
    
    convert_to_xastir_coordinates(&coord_lon, &coord_lat,
                                 (float)result->lon,
                                 (float)result->lat);
    
    // If marking, store the coordinates for display
    mark_destination = mark_location;
    if (mark_location) {
        destination_coord_lat = coord_lat;
        destination_coord_lon = coord_lon;
    }
    
    // Set map position
    set_map_position(geocoder_dialog, coord_lat, coord_lon);
}


static void goto_callback(Widget w, XtPointer UNUSED(clientData),
                         XtPointer UNUSED(callData))
{
    int *position_list;
    int position_count;
    
    if (!results_list || current_results.count == 0) return;
    
    // Get selected item(s)
    if (XmListGetSelectedPos(results_list, &position_list, &position_count)) {
        if (position_count > 0) {
            int index = position_list[0] - 1; // XmList is 1-indexed
            
            if (index >= 0 && index < current_results.count) {
                goto_location(&current_results.results[index], 0);
            }
        }
        XtFree((char *)position_list);
    }
}


static void mark_callback(Widget w, XtPointer UNUSED(clientData),
                         XtPointer UNUSED(callData))
{
    int *position_list;
    int position_count;
    
    if (!results_list || current_results.count == 0) return;
    
    // Get selected item(s)
    if (XmListGetSelectedPos(results_list, &position_list, &position_count)) {
        if (position_count > 0) {
            int index = position_list[0] - 1; // XmList is 1-indexed
            
            if (index >= 0 && index < current_results.count) {
                struct geocode_result *result = &current_results.results[index];
                // Go to the location and mark it
                goto_location(result, 1);
            }
        }
        XtFree((char *)position_list);
    }
}


static void close_callback(Widget widget, XtPointer clientData,
                          XtPointer UNUSED(callData))
{
    Widget shell = (Widget)clientData;
    
    begin_critical_section(&geocoder_dialog_lock,
                          "geocoder_gui.c:close_callback");
    
    free_current_results();
    
    XtPopdown(shell);
    XtDestroyWidget(shell);
    geocoder_dialog = (Widget)NULL;
    
    end_critical_section(&geocoder_dialog_lock,
                        "geocoder_gui.c:close_callback");
}


void Geocoder_place(Widget w, XtPointer UNUSED(clientData), XtPointer UNUSED(callData))
{
    Widget form, label, rowcol;
    Widget search_button, clear_button, close_button;
    Arg args[50];
    int n;
    XmString str;
    int i;
    
    if (!geocode_service_available(GEOCODE_SERVICE_NOMINATIM)) {
        popup_message_always(langcode("POPEM00035"),
                            (char *)langcode("GEOCODE023")); // Service not available
        return;
    }
    
    begin_critical_section(&geocoder_dialog_lock,
                          "geocoder_gui.c:Geocoder_place");
    
    if (geocoder_dialog) {
        end_critical_section(&geocoder_dialog_lock,
                            "geocoder_gui.c:Geocoder_place");
        
        (void)XRaiseWindow(XtDisplay(geocoder_dialog), XtWindow(geocoder_dialog));
        return;
    }
    
    // Create dialog shell
    geocoder_dialog = XtVaCreatePopupShell(langcode("GEOCODE001"),
                                          xmDialogShellWidgetClass,
                                          appshell,
                                          XmNdeleteResponse, XmDESTROY,
                                          XmNdefaultPosition, FALSE,
                                          XmNfontList, fontlist1,
                                          NULL);
    
    // Create main form
    form = XtVaCreateWidget("geocoder_form",
                           xmFormWidgetClass,
                           geocoder_dialog,
                           XmNfractionBase, 5,
                           XmNautoUnmanage, FALSE,
                           XmNshadowThickness, 1,
                           NULL);
    
    // Query input section
    label = XtVaCreateManagedWidget(langcode("GEOCODE002"), // Search Query:
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_FORM,
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);
    
    query_text = XtVaCreateManagedWidget("query_text",
                                        xmTextFieldWidgetClass,
                                        form,
                                        XmNcolumns, 50,
                                        XmNtopAttachment, XmATTACH_FORM,
                                        XmNtopOffset, 5,
                                        XmNleftAttachment, XmATTACH_WIDGET,
                                        XmNleftWidget, label,
                                        XmNleftOffset, 5,
                                        XmNrightAttachment, XmATTACH_FORM,
                                        XmNrightOffset, 10,
                                        XmNfontList, fontlist1,
                                        NULL);
    
    // Country filter
    label = XtVaCreateManagedWidget(langcode("GEOCODE003"), // Country Filter:
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, query_text,
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, query_text); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNleftWidget, label); n++;
    XtSetArg(args[n], XmNleftOffset, 5); n++;
    XtSetArg(args[n], XmNvisibleItemCount, 5); n++;
    XtSetArg(args[n], XmNfontList, fontlist1); n++;
    country_combo = XmCreateScrolledList(form, "country_combo", args, n);
    
    // Populate country filter
    for (i = 0; country_options[i].label != NULL; i++) {
        str = XmStringCreateLocalized((char *)country_options[i].label);
        XmListAddItem(country_combo, str, 0);
        XmStringFree(str);
    }
    XmListSelectPos(country_combo, 1, False); // Select "None" by default
    XtManageChild(country_combo);
    
    // Custom country code text field (for entering ISO codes not in list)
    label = XtVaCreateManagedWidget("Custom Code:",
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, XtParent(country_combo),
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);
    
    country_custom_text = XtVaCreateManagedWidget("country_custom_text",
                                                  xmTextFieldWidgetClass,
                                                  form,
                                                  XmNcolumns, 20,
                                                  XmNtopAttachment, XmATTACH_WIDGET,
                                                  XmNtopWidget, XtParent(country_combo),
                                                  XmNtopOffset, 5,
                                                  XmNleftAttachment, XmATTACH_WIDGET,
                                                  XmNleftWidget, label,
                                                  XmNleftOffset, 5,
                                                  XmNfontList, fontlist1,
                                                  NULL);
    
    // Help text for custom field
    label = XtVaCreateManagedWidget("(e.g., 'us' or 'us,ca,mx')",
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, XtParent(country_combo),
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_WIDGET,
                                   XmNleftWidget, country_custom_text,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);
    
    // Button row
    rowcol = XtVaCreateManagedWidget("button_row",
                                    xmRowColumnWidgetClass,
                                    form,
                                    XmNorientation, XmHORIZONTAL,
                                    XmNpacking, XmPACK_TIGHT,
                                    XmNentryAlignment, XmALIGNMENT_CENTER,
                                    XmNtopAttachment, XmATTACH_WIDGET,
                                    XmNtopWidget, country_custom_text,
                                    XmNtopOffset, 10,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment, XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNfontList, fontlist1,
                                    NULL);
    
    search_button = XtVaCreateManagedWidget(langcode("GEOCODE004"), // Search
                                           xmPushButtonWidgetClass,
                                           rowcol,
                                           XmNfontList, fontlist1,
                                           NULL);
    XtAddCallback(search_button, XmNactivateCallback, search_callback, NULL);
    
    clear_button = XtVaCreateManagedWidget(langcode("GEOCODE005"), // Clear
                                          xmPushButtonWidgetClass,
                                          rowcol,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtAddCallback(clear_button, XmNactivateCallback, clear_callback, NULL);
    
    // Results section
    label = XtVaCreateManagedWidget(langcode("GEOCODE006"), // Results:
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, rowcol,
                                   XmNtopOffset, 15,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);
    
    n = 0;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, label); n++;
    XtSetArg(args[n], XmNtopOffset, 5); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 10); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightOffset, 10); n++;
    XtSetArg(args[n], XmNvisibleItemCount, 10); n++;
    XtSetArg(args[n], XmNselectionPolicy, XmSINGLE_SELECT); n++;
    XtSetArg(args[n], XmNfontList, fontlist1); n++;
    results_list = XmCreateScrolledList(form, "results_list", args, n);
    XtManageChild(results_list);
    
    // Status label
    status_label = XtVaCreateManagedWidget("",
                                          xmLabelWidgetClass,
                                          form,
                                          XmNtopAttachment, XmATTACH_WIDGET,
                                          XmNtopWidget, XtParent(results_list),
                                          XmNtopOffset, 5,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNleftOffset, 10,
                                          XmNfontList, fontlist1,
                                          NULL);
    
    // Set initial status message to prevent layout shift
    update_status(langcode("GEOCODE024")); // Enter a location to search
    
    // Attribution label (OSM credit)
    attribution_label = XtVaCreateManagedWidget(langcode("GEOCODE014"),
                                               xmLabelWidgetClass,
                                               form,
                                               XmNtopAttachment, XmATTACH_WIDGET,
                                               XmNtopWidget, status_label,
                                               XmNtopOffset, 10,
                                               XmNleftAttachment, XmATTACH_FORM,
                                               XmNleftOffset, 10,
                                               XmNfontList, fontlist1,
                                               NULL);
    
    // Action buttons
    rowcol = XtVaCreateManagedWidget("action_buttons",
                                    xmRowColumnWidgetClass,
                                    form,
                                    XmNorientation, XmHORIZONTAL,
                                    XmNpacking, XmPACK_TIGHT,
                                    XmNentryAlignment, XmALIGNMENT_CENTER,
                                    XmNtopAttachment, XmATTACH_WIDGET,
                                    XmNtopWidget, attribution_label,
                                    XmNtopOffset, 15,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment, XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 10,
                                    XmNfontList, fontlist1,
                                    NULL);
    
    goto_button = XtVaCreateManagedWidget(langcode("GEOCODE007"), // Go To
                                         xmPushButtonWidgetClass,
                                         rowcol,
                                         XmNsensitive, False,
                                         XmNfontList, fontlist1,
                                         NULL);
    XtAddCallback(goto_button, XmNactivateCallback, goto_callback, NULL);
    
    mark_button = XtVaCreateManagedWidget(langcode("GEOCODE008"), // Mark
                                         xmPushButtonWidgetClass,
                                         rowcol,
                                         XmNsensitive, False,
                                         XmNfontList, fontlist1,
                                         NULL);
    XtAddCallback(mark_button, XmNactivateCallback, mark_callback, NULL);
    
    close_button = XtVaCreateManagedWidget(langcode("GEOCODE009"), // Close
                                          xmPushButtonWidgetClass,
                                          rowcol,
                                          XmNfontList, fontlist1,
                                          NULL);
    XtAddCallback(close_button, XmNactivateCallback, close_callback,
                 geocoder_dialog);
    
    // Set up Enter key in text field to trigger search
    XtAddCallback(query_text, XmNactivateCallback, search_callback, NULL);
    
    pos_dialog(geocoder_dialog);
    
    delw = XmInternAtom(XtDisplay(geocoder_dialog), "WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(geocoder_dialog, delw, close_callback,
                           (XtPointer)geocoder_dialog);
    
    XtManageChild(form);
    
    end_critical_section(&geocoder_dialog_lock,
                        "geocoder_gui.c:Geocoder_place");
    
    XtPopup(geocoder_dialog, XtGrabNone);
    
    // Initialize geocoding service
    geocode_init();
}
