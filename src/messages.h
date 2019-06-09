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

#ifndef XASTIR_MESSAGES_H
#define XASTIR_MESSAGES_H

/*
 * Message structures
 *
 */

/* define MESSAGE STATUS */
#define MESSAGE_ACTIVE  'A'
#define MESSAGE_CLEAR 'C'

#define MAX_OUTGOING_MESSAGES 100
#define MAX_MESSAGE_OUTPUT_LENGTH 67
#define MAX_MESSAGE_ORDER 10

// Max tries to get a message through
#define MAX_TRIES 18

typedef struct
{
  char active;
  char to_call_sign[MAX_CALLSIGN+1];
  char from_call_sign[MAX_CALLSIGN+1];
  char message_line[MAX_MESSAGE_OUTPUT_LENGTH+1];
  char path[200];
  char seq[MAX_MESSAGE_ORDER+1];
  time_t active_time;
  time_t next_time;
  int tries;
  int wait_on_first_ack;
} Message_transmit;

#define MAX_MESSAGE_WINDOWS 25

typedef struct
{
  char win[10];
  char to_call_sign[MAX_CALLSIGN+1];
  int message_group;
  Widget send_message_dialog;
  Widget send_message_call_data;
  Widget D700_mode;
  Widget D7_mode;
  Widget HamHUD_mode;
  Widget message_data_line1;
  Widget message_data_line2;
  Widget message_data_line3;
  Widget message_data_line4;
  Widget send_message_text;
  Widget send_message_path;
  Widget send_message_reverse_path;
  Widget send_message_change_path;
  Widget pane, form, button_ok, button_cancel;
  Widget button_clear_old_msgs, button_submit_call;
  Widget button_clear_pending_msgs;
  Widget button_kick_timer;
  Widget call, message, path, reverse_path_label;
} Message_Window;


extern Message_transmit message_pool[MAX_OUTGOING_MESSAGES+1];

extern Widget auto_msg_on, auto_msg_off;

extern int auto_reply;
extern char auto_reply_message[100];
extern char group_data_file[400];

extern void clear_acked_message(char *from, char *to, char *seq);
extern void transmit_message_data(char *to, char *message, char *path);
extern void transmit_message_data_delayed(char *to, char *message, char *path, time_t when);
extern void check_delayed_transmit_queue(int curr_sec);


//extern void output_message(char *from, char *to, char *message);
extern int check_popup_window(char *from_call_sign, int group);
extern int look_for_open_group_data(char *to);
extern int group_active(char *from);
extern void send_queued(char *to);
extern void clear_outgoing_messages_to(char *callsign);
extern void change_path_outgoing_messages_to(char *callsign, char *new_path);

/* from messages_gui.c */
extern xastir_mutex send_message_dialog_lock;
extern void messages_gui_init(void);
extern void get_send_message_path(char *callsign, char *path, int path_size);
extern void Send_message(Widget w, XtPointer clientData, XtPointer callData);
extern void Show_pending_messages(Widget w, XtPointer clientData, XtPointer callData);
extern void Clear_messages(Widget w, XtPointer clientData, XtPointer callData);
void kick_outgoing_timer(char *callsign);
extern void Send_message_call(Widget w, XtPointer clientData, XtPointer callData);

// view_message_gui.c
extern int vm_range;
extern int view_message_limit;
extern int Read_messages_packet_data_type;
extern int Read_messages_mine_only;

#endif  /*  XASTIR_MESSAGES_H */

