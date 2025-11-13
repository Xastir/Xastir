/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2023 The Xastir Group
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

#if HAVE_SYS_TIME_H
  #include <sys/time.h>
#endif // HAVE_SYS_TIME_H
#include <time.h>

#include <stdio.h>

struct timeval timer_start;
struct timeval timer_stop;
struct timezone tz;



// Save the current time, used for timing code sections.
void start_timer(void)
{
  gettimeofday(&timer_start,&tz);
}





// Save the current time, used for timing code sections.
void stop_timer(void)
{
  gettimeofday(&timer_stop,&tz);
}





// Print the difference in the two times saved above.
void print_timer_results(void)
{
  fprintf(stderr,"Total: %f sec\n",
          (float)(timer_stop.tv_sec - timer_start.tv_sec +
                  ((timer_stop.tv_usec - timer_start.tv_usec) / 1000000.0) ));
}
