#!/usr/bin/perl
# -*- perl -*-
# Copyright (C) 2002 Derrick J Brashear, KB3EGH
# Released to the public domain.

# $Id$

# Usage: mapfgd <directory>
# Creates fake fgd files for all correctly USGS-named maps which don't 
# already have them

local (@dirlist) = @ARGV;
local ($mapdir);
foreach $mapdir (@dirlist)
{
    opendir (DIR, $mapdir) || die "mapini: couldn't open directory \`$mapdir': $!\n";
    local ($file, $fullfile, $expr, $mlat, $mlon, $mlats, $mlons);
    local ($nlat, $nlon, $nlats, $nlons);
    local ($dlat, $dlon, $inifile);
    foreach $file (sort grep (! /^\./, readdir (DIR)))
    {
	# Only examine .tif files.
	next unless $file =~ /\.tif$/;
	next unless $file =~ /^[oOcCfFkKlLpPjJgG]/;
	$fullfile = $mapdir . '/' . $file;
	$inifile = $mapdir . '/' . $file;
	$inifile =~ s/\.tif$/.fgd/;
	next if (-f $inifile);
	open (INI, ">$inifile");
	$file =~ /^([oOcCfFkKlLpPjJgG])([0-9][0-9])([0-9][0-9][0-9])([a-hA-H])([1-8])/;
	$letter = $1;
	$mlat = $2;
	$mlon = $3;
	$mlats = $4;
	$mlons = $5;

	if ($letter eq 'c') {
	    $dlon = 0-$mlon-2;
	    printf INI "1.5.1.1   WEST BOUNDING COORDINATE:  %.6f\n", $dlon;
	    $dlon = 0-$mlon;
	    printf INI "1.5.1.2   EAST BOUNDING COORDINATE:  %.6f\n", $dlon;
	    $dlat = $mlat+1;
	    printf INI "1.5.1.3   NORTH BOUNDING COORDINATE:  %.6f\n", $dlat;
	    $dlat = $mlat;
	    printf INI "1.5.1.4   SOUTH BOUNDING COORDINATE:  %.6f\n", $dlat;
	} else {
	    if (($letter eq 'f') || ($letter eq 'g')){
		$dlon = 0-$mlon-1;
		printf INI "1.5.1.1   WEST BOUNDING COORDINATE:  %.6f\n", $dlon;
		$dlon = 0-$mlon-(.125*$mlons)+.125;
		printf INI "1.5.1.2   EAST BOUNDING COORDINATE:  %.6f\n", $dlon;
		$dlat = $mlat+.375+(.125*(&lettonum ($mlats)));
		printf INI "1.5.1.3   NORTH BOUNDING COORDINATE:  %.6f\n", $dlat;
		$dlat = $mlat-.125+(.125*(&lettonum ($mlats)));
		printf INI "1.5.1.4   SOUTH BOUNDING COORDINATE:  %.6f\n", $dlat;
	    } else {
		if ($letter eq 'k') {
		    $dlon = 0-$mlon-1;
		    printf INI "1.5.1.1   WEST BOUNDING COORDINATE:  %.6f\n", $dlon;
		    $dlon = 0-$mlon-(.125*$mlons)+.25;
		    printf INI "1.5.1.2   EAST BOUNDING COORDINATE:  %.6f\n", $dlon;
		    $dlat = $mlat+.375+(.125*(&lettonum ($mlats)));
		    printf INI "1.5.1.3   NORTH BOUNDING COORDINATE:  %.6f\n", $dlat;
		    $dlat = $mlat-.125+(.125*(&lettonum ($mlats)));
		    printf INI "1.5.1.4   SOUTH BOUNDING COORDINATE:  %.6f\n", $dlat;
		} else {
		    $dlon = 0-$mlon-(.125*$mlons);
		    printf INI "1.5.1.1   WEST BOUNDING COORDINATE:  %.6f\n", $dlon;
		    $dlon = 0-$mlon-(.125*$mlons)+.125;
		    printf INI "1.5.1.2   EAST BOUNDING COORDINATE:  %.6f\n", $dlon;
		    $dlat = $mlat+(.125*(&lettonum ($mlats)));
		    printf INI "1.5.1.3   NORTH BOUNDING COORDINATE:  %.6f\n", $dlat;
		    $dlat = $mlat-.125+(.125*(&lettonum ($mlats)));
		    printf INI "1.5.1.4   SOUTH BOUNDING COORDINATE:  %.6f\n", $dlat;
		}
	    }
	}

	close (INI);
    }
    closedir (DIR);
}

sub lettonum
{
    local ($let) = @_;
    if ($let eq 'a')
    {return 1;}
    if ($let eq 'b')
    {return 2;}
    if ($let eq 'c')
    {return 3;}
    if ($let eq 'd')
    {return 4;}
    if ($let eq 'e')
    {return 5;}
    if ($let eq 'f')
    {return 6;}
    if ($let eq 'g')
    {return 7;}
    if ($let eq 'h')
    {return 8;}
}
sub nextlet
{
    local ($let,$scale) = @_;
    if ($scale eq 'c') 
    {$nlat++;return 'a';}
    if ($scale eq 'f') {
	if ($let eq 'a') 
	{return 'e';}
	if ($let eq 'e') 
	{$nlat++;return 'a';}
    }    
    if ($let eq 'a')
    {return 'b';}
    if ($let eq 'b')
    {return 'c';}
    if ($let eq 'c')
    {return 'd';}
    if ($let eq 'd')
    {return 'e';}
    if ($let eq 'e')
    {return 'f';}
    if ($let eq 'f')
    {return 'g';}
    if ($let eq 'g')
    {return 'h';}
    if ($let eq 'h')
    {$nlat++;return 'a';}
}
sub nextnum
{
    local ($let,$scale) = @_;
    if ($scale eq 'c') {
	$nlon+=2;
	if ($nlon < 100) {
	    $nlon="0$nlon";
	}
	return '1';
    }
    if ($scale eq 'f') {
	$nlon++;
	if ($nlon < 100) {
	    $nlon="0$nlon";
	}
	return '1';
    }
    if ($let eq '1')
    {return '2';}
    if ($let eq '2')
    {return '3';}
    if ($let eq '3')
    {return '4';}
    if ($let eq '4')
    {return '5';}
    if ($let eq '5')
    {return '6';}
    if ($let eq '6')
    {return '7';}
    if ($let eq '7')
    {return '8';}
    if ($let eq '8')
    {$nlon++;
     if ($nlon < 100) {
	 $nlon="0$nlon";
     }
     return '1';}
}
sub oldlastnum
{
    local ($let,$scale) = @_;
    if ($let = 1) 
    {print INI "let was 1\n";$let=9; $nlon--; $nlon="0$nlon";}
    $let--;
    return $let;
}
sub lastnum
{
    local ($let,$scale) = @_;
    if ($scale eq 'c') {
	$nlon-=2; 
	if ($nlon < 100) {
	    $nlon="0$nlon";
	}
	return '1';
    }
    if ($scale eq 'f') {
	$nlon--; 
	if ($nlon < 100) {
	    $nlon="0$nlon";
	}
	return '1';
    }
    if ($let eq '1') {
	$nlon--; 
	if ($nlon < 100) {
	    $nlon="0$nlon";
	}
	return '8';
    }
    if ($let eq '2')
    {return '1';}
    if ($let eq '3')
    {return '2';}
    if ($let eq '4')
    {return '3';}
    if ($let eq '5')
    {return '4';}
    if ($let eq '6')
    {return '5';}
    if ($let eq '7')
    {return '6';}
    if ($let eq '8')
    {return '7';}
}
sub lastlet
{
    local ($let,$scale) = @_;
    if ($scale eq 'c') 
    {$nlat--;return 'a';}
    if ($scale eq 'f') {
	if ($let eq 'a') 
	{$nlat--;return 'e';}
	if ($let eq 'e') 
	{return 'a';}
    }    
    if ($let eq 'a')
    {$nlat--;return 'h';}
    if ($let eq 'b')
    {return 'a';}
    if ($let eq 'c')
    {return 'b';}
    if ($let eq 'd')
    {return 'c';}
    if ($let eq 'e')
    {return 'd';}
    if ($let eq 'f')
    {return 'e';}
    if ($let eq 'g')
    {return 'f';}
    if ($let eq 'h')
    {return 'g';}
}

