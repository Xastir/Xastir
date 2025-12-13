/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025 The Xastir Group
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

#ifndef XASTIR_NOMINATIM_H
#define XASTIR_NOMINATIM_H

#ifdef HAVE_NOMINATIM

#include "geocoder.h"

/*
 * Nominatim Geocoding Service Implementation
 * 
 * This module provides Nominatim-specific implementation for the abstract
 * geocoding API. It handles:
 * - HTTP requests to Nominatim servers
 * - JSON response parsing
 * - Rate limiting (1 request/second per Nominatim usage policy)
 * - Result caching
 * - Address component extraction
 */

/**
 * Initialize Nominatim subsystem
 * Sets up rate limiting, cache, and configuration
 */
void nominatim_init(void);

/**
 * Search Nominatim for locations matching query
 * 
 * @param query Search query string
 * @param country_codes Optional country filter (e.g., "us,ca"), NULL for none
 * @param limit Maximum number of results
 * @param results Output parameter for results
 * @return Number of results, or negative error code
 */
int nominatim_search(const char *query,
                     const char *country_codes,
                     int limit,
                     struct geocode_result_list *results);

/**
 * Get last Nominatim error message
 * 
 * @return Error message string, or NULL if no error
 */
const char *nominatim_get_error(void);

/**
 * Clear the Nominatim result cache
 * Useful for testing or when configuration changes
 */
void nominatim_clear_cache(void);

#endif  // HAVE_NOMINATIM

#endif  // XASTIR_NOMINATIM_H
