/*
 * $Id: dlm.h,v 1.5 2018/07/14 21:32:43 MikeNix Exp $
 *
 * Copyright (C) 2018 The Xastir Group
 *
 * This file was contributed by Mike Nix.
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
 *
 */
#ifndef DLM_H
#define DLM_H

#include <X11/X.h>           // for KeySym

int DLM_wait_done(time_t timeout);
int DLM_check_progress(void);
int DLM_queue_len(void);
void DLM_queue_abort(void);
void DLM_queue_abort_tiles(void);
void DLM_queue_abort_files(void);
void DLM_do_transfers(void);

void DLM_queue_tile(
  char            *serverURL,
  unsigned long   x,
  unsigned long   y,
  int             osm_zl,
  char            *baseDir,
  char            *ext
);

void DLM_queue_file(
  char      *url,
  char      *filename,
  time_t    expiry
);

#endif //DLM_H
