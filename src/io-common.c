
/* Copyright 2002 Daniel Egnor.  See LICENSE.geocoder file.
 * Portions Copyright (C) 2000-2019 The Xastir Group
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "io.h"
#include <ctype.h>

// Must be last include file
#include "leak_detection.h"





int io_out_i4(struct io_file *f,int pos,int o)
{
  unsigned char x[4];
  x[0] = o >> 24;
  x[1] = o >> 16;
  x[2] = o >> 8;
  x[3] = o;
  return io_out(f,pos,x,sizeof x);
}

int io_out_i2(struct io_file *f,int pos,short int o)
{
  unsigned char x[2];
  x[0] = o >> 8;
  x[1] = o;
  return io_out(f,pos,x,sizeof x);
}

int io_out_i1(struct io_file *f,int pos,signed char o)
{
  return io_out(f,pos,&o,sizeof o);
}

int io_in_i4(struct io_file *f,int pos,int *i)
{
  unsigned char x[4];
  const int r = io_in(f,pos,x,sizeof x);
  if (i)
  {
    *i = (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3];
  }
  return r;
}

int io_in_i2(struct io_file *f,int pos,short *i)
{
  unsigned char x[2];
  const int r = io_in(f,pos,x,sizeof x);
  if (i)
  {
    *i = (x[0] << 8) | x[1];
  }
  return r;
}

int io_in_i1(struct io_file *f,int pos,signed char *i)
{
  return io_in(f,pos,i,sizeof *i);
}

int io_strntoi(const char *str,int len)
{
  int r = 0;
  int sign = 1;
  const char * const end = str + len;
  while (end != str && isspace((int)*str))
  {
    ++str;
  }
  if (end != str && *str == '-')
  {
    (sign *= -1),++str;
  }
  if (end != str && *str == '+')
  {
    ++str;
  }
  while (end != str && isdigit((int)*str))
  {
    r = *str++ - '0' + 10 * r;
  }
  return r * sign;
}
