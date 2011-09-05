#!/usr/bin/env perl

###############################################################################
# $Id$
#
# Portions Copyright (C) 2004-2011  The Xastir Group
#
# Script to convert a GeoPDF file with included neatline into a collar-stripped
# geotiff in EPSG:4326 projection (WGS84 equidistant cylindrical) in 8-bit
# color.
#
# You must have installed GDAL/OGR, configured to use python in order to use 
# this script.  This is because the script attempts to run rgb2pct.py, which
# is only installed if you've built GDAL with python support.
#
# This also depends on the -cutline and -crop_to_cutline features of 
# gdalwarp, which are only present in versions of GDAL after 1.8.1.  
# 
# Last Edit:$Date$
# Revision:$Revision$
# Last Edited By: $Author$
###############################################################################

use Getopt::Long;

my $fudgeNeatline;
$result=GetOptions("fixneatline|f"=>\$fudgeNeatline);

# Input file is a geopdf:
if ($#ARGV<0)
{
    print STDERR "Usage: $0 <geopdfname>\n";
    exit 1;
}

my $inputPDF=$ARGV[0];
if (! -e $inputPDF)
{
    print STDERR "File $inputPDF does not exist.\n";
    exit 1;
}

open GDALINFO, "gdalinfo $inputPDF |" or die "can't fork $!";
my $readingCoordSys=0;
my $coordSys="";
my $readingMetadata=0;
my $neatlinePoly="";
my $minLat=360,$maxLat=-360,$minLon=360,$maxLon=-360,$templat,$templon;

while (<GDALINFO>)
{
  if (/^Driver: (.*)$/)
  {
      if ($1 ne "PDF/Geospatial PDF")
      {
          print STDERR "This script is intended to run only on GeoPDF input files.\n";
          exit 1;
      }
      next;
  }
  if (/^Coordinate System is:/)
  {
      $readingCoordSys=1;
      next;
  }
  if ($readingCoordSys == 1)
  {
      # This is tricky --- finding the end of the coordinate system
      # means looking for the first line that starts in column 1 that isn't
      # the first line of the coordinate system.  All lines of the coordinate
      # system other than the first start with space.
      if (/^[^ ]/ && $coordSys ne "")
      {
          $readingCoordSys=0;
          next;
      }
      else
      {
          chomp($_);
          $coordSys .= $_;
          next;
      }
  }
  if (/^Metadata:/)
  {
      $readingMetadata=1;
      next;
  }
  if ($readingMetadata==1)
  {
      if (/^Corner Coordinates:/)
      {
          $readingMetadata=0;
          next;
      }
      if (/^ *NEATLINE=(.*)$/)
      {
          $neatlinePoly=$1;
          next;
      }
  }
  if (/^Band ([0-9]+).*Type=([^,]*)/)
  {
      $bandinfo[$#bandinfo+1]=$2;
      next;
  }
  if (/^(Upper|Lower) (Left|Right)/)
  {
      s/(Upper|Lower) (Left|Right) *\([^)]*\) *//;
      /\(([^,]*), *([^\)]*)\)/;
      $templonstr=$1;
      $templatstr=$2;
      $templonstr =~ /([0-9]*)d( *[0-9]*)'( *[0-9.]*)"([EW])/;
      $deg=$1; $min=$2; $sec=$3; $hem=$4;
      $templon=$deg+$min/60+$sec/3600;
      $templon *= -1 if ($hem eq "W");
      $minLon=$templon if ($templon<$minLon);
      $maxLon=$templon if ($templon>$maxLon);

      $templatstr =~ /([0-9]*)d( *[0-9]*)'( *[0-9.]*)"([NS])/;
      $deg=$1; $min=$2; $sec=$3; $hem=$4;
      $templat=$deg+$min/60+$sec/3600;
      $templat *= -1 if ($hem eq "S");
      $minLat=$templat if ($templat<$minLat);
      $maxLat=$templat if ($templat>$maxLat);

  }
    
}
close GDALINFO;

if (! $fudgeNeatline)
{
# Create a CSV for the neatline.  OGR will recognize this geometry
    open CSVFILE, ">$inputPDF.csv";
    print CSVFILE "foo,WKT\n";
    print CSVFILE "bar,\"$neatlinePoly\"\n";
    close CSVFILE;
    
# Unfortunately, there's no easy way to attach a spatial reference system
# (SRS) to the csv file so that gdalwarp will recognize it.  So make a 
# virtual layer out of the CSV with the reference system in it.
    open VRTFILE, ">$inputPDF.vrt";
    print VRTFILE "<OGRVRTDataSource>\n";
    print VRTFILE " <OGRVRTLayer name=\"$inputPDF\">\n";
    print VRTFILE "   <LayerSRS>$coordSys</LayerSRS>\n";
    print VRTFILE "   <SrcDataSource>$inputPDF.csv</SrcDataSource>\n";
    print VRTFILE "   <GeometryType>wkbPolygon</GeometryType>\n";
    print VRTFILE "   <GeometryField>WKT</GeometryField>\n";
    print VRTFILE " </OGRVRTLayer>\n";
    print VRTFILE "</OGRVRTDataSource>\n";
    close VRTFILE;
}
else
{
    print "User asked us to fudge the neatline, finding nearest 7.5 minute quad boundaries\n";


    # Instead of using the neatline specified, round it to nearest 7.5' 
    # quad boundary
    $left=(int((abs($minLon)-int(abs($minLon)))/.125+.5)*.125+int(abs($minLon)))*($minLon/abs($minLon));
    $right=(int((abs($maxLon)-int(abs($maxLon)))/.125+.5)*.125+int(abs($maxLon)))*($maxLon/abs($maxLon));
    if ($maxLon<0)
    {
        $temp=$left;
        $left=$right;
        $right=$temp;
    }
    $bottom=(int((abs($minLat)-int(abs($minLat)))/.125+.5)*.125+int(abs($minLat)))*($minLat/abs($minLat));
    $top=(int((abs($maxLat)-int(abs($maxLat)))/.125+.5)*.125+int(abs($maxLat)))*($maxLat/abs($maxLat));
    if ($maxLat<0)
    {
        $temp=$top;
        $top=$bottom;
        $bottom=$temp;
    }


    open CSVFILE, ">$inputPDF.csv";
    print CSVFILE "foo,WKT\n";
    print CSVFILE "bar,\"POLYGON(($left $top,$right $top, $right $bottom, $left $bottom, $left $top))\"\n";
    close CSVFILE;

# Unfortunately, there's no easy way to attach a spatial reference system
# (SRS) to the csv file so that gdalwarp will recognize it.  So make a 
# virtual layer out of the CSV with the reference system in it.
    open VRTFILE, ">$inputPDF.vrt";
    print VRTFILE "<OGRVRTDataSource>\n";
    print VRTFILE " <OGRVRTLayer name=\"$inputPDF\">\n";
    print VRTFILE "   <LayerSRS>EPSG:4326</LayerSRS>\n";
    print VRTFILE "   <SrcDataSource>$inputPDF.csv</SrcDataSource>\n";
    print VRTFILE "   <GeometryType>wkbPolygon</GeometryType>\n";
    print VRTFILE "   <GeometryField>WKT</GeometryField>\n";
    print VRTFILE " </OGRVRTLayer>\n";
    print VRTFILE "</OGRVRTDataSource>\n";
    close VRTFILE;
}

# The VRT virtual source will now appear to gdal warp as a complete 
# specification of the neatline, with both coordinates *AND* description of
# the coordinate system.  This will enable gdalwarp to do the clipping of the
# neatline *AND* conversion to a different coordinate system in the same
# operation.  Had we not done the vrt, gdalwarp would have assumed the 
# neatline to be in the coordinates of the DESTINATION raster, not the
# source raster.

$outputTif=$inputPDF;
$outputTif =~ s/pdf/tif/i;

# We will warp from whatever the coordinate system is into EPSG:4326, the
# coordinate system that requires the least work and involves the fewest
# approximations from Xastir.
$theGdalWarp="gdalwarp -cutline $inputPDF.vrt -crop_to_cutline -t_srs EPSG:4326  -co \"COMPRESS=PACKBITS\" $inputPDF $outputTif";
#$theGdalWarp="gdalwarp -cutline $inputPDF.vrt -crop_to_cutline -t_srs EPSG:4326  $inputPDF $outputTif";

system ($theGdalWarp) == 0 or die "System $theGdalWarp failed: $?";


if ($#bandinfo>0)
{
    print "This is a multi-band raster, dithering...\n";
    system ("mv $outputTif $$.tif");
    $theRGB2PCT="rgb2pct.py $$.tif $$-2.tif";
    system ($theRGB2PCT) == 0 or die "Could not run rgb2pct: $?";
    system("gdal_translate -co \"COMPRESS=PACKBITS\" $$-2.tif $outputTif") == 0 or die "Could not gdal_translate: $?";
    system("rm  $$.tif $$-2.tif");
}

# now clean up our mess:
system ("rm $inputPDF.csv $inputPDF.vrt");
