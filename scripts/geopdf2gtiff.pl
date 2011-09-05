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
}
close GDALINFO;

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
$theGdalWarp="gdalwarp -cutline $inputPDF.vrt -crop_to_cutline -t_srs EPSG:4326 $inputPDF $outputTif";

system ($theGdalWarp) == 0 or die "System $theGdalWarp failed: $?";


if ($#bandinfo>0)
{
    print "This is a multi-band raster, dithering...\n";
    system ("mv $outputTif $$.tif");
    $theRGB2PCT="rgb2pct.py $$.tif $outputTif";
    system ($theRGB2PCT) == 0 or die "Could not run rgb2pct: $?";
    system("rm  $$.tif");
}

# now clean up our mess:
system ("rm $inputPDF.csv $inputPDF.vrt");
