/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
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
/****************************************************************************
 * MODULE:       R-Tree library
 *
 * AUTHOR(S):    Antonin Guttman - original code
 *               Melinda Green (melinda@superliminal.com) - major clean-up
 *                               and implementation of bounding spheres
 *
 * PURPOSE:      Multidimensional index
 *
 */

/*
 *                   SPHERE VOLUME
 *                   by Melinda Green
 *                   melinda@superliminal.com
 *
 * Calculates and prints the volumes of the unit hyperspheres for
 * dimensions zero through the given value, or 9 by default.
 * Prints in the form of a C array of double called sphere_volumes.
 *
 * From formule in "Regular Polytopes" by H.S.M Coxeter, the volume
 * of a hypersphere of dimension d is:
 *        Pi^(d/2) / gamma(d/2 + 1)
 *
 * This implementation works by first computing the log of the above
 * function and then returning the exp of that value in order to avoid
 * instabilities due to the huge values that the real gamma function
 * would return.
 *
 * Multiply the output volumes by R^n to get the volume of an n
 * dimensional sphere of radius R.
 */

#include <stdio.h>
#include <math.h>

#ifndef M_PI
  #define M_PI 3.1415926535
#endif

static void print_volume(int dimension, double volume)
{
  printf("\t%.6f,  /* dimension %3d */\n", volume, dimension);
}

static double sphere_volume(double dimension)
{
  const double log_pi = log(M_PI);
  double log_gamma, log_volume;
  log_gamma = gamma(dimension/2.0 + 1);
  log_volume = dimension/2.0 * log_pi - log_gamma;
  return exp(log_volume);
}

extern int main(int argc, char *argv[])
{
  int dim, max_dims=9;

  if(2 == argc)
  {
    max_dims = atoi(argv[1]);
  }

  printf("static const double sphere_volumes[] = {\n");
  for(dim=0; dim<max_dims+1; dim++)
  {
    print_volume(dim, sphere_volume(dim));
  }
  printf("};\n");
  return 0;
}
