#!/usr/bin/perl -w


#
# Copyright (C) 2002 Paul Lutt, KE7XT.
# Released to the public domain.
#
#
# $Id$
#
#
# Converts between different lat/lon formats.  Will also give UMS
# position if the lat/lon resides somewhere inside the Seattle area
# aeronautical map.
#
# UMS coordinates have been used in the past by King County, WA SAR.
# It can be useful for plotting positions on Green Trails maps and
# perhaps other maps.  The maps must be 15' topo maps and marked in
# tenths of miles along the edge in order to make use of this
# coordinate system.
#
# Web pages which discuss UMS format:
#   http://www.impulse.net/~mlynch/land_nav.html
#   http://www.logicsouth.com/~lcoble/dir9/land_nav.htm
#   http://www.aasar.org/training/academy/navigation.pdf
#


use Coordinate;         # WE7U's Coordinate.pm module


# Create new Coordinate object
my $position = Coordinate->new();


$position->datum("WGS 84");    # Datum


print "\n";
print "Examples:    48.123N     122.123W\n";
print "             48 07.380N  122 07.380W\n";
print "             48 07 22.8N 122 07 22.8W\n";
print "             10U  0565264  5330343\n";


while (1) {

    print "\nEnter a Lat/Long value or UTM value:\n";

    # Snag the input
    $_ = <>;

    # If the first item has 2 digits and one character and there are
    # three "words" in the input, we're starting with a UTM value.
    if (/^\d\d[a-zA-Z]\s+\w+\s+\w+\s*$/) {

        # printf("Found a UTM value\n");

        # We'll convert it to the standard format first and then run
        # through the rest of the code.

        $zone = $_;
        $easting = $_;
        $northing = $_;

        $zone =~ s/^(\d\d[a-zA-Z])\s+\w+\s+\w+\s*$/$1/;
        $easting =~ s/^\d\d[a-zA-Z]\s+(\w+)\s+\w+\s*$/$1/;
        $northing =~ s/^\d\d[a-zA-Z]\s+\w+\s+(\w+)\s*$/$1/;

        if ($easting > 999999) {
            printf("Easting value is too high!\n");
            next;
        }

        $position->zone($zone);
        $position->easting($easting);
        $position->northing($northing);

        # Convert to lat/lon values
        $position->utm_to_lat_lon();

        # printf("Calculated Lat, Long position(Lat, Long):  %f   %f\n",
        #    $position->latitude(),
        #    $position->longitude() );

        $latitude = $position->latitude();
        $longitude = $position->longitude();

        $lat_dir = "N";
        $long_dir = "E";
 
        if ($latitude < 0.0) {
            $latitude = abs($latitude);
            $lat_dir = "S"
        }
        if ($longitude < 0.0) {
            $longitude = abs($longitude);
            $long_dir = "W";
        }

 
        # printf("%f%s %f%s\n", $latitude,$lat_dir,$longitude,$long_dir);

       $_ = sprintf("%f%s %f%s",
            $latitude,$lat_dir,$longitude,$long_dir);

    }

    # Look for lat/long value in the input

    # Check for N/S/E/W characters in the input.  Set the
    # appropriate flags if found.
    $lat_dir = "N";
    $long_dir = "E";
    if (/S/ || /s/) { $lat_dir  = "S"; }
    if (/W/ || /w/) { $long_dir = "W"; }
    # Filter out these characters from the input
    tr/nsewNSEW//d;

    # Convert to DD MM SS format
    ($lat_deg, $lat_min, $lat_sec,
    $long_deg, $long_min, $long_sec) = split(' ');

    # Decimal Degrees?
    if ($lat_deg =~ /\./) {

        $long_deg = $lat_min;   # Save long_degrees in proper place

        $temp = $lat_deg;
        $lat_deg = int $temp;
        $lat_min = int ((abs($temp) * 60.0) % 60);

        # Modulus converts to integers, so we bump up by 10 and then
        # back down.
        $lat_sec = (abs($temp) * 36000.0) % 600;
        $lat_sec = $lat_sec / 10;

        $temp = $long_deg;

        $long_deg = int $temp;
        $long_min = int ((abs($temp) * 60.0) % 60);
        $long_sec = (abs($temp) * 36000.0) % 600;
        $long_sec = $long_sec / 10;
    }
    # Decimal Minutes?
    elsif ($lat_min =~ /\./) {

        $long_min = $long_deg;  # Save long_minutes in proper place
        $long_deg = $lat_sec;   # Save long_degrees in proper place

        $temp = $lat_min;
        $lat_min = int abs($temp);
        $lat_sec = (abs($temp) * 60.0) % 60;
        $lat_sec = $lat_sec / 10;

        $temp = $long_min;
        $long_min = int abs($temp);
        $long_sec = (abs($temp) * 600.0) % 600;
        $long_sec = $long_sec / 10;
    }
    # Decimal Seconds
    else {  # Already in DD MM SS format, don't convert
    }

    # Print out the three lat/long formats
    printf("                Decimal Degrees: %8.5f%s   %8.5f%s\n",
        $lat_deg + ($lat_min/60.0) + ($lat_sec/3600.0), $lat_dir,
        $long_deg + ($long_min/60.0) + ($long_sec/3600.0), $long_dir );
 
    printf("        Degrees/Decimal Minutes: %02d %06.3f%s  %02d %06.3f%s\n",
        $lat_deg, $lat_min + ($lat_sec/60.0), $lat_dir,
        $long_deg, $long_min + ($long_sec/60.0), $long_dir );

    printf("   Degrees/Minutes/Dec. Seconds: %02d %02d %4.1f%s %02d %02d %4.1f%s\n",
        $lat_deg, $lat_min, $lat_sec, $lat_dir,
        $long_deg, $long_min, $long_sec, $long_dir);

    # Fill in the coordinate object with the current lat/lon.
    # Assuming WGS84 datum
    if ($lat_dir =~ /S/) {
        $position->latitude( -( $lat_deg + ($lat_min/60.0) + ($lat_sec/3600.0) ) );
    }
    else {
        $position->latitude( $lat_deg + ($lat_min/60.0) + ($lat_sec/3600.0) );
    }
 
    if ($long_dir =~ /W/) {
        $position->longitude( -( $long_deg + ($long_min/60.0) + ($long_sec/3600.0) ) );
    }
    else {
        $position->longitude( $long_deg + ($long_min/60.0) + ($long_sec/3600.0) );
    }

    #printf("%f %f\n",$position->latitude,$position->longitude);

    $position->lat_lon_to_utm();
    printf("  Universal Transverse Mercator: %s  %07.0f  %07.0f\n",
        $position->zone(),
        $position->easting(),
        $position->northing() );

    # Check whether the coordinates are within the SEA aeronautical
    # map area
    $lat_err = 0;
    if ($lat_dir =~ /S/ ||
        $lat_deg < 44 || $lat_deg > 49 ||
	    ($lat_deg == 44 && ($lat_min < 30 || ($lat_min == 30 && $lat_sec == 0))) ||
	    ($lat_deg == 49 && ($lat_min > 0 || $lat_sec > 0))) {
	print " lat. out of range ";
	$lat_err = 1;
    }

    $long_err = 0;
    if ($long_dir =~ /E/ ||
        $long_deg < 117 || $long_deg > 125 ||
	    ($long_deg == 117 && ($long_min == 0 && $long_sec == 0)) ||
	    ($long_deg == 125 && ($long_min > 0 || $long_sec > 0))) {
	print " long. out of range";
        $long_err = 1;
    }

    next if ( $lat_err || $long_err);


    # Compute UMS coordinates
    $y_sec = 3600 * ($lat_deg - 44) + 60 * $lat_min + $lat_sec;
    $y_sec = 18000 - $y_sec;

    $x_sec = 3600 * ($long_deg - 117) + 60 * $long_min + $long_sec;
    $x_sec = 28800 - $x_sec;

    $quad = 32 * int($y_sec / 900) + int($x_sec / 900) + 1;

#    print "\tx_sec= $x_sec, y_sec= $y_sec, quad= $quad\n";

    $y_subquad_offset = int($y_sec / 450);
    $x_subquad_offset = int($x_sec / 450);

    if (&even($x_subquad_offset) && &even($y_subquad_offset)) {
        print "        UMS (Green Trails Maps): SEA ${quad} A ";
	printf "%02d", &s2m_x($x_sec - (450 * $x_subquad_offset));
	printf "%02d\n", &s2m_y($y_sec - (450 * $y_subquad_offset));
    } elsif (&odd($x_subquad_offset) && &even($y_subquad_offset)) {
        print "        UMS (Green Trails Maps): SEA ${quad} B ";
	printf "%02d", &s2m_x(450 * ($x_subquad_offset + 1) - $x_sec);
	printf "%02d\n", &s2m_y($y_sec - (450 * $y_subquad_offset));
    } elsif (&even($x_subquad_offset) && &odd($y_subquad_offset)) {
        print "        UMS (Green Trails Maps): SEA ${quad} C ";
	printf "%02d", &s2m_x($x_sec - (450 * $x_subquad_offset));
	printf "%02d\n", &s2m_y(450 * ($y_subquad_offset + 1) - $y_sec);
    } else {
        print "        UMS (Green Trails Maps): SEA ${quad} D ";
	printf "%02d", &s2m_x(450 * ($x_subquad_offset + 1) - $x_sec);
	printf "%02d\n", &s2m_y(450 * ($y_subquad_offset + 1) - $y_sec);
    }
}

sub even {
    return (($_[0] & 1) == 0);
}

sub odd {
    return (($_[0] & 1) == 1);
}

sub s2m_y {
    return (int((0.1917966 * $_[0]) + 0.5));
}

sub s2m_x {
    return (int((cos(($lat_deg + ($lat_min / 60.0) + ($lat_sec / 3600.0)) / 57.29578) * (0.1917966 * $_[0])) + 0.5));
}


