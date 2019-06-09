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
#include <stdio.h>
#include <math.h>

#ifndef M_PI
  #define M_PI 3.1415926535
#endif
#ifndef ABS
  #define ABS(a) ((a) > 0 ? (a) : -(a))
#endif

#define EP .0000000001

const double log_pi = log(M_PI);

double sphere_volume(double dimension)
{
  double log_gamma, log_volume;
  log_gamma = gamma(dimension/2.0 + 1);
  log_volume = dimension/2.0 * log_pi - log_gamma;
  return exp(log_volume);
}


int main(void)
{
  double dim=0, delta=1;
  while(ABS(delta) > EP)
    if(sphere_volume(dim + delta) > sphere_volume(dim))
    {
      dim += delta;
    }
    else
    {
      delta /= -2;
    }
  printf("max volume = %.10f at dimension %.10f\n",
         sphere_volume(dim), dim);
  return 0;
}
