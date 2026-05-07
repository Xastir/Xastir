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

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "popup_controller.h"

void popup_controller_init(popup_controller_t *pc)
{
  if (pc == NULL)
  {
    return;
  }
  memset(pc, 0, sizeof(*pc));
}

void popup_controller_clear(popup_controller_t *pc)
{
  int i;

  if (pc == NULL)
  {
    return;
  }
  for (i = 0; i < POPUP_MAX_SLOTS; i++)
  {
    pc->slots[i].active = 0;
    pc->slots[i].sec_opened = 0;
  }
}

int popup_controller_find_free_slot(const popup_controller_t *pc)
{
  int i;

  if (pc == NULL)
  {
    return -1;
  }
  for (i = 0; i < POPUP_MAX_SLOTS; i++)
  {
    if (!pc->slots[i].active)
    {
      return i;
    }
  }
  return -1;
}

int popup_controller_slot_is_active(const popup_controller_t *pc, int idx)
{
  if (pc == NULL || idx < 0 || idx >= POPUP_MAX_SLOTS)
  {
    return 0;
  }
  return pc->slots[idx].active ? 1 : 0;
}

void popup_controller_open_slot(popup_controller_t *pc, int idx, time_t now)
{
  if (pc == NULL || idx < 0 || idx >= POPUP_MAX_SLOTS)
  {
    return;
  }
  pc->slots[idx].active = 1;
  pc->slots[idx].sec_opened = now;
}

void popup_controller_close_slot(popup_controller_t *pc, int idx)
{
  if (pc == NULL || idx < 0 || idx >= POPUP_MAX_SLOTS)
  {
    return;
  }
  pc->slots[idx].active = 0;
  pc->slots[idx].sec_opened = 0;
}

int popup_controller_should_run_timeout_check(const popup_controller_t *pc, time_t now)
{
  if (pc == NULL)
  {
    return 0;
  }
  return (pc->last_timeout_check + POPUP_TIMEOUT_CHECK_INTERVAL < now) ? 1 : 0;
}

void popup_controller_mark_timeout_check(popup_controller_t *pc, time_t now)
{
  if (pc == NULL)
  {
    return;
  }
  pc->last_timeout_check = now;
}

int popup_controller_slot_expired(const popup_controller_t *pc, int idx, time_t now)
{
  if (pc == NULL || idx < 0 || idx >= POPUP_MAX_SLOTS)
  {
    return 0;
  }
  if (!pc->slots[idx].active)
  {
    return 0;
  }
  return ((now - pc->slots[idx].sec_opened) > POPUP_MAX_AGE_SECONDS) ? 1 : 0;
}

int popup_controller_message_valid(const char *banner, const char *message)
{
  return (banner != NULL && message != NULL) ? 1 : 0;
}

void popup_controller_format_slot_name(int idx, char *out, size_t outsz)
{
  if (out == NULL || outsz == 0)
  {
    return;
  }
  snprintf(out, outsz, "%9d", idx % 1000);
}
