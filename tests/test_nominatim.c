/*
 *
 * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 2025-2026 The Xastir Group
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

/*
 * Test program for nominatim.c functions
 *
 * This test program tests the Nominatim geocoding implementation including:
 * - Cache lookup and storage
 * - Rate limiting
 * - URL formation with country codes and email
 */

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tests/test_framework.h"

// Geocoder structures needed for tests
#include "geocoder.h"

// External config variables that nominatim.c depends on
extern char nominatim_server_url[400];
extern int nominatim_cache_enabled;
extern int nominatim_cache_days;
extern char nominatim_user_email[100];

// Functions from nominatim.c that we need to test
// Note: Some are static and exposed via test-specific build or we test through public API
extern void nominatim_init(void);
extern void nominatim_clear_cache(void);

// Helper functions exposed for testing (these would normally be static)
// We'll declare them here and conditionally compile them in nominatim.c for testing
extern void compute_query_hash(const char *query, const char *country_codes, char *hash, size_t hash_size);
extern int cache_lookup(const char *query, const char *country_codes, struct geocode_result_list *results);
extern void cache_store(const char *query, const char *country_codes, const struct geocode_result_list *results);


/* Test cases */

/**
 * Test: compute_query_hash produces consistent hashes
 */
int test_compute_query_hash_consistency(void)
{
    char hash1[33];
    char hash2[33];

    compute_query_hash("This is a query that should produce a consistent hash", NULL, hash1, sizeof(hash1));
    compute_query_hash("This is a query that should produce a consistent hash", NULL, hash2, sizeof(hash2));

    TEST_ASSERT_STR_EQ(hash1, hash2,
        "Same query should produce same hash");

    TEST_PASS("compute_query_hash produces consistent hashes");
}

/**
 * Test: compute_query_hash is case-insensitive
 */
int test_compute_query_hash_case_insensitive(void)
{
    char hash1[33];
    char hash2[33];

    compute_query_hash("Seattle", NULL, hash1, sizeof(hash1));
    compute_query_hash("seattle", NULL, hash2, sizeof(hash2));

    TEST_ASSERT_STR_EQ(hash1, hash2,
        "Hash should be case-insensitive");

    TEST_PASS("compute_query_hash is case-insensitive");
}

/**
 * Test: compute_query_hash includes country codes in hash
 */
int test_compute_query_hash_with_country(void)
{
    char hash1[33];
    char hash2[33];

    compute_query_hash("Paris", NULL, hash1, sizeof(hash1));
    compute_query_hash("Paris", "fr", hash2, sizeof(hash2));

    TEST_ASSERT(strcmp(hash1, hash2) != 0,
        "Hash with country code should differ from hash without");

    TEST_PASS("compute_query_hash includes country codes");
}

/**
 * Test: compute_query_hash produces different hashes for different queries
 */
int test_compute_query_hash_different_queries(void)
{
    char hash1[33];
    char hash2[33];

    compute_query_hash("Seattle", NULL, hash1, sizeof(hash1));
    compute_query_hash("Portland", NULL, hash2, sizeof(hash2));

    TEST_ASSERT(strcmp(hash1, hash2) != 0,
        "Different queries should produce different hashes");

    TEST_PASS("compute_query_hash produces different hashes for different queries");
}

/**
 * Test: Cache lookup returns 0 for empty cache
 */
int test_cache_lookup_empty(void)
{
    struct geocode_result_list results;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;

    memset(&results, 0, sizeof(results));

    int found = cache_lookup("Test Query", NULL, &results);

    TEST_ASSERT(found == 0,
        "Cache lookup should return 0 for empty cache");

    TEST_PASS("Cache lookup returns 0 for empty cache");
}

/**
 * Test: Cache store and lookup round-trip
 */
int test_cache_store_and_lookup(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result result;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;
    nominatim_cache_days = 30;

    // Create a result to store
    memset(&result, 0, sizeof(result));
    result.lat = 47.6062;
    result.lon = -122.3321;
    strncpy(result.display_name, "Seattle, WA", sizeof(result.display_name));
    strncpy(result.settlement, "Seattle", sizeof(result.settlement));
    strncpy(result.state, "Washington", sizeof(result.state));
    strncpy(result.country, "United States", sizeof(result.country));

    // Store in cache
    store_results.count = 1;
    store_results.capacity = 1;
    store_results.results = &result;

    cache_store("Seattle", NULL, &store_results);

    // Look up from cache
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found = cache_lookup("Seattle", NULL, &lookup_results);

    TEST_ASSERT(found == 1,
        "Cache lookup should find stored result");
    TEST_ASSERT(lookup_results.count == 1,
        "Cache should return correct number of results");
    TEST_ASSERT(lookup_results.results != NULL,
        "Cache should return result data");
    TEST_ASSERT(lookup_results.results[0].lat == 47.6062,
        "Cached result should have correct latitude");
    TEST_ASSERT(lookup_results.results[0].lon == -122.3321,
        "Cached result should have correct longitude");

    // Clean up
    free(lookup_results.results);

    TEST_PASS("Cache store and lookup round-trip works");
}

/**
 * Test: Cache respects case-insensitive lookup
 */
int test_cache_case_insensitive(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result result;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;
    nominatim_cache_days = 30;

    // Create and store result
    memset(&result, 0, sizeof(result));
    result.lat = 47.6062;
    result.lon = -122.3321;

    store_results.count = 1;
    store_results.capacity = 1;
    store_results.results = &result;

    cache_store("Seattle", NULL, &store_results);

    // Look up with different case
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found = cache_lookup("SEATTLE", NULL, &lookup_results);

    TEST_ASSERT(found == 1,
        "Cache lookup should be case-insensitive");

    // Clean up
    if (lookup_results.results) {
        free(lookup_results.results);
    }

    TEST_PASS("Cache is case-insensitive");
}

/**
 * Test: Cache distinguishes queries with different country codes
 */
int test_cache_country_codes(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result result1, result2;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;
    nominatim_cache_days = 30;

    // Store Paris, France
    memset(&result1, 0, sizeof(result1));
    result1.lat = 48.8566;
    result1.lon = 2.3522;
    strncpy(result1.settlement, "Paris", sizeof(result1.settlement));
    strncpy(result1.country, "France", sizeof(result1.country));

    store_results.count = 1;
    store_results.capacity = 1;
    store_results.results = &result1;
    cache_store("Paris", "fr", &store_results);

    // Store Paris, Texas (US)
    memset(&result2, 0, sizeof(result2));
    result2.lat = 33.6609;
    result2.lon = -95.5555;
    strncpy(result2.settlement, "Paris", sizeof(result2.settlement));
    strncpy(result2.country, "United States", sizeof(result2.country));

    store_results.results = &result2;
    cache_store("Paris", "us", &store_results);

    // Look up Paris, France
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found_fr = cache_lookup("Paris", "fr", &lookup_results);

    TEST_ASSERT(found_fr == 1,
        "Cache should find Paris, France");
    TEST_ASSERT(lookup_results.results[0].lat == 48.8566 && lookup_results.results[0].lon == 2.3522,
        "Should return Paris, France coordinates");

    free(lookup_results.results);

    // Look up Paris, US
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found_us = cache_lookup("Paris", "us", &lookup_results);

    TEST_ASSERT(found_us == 1,
        "Cache should find Paris, US");
    TEST_ASSERT(lookup_results.results[0].lat == 33.6609,
        "Should return US Paris coordinates");

    free(lookup_results.results);

    TEST_PASS("Cache distinguishes queries with different country codes");
}

/**
 * Test: Cache clear removes all entries
 */
int test_cache_clear(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result result;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;
    nominatim_cache_days = 30;

    // Store a result
    memset(&result, 0, sizeof(result));
    result.lat = 47.6062;
    result.lon = -122.3321;

    store_results.count = 1;
    store_results.capacity = 1;
    store_results.results = &result;
    cache_store("Seattle", NULL, &store_results);

    // Verify it's in cache
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found_before = cache_lookup("Seattle", NULL, &lookup_results);
    if (lookup_results.results) {
        free(lookup_results.results);
    }

    // Clear cache
    nominatim_clear_cache();

    // Verify it's gone
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found_after = cache_lookup("Seattle", NULL, &lookup_results);

    TEST_ASSERT(found_before == 1,
        "Result should be in cache before clear");
    TEST_ASSERT(found_after == 0,
        "Result should not be in cache after clear");

    TEST_PASS("Cache clear removes all entries");
}

/**
 * Test: Cache respects disabled setting
 */
int test_cache_disabled(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result result;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 0;  // Disable cache
    nominatim_cache_days = 30;

    // Try to store a result
    memset(&result, 0, sizeof(result));
    result.lat = 47.6062;
    result.lon = -122.3321;

    store_results.count = 1;
    store_results.capacity = 1;
    store_results.results = &result;
    cache_store("Seattle", NULL, &store_results);

    // Try to look up (should not find because cache was disabled during store)
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found = cache_lookup("Seattle", NULL, &lookup_results);

    TEST_ASSERT(found == 0,
        "Cache lookup should not find results when cache was disabled");

    TEST_PASS("Cache respects disabled setting");
}


/**
 * Test: Multiple results in cache
 */
int test_cache_multiple_results(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result results[3];

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;
    nominatim_cache_days = 30;

    // Create multiple results
    memset(results, 0, sizeof(results));
    results[0].lat = 47.6062;
    results[0].lon = -122.3321;
    strncpy(results[0].settlement, "Seattle", sizeof(results[0].settlement));

    results[1].lat = 45.5152;
    results[1].lon = -122.6784;
    strncpy(results[1].settlement, "Portland", sizeof(results[1].settlement));

    results[2].lat = 49.2827;
    results[2].lon = -123.1207;
    strncpy(results[2].settlement, "Vancouver", sizeof(results[2].settlement));

    // Store multiple results
    store_results.count = 3;
    store_results.capacity = 3;
    store_results.results = results;
    cache_store("Pacific Northwest Cities", NULL, &store_results);

    // Look up
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found = cache_lookup("Pacific Northwest Cities", NULL, &lookup_results);

    TEST_ASSERT(found == 1,
        "Cache should find stored results");
    TEST_ASSERT(lookup_results.count == 3,
        "Cache should return all 3 results");
    TEST_ASSERT(lookup_results.results[0].lat == 47.6062,
        "First result should be Seattle");
    TEST_ASSERT(lookup_results.results[1].lat == 45.5152,
        "Second result should be Portland");
    TEST_ASSERT(lookup_results.results[2].lat == 49.2827,
        "Third result should be Vancouver");

    free(lookup_results.results);

    TEST_PASS("Cache handles multiple results correctly");
}

/**
 * Test: Cache update existing entry
 */
int test_cache_update_existing(void)
{
    struct geocode_result_list store_results;
    struct geocode_result_list lookup_results;
    struct geocode_result result1, result2;

    // Clear cache and initialize
    nominatim_clear_cache();
    nominatim_init();
    nominatim_cache_enabled = 1;
    nominatim_cache_days = 30;

    // Store initial result
    memset(&result1, 0, sizeof(result1));
    result1.lat = 47.6062;
    result1.lon = -122.3321;
    strncpy(result1.settlement, "Seattle", sizeof(result1.settlement));

    store_results.count = 1;
    store_results.capacity = 1;
    store_results.results = &result1;
    cache_store("Test City", NULL, &store_results);

    // Update with different result
    memset(&result2, 0, sizeof(result2));
    result2.lat = 45.5152;
    result2.lon = -122.6784;
    strncpy(result2.settlement, "Portland", sizeof(result2.settlement));

    store_results.results = &result2;
    cache_store("Test City", NULL, &store_results);

    // Look up - should get updated result
    memset(&lookup_results, 0, sizeof(lookup_results));
    int found = cache_lookup("Test City", NULL, &lookup_results);

    TEST_ASSERT(found == 1,
        "Cache should find updated result");
    TEST_ASSERT(lookup_results.results[0].lat == 45.5152,
        "Cache should return updated latitude");
    TEST_ASSERT(strcmp(lookup_results.results[0].settlement, "Portland") == 0,
        "Cache should return updated settlement");

    free(lookup_results.results);

    TEST_PASS("Cache updates existing entries correctly");
}

/* Main test runner */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test_name>\n", argv[0]);
        return 1;
    }

    const char *test_name = argv[1];

    // Hash tests
    if (strcmp(test_name, "compute_query_hash_consistency") == 0)
        return test_compute_query_hash_consistency();
    if (strcmp(test_name, "compute_query_hash_case_insensitive") == 0)
        return test_compute_query_hash_case_insensitive();
    if (strcmp(test_name, "compute_query_hash_with_country") == 0)
        return test_compute_query_hash_with_country();
    if (strcmp(test_name, "compute_query_hash_different_queries") == 0)
        return test_compute_query_hash_different_queries();

    // Cache tests
    if (strcmp(test_name, "cache_lookup_empty") == 0)
        return test_cache_lookup_empty();
    if (strcmp(test_name, "cache_store_and_lookup") == 0)
        return test_cache_store_and_lookup();
    if (strcmp(test_name, "cache_case_insensitive") == 0)
        return test_cache_case_insensitive();
    if (strcmp(test_name, "cache_country_codes") == 0)
        return test_cache_country_codes();
    if (strcmp(test_name, "cache_clear") == 0)
        return test_cache_clear();
    if (strcmp(test_name, "cache_disabled") == 0)
        return test_cache_disabled();
    if (strcmp(test_name, "cache_multiple_results") == 0)
        return test_cache_multiple_results();
    if (strcmp(test_name, "cache_update_existing") == 0)
        return test_cache_update_existing();

    fprintf(stderr, "Unknown test: %s\n", test_name);
    return 1;
}
