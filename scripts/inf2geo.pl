#!/usr/bin/perl

# Copyright (C) 2001 Curt Mills, WE7U
# Released to the public domain.
#
# $Id$
#
# Read in .inf file (from Ui-View)
# Convert the lat/long coordinates into dd.dddd format
# Get the image extents via "identify -ping filename"
# Write out the .geo file
# Note that this program assumes (and converts to)
# lower-case for the filename.  It also assumes .gif
# files in all cases.
#
# Note:  It appears that .INF files store the lat/lon
# in DD.MM.MMMM format.  Converting the script to this
# format.


use IO::File;


$inf = IO::File->new("< $ARGV[0].inf")
  or $inf = IO::File->new("< $ARGV[0].INF")
  or $inf = IO::File->new("< $ARGV[0].Inf")
    or die "\nCouldn't open $ARGV[0].inf, $ARGV[0].INF, or $ARGV[0].Inf for reading:\n$!\n\n";

$geo = IO::File->new("> $ARGV[0].geo")
    or die "Couldn't open $ARGV[0].geo for writing: $!\n";

$upper_left = $inf->getline();
($tp0_lon, $tp0_lat) = split(',', $upper_left);
chomp($tp0_lat);

if ( ($tp0_lat =~ /E/) || ($tp0_lat =~ /W/) )   # Reverse them
{
  $temp = $tp0_lat;
  $tp0_lat = $tp0_lon;
  $tp0_lon = $temp;
}

$lower_right = $inf->getline();
($tp1_lon, $tp1_lat) = split(',', $lower_right);
chomp($tp1_lat);

if ( ($tp1_lat =~ /E/) || ($tp1_lat =~ /W/) )   # Reverse them
{
  $temp = $tp1_lat;
  $tp1_lat = $tp1_lon;
  $tp1_lon = $temp;
}

$filename = $ARGV[0];
#$filename =~ tr/A-Z/a-z/;
$filename1 = $filename . '.GIF';
$filename2 = $filename . '.Gif';
$filename  = $filename . '.gif';

$tp0_lat2 = &convert($tp0_lat);
$tp0_lon2 = &convert($tp0_lon);
$tp1_lat2 = &convert($tp1_lat);
$tp1_lon2 = &convert($tp1_lon);

$final_filename = $filename;
$string = `identify -ping $filename 2>/dev/null`;
if ($string eq "")
{
    $final_filename = $filename1;
    $string = `identify -ping $filename1 2>/dev/null`;
}
if ($string eq "")
{
    $final_filename = $filename2;
    $string = `identify -ping $filename2 2>/dev/null`;
}

if ($string eq "")
{
    print "\nCouldn't run 'identify -ping' on the file\n";
    print "Perhaps $filename, $filename1, or $filename2 couldn't be found.\n";
    die "\n";
}

print $string;

# The format returned by string changed from this:
# test.gif 1148x830+0+0 PseudoClass 256c 48kb GIF 1s
# to this:
# test.gif GIF 1148x830+0+0 PseudoClass 256c Palette 8-bit 48kb 0.4u 0:01
# in later versions of ImageMagick.

chomp($string);
$string =~ s/^.*\s(\d+x\d+)\+\d+\+\d+\s+.*/$1/;    # Grab the 1148x830 portion
$x = $y = $string;
$x =~ s/^(\d+)x.*/$1/;
$y =~ s/^\d+x(\d+).*/$1/;

$x1 = $x - 1;    # We start numbering pixels at zero, not 1
$y1 = $y - 1;    # We start numbering pixels at zero, not 1

printf $geo "FILENAME    $final_filename\n";
printf $geo "TIEPOINT    0\t\t0\t$tp0_lon2\t$tp0_lat2\n";
printf $geo "TIEPOINT    $x1\t$y1\t$tp1_lon2\t$tp1_lat2\n";
printf $geo "IMAGESIZE   $x\t$y\n";
printf $geo "#\n# Converted from a .INF file by WE7U's inf2geo.pl script\n#\n";

$inf->close();
$geo->close();





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
            die "Latitude degrees out-of-bounds: $dd.  Must be <= 90\n";
        }
        if ($mm >= 60) {
            die "Latitude minutes out-of-bounds: $mm.  Must be < 60\n";
        }
        if (abs($number) > 90.0) {
            die "Latitude out-of-bounds: $number.  Must be between -90 and +90\n";
        }
    }
    # Longitude bounds checking
    else {
        if ($dd > 180) {
            die "Longitude degrees out-of-bounds: $dd.  Must be <= 180\n";
        }
        if ($mm >= 60) {
            die "Longitude minutes out-of-bounds: $mm.  Must be < 60\n";
        }
        if (abs($number) > 180.0) {
            die "Longitude out-of-bounds: $number.  Must be between -180 and +180\n";
        }
    }
 
    #print "$number\n";
    #print "Temp = $temp\n";
    return($number);
}

