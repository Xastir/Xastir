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

#ifdef HAVE_NOMINATIM

#include "snprintf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>

#include "xastir.h"
#include "nominatim.h"
#include "geocoder.h"
#include "xa_config.h"
#include "mutex_utils.h"
#include "main.h"

#ifdef HAVE_LIBCURL
  #include <curl/curl.h>
  #include "fetch_remote.h"
#endif

#ifdef HAVE_CJSON
  #include <cjson/cJSON.h>
#endif

// Must be last include file
#include "leak_detection.h"

// Error message storage
static char nominatim_error[512] = "";

// Rate limiting (1 request per second per Nominatim usage policy)
static time_t last_request_time = 0;
static xastir_mutex rate_limit_lock;

// Cache entry structure
struct nominatim_cache_entry {
    char query_hash[33];        // MD5 hash of normalized query
    time_t timestamp;           // When cached
    int result_count;
    struct geocode_result *results;
    struct nominatim_cache_entry *next;
};

static struct nominatim_cache_entry *cache_head = NULL;
static xastir_mutex cache_lock;

// Buffer for HTTP response
struct http_response {
    char *data;
    size_t size;
};

/**
 * Callback for libcurl to write response data
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct http_response *response = (struct http_response *)userp;
    
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Out of memory");
        return 0;
    }
    
    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;
    
    return realsize;
}

/**
 * Enforce rate limiting - wait if needed to maintain 1 req/sec
 */
static void enforce_rate_limit(void)
{
    begin_critical_section(&rate_limit_lock, "nominatim.c:enforce_rate_limit");
    
    time_t now = time(NULL);
    time_t elapsed = now - last_request_time;
    
    if (last_request_time > 0 && elapsed < 1) {
        // Wait to maintain 1 request per second
        sleep(1 - elapsed);
    }
    
    last_request_time = time(NULL);
    
    end_critical_section(&rate_limit_lock, "nominatim.c:enforce_rate_limit");
}

/**
 * Simple hash function for cache keys
 * Uses basic string hash (not cryptographic - just for cache lookup)
 */
static void compute_query_hash(const char *query, char *hash, size_t hash_size)
{
    unsigned long h = 5381;
    int c;
    const char *str = query;
    
    // Normalize to lowercase for case-insensitive matching
    while ((c = *str++)) {
        h = ((h << 5) + h) + tolower(c);
    }
    
    xastir_snprintf(hash, hash_size, "%016lx", h);
}

/**
 * Look up query in cache
 * Returns 1 if found, 0 if not found or expired
 */
static int cache_lookup(const char *query, struct geocode_result_list *results)
{
    char query_hash[33];
    struct nominatim_cache_entry *entry;
    time_t now = time(NULL);
    int found = 0;
    
    compute_query_hash(query, query_hash, sizeof(query_hash));
    
    begin_critical_section(&cache_lock, "nominatim.c:cache_lookup");
    
    for (entry = cache_head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->query_hash, query_hash) == 0) {
            // Check if expired
            if (nominatim_cache_days > 0 &&
                (now - entry->timestamp) > (nominatim_cache_days * 86400)) {
                // Expired - will be cleaned up later
                break;
            }
            
            // Found valid cached entry
            if (entry->result_count > 0) {
                results->capacity = entry->result_count;
                results->count = entry->result_count;
                results->results = malloc(sizeof(struct geocode_result) * entry->result_count);
                
                if (results->results) {
                    memcpy(results->results, entry->results,
                           sizeof(struct geocode_result) * entry->result_count);
                    found = 1;
                }
            }
            break;
        }
    }
    
    end_critical_section(&cache_lock, "nominatim.c:cache_lookup");
    
    return found;
}

/**
 * Store results in cache
 */
static void cache_store(const char *query, const struct geocode_result_list *results)
{
    char query_hash[33];
    struct nominatim_cache_entry *entry;
    
    if (!nominatim_cache_enabled || results->count == 0) {
        return;
    }
    
    compute_query_hash(query, query_hash, sizeof(query_hash));
    
    begin_critical_section(&cache_lock, "nominatim.c:cache_store");
    
    // Check if already in cache (update if so)
    for (entry = cache_head; entry != NULL; entry = entry->next) {
        if (strcmp(entry->query_hash, query_hash) == 0) {
            // Update existing entry
            if (entry->results) {
                free(entry->results);
            }
            entry->results = malloc(sizeof(struct geocode_result) * results->count);
            if (entry->results) {
                memcpy(entry->results, results->results,
                       sizeof(struct geocode_result) * results->count);
                entry->result_count = results->count;
                entry->timestamp = time(NULL);
            }
            end_critical_section(&cache_lock, "nominatim.c:cache_store");
            return;
        }
    }
    
    // Create new entry
    entry = malloc(sizeof(struct nominatim_cache_entry));
    if (entry) {
        xastir_snprintf(entry->query_hash, sizeof(entry->query_hash), "%s", query_hash);
        entry->timestamp = time(NULL);
        entry->result_count = results->count;
        entry->results = malloc(sizeof(struct geocode_result) * results->count);
        
        if (entry->results) {
            memcpy(entry->results, results->results,
                   sizeof(struct geocode_result) * results->count);
            entry->next = cache_head;
            cache_head = entry;
        } else {
            free(entry);
        }
    }
    
    end_critical_section(&cache_lock, "nominatim.c:cache_store");
}

/**
 * Parse a single JSON result into geocode_result structure
 */
static int parse_nominatim_result(cJSON *json_result, struct geocode_result *result)
{
    cJSON *item;
    
    memset(result, 0, sizeof(struct geocode_result));
    result->service = GEOCODE_SERVICE_NOMINATIM;
    
    // Extract core fields
    item = cJSON_GetObjectItem(json_result, "lat");
    if (item && cJSON_IsString(item)) {
        result->lat = atof(item->valuestring);
    } else {
        return -1;  // Lat is required
    }
    
    item = cJSON_GetObjectItem(json_result, "lon");
    if (item && cJSON_IsString(item)) {
        result->lon = atof(item->valuestring);
    } else {
        return -1;  // Lon is required
    }
    
    // Display name
    item = cJSON_GetObjectItem(json_result, "display_name");
    if (item && cJSON_IsString(item)) {
        xastir_snprintf(result->display_name, sizeof(result->display_name),
                       "%s", item->valuestring);
    }
    
    // Bounding box
    item = cJSON_GetObjectItem(json_result, "boundingbox");
    if (item && cJSON_IsArray(item) && cJSON_GetArraySize(item) == 4) {
        result->bbox[0] = atof(cJSON_GetArrayItem(item, 0)->valuestring);  // min_lat
        result->bbox[1] = atof(cJSON_GetArrayItem(item, 1)->valuestring);  // max_lat
        result->bbox[2] = atof(cJSON_GetArrayItem(item, 2)->valuestring);  // min_lon
        result->bbox[3] = atof(cJSON_GetArrayItem(item, 3)->valuestring);  // max_lon
    }
    
    // Importance/relevance
    item = cJSON_GetObjectItem(json_result, "importance");
    if (item && cJSON_IsNumber(item)) {
        result->relevance = item->valuedouble;
    }
    
    // Place ID (service-specific)
    item = cJSON_GetObjectItem(json_result, "place_id");
    if (item && cJSON_IsNumber(item)) {
        xastir_snprintf(result->service_id, sizeof(result->service_id),
                       "%d", item->valueint);
    }
    
    // Classification
    item = cJSON_GetObjectItem(json_result, "class");
    if (item && cJSON_IsString(item)) {
        xastir_snprintf(result->place_class, sizeof(result->place_class),
                       "%s", item->valuestring);
    }
    
    item = cJSON_GetObjectItem(json_result, "type");
    if (item && cJSON_IsString(item)) {
        xastir_snprintf(result->place_type, sizeof(result->place_type),
                       "%s", item->valuestring);
    }
    
    // Address components (optional)
    cJSON *address = cJSON_GetObjectItem(json_result, "address");
    if (address && cJSON_IsObject(address)) {
        // House number
        item = cJSON_GetObjectItem(address, "house_number");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->house_number, sizeof(result->house_number),
                           "%s", item->valuestring);
        }
        
        // Road
        item = cJSON_GetObjectItem(address, "road");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->road, sizeof(result->road),
                           "%s", item->valuestring);
        }
        
        // Neighbourhood
        item = cJSON_GetObjectItem(address, "neighbourhood");
        if (!item || !cJSON_IsString(item)) {
            item = cJSON_GetObjectItem(address, "suburb");
        }
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->neighbourhood, sizeof(result->neighbourhood),
                           "%s", item->valuestring);
        }
        
        // Settlement (check city, town, village, hamlet in priority order)
        item = cJSON_GetObjectItem(address, "city");
        if (!item || !cJSON_IsString(item)) {
            item = cJSON_GetObjectItem(address, "town");
        }
        if (!item || !cJSON_IsString(item)) {
            item = cJSON_GetObjectItem(address, "village");
        }
        if (!item || !cJSON_IsString(item)) {
            item = cJSON_GetObjectItem(address, "hamlet");
        }
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->settlement, sizeof(result->settlement),
                           "%s", item->valuestring);
        }
        
        // County
        item = cJSON_GetObjectItem(address, "county");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->county, sizeof(result->county),
                           "%s", item->valuestring);
        }
        
        // State
        item = cJSON_GetObjectItem(address, "state");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->state, sizeof(result->state),
                           "%s", item->valuestring);
        }
        
        // State district
        item = cJSON_GetObjectItem(address, "state_district");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->state_district, sizeof(result->state_district),
                           "%s", item->valuestring);
        }
        
        // Postcode
        item = cJSON_GetObjectItem(address, "postcode");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->postcode, sizeof(result->postcode),
                           "%s", item->valuestring);
        }
        
        // Country
        item = cJSON_GetObjectItem(address, "country");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->country, sizeof(result->country),
                           "%s", item->valuestring);
        }
        
        // Country code
        item = cJSON_GetObjectItem(address, "country_code");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->country_code, sizeof(result->country_code),
                           "%s", item->valuestring);
        }
        
        // ISO3166-2
        item = cJSON_GetObjectItem(address, "ISO3166-2-lvl4");
        if (item && cJSON_IsString(item)) {
            xastir_snprintf(result->iso3166_2, sizeof(result->iso3166_2),
                           "%s", item->valuestring);
        }
    }
    
    return 0;
}

/**
 * Initialize Nominatim subsystem
 */
void nominatim_init(void)
{
    init_critical_section(&rate_limit_lock);
    init_critical_section(&cache_lock);
    nominatim_error[0] = '\0';
    last_request_time = 0;
    cache_head = NULL;
}

/**
 * Search Nominatim for locations matching query
 */
int nominatim_search(const char *query,
                     const char *country_codes,
                     int limit,
                     struct geocode_result_list *results)
{
#ifndef HAVE_LIBCURL
    xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                   "libcurl not available - cannot perform network request");
    return -1;
#else
    CURL *curl = NULL;
    CURLcode res;
    struct http_response response = {0};
    char url[2048];
    char *escaped_query = NULL;
    cJSON *json = NULL;
    int ret = -1;
    int i;
    
    // Check cache first
    if (nominatim_cache_enabled && cache_lookup(query, results)) {
        return results->count;
    }
    
    // Enforce rate limiting
    enforce_rate_limit();
    
    // Initialize curl
    curl = xastir_curl_init(nominatim_error);
    if (!curl) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Failed to initialize HTTP client");
        return -1;
    }
    
    // URL-encode the query
    escaped_query = curl_easy_escape(curl, query, 0);
    if (!escaped_query) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Failed to encode query");
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Build URL
    xastir_snprintf(url, sizeof(url),
                   "%s/search?q=%s&format=jsonv2&addressdetails=1&limit=%d",
                   nominatim_server_url, escaped_query, limit);
    
    if (country_codes && country_codes[0]) {
        char *escaped_countries = curl_easy_escape(curl, country_codes, 0);
        if (escaped_countries) {
            strncat(url, "&countrycodes=", sizeof(url) - strlen(url) - 1);
            strncat(url, escaped_countries, sizeof(url) - strlen(url) - 1);
            curl_free(escaped_countries);
        }
    }
    
    // Add email if configured
    if (nominatim_user_email[0]) {
        char *escaped_email = curl_easy_escape(curl, nominatim_user_email, 0);
        if (escaped_email) {
            strncat(url, "&email=", sizeof(url) - strlen(url) - 1);
            strncat(url, escaped_email, sizeof(url) - strlen(url) - 1);
            curl_free(escaped_email);
        }
    }
    
    curl_free(escaped_query);
    
    // Set up HTTP request
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Xastir/2.2.0");
    
    // Perform request
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Network error: %s", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return -1;
    }
    
    if (!response.data || response.size == 0) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Empty response from server");
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse JSON response
    json = cJSON_Parse(response.data);
    free(response.data);
    
    if (!json) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Invalid JSON response");
        return -1;
    }
    
    if (!cJSON_IsArray(json)) {
        xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                       "Unexpected JSON format");
        cJSON_Delete(json);
        return -1;
    }
    
    // Process results
    int array_size = cJSON_GetArraySize(json);
    if (array_size == 0) {
        // No results found - not an error
        results->count = 0;
        results->capacity = 0;
        results->results = NULL;
        ret = 0;
    } else {
        results->capacity = array_size;
        results->results = malloc(sizeof(struct geocode_result) * array_size);
        
        if (results->results) {
            results->count = 0;
            for (i = 0; i < array_size; i++) {
                cJSON *item = cJSON_GetArrayItem(json, i);
                if (parse_nominatim_result(item, &results->results[results->count]) == 0) {
                    results->count++;
                }
            }
            ret = results->count;
            
            // Store in cache
            cache_store(query, results);
        } else {
            xastir_snprintf(nominatim_error, sizeof(nominatim_error),
                           "Out of memory");
            ret = -1;
        }
    }
    
    cJSON_Delete(json);
    return ret;
#endif  // HAVE_LIBCURL
}

/**
 * Get last Nominatim error message
 */
const char *nominatim_get_error(void)
{
    if (nominatim_error[0]) {
        return nominatim_error;
    }
    return NULL;
}

/**
 * Clear the Nominatim result cache
 */
void nominatim_clear_cache(void)
{
    struct nominatim_cache_entry *entry, *next;
    
    begin_critical_section(&cache_lock, "nominatim.c:nominatim_clear_cache");
    
    entry = cache_head;
    while (entry) {
        next = entry->next;
        if (entry->results) {
            free(entry->results);
        }
        free(entry);
        entry = next;
    }
    cache_head = NULL;
    
    end_critical_section(&cache_lock, "nominatim.c:nominatim_clear_cache");
}

#endif  // HAVE_NOMINATIM
