#!/usr/bin/env perl
use warnings;

# Enable various functions below.
$start_xastir           = 0;
$enable_planes          = 1;  # Requires 1 SDR dongle
$enable_plane_logging   = 0;  # Can fill up hard drive
$enable_plane_alternate = 0;  # Must also set $enable_planes. Requires another SDR dongle
$enable_ships           = 0;  # Requires 1 SDR dongle
$enable_ship_long_range = 0;  # Requires 1 SDR dongle

# Set RTL-SDR device numbers below (Starts at device 0). Set the numbers
# based on how the SDR dongles get enumerated and how your antennas are
# connected. You'll need a separate RTL-SDR dongle for each.
$plane1090_SDR       = 0;  # Used with "enable_planes" above
$plane978_SDR        = 1;  # Used with "enable_plane_alternate" above
$ship_SDR            = 2;  # Used with "enable_ships" above
$ship_long_range_SDR = 3;  # Used with "enable_ship_long_range" above

# Paths to executables: Set to match where things reside on your system.
$XTERM    = "/usr/bin/xterm";
$DUMP1090 = "~/src/dump1090/mutability/dump1090";
$ADSBPL   = "/usr/local/share/xastir/scripts/ads-b.pl";
$RTLSDR   = "/usr/local/bin/rtl_sdr";
$DUMP978  = "~/src/dump978/dump978";
$UAT2ESNT = "~/src/dump978/uat2esnt";
$NC       = "/usr/bin/nc";
$XASTIR   = "/usr/local/bin/xastir";
$RTLAIS   = "~/src/rtl-ais/rtl-ais/rtl_ais";
$AISPL    = "/usr/local/share/xastir/scripts/ais.pl";

#----------------------------------------------------

if ($start_xastir == 1) {
  system("$XASTIR &");
  sleep(5);
}

if ($enable_planes == 1) {
  # SDR on main ADS-B frequency/protocol of 1.09 GHz:
  system("$XTERM -T ADS-B -e $DUMP1090 --interactive --net --interactive-ttl 86400 --net-sbs-port 30003 --phase-enhance --oversample --fix --ppm -1.2 --gain -10 --device-index $plane1090_SDR &");

  if ($enable_plane_alternate == 1) {
    sleep(3); # Wait after starting dump1090 before dump978 connects to it

    # SDR on alternate ADS-B frequency of 978 MHz (and alternate ADS-B protocol):
    system("$XTERM -T ADS-B-Alternate -e $RTLSDR -f 978000000 -s 2083334 -g 0 -d $plane978_SDR - | $DUMP978 | $UAT2ESNT | $NC -q1 localhost 30001 &");
  }

  sleep(3); # Give time to start programs above before Perl script connects

  if ($enable_plane_logging) {
    system("$XTERM -T ads-b.pl -e $ADSBPL planes 10163 --logging &");
  }
  else {
    system("$XTERM -T ads-b.pl -e $ADSBPL planes 10163 &");
  }
}

if ( ($enable_ships == 1) || ($enable_ship_long_range == 1) ) {

  if ($enable_ships == 1) {
    # SDR receiving the two normal AIS frequencies:
    system("$XTERM -T AIS -e $RTLAIS -h 127.0.0.1 -P 10110 -d $ship_SDR -l 161.975M -r 162.025M -n -p -2 &");
  }

  if ($enable_ship_long_range == 1) {
    # SDR receiving the two long-range AIS frequencies:
    system("$XTERM -T AISLongRange -e $RTLAIS -h 127.0.0.1 -P 10110 -d $ship_long_range_SDR -l 156.775M -r 156.825M -n -p -2 &");
  }

  sleep(3); # Give time to start programs above before Perl script connects

  system("$XTERM -T ais.pl -e $AISPL boats 9209 &");
}


