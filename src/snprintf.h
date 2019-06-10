/*
 * snprintf.h
 *   header file for snprintf.c
 *
 */
/*
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
 */

#ifndef _XASTIR_COMPAT_SNPRINTF_H_
#define _XASTIR_COMPAT_SNPRINTF_H_

#include <stdio.h>
#include "config.h"

#ifdef HAVE_STDARG_H
  #include <stdarg.h>
#endif  // HAVE_STDARG_H

/* Use the system libraries version of vsnprintf() if available. Otherwise
 * use our own.
 */
#ifndef HAVE_VSNPRINTF
  int xastir_vsnprintf(char *str, size_t count, const char *fmt, va_list ap);
#else   // HAVE_VSNPRINTF
  #define xastir_vsnprintf vsnprintf
#endif  // HAVE_VSNPRINTF

/* Use the system libraries version of snprintf() if available. Otherwise
 * use our own.
 */
#ifndef HAVE_SNPRINTF
  #ifdef __STDC__
    int xastir_snprintf(char *str, size_t count, const char *fmt, ...);
  #else // __STDC__
    int xastir_snprintf();
  #endif    // __STDC__
#else   // HAVE_SNPRINTF
  #define xastir_snprintf snprintf
#endif  // HAVE_SNPRINTF

#endif  /* !XASTIR_COMPAT_SNPRINTF_H_ */


