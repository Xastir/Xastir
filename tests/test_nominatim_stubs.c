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
 * Stub implementations for symbols referenced by nominatim.o
 * but not needed by the unit tests.
 *
 * These stubs allow us to link with the nominatim code for testing
 * without pulling in the entire Xastir codebase.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tests/test_framework.h"

// Debug level (used throughout Xastir)
int debug_level = 0;

// Package version (for User-Agent string)
const char PACKAGE_VERSION[] = "2.2.0-test";

// Stub implementations of functions called by nominatim.c

// Mutex functions - for testing, we can use no-op implementations
void init_critical_section(void *mutex) {
    // No-op for testing
    (void)mutex;
}

void begin_critical_section(void *mutex, const char *location) {
    // No-op for testing
    (void)mutex;
    (void)location;
}

void end_critical_section(void *mutex, const char *location) {
    // No-op for testing
    (void)mutex;
    (void)location;
}

// snprintf wrapper - just use system snprintf
int xastir_snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    int ret;

    va_start(args, format);
    ret = vsnprintf(str, size, format, args);
    va_end(args);

    return ret;
}

#include <curl/curl.h>

// curl initialization wrapper - stub implementation for tests
CURL* xastir_curl_init(char *error_buffer) {
    (void)error_buffer;  // Unused in stub
    CURL *curl = curl_easy_init();
    return curl;
}

// Leak detection - no-op for tests
void leak_detection_init(void) {}
void leak_detection_cleanup(void) {}
