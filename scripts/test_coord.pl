#!/usr/bin/perl -w


# test_coord.pl:  Perl code to test out the Coordinate.pm
# module.
#
# Copyright (C) 2000-2002  Curt Mills, WE7U
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#


#
# $Id$
#
#
# TODO:
# -----
#


#------------------------------------------------------------------------------------------------

use lib "/usr/local/lib";
use Coordinate;		# Snag WE7U's Coordinate module


&test();


#
# Routine for testing out the module.
#
# Need to add in tests for each method and object type.
#
sub test
{
  my $to_WGS84 = 0;
  my $from_WGS84 = 1;

  my $position = Coordinate->new();


  $position->latitude(48.125);
  $position->longitude(-122.500);
  $position->datum("NAD27 CONUS MEAN:W of Mississippi/Except Louisiana/Minnesota/Missouri"); 	# Datum


  printf("Starting position(Lat, Long):  %s   %s\n",
	$position->latitude(),
	$position->longitude() );

  $position->degrees_minutes_seconds();		# Convert to DD MM SS format
  printf("Starting position(Lat, Long):  %s   %s\n",
	$position->formatted_latitude(),
	$position->formatted_longitude() );

  $position->lat_lon_to_utm();
  printf("Calculated UTM position(Easting, Northing, Zone):  %f   %f   %s\n",
	$position->easting(),
	$position->northing(),
	$position->zone() );
	
  $position->utm_to_lat_lon();
  printf("Calculated Lat, Long position(Lat, Long):  %f   %f\n",
	$position->latitude(),
	$position->longitude() );


  print "Changing from NAD27 to WGS84 datum...\n";
  $position = $position->datum_shift_to_wgs84();
  printf("Calculated Lat, Long position(Lat, Long):  %f   %f\n",
        $position->latitude(),
        $position->longitude() );

  $position->degrees_minutes_seconds();		# Convert to DD MM SS
  printf("Calculated Lat, Long position(Lat, Long):  %s   %s\n",
	$position->formatted_latitude(),
	$position->formatted_longitude() );

  print "Changing from WGS84 to NAD27 datum...\n";
  $position = $position->datum_shift_from_wgs84_to( "NAD27 CONUS MEAN:W of Mississippi/Except Louisiana/Minnesota/Missouri" );
  printf("Calculated Lat, Long position(Lat, Long):  %f   %f\n",
        $position->latitude(),
        $position->longitude() );

  print "\n0\n";
  my $temp = CoordinateFormat->new( "0" );
  printf("        decimal_degrees: %s\n",   $temp->decimal_degrees( ) );
  printf("        degrees_minutes: %s\n",   $temp->degrees_minutes( ) );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds() );

  print "180\n";
  $temp->raw( "180" );
  printf("        decimal_degrees: %s\n",   $temp->decimal_degrees( "180") );
  printf("        degrees_minutes: %s\n",   $temp->degrees_minutes( "180") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("180") );

  print "180 30\n";
  $temp->raw( "180 30" );
  printf("        decimal_degrees: %s\n",   $temp->decimal_degrees( "180 30") );
  printf("        degrees_minutes: %s\n",   $temp->degrees_minutes( "180 30") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("180 30") );

  print "180.50\n";
  $temp->raw( "180.50" );
  printf("        decimal_degrees: %s\n",   $temp->decimal_degrees( "180.50") );
  printf("        degrees_minutes: %s\n",   $temp->degrees_minutes( "180.50") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("180.50") );

  $temp->raw( "180 30.50" );
  print "180 30.50\n";
  printf("        decimal_degrees: %s\n",   $temp->decimal_degrees( "180 30.50") );
  printf("        degrees_minutes: %s\n",   $temp->degrees_minutes( "180 30.50") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("180 30.50") );

  $temp->raw( "180 30 30" );
  print "180 30 30\n";
  printf("        decimal_degrees: %s\n",   $temp->decimal_degrees( "180 30 30") );
  printf("        degrees_minutes: %s\n",   $temp->degrees_minutes( "180 30 30") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("180 30 30") );

  $temp->raw( "180 30 30.5" );
  print "180 30 30.5\n";
  printf("        decimal_degrees: %s\n", $temp->decimal_degrees("180 30 30.5") );
  printf("        degrees_minutes: %s\n", $temp->degrees_minutes("180 30 30.5") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("180 30 30.5") );

  $temp->raw( "-180 30 30.5" );
  print "-180 30 30.5\n";
  printf("        decimal_degrees: %s\n", $temp->decimal_degrees("-180 30 30.5") );
  printf("        degrees_minutes: %s\n", $temp->degrees_minutes("-180 30 30.5") );
  printf("degrees_minutes_seconds: %s\n\n", $temp->degrees_minutes_seconds("-180 30 30.5") );

  EllipsoidTable->enumerate();
  DatumTable->enumerate();
}


