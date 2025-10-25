/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025 The Xastir Group
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

/* 
 * Mock implementations for output_my_aprs_data() testing
 * 
 * This file contains mock/stub implementations of all external dependencies
 * needed to test output_my_aprs_data() in isolation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdarg.h>
#include "interface.h"

/* Define necessary constants and types */
#define MAX_COMMENT 100
#define MAX_PHG 10



/* Global variables that output_my_aprs_data needs */
int transmit_disable = 0;
int emergency_beacon = 0;
int posit_tx_disable = 0;
char my_callsign[MAX_CALLSIGN+1] = "";
char my_lat[MAX_LAT] = "";
char my_long[MAX_LONG] = "";
char my_group = '/';
char my_symbol = '-';
char my_comment[MAX_COMMENT+1] = "";
char my_phg[MAX_PHG] = "";
int my_last_course = 0;
int my_last_speed = 0;
long my_last_altitude = 0;
time_t my_last_altitude_time = 0;
int transmit_compressed_posit = 0;
int output_station_type = 0;
int debug_level = 0xfff;
char aprs_station_message_type = '!';

iface port_data[MAX_IFACE_DEVICES];
ioparam devices[MAX_IFACE_DEVICES];
xastir_mutex devices_lock;

#define VERSIONFRM "APX999"


/* Mock control variables - track what was written */
typedef struct
{
  int write_count;
  char last_write[4096];
  char all_writes[10][4096];
  int total_writes;
} mock_port_state_t;

static mock_port_state_t mock_ports[MAX_IFACE_DEVICES];

/* Popup tracking */
static int popup_count = 0;
static char last_popup_title[256] = "";
static char last_popup_message[1024] = "";

void mock_record_write(int port, const char *data)
{
  if (port < 0 || port >= MAX_IFACE_DEVICES)
    return;
    
  mock_ports[port].write_count++;
  
  strncpy(mock_ports[port].last_write, data, sizeof(mock_ports[port].last_write)-1);
  mock_ports[port].last_write[sizeof(mock_ports[port].last_write)-1] = '\0';
  
  if (mock_ports[port].total_writes < 10)
  {
    strncpy(mock_ports[port].all_writes[mock_ports[port].total_writes], 
            data, 
            sizeof(mock_ports[port].all_writes[0])-1);
    mock_ports[port].all_writes[mock_ports[port].total_writes][sizeof(mock_ports[port].all_writes[0])-1] = '\0';
    mock_ports[port].total_writes++;
  }
}

/* Mock control functions */
void mock_reset_all(void)
{
  int i;
  
  transmit_disable = 0;
  emergency_beacon = 0;
  posit_tx_disable = 0;
  strcpy(my_callsign, "");
  strcpy(my_lat, "");
  strcpy(my_long, "");
  my_group = '/';
  my_symbol = '-';
  strcpy(my_comment, "");
  strcpy(my_phg, "");
  my_last_course = 0;
  my_last_speed = 0;
  my_last_altitude = 0;
  my_last_altitude_time = 0;
  transmit_compressed_posit = 0;
  output_station_type = 0;
  
  /* Reset popup tracking */
  popup_count = 0;
  last_popup_title[0] = '\0';
  last_popup_message[0] = '\0';
  
  for (i = 0; i < MAX_IFACE_DEVICES; i++)
  {
    memset(&port_data[i], 0, sizeof(iface));
    memset(&devices[i], 0, sizeof(iodevices));
    memset(&mock_ports[i], 0, sizeof(mock_port_state_t));
    
    port_data[i].device_type = DEVICE_NONE;
    port_data[i].status = DEVICE_DOWN;
    devices[i].transmit_data = 0;
    strcpy(devices[i].device_converse_string, "CONV");
  }
}

void mock_set_transmit_disable(int value)
{
  transmit_disable = value;
}

void mock_set_emergency_beacon(int value)
{
  emergency_beacon = value;
}

void mock_set_my_callsign(const char *callsign)
{
  strncpy(my_callsign, callsign, MAX_CALLSIGN);
  my_callsign[MAX_CALLSIGN] = '\0';
}

void mock_set_my_position(const char *lat, const char *lon)
{
  strncpy(my_lat, lat, sizeof(my_lat)-1);
  my_lat[sizeof(my_lat)-1] = '\0';
  strncpy(my_long, lon, sizeof(my_long)-1);
  my_long[sizeof(my_long)-1] = '\0';
}

void mock_set_output_station_type(int type)
{
  output_station_type = type;
}

void mock_add_interface(int port, int device_type, int status, int transmit_enabled)
{
  if (port < 0 || port >= MAX_IFACE_DEVICES)
    return;
    
  port_data[port].device_type = device_type;
  port_data[port].status = status;
  port_data[port].active = 1;
  port_data[port].write_in_pos = 0;
  port_data[port].write_out_pos = 0;
  
  devices[port].transmit_data = transmit_enabled;
  devices[port].device_type = device_type;
  
  // Set up some defaults
  strcpy(devices[port].unproto1, "WIDE1-1,WIDE2-1");
  devices[port].unprotonum = 0;
  strcpy(devices[port].device_converse_string, "CONV");
  
  fprintf(stderr, "DEBUG mock_add_interface: port=%d, type=%d, status=%d, tx=%d\n",
          port, device_type, status, transmit_enabled);
}

int mock_get_write_count(int port)
{
  if (port < 0 || port >= MAX_IFACE_DEVICES)
    return 0;
  
  /* Debug: print buffer positions */
  if (debug_level & 2)
  {
    fprintf(stderr, "mock_get_write_count: Port %d: write_in_pos=%d, write_out_pos=%d, error_count=%d\n",
            port, port_data[port].write_in_pos, port_data[port].write_out_pos, port_data[port].errors);
  }
  
  /* Check if anything was written to the device buffer */
  if (port_data[port].write_in_pos != port_data[port].write_out_pos)
    return 1;
  
  return 0;
}

const char *mock_get_last_write(int port)
{
  static char buffer[MAX_DEVICE_BUFFER];
  int start, end, i, j;
  
  if (port < 0 || port >= MAX_IFACE_DEVICES)
    return NULL;
  
  start = port_data[port].write_out_pos;
  end = port_data[port].write_in_pos;
  
  if (start == end)
    return NULL;
  
  /* Copy data from circular buffer to linear buffer */
  j = 0;
  i = start;
  while (i != end && j < MAX_DEVICE_BUFFER - 1)
  {
    buffer[j++] = port_data[port].device_write_buffer[i];
    i++;
    if (i >= MAX_DEVICE_BUFFER)
      i = 0;
  }
  buffer[j] = '\0';
  
  return buffer;
}

int mock_get_popup_count(void)
{
  return popup_count;
}

const char *mock_get_last_popup_title(void)
{
  return last_popup_title;
}

const char *mock_get_last_popup_message(void)
{
  return last_popup_message;
}

/* Mock implementations of external functions */

int begin_critical_section(xastir_mutex *lock, char *msg)
{
  /* Mock - return 0 to indicate success */
  return 0;
}

int end_critical_section(xastir_mutex *lock, char *msg)
{
  /* Mock - return 0 to indicate success */
  return 0;
}

time_t sec_now(void)
{
  return time(NULL);
}

void popup_message_always(char *title, char *message)
{
  /* Track popup calls */
  popup_count++;
  strncpy(last_popup_title, title, sizeof(last_popup_title) - 1);
  last_popup_title[sizeof(last_popup_title) - 1] = '\0';
  strncpy(last_popup_message, message, sizeof(last_popup_message) - 1);
  last_popup_message[sizeof(last_popup_message) - 1] = '\0';
  
  /* In tests, also print to stderr for debugging */
  fprintf(stderr, "POPUP: %s - %s\n", title, message);
}

char *langcode(char *code)
{
  /* Return the code itself as the "translated" string */
  return (char *)code;
}

int xastir_snprintf(char *str, size_t size, const char *format, ...)
{
  va_list args;
  int result;
  
  va_start(args, format);
  result = vsnprintf(str, size, format, args);
  va_end(args);
  
  return result;
}

long convert_lat_s2l(char *lat)
{
  /* Simple mock - just return a fixed value */
  return 47000000; /* ~47 degrees */
}

long convert_lon_s2l(char *lon)
{
  /* Simple mock - just return a fixed value */
  return -122000000; /* ~-122 degrees */
}

void convert_lat_l2s(long lat, char *str, int str_len, int type)
{
  /* Simple mock conversion */
  snprintf(str, str_len, "4700.00N");
}

void convert_lon_l2s(long lon, char *str, int str_len, int type)
{
  /* Simple mock conversion */
  snprintf(str, str_len, "12200.00W");
}

char *output_lat(char *lat, int compressed)
{
  /* Mock - just return success */
  return lat;
}

char *output_long(char *lon, int compressed)
{
  /* Mock - just return success */
  return lon;
}

char *compress_posit(const char *input_lat, const char group, const char *input_lon, const char symbol,
                     const unsigned int last_course, const unsigned int last_speed, const char *phg)
{
  /* Return a mock compressed position */
  static char compressed[20];
  
  snprintf(compressed, sizeof(compressed), "/5L!!<*e7>");
  return compressed;
}

void makePrintable(char *str)
{
  /* Mock - just ensure it's printable by removing control chars */
  char *p = str;
  while (*p)
  {
    if (*p == '\r' || *p == '\n')
      *p = ' ';
    p++;
  }
}

void packet_data_add(const char *from, char *data, int port)
{
  /* Mock - do nothing, just for incoming data display */
}

time_t wx_tx_data1(char *wx_data, int wx_data_size)
{
  /* Mock - return 0 to indicate no weather data */
  return 0;
}

/* Additional stubs for other functions interface.c needs */
void *LOGFILE_NET = NULL;
void *LOGFILE_TNC = NULL;
int altnet = 0;
char altnet_call[MAX_CALLSIGN+1] = "";
Widget appshell;
void busy_cursor(void *w) { (void)w; }
int check_unproto_path(char* data) { return 1; }
void decode_ax25_header(unsigned char *data, int *from_type, char *call) {}
void decode_ax25_line(char *line, int line_len, int port){}
int egid = 0;
int enable_server_port = 0;
int euid = 0;
int filethere(char *path) { return 0; }
void forked_freeaddrinfo(void *res) { (void)res; }
int forked_getaddrinfo(const char *node, const char *service,
                      const void *hints, void **res)
{
  return -1;
}
char *get_data_base_dir(char *dir) 
{ 
  strcpy(dir, "/tmp"); 
  return dir; 
}
char *get_user_base_dir(char *dir, char *path, size_t pathsize)
{
  strcpy(dir, "/tmp");
  return dir;
}
char gpgga_save_string[MAX_LINE_SIZE+1] = "";
char gprmc_save_string[MAX_LINE_SIZE+1] = "";
int gps_port_save = 0;
void init_critical_section(xastir_mutex *lock){}

int isGGA(char *line) { (void)line; return 0; }
int isRMC(char *line) { (void)line; return 0; }
void log_data(char *file, char *line)
{
  (void)file; (void)line;
}
int log_net_data = 0;
int log_tnc_data = 0;
int my_position_valid = 0;
int pipe_xastir_to_tcp_server = 0;
void popup_message(char *title, char *message)
{
  fprintf(stderr, "POPUP: %s - %s\n", title, message);
}
int serial_char_pacing = 0;
void split_string ( char *data, char *cptr[], int max, char search_char )
{
  char *start = data;
  char *end;
  int count = 0;

  while ((end = strchr(start, search_char)) != NULL && count < max)
  {
    *end = '\0';
    cptr[count++] = start;
    start = end + 1;
  }
  if (count < max)
    cptr[count] = start;
}
void statusline(const char *msg, int clear){}
void substr(char *dest, char *src, int size)
{
  memcpy(dest, src, size);
  dest[size] = '\0';
}
char *to_upper(char *str)
{
  return str;
}
void update_interface_list(void) { }
int using_gps_position = 0;
int writen(int fd, const void *buf, size_t count)
{
  (void)fd; (void)buf; (void)count;
  return 0;
}
