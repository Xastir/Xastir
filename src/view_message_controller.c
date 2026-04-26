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
 * Presentation-free kernel extracted from view_message_gui.c.
 * No Motif, no X11, no Xastir globals — standard C only.
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "view_message_controller.h"


/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void view_message_controller_init(view_message_controller_t *vc)
{
  if (!vc)
  {
    return;
  }
  vc->range            = 0;
  vc->message_limit    = 10000;
  vc->packet_data_type = VM_SOURCE_ALL;
  vc->mine_only        = 0;
}


/* ------------------------------------------------------------------ */
/* Pure helpers                                                        */
/* ------------------------------------------------------------------ */

char *view_message_strip_ssid(const char *src, char *dst, size_t dstsz)
{
  const char *p;
  size_t      n;

  if (!dst || dstsz == 0)
  {
    return dst;
  }

  dst[0] = '\0';

  if (!src)
  {
    return dst;
  }

  p = strchr(src, '-');
  n = p ? (size_t)(p - src) : strlen(src);

  if (n >= dstsz)
  {
    n = dstsz - 1;
  }

  memcpy(dst, src, n);
  dst[n] = '\0';
  return dst;
}


/* ------------------------------------------------------------------ */
/* Filtering                                                           */
/* ------------------------------------------------------------------ */

int view_message_controller_should_display(
    const view_message_controller_t *vc,
    char        from,
    const char *call_sign,
    const char *from_call,
    int         distance,
    const char *my_callsign_base)
{
  if (!vc)
  {
    return 0;
  }

  /* ---- Distance / mine-only pre-filter ---- */
  if (!vc->mine_only)
  {
    /* Range > 0: skip messages beyond the configured distance. */
    if (vc->range != 0 && distance > vc->range)
    {
      return 0;
    }
  }
  /* When mine_only is set the range check is bypassed entirely,
   * matching the original legacy behaviour. */

  /* ---- Packet-source filter ---- */
  switch (vc->packet_data_type)
  {
    case VM_SOURCE_NET:
      /* Internet data only — drop anything that didn't arrive via 'I'. */
      if (from != 'I')
      {
        return 0;
      }
      break;

    case VM_SOURCE_TNC:
      /* TNC data only — drop anything that didn't arrive via 'T'. */
      if (from != 'T')
      {
        return 0;
      }
      break;

    case VM_SOURCE_ALL:
    default:
      break;
  }

  /* ---- Mine-only callsign filter ---- */
  if (vc->mine_only)
  {
    /* Require valid pointers; missing base callsign means nothing matches. */
    if (!my_callsign_base || my_callsign_base[0] == '\0')
    {
      return 0;
    }
    if (!call_sign || !from_call)
    {
      return 0;
    }
    if (!strstr(call_sign, my_callsign_base)
        && !strstr(from_call, my_callsign_base))
    {
      return 0;
    }
  }

  return 1;
}


/* ------------------------------------------------------------------ */
/* Formatting                                                          */
/* ------------------------------------------------------------------ */

int view_message_format_record(
    const char *from_call,
    const char *call_sign,
    const char *seq,
    char        type,
    const char *message_line,
    const char *seq_label,
    const char *via_label,
    char       *out,
    size_t      outsz)
{
  if (!from_call || !call_sign || !seq || !message_line
      || !seq_label || !via_label || !out || outsz == 0)
  {
    return -1;
  }

  snprintf(out, outsz, "%-9s>%-9s %s:%5s %s:%c :%s\n",
           from_call, call_sign,
           seq_label, seq,
           via_label, type,
           message_line);
  return 0;
}


int view_message_format_line(
    char        from,
    const char *call_sign,
    const char *from_call,
    const char *message,
    const char *broadcast_label,
    char       *out,
    size_t      outsz)
{
  /* data1: first ≤95 chars; data2: "\n\t" + remainder (may be empty). */
  char        data1[97];
  char        data2[200];
  const char *effective_call;
  char        from_str[3];
  int         is_broadcast;

  if (!call_sign || !from_call || !message || !out || outsz == 0)
  {
    return -1;
  }

  /* ---- Split long messages at char 95 ---- */
  if (strlen(message) > 95)
  {
    snprintf(data1, sizeof(data1), "%s", message);
    data1[95] = '\0';
    snprintf(data2, sizeof(data2), "\n\t%s", message + 95);
  }
  else
  {
    snprintf(data1, sizeof(data1), "%s", message);
    data2[0] = '\0';
  }

  /* ---- Detect broadcast sources ---- */
  is_broadcast = (strncmp(call_sign, "java", 4) == 0
                  || strncmp(call_sign, "USER", 4) == 0);

  if (is_broadcast)
  {
    effective_call = (broadcast_label != NULL) ? broadcast_label : call_sign;
    snprintf(out, outsz, "%s %s\t%s%s\n",
             from_call, effective_call, data1, data2);
  }
  else
  {
    from_str[0] = from;
    from_str[1] = '\0';
    snprintf(out, outsz, "%s to %s via:%s\t%s%s\n",
             from_call, call_sign, from_str, data1, data2);
  }

  return 0;
}
