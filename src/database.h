/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
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

/* Note: this file should be called db.h, but was renamed to database.h
 * to avoid conflicts with the Berkeley DB package.  */

/*
 * Database structures
 *
 */

#ifndef XASTIR_DATABASE_H
#define XASTIR_DATABASE_H


#define MSG_INCREMENT 200
#define MAX_CALLSIGN 9       // Objects are up to 9 chars
#define MAX_TACTICAL_CALL 57 // Up to XX chars for tactical calls
#define MAX_COMMENT_LINES 20  // Save XX unique comment strings per station
#define MAX_STATUS_LINES 20   // Save XX unique status strings per station

/* define max size of info field */
#define MAX_INFO_FIELD_SIZE 256

// Number of times to send killed objects/items before ceasing to
// transmit them.
#define MAX_KILLED_OBJECT_RETRANSMIT 20

// Check entire station list at this rate for objects/items that
// might need to be transmitted via the decaying algorithm.  This is
// the start rate, which gets doubled on each transmit.
#define OBJECT_CHECK_RATE 20

// We should probably be using APRS_DF in extract_bearing_NRQ()
// and extract_omnidf() functions.  We aren't currently.
/* Define APRS Types */
enum APRS_Types
{
  APRS_NULL,
  APRS_MSGCAP,
  APRS_FIXED,
  APRS_DOWN,      // Not used anymore
  APRS_MOBILE,
  APRS_DF,
  APRS_OBJECT,
  APRS_ITEM,
  APRS_STATUS,
  APRS_WX1,
  APRS_WX2,
  APRS_WX3,
  APRS_WX4,
  APRS_WX5,
  APRS_WX6,
  QM_WX,
  PEET_COMPLETE,
  RSWX200,
  GPS_RMC,
  GPS_GGA,
  GPS_GLL,
  STATION_CALL_DATA,
  OTHER_DATA,
  APRS_MICE,
  APRS_GRID,
  DALLAS_ONE_WIRE,
  DAVISMETEO,
  DAVISAPRSDL
};


/* Define Record Types */
#define NORMAL_APRS     'N'
#define MOBILE_APRS     'M'
#define DF_APRS         'D'
#define DOWN_APRS       'Q'
#define NORMAL_GPS_RMC  'C'
#define NORMAL_GPS_GGA  'A'
#define NORMAL_GPS_GLL  'L'
#define APRS_WX1        '1'
#define APRS_WX2        '2'
#define APRS_WX3        '3'
#define APRS_WX4        '4'
#define APRS_WX5        '5'
#define APRS_WX6        '6'

/* define RECORD ACTIVES */
#define RECORD_ACTIVE    'A'
#define RECORD_NOTACTIVE 'N'
#define RECORD_CLOSED     'C'

/* define data from info type */
#define DATA_VIA_LOCAL 'L'
#define DATA_VIA_TNC   'T'
#define DATA_VIA_NET   'I'
#define DATA_VIA_FILE  'F'
#define DATA_VIA_DATABASE  'D'


/* define Heard info type */
#define VIA_TNC         'Y'
#define NOT_VIA_TNC     'N'

/* define Message types */
#define MESSAGE_MESSAGE  'M'
#define MESSAGE_BULLETIN 'B'
#define MESSAGE_NWS      'W'

// Define file info, string length are without trailing '\0'
#define MAX_TIME             20
#define MAX_LONG             12
#define MAX_LAT              11
#define MAX_ALTITUDE         10         //-32808.4 to 300000.0? feet
#define MAX_SPEED             9         /* ?? 3 in knots */
#define MAX_COURSE            7         /* ?? */
#define MAX_POWERGAIN         7
#define MAX_STATION_TIME     10         /* 6+1 */
#define MAX_SAT               4
#define MAX_DISTANCE         10
#define MAX_WXSTATION        50
#define MAX_TEMP            100

#define MAX_MESSAGE_LENGTH  100
#define MAX_MESSAGE_ORDER    10

// track export file formats
#define EXPORT_XASTIR_TRACK 0
#define EXPORT_KML_TRACK 1

extern char *get_most_recent_ack(char *callsign);

extern void Set_Del_Object(Widget w, XtPointer clientData, XtPointer calldata); // From main.c


extern char my_callsign[MAX_CALLSIGN+1];
extern char my_lat[MAX_LAT];
extern char my_long[MAX_LONG];



// Used for messages and bulletins
typedef struct
{
  char active;
  char data_via;
  char type;
  char heard_via_tnc;
  time_t sec_heard;
  time_t last_ack_sent;
  char packet_time[MAX_TIME];
  char call_sign[MAX_CALLSIGN+1];
  char from_call_sign[MAX_CALLSIGN+1];
  char message_line[MAX_MESSAGE_LENGTH+1];
  char seq[MAX_MESSAGE_ORDER+1];
  char acked;
  char position_known;
  time_t interval;
  int tries;
} Message;



// Struct used to create linked list of most recent ack's
typedef struct _ack_record
{
  char callsign[MAX_CALLSIGN+1];
  char ack[5+1];
  struct _ack_record *next;
} ack_record;



#ifdef MSG_DEBUG
extern void msg_clear_data(Message *clear);
extern void msg_copy_data(Message *to, Message *from);
#else   // MSG_DEBUG
#define msg_clear_data(clear) memset((Message *)clear, 0, sizeof(Message))
#define msg_copy_data(to, from) memmove((Message *)to, (Message *)from, \
                                        sizeof(Message))
#endif /* MSG_DEBUG */

extern int message_update_time(void);



enum AreaObjectTypes
{
  AREA_OPEN_CIRCLE     = 0x0,
  AREA_LINE_LEFT       = 0x1,
  AREA_OPEN_ELLIPSE    = 0x2,
  AREA_OPEN_TRIANGLE   = 0x3,
  AREA_OPEN_BOX        = 0x4,
  AREA_FILLED_CIRCLE   = 0x5,
  AREA_LINE_RIGHT      = 0x6,
  AREA_FILLED_ELLIPSE  = 0x7,
  AREA_FILLED_TRIANGLE = 0x8,
  AREA_FILLED_BOX      = 0x9,
  AREA_MAX             = 0x9,
  AREA_NONE            = 0xF
};



enum AreaObjectColors
{
  AREA_BLACK_HI  = 0x0,
  AREA_BLUE_HI   = 0x1,
  AREA_GREEN_HI  = 0x2,
  AREA_CYAN_HI   = 0x3,
  AREA_RED_HI    = 0x4,
  AREA_VIOLET_HI = 0x5,
  AREA_YELLOW_HI = 0x6,
  AREA_GRAY_HI   = 0x7,
  AREA_BLACK_LO  = 0x8,
  AREA_BLUE_LO   = 0x9,
  AREA_GREEN_LO  = 0xA,
  AREA_CYAN_LO   = 0xB,
  AREA_RED_LO    = 0xC,
  AREA_VIOLET_LO = 0xD,
  AREA_YELLOW_LO = 0xE,
  AREA_GRAY_LO   = 0xF
};



typedef struct
{
  unsigned type : 4;
  unsigned color : 4;
  unsigned sqrt_lat_off : 8;
  unsigned sqrt_lon_off : 8;
  unsigned corridor_width : 16;
} AreaObject;



typedef struct
{
  char aprs_type;
  char aprs_symbol;
  char special_overlay;
  AreaObject area_object;
} APRS_Symbol;



// Struct for holding current weather data.
// This struct is pointed to by the DataRow structure.
// An empty string indicates undefined data.
typedef struct                  //                      strlen
{
  time_t  wx_sec_time;
  int     wx_storm;           // Set to one if severe storm
  char    wx_time[MAX_TIME];
  char    wx_course[4];       // in °                     3
  char    wx_speed[4];        // in mph                   3
  time_t  wx_speed_sec_time;
  char    wx_gust[4];         // in mph                   3
  char    wx_hurricane_radius[4];  //nautical miles       3
  char    wx_trop_storm_radius[4]; //nautical miles       3
  char    wx_whole_gale_radius[4]; // nautical miles      3
  char    wx_temp[5];         // in °F                    3
  char    wx_rain[10];        // in hundredths inch/h     3
  char    wx_rain_total[10];  // in hundredths inch
  char    wx_snow[6];         // in inches/24h            3
  char    wx_prec_24[10];     // in hundredths inch/day   3
  char    wx_prec_00[10];     // in hundredths inch       3
  char    wx_hum[5];          // in %                     3
  char    wx_baro[10];        // in hPa                   6
  char    wx_fuel_temp[5];    // in °F                    3
  char    wx_fuel_moisture[5];// in %                     2
  char    wx_type;
  char    wx_station[MAX_WXSTATION];
  int     wx_compute_rain_rates;  //  Some stations provide rain rates
  // directly, others require Xastir to
  // compute from total rain.  Flag this,
  // so we don't clobber useful info from
  // a station.
} WeatherRow;



// Struct for holding track data.  Keeps a dynamically allocated
// doubly-linked list of track points.  The first record should have its
// "prev" pointer set to NULL and the last record should have its "next"
// pointer set to NULL.  If no track storage exists then the pointers to
// these structs in the DataRow struct should be NULL.
typedef struct _TrackRow
{
  long    trail_long_pos;     // coordinate of trail point
  long    trail_lat_pos;      // coordinate of trail point
  time_t  sec;                // date/time of position
  long    speed;              // in 0.1 km/h   undefined: -1
  int     course;             // in degrees    undefined: -1
  long    altitude;           // in 0.1 m      undefined: -99999
  char    flag;               // several flags, see below
  struct  _TrackRow *prev;    // pointer to previous record in list
  struct  _TrackRow *next;    // pointer to next record in list
} TrackRow;



// trail flag definitions
#define TR_LOCAL        0x01    // heard direct (not via digis)
#define TR_NEWTRK       0x02    // start new track



// Struct for holding comment/status data.  Will keep a dynamically
// allocated list of text.  Every different comment field will be
// stored in a separate line.
typedef struct _CommentRow
{
  char   *text_ptr;           // Ptr to the comment text
  time_t sec_heard;           // Latest timestamp for this comment/status
  struct _CommentRow *next;   // Ptr to next record or NULL
} CommentRow;



#define MAX_MULTIPOINTS 35



// Struct for holding multipoint data.
typedef struct _MultipointRow
{
  long multipoints[MAX_MULTIPOINTS][2];
} MultipointRow;



// Break DataRow into several structures.  DataRow will contain the
// parameters that are common across all types of stations.  DataRow
// will contain a pointer to TrackRow if it is a moving station, and
// contain a pointer to WeatherRow if it is a weather station.  If no
// weather or track data existed, the pointers will be NULL.  This new
// way of storing station data will save a LOT of memory.  If a
// station suddenly starts moving or spitting out weather data the new
// structures will be allocated, filled in, and pointers to them
// installed in DataRow.
//
// Station storage now is organized as an ordered linked list. We have
// both sorting by name and by time last heard
//
// todo: check the string length!
//
typedef struct _DataRow
{

  struct _DataRow *n_next;    // pointer to next element in name ordered list
  struct _DataRow *n_prev;    // pointer to previous element in name ordered
  // list
  struct _DataRow *t_newer;   // pointer to next element in time ordered
  // list (newer)
  struct _DataRow *t_older;   // pointer to previous element in time ordered
  // list (older)

  char call_sign[MAX_CALLSIGN+1]; // call sign or name index or object/item
  // name
  char *tactical_call_sign;   // Tactical callsign.  NULL if not assigned
  APRS_Symbol aprs_symbol;
  long coord_lon;             // Xastir coordinates 1/100 sec, 0 = 180°W
  long coord_lat;             // Xastir coordinates 1/100 sec, 0 =  90°N

  int  time_sn;               // serial number for making time index unique
  time_t sec_heard;           // time last heard, used also for time index
  time_t heard_via_tnc_last_time;
  time_t direct_heard;        // KC2ELS - time last heard direct

// Change into time_t structs?  It'd save us a bunch of space.
  char packet_time[MAX_TIME];
  char pos_time[MAX_TIME];

//    char altitude_time[MAX_TIME];
//    char speed_time[MAX_TIME];
//    char station_time[MAX_STATION_TIME];
//    char station_time_type;

  short flag;                 // several flags, see below
  char pos_amb;               // Position ambiguity, 0 = none,
  // 1 = 0.1 minute...

  unsigned int error_ellipse_radius; // Degrades precision for this
  // station, from 0 to 65535 cm or
  // 655.35 meters.  Assigned when we
  // decode each type of packet.
  // Default is 6.0 meters (600 cm)
  // unless we know the GPS position
  // is augmented, or is degraded by
  // less precision in the packet.

  unsigned int lat_precision; // In 100ths of a second latitude
  unsigned int lon_precision; // In 100ths of a second longitude

  int trail_color;            // trail color (when assigned)
  char record_type;
  char data_via;              // L local, T TNC, I internet, F file

// Change to char's to save space?
  int  heard_via_tnc_port;
  int  last_port_heard;
  unsigned int  num_packets;
  char *node_path_ptr;        // Pointer to path string
  char altitude[MAX_ALTITUDE]; // in meters (feet gives better resolution ??)
  char speed[MAX_SPEED+1];    // in knots (same as nautical miles/hour)
  char course[MAX_COURSE+1];
  char bearing[MAX_COURSE+1];
  char NRQ[MAX_COURSE+1];
  char power_gain[MAX_POWERGAIN+1];   // Holds the phgd values
  char signal_gain[MAX_POWERGAIN+1];  // Holds the shgd values (for DF'ing)

  WeatherRow *weather_data;   // Pointer to weather data or NULL

  CommentRow *status_data;    // Ptr to status records or NULL
  CommentRow *comment_data;   // Ptr to comment records or NULL

  // Below two pointers are NULL if only one position has been received
  TrackRow *oldest_trackpoint; // Pointer to oldest track point in
  // doubly-linked list
  TrackRow *newest_trackpoint; // Pointer to newest track point in
  // doubly-linked list

  // When the station is an object, it can include coordinates
  // of related points. Currently these are being used to draw
  // outlines of NWS severe weather watches and warnings, and
  // storm regions. The coordinates are stored here in Xastir
  // coordinate form. Element [x][0] is the latitude, and
  // element [x][1] is the longitude.  --KG4NBB
  //
  // Is there anything preventing a multipoint string from being
  // in other types of packets, in the comment field?  --WE7U
  //
  int num_multipoints;
  char type;      // from '0' to '9'
  char style;     // from 'a' to 'z'
  MultipointRow *multipoint_data;


///////////////////////////////////////////////////////////////////////
// Optional stuff for Objects/Items only (I think, needs to be
// checked).  These could be moved into an ObjectRow structure, with
// only a NULL pointer here if not an object/item.
///////////////////////////////////////////////////////////////////////

  char origin[MAX_CALLSIGN+1]; // call sign originating an object
  short object_retransmit;     // Number of times to retransmit object.
  // -1 = forever
  // Used currently to stop sending killed
  // objects.
  time_t last_transmit_time;   // Time we last transmitted an object/item.
  // Used to implement decaying transmit time
  // algorithm
  short transmit_time_increment; // Seconds to add to transmit next time
  // around.  Used to implement decaying
  // transmit time algorithm
//    time_t last_modified_time;   // Seconds since the object/item
  // was last modified.  We'll
  // eventually use this for
  // dead-reckoning.
  char signpost[5+1];          // Holds signpost data
  int  df_color;
  char sats_visible[MAX_SAT];
  char probability_min[10+1];  // Holds prob_min (miles)
  char probability_max[10+1];  // Holds prob_max (miles)

} DataRow;



// Used to store one vertice in CADRow object
typedef struct _VerticeRow
{
  long    latitude;           // Xastir coordinates 1/100sec, 0 = 180W
  long    longitude;          // Xastir coordinates 1/100sec, 0 =  90N
  struct  _VerticeRow *next;  // Pointer to next record in list
} VerticeRow;

#define CAD_LABEL_MAX_SIZE 40
#define CAD_COMMENT_MAX_SIZE 256


// CAD Objects
typedef struct _CADRow
{
  struct _CADRow *next;       // Pointer to next element in list
  time_t creation_time;       // Time at which object was first created
  VerticeRow *start;          // Pointer to first VerticeRow
  int line_color;             // Border color
  int line_type;              // Border linetype
  int line_width;             // Border line width
  float computed_area;        // Area in square kilometers
  float raw_probability;      // Probability of area (POA) or probability of
  // detection (POD) stored as probability
  // with a value between 0 and 1.
  // Set and get with CAD_object_get_raw_probability()
  // and CAD_object_set_raw_probability(), rather
  // than by a direct request for CADRow->raw_probability.
  long label_latitude;        // Latitude for label placement
  long label_longitude;       // Longitude for label placement
  char label[CAD_LABEL_MAX_SIZE];             // Name of polygon
  char comment[CAD_COMMENT_MAX_SIZE];          // Comments associated with polygon
} CADRow;


extern CADRow *CAD_list_head;



// station flag definitions.  We have 16 bits available here as
// "flag" in "DataRow" is defined as a short.
//
#define ST_OBJECT       0x01    // station is an object
#define ST_ITEM         0x02    // station is an item
#define ST_ACTIVE       0x04    // station is active (deleted objects are
// inactive)
#define ST_MOVING       0x08    // station is moving
#define ST_DIRECT       0x10    // heard direct (not via digis)
#define ST_VIATNC       0x20    // station heard via TNC
#define ST_3RD_PT       0x40    // third party traffic
#define ST_MSGCAP       0x80    // message capable (not used yet)
#define ST_STATUS       0x100   // got real status message
#define ST_INVIEW       0x200   // station is in current screen view
#define ST_MYSTATION    0x400   // station is owned by my call-SSID
#define ST_MYOBJITEM    0x800   // object/item owned by me


#ifdef DATA_DEBUG
extern void clear_data(DataRow *clear);
extern void copy_data(DataRow *to, DataRow *from);
#else   // DATA_DEBUG
#define clear_data(clear) memset((DataRow *)clear, 0, sizeof(DataRow))
#define copy_data(to, from) memmove((DataRow *)to, (DataRow *)from, \
                                    sizeof(DataRow))
#endif /* DATA_DEBUG */


extern void db_init(void);
extern void export_trail_as_kml(DataRow *p_station);   // export trail of one or all stations to kml file

//
extern int is_my_call(char *call, int exact);
extern int is_my_station(DataRow *p_station);
extern int is_my_object_item(DataRow *p_station);

void mscan_file(char msg_type, void (*function)(Message *fill));
extern void msg_record_ack(char *to_call_sign, char *my_call, char *seq,
                           int timeout, int cancel);
extern void msg_record_interval_tries(char *to_call_sign, char *my_call,
                                      char *seq, time_t interval, int tries);
extern void display_file(Widget w);
extern void clean_data_file(void);
extern void read_file_line(FILE *f);
extern void mdisplay_file(char msg_type);
extern void mem_display(void);
extern long sort_input_database(char *filename, char *fill, int size);
extern void sort_display_file(char *filename, int size);
extern void clear_sort_file(char *filename);
extern void display_packet_data(void);
extern int  redraw_on_new_packet_data;
extern int decode_ax25_header(unsigned char *data_string, int *length);
extern int decode_ax25_line(char *line, char from, int port, int dbadd);

// utilities
extern void packet_data_add(char *from, char *line, int data_port);
extern void General_query(Widget w, XtPointer clientData, XtPointer calldata);
extern void IGate_query(Widget w, XtPointer clientData, XtPointer calldata);
extern void WX_query(Widget w, XtPointer clientData, XtPointer calldata);
extern unsigned long max_stations;
extern int  heard_via_tnc_in_past_hour(char *call);

// messages
extern void update_messages(int force);
extern void mdelete_messages_from(char *from);
extern void mdelete_messages_to(char *to);
extern void init_message_data(void);
extern void check_message_remove(time_t curr_sec);
extern int  new_message_data;
extern time_t msg_data_add(char *call_sign, char *from_call, char *data,
                           char *seq, char type, char from, long *record_out);

// stations
extern int st_direct_timeout;   // Interval that ST_DIRECT flag stays set
extern int station_count;       // Count of stations in the database
extern int station_count_save;  // Old copy of the above
extern DataRow *n_first;  // pointer to first element in name ordered station
// list
extern DataRow *n_last;   // pointer to last element in name ordered station
// list
extern DataRow *t_oldest; // pointer to first element in time ordered station
// list
extern DataRow *t_newest; // pointer to last element in time ordered station
// list
extern void init_station_data(void);
extern void Station_data(Widget w, XtPointer clientData, XtPointer calldata);
extern int station_data_auto_update;
extern int  next_station_name(DataRow **p_curr);
extern int  prev_station_name(DataRow **p_curr);
extern int  next_station_time(DataRow **p_curr);
extern int  prev_station_time(DataRow **p_curr);
extern int  search_station_name(DataRow **p_name, char *call, int exact);
extern int  search_station_time(DataRow **p_time, time_t heard, int serial);
extern void check_station_remove(time_t curr_sec);
extern void delete_all_stations(void);
extern void station_del(char *callsign);
extern void my_station_add(char *my_call_sign, char my_group, char my_symbol,
                           char *my_long, char *my_lat, char *my_phg,
                           char *my_comment, char my_amb);
extern void my_station_gps_change(char *pos_long, char *pos_lat, char *course,
                                  char *speed, char speedu, char *alt,
                                  char *sats);
extern int  locate_station(Widget w, char *call, int follow_case,
                           int get_match, int center_map);
extern void update_station_info(Widget w);

// objects/items
extern time_t last_object_check;

// trails
extern int  delete_trail(DataRow *fill);

// weather
extern int  get_weather_record(DataRow *fill);

extern void set_map_position(Widget w, long lat, long lon);


// just used for aloha calcs
typedef struct
{
  double distance;
  char call_sign[MAX_CALLSIGN+1]; // call sign or name index or object/item
  // name
  char is_mobile;
  char is_other_mobile;
  char is_wx;
  char is_digi; // can only tell this if using a digi icon!
  char is_home; // stationary stations that are not digis
} aloha_entry;
typedef struct
{
  int digis;
  int wxs;
  int other_mobiles;
  int mobiles_in_motion;
  int homes;
  int total;
} aloha_stats;

double calc_aloha_distance(void); //meat
void calc_aloha(int curr_sec); // periodic function
void Show_Aloha_Stats(Widget w, XtPointer clientData,
                      XtPointer callData); // popup window

int comp_by_dist(const void *,const void *);// used only for qsort
DataRow * sanity_check_time_list(time_t); // used only for debugging
void dump_time_sorted_list(void);

extern int store_trail_point(DataRow *p_station, long lon, long lat, time_t sec, char *alt, char *speed, char *course, short stn_flag);

#ifdef HAVE_DB
  extern int add_simple_station(DataRow *p_new_station,char *station, char *origin, char *symbol, char *overlay, char *aprs_type, char *latitude, char *longitude, char *record_type, char *node_path, char *transmit_time, char *timeformat);

#endif /* HAVE_DB */

#endif /* XASTIR_DATABASE_H */
