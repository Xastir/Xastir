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

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xastir.h"
#include "geocoder.h"

#ifdef HAVE_NOMINATIM
  #include "nominatim.h"
#endif

// Must be last include file
#include "leak_detection.h"

// Destination marking
long destination_coord_lat = 0;
long destination_coord_lon = 0;
int mark_destination = 0;

// Error message storage
static char last_error[256] = "";

/**
 * Initialize the geocoding subsystem
 */
void geocode_init(void)
{
#ifdef HAVE_NOMINATIM
    nominatim_init();
#endif
    last_error[0] = '\0';
}

/**
 * Search for locations matching the query string
 */
int geocode_search(geocode_service_t service,
                   const char *query,
                   const char *country_codes,
                   int limit,
                   struct geocode_result_list *results)
{
    int ret = -1;

    // Validate parameters
    if (!query || !results) {
        xastir_snprintf(last_error, sizeof(last_error),
                       "Invalid parameters");
        return -1;
    }

    if (limit <= 0) {
        limit = 10;  // Default
    }

    // Initialize result list
    results->count = 0;
    results->capacity = 0;
    results->results = NULL;

    // Dispatch to appropriate service
    switch (service) {
#ifdef HAVE_NOMINATIM
        case GEOCODE_SERVICE_NOMINATIM:
            ret = nominatim_search(query, country_codes, limit, results);
            if (ret < 0) {
                const char *err = nominatim_get_error();
                if (err) {
                    xastir_snprintf(last_error, sizeof(last_error), "%s", err);
                }
            }
            break;
#endif

        case GEOCODE_SERVICE_NONE:
            xastir_snprintf(last_error, sizeof(last_error),
                           "No geocoding service specified");
            ret = -1;
            break;

        default:
            xastir_snprintf(last_error, sizeof(last_error),
                           "Geocoding service not available (not compiled in)");
            ret = -1;
            break;
    }

    return ret;
}

/**
 * Free memory allocated for result list
 */
void geocode_free_results(struct geocode_result_list *results)
{
    if (results && results->results) {
        free(results->results);
        results->results = NULL;
        results->count = 0;
        results->capacity = 0;
    }
}

/**
 * Get last error message
 */
const char *geocode_get_error(void)
{
    if (last_error[0]) {
        return last_error;
    }
    return NULL;
}

/**
 * Check if a geocoding service is available
 */
int geocode_service_available(geocode_service_t service)
{
    switch (service) {
#ifdef HAVE_NOMINATIM
        case GEOCODE_SERVICE_NOMINATIM:
            return 1;
#endif
        default:
            return 0;
    }
}

/**
 * Get human-readable name of a geocoding service
 */
const char *geocode_service_name(geocode_service_t service)
{
    switch (service) {
        case GEOCODE_SERVICE_NOMINATIM:
            return "Nominatim (OpenStreetMap)";
        case GEOCODE_SERVICE_NONE:
            return "None";
        default:
            return "Unknown";
    }
}

/**
 * Build a human-readable subtitle from geocode_result address components
 * Uses intelligent fallback for missing fields
 */
void geocode_format_subtitle(const struct geocode_result *result,
                             char *subtitle,
                             size_t size)
{
    char components[256] = "";
    int added = 0;

    if (!result || !subtitle || size == 0) {
        return;
    }

    // Build hierarchy based on what's available
    // Typical patterns by region:
    //   US: settlement, county, state
    //   UK: settlement, county/state_district, state (England/Scotland/Wales)
    //   Germany: settlement, state
    //   France: settlement, county (département), state (région)
    //   Japan: settlement, state (prefecture)

    // Add primary settlement if present
    if (result->settlement[0]) {
        xastir_snprintf(components, sizeof(components), "%s", result->settlement);
        added = 1;
    }

    // Add county if present and not redundant with settlement
    if (result->county[0] &&
        (!result->settlement[0] || strcmp(result->county, result->settlement) != 0)) {
        if (added) {
            strncat(components, ", ", sizeof(components) - strlen(components) - 1);
        }
        strncat(components, result->county, sizeof(components) - strlen(components) - 1);
        added = 1;
    }

    // Add state_district if present (e.g., "Greater London")
    if (result->state_district[0]) {
        if (added) {
            strncat(components, ", ", sizeof(components) - strlen(components) - 1);
        }
        strncat(components, result->state_district,
                sizeof(components) - strlen(components) - 1);
        added = 1;
    }

    // Add state/province if different from previous components
    if (result->state[0] &&
        (!result->settlement[0] || strcmp(result->state, result->settlement) != 0) &&
        (!result->county[0] || strcmp(result->state, result->county) != 0)) {
        if (added) {
            strncat(components, ", ", sizeof(components) - strlen(components) - 1);
        }
        strncat(components, result->state, sizeof(components) - strlen(components) - 1);
        added = 1;
    }

    // Always add country if available (unless city-state like Singapore)
    if (result->country[0] &&
        (!result->settlement[0] || strcmp(result->country, result->settlement) != 0) &&
        (!result->state[0] || strcmp(result->country, result->state) != 0)) {
        if (added) {
            strncat(components, ", ", sizeof(components) - strlen(components) - 1);
        }
        strncat(components, result->country, sizeof(components) - strlen(components) - 1);
    }

    xastir_snprintf(subtitle, size, "%s", components);
}
