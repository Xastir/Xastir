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

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <netinet/in.h>

#include <Xm/XmAll.h>

#include "xastir.h"
#include "main.h"
#include "track_gui.h"

// Must be last include file
#include "leak_detection.h"



static long last_center_longitude;     // remember last screen settings
static long last_center_latitude;
static long last_scale_x;
static long last_scale_y;


/***********************************************************/
/* set last map position                                   */
/* store lat long and zoom                                 */
/***********************************************************/
void set_last_position(void)
{
  last_center_longitude=center_longitude;
  last_center_latitude=center_latitude;
  last_scale_x = scale_x;     // we don't restore this...
  last_scale_y = scale_y;
}



/***********************************************************/
/* reset map to last position                              */
/*                                                         */
/***********************************************************/
void map_pos_last_position(void)
{

  map_pos(last_center_latitude,last_center_longitude,last_scale_y);
}



/***********************************************************/
/* Jump map to position                                    */
/*                                                         */
/***********************************************************/
void map_pos(long mid_y, long mid_x, long sz)
{
  // see also set_map_position() in db.c

  // Set interrupt_drawing_now because conditions have changed
  // (new map center).
  interrupt_drawing_now++;

  set_last_position();
  center_longitude = mid_x;
  center_latitude  = mid_y;
  scale_y = sz;
  scale_x = get_x_scale(mid_x,mid_y,scale_y);
  setup_in_view();  // flag all stations in screen view

  // Request that a new image be created.  Calls create_image,
  // XCopyArea, and display_zoom_status.
  request_new_image++;

//    if (create_image(da)) {
//        // We don't care whether or not this succeeds?
//        (void)XCopyArea(XtDisplay(da),pixmap_final,XtWindow(da),gc,0,0,(unsigned int)screen_width,(unsigned int)screen_height,0,0);
//        display_zoom_status();
//    }
}

