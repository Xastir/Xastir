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

#ifndef __XASTIR_INTERFACE_H
#define __XASTIR_INTERFACE_H

#include <termios.h>
#include <unistd.h>
#include "util.h"
#include "xastir.h"


#define MAX_DEVICE_NAME 128
#define MAX_DEVICE_BUFFER 4096
#define MAX_DEVICE_BUFFER_UNTIL_BINARY_SWITCH 700
#define MAX_DEVICE_HOSTNM 40
#define MAX_DEVICE_HOSTPW 40

#define MAX_IFACE_DEVICES 15

#define NET_CONNECT_TIMEOUT 20

#define DEFAULT_GPS_RETR 0x05 /* CTRL-E */

// Define a 60 second max wait on a serial port (in microseconds)
#define SERIAL_MAX_WAIT 60000000

// KISS Protocol Special Characters & Commands:
#define KISS_FEND           0xc0  // Frame End
#define KISS_FESC           0xdb  // Frame Escape
#define KISS_TFEND          0xdc  // Transposed Frame End
#define KISS_TFESC          0xdd  // Transposed Frame Escape
#define KISS_DATA           0x00
#define KISS_TXDELAY        0x01
#define KISS_PERSISTENCE    0x02
#define KISS_SLOTTIME       0x03
#define KISS_TXTAIL         0x04
#define KISS_FULLDUPLEX     0x05
#define KISS_SETHARDWARE    0x06
#define KISS_RETURN         0xff



#define MAX_IFACE_DEVICE_TYPES 15

/* Define Device Types */
enum Device_Types
{
  DEVICE_NONE,
  DEVICE_SERIAL_TNC,
  DEVICE_SERIAL_TNC_HSP_GPS,
  DEVICE_SERIAL_GPS,
  DEVICE_SERIAL_WX,
  DEVICE_NET_STREAM,
  DEVICE_AX25_TNC,
  DEVICE_NET_GPSD,
  DEVICE_NET_WX,
  DEVICE_SERIAL_TNC_AUX_GPS,  // KB6MER -> KAM XL or other TNC w/GPS on AUX port
  DEVICE_SERIAL_KISS_TNC,     // KISS TNC on serial port (not ax.25 kernel device)
  DEVICE_NET_DATABASE,
  DEVICE_NET_AGWPE,
  DEVICE_SERIAL_MKISS_TNC,    // Multi-port KISS TNC, like the Kantronics KAM
  DEVICE_SQL_DATABASE         // SQL server (MySQL/Postgis) database
};

enum Device_Active
{
  DEVICE_NOT_IN_USE,
  DEVICE_IN_USE
};

enum Device_Status
{
  DEVICE_DOWN,
  DEVICE_UP,
  DEVICE_ERROR
};


typedef struct
{
  int    device_type;                           /* device type                             */
  int    active;                                /* channel in use                          */
  int    status;                                /* current status (up or down)             */
  char   device_name[MAX_DEVICE_NAME+1];        /* device name                             */
  char   device_host_name[MAX_DEVICE_HOSTNM+1]; /* device host name for network            */
  struct addrinfo *addr_list;                   /* possible network addresses              */
  unsigned long int address;                    /* XXX Delete this                         */
  int    thread_status;                         /* thread status for connect thread        */
  int    connect_status;                        /* connect status for connect thread       */
  int    decode_errors;                         /* decode error count, used for data type  */
  int    data_type;                             /* 0=normal 1=wx_binary                    */
  int    socket_port;                           /* socket port# for network                */
  char   device_host_pswd[MAX_DEVICE_HOSTPW+1]; /* host password                           */
  int    channel;                               /* for serial and net ports                */
  int    channel2;                              /* for AX25 ports                          */
  char   ui_call[30];                           /* current call for this port              */
  struct termios t,t_old;                       /* terminal struct for serial port         */
  int    dtr;                                   /* dtr signal for HSP cable (status)       */
  int    sp;                                    /* serial port speed                       */
  int    style;                                 /* serial port style                       */
  int    scan;                                  /* data read available                     */
  int    errors;                                /* errors for this port                    */
  int    reconnect;                             /* reconnect on net failure                */
  int    reconnects;                            /* total number of reconnects by this port */
  unsigned long   bytes_input;                  /* total bytes read by this port           */
  unsigned long   bytes_output;                 /* total bytes written by this port        */
  unsigned long   bytes_input_last;             /* total bytes read last check             */
  unsigned long   bytes_output_last;            /* total bytes read last check             */
  int    port_activity;                         /* 0 if no activity between checks         */
  pthread_t read_thread;                        /* read thread                             */
  int    read_in_pos;                           /* current read buffer input pos           */
  int    read_out_pos;                          /* current read buffer output pos          */
  char   device_read_buffer[MAX_DEVICE_BUFFER]; /* read buffer for this port               */
  xastir_mutex read_lock;                       /* Lock for reading the port data          */
  pthread_t write_thread;                       /* write thread                            */
  int    write_in_pos;                          /* current write buffer input pos          */
  int    write_out_pos;                         /* current write buffer output pos         */
  xastir_mutex write_lock;                      /* Lock for writing the port data          */
  char   device_write_buffer[MAX_DEVICE_BUFFER];/* write buffer for this port              */
} iface;

typedef struct
{
  char device_name[100];
} iodevices;


typedef struct
{
  int    device_type;                           /* device type                             */
  char   device_name[MAX_DEVICE_NAME+1];        /* device name                             */
  char   radio_port[3];                         /* port for multi-port TNC's               */
  char   device_host_name[MAX_DEVICE_HOSTNM+1]; /* device host name for network            */
  char   device_host_pswd[MAX_DEVICE_HOSTPW+1]; /* host password also WX device data type  */
  char   device_host_filter_string[201];        /* host filter string                      */
  char   device_converse_string[10+1];          /* string used to enter converse mode      */
  char   comment[50];                           /* Local comment or name for port          */
  char   unproto1[50];                          /* unproto path 1 for this port            */
  char   unproto2[50];                          /* unproto path 2 for this port            */
  char   unproto3[50];                          /* unproto path 3 for this port            */
  char   unproto_igate[50];                     /* unproto igate path for this port        */
  int    unprotonum;                            /* unproto path selection                  */
  char   tnc_up_file[100];                      /* file for setting up TNC on this port    */
  char   tnc_down_file[100];                    /* file for shutting down TNC on this port */
  int    sp;                                    /* serial port speed/Net port              */
  int    style;                                 /* serial port style                       */
  int    igate_options;                         /* Igate options (0=none,1=input,2=in/out) */
  int    transmit_data;                         /* Data transmit out of this port          */
  int    reconnect;                             /* reconnect on net failure                */
  int    connect_on_startup;                    /* connect to this device on startup       */
  int    gps_retrieve;                          /* Character to cause SERIAL_TNC_AUX_GPS to spit out current GPS data */
  int    tnc_extra_delay;                       /* Introduces fixed delay when talking to TNC in command-mode */
  int    set_time;                              /* Set System Time from GPS on this port   */
  char   txdelay[4];                            /* KISS parameter */
  char   persistence[4];                        /* KISS parameter */
  char   slottime[4];                           /* KISS parameter */
  char   txtail[4];                             /* KISS parameter */
  int    fullduplex;                            /* KISS parameter */
  int    relay_digipeat;                        /* If 1: interface should RELAY digipeat */
  int    init_kiss;                             /* Initialize KISS-Mode on startup */
#ifdef HAVE_DB
  // to support connections to sql server databases for db_gis.c
  char   database_username[20];                 /* Username to use to connect to database  */
  int    database_type;                         /* Type of dbms (posgresql, mysql, etc)    */
  char   database_schema[20];                   /* Name of database or schema to use       */
  char   database_errormessage[255];            /* Most recent error message from
                                                     attempting to make a
                                                     connection with using this descriptor.  */
  int    database_schema_type;                  /* table structures to use in the database
                                                     A database schema could contain both
                                                     APRSWorld and XASTIR table structures,
                                                     but a separate database descriptor
                                                     needs to be defined for each.  */
  char   database_unix_socket[255];             /* MySQL - unix socket parameter (path and
                                                     filename) */
  // Need a pointer here, and one in connection pointing back here.  How to do????
  //Connection *database_connection;
  /* Pointer to database connection that
     contains database handle (with type
     of handle being dependent on type of
     database.  */
  int    query_on_startup;                      /* Load stations from this database on
                                                     startup. */
  // Use of other ioparam variables for sql server database connections:
  // device_host_name = hostname for database server
  // sp = port on which to connect to database server
  // device_host_pswd =  password to use to connect to database -- security issue needs to be addressed
#endif  // HAVE_DB
} ioparam;


extern iodevices dtype[];

extern xastir_mutex port_data_lock; // Protects the port_data[] array of structs
extern xastir_mutex devices_lock;    // Protects the devices[] array

extern iface port_data[];
extern int port_id[];
extern int get_device_status(int port);
extern int del_device(int port);
extern int get_open_device(void);
extern int add_device(int port_avail,int dev_type,
                      char *dev_nm,
                      char *passwd,
                      int dev_sck_p, int dev_sp,
                      int dev_sty,
                      int reconnect,
                      char *filter_string);

extern xastir_mutex data_lock;          // Protects incoming_data_queue
extern xastir_mutex output_data_lock;   // Protects interface.c:channel_data() function only
extern xastir_mutex connect_lock;       // Protects port_data[].thread_status and port_data[].connect_status

extern ioparam devices[];

#if !HAVE_SOCKLEN_T
  typedef unsigned int socklen_t;
#endif

/* from interface_gui.c */
extern void interface_gui_init(void);
extern void Configure_interface_destroy_shell(Widget widget, XtPointer clientData, XtPointer callData);
extern void Configure_interface(Widget w, XtPointer clientData, XtPointer callData);
extern void output_my_aprs_data(void);
extern void control_interface(Widget w, XtPointer clientData, XtPointer callData);
extern void dtr_all_set(int dtr);
extern void interface_status(Widget w);
extern void update_interface_list(void);
extern int WX_rain_gauge_type;

/* interface.c */
extern int is_local_interface(int port);
extern int is_network_interface(int port);
extern void send_agwpe_packet(int xastir_interface, int RadioPort, unsigned char type, unsigned char *FromCall, unsigned char *ToCall, unsigned char *Path, unsigned char *Data, int length);

extern int pop_incoming_data(unsigned char *data_string, int *port);
extern int push_incoming_data(unsigned char *data_string, int length, int port);

extern unsigned char incoming_data_copy[MAX_LINE_SIZE];
extern unsigned char incoming_data_copy_previous[MAX_LINE_SIZE];
extern int NETWORK_WAITTIME;
extern void startup_all_or_defined_port(int port);
extern void shutdown_all_active_or_defined_port(int port);
extern void check_ports(void);
extern void clear_all_port_data(void);
extern char aprs_station_message_type;
extern void port_dtr(int port, int dtr);
extern void send_kiss_config(int port, int device, int command, int value);
void port_write_string(int port, char *data);
extern void init_device_names(void);
extern void output_my_data(char *message, int port, int type, int loopback_only, int use_igate_path, char *path);
int tnc_get_data_type(char *buf, int port);
void tnc_data_clean(char *buf);
extern void output_waypoint_data(char *message);
extern void send_ax25_frame(int port, char *source, char *destination, char *path, char *data);

extern pid_t getpgid(pid_t pid);


#endif /* XASTIR_INTERFACE_H */

