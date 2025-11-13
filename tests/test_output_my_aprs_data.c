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

/* 
 * Test program for output_my_aprs_data() function
 * 
 * This test program tests the APRS position data output function from interface.c.
 * It requires mocking most of the Xastir infrastructure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tests/test_framework.h"
#include "interface.h"

/* Forward declaration of function under test */
void output_my_aprs_data(void);

/* Forward declarations of mock control functions */
void mock_reset_all(void);
void mock_set_transmit_disable(int value);
void mock_set_emergency_beacon(int value);
void mock_set_my_callsign(const char *callsign);
void mock_set_my_position(const char *lat, const char *lon);
void mock_set_output_station_type(int type);
void mock_set_compressed_posit(int value);
void mock_set_phg(const char *phg);
void mock_set_course_speed(int course, int speed);
void mock_set_altitude(long altitude);
void mock_set_comment(const char *comment);
void mock_set_message_type(char type);
void mock_add_interface(int port, int device_type, int status, int transmit_enabled);
int mock_get_write_count(int port);
const char *mock_get_last_write(int port);
int mock_get_popup_count(void);
const char *mock_get_last_popup_title(void);
const char *mock_get_last_popup_message(void);

/* Test cases */

int test_transmit_disabled(void)
{
    mock_reset_all();
    mock_set_transmit_disable(1);
    mock_set_emergency_beacon(0);

    // Add one active TNC interface
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1); // transmit enabled

    output_my_aprs_data();
    
    // Should not transmit anything
    TEST_ASSERT(mock_get_write_count(0) == 0, 
        "No data should be transmitted when transmit is disabled");
    
    TEST_PASS("output_my_aprs_data with transmit disabled");
}

int test_transmit_disabled_with_emergency(void)
{
    mock_reset_all();
    mock_set_transmit_disable(1);
    mock_set_emergency_beacon(1);

    // Add one active TNC interface
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1); // transmit enabled

    output_my_aprs_data();
    
    // Should not transmit but should popup warning
    TEST_ASSERT(mock_get_write_count(0) == 0, 
        "No data should be transmitted when transmit is disabled even with emergency beacon");
    
    // Verify that a popup occurred warning about the situation
    TEST_ASSERT(mock_get_popup_count() > 0,
        "A popup warning should be displayed when emergency beacon is on but transmit is disabled");
    
    // Optionally check the popup content
    const char *popup_msg = mock_get_last_popup_message();
    if (popup_msg != NULL)
    {
        fprintf(stderr, "DEBUG: Popup message was: %s\n", popup_msg);
    }
    
    TEST_PASS("output_my_aprs_data with transmit disabled and emergency beacon");
}

int test_no_active_interfaces(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    
    output_my_aprs_data();
    
    // Should not transmit with no interfaces configured
    TEST_ASSERT(mock_get_write_count(0) == 0, 
        "No data should be transmitted with no active interfaces");
    
    TEST_PASS("output_my_aprs_data with no active interfaces");
}

int test_basic_position_output(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    
    // Add one active TNC interface
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1); // transmit enabled
    
    output_my_aprs_data();
    int write_count = mock_get_write_count(0);
    
    // Should transmit something
    TEST_ASSERT(write_count > 0, 
        "Data should be transmitted with active interface");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    TEST_PASS("output_my_aprs_data basic position output");
}

int test_network_stream_output(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    
    // Add network stream interface
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1); // transmit enabled
    
    output_my_aprs_data();
    
    // Should transmit with proper header
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on network stream");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Network stream should have TCPIP* in the path
    TEST_ASSERT(strstr(output, "TCPIP*") != NULL,
        "Network stream output should contain TCPIP* in path");
    
    TEST_PASS("output_my_aprs_data network stream output");
}

int test_interface_down(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    
    // Add interface but mark it as down
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_DOWN, 1); // transmit enabled
    
    output_my_aprs_data();
    
    // Should not transmit on down interface
    TEST_ASSERT(mock_get_write_count(0) == 0, 
        "No data should be transmitted on down interface");
    
    TEST_PASS("output_my_aprs_data with interface down");
}

int test_transmit_disabled_on_interface(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    
    // Add interface but disable transmit on it
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 0); // transmit disabled
    
    output_my_aprs_data();
    
    // Should not transmit on interface with transmit disabled
    TEST_ASSERT(mock_get_write_count(0) == 0, 
        "No data should be transmitted on interface with transmit disabled");
    
    TEST_PASS("output_my_aprs_data with interface transmit disabled");
}

int test_mobile_local_time_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(1); // APRS_MOBILE LOCAL TIME
    
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1); // transmit enabled
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Mobile local time should have @ and / in the timestamp
    // The @ comes after any header (like MYCALL, UNPROTO commands for TNC)
    const char *at_pos = strchr(output, '@');
    TEST_ASSERT(at_pos != NULL,
        "Mobile local time format should contain @ for timestamp");
    TEST_ASSERT(strchr(at_pos, '/') != NULL,
        "Mobile local time format should contain / in timestamp");
    
    TEST_PASS("output_my_aprs_data mobile local time format");
}

int test_mobile_zulu_datetime_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(2); // APRS_MOBILE ZULU DATE-TIME
    
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1); // transmit enabled
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Mobile zulu should have @ and z in the timestamp
    // The @ comes after any header (like MYCALL, UNPROTO commands for TNC)
    const char *at_pos = strchr(output, '@');
    TEST_ASSERT(at_pos != NULL,
        "Mobile zulu format should contain @ for timestamp");
    TEST_ASSERT(strchr(at_pos, 'z') != NULL,
        "Mobile zulu format should contain 'z' in timestamp");
    
    TEST_PASS("output_my_aprs_data mobile zulu datetime format");
}

int test_mobile_zulu_time_with_seconds_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(3); // APRS_MOBILE ZULU TIME w/SEC
    
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1); // transmit enabled
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Mobile zulu with seconds should have @ and h in the timestamp
    // The @ comes after any header (like MYCALL, UNPROTO commands for TNC)
    const char *at_pos = strchr(output, '@');
    TEST_ASSERT(at_pos != NULL,
        "Mobile zulu with seconds format should contain @ for timestamp");
    TEST_ASSERT(strchr(at_pos, 'h') != NULL,
        "Mobile zulu with seconds format should contain 'h' in timestamp");
    
    TEST_PASS("output_my_aprs_data mobile zulu time with seconds format");
}

/* Tests: Core Position Formatting */

int test_uncompressed_fixed_basic(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Should have uncompressed position format (may use low-res format like 470..0N)
    TEST_ASSERT(strstr(output, "470") != NULL,
        "Output should contain latitude starting with 470");
    TEST_ASSERT(strstr(output, "1220") != NULL,
        "Output should contain longitude starting with 1220");
    
    // Should have position without timestamp (! or =)
    TEST_ASSERT(strchr(output, '!') != NULL || strchr(output, '=') != NULL,
        "Output should contain position identifier (! or =)");
    
    TEST_PASS("output_my_aprs_data uncompressed fixed basic");
}

int test_uncompressed_fixed_phg(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_phg("PHG5132");
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Should have uncompressed position with PHG
    TEST_ASSERT(strstr(output, "470") != NULL,
        "Output should contain latitude starting with 470");
    TEST_ASSERT(strstr(output, "PHG5132") != NULL,
        "Output should contain PHG data");
    
    TEST_PASS("output_my_aprs_data uncompressed fixed with PHG");
}

int test_uncompressed_mobile_course_speed(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(1); // APRS_MOBILE LOCAL TIME (required for course/speed)
    mock_set_compressed_posit(0);
    mock_set_course_speed(90, 36);
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Should have uncompressed position with course/speed
    TEST_ASSERT(strstr(output, "470") != NULL,
        "Output should contain latitude starting with 470");
    TEST_ASSERT(strstr(output, "090/036") != NULL,
        "Output should contain course/speed 090/036");
    
    TEST_PASS("output_my_aprs_data uncompressed mobile with course/speed");
}

int test_uncompressed_fixed_altitude(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_altitude(1234);
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Should have uncompressed position with altitude
    TEST_ASSERT(strstr(output, "470") != NULL,
        "Output should contain latitude starting with 470");
    TEST_ASSERT(strstr(output, "/A=001234") != NULL,
        "Output should contain altitude /A=001234");
    
    TEST_PASS("output_my_aprs_data uncompressed fixed with altitude");
}

int test_compressed_fixed_basic(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(1);
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Compressed position should be much shorter and not contain readable lat/lon
    TEST_ASSERT(strstr(output, "4700.00N") == NULL,
        "Compressed output should not contain readable latitude");
    
    // Should still have position identifier
    TEST_ASSERT(strchr(output, '!') != NULL || strchr(output, '=') != NULL,
        "Output should contain position identifier (! or =)");
    
    TEST_PASS("output_my_aprs_data compressed fixed basic");
}

int test_message_type_equals(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_message_type('=');
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Should use = for message-capable station
    const char *equals_pos = strchr(output, '=');
    TEST_ASSERT(equals_pos != NULL,
        "Output should contain = for message-capable station");
    
    TEST_PASS("output_my_aprs_data message type equals");
}

int test_message_type_exclamation(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_message_type('!');
    mock_set_comment("Test Comment");
    
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // Should use ! for non-message-capable station
    const char *exclaim_pos = strchr(output, '!');
    TEST_ASSERT(exclaim_pos != NULL,
        "Output should contain ! for non-message-capable station");
    
    TEST_PASS("output_my_aprs_data message type exclamation");
}

/*  Device-Specific Formatting Tests */

int test_network_stream_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_comment("Test");
    
    // Add network stream interface (DEVICE_NET_STREAM = 5)
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on network stream");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    fprintf(stderr, "DEBUG: Network stream output: %s\n", output);
    
    // Network stream should have proper header format
    TEST_ASSERT(strstr(output, "N7ABC>") != NULL,
        "Network stream should include source callsign in header");
    TEST_ASSERT(strstr(output, "APX2") != NULL,
        "Network stream should include destination in header (APX2xx)");
    TEST_ASSERT(strstr(output, "TCPIP*") != NULL,
        "Network stream should include TCPIP* in path");
    
    // Should have a colon separator before data
    TEST_ASSERT(strchr(output, ':') != NULL,
        "Network stream should have colon separator");
    
    // Should end with \r\n (network format)
    size_t len = strlen(output);
    TEST_ASSERT(len >= 2 && output[len-2] == '\r' && output[len-1] == '\n',
        "Network stream should end with \\r\\n");
    
    TEST_PASS("output_my_aprs_data network stream format");
}

int test_serial_tnc_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_comment("Test");
    
    // Add serial TNC interface
    mock_add_interface(0, DEVICE_SERIAL_TNC, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on serial TNC");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    fprintf(stderr, "DEBUG: Serial TNC output: %s\n", output);
    
    // Serial TNC should have TNC commands
    // The output should contain MYCALL, UNPROTO, and CONV commands followed by data
    // These may be in the buffer as separate writes or combined
    TEST_ASSERT(strstr(output, "MYCALL") != NULL || strstr(output, "N7ABC") != NULL,
        "Serial TNC should include MYCALL command or callsign");
    
    // Should contain actual position data
    TEST_ASSERT(strstr(output, "470") != NULL,
        "Serial TNC should include position data");
    
    TEST_PASS("output_my_aprs_data serial TNC format");
}

int test_kiss_tnc_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_comment("Test");
    
    // Add KISS TNC interface
    mock_add_interface(0, DEVICE_SERIAL_KISS_TNC, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    // KISS TNC uses send_ax25_frame() which we're mocking
    // The data should still be written to the device buffer
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on KISS TNC");

    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    // KISS format should NOT have TNC commands like MYCALL or UNPROTO
    TEST_ASSERT(strstr(output, "MYCALL") == NULL,
        "KISS TNC should not include MYCALL command");
    TEST_ASSERT(strstr(output, "UNPROTO") == NULL,
        "KISS TNC should not include UNPROTO command");

    // Should have AX.25 framing (starts with FEND byte 0xC0 in KISS)
    // and at least contain the position data
    TEST_ASSERT(output != NULL && (unsigned char)output[0] == 0xC0,
        "KISS TNC output should start with FEND (0xC0)");
    TEST_ASSERT(strstr(output+2, "470") != NULL,
        "KISS TNC should contain position data starting with 470");

    TEST_PASS("output_my_aprs_data KISS TNC format");
}

int test_agwpe_format(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    
    mock_set_compressed_posit(0);
    mock_set_comment("Test");
    
    // Add AGWPE interface
    mock_add_interface(0, DEVICE_NET_AGWPE, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on AGWPE");
    
    const char *output = mock_get_last_write(0);
    TEST_ASSERT(output != NULL, "Output should not be NULL");
    
    fprintf(stderr, "DEBUG: AGWPE output: %s\n", output);
    
    // AGWPE should not have TNC commands
    TEST_ASSERT(strstr(output, "MYCALL") == NULL,
        "AGWPE should not include MYCALL command");
    TEST_ASSERT(strstr(output, "UNPROTO") == NULL,
        "AGWPE should not include UNPROTO command");
    
    // Note - I would like to expand this test to verify proper AGWPE framing,
    TEST_PASS("output_my_aprs_data AGWPE format");
}

int test_multiple_interfaces(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_comment("Test");
    
    // Add multiple interfaces
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);
    mock_add_interface(1, DEVICE_SERIAL_TNC, DEVICE_UP, 1);
    
    output_my_aprs_data();
    
    // Both interfaces should receive data
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on first interface");
    TEST_ASSERT(mock_get_write_count(1) > 0, 
        "Data should be transmitted on second interface");
    
    // Get output from port 0 and make a copy since mock_get_last_write uses a static buffer
    const char *output0_tmp = mock_get_last_write(0);
    TEST_ASSERT(output0_tmp != NULL, "First output should not be NULL");
    char output0_copy[1024];
    strncpy(output0_copy, output0_tmp, sizeof(output0_copy) - 1);
    output0_copy[sizeof(output0_copy) - 1] = '\0';
    const char *output0 = output0_copy;
    
    const char *output1 = mock_get_last_write(1);
    TEST_ASSERT(output1 != NULL, "Second output should not be NULL");
    
    fprintf(stderr, "DEBUG: Port 0 output: '%s'\n", output0);
    fprintf(stderr, "DEBUG: Port 1 output: '%s'\n", output1);
    
    // Network stream should have TCPIP*, TNC should not
    TEST_ASSERT(strstr(output0, "TCPIP*") != NULL,
        "Network stream should have TCPIP* in output");
    TEST_ASSERT(strstr(output1, "TCPIP*") == NULL,
        "Serial TNC should not have TCPIP* in output");
    /* Require both MYCALL and callsign to be present */
    TEST_ASSERT(strstr(output1, "MYCALL") != NULL,
        "Serial TNC should include MYCALL command");
    TEST_ASSERT(strstr(output1, "N7ABC") != NULL,
        "Serial TNC should include callsign");

    TEST_PASS("output_my_aprs_data multiple interfaces");
}

int test_mixed_interface_states(void)
{
    mock_reset_all();
    mock_set_transmit_disable(0);
    mock_set_my_callsign("N7ABC");
    mock_set_my_position("4700.00N", "12200.00W");
    mock_set_output_station_type(0); // APRS_FIXED
    mock_set_compressed_posit(0);
    mock_set_comment("Test");
    
    // Add interfaces with different states
    mock_add_interface(0, DEVICE_NET_STREAM, DEVICE_UP, 1);     // Active, TX enabled
    mock_add_interface(1, DEVICE_SERIAL_TNC, DEVICE_DOWN, 1);   // Down
    mock_add_interface(2, DEVICE_NET_STREAM, DEVICE_UP, 0);     // Active, TX disabled
    
    output_my_aprs_data();
    
    // Only first interface should transmit
    TEST_ASSERT(mock_get_write_count(0) > 0, 
        "Data should be transmitted on active interface with TX enabled");
    TEST_ASSERT(mock_get_write_count(1) == 0, 
        "No data should be transmitted on down interface");
    TEST_ASSERT(mock_get_write_count(2) == 0, 
        "No data should be transmitted on interface with TX disabled");
    TEST_PASS("output_my_aprs_data mixed interface states");
}

/* Test runner */
typedef struct
{
  const char *name;
  int (*func)(void);
} test_case_t;

int main(int argc, char *argv[])
{
  test_case_t tests[] =
  {
    {"transmit_disabled", test_transmit_disabled},
    {"transmit_disabled_with_emergency", test_transmit_disabled_with_emergency},
    {"no_active_interfaces", test_no_active_interfaces},
    {"basic_position_output", test_basic_position_output},
    {"network_stream_output", test_network_stream_output},
    {"interface_down", test_interface_down},
    {"transmit_disabled_on_interface", test_transmit_disabled_on_interface},
    /* Mobile time formats */
    {"mobile_local_time_format", test_mobile_local_time_format},
    {"mobile_zulu_datetime_format", test_mobile_zulu_datetime_format},
    {"mobile_zulu_time_with_seconds_format", test_mobile_zulu_time_with_seconds_format},
    /* Basic packet format tests */
    {"uncompressed_fixed_basic", test_uncompressed_fixed_basic},
    {"uncompressed_fixed_phg", test_uncompressed_fixed_phg},
    {"uncompressed_mobile_course_speed", test_uncompressed_mobile_course_speed},
    {"uncompressed_fixed_altitude", test_uncompressed_fixed_altitude},
    {"compressed_fixed_basic", test_compressed_fixed_basic},
    {"message_type_equals", test_message_type_equals},
    {"message_type_exclamation", test_message_type_exclamation},
    /* Phase 3: Device-specific formatting tests */
    {"network_stream_format", test_network_stream_format},
    {"serial_tnc_format", test_serial_tnc_format},
    {"kiss_tnc_format", test_kiss_tnc_format},
    {"agwpe_format", test_agwpe_format},
    {"multiple_interfaces", test_multiple_interfaces},
    {"mixed_interface_states", test_mixed_interface_states},
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
