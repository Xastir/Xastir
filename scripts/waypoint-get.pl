#!/usr/bin/perl

# $Id$

# Copyright (C) 2000-2003  Curt Mills, WE7U
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#

# This script will ask for a name, then create a file
# with that name + ".log" in the ~/.xastir/logs directory.  The
# file will contain APRS items created from the downloaded Garmin
# waypoints.  Reading that log file with Xastir will result in one
# item appearing on the map screen for each waypoint, labeled with
# the name of that waypoint.

# This script uses the GPS::Garmin module which can be obtained
# from http://sourceforge.net/projects/perl-gps/


use GPS::Garmin;
use IO::File;


# Flush STDOUT
$| = 1;


# Hard-coded "from" callsign name.  This becomes the "sending"
# callsign for the APRS Items.
$from = "SAR";


# Ask for the name.  This gets changed into the complete log file name.
printf("\n\nEnter name for file: '.log' will get added to the end:\n\n");
$name = <>;
chomp $name;
if (length($name) < 1) {
  die "Must be at least one character!\n";
}

$filename = "~/.xastir/logs/$name.log";

# Get rid of spaces in the filename
$filename =~ s/\s+//g;

# Expand the tilde into the home directory
$filename =~ s{ ^ ~ ( [^/]* ) }
              { $1
                    ? (getpwnam($1))[7]
                    : ( $ENV{HOME} || $ENV{LOGDIR}
                         || (getpwuid($>))[7]
                      )
}ex;

printf("\nThe output filename will be: ($filename)\n\n");
printf("Connect Garmin GPS to /dev/ttyS0 (COM1), set it to Garmin/Garmin mode.\n");
printf("Press <ENTER> to proceed with download, Ctrl-C to abort\n");
<>;


# Create a file to hold the data
$output = IO::File->new("> $filename")
  or die "Couldn't open $filename for writing: $!\n";


$gps = new GPS::Garmin( 'Port' => '/dev/ttyS0',
                       'Baud' => 9600,
                      ) or die "Unable to connect to receiver: $!";


# Transfer waypoints:
$i = 0;
$gps->prepare_transfer("wpt");
while ($gps->records)
{
  ($title,$lat,$lon,$desc) = $gps->grab;

  #printf("%f\t%f\t%s\t%s\n",$lat,$lon,$desc,$title);

  if ($lon < 0) {
    $londeg = sprintf("%d",-$lon);
    $lonmin = (-$lon - $londeg) * 60;
    $lonchar = "W";
  }
  else {
    $londeg = sprintf("%d",$lon);
    $lonchar = "E";
    $lonmin = ($lon - $londeg) * 60;
  }
  if ($lat < 0) {
    $latdeg = sprintf("%d",-$lat);
    $latmin = (-$lat - $latdeg) * 60;
    $latchar = "S";
  }
  else {
    $latdeg = sprintf("%d",$lat);
    $latmin = ($lat - $latdeg) * 60;
    $latchar = "N";
  }

  chomp $title;
  if (length($title) < 1) {
    die "Must be at least one character!\n";
  }
  if (length($title) > 9) {
    $title =~ s/(.{9}).*/\1/;  # Terminate it at 9 chars
  }
  if (length($title) < 3) {
    $title = $title . "   ";    # Pad the title with spaces
    $title =~ s/(.{3}).*/\1/;  # Terminate it at 3 chars
  }

  # We need to enforce the naming restrictions for APRS Items.  '!'
  # and '_' are not allowed, anything else that's printable ASCII is
  # ok.
  @j = split(//, $title);
  for ($k = 0; $k < length($title); $k++) {
  #printf("$k  $j[$k]\n");
    if (     ($j[$k] lt ' ')
          || ($j[$k] gt '~')
          || ($j[$k] eq '!')
          || ($j[$k] eq '_') ) {
      $j[$k] = ' ';
    }
  }
  $title = join("",@j);

  # Write an APRS Item for each waypoint
  printf($output "%s>APRS:)%s!%02d%05.2f%s/%03d%05.2f%s%s\n",
    $from,
    $title,
    $latdeg,
    $latmin,
    $latchar,
    $londeg,
    $lonmin,
    $lonchar,
    "/");

  if (! ($i % 10) ) {
    printf("$i ");
  }

  $i++;
}

printf("\n\nThe data has been saved in: ($filename)\n");
printf("Please open this logfile with Xastir to display the waypoints.\n");
printf("!!!Remember to set your GPS back to NMEA mode for APRS!!!\n");


