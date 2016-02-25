#!/usr/bin/perl -W
#
# Convert "dump1090" telnet port output to Xastir UDP input.  This script will
# parse packets containing lat/long, turn them into APRS-like packets, then use
# "xastir_udp_client" to inject them into Xastir.
#
# Invoke it as:
#   ./ads-b.pl <callsign> <passcode>
#
# I got the "dump1090" program from here originally:
#       https://github.com/antirez/dump1090
# but the author says the version here by another author is more feature-complete:
#       https://github.com/MalcolmRobb/dump1090
#
# Invoke the "dump1090" program like so:
#   "./dump1090 --interactive --net --aggressive"
# or
#   "./dump1090 --net --aggressive"
#
# Then invoke this script in another xterm:
#   "./ads-b.pl <callsign> <passcode>"
#
# It will receive packets from port 30003 of "dump1090", parse them, then inject
# APRS packets into Xastir's UDP port (2023) if "Server Ports" are enabled in Xastir.
#
# A good addition for later: Timestamp of last update.
# If too old when new message comes in, delete old data and start over.
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
#   NOTE: I changed the numbers by -1 to fit Perl's "split()" command's field numbering.
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
# Example packets to parse:
# MSG,3,,,ABB2D5,,,,,,,40975,,,47.68648,-122.67834,,,0,0,0,0    # Lat/long/altitude (ft)
# MSG,1,,,A2CB32,,,,,,BOE181  ,,,,,,,,0,0,0,0   # Tail number or flight number
# MSG,4,,,A0F4F6,,,,,,,,175,152,,,-1152,,0,0,0,0    # Ground speed (knots)/Track
#
#
# Note: There's also "dump978" which listens to 978 MHz ADS-B transmissions. You can
# invoke "dump1090" as above, then invoke "dump978" like this:
#       rtl_sdr -f 978000000 -s 2083334 -g 48 -d 1 - | ./dump978 | ./uat2esnt | nc -q1 localhost 30001
# which will convert the 978 MHz packets into ADS-B ES packets and inject them into "dump1090"
# for decoding. I haven't determined yet whether those packets will come out on "dump1090"'s
# port 30003 (which this script uses). The above command also uses RTL device 1 instead of 0.
# If you're only interested in 978 MHz decoding, there's a way to start "dump1090" w/o a
# device attached, then start "dump978" and connect it to "dump1090".
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

$dump1090_host = "localhost"; # Server where dump1090 is running
$dump1090_port = 30003;     # 30003 is dump1090 default port

$xastir_host = "localhost"; # Server where Xastir is running
$xastir_port = 2023;        # 2023 is Xastir default UDP port

$xastir_user = shift;
chomp $xastir_user;
if ($xastir_user eq "") {
  print "Please enter a callsign for Xastir injection\n";
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
    $lon{$plane_id} = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    if ( ($oldlat ne $lat{$plane_id}) || ($oldlon ne $lon{$plane_id}) ) {
      $print5 = sprintf("%8s,%9s", $lat{$plane_id},$lon{$plane_id});
      $newdata{$plane_id}++;
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

    $tail{$plane_id} = $fields[10];

    if ($old ne $tail{$plane_id}) {
      $print6 = sprintf("Tactical=%-9s", $fields[10]);
      $newdata{$plane_id}++;
 
      # Assign tactical call = tail number or flight number
      $aprs = $xastir_user . '>' . "APRS::TACTICAL :" . $plane_id . "=" . $fields[10];
      print("$print1\t\t$print2  $print3  $print4  $print6  $aprs\n");
 
      # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
      system("/usr/local/bin/xastir_udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\" ");
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

    # Do we have a lat/lon?
    if ( defined($lat{$plane_id})
         && defined($lon{$plane_id}) ) {
      # Yes, we have a lat/lon
      $aprs="$xastir_user>APRS:)$plane_id!$lat{$plane_id}/$lon{$plane_id}$symbol$newtrack/$newspeed$newalt$newtail$emerg_txt$squawk_txt$onGroundTxt";
      print("$print1\t\t$print2  $print3  $print4  $print5  $aprs\n");

      # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
      system("/usr/local/bin/xastir_udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\" ");
    }
    else {
      # No, we have no lat/lon.
      # Send out packet with position ambiguity since we don't know
      # the lat/long, but it is definitely within receive range.
#      $aprs="$xastir_user>APRS:)$plane_id!$my_lat/$my_lon$symbol$newtrack/$newspeed$newalt$newtail";
#      print "\t\t\t\t\t\t\t\t$aprs\n";

      # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
#      system("/usr/local/bin/xastir_udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\" ");

      print("$print1\t\t$print2  $print3  $print4$emerg_txt$squawk_txt$onGroundTxt\n");
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


