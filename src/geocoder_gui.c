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
#include "xa_config.h"

// Must be last include file
#include "leak_detection.h"

#ifndef HAVE_NOMINATIM
// Configuration variables - defined here to avoid linker errors if Nominatim is not compiled in
char nominatim_server_url[400];
int nominatim_cache_enabled = 1;
int nominatim_cache_days = 30;
char nominatim_user_email[100];
char nominatim_country_default[20];
#endif  // HAVE_NOMINATIM


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
static xastir_mutex geocoder_config_lock;
static struct geocode_result_list current_results;

// Country code mapping (ISO 3166-1 alpha-2)
struct country_option {
    const char *label;
    const char *value;
};

static const struct country_option country_options[] = {
    {"None (Worldwide)", ""},

    {"Afghanistan", "af"},
    {"Albania", "al"},
    {"Algeria", "dz"},
    {"American Samoa", "as"},
    {"Andorra", "ad"},
    {"Angola", "ao"},
    {"Argentina", "ar"},
    {"Armenia", "am"},
    {"Australia", "au"},
    {"Austria", "at"},
    {"Azerbaijan", "az"},

    {"Bahamas", "bs"},
    {"Bahrain", "bh"},
    {"Bangladesh", "bd"},
    {"Barbados", "bb"},
    {"Belarus", "by"},
    {"Belgium", "be"},
    {"Belize", "bz"},
    {"Bolivia", "bo"},
    {"Bosnia and Herzegovina", "ba"},
    {"Botswana", "bw"},
    {"Brazil", "br"},
    {"Brunei", "bn"},
    {"Bulgaria", "bg"},

    {"Cambodia", "kh"},
    {"Cameroon", "cm"},
    {"Canada", "ca"},
    {"Chile", "cl"},
    {"China", "cn"},
    {"Colombia", "co"},
    {"Costa Rica", "cr"},
    {"Croatia", "hr"},
    {"Cuba", "cu"},
    {"Cyprus", "cy"},
    {"Czech Republic", "cz"},

    {"Denmark", "dk"},
    {"Dominican Republic", "do"},

    {"Ecuador", "ec"},
    {"Egypt", "eg"},
    {"El Salvador", "sv"},
    {"Estonia", "ee"},
    {"Ethiopia", "et"},

    {"Finland", "fi"},
    {"France", "fr"},

    {"Georgia", "ge"},
    {"Germany", "de"},
    {"Ghana", "gh"},
    {"Greece", "gr"},
    {"Greenland", "gl"},
    {"Guatemala", "gt"},

    {"Haiti", "ht"},
    {"Honduras", "hn"},
    {"Hong Kong", "hk"},
    {"Hungary", "hu"},

    {"Iceland", "is"},
    {"India", "in"},
    {"Indonesia", "id"},
    {"Iran", "ir"},
    {"Ireland", "ie"},
    {"Israel", "il"},
    {"Italy", "it"},

    {"Jamaica", "jm"},
    {"Japan", "jp"},
    {"Jordan", "jo"},

    {"Kazakhstan", "kz"},
    {"Kenya", "ke"},
    {"Kuwait", "kw"},

    {"Latvia", "lv"},
    {"Lebanon", "lb"},
    {"Liechtenstein", "li"},
    {"Lithuania", "lt"},
    {"Luxembourg", "lu"},

    {"Malaysia", "my"},
    {"Malta", "mt"},
    {"Mexico", "mx"},
    {"Moldova", "md"},
    {"Monaco", "mc"},
    {"Mongolia", "mn"},
    {"Morocco", "ma"},

    {"Namibia", "na"},
    {"Netherlands", "nl"},
    {"New Zealand", "nz"},
    {"Nigeria", "ng"},
    {"North Macedonia", "mk"},
    {"Norway", "no"},

    {"Oman", "om"},

    {"Pakistan", "pk"},
    {"Panama", "pa"},
    {"Paraguay", "py"},
    {"Peru", "pe"},
    {"Philippines", "ph"},
    {"Poland", "pl"},
    {"Portugal", "pt"},

    {"Qatar", "qa"},

    {"Romania", "ro"},
    {"Russia", "ru"},

    {"Saudi Arabia", "sa"},
    {"Serbia", "rs"},
    {"Singapore", "sg"},
    {"Slovakia", "sk"},
    {"Slovenia", "si"},
    {"South Africa", "za"},
    {"South Korea", "kr"},
    {"Spain", "es"},
    {"Sri Lanka", "lk"},
    {"Sweden", "se"},
    {"Switzerland", "ch"},

    {"Taiwan", "tw"},
    {"Thailand", "th"},
    {"Trinidad and Tobago", "tt"},
    {"Tunisia", "tn"},
    {"Turkey", "tr"},

    {"Ukraine", "ua"},
    {"United Arab Emirates", "ae"},
    {"United Kingdom", "gb"},
    {"United States", "us"},
    {"Uruguay", "uy"},

    {"Venezuela", "ve"},
    {"Vietnam", "vn"},

    {"Zambia", "zm"},
    {"Zimbabwe", "zw"},

    {"--- Custom Code ---", "custom"},
    {NULL, NULL}
};

// Initialize the geocoder GUI module
void geocoder_gui_init(void)
{
    init_critical_section(&geocoder_dialog_lock);
    init_critical_section(&geocoder_config_lock);
    current_results.results = NULL;
    current_results.count = 0;
    current_results.capacity = 0;
}


// Helper function to populate a country combo box with the standard list
// Returns the 1-based index of the default_value if found, or 0 if not found
static int populate_country_list(Widget combo, const char *default_value)
{
    XmString str;
    int i;
    int default_index = 0;

    for (i = 0; country_options[i].label != NULL; i++) {
        str = XmStringCreateLocalized((char *)country_options[i].label);
        XmListAddItem(combo, str, 0);
        XmStringFree(str);

        // Check if this matches the default value
        if (default_value && default_value[0] != '\0' &&
            strcmp(country_options[i].value, default_value) == 0) {
            default_index = i + 1; // XmList is 1-indexed
        }
    }

    return default_index;
}


// Helper function to get the selected country code from a combo box
// If custom_text_widget is provided and "custom" is selected, reads from that field
// Returns a newly allocated string that must be freed by caller, or NULL if none selected
// If return value is non-NULL and *is_custom is non-NULL, sets *is_custom to 1 if custom field was used
static char *get_selected_country_code(Widget combo, Widget custom_text_widget, int *is_custom)
{
    XmString *selected_items;
    int selected_count;
    char *result = NULL;

    if (is_custom) {
        *is_custom = 0;
    }

    XtVaGetValues(combo,
                 XmNselectedItems, &selected_items,
                 XmNselectedItemCount, &selected_count,
                 NULL);

    if (selected_count > 0) {
        char *selected_text = NULL;
        XmStringGetLtoR(selected_items[0], XmFONTLIST_DEFAULT_TAG, &selected_text);

        if (selected_text) {
            // Find matching option in our array
            int i;
            for (i = 0; country_options[i].label != NULL; i++) {
                if (strcmp(selected_text, country_options[i].label) == 0) {
                    const char *selected_value = country_options[i].value;

                    if (strcmp(selected_value, "custom") == 0) {
                        // Use custom text field
                        if (custom_text_widget) {
                            char *custom = XmTextFieldGetString(custom_text_widget);
                            if (custom && custom[0]) {
                                result = XtNewString(custom);
                                if (is_custom) {
                                    *is_custom = 1;
                                }
                            }
                            if (custom) {
                                XtFree(custom);
                            }
                        }
                    } else if (selected_value[0] != '\0') {
                        // Non-empty value - use it
                        result = XtNewString(selected_value);
                    }
                    break;
                }
            }
            XtFree(selected_text);
        }
    }

    return result;
}

// Free the current geocode search results
static void free_current_results(void)
{
    if (current_results.results) {
        geocode_free_results(&current_results);
        current_results.results = NULL;
        current_results.count = 0;
        current_results.capacity = 0;
    }
}

// Update the status label in the geocoder dialog
static void update_status(const char *message)
{
    if (status_label) {
        XmString str = XmStringCreateLocalized((char *)message);
        XtVaSetValues(status_label, XmNlabelString, str, NULL);
        XmStringFree(str);
    }
}

// Populate the results list widget with current search results
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


// Callback for the search button, performs geocoding search
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

    // Get selected country filter using helper function
    if (country_combo) {
        country_codes = get_selected_country_code(country_combo, country_custom_text, NULL);
    }

    // Clear previous results
    free_current_results();

    // Update status
    update_status(langcode("GEOCODE010")); // Searching...
    XmUpdateDisplay(geocoder_dialog);

    // Perform search
    result = geocode_search(GEOCODE_SERVICE_NOMINATIM,
                           query,
                           country_codes,
                           10,  // max results
                           &current_results);

    XtFree(query);
    if (country_codes) {
        XtFree(country_codes);
    }

    if (result < 0) {
        // Error occurred
        const char *error = geocode_get_error();
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


// Callback for the clear button, clears query and results
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

    update_status(langcode("GEOCODE024")); // Enter a location to search

    if (goto_button) XtSetSensitive(goto_button, False);
    if (mark_button) XtSetSensitive(mark_button, False);
}

// Navigate the map to the specified geocode result location
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


// Callback for the Go To button, navigates to selected result
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

// Callback for the Mark button, navigates to and marks selected result
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

// Callback for closing the geocoder dialog
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

// Create and display the main geocoder dialog
void Geocoder_place(Widget w, XtPointer UNUSED(clientData), XtPointer UNUSED(callData))
{
    Widget form, label, rowcol;
    Widget search_button, clear_button, close_button;
    Arg args[50];
    int n;
    int default_country_index = 0;

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

    // Populate country filter using helper function
    default_country_index = populate_country_list(country_combo, nominatim_country_default);

    // Select configured default or "None" if not set
    if (default_country_index > 0) {
        XmListSelectPos(country_combo, default_country_index, False);
        XmListSetPos(country_combo, default_country_index);
    } else {
        XmListSelectPos(country_combo, 1, False); // Select "None" by default
        XmListSetPos(country_combo, 1);
    }
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


// Geocoder configuration dialog
static Widget geocoder_config_dialog = (Widget)NULL;
static Widget config_server_url_text = (Widget)NULL;
static Widget config_email_text = (Widget)NULL;
static Widget config_country_combo = (Widget)NULL;
static Widget config_country_custom_text = (Widget)NULL;

// Destroy the geocoder configuration dialog shell
static void geocoder_config_destroy_shell(Widget UNUSED(widget), XtPointer clientData, XtPointer UNUSED(callData))
{
    Widget shell = (Widget)clientData;

    XtPopdown(shell);

    begin_critical_section(&geocoder_config_lock, "geocoder_gui.c:geocoder_config_destroy_shell");

    XtDestroyWidget(shell);
    geocoder_config_dialog = (Widget)NULL;

    end_critical_section(&geocoder_config_lock, "geocoder_gui.c:geocoder_config_destroy_shell");
}

// Save the geocoder configuration settings from the dialog
static void geocoder_config_save_callback(Widget widget, XtPointer clientData, XtPointer callData)
{
    char *server_url;
    char *email;
    char *country_code;

    // Get server URL (save even if empty to allow clearing)
    if (config_server_url_text) {
        server_url = XmTextFieldGetString(config_server_url_text);
        if (server_url) {
            if (server_url[0] == '\0') {
                // User cleared the field - restore default
                xastir_snprintf(nominatim_server_url, sizeof(nominatim_server_url),
                              "https://nominatim.openstreetmap.org");
            } else {
                xastir_snprintf(nominatim_server_url, sizeof(nominatim_server_url), "%s", server_url);
            }
            XtFree(server_url);
        }
    }

    // Get email address (save even if empty to allow clearing)
    if (config_email_text) {
        email = XmTextFieldGetString(config_email_text);
        if (email) {
            // Empty string is valid - clears the email
            xastir_snprintf(nominatim_user_email, sizeof(nominatim_user_email), "%s", email);
            XtFree(email);
        }
    }

    // Get default country using helper function
    if (config_country_combo) {
        country_code = get_selected_country_code(config_country_combo, config_country_custom_text, NULL);

        if (country_code) {
            xastir_snprintf(nominatim_country_default,
                          sizeof(nominatim_country_default),
                          "%s", country_code);
            XtFree(country_code);
        } else {
            // No country selected - clear the default
            nominatim_country_default[0] = '\0';
        }
    }

    // Save configuration
    save_data();

    geocoder_config_destroy_shell(widget, clientData, callData);
}

// Create and display the geocoder configuration dialog
void Configure_geocoder_settings(Widget UNUSED(w), XtPointer UNUSED(clientData), XtPointer UNUSED(callData))
{
    Widget form, label, button_ok, button_cancel, rowcol;
    Arg args[50];
    int n;
    int default_country_index = 0;

    begin_critical_section(&geocoder_config_lock, "geocoder_gui.c:Configure_geocoder_settings");

    if (geocoder_config_dialog) {
        end_critical_section(&geocoder_config_lock, "geocoder_gui.c:Configure_geocoder_settings");
        (void)XRaiseWindow(XtDisplay(geocoder_config_dialog), XtWindow(geocoder_config_dialog));
        return;
    }

    // Create dialog shell
    geocoder_config_dialog = XtVaCreatePopupShell(langcode("GEOCFG001"), // Geocoding Settings
                                                  xmDialogShellWidgetClass,
                                                  appshell,
                                                  XmNdeleteResponse, XmDESTROY,
                                                  XmNdefaultPosition, FALSE,
                                                  XmNfontList, fontlist1,
                                                  NULL);

    // Create main form
    form = XtVaCreateWidget("geocoder_config_form",
                           xmFormWidgetClass,
                           geocoder_config_dialog,
                           XmNfractionBase, 5,
                           XmNautoUnmanage, FALSE,
                           XmNshadowThickness, 1,
                           NULL);

    // Server URL section
    label = XtVaCreateManagedWidget(langcode("GEOCFG002"), // Server URL:
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_FORM,
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);

    config_server_url_text = XtVaCreateManagedWidget("config_server_url_text",
                                                     xmTextFieldWidgetClass,
                                                     form,
                                                     XmNcolumns, 50,
                                                     XmNmaxLength, 399,
                                                     XmNtopAttachment, XmATTACH_FORM,
                                                     XmNtopOffset, 5,
                                                     XmNleftAttachment, XmATTACH_WIDGET,
                                                     XmNleftWidget, label,
                                                     XmNleftOffset, 5,
                                                     XmNrightAttachment, XmATTACH_FORM,
                                                     XmNrightOffset, 10,
                                                     XmNfontList, fontlist1,
                                                     NULL);

    // Set current value
    XmTextFieldSetString(config_server_url_text, nominatim_server_url);

    // Email address section
    label = XtVaCreateManagedWidget(langcode("GEOCFG003"), // Email Address:
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, config_server_url_text,
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);

    config_email_text = XtVaCreateManagedWidget("config_email_text",
                                                xmTextFieldWidgetClass,
                                                form,
                                                XmNcolumns, 40,
                                                XmNmaxLength, 99,
                                                XmNtopAttachment, XmATTACH_WIDGET,
                                                XmNtopWidget, config_server_url_text,
                                                XmNtopOffset, 5,
                                                XmNleftAttachment, XmATTACH_WIDGET,
                                                XmNleftWidget, label,
                                                XmNleftOffset, 5,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                XmNrightOffset, 10,
                                                XmNfontList, fontlist1,
                                                NULL);

    // Set current value
    XmTextFieldSetString(config_email_text, nominatim_user_email);

    // Help text for email
    label = XtVaCreateManagedWidget(langcode("GEOCFG004"), // (Optional but recommended)
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, config_email_text,
                                   XmNtopOffset, 2,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 150,
                                   XmNfontList, fontlist1,
                                   NULL);

    // Default country section
    label = XtVaCreateManagedWidget(langcode("GEOCFG005"), // Default Country:
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, label,
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
    XtSetArg(args[n], XmNvisibleItemCount, 8); n++;
    XtSetArg(args[n], XmNfontList, fontlist1); n++;
    config_country_combo = XmCreateScrolledList(form, "config_country_combo", args, n);

    // Populate country list using helper function
    default_country_index = populate_country_list(config_country_combo, nominatim_country_default);

    // Select current default or "None" if not set
    if (default_country_index > 0) {
        XmListSelectPos(config_country_combo, default_country_index, False);
        XmListSetPos(config_country_combo, default_country_index);
    } else {
        XmListSelectPos(config_country_combo, 1, False); // Select "None" by default
        XmListSetPos(config_country_combo, 1);
    }

    XtManageChild(config_country_combo);

    // Custom country code text field (for entering ISO codes not in list)
    label = XtVaCreateManagedWidget("Custom Code:",
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, XtParent(config_country_combo),
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_FORM,
                                   XmNleftOffset, 10,
                                   XmNfontList, fontlist1,
                                   NULL);

    config_country_custom_text = XtVaCreateManagedWidget("config_country_custom_text",
                                                         xmTextFieldWidgetClass,
                                                         form,
                                                         XmNcolumns, 20,
                                                         XmNmaxLength, 99,
                                                         XmNtopAttachment, XmATTACH_WIDGET,
                                                         XmNtopWidget, XtParent(config_country_combo),
                                                         XmNtopOffset, 5,
                                                         XmNleftAttachment, XmATTACH_WIDGET,
                                                         XmNleftWidget, label,
                                                         XmNleftOffset, 5,
                                                         XmNfontList, fontlist1,
                                                         NULL);

    // Help text for custom field
    label = XtVaCreateManagedWidget("(e.g., 'us' or 'us,ca,mx' when Custom selected)",
                                   xmLabelWidgetClass,
                                   form,
                                   XmNtopAttachment, XmATTACH_WIDGET,
                                   XmNtopWidget, XtParent(config_country_combo),
                                   XmNtopOffset, 10,
                                   XmNleftAttachment, XmATTACH_WIDGET,
                                   XmNleftWidget, config_country_custom_text,
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
                                    XmNtopWidget, config_country_custom_text,
                                    XmNtopOffset, 15,
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNleftOffset, 10,
                                    XmNrightAttachment, XmATTACH_FORM,
                                    XmNrightOffset, 10,
                                    XmNbottomAttachment, XmATTACH_FORM,
                                    XmNbottomOffset, 10,
                                    XmNfontList, fontlist1,
                                    NULL);

    button_ok = XtVaCreateManagedWidget(langcode("UNIOP00001"), // OK
                                       xmPushButtonWidgetClass,
                                       rowcol,
                                       XmNfontList, fontlist1,
                                       NULL);
    XtAddCallback(button_ok, XmNactivateCallback, geocoder_config_save_callback, geocoder_config_dialog);

    button_cancel = XtVaCreateManagedWidget(langcode("UNIOP00002"), // Cancel
                                           xmPushButtonWidgetClass,
                                           rowcol,
                                           XmNfontList, fontlist1,
                                           NULL);
    XtAddCallback(button_cancel, XmNactivateCallback, geocoder_config_destroy_shell, geocoder_config_dialog);

    pos_dialog(geocoder_config_dialog);

    delw = XmInternAtom(XtDisplay(geocoder_config_dialog), "WM_DELETE_WINDOW", FALSE);
    XmAddWMProtocolCallback(geocoder_config_dialog, delw, geocoder_config_destroy_shell,
                           (XtPointer)geocoder_config_dialog);

    XtManageChild(form);

    end_critical_section(&geocoder_config_lock, "geocoder_gui.c:Configure_geocoder_settings");

    XtPopup(geocoder_config_dialog, XtGrabNone);
}
