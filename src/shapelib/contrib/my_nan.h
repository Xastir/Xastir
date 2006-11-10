/* `NAN' constant for IEEE 754 machines.
   Copyright (C) 1992, 1996 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef	_GNU_NAN_H

#define	_GNU_NAN_H	1


/* hacked to define NAN on Solaris 2.7 if it wasn't defined  */

/* IEEE Not A Number.  */

#ifdef _BIG_ENDIAN
#  define	__nan_bytes		{ 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 }
#endif
#ifdef _LITTLE_ENDIAN
#  define	__nan_bytes		{ 0, 0, 0, 0, 0, 0, 0xf8, 0x7f }
#endif

#ifdef	__GNUC__
#  define	NAN \
  (__extension__ ((union { unsigned char __c[8];			      \
			   double __d; })				      \
		  { __nan_bytes }).__d)
#else	/* Not GCC.  */
   static const char __nan[8] = __nan_bytes;
#  define	NAN	(*(const double *) __nan)
#endif

#endif	/* gnu_nan.h */
