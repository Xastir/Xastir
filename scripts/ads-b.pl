#!/usr/bin/perl -W
#
# Convert "dump1090" telnet port output to Xastir UDP input.  This script will
# parse packets containing lat/long, turn them into APRS-like packets, then use
# "xastir_udp_client" to inject them into Xastir.
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
#   "./ads-b.pl"
#
# It will receive packets from port 30003 of "dump1090", parse them, then inject
# APRS packets into Xastir's UDP port (2023) if "Server Ports" are enabled in Xastir.
#
# A good addition for later: Timestamp of last update.
# If too old when new message comes in, delete old data and start over.
#
# "dump1090" output format is here:
#   http://woodair.net/SBS/Article/Barebones42_Socket_Data.htm
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


eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
& eval 'exec perl -S $0 $argv:q'
if 0;

use IO::Socket;


$xastir_user = "****";      # Callsign (in all CAPS) for Xastir injection & packet FROM
$xastir_pass = 1234;        # Numeric password to go with above callsign

$dump1090_host = "localhost"; # Server where dump1090 is running
$dump1090_port = 30003;     # 30003 is dump1090 default port

$xastir_host = "localhost"; # Server where Xastir is running
$xastir_port = 2023;        # 2023 is Xastir default UDP port


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
  #$_ = "MSG,3,,,A0CF8D,,,,,,,28000,,,47.87670,-122.27269,,,0,0,0,0";

  chomp;


  # Sentences have either 10 or 22 fields.
  @fields = split(",");


  # Parse altitude if MSG Type 2, 3, 5, 6, or 7, save in hash.
  if (    $fields[1] == 2
       || $fields[1] == 3
       || $fields[1] == 5
       || $fields[1] == 6
       || $fields[1] == 7 ) {

    if ( $fields[11] ne ""
         && $fields[11] ne 0 ) {
      $altitude{$fields[4]} = sprintf("%06d", $fields[11]);
      print "$fields[4]\t\t$fields[11]ft\n";
      $newdata{$fields[4]}++;
    }
  }


  # Parse ground speed and track if MSG Type 2 or 4, save in hash.
  if ( $fields[1] ==  4 || $fields[1] == 2 ) {

    if ( $fields[12] ne "" ) {
      $groundspeed{$fields[4]} = $fields[12];
      print "$fields[4]\t\t\t$fields[12]kn\n";
      $newdata{$fields[4]}++;
    }

    if ( $fields[13] ne "" ) {
      $track{$fields[4]} = $fields[13];
      print "$fields[4]\t\t\t\t$fields[13]°\n";
      $newdata{$fields[4]}++;
    }
  }


  # Parse lat/long if MSG Type 2 or 3, save in hash.
  if (  ( $fields[1] == 2 || $fields[1] == 3 )
       && $fields[14] ne ""
       && $fields[15] ne "" ) {

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
    $lat{$fields[4]} = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

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
    $lon{$fields[4]} = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    print "$fields[4]\t\t\t\t\t$lat{$fields[4]} / $lon{$fields[4]}\n";
    $newdata{$fields[4]}++;
  }


  # Save tail or flight number in hash if MSG Type 1 or "ID" sentence.
  if (    ($fields[0] eq "ID" || $fields[1] ==  1)
       && $fields[10] ne "????????"
       && $fields[10] ne "" ) {
 
    $tail{$fields[4]} = $fields[10];
    print "$fields[4]\t\t\t\t\t$fields[10]\n";
    $newdata{$fields[4]}++;
  }


  # Above we parsed some message that changed some of our data, send out the
  # new APRS string if we have enough data defined.
  #
  if ( defined($newdata{$fields[4]})
       && $newdata{$fields[4]}
       && defined($lat{$fields[4]})
       && defined($lon{$fields[4]}) ) {

    $newtrack = "000";
    if ( defined($track{$fields[4]}) ) {
      $newtrack = sprintf("%03d", $track{$fields[4]} );
    }

    $newspeed = "000";
    if ( defined ($groundspeed{$fields[4]}) ) {
      $newspeed = sprintf("%03d", $groundspeed{$fields[4]} );
    }

    $newtail = "";
    if ( defined($tail{$fields[4]}) ) {
      $newtail = " $tail{$fields[4]}";
    }

    $newalt = "";
    if ( defined($altitude{$fields[4]}) ) {
      $newalt = " /A=$altitude{$fields[4]}";
    }

    $aprs="$xastir_user>APRS:)$fields[4]!$lat{$fields[4]}/$lon{$fields[4]}'$newtrack/$newspeed$newalt$newtail";
    print "\t\t\t\t\t\t\t\t$aprs\n";

    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    system("/usr/local/bin/xastir_udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\" ");

    $newdata{$fields[4]} = 0;
  }

  # Create Object or Item APRS packet
  # Symbol:   Small airplane = /'
  #           Helicopter = /X 
  #           Large aircraft = /^
  #           Aircraft = \^
  #
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


