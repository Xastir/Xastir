#!/usr/bin/perl
# $Id$

# XASTIR .geo file generator for mapblast pixel maps    16.10.2001
#  Copyright (C) 2001 Rolf Bleher              http://www.dk7in.de

#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#  see file COPYING for details

#--------------------------------------------------------------------------

use POSIX;                      # provides acos() function
                                            
# some default values:
$width  = 1280;                 # seems to be the maximum supported by mapblast
$height = 1024;                 # seems to be the maximum supported by mapblast
$pfx    = "../../pixelmaps/";   # prefix for path
$pfx    = "";

# undefined values:
$scale  =    0;
$lat    = 9999;
$lon    = 9999;
$help   =    0;

#--------------------------------------------------------------------------

while (@ARGV) {
    $arg = shift @ARGV;
    if ($arg =~ /^\-N(\d{1,2}(\.\d+)?)$/)  { $lat   =  $1 }
    if ($arg =~ /^\-S(\d{1,2}(\.\d+)?)$/)  { $lat   = -$1 }
    if ($arg =~ /^\-E(\d{1,3}(\.\d+)?)$/)  { $lon   =  $1 }
    if ($arg =~ /^\-W(\d{1,3}(\.\d+)?)$/)  { $lon   = -$1 }
    if ($arg =~ /^\-w(\d+)$/)              { $width =  $1 }
    if ($arg =~ /^\-h(\d+)$/)              { $height=  $1 }
    if ($arg =~ /^\-s(\d+)$/)              { $scale =  $1 }
    if ($arg =~ /^\-s(\d+)k$/)             { $scale =  $1 * 1000 }
    if ($arg =~ /^\-s(\d+)M$/)             { $scale =  $1 * 1000000 }
    if ($arg =~ /^\-p(.+)$/)               { $pfx   =  $1 }
    if ($arg eq '-?')                      { $help  =  1 }
    if ($arg eq '-h')                      { $help  =  1 }
    if ($arg eq '--help')                  { $help  =  1 }
}

if (!$help && $arg && $arg !~ /^\-/) {
    $file = $arg;                       # last arg is file name
} else {
    print("ERROR: Map file name is missing\n");
    $help = 1;
}

if (!$help) {
    if ($lat == 9999) {         # we don't yet have a latitude
        if ($file =~ /N(\d{1,2}(\.\d+)?)/)  { $lat =  $1 }
        if ($file =~ /S(\d{1,2}(\.\d+)?)/)  { $lat = -$1 }
    }
    if ($lon == 9999) {         # we don't yet have a longitude
        if ($file =~ /E(\d{1,3}(\.\d+)?)/)  { $lon =  $1 }
        if ($file =~ /W(\d{1,3}(\.\d+)?)/)  { $lon = -$1 }
    }
    if ($scale == 0) {          # we don't yet have a map scale
        if ($file =~ /\-(\d+)/)  { $scale =  $1 }
        if ($file =~ /\-(\d+)k/) { $scale =  $1 * 1000 }
        if ($file =~ /\-(\d+)M/) { $scale =  $1 * 1000000 }
    }
    if ($lat == 9999) {         # we need a latitude
        print("ERROR: Latitude is missing\n");
        $help = 1;
    }
    if ($lon == 9999) {         # we need a longitude
        print("ERROR: Longitude is missing\n");
        $help = 1;
    }
    if ($scale == 0) {          # we need a map scale
        print("ERROR: Map scale is missing\n");
        $help = 1;
    }
}
    
if ($help) {
    usage();
    exit 0;
}

if ($pfx && $pfx !~ /\/$/) { $pfx .= "/" }      # add trailing '/' if missing

#--------------------------------------------------------------------------

if ($scale >= 1000000) {
    printf("ERROR: Maps with scaling of 1M and above are not yet supported!\n");
    printf("       They use a different projection...\n");
    exit 1;
}

if (abs($lat) > 89) {
    printf("ERROR: Map center too near to the poles!\n");
    exit 1;
}

# This scaling factor is just a wild guess!
# But I need one, and this one is nice AND works quite well... ;-)
$scale_y = 100.0 * pi();                # pixel/degree at 1M map scale

$scale_x = $scale_y * calcScale($lat);  # adjust horizontal scale for latitude

$scale_y *= (1000000 / $scale);         # adjust for current map scale
$scale_x *= (1000000 / $scale);

#--------------------------------------------------------------------------

# DK7IN: I'm not sure, if this formula is exact for what Xastir
#        is decoding, but in my region it works quite well
#        I need to do some further investigation for the best accuracy
$latmin = $lat - ($height / 2.0 - 2.5) / $scale_y;
$latmax = $lat + ($height / 2.0 - 0.5) / $scale_y;
$lonmin = $lon - ($width  / 2.0 - 0.5) / $scale_x;
$lonmax = $lon + ($width  / 2.0 - 2.5) / $scale_x;

printf("FileName %s%s\n",$pfx,$file);
printf("\n");
printf("ImageSize  %4d %4d\n",$width,$height);
printf("TiePoint   %4d %4d  %10.6f %11.6f\n",0,0,$lonmin,$latmax);
printf("TiePoint   %4d %4d  %10.6f %11.6f\n",$width-1,$height-1,$lonmax,$latmin);
printf("Datum      WGS84\n");
printf("Projection LatLon\n");
printf("\n");
printf("# map from mapblast center %10.6f %11.6f, scale %d\n",$lat,$lon,$scale);
printf("# created with mapblast2geo.pl (DK7IN)\n");

exit 0;

#--------------------------------------------------------------------------

sub usage {
    my $name = $0;
    if ($name =~ /^.*\/(.+)$/) { $name = $1 }
    print("\n");
    print("$name (c) 2001 Rolf Bleher <Rolf\@dk7in.de>\n");
    print("create Xastir .geo files for mapblast pixel maps\n");
    print("usage: $name [options] mapfile\n");
    print("       -N52.5 -S10    define latitude\n");
    print("       -E13.3 -W0.5   define longitude\n");
    print("       -h1024         define map height in pixels (default 1024)\n");
    print("       -w1280         define map width in pixels  (default 1280)\n");
    print("       -s50000        define map scale, 50k or 1M is ok\n");
    print("       -p../pixmaps   define prefix for path\n");
    print("       -h -? --help   print this help file\n");
    print("    it tries to extract center Lat/Lon and map scale\n");
    print("    from the filename like N52.5E13.3-50k.xpm\n");
    print("\n");
}

#--------------------------------------------------------------------------

sub pi {
    return(3.14159265358979323846);
}

#--------------------------------------------------------------------------

sub deg2rad {
    my ($deg) = @_;
    return($deg * pi()/180.0);
}

#--------------------------------------------------------------------------

# Calculate distance in meters between two locations
sub dist {
    my ($lat1, $lon1, $lat2, $lon2) = @_;
    my $r_lat1 = deg2rad($lat1);
    my $r_lon1 = deg2rad($lon1);
    my $r_lat2 = deg2rad($lat2);
    my $r_lon2 = deg2rad($lon2);
    my $r_d = acos(sin($r_lat1) * sin($r_lat2) + cos($r_lat1) * cos($r_lat2) * cos($r_lon1-$r_lon2));
    return($r_d*180.0*60.0/pi()*1852.0);
}

#--------------------------------------------------------------------------

sub calcScale {                 # EW / NS scaling
    my ($lat) = @_;
    return(dist($lat,-1.0/120.0,$lat,1.0/120.0) / 1852.0);
}

#--------------------------------------------------------------------------

