#ifndef XASTIR_DB_GUI_H
#define XASTIR_DB_GUI_H

#include <Xm/XmAll.h>

#define MY_TRAIL_COLOR      0x16    /* trail color index reserved for my station */


// ------------------------------------------------------------------------
// INITIALIZATION FUNCTIONS
// ------------------------------------------------------------------------
void db_gui_init(void);


// ------------------------------------------------------------------------
// STATION TRACKING FUNCTIONS
// ------------------------------------------------------------------------
void track_station(Widget w, char *call_tracked, DataRow *p_station);


// ------------------------------------------------------------------------
// DRAWING AND RENDERING FUNCTIONS
// ------------------------------------------------------------------------
void draw_trail(Widget w, DataRow *fill, int solid);
void display_station(Widget w, DataRow *p_station, int single);
void draw_test_line(Widget w, long x, long y, long dx, long dy, long ofs);
void draw_ruler_text(Widget w, char * text, long ofs);
void draw_range_scale(Widget w);
void draw_ruler(Widget w);
void display_file(Widget w);


// ------------------------------------------------------------------------
// STATION DATA DIALOG FUNCTIONS
// ------------------------------------------------------------------------
extern void Station_data_destroy_track(Widget widget, XtPointer clientData, XtPointer callData);

// ------------------------------------------------------------------------
// STATION INFO DIALOG FUNCTIONS
// ------------------------------------------------------------------------
extern void Station_info(Widget w, XtPointer clientData, XtPointer calldata);


// ------------------------------------------------------------------------
// STATION QUERY FUNCTIONS
// ------------------------------------------------------------------------
extern void General_query(Widget w, XtPointer clientData, XtPointer calldata);
extern void IGate_query(Widget w, XtPointer clientData, XtPointer calldata);
extern void WX_query(Widget w, XtPointer clientData, XtPointer calldata);
void Show_Aloha_Stats(Widget w, XtPointer clientData, XtPointer callData);


// ------------------------------------------------------------------------
// MAIN STATION DATA DIALOG
// ------------------------------------------------------------------------
void Station_data(Widget w, XtPointer clientData, XtPointer calldata);


// ------------------------------------------------------------------------
// STATION INFO DIALOG FUNCTIONS
// ------------------------------------------------------------------------
extern void update_station_info(Widget w);


// ------------------------------------------------------------------------
// MAP POSITION AND LOCATION UTILITIES
// ------------------------------------------------------------------------
extern void set_map_position(Widget w, long lat, long lon);
int locate_station(Widget w, char *call, int follow_case, int get_match, int center_map);


#endif // XASTIR_DB_GUI_H