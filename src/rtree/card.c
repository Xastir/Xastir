/* -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
 * $Id$
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2005  The Xastir Group
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
 *               Daniel Green (green@superliminal.com) - major clean-up
 *                               and implementation of bounding spheres
 *               
 * PURPOSE:      Multidimensional index
 *
 */

#include "index.h"
#include "card.h"

int NODECARD = MAXCARD;
int LEAFCARD = MAXCARD;

static int set_max(int *which, int new_max)
{
	if(2 > new_max || new_max > MAXCARD)
		return 0;
	*which = new_max;
	return 1;
}

int RTreeSetNodeMax(int new_max) { return set_max(&NODECARD, new_max); }
int RTreeSetLeafMax(int new_max) { return set_max(&LEAFCARD, new_max); }
int RTreeGetNodeMax() { return NODECARD; }
int RTreeGetLeafMax() { return LEAFCARD; }
