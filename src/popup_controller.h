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
 * Do NOT include popup.h, xastir.h, or any Xt/Xm header from here.
 */

#ifndef XASTIR_POPUP_CONTROLLER_H
#define XASTIR_POPUP_CONTROLLER_H

#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POPUP_MAX_SLOTS               30
#define POPUP_MAX_AGE_SECONDS        600   /* slot auto-closes after this */
#define POPUP_TIMEOUT_CHECK_INTERVAL 120   /* sweep cadence */
#define POPUP_SLOT_NAME_SIZE          10   /* matches Popup_Window.name[10] */

typedef struct
{
  int    active;       /* 1 if slot currently holds a popup, 0 otherwise */
  time_t sec_opened;   /* wall-clock seconds at which the popup was opened */
} popup_slot_t;

typedef struct
{
  popup_slot_t slots[POPUP_MAX_SLOTS];
  time_t       last_timeout_check;
} popup_controller_t;

/* Lifecycle */
void popup_controller_init(popup_controller_t *pc);
void popup_controller_clear(popup_controller_t *pc);

/* Slot bookkeeping */
int  popup_controller_find_free_slot(const popup_controller_t *pc);
int  popup_controller_slot_is_active(const popup_controller_t *pc, int idx);
void popup_controller_open_slot(popup_controller_t *pc, int idx, time_t now);
void popup_controller_close_slot(popup_controller_t *pc, int idx);

/* Timeout sweep */
int  popup_controller_should_run_timeout_check(const popup_controller_t *pc, time_t now);
void popup_controller_mark_timeout_check(popup_controller_t *pc, time_t now);
int  popup_controller_slot_expired(const popup_controller_t *pc, int idx, time_t now);

/* Pure helpers */
int  popup_controller_message_valid(const char *banner, const char *message);
void popup_controller_format_slot_name(int idx, char *out, size_t outsz);

#ifdef __cplusplus
}
#endif

#endif /* XASTIR_POPUP_CONTROLLER_H */
