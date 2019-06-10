/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
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

/* All of the misc entry points to be included for all packages */

#ifndef _LEAK_DETECTION_H
#define _LEAK_DETECTION_H


// If libgc is installed, uncomment this next line to enable memory
// leak detection:
#define DETECT_MEMORY_LEAKS


// Defines for including the libgc garbage collection library.
// This enables automatic garbage collection of unused memory,
// very similar to the garbage collection in Java.  Get libgc from
// here:  http://www.hpl.hp.com/personal/Hans_Boehm/gc/
//
// This will cause stats to be printed every 60 seconds, 'cuz we
// call GC_collect via a macro from UpdateTime() once per minute:
// export GC_PRINT_STATS=1; xastir &
//
// Compile libgc with this option for more debugging output.  I
// didn't do so:  --enable-full_debug
//
// If we enable these thread options, Xastir won't link with the
// library.  Since we don't allocate dynamic memory in the child
// threads anyway, skip them.
// --enable-threads=posix --enable-thread-local-alloc --enable-parallel-mark
//
// Call GC_gcollect at appropriate points to check for leaks.  We do
// this via the CHECK_LEAKS macro called from main.c:UpdateTime.
//
//
// Note:  The thread includes must be done before the libgc includes
// as libgc redefines some thread stuff so that it cooperates with
// the garbage collector routines.  Any code module that does
// malloc's/free's or thread operations should include
// leak_detection.h as the last include if at all possible, and
// should not include pthread.h themselves.
//
#include <pthread.h>
#include <stdlib.h> /* Where malloc/free definitions reside */
#ifdef HAVE_DMALLOC
  #include <dmalloc.h>
#endif  // HAVE_DMALLOC
//
#ifdef HAVE_GC_H
  #ifdef HAVE_LIBGC

    // We use this define to enable code in *.c files
    #define USING_LIBGC

    // Set up for threads
    #define GC_THREADS

    #ifdef __LINUX__
      #define GC_LINUX_THREADS
    #endif  // __LINUX__

    //    #define _REENTRANT

    // Ask for more debugging
    #define GC_DEBUG

    #include <gc.h>
    #define malloc(n) GC_MALLOC(n)
    #define calloc(m,n) GC_MALLOC((m)*(n))
    #define free(p) GC_FREE(p)
    #define realloc(p,n) GC_REALLOC((p),(n))
    #define CHECK_LEAKS() GC_gcollect()

  #endif    // HAVE_LIBGC
#endif  // HAVE_GC_H



#endif /* LEAK_DETECTION_H */


