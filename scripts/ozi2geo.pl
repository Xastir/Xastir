#!/usr/bin/perl

# Copyright (C) 2001-2004 Curt Mills, WE7U
# Released to the public domain.
# Modified from the earlier inf2geo.pl script.
#
# $Id$
#
# Read in .map file (an OziExplorer file in this case).
# Check the version of the map format (1st line).  My example is
# version 2.2.
# Get the filename from the 2nd line of the file.
# Check that "WGS 84" or "NAD 83" are present in the file (5th
# line?).
# Check the "Map Projection".
# Grab the first four points, which hopefully are the corners.
# Snag the MMPXY lines and the MMPLL lines.  These are the X/Y and
# Lat/Long of the corners in the example I have.
# Write out the .geo file
#
# 2003-08-15 ZL2UMF: add processing multiple files in one go (masks
# and whot-not) so you can convert all your maps in one step.

#use strict; 
use IO::File;


#go through every filename passed to this script
foreach my $file (@ARGV) {
    print "*** $file ***\n";
    
    #make a geo of this file
    makeGeo ($file);
    
}

exit; #just in case





sub makeGeo {
    my $ozimap_filename = shift;
   
    # Snag the filename out of the .map file instead?  Here we
    # create it from the.map filename.
    #
    my $filename = $ozimap_filename;
    $filename =~ s/\.map$//i; 
    
    my $geo_filename = $filename . ".geo";
    
#    $ozimap = IO::File->new("< $ARGV[0].map")
#	or $ozimap = IO::File->new("< $ARGV[0].MAP")
#	or $ozimap = IO::File->new("< $ARGV[0].Map")

    #skip this file if geo already exists
    return print "$filename.geo already exists, will not overwrite\n"   if( -e "$filename.geo" );

    #read the map file
    my $ozimap = IO::File->new ( "< $ozimap_filename" )
	or return print "\nCouldn't open $ozimap_filename for reading:\n$!\n\n";

    #make the new geo file
    $geo = IO::File->new("> $geo_filename")
	or return print "Couldn't open $geo_filename for writing: $!\n";

    # First line should be something like:
    # "OziExplorer Map Data File Version 2.2"
    #
    $tmp = $ozimap->getline();
print "\t$tmp";

    $final_filename = $ozimap->getline();
    chomp($final_filename);
#print $final_filename;

# Check that the filename we just read matches the filename we
# created from the .map filename.

    $tmp = $ozimap->getline();
#print $tmp;

    $tmp = $ozimap->getline();

    # Get the datum
    $tmp = $ozimap->getline();
#print $tmp;

# Check that the datum is "WGS 84", "WGS84", "NAD 83", or "NAD83".
    if ($tmp =~ /WGS 84/i
        || $tmp =~ /WGS84/i
        || $tmp =~ /NAD 83/i
        || $tmp =~ /NAD83/i) {
    }
    else {
      print "***Datum is not WGS84 or NAD83: Results will be inaccurate***\n";
    }

    # Read until we find "Map Projection" at the start of a line.
    #
    $done = 0;
    while (!$done) {
      $tmp = $ozimap->getline();
#print $tmp;
      if ($tmp=~ /Map Projection/) {
        $done++;
      }
    }
    if ($tmp =~ /UTM/i) {
      print "***Found UTM projection: Results will be inaccurate***\n";
#print "$tmp";
    }

# We have the map projection.  Check it.  Issue a warning if it's
# not what we can handle easily.

    # Read until we find "MMPXY" at the start of a line.
    #
    $done = 0;
    while (!$done) {
      $tmp = $ozimap->getline();
      if ($tmp=~ /MMPXY/) {
        $done++;
      }
    }
    $MMPXY1 = $tmp;
#print $MMPXY1;
    $MMPXY2 = $ozimap->getline();
#print $MMPXY2;
    $MMPXY3 = $ozimap->getline();
#print $MMPXY3;
    $MMPXY4 = $ozimap->getline();
#print $MMPXY4;
    $MMPLL1 = $ozimap->getline();
#print $MMPLL1;
    $MMPLL2 = $ozimap->getline();
#print $MMPLL2;
    $MMPLL3 = $ozimap->getline();
#print $MMPLL3;
    $MMPLL4 = $ozimap->getline();
#print $MMPLL4;

# If the corners are always listed in circular order, we can choose
# to use corners 1 and 3 for our tiepoints.  Try this.


    # Read until we find "Map Image Width" inside a line.
    #
    $done = 0;
    while (!$done) {
      $tmp = $ozimap->getline();
      if ($tmp=~ /Map Image Width/) {
        $done++;
      }
    }
#print $tmp;
    $image_size = $tmp;

# That should be all that we need to read from the file.

    # Get the X/Y for the tiepoints
    my ($a, $b, $x0, $y0) = split(',', $MMPXY1);
    chomp($x0);
    chomp($y0);
    # Convert to numbers
    $x0 = $x0 * 1;
    $y0 = $y0 * 1;

#print "$x0 $y0\n";

    my ($a, $b, $x1, $y1) = split(',', $MMPXY3);
    chomp($x1);
    chomp($y1);
    # Convert to numbers
    $x1 = $x1 * 1;
    $y1 = $y1 * 1;

#print "$x1 $y1\n";

    # Get the lat/long for the tiepoints
    my ($a, $b, $tp0_lon, $tp0_lat) = split(',', $MMPLL1);
    chomp($tp0_lon);
    chomp($tp0_lat);

#print "$tp0_lon $tp0_lat\n";

    my ($a, $b, $tp1_lon, $tp1_lat) = split(',', $MMPLL3);
    chomp($tp1_lon);
    chomp($tp1_lat);

#print "$tp1_lon $tp1_lat\n";

    # Split out the imagesize string
    my ($a, $b, $width, $height) = split(',', $image_size);
    chomp($width);
    chomp($height);

    #write to the geo file
    printf $geo "FILENAME    $final_filename\n";
    printf $geo "TIEPOINT    $x0\t\t$y0\t$tp0_lon\t$tp0_lat\n";
    printf $geo "TIEPOINT    $x1\t$y1\t$tp1_lon\t$tp1_lat\n";
    printf $geo "IMAGESIZE   $width\t$height\n";
    printf $geo "#\n# Converted from an OziExplorer .MAP file by WE7U's ozi2geo.pl script\n#\n";

    $ozimap->close();
    $geo->close();
}


