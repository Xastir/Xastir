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
 * Test program for db.c functions
 * 
 * This test program tests standalone utility functions from db.c that
 * don't require complex Xastir infrastructure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tests/test_framework.h"

/* Forward declarations of functions under test */
void pad_callsign(char *callsignout, char *callsignin);
int extract_speed_course(char *info, char *speed, char *course);

/* Local implementation of substr helper function */
static void substr(char *dest, char *src, int size)
{
  memcpy(dest, src, size+1);
  dest[size] = '\0';  // Terminate string
}

/* Test cases for pad_callsign() */

int test_pad_callsign_basic(void)
{
    char output[10];
    
    pad_callsign(output, "N7ABC");
    TEST_ASSERT_STR_EQ("N7ABC    ", output, 
        "Basic callsign should be padded to 9 characters");
    
    TEST_PASS("pad_callsign with basic callsign");
}

int test_pad_callsign_with_ssid(void)
{
    char output[10];
    
    pad_callsign(output, "N7ABC-15");
    TEST_ASSERT_STR_EQ("N7ABC-15 ", output, 
        "Callsign with SSID should preserve hyphen");
    
    TEST_PASS("pad_callsign with SSID");
}

int test_pad_callsign_full_length(void)
{
    char output[10];
    
    pad_callsign(output, "ABCDEF-12");
    TEST_ASSERT_STR_EQ("ABCDEF-12", output, 
        "9-character callsign should not be padded");
    
    TEST_PASS("pad_callsign with full length callsign");
}

int test_pad_callsign_empty(void)
{
    char output[10];
    
    pad_callsign(output, "");
    TEST_ASSERT_STR_EQ("         ", output, 
        "Empty callsign should produce all spaces");
    
    TEST_PASS("pad_callsign with empty string");
}

int test_pad_callsign_short(void)
{
    char output[10];
    
    pad_callsign(output, "AB");
    TEST_ASSERT_STR_EQ("AB       ", output, 
        "Short callsign should be right-padded with spaces");
    
    TEST_PASS("pad_callsign with short callsign");
}

int test_pad_callsign_invalid_chars(void)
{
    char output[10];
    
    pad_callsign(output, "N7*ABC");
    TEST_ASSERT_STR_EQ("N7 ABC   ", output, 
        "Invalid character (*) should be replaced with space");
    
    TEST_PASS("pad_callsign with invalid characters");
}

int test_pad_callsign_multiple_invalid(void)
{
    char output[10];
    
    pad_callsign(output, "AB@#CD!EF");
    TEST_ASSERT_STR_EQ("AB  CD EF", output, 
        "Multiple invalid characters should be replaced with spaces");
    
    TEST_PASS("pad_callsign with multiple invalid characters");
}

int test_pad_callsign_lowercase(void)
{
    char output[10];
    
    pad_callsign(output, "n7abc");
    TEST_ASSERT_STR_EQ("n7abc    ", output, 
        "Lowercase letters are alphanumeric and should be preserved");
    
    TEST_PASS("pad_callsign with lowercase letters");
}

int test_pad_callsign_mixed_case(void)
{
    char output[10];
    
    pad_callsign(output, "N7aBc-1");
    TEST_ASSERT_STR_EQ("N7aBc-1  ", output, 
        "Mixed case should be preserved");
    
    TEST_PASS("pad_callsign with mixed case");
}

int test_pad_callsign_numbers_only(void)
{
    char output[10];
    
    pad_callsign(output, "12345");
    TEST_ASSERT_STR_EQ("12345    ", output, 
        "Numbers-only callsign should work");
    
    TEST_PASS("pad_callsign with numbers only");
}

int test_pad_callsign_hyphen_only(void)
{
    char output[10];
    
    pad_callsign(output, "-");
    TEST_ASSERT_STR_EQ("-        ", output, 
        "Single hyphen should be preserved");
    
    TEST_PASS("pad_callsign with single hyphen");
}

int test_pad_callsign_spaces_in_input(void)
{
    char output[10];
    
    pad_callsign(output, "AB CD");
    TEST_ASSERT_STR_EQ("AB CD    ", output, 
        "Spaces in input should be converted to spaces in output");
    
    TEST_PASS("pad_callsign with spaces in input");
}

int test_pad_callsign_trailing_hyphen(void)
{
    char output[10];
    
    pad_callsign(output, "N7ABC-");
    TEST_ASSERT_STR_EQ("N7ABC-   ", output, 
        "Trailing hyphen should be preserved");
    
    TEST_PASS("pad_callsign with trailing hyphen");
}

int test_pad_callsign_leading_hyphen(void)
{
    char output[10];
    
    pad_callsign(output, "-N7ABC");
    TEST_ASSERT_STR_EQ("-N7ABC   ", output, 
        "Leading hyphen should be preserved");
    
    TEST_PASS("pad_callsign with leading hyphen");
}

int test_pad_callsign_multiple_hyphens(void)
{
    char output[10];
    
    pad_callsign(output, "A-B-C");
    TEST_ASSERT_STR_EQ("A-B-C    ", output, 
        "Multiple hyphens should be preserved");
    
    TEST_PASS("pad_callsign with multiple hyphens");
}

/* Test cases for extract_speed_course() */

int test_extract_speed_course_valid_basic(void)
{
    char info[100] = "090/036more data";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("036", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("090", course, "Course should be extracted");
    TEST_ASSERT_STR_EQ("more data", info, "Speed/course should be removed from info");
    
    TEST_PASS("extract_speed_course with valid basic format");
}

int test_extract_speed_course_valid_no_extra_data(void)
{
    char info[100] = "180/050";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("050", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("180", course, "Course should be extracted");
    TEST_ASSERT_STR_EQ("", info, "Info should be empty after extraction");
    
    TEST_PASS("extract_speed_course with exact length");
}

int test_extract_speed_course_course_zero(void)
{
    char info[100] = "000/025data";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 (format is valid)");
    TEST_ASSERT_STR_EQ("025", speed, "Speed should still be extracted");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty (000 means undefined)");
    TEST_ASSERT_STR_EQ("data", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with course=000 (undefined)");
}

int test_extract_speed_course_with_spaces(void)
{
    char info[100] = "1 0/ 25rest";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 (spaces allowed in format check)");
    // Note: The function does a second validation pass that checks if first 2 chars
    // are digits, so both speed and course will be cleared
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty (space is not digit in validation)");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty (space is not digit in validation)");
    TEST_ASSERT_STR_EQ("rest", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with spaces in numbers");
}

int test_extract_speed_course_with_periods(void)
{
    char info[100] = "123/456more";  // Use valid format first
    char speed[10];
    char course[10];
    int result;
    
    // First test with valid numeric format
    result = extract_speed_course(info, speed, course);
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("456", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("123", course, "Course should be extracted");
    
    // Now test with periods (which pass format check but fail atoi)
    strcpy(info, "1.2/3.4more");
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 (periods pass format check)");
    // atoi("1.2") = 1, which is >= 1, so we enter the else block for validation
    // The validation checks if first 2 chars are digits - they're not (position 1 is '.')
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty (period in position 1)");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty (period in position 1)");
    TEST_ASSERT_STR_EQ("more", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with periods");
}

int test_extract_speed_course_invalid_separator(void)
{
    char info[100] = "090-036data";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 0, "Should return 0 for invalid separator");
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty");
    TEST_ASSERT_STR_EQ("090-036data", info, "Info should be unchanged");
    
    TEST_PASS("extract_speed_course with invalid separator");
}

int test_extract_speed_course_too_short(void)
{
    char info[100] = "090/03";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 0, "Should return 0 for too short string");
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty");
    TEST_ASSERT_STR_EQ("090/03", info, "Info should be unchanged");
    
    TEST_PASS("extract_speed_course with too short input");
}

int test_extract_speed_course_invalid_chars(void)
{
    char info[100] = "ABC/DEFdata";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 0, "Should return 0 for invalid characters");
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty");
    TEST_ASSERT_STR_EQ("ABC/DEFdata", info, "Info should be unchanged");
    
    TEST_PASS("extract_speed_course with invalid characters");
}

int test_extract_speed_course_empty_string(void)
{
    char info[100] = "";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 0, "Should return 0 for empty string");
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty");
    TEST_ASSERT_STR_EQ("", info, "Info should be unchanged");
    
    TEST_PASS("extract_speed_course with empty string");
}

int test_extract_speed_course_max_values(void)
{
    char info[100] = "360/999end";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("999", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("360", course, "Course should be extracted");
    TEST_ASSERT_STR_EQ("end", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with maximum values");
}

int test_extract_speed_course_leading_zeros(void)
{
    char info[100] = "001/002text";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("002", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("001", course, "Course should be extracted");
    TEST_ASSERT_STR_EQ("text", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with leading zeros");
}

int test_extract_speed_course_mixed_valid_chars(void)
{
    char info[100] = "1.0/0 5data";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 (periods and spaces allowed in format)");
    // Second validation pass checks first 2 chars for digits, so both will be cleared
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty (space in first 2 chars)");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty (period in first 2 chars)");
    TEST_ASSERT_STR_EQ("data", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with mixed valid characters");
}

int test_extract_speed_course_partial_match(void)
{
    char info[100] = "123/45Xrest";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 0, "Should return 0 for partial invalid match");
    TEST_ASSERT_STR_EQ("", speed, "Speed should be empty");
    TEST_ASSERT_STR_EQ("", course, "Course should be empty");
    TEST_ASSERT_STR_EQ("123/45Xrest", info, "Info should be unchanged");
    
    TEST_PASS("extract_speed_course with partial match");
}

int test_extract_speed_course_long_string(void)
{
    char info[100] = "270/045this is a long additional string";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("045", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("270", course, "Course should be extracted");
    TEST_ASSERT_STR_EQ("this is a long additional string", info, 
        "Rest of string should remain");
    
    TEST_PASS("extract_speed_course with long additional data");
}

int test_extract_speed_course_course_001(void)
{
    char info[100] = "001/100end";
    char speed[10];
    char course[10];
    int result;
    
    result = extract_speed_course(info, speed, course);
    
    TEST_ASSERT(result == 1, "Should return 1 for valid format");
    TEST_ASSERT_STR_EQ("100", speed, "Speed should be extracted");
    TEST_ASSERT_STR_EQ("001", course, "Course 001 is valid (>0)");
    TEST_ASSERT_STR_EQ("end", info, "Info should be updated");
    
    TEST_PASS("extract_speed_course with course=001 (minimum valid)");
}

/* Test runner */
typedef struct {
    const char *name;
    int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
    test_case_t tests[] = {
        /* pad_callsign tests */
        {"pad_callsign_basic", test_pad_callsign_basic},
        {"pad_callsign_with_ssid", test_pad_callsign_with_ssid},
        {"pad_callsign_full_length", test_pad_callsign_full_length},
        {"pad_callsign_empty", test_pad_callsign_empty},
        {"pad_callsign_short", test_pad_callsign_short},
        {"pad_callsign_invalid_chars", test_pad_callsign_invalid_chars},
        {"pad_callsign_multiple_invalid", test_pad_callsign_multiple_invalid},
        {"pad_callsign_lowercase", test_pad_callsign_lowercase},
        {"pad_callsign_mixed_case", test_pad_callsign_mixed_case},
        {"pad_callsign_numbers_only", test_pad_callsign_numbers_only},
        {"pad_callsign_hyphen_only", test_pad_callsign_hyphen_only},
        {"pad_callsign_spaces_in_input", test_pad_callsign_spaces_in_input},
        {"pad_callsign_trailing_hyphen", test_pad_callsign_trailing_hyphen},
        {"pad_callsign_leading_hyphen", test_pad_callsign_leading_hyphen},
        {"pad_callsign_multiple_hyphens", test_pad_callsign_multiple_hyphens},
        /* extract_speed_course tests */
        {"extract_speed_course_valid_basic", test_extract_speed_course_valid_basic},
        {"extract_speed_course_valid_no_extra_data", test_extract_speed_course_valid_no_extra_data},
        {"extract_speed_course_course_zero", test_extract_speed_course_course_zero},
        {"extract_speed_course_with_spaces", test_extract_speed_course_with_spaces},
        {"extract_speed_course_with_periods", test_extract_speed_course_with_periods},
        {"extract_speed_course_invalid_separator", test_extract_speed_course_invalid_separator},
        {"extract_speed_course_too_short", test_extract_speed_course_too_short},
        {"extract_speed_course_invalid_chars", test_extract_speed_course_invalid_chars},
        {"extract_speed_course_empty_string", test_extract_speed_course_empty_string},
        {"extract_speed_course_max_values", test_extract_speed_course_max_values},
        {"extract_speed_course_leading_zeros", test_extract_speed_course_leading_zeros},
        {"extract_speed_course_mixed_valid_chars", test_extract_speed_course_mixed_valid_chars},
        {"extract_speed_course_partial_match", test_extract_speed_course_partial_match},
        {"extract_speed_course_long_string", test_extract_speed_course_long_string},
        {"extract_speed_course_course_001", test_extract_speed_course_course_001},
        {NULL, NULL}
    };

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <test_name>\n", argv[0]);
        fprintf(stderr, "Available tests:\n");
        for (int i = 0; tests[i].name != NULL; i++)
        {
            fprintf(stderr, "  %s\n", tests[i].name);
        }
        return 1;
    }

    const char *test_name = argv[1];

    /* Run the requested test */
    for (int i = 0; tests[i].name != NULL; i++)
    {
        if (strcmp(test_name, tests[i].name) == 0)
        {
            return tests[i].func();
        }
    }

    fprintf(stderr, "Unknown test: %s\n", test_name);
    return 1;
}
