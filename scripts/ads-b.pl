#!/usr/bin/perl -W
#
# Converts "dump1090" telnet port output to Xastir UDP input.  This script will
# parse packets containing lat/long, turn them into APRS-like packets, then use
# "xastir_udp_client" to inject them into Xastir. Must have "dump1090" running,
# and optionally "dump978" to dump packets into "dump1090" from the other
# frequency/protocol for ADS-B.
#
# Invoke it as:
#   ./ads-b.pl planes <passcode>
# or
#   ./ads-b.pl p1anes <passcode>
#
# Injecting them from "planes" or "p1anes" assures that Xastir won't try to adopt
# the APRS Item packets as its own and re-transmit them.
#
#
# I got the "dump1090" program from here originally:
#       https://github.com/antirez/dump1090
# but the author says the version here by another author is more feature-complete:
#       https://github.com/MalcolmRobb/dump1090
#
# Invoke the "dump1090" program like so:
#   "./dump1090 --interactive --net --aggressive --enable-agc"
# or
#   "./dump1090 --net --aggressive --enable-agc"
#
#
# Note: There's also "dump978" which listens to 978 MHz ADS-B transmissions. You can
# invoke "dump1090" as above, then invoke "dump978" like this:
#
#       rtl_sdr -f 978000000 -s 2083334 -g 0 -d 1 - | ./dump978 | ./uat2esnt | nc -q1 localhost 30001
#
# which will convert the 978 MHz packets into ADS-B ES packets and inject them into "dump1090"
# for decoding. I haven't determined yet whether those packets will come out on "dump1090"'s
# port 30003 (which this script uses). The above command also uses RTL device 1 instead of 0.
# If you're only interested in 978 MHz decoding, there's a way to start "dump1090" w/o a
# device attached, then start "dump978" and connect it to "dump1090".
#
#
# Then invoke this script in another xterm:
#   "./ads-b.pl planes <passcode>"
#
#
# NOTE: Do NOT use the same callsign as your Xastir instance, else it will
# "adopt" those APRS Item packets as its own and retransmit them. Code was
# added to the script to prevent such operation, but using "planes" as the
# callsign works great too!
#
#
# This script snags packets from port 30003 of "dump1090", parses them, then injects
# APRS packets into Xastir's UDP port (2023) if "Server Ports" are enabled in Xastir.
#
#
# Port 30001 is an input port (dump978 connects there and dumps data in).
#
# Port 30002 outputs in raw format, like:
#   *8D451E8B99019699C00B0A81F36E;
#   Every entry is separated by a simple newline (LF character, hex 0x0A).
#   The "callsign" (6 digits of hex) in chunk #4 = ICAO (airframe identifier).
#   Decoding the sentences:
#     https://www.sussex.ac.uk/webteam/gateway/file.php?name=coote-proj.pdf&site=20]
#     http://adsb-decode-guide.readthedocs.org/en/latest/
#
# Port 30003 outputs data is SBS1 (BaseStation) format, and is used by this script.
#   Decoding the sentences:
#   http://woodair.net/SBS/Article/Barebones42_Socket_Data.htm
#   NOTE: I changed the numbers by -1 to fit Perl's "split()" command field numbering.
#     Field 0:
#       Message type    (MSG, STA, ID, AIR, SEL or CLK)
#     Field 1:
#       Transmission Type   MSG sub types 1 to 8. Not used by other message types.
#     Field 2:
#       Session ID      Database Session record number
#     Field 3:
#       AircraftID      Database Aircraft record number
#     Field 4:
#       HexIdent    Aircraft Mode S hexadecimal code (What we use here... Unique identifier)
#     Field 5:
#       FlightID    Database Flight record number
#     Field 6:
#       Date message generated       As it says
#     Field 7:
#       Time message generated       As it says
#     Field 8:
#       Date message logged      As it says
#     Field 9:
#       Time message logged      As it says
#     Field 10:
#       Callsign    An eight digit flight ID - can be flight number or registration (or even nothing).
#     Field 11:
#       Altitude    Mode C altitude. Height relative to 1013.2mb (Flight Level). Not height AMSL..
#     Field 12:
#       GroundSpeed     Speed over ground (not indicated airspeed)
#     Field 13:
#       Track   Track of aircraft (not heading). Derived from the velocity E/W and velocity N/S
#     Field 14:
#       Latitude    North and East positive. South and West negative.
#     Field 15:
#       Longitude   North and East positive. South and West negative.
#     Field 16:
#       VerticalRate    64ft resolution
#     Field 17:
#       Squawk      Assigned Mode A squawk code.
#     Field 18:
#       Alert (Squawk change)   Flag to indicate squawk has changed.
#     Field 19:
#       Emergency   Flag to indicate emergency code has been set
#     Field 20:
#       SPI (Ident)     Flag to indicate transponder Ident has been activated.
#     Field 21:
#       IsOnGround      Flag to indicate ground squat switch is active
#
#
# Squawk Codes:
#   https://en.wikipedia.org/wiki/Transponder_%28aeronautics%29
#
#
# For reference, using 468/frequency (MHz) to get length of 1/2 wave dipole in feet:
#
#   1/2 wavelength on 1090 MHz: 5.15"
#   1/4 wavelength on 1090 MHz: 2.576" or 2  9/16"
#
#   1/2 wavelength on 978 MHz: 5.74"
#   1/4 wavelength on 978 MHz: 2.87" or 2  7/8"
#


eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
& eval 'exec perl -S $0 $argv:q'
if 0;

use IO::Socket;


# These two used for position ambiguity. Set them to a truncated lat/long based on your
# receive location, such as: "47  .  N" and "122  .  W" or ""475 .  N" and "1221 .  W",
# (with spaces in place of numbers) depending on how big you want the abiguity rectangle
# box to be.
#$my_lat = "47  .  N";   # Formatted like:  "475 .  N". Spaces for position ambiguity.
#$my_lon = "122  .  W";  # Formatted like: "1221 .  W". Spaces for position ambiguity.

$udp_client = "/usr/local/bin/xastir_udp_client";
$dump1090_host = "localhost"; # Server where dump1090 is running
$dump1090_port = 30003;     # 30003 is dump1090 default port

$xastir_host = "localhost"; # Server where Xastir is running
$xastir_port = 2023;        # 2023 is Xastir default UDP port

$plane_TTL = 30;            # Seconds after which posits too old to create APRS packets


$xastir_user = shift;
chomp $xastir_user;
if ($xastir_user eq "") {
  print "Please enter a callsign for Xastir injection, but not Xastir's callsign/SSID!\n";
  die;
}
$xastir_user =~ tr/a-z/A-Z/;

$xastir_pass = shift;
chomp $xastir_pass;
if ($xastir_pass eq "") {
  print "Please enter a passcode for Xastir injection\n";
  die;
}

# Connect to the server using a tcp socket
#
$socket = IO::Socket::INET->new(PeerAddr => $dump1090_host,
                                PeerPort => $dump1090_port,
                                Proto    => "tcp",
                                Type     => SOCK_STREAM)
  or die "Couldn't connect to $dump1090_host:$dump1090_port : $@\n";


# Flush output buffers often
select((select(STDOUT), $| = 1)[0]);


# Check Xastir's callsign/SSID to make sure we don't have a collision.  This
# will prevent Xastir adopting the Items as its own and retransmitting them.
#   xastir_udp_client localhost 2023 <callsign> <passcode> -identify
#   Received: WE7U-13
#
$injection_call = $xastir_user;
$injection_call =~ s/-\d+//;    # Get rid of dash and numbers

$injection_ssid = $xastir_user;
$injection_ssid =~ s/\w+//;     # Get rid of letters
$injection_ssid =~ s/-//;       # Get rid of dash
if ($injection_ssid eq "") { $injection_ssid = 0; }

# Find out Callsign/SSID of Xastir instance
$result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass -identify`;
if ($result =~ m/NACK/) {
  die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
}
($remote_call, $remote_ssid) = split('-', $result);

chomp($remote_call);
$remote_call =~ s/Received:\s+//;

chomp($remote_ssid);
if ($remote_ssid eq "") { $remote_ssid = 0; }

#print "$remote_call $remote_ssid $injection_call $injection_ssid\n";
#if ($remote_call eq $injection_call) { print "Call matches\n"; }
#if ($remote_ssid == $injection_ssid) { print "SSID matches\n"; }

if (     ($remote_call eq $injection_call)
     &&  ($remote_ssid == $injection_ssid) ) {
    $remote_ssid++;
    $remote_ssid%= 16;  # Increment by 1 mod 16
    $xastir_user = "$remote_call-$remote_ssid";
    print "Injection conflict. Corrected. New user = $xastir_user\n";
}


while (<$socket>)
{

  # For testing:
  #
  #$_ = "MSG,3,,,A0CF8D,,,,,,,28000,,,47.87670,-122.27269,,,0,0,0,0";
  #$_ = "MSG,5,,,AD815E,,,,,,,575,,,,,,,,,,";
  #$_ = "MSG,4,,,AAB812,,,,,,,,454,312,,,0,,0,0,0,0";
  #$_ = "MSG,3,,,ABB2D5,,,,,,,40975,,,47.68648,-122.67834,,,0,0,0,0";   # Lat/long/altitude (ft)
  #$_ = "MSG,1,,,A2CB32,,,,,,BOE181  ,,,,,,,,0,0,0,0";  # Tail number or flight number
  #$_ = "MSG,4,,,A0F4F6,,,,,,,,175,152,,,-1152,,0,0,0,0";   # Ground speed (knots)/Track


  chomp;


  # Sentences have either 10 or 22 fields.
  @fields = split(",");


  $plane_id = $fields[4];
  $print1 = $plane_id;
  #
  # Decode the country of registration via this webpage:
  #   http://www.kloth.net/radio/icao24alloc.php
  # Add it to the APRS comment field.  Remove the Tactical call
  # from the comment field since it is redundant.
  # Match on more specific first (longer string).
  #
  $registry = "Registry?";

  # Convert $plane_id to binary string
  $binary_plane_id = unpack ('B*', pack ('H*',$plane_id) );

  # 14-bit addresses:
  # -----------------
  if ($binary_plane_id =~ m/^01010000000100/) { $registry = "Albania"; }
  if ($binary_plane_id =~ m/^00001100101000/) { $registry = "Antigua and Barbuda"; }
  if ($binary_plane_id =~ m/^01100000000000/) { $registry = "Armenia"; }
  if ($binary_plane_id =~ m/^01100000000010/) { $registry = "Azerbaijan"; }
  if ($binary_plane_id =~ m/^00001010101000/) { $registry = "Barbados"; }
  if ($binary_plane_id =~ m/^01010001000000/) { $registry = "Belarus"; }
  if ($binary_plane_id =~ m/^00001010101100/) { $registry = "Belize"; }
  if ($binary_plane_id =~ m/^00001001010000/) { $registry = "Benin"; }
  if ($binary_plane_id =~ m/^01101000000000/) { $registry = "Bhutan"; }
  if ($binary_plane_id =~ m/^01010001001100/) { $registry = "Bosnia and Herzegovina"; }
  if ($binary_plane_id =~ m/^00000011000000/) { $registry = "Botswana"; }
  if ($binary_plane_id =~ m/^10001001010100/) { $registry = "Brunei Darussalam"; }
  if ($binary_plane_id =~ m/^00001001011000/) { $registry = "Cape Verde"; }
  if ($binary_plane_id =~ m/^00000011010100/) { $registry = "Comoros"; }
  if ($binary_plane_id =~ m/^10010000000100/) { $registry = "Cook Islands"; }
  if ($binary_plane_id =~ m/^01010000000111/) { $registry = "Croatia"; }
  if ($binary_plane_id =~ m/^01001100100000/) { $registry = "Cyprus"; }
  if ($binary_plane_id =~ m/^00001001100000/) { $registry = "Djibouti"; }
  if ($binary_plane_id =~ m/^00100000001000/) { $registry = "Eritrea"; }
  if ($binary_plane_id =~ m/^01010001000100/) { $registry = "Estonia"; }
  if ($binary_plane_id =~ m/^01010001010000/) { $registry = "Georgia"; }
  if ($binary_plane_id =~ m/^00001100110000/) { $registry = "Grenada"; }
  if ($binary_plane_id =~ m/^00000100100000/) { $registry = "Guinea-Bissau"; }
  if ($binary_plane_id =~ m/^01101000001100/) { $registry = "Kazakhstan"; }
  if ($binary_plane_id =~ m/^11001000111000/) { $registry = "Kiribati"; }
  if ($binary_plane_id =~ m/^01100000000100/) { $registry = "Kyrgyzstan"; }
  if ($binary_plane_id =~ m/^01010000001011/) { $registry = "Latvia"; }
  if ($binary_plane_id =~ m/^00000100101000/) { $registry = "Lesotho"; }
  if ($binary_plane_id =~ m/^01010000001111/) { $registry = "Lithuania"; }
  if ($binary_plane_id =~ m/^01001101000000/) { $registry = "Luxembourg"; }
  if ($binary_plane_id =~ m/^00000101101000/) { $registry = "Maldives"; }
  if ($binary_plane_id =~ m/^01001101001000/) { $registry = "Malta"; }
  if ($binary_plane_id =~ m/^10010000000000/) { $registry = "Marshall Islands"; }
  if ($binary_plane_id =~ m/^00000101111000/) { $registry = "Mauritania"; }
  if ($binary_plane_id =~ m/^00000110000000/) { $registry = "Mauritius"; }
  if ($binary_plane_id =~ m/^01101000000100/) { $registry = "Micronesia"; }   # Micronesia, Federated States of
  if ($binary_plane_id =~ m/^01001101010000/) { $registry = "Monaco"; }
  if ($binary_plane_id =~ m/^01101000001000/) { $registry = "Mongolia"; }
  if ($binary_plane_id =~ m/^00100000000100/) { $registry = "Namibia"; }
  if ($binary_plane_id =~ m/^11001000101000/) { $registry = "Nauru"; }
  if ($binary_plane_id =~ m/^01110000110000/) { $registry = "Oman"; }
  if ($binary_plane_id =~ m/^01101000010000/) { $registry = "Palau"; }
  if ($binary_plane_id =~ m/^00000110101000/) { $registry = "Qatar"; }
  if ($binary_plane_id =~ m/^01010000010011/) { $registry = "Rep. of Moldova"; }   # Republic of Moldova
  if ($binary_plane_id =~ m/^11001000110000/) { $registry = "Saint Lucia"; }
  if ($binary_plane_id =~ m/^00001011110000/) { $registry = "Saint Vincent and the Grenadines"; }
  if ($binary_plane_id =~ m/^10010000001000/) { $registry = "Samoa"; }
  if ($binary_plane_id =~ m/^01010000000000/) { $registry = "San Marino"; }
  if ($binary_plane_id =~ m/^00001001111000/) { $registry = "Sao Tome and Principe"; }
  if ($binary_plane_id =~ m/^00000111010000/) { $registry = "Seychelles"; }
  if ($binary_plane_id =~ m/^00000111011000/) { $registry = "Sierra Leone"; }
  if ($binary_plane_id =~ m/^01010000010111/) { $registry = "Slovakia"; }
  if ($binary_plane_id =~ m/^01010000011011/) { $registry = "Slovenia"; }
  if ($binary_plane_id =~ m/^10001001011100/) { $registry = "Solomon Islands"; }
  if ($binary_plane_id =~ m/^00000111101000/) { $registry = "Swaziland"; }
  if ($binary_plane_id =~ m/^01010001010100/) { $registry = "Tajikistan"; }
  if ($binary_plane_id =~ m/^01010001001000/) { $registry = "Macedonia"; } # The former Yugoslav Republic of Macedonia
  if ($binary_plane_id =~ m/^11001000110100/) { $registry = "Tonga"; }
  if ($binary_plane_id =~ m/^01100000000110/) { $registry = "Turkmenistan"; }
  if ($binary_plane_id =~ m/^01010000011111/) { $registry = "Uzbekistan"; }
  if ($binary_plane_id =~ m/^11001001000000/) { $registry = "Vanuatu"; }
  if ($binary_plane_id =~ m/^00000000010000/) { $registry = "Zimbabwe"; }
  if ($binary_plane_id =~ m/^10001001100100/) { $registry = "ICAO"; }
  if ($binary_plane_id =~ m/^11110000100100/) { $registry = "ICAO"; }
  #
  # 12-bit addresses:
  # -----------------
  if ($binary_plane_id =~ m/^011100000000/) { $registry = "Afghanistan"; }
  if ($binary_plane_id =~ m/^000010010000/) { $registry = "Angola"; }
  if ($binary_plane_id =~ m/^000010101000/) { $registry = "Bahamas"; }
  if ($binary_plane_id =~ m/^100010010100/) { $registry = "Bahrain"; }
  if ($binary_plane_id =~ m/^011100000010/) { $registry = "Bangladesh"; }
  if ($binary_plane_id =~ m/^111010010100/) { $registry = "Bolivia"; }
  if ($binary_plane_id =~ m/^000010011100/) { $registry = "Burkina Faso"; }
  if ($binary_plane_id =~ m/^000000110010/) { $registry = "Burundi"; }
  if ($binary_plane_id =~ m/^011100001110/) { $registry = "Cambodia"; }
  if ($binary_plane_id =~ m/^000000110100/) { $registry = "Cameroon"; }
  if ($binary_plane_id =~ m/^000001101100/) { $registry = "Central African Rep."; }    # Central African Republic
  if ($binary_plane_id =~ m/^000010000100/) { $registry = "Chad"; }
  if ($binary_plane_id =~ m/^111010000000/) { $registry = "Chile"; }
  if ($binary_plane_id =~ m/^000010101100/) { $registry = "Colombia"; }
  if ($binary_plane_id =~ m/^000000110110/) { $registry = "Congo"; }
  if ($binary_plane_id =~ m/^000010101110/) { $registry = "Costa Rica"; }
  if ($binary_plane_id =~ m/^000000111000/) { $registry = "Cote d Ivoire"; }
  if ($binary_plane_id =~ m/^000010110000/) { $registry = "Cuba"; }
  if ($binary_plane_id =~ m/^000010001100/) { $registry = "Dem. Rep. of the Congo"; }    # Democratic Republic of the Congo
  if ($binary_plane_id =~ m/^000011000100/) { $registry = "Dominican Republic"; }
  if ($binary_plane_id =~ m/^111010000100/) { $registry = "Ecuador"; }
  if ($binary_plane_id =~ m/^000010110010/) { $registry = "El Salvador"; }
  if ($binary_plane_id =~ m/^000001000010/) { $registry = "Equatorial Guinea"; }
  if ($binary_plane_id =~ m/^000001000000/) { $registry = "Ethiopia"; }
  if ($binary_plane_id =~ m/^110010001000/) { $registry = "Fiji"; }
  if ($binary_plane_id =~ m/^000000111110/) { $registry = "Gabon"; }
  if ($binary_plane_id =~ m/^000010011010/) { $registry = "Gambia"; }
  if ($binary_plane_id =~ m/^000001000100/) { $registry = "Ghana"; }
  if ($binary_plane_id =~ m/^000010110100/) { $registry = "Guatemala"; }
  if ($binary_plane_id =~ m/^000001000110/) { $registry = "Guinea"; }
  if ($binary_plane_id =~ m/^000010110110/) { $registry = "Guyana"; }
  if ($binary_plane_id =~ m/^000010111000/) { $registry = "Haiti"; }
  if ($binary_plane_id =~ m/^000010111010/) { $registry = "Honduras"; }
  if ($binary_plane_id =~ m/^010011001100/) { $registry = "Iceland"; }
  if ($binary_plane_id =~ m/^010011001010/) { $registry = "Ireland"; }
  if ($binary_plane_id =~ m/^000010111110/) { $registry = "Jamaica"; }
  if ($binary_plane_id =~ m/^000001001100/) { $registry = "Kenya"; }
  if ($binary_plane_id =~ m/^011100000110/) { $registry = "Kuwait"; }
  if ($binary_plane_id =~ m/^011100001000/) { $registry = "Lao People's Dem. Rep."; }    # Lao People's Democratic Republic
  if ($binary_plane_id =~ m/^000001010000/) { $registry = "Liberia"; }
  if ($binary_plane_id =~ m/^000001010100/) { $registry = "Madagascar"; }
  if ($binary_plane_id =~ m/^000001011000/) { $registry = "Malawi"; }
  if ($binary_plane_id =~ m/^000001011100/) { $registry = "Mali"; }
  if ($binary_plane_id =~ m/^000000000110/) { $registry = "Mozambique"; }
  if ($binary_plane_id =~ m/^011100000100/) { $registry = "Myanmar"; }
  if ($binary_plane_id =~ m/^011100001010/) { $registry = "Nepal"; }
  if ($binary_plane_id =~ m/^000011000000/) { $registry = "Nicaragua"; }
  if ($binary_plane_id =~ m/^000001100010/) { $registry = "Niger"; }
  if ($binary_plane_id =~ m/^000001100100/) { $registry = "Nigeria"; }
  if ($binary_plane_id =~ m/^000011000010/) { $registry = "Panama"; }
  if ($binary_plane_id =~ m/^100010011000/) { $registry = "Papua New Guinea"; }
  if ($binary_plane_id =~ m/^111010001000/) { $registry = "Paraguay"; }
  if ($binary_plane_id =~ m/^111010001100/) { $registry = "Peru"; }
  if ($binary_plane_id =~ m/^000001101110/) { $registry = "Rwanda"; }
  if ($binary_plane_id =~ m/^000001110000/) { $registry = "Senegal"; }
  if ($binary_plane_id =~ m/^000001111000/) { $registry = "Somalia"; }
  if ($binary_plane_id =~ m/^000001111100/) { $registry = "Sudan"; }
  if ($binary_plane_id =~ m/^000011001000/) { $registry = "Suriname"; }
  if ($binary_plane_id =~ m/^000010001000/) { $registry = "Togo"; }
  if ($binary_plane_id =~ m/^000011000110/) { $registry = "Trinidad and Tobago"; }
  if ($binary_plane_id =~ m/^000001101000/) { $registry = "Uganda"; }
  if ($binary_plane_id =~ m/^100010010110/) { $registry = "United Arab Emirates"; }
  if ($binary_plane_id =~ m/^000010000000/) { $registry = "United Rep. of Tanzania"; } # United Republic of Tanzania
  if ($binary_plane_id =~ m/^111010010000/) { $registry = "Uruguay"; }
  if ($binary_plane_id =~ m/^100010010000/) { $registry = "Yemen"; }
  if ($binary_plane_id =~ m/^000010001010/) { $registry = "Zambia"; }
  #
  # 9-bit addresses:
  # ----------------
  if ($binary_plane_id =~ m/^000010100/) { $registry = "Algeria"; }
  if ($binary_plane_id =~ m/^010001000/) { $registry = "Austria"; }
  if ($binary_plane_id =~ m/^010001001/) { $registry = "Belgium"; }
  if ($binary_plane_id =~ m/^010001010/) { $registry = "Bulgaria"; }
  if ($binary_plane_id =~ m/^010010011/) { $registry = "Czech Rep."; } # Czech Republic
  if ($binary_plane_id =~ m/^011100100/) { $registry = "Dem. People's Rep. of Korea"; }  # Democratic People's Republic of Korea
  if ($binary_plane_id =~ m/^010001011/) { $registry = "Denmark"; }
  if ($binary_plane_id =~ m/^000000010/) { $registry = "Egypt"; }
  if ($binary_plane_id =~ m/^010001100/) { $registry = "Finland"; }
  if ($binary_plane_id =~ m/^010001101/) { $registry = "Greece"; }
  if ($binary_plane_id =~ m/^010001110/) { $registry = "Hungary"; }
  if ($binary_plane_id =~ m/^100010100/) { $registry = "Indonesia"; }
  if ($binary_plane_id =~ m/^011100110/) { $registry = "Iran"; }  # Iran, Islamic Republic of
  if ($binary_plane_id =~ m/^011100101/) { $registry = "Iraq"; }
  if ($binary_plane_id =~ m/^011100111/) { $registry = "Israel"; }
  if ($binary_plane_id =~ m/^011101000/) { $registry = "Jordan"; }
  if ($binary_plane_id =~ m/^011101001/) { $registry = "Lebanon"; }
  if ($binary_plane_id =~ m/^000000011/) { $registry = "Libyan Arab Jamahiriya"; }
  if ($binary_plane_id =~ m/^011101010/) { $registry = "Malaysia"; }
  if ($binary_plane_id =~ m/^000011010/) { $registry = "Mexico"; }
  if ($binary_plane_id =~ m/^000000100/) { $registry = "Morocco"; }
  if ($binary_plane_id =~ m/^010010000/) { $registry = "Netherlands"; }    # Netherlands, Kingdom of the
  if ($binary_plane_id =~ m/^110010000/) { $registry = "New Zealand"; }
  if ($binary_plane_id =~ m/^010001111/) { $registry = "Norway"; }
  if ($binary_plane_id =~ m/^011101100/) { $registry = "Pakistan"; }
  if ($binary_plane_id =~ m/^011101011/) { $registry = "Phillipines"; }
  if ($binary_plane_id =~ m/^010010001/) { $registry = "Poland"; }
  if ($binary_plane_id =~ m/^010010010/) { $registry = "Portugal"; }
  if ($binary_plane_id =~ m/^011100011/) { $registry = "Rep. of Korea"; }  # Republic of Korea
  if ($binary_plane_id =~ m/^010010100/) { $registry = "Romania"; }
  if ($binary_plane_id =~ m/^011100010/) { $registry = "Saudi Arabia"; }
  if ($binary_plane_id =~ m/^011101101/) { $registry = "Singapore"; }
  if ($binary_plane_id =~ m/^000000001/) { $registry = "South Africa"; }
  if ($binary_plane_id =~ m/^011101110/) { $registry = "Sri Lanka"; }
  if ($binary_plane_id =~ m/^010010101/) { $registry = "Sweden"; }
  if ($binary_plane_id =~ m/^010010110/) { $registry = "Switzerland"; }
  if ($binary_plane_id =~ m/^011101111/) { $registry = "Syrian Arab Rep."; }   # Syrian Arab Republic
  if ($binary_plane_id =~ m/^100010000/) { $registry = "Thailand"; }
  if ($binary_plane_id =~ m/^000000101/) { $registry = "Tunisia"; }
  if ($binary_plane_id =~ m/^010010111/) { $registry = "Turkey"; }
  if ($binary_plane_id =~ m/^010100001/) { $registry = "Ukraine"; }
  if ($binary_plane_id =~ m/^000011011/) { $registry = "Venezuela"; }
  if ($binary_plane_id =~ m/^100010001/) { $registry = "Viet Nam"; }
  if ($binary_plane_id =~ m/^010011000/) { $registry = "Yugoslavia"; }
  if ($binary_plane_id =~ m/^111100000/) { $registry = "ICAO"; }
  #
  # 6-bit addresses:
  # ----------------
  if ($binary_plane_id =~ m/^001100/) { $registry = "Italy"; }
  if ($binary_plane_id =~ m/^001101/) { $registry = "Spain"; }
  if ($binary_plane_id =~ m/^001110/) { $registry = "France"; }
  if ($binary_plane_id =~ m/^001111/) { $registry = "Germany"; }
  if ($binary_plane_id =~ m/^010000/) { $registry = "U.K."; }  # United Kingdom
  if ($binary_plane_id =~ m/^011110/) { $registry = "China"; }
  if ($binary_plane_id =~ m/^011111/) { $registry = "Australia"; }
  if ($binary_plane_id =~ m/^100000/) { $registry = "India"; }
  if ($binary_plane_id =~ m/^100001/) { $registry = "Japan"; }
  if ($binary_plane_id =~ m/^110000/) { $registry = "Canada"; }
  if ($binary_plane_id =~ m/^111000/) { $registry = "Argentina"; }
  if ($binary_plane_id =~ m/^111001/) { $registry = "Brazil"; }
  #
  # 4-bit addresses:
  # ----------------
  if ($binary_plane_id =~ m/^1010/) { $registry = "U.S."; }        # United States
  if ($binary_plane_id =~ m/^0001/) { $registry = "Russian Fed."; }  # Russian Federation

 
  # Parse altitude if MSG Type 2, 3, 5, 6, or 7, save in hash.
  $print2 = "       ";
  if (    $fields[1] == 2
       || $fields[1] == 3
       || $fields[1] == 5
       || $fields[1] == 6
       || $fields[1] == 7 ) {

    if ( $fields[11] ne ""
         && $fields[11] ne 0 ) {

      $old = "";
      if (defined($altitude{$plane_id})) {
        $old = $altitude{$plane_id};
      }

      # Save new altitude
      $altitude{$plane_id} = sprintf("%06d", $fields[11]);

      if ($old ne $altitude{$plane_id}) {
        $print2 = sprintf("%5sft", $fields[11]);
        $newdata{$plane_id}++;
      }
    }
  }


  # Parse ground speed and track if MSG Type 2 or 4, save in hash.
  $print3 = "     ";
  $print4 = "    ";
  if ( $fields[1] ==  4 || $fields[1] == 2 ) {

    if ( $fields[12] ne "" ) {

      $old = "";
      if (defined($groundspeed{$plane_id})) {
        $old = $groundspeed{$plane_id};
      }

      # Save new ground speed
      $groundspeed{$plane_id} = $fields[12];

      if ($old ne $groundspeed{$plane_id}) {
        $print3 = sprintf("%3skn", $fields[12]);
        $newdata{$plane_id}++;
      }
    }

    if ( $fields[13] ne "" ) {

      $old = "";
      if (defined($track{$plane_id})) {
        $old = $track{$plane_id};
      }

      # Save new track
      $track{$plane_id} = $fields[13];

      if ($old ne $track{$plane_id}) {
        $print4 = sprintf("%3s°", $fields[13]);
        $newdata{$plane_id}++;
      }
    }
  }


  # Parse lat/long if MSG Type 2 or 3, save in hash.
  $print5 = "\t\t\t";
  if (  ( $fields[1] == 2 || $fields[1] == 3 )
       && $fields[14] ne ""
       && $fields[15] ne "" ) {

    $oldlat = "";
    if (defined($lat{$plane_id})) {
      $oldlat = $lat{$plane_id};
    }

    $lat = $fields[14] * 1.0;
    if ($lat >= 0.0) {
      $NS = 'N';
    } else {
      $NS = 'S';
      $lat = abs($lat);
    }
    $latdeg = int($lat);
    $latmins = $lat - $latdeg;
    $latmins2 = $latmins * 60.0;
    # Save new latitude
    $lat{$plane_id} = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

    $oldlon = "";
    if (defined($lon{$plane_id})) {
      $oldlon = $lon{$plane_id};
    }

    $lon = $fields[15] * 1.0;
    if ($lon >= 0.0) {
      $EW = 'E';
    } else {
      $EW = 'W';
      $lon = abs($lon);
    }
    $londeg = int($lon);
    $lonmins = $lon - $londeg;
    $lonmins2 = $lonmins * 60.0;
    # Save new longitude
    $lon{$plane_id} = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    if ( ($oldlat ne $lat{$plane_id}) || ($oldlon ne $lon{$plane_id}) ) {
      $print5 = sprintf("%8s,%9s", $lat{$plane_id},$lon{$plane_id});

      # Save most recent posit time
      $last_posit_time{$plane_id} = time();

      $newdata{$plane_id}++;

      # Tactical callsign:
      # Have new lat/lon. Check whether we have a tail number
      # already defined, which tells us whether we've assigned
      # a tactical callsign yet. If not, assign one that includes
      # the plane_id and the country of registration.
      if ( !defined($tail{$plane_id}) && ($registry ne "Registry?") ) {

        # Assign tactical call = $plane_id + registry
        # Max tactical call in Xastir is 57 chars (56 + terminator?)
        #
        $tac_call = $plane_id . " (" . $registry . ")";
        $tac_call =~ s/\s+/ /g; # Change multiple spaces to one
        $aprs = $xastir_user . '>' . "APRS::TACTICAL :" . $plane_id . "=" . $tac_call;

        print("$print1  $print2  $print3  $print4  $print6  $aprs\n");

        # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
        $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
          die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
      }
    }
  }


  # Save tail or flight number in hash if MSG Type 1 or "ID" sentence.
  if (    ($fields[0] eq "ID" || $fields[1] ==  1)
       && $fields[10] ne "????????"
       && $fields[10] ne "" ) {

    $old = "";
    if (defined($tail{$plane_id})) {
      $old = $tail{$plane_id}; 
    }

    # Save new tail number or flight number, assign tactical call
    $tail{$plane_id} = $fields[10];

    if ($old ne $tail{$plane_id}) {
      $print6 = sprintf("Tactical=%-9s", $fields[10]);
      $newdata{$plane_id}++;
 
      # Assign tactical call = tail number or flight number + registry (if defined)
      # Max tactical call in Xastir is 57 chars (56 + terminator?)
      #
      $tac_call = $fields[10];
      if ($registry ne "Registry?") {
        $tac_call = $fields[10] . " (" . $registry . ")";
        $tac_call =~ s/\s+/ /g; # Change multiple spaces to one
      }
      $aprs = $xastir_user . '>' . "APRS::TACTICAL :" . $plane_id . "=" . $tac_call;

      print("$print1  $print2  $print3  $print4  $print6  $aprs\n");
 
      # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
      $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
      if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
      }
    }
  }


  $squawk_txt = "";
  if ( defined($fields[17]) && ($fields[17] ne "") ) {
    $squawk = $fields[17];
    $squawk_txt = sprintf(" SQUAWK=%s", $squawk); 
  }


  $emerg_txt = "";
  $emergency = "0";
  if ( defined($fields[19]) ) {
    $emergency = $fields[19];
  }
  if ( $emergency eq "-1" ) {       # Emergency of some type
    $emerg_txt = " EMERGENCY=";     # Keyword triggers Xastir's emergency mode!!!
 
    # Check squawk code
    if ($squawk eq "7500") {        # Unlawful Interference (hijacking)
      $emerg_txt = $emerg_txt . "Hijacking";
    }
    if ($squawk eq "7600") {        # Communications failure/problems
      $emerg_txt = $emerg_txt . "Comms_Failure";
    }
    if ($squawk eq "7700") {        # General Emergency
      $emerg_txt = $emerg_txt . "General";
    }
  }


  $onGroundTxt = "";
  if ( defined($fields[21]) ) {
    $onGround = $fields[21];
    if ($onGround eq "-1") {
      $onGroundTxt = " On_Ground";
    }
  }
 

  # Above we parsed some message that changed some of our data, send out the
  # new APRS string if we have enough data defined.
  #
  if ( defined($newdata{$plane_id})
       && $newdata{$plane_id} ) {

    # Auto-switch the symbol based on speed/altitude.
    # Symbols:
    #   Small airplane = /'
    #       Helicopter = /X 
    #   Large aircraft = /^
    #         Aircraft = \^ (Not used in this script)
    #
    #  Cessna 150:  57 knots minimum.
    # Twin Cessna: 215 knots maximum.
    #  UH-1N Huey: 110 knots maximum.
    # Landing speed Boeing 757: 126 knots.
    #
    # If <= 10000 feet and  1 -  56 knots: Helicopter
    # If <= 20000 feet and 57 - 125 knots: Small aircraft
    # If >  20000 feet                   : Large aircraft
    # if                      > 126 knots: Large aircraft
    #
    $symbol = "'"; # Start with small aircraft symbol
 
    $newtrack = "000";
    if ( defined($track{$plane_id}) ) {
      $newtrack = sprintf("%03d", $track{$plane_id} );
    }

    $newspeed = "000";
    if ( defined ($groundspeed{$plane_id}) ) {
      $newspeed = sprintf("%03d", $groundspeed{$plane_id} );
      if ( ($groundspeed{$plane_id} > 0) && ($groundspeed{$plane_id} < 57) ) {
        $symbol = "X";  # Switch to helicopter symbol
      }
      if ($groundspeed{$plane_id} > 126) {
        $symbol = "^";  # Switch to large aircraft symbol
      } 
    }

    $newtail = "";
    if ( defined($tail{$plane_id}) ) {
      $newtail = " $tail{$plane_id}";
    }

    $newalt = "";
    if ( defined($altitude{$plane_id}) ) {
      $newalt = " /A=$altitude{$plane_id}";

      if ($altitude{$plane_id} > 20000) {
        $symbol = "^";  # Switch to large aircraft symbol
      }
      elsif ($symbol eq "^") {
        # Do nothing, already switched to large aircraft due to speed
      }
      elsif ($symbol eq "X" && $altitude{$plane_id} > 10000) {
        $symbol = "'";  # Switch to small aircraft from helicopter
      }
    }

    # Do we have a lat/lon and is it recent enough?
    if (    defined($lat{$plane_id})
         && defined($lon{$plane_id}) ) {

      $age = time() - $last_posit_time{$plane_id};
      if ( $age > $plane_TTL ) {
        #
        # We have a lat/lon but it is too old
        #
        $print_age1 = sprintf("(%s%s)", $age, "s");
#       $print_age2 = sprintf("%-16s", $print_age1);
        $print_age2 = sprintf("%18s", $print_age1);
#       print("$print1  $print2  $print3  $print4                      Lat/Lon old: No APRS packet generated\n");   # For testing
        print("$print1  $print2  $print3  $print4  $print_age2\t\t\t\t\t\t\t\t    ($registry)\n");
#       print("$print1  $print2  $print3  $print4  $emerg_txt$squawk_txt$onGroundTxt\n");
 
      }
      else {
        #
        # Yes, we have a lat/lon and it is recent enough
        #
        $aprs="$xastir_user>APRS:)$plane_id!$lat{$plane_id}/$lon{$plane_id}$symbol$newtrack/$newspeed$newalt$newtail$emerg_txt$squawk_txt$onGroundTxt ($registry)";
        if (    $age > 0                    # Lat/lon is aging a bit
             || $print5 eq "\t\t\t" ) {     # Didn't parse lat/lon this time
          $print_age1 = sprintf("%s%s", $age, "s");
#         $print_age2 = sprintf("%-18s", $print_age1);
          $print_age2 = sprintf("%18s", $print_age1);
          print("$print1  $print2  $print3  $print4  $print_age2  $aprs\n");
        }
        else {
          print("$print1  $print2  $print3  $print4  $print5  $aprs\n");
        }

        # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
        $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
          die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
      }
    }
    else {
      #
      # No, we have no lat/lon, or it is too old
      #
      # Send out packet with position ambiguity since we don't know
      # the lat/long, but it is definitely within receive range.
#      $aprs="$xastir_user>APRS:)$plane_id!$my_lat/$my_lon$symbol$newtrack/$newspeed$newalt$newtail";
#      print "\t\t\t\t\t\t\t\t$aprs\n";

      # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
#      $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
#       if ($result =~ m/NACK/) {
#         die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
#       }
      $print_age = sprintf("%18s", "-");
#     print("$print1  $print2  $print3  $print4  $emerg_txt$squawk_txt$onGroundTxt\n");
#     print("$print1  $print2  $print3  $print4  -$emerg_txt$squawk_txt$onGroundTxt\n");
      print("$print1  $print2  $print3  $print4  $print_age$emerg_txt$squawk_txt$onGroundTxt\t\t\t\t\t\t\t\t    ($registry)\n");
 
    }

    $newdata{$plane_id} = 0;
  }

  # Convert altitude to altitude above MSL instead of altitude from a reference barometric reading?
  # Info:  http://forums.flyer.co.uk/viewtopic.php?t=16375
  # How many millibar in 1 feet of air? The answer is 0.038640888 @ 0C (25.879232 ft/mb).
  # How many millibar in 1 foot of air [15 °C]? The answer is 0.036622931 @ 15C (27.3053 ft/mb).
  # Flight Level uses a pressure of 29.92 "Hg (1,013.2 mb).
  # Yesterday the pressure here was about 1013, so the altitudes looked right on. Today the planes
  # are reporting 500' higher via ADS-B when landing at Paine Field. Turns out the pressure today
  # is 21mb lower, which equates to an additional 573' of altitude reported using the 15C figure!
  # 
  # /A=aaaaaa feet
  # CSE/SPD, i.e. 180/999 (speed in knots, direction is WRT true North), if unknown:  .../...
  # )AIDV#2!4903.50N/07201.75WA
  # ;LEADERVVV*092345z4903.50N/07201.75W>088/036
  # )A19E61!4903.50N/07201.75W' 356/426 /A=29275  # Example Small Airplane Object 
}

close($socket);


