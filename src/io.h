
/* Copyright 2002 Daniel Egnor.  See LICENSE.geocoder file.
 * Portions Copyright (C) 2000-2019 The Xastir Group
 *
 * This file defines the I/O interface used for access to index files.
 * There are two implementations of this interface; io-mmap.c uses Unix
 * mmap() and is quite efficient, and io-stdio.c uses C stdio and is slower
 * but more portable.  Which one is used depends on which file is compiled. */

#ifndef GEOCODER_IO_H
#define GEOCODER_IO_H

struct io_file;

/* Open a file.  The file will be created if it did not exist.
 * It will be opened R/W if possible, R/O otherwise.
 * Returns NULL on error.*/

struct io_file *io_open(const char *fname);
void io_close(struct io_file *);

/* In general, all I/O is done with an offset ("pos") directly.  There is
 * no "current position" associated with the file handle.  This is really
 * a lot more convenient for random access.
 *
 * Functions almost always return the position just after the data that was
 * read or written, so they may be easily "chained" to read or write several
 * contiguous items; this makes it almost as easy to do this as it would be
 * with a file pointer, which keeping things more flexible.
 *
 * If an error occurs, the return value will be -1.  If the input "pos"
 * is -1, these functions will silently return -1, so you can safely wait
 * until the end of a "chain" to check for errors, if you want.
 *
 * Integer values are written in byte-order-independent fashion. */

int io_out(struct io_file *,int pos,const void *o,int len); /* Write data */
int io_out_i4(struct io_file *,int pos,int o);              /* Write an int */
int io_out_i2(struct io_file *,int pos,short o);            /* Write a short */
int io_out_i1(struct io_file *,int pos,signed char o);      /* Write a char */

int io_in(struct io_file *,int pos,void *i,int len);        /* Read data */
int io_in_i4(struct io_file *,int pos,int *i);              /* Read an int */
int io_in_i2(struct io_file *,int pos,short *i);            /* Read a short */
int io_in_i1(struct io_file *,int pos,signed char *i);      /* Read a char */

/* Convert a string to integer.  Like strtol(), only with a count.
 * Kept here, 'cuz ... where else? */
int io_strntoi(const char *str,int len);

#endif
