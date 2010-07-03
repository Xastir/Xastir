#!/usr/bin/perl
#
# $Id$
#
# Contributed to the public domain.  Authored by Jeremy McDermond
# (NH6Z).
#
# This script takes a single argument:  The abbreviation of the
# radar station off of the NWS site, and outputs to STDOUT a .geo
# file that should be correct.
# NOTE:  You'll need to install "LWP::UserAgent" and "Image::Size"
# from CPAN to make it work.
#
# Here's a typical invocation which creates a NYC Ridge Radar .geo
# file called OKX_NOR.geo:
#
#       ./ridge_radar.pl OKX > OKX_NOR.geo
#
# Of course you'd typically put the resulting file in your Xastir
# maps directory and reindex maps to make it available for use.


use strict;
use LWP::UserAgent;
use Image::Size;

my $station = uc($ARGV[0]);

my $gif_url = 'http://radar.weather.gov/ridge/RadarImg/N0R/' .  $station . '_N0R_0.gif';

my $response = LWP::UserAgent->new->request(
  HTTP::Request->new( GET => $gif_url )
);

unless($response->is_success) {
  die "Couldn't get radar image: ", $response->status_line, "\n";
}

my ($img_x, $img_y) = imgsize(\$response->content);

my $response = LWP::UserAgent->new->request(
  HTTP::Request->new( GET => 'http://radar.weather.gov/ridge/RadarImg/N0R/' . $station .  '_N0R_0.gfw' )
);

unless($response->is_success) {
  die "Couldn't get radar descriptor: ", $response->status_line, "\n";
}

my ( $yscale, undef, undef, $xscale, $lon, $lat ) = split(/\r\n/, $response->content);

my $tiepoint_lat = $lat - ($yscale * $img_y);
my $tiepoint_lon = $lon - ($xscale * $img_x);

print "URL\t\t$gif_url\n";
print "TIEPOINT\t0\t0\t$lon\t$lat\n";
print "TIEPOINT\t$img_x\t$img_y\t$tiepoint_lon\t$tiepoint_lat\n";
print "IMAGESIZE\t$img_x\t$img_y\n";
print "REFRESH\t\t60\n";
print "TRANSPARENT\t0x0\n";
print "PROJECTION\tLatLon\n";


