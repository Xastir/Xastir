#!/usr/bin/perl

# Copyright (C) 2001-2004 Curt Mills, WE7U
# Released to the public domain.
#
# $Id$
#
#
# Invoke this script against one or more info files by typing:
#
#       inf2geo.pl filename.inf
# -or-
#       inf2geo.pl *.inf
#
# To process all .inf files in that directory.
#
#
# Read in .inf file (from Ui-View)
# Convert the lat/long coordinates into dd.dddd format
# Get the image extents via "identify -ping filename"
# Write out the .geo file
# Note that this program assumes (and converts to)
# lower-case for the filename.
#
# Note:  It appears that .INF files store the lat/lon
# in DD.MM.MMMM format.  Converting the script to this
# format.
#
# 2003-08-15 ZL2UMF: add processing multiple files in one go (masks and whot-not)
#	    so you can convert all your ui-view maps in one step

#use strict; 
use IO::File;

#go through every filename passed to this script

#printf("ARGV:%d\n",$#ARGV);

if ($#ARGV == -1) {
    # No filenames on the command line
    printf("\n\n\nNo filenames specified.  Invoke this script against one\n");
    printf("or more .inf files by typing:\n\n");
    printf("        inf2geo.pl filename.inf\n");
    printf("-or-\n");
    printf("        inf2geo.pl *.inf\n\n");
    printf("To process all .inf files in that directory.\n\n");
    exit;
}
else {
    foreach my $file (@ARGV) {
        print "*** $file ***\n";
    
        #make a geo of this file
        makeGeo ($file);
    }
    
}

exit; #just in case

sub makeGeo {
    my $inf_filename = shift;
    
    my $filename = $inf_filename;
    $filename =~ s/\.inf$//i; 
    
    my $geo_filename = $filename . ".geo";
    
#    $inf = IO::File->new("< $ARGV[0].inf")
#	or $inf = IO::File->new("< $ARGV[0].INF")
#	or $inf = IO::File->new("< $ARGV[0].Inf")

    #skip this file if geo already exists
    return print "$filename.geo already exists, will not overwrite\n"   if( -e "$filename.geo" );

    #read the inf file
    my $inf = IO::File->new ( "< $inf_filename" )
	or return print "\nCouldn't open $inf_filename for reading:\n$!\n\n";

    #make the new geo file
    $geo = IO::File->new("> $geo_filename")
	or return print "Couldn't open $geo_filename for writing: $!\n";

    $upper_left = $inf->getline();
    my ($tp0_lon, $tp0_lat) = split(',', $upper_left);
    chomp($tp0_lat);

    # Reverse them
    if ( ($tp0_lat =~ /E/) || ($tp0_lat =~ /W/) )  {
	$temp = $tp0_lat;
	$tp0_lat = $tp0_lon;
	$tp0_lon = $temp;
    }

    $lower_right = $inf->getline();
    ($tp1_lon, $tp1_lat) = split(',', $lower_right);
    chomp($tp1_lat);

    # Reverse them
    if ( ($tp1_lat =~ /E/) || ($tp1_lat =~ /W/) ) {
	$temp = $tp1_lat;
	$tp1_lat = $tp1_lon;
	$tp1_lon = $temp;
    }

    #do some maths
    $tp0_lat2 = &convert($tp0_lat) or return;
    $tp0_lon2 = &convert($tp0_lon) or return;
    $tp1_lat2 = &convert($tp1_lat) or return;
    $tp1_lon2 = &convert($tp1_lon) or return;

    my ($final_filename, $string) = &findImageFile($filename) or return;


    # The format returned by string changed from this:
    # test.gif 1148x830+0+0 PseudoClass 256c 48kb GIF 1s
    # to this:
    # test.gif GIF 1148x830+0+0 PseudoClass 256c Palette 8-bit 48kb 0.4u 0:01
    # in later versions of ImageMagick.
    #

    chomp($string);
    $string =~ s/.*\s(\d+x\d+).*/$1/;    # Grab the 1148x830 portion

    #print "String: $string\n";

    $x = $y = $string;

    $x =~ s/(\d+)x\d+/$1/;
    $y =~ s/\d+x(\d+)/$1/;

    #print "X: $x\nY: $y\n";

    $x1 = $x - 1;    # We start numbering pixels at zero, not 1
    $y1 = $y - 1;    # We start numbering pixels at zero, not 1

    #print "X: $x\nY: $y\n";
    #print "X1: $x1\nY1: $y1\n";
    
    
    #write to the geo file
    printf $geo "FILENAME    $final_filename\n";
    printf $geo "TIEPOINT    0\t\t0\t$tp0_lon2\t$tp0_lat2\n";
    printf $geo "TIEPOINT    $x1\t$y1\t$tp1_lon2\t$tp1_lat2\n";
    printf $geo "IMAGESIZE   $x\t$y\n";
    printf $geo "#$string\n";
    printf $geo "#\n# Converted from a .INF file by WE7U's inf2geo.pl script\n#\n";


    $inf->close();
    $geo->close();

}




sub convert
{
    #print "$_[0] -> ";
    ($dd,$mm,$mm2) = split('\.', $_[0]);
    $mm2 =~ s/(\d+).*/$1/;
    $mm = $mm . "\." . $mm2;
    $number = $dd + ($mm / 60.0);

    if ( ($_[0] =~ /S/) || ($_[0] =~ /s/)
      || ($_[0] =~ /W/) || ($_[0] =~ /w/) )
    {
        $number = -$number;
    }

    # Latitude bound checking
    if ( ($_[0] =~ /S/) || ($_[0] =~ /s/) || ($_[0] =~ /N/) || ($_[0] =~ /n/) ) {
        if ($dd > 90) {
            print "Latitude degrees out-of-bounds: $dd.  Must be <= 90\n";
	    return;
        }
        if ($mm >= 60) {
            print "Latitude minutes out-of-bounds: $mm.  Must be < 60\n";
	    return;
        }
        if (abs($number) > 90.0) {
            print "Latitude out-of-bounds: $number.  Must be between -90 and +90\n";
	    return;
        }
    }
    # Longitude bounds checking
    else {
        if ($dd > 180) {
            print "Longitude degrees out-of-bounds: $dd.  Must be <= 180\n";
	    return;
        }
        if ($mm >= 60) {
            print "Longitude minutes out-of-bounds: $mm.  Must be < 60\n";
	    return;
        }
        if (abs($number) > 180.0) {
            print "Longitude out-of-bounds: $number.  Must be between -180 and +180\n";
	    return;
        }
    }
 
    #print "$number\n";
    #print "Temp = $temp\n";
    return($number);
}



sub findImageFile {
    $filename = shift;
    @extensions = ("gif", "bmp", "jpg", "png", "emf");
    foreach $xtn (@extensions) {
	$try_filename = "$filename.$xtn";
#	print "Looking for $try_filename\n";
	$string = `identify -ping $try_filename 2>/dev/null`;
	if ($string ne "") {
	    $filename = $try_filename;
	    $image_size = $string;
	}
    }
    if ($image_size eq "") {
	print "Image file not found for $filename, maybe be a case problem\n" ;
	return;
    }
    
    print "Found this image: $image_size\n";
    return ($filename, $image_size);
}


