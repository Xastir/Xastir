
/* Copyright 2002 Daniel Egnor.  See LICENSE.geocoder file.
 * Portions Copyright (C) 2000-2019 The Xastir Group
 *
 * The geo_find() function is the query interface for turning addressees
 * into geographical locations using an 'address map' built from TIGER/Line
 * and FIPS-55 data by the geo-*-to-* programs. */

#ifndef GEOCODER_GEO_H
#define GEOCODER_GEO_H

#include "io.h"

extern long destination_coord_lat;
extern long destination_coord_lon;
extern int mark_destination;
extern int show_destination_mark;

extern char geocoder_map_filename[400];


struct geo_corner
{
  int address;
  double latitude,longitude;
};

/* Decoded address location. */
struct geo_location
{
  /* "Before" and "after" are the previous and next "control point";
   * these are the known locations, "at" is the interpolated point
   * corresponding to the supplied address. */
  struct geo_corner before,after,at;
  int zip_code; /* 0 if none found in address */
  char side;    /* 'L' or 'R' */
  char street_name[41],city_name[41],state_name[41];
  /* Empty if none found in address */
};

/* Arguments: m = Address map file to use
 *            a = Address string to parse
 *            len = Length of address string (in characters)
 *            out = Address of location to output
 * Returns: Nonzero iff an address was recognized and decoded. */
int geo_find(struct io_file *m,const char *a,int len,struct geo_location *out);

#endif
