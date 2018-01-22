#!/usr/bin/perl -W

#
# Copyright (C) 2000-2018 The Xastir Group
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
# Run this as "./wms.pl URL", where URL = a "GetCapabilities" link, but
# with a backslash added before each '&' character. The script will
# download the XML file at that URL and parse it. It will then  attempt
# to find the map layer names and list them at the end of the run.
#my $xml_text = `wget -O - "http://geogratis.gc.ca/maps/CBMT?service=wms\&version=1.1.1\&request\=GetCapabilities"`;
#my $xml_text = `wget -O - "http://nowcoast.noaa.gov/arcgis/services/nowcoast/radar_meteo_imagery_nexrad_time/MapServer/WMSServer?request=GetCapabilities\&service=WMS\&version=1.3.0"`;


#
# Once you have the map layer names it is relatively easy to construct a
# .geo file for Xastir that uses one of those map layers. Here's an example
# from README.MAPS. Both lines are required in the .geo file:
# -----------------------
# WMSSERVER
# URL http://geogratis.gc.ca/maps/CBMT?VERSION=1.1.1&SERVICE=WMS&REQUEST=GetMap&SRS=EPSG:4326&LAYERS=Sub_regional&STYLES=&FORMAT=image/png&BGCOLOR=0xFFFFFF&TRANSPARENT=FALSE
# -----------------------
#
#Here's the info from Dumper that we're interested in (Name):
#$VAR1 = {
#          'Capability' => [
#                          {
#                            'Layer' => [
#                                       {
#                                         'Layer' => [
#                                                    {
#                                                      'Name' => [
#                                                                'National'
#
# -or-
#
#$VAR1 = {
#          'Capability' => {
#                          'Layer' => {
#                                     'Layer' => {
#                                                'Layer' => [
#                                                           {
#                                                             'Name' => '1',


use strict;
use XML::Simple;
use Getopt::Std;
use Data::Dumper;
use Scalar::Util 'reftype';


my $url;
$url = shift;
if (!defined $url || $url eq "") {
  print "Please enter GetCapabilities URL with '\' before each '&' character.\n";
  $url = <>;
}

my $xml_text= `wget -O - $url`;
my $xml = XMLin( $xml_text );

print Dumper($xml);

print "\n----------------------------------------------------------\n\n";

my $version = $url;
$version =~ s/.*version=(\d+\.\d+\.\d+).*/$1/;
if ($version =~ /\d+\.\d+\.\d+/) {
}
else {
  $version = "1.0.0";
}

my $url_filtered = $url;
$url_filtered =~ s/\\//g;   # Get rid of backslashes
$url_filtered =~ s/(.*\?).*/$1/;   # Remove everything after '?'
$url_filtered = "URL " . $url_filtered . "SERVICE=wms&VERSION=$version&REQUEST=GetMap&SRS=EPSG:4326&FORMAT=image/png&BGCOLOR=0xFFFFFF&TRANSPARENT=false&STYLES=&LAYERS=";

print "POSSIBLE .GEO FILE CONTENTS:\n\n";

my $ii;
my $reftype = reftype $xml->{Capability}->{Layer}->{Layer};
if ( (defined $reftype) && ($reftype eq 'ARRAY') ) {
  for ($ii = 0; $ii < 15; $ii++) {
    if ( defined($xml->{Capability}->{Layer}->{Layer}->[$ii]->{Name}) ) {
      print "-----\n";
      print "WMSSERVER\n";
      print $url_filtered;
      print "$xml->{Capability}->{Layer}->{Layer}->[$ii]->{Name}\n";
    }
  }
}

if ( (defined $reftype) && ($reftype eq 'HASH') ) {
  for ($ii = 0; $ii < 15; $ii++) {
    if ( defined($xml->{Capability}->{Layer}->{Layer}->{Layer}->[$ii]->{Name}) ) {
      print "-----\n";
      print "\nWMSSERVER\n";
      print $url_filtered;
      print "$xml->{Capability}->{Layer}->{Layer}->{Layer}->[$ii]->{Name}\n";
    }
  }
}


