/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2026 The Xastir Group
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
 * Presentation/logic separation: this header is intentionally free of
 * Motif/X11 dependencies so the controller can be unit-tested standalone.
 * Do NOT include xastir.h or any Xt/Xm header from here.
 */

#ifndef XASTIR_VIEW_MESSAGE_CONTROLLER_H
#define XASTIR_VIEW_MESSAGE_CONTROLLER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Packet-source filter values (match Read_messages_packet_data_type) */
#define VM_SOURCE_ALL  0   /* TNC + NET (default) */
#define VM_SOURCE_TNC  1   /* TNC only             */
#define VM_SOURCE_NET  2   /* NET only             */

/*
 * Filter state owned by the view-message subsystem.  In the current pass
 * the four persistent globals (vm_range, view_message_limit,
 * Read_messages_packet_data_type, Read_messages_mine_only) are still the
 * xa_config source-of-truth; the GUI syncs them into this struct before
 * each filtering decision.  A future pass will eliminate the globals and
 * wire xa_config directly to this struct.
 */
typedef struct
{
  int range;            /* 0 = no distance limit; otherwise km/mi max     */
  int message_limit;    /* maximum characters kept in the scroll area      */
  int packet_data_type; /* VM_SOURCE_ALL / VM_SOURCE_TNC / VM_SOURCE_NET   */
  int mine_only;        /* 0 = all stations; 1 = my callsign only          */
} view_message_controller_t;

/* Initialise with safe defaults (range=0, limit=10000, ALL, mine_only=0). */
void view_message_controller_init(view_message_controller_t *vc);

/*
 * Strip SSID suffix from a callsign (everything from '-' onwards).
 * Writes null-terminated result into dst (capacity dstsz); returns dst.
 * NULL src → dst set to "".
 */
char *view_message_strip_ssid(const char *src, char *dst, size_t dstsz);

/*
 * Returns 1 if a message should be displayed given the current filter
 * settings stored in *vc, 0 otherwise.
 *
 *   from              — data_via character ('I'=Internet, 'T'=TNC, …)
 *   call_sign         — destination callsign (to)
 *   from_call         — originating callsign (from)
 *   distance          — pre-computed distance from my station (caller's units)
 *   my_callsign_base  — my callsign with SSID already stripped (use
 *                       view_message_strip_ssid() before calling)
 *
 * All pointer args may be NULL; the function returns 0 when required
 * pointers are NULL rather than crashing.
 */
int view_message_controller_should_display(
    const view_message_controller_t *vc,
    char        from,
    const char *call_sign,
    const char *from_call,
    int         distance,
    const char *my_callsign_base);

/*
 * Format a message-record display line in the style used by
 * view_message_print_record():
 *
 *   "%-9s>%-9s <seq_label>:%5s <via_label>:%c :%s\n"
 *
 * seq_label / via_label are the localised column-header strings the
 * caller obtains from langcode() (or passes as literals in tests).
 *
 * Writes into out[outsz].  Returns 0 on success, -1 if any pointer
 * argument is NULL or outsz == 0.
 */
int view_message_format_record(
    const char *from_call,
    const char *call_sign,
    const char *seq,
    char        type,
    const char *message_line,
    const char *seq_label,
    const char *via_label,
    char       *out,
    size_t      outsz);

/*
 * Format an "all messages" display line in the style used by
 * all_messages():
 *
 *   Broadcast (call_sign starts "java" or "USER"):
 *     "<from_call> <broadcast_label>\t<message>\n"
 *     (broadcast_label used only when non-NULL; raw call_sign otherwise)
 *
 *   Normal:
 *     "<from_call> to <call_sign> via:<from>\t<message>\n"
 *
 * Messages longer than 95 characters are split: the first 95 go into
 * the tab-separated field and the remainder is appended on a new line
 * with a leading tab.
 *
 * Writes into out[outsz].  Returns 0 on success, -1 if a required
 * pointer is NULL or outsz == 0.
 */
int view_message_format_line(
    char        from,
    const char *call_sign,
    const char *from_call,
    const char *message,
    const char *broadcast_label,
    char       *out,
    size_t      outsz);

#ifdef __cplusplus
}
#endif

#endif /* XASTIR_VIEW_MESSAGE_CONTROLLER_H */
