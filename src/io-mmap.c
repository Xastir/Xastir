


/* Copyright 2002 Daniel Egnor.  See LICENSE.geocoder file.
 * Portions Copyright (C) 2000-2019 The Xastir Group
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "io.h"
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include "xastir.h"

// Must be last include file
#include "leak_detection.h"



struct io_file
{
  int fd;
  int prot;
  off_t file_size;
  void *map;
  off_t map_offset;
  size_t map_size,map_page;
  char buffer[4096];
  off_t buffer_offset;
  size_t buffer_size;
};

/* Writes any buffered append data.
   Returns nonzero iff failed. */
static int unbuffer(struct io_file *f)
{
  int r;
  if (!f->buffer_size)
  {
    return 0;
  }
  r = write(f->fd,f->buffer,f->buffer_size);
  if (r > 0)
  {
    f->buffer_offset += r;
    f->buffer_size -= r;
    if (f->buffer_offset > f->file_size)
    {
      f->file_size = f->buffer_offset;
    }
  }

  if (0 != f->buffer_size)
  {
    perror("write");
    return -1;
  }

  return 0;
}

/* Resets f->file_size to the actual size of the file.
   Returns nonzero iff failed. */
static int checksize(struct io_file *f)
{
  struct stat buf;
  if (unbuffer(f))
  {
    return -1;
  }
  if (fstat(f->fd,&buf))
  {
    perror("fstat");
    return -1;
  }

  f->file_size = buf.st_size;
  return 0;
}

struct io_file *io_open(const char *fname)
{
  struct io_file * const f = malloc(sizeof *f);

  if (NULL == f)
  {
    return NULL;
  }
  f->fd = open(fname,O_RDWR|O_CREAT,0666);
  f->prot = PROT_READ|PROT_WRITE;
  if (f->fd < 0)
  {
    f->fd = open(fname,O_RDONLY);
    f->prot = PROT_READ;
  }
  if (f->fd < 0)
  {
    perror(fname);
    free(f);
    return NULL;
  }

  f->map = NULL;
  f->map_offset = 0;
  f->map_size = 0;
  f->map_page = getpagesize();
  f->file_size = 0;
  f->buffer_offset = 0;
  f->buffer_size = 0;
  if (checksize(f))
  {
    close(f->fd);
    free(f);
    return NULL;
  }

  return f;
}

void io_close(struct io_file *f)
{
  if (NULL != f)
  {
    unbuffer(f);
    close(f->fd);
    if (NULL != f->map)
    {
      munmap(f->map,f->map_size);
    }
    free(f);
  }
}

/* Attempts to make the mapping cover [pos,pos+len), or at least [pos].
   Returns nonzero iff failed. */
static int remap(struct io_file *f,int pos,int UNUSED(len) )
{
  if (pos < (int)f->map_offset || pos >= (int)(f->map_offset + f->map_size) )
  {
    const int flags = MAP_SHARED;

    const off_t b1 = pos / f->map_page * f->map_page;
    const off_t e1 = b1 + f->map_page;

    off_t b2 = f->map_offset;
    off_t e2 = b2 + f->map_size;
    if (b2 > b1)
    {
      b2 = b1 - (b2 - b1);
    }
    if (b2 < 0)
    {
      b2 = 0;
    }
    if (e2 < e1)
    {
      e2 = e1 + (e1 - e2);
    }

    if (NULL != f->map)
    {
      munmap(f->map,f->map_size);
    }
    if (MAP_FAILED != (f->map = mmap(NULL,e2 - b2,f->prot,flags,f->fd,b2)))
    {
      f->map_size = e2 - b2;
      f->map_offset = b2;
    }
    else if (MAP_FAILED != (f->map = mmap(NULL,e1 - b1,f->prot,flags,f->fd,b1)))
    {
      f->map_size = e1 - b1;
      f->map_offset = b1;
    }
    else
    {
      perror("mmap");
      f->map = NULL;
      f->map_size = 0;
      f->map_offset = 0;
      return -1;
    }
  }

  return 0;
}

int io_out(struct io_file *f,int pos,const void *o,int len)
{
  int end = pos + len;
  if (0 == len)
  {
    return pos;
  }
  if (NULL == f || -1 == pos)
  {
    return -1;
  }
  if (NULL == o)
  {
    return pos + len;
  }
  if (remap(f,pos,len))
  {
    return -1;
  }

  if (pos < f->file_size)
  {
    assert(pos >= f->map_offset);
    if (pos >= f->buffer_offset && unbuffer(f))
    {
      return -1;
    }
    if (end > (int)(f->map_offset + f->map_size))
    {
      end = f->map_offset + f->map_size;
    }
    if (end > f->file_size)
    {
      end = f->file_size;
    }
    memcpy(pos - f->map_offset + (char *) f->map,o,end - pos);
  }
  else
  {
    if (pos < f->buffer_offset
        ||  pos > (int)(f->buffer_offset + f->buffer_size))
    {
      if (unbuffer(f))
      {
        return -1;
      }
      if (lseek(f->fd,pos,SEEK_SET) == (off_t) -1)
      {
        perror("lseek");
        return -1;
      }

      assert(0 == f->buffer_size);
      f->buffer_offset = pos;
    }
    else if (pos == (int)(f->buffer_offset + sizeof f->buffer))
      if (unbuffer(f))
      {
        return -1;
      }

    assert(pos >= f->buffer_offset
           && pos <= (int)(f->buffer_offset + f->buffer_size));
    if (end > (int)(f->buffer_offset + sizeof f->buffer))
    {
      end = f->buffer_offset + sizeof f->buffer;
    }
    memcpy(&f->buffer[pos - f->buffer_offset],o,end - pos);
    if ((int)(end - f->buffer_offset) > (int)f->buffer_size)
    {
      f->buffer_size = end - f->buffer_offset;
    }
  }

  return io_out(f,end,end - pos + (char *) o,len + pos - end);
}

int io_in(struct io_file *f,int pos,void *i,int len)
{
  int end = pos + len;
  if (0 == len)
  {
    return pos;
  }
  if (NULL == f || -1 == pos)
  {
    return -1;
  }
  if (NULL == i)
  {
    return pos + len;
  }
  if (remap(f,pos,len))
  {
    return -1;
  }
  if (unbuffer(f))
  {
    return -1;
  }

  if (pos >= f->file_size && checksize(f))
  {
    return -1;
  }
  if (pos >= f->file_size)
  {
    /* TODO: fill with zeroes instead */
    fputs("read: EOF\n",stderr);
    return -1;
  }

  if (end > (int)(f->map_offset + f->map_size))
  {
    end = f->map_offset + f->map_size;
  }
  if (end > f->file_size)
  {
    end = f->file_size;
  }
  memcpy(i,pos - f->map_offset + (char *) f->map,end - pos);
  return io_in(f,end,end - pos + (char *) i,len + pos - end);
}
