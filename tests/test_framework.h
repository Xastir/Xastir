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

 /* This file contains test framework macros for unit testing  */


#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n  Assertion failed: %s\n", msg, #cond); \
        return 1; \
    } \
} while(0)

#define TEST_ASSERT_STR_EQ(expected, actual, msg) do { \
    if (strcmp(expected, actual) != 0) { \
        fprintf(stderr, "FAIL: %s\n  Expected: '%s'\n  Got: '%s'\n", \
                msg, expected, actual); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    printf("PASS: %s\n", msg); \
    return 0; \
} while(0)


/* Stub implementations - these should never be called by our tests */
#define STUB_IMPL(name) \
    void name(void) { \
        fprintf(stderr, "ERROR: Stub function %s called - test is using untested code path\n", #name); \
        abort(); \
    }

