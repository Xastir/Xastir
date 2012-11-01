#!/usr/bin/perl

# $Id$

# Copyright (C) 2000-2012  Curt Mills, WE7U
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

# This script will ask for an item designator, then create a file
# with that name + ".log" in the ~/.xastir/logs directory.  The
# file will contain APRS items created from the downloaded Garmin
# tracklog.  Reading that log file with Xastir will result in an
# object with a track being displayed on the map screen.

# You may wish to bump up MAX_TRACKS in db.h to 1024 in order to see
# the entire log, but be careful with Xastir's memory image growing
# too fast if you're connected to the 'net after making this change.

# This script uses the GPS::Garmin module which can be obtained
# from http://sourceforge.net/projects/perl-gps/


use GPS::Garmin;
use IO::File;


# Flush STDOUT
$| = 1;


# Hard-coded "from" callsign name.  This becomes the "sending"
# callsign for the APRS Items.
$from = "SAR";


# Ask for the item name.  This will turn into the 3-9 character APRS
# item name.
printf("\n\nEnter item designator: Length 1-9, all except '!' and '_' allowed, case respected:\n\n");
printf("\t123456789\n\t");
$name = <>;
chomp $name;
if (length($name) < 1) {
  die "Must be at least one character!\n";
}
if (length($name) > 9) {
  $name =~ s/(.{9}).*/\1/;  # Terminate it at 9 chars
}
if (length($name) < 3) {
  $name = $name . "   ";    # Pad the name with spaces
  $name =~ s/(.{3}).*/\1/;  # Terminate it at 3 chars
}

# We need to enforce the naming restrictions for APRS Items.  '!'
# and '_' are not allowed, anything else that's printable ASCII is
# ok.
@j = split(//, $name);
for ($i = 0; $i < length($name); $i++) {
#printf("$i  $j[$i]\n");
  if (     ($j[$i] lt ' ')
        || ($j[$i] gt '~')
        || ($j[$i] eq '!')
        || ($j[$i] eq '_') ) {
    $j[$i] = ' ';
  }
}
$name = join("",@j);

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

printf("\nThe item designator will be: ($name)\n");
printf("The output filename will be: ($filename)\n\n");
printf("Connect Garmin GPS to /dev/ttyS0 (COM1), set it to Garmin/Garmin mode.\n");
printf("Press <ENTER> to proceed with download, Ctrl-C to abort\n");
<>;


# Create a file to hold the data
$output = IO::File->new("> $filename")
  or die "Couldn't open $filename for writing: $!\n";


$gps = new GPS::Garmin( 'Port' => '/dev/ttyS0',
                       'Baud' => 9600,
                      ) or die "Unable to connect to receiver: $!";


# Transfer trackpoints:
$i = 0;
$gps->prepare_transfer("trk");
while ($gps->records)
{
  ($lat,$lon,$time) = $gps->grab;
#  printf("%f %f %d\n",$lat,$lon,$time);
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

  # Write an APRS Item for each trackpoint
  printf($output "%s>APRS:)%s!%02d%05.2f%s/%03d%05.2f%s%s\n",
    $from,
    $name,
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
printf("Please open this logfile with Xastir to display the track\n");
printf("!!!Remember to set your GPS back to NMEA mode for APRS!!!\n");


