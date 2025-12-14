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

#ifndef XASTIR_GEOCODER_H
#define XASTIR_GEOCODER_H

/*
 * Abstract Geocoding API
 *
 * This module provides a service-agnostic interface for geocoding operations.
 * Multiple geocoding services can be added by implementing the service-specific
 * interface and registering with the dispatcher.
 *
 * Initial implementation supports Nominatim (OpenStreetMap).
 */

// Geocoding service enumeration
typedef enum {
    GEOCODE_SERVICE_NONE = 0,
    GEOCODE_SERVICE_NOMINATIM = 1,
    // Future services can be added here
} geocode_service_t;

// Generic geocoding result (service-agnostic)
// Designed to support international addresses with flexible components
struct geocode_result {
    // Core location data
    double lat;
    double lon;
    double bbox[4];             // Bounding box: [min_lat, max_lat, min_lon, max_lon]

    // Primary identification
    char display_name[512];     // Full formatted address
    char service_id[64];        // Service-specific ID (e.g., Nominatim place_id)
    double relevance;           // Service-specific relevance/importance score (0.0-1.0)

    // Hierarchical address components (international-aware)
    // Not all fields will be populated for every result
    char house_number[32];      // Building/house number
    char road[128];             // Street/road name
    char neighbourhood[64];     // Neighbourhood/quarter/suburb

    // Primary settlement (mutually exclusive - only ONE is set by Nominatim)
    // Could be city, town, village, or hamlet - check place_type if needed
    char settlement[64];        // Name of primary settlement

    char county[64];            // County/district
    char state[64];             // State/province/region
    char state_district[64];    // State district (e.g., "Greater London")
    char postcode[32];          // Postal/ZIP code
    char country[64];           // Country name
    char country_code[3];       // ISO 3166-1 alpha-2 country code (lowercase)

    // Administrative hierarchy (for international addresses)
    char iso3166_2[16];         // ISO3166-2 code (e.g., "GB-ENG", "US-CA")

    // Place classification
    char place_type[32];        // Type of place (house, street, city, etc.)
    char place_class[32];       // OSM class (place, building, amenity, etc.)

    // Service metadata
    geocode_service_t service;  // Which service provided this result
};

// Result list container
struct geocode_result_list {
    int count;
    int capacity;
    struct geocode_result *results;
};

/*
 * Public API Functions
 */

/**
 * Initialize the geocoding subsystem
 * Must be called before any other geocoding functions
 */
void geocode_init(void);

/**
 * Search for locations matching the query string
 *
 * @param service Which geocoding service to use
 * @param query Search query (freeform text)
 * @param country_codes Optional country filter (e.g., "us,ca"), NULL for no filter
 * @param limit Maximum number of results to return
 * @param results Output parameter - will be populated with results
 * @return Number of results found, or negative error code
 */
int geocode_search(geocode_service_t service,
                   const char *query,
                   const char *country_codes,
                   int limit,
                   struct geocode_result_list *results);

/**
 * Free memory allocated for result list
 *
 * @param results Result list to free
 */
void geocode_free_results(struct geocode_result_list *results);

/**
 * Get last error message
 *
 * @return Error message string, or NULL if no error
 */
const char *geocode_get_error(void);

/**
 * Check if a geocoding service is available
 *
 * @param service Service to check
 * @return 1 if available, 0 if not compiled in or disabled
 */
int geocode_service_available(geocode_service_t service);

/**
 * Get human-readable name of a geocoding service
 *
 * @param service Service identifier
 * @return Service name string
 */
const char *geocode_service_name(geocode_service_t service);

/**
 * Build a human-readable subtitle from geocode_result address components
 * Uses intelligent fallback for missing fields
 *
 * @param result Geocoding result
 * @param subtitle Output buffer
 * @param size Size of output buffer
 */
void geocode_format_subtitle(const struct geocode_result *result,
                             char *subtitle,
                             size_t size);

#endif  // XASTIR_GEOCODER_H
