#!/usr/bin/env perl
#
# $Id$
#
#  Copyright (C) 2006-2009 Tom Russo

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
#
# NOTE:  This script is not finished yet, so won't work for
# converting POS (overlay) files to Shapefile maps yet.  Stay tuned!
#
# This script is a repurposed object2shp.pl script.  The intent is
# to do this:
#
# Tom Russo said:  "It could be trivially modified to do pos format
# instead just by changing the parsing lines between lines 81 and
# 89, I think.  Those lines look for the semicolon at the beginning
# of the line...  None of those are present in the
# pos format.  And it assumes the name is 9 characters, which is not
# the case, so one would have to  replace that by a search forward
# for the bang ("!") to get the name delimiter.  All the assumptions
# about where things live on the line are broken for pos format, but
# once one rewrote that block to fill in the right variables from
# the alternate format, the rest of the dbfadd/shpadd stuff will all
# work."
#
#---------------------------------------------
#
# This script produces an ESRI point shapefile from an APRS overlay
# file (*.pos), according to the "Rolling your own shapefile maps"
# section of README.MAPS The point file will display using the TIGER
# Landmark Point dbfawk file.
#
# This enables fast generation of point maps in Shapefile format
# from APRS overlay files.
#
# Typical usage:  
#   pos2shp.pl file.pos myshape
#--------------------------------------------------------------------------


if ($#ARGV != 1)
{
    print "Usage: $0 <file.pos> <shapefile base name>\n";
    exit 1;
}


open(INOBJ,"<$ARGV[0]") || die "Cannot open input overlay file $ARGV[0]\n";

$cmd[0]="shpcreate $ARGV[1] point";
$cmd[1]="dbfcreate $ARGV[1] -n ID 8 0 -s CFCC 4 -s NAME 30";
$outfile=$ARGV[1];




foreach $command (@cmd)
{
    system($command);
    if ($? == -1) 
    {
        print "failed to execute: $!\n";
    }
    elsif ($? & 127) 
    {
        printf "child died with signal %d, %s coredump\n",
        ($? & 127),  ($? & 128) ? 'with' : 'without';
    }
    elsif ($?&0xF0)
    {
        printf "child exited with value %d\n", $? >> 8;
    }
}


# -------------------------------------------------------------------
# APRSdos instructions (delete these when we get it working):
#
# FORMAT: Each line has a variable-length NAME of up to 9 characters
# followed by the normal FIXED station APRS position format
# beginnning with an exclamation point. They also contain a
# single-line descriptive comment at the beginning of the file
# intended to be displayed to the user when he executes the OVERLAY
# command.

# Examples: All of "stores.pos" from APRSdos:
# -------------------------------------------
#   * All known HAM stores Nationwide
#   HRO!3350.64N\11756.49WhAnaheim
#   HRO!3411.07N\11820.26WhBurbank
#   HRO!3746.89N\12214.42WhOkland
#   HRO!3249.88N\11708.32WhSan Diego
#   HRO!3723.20N\12159.77WhSunnyvale
#   HRO!3941.43N\07534.86WhNew CastleDE
#   HRO!4526.15N\12245.63WhPortlandOR
#   HRO!3940.48N\10453.56WhDenver
#   HRO!3334.06N\11205.99WhPhoenix
#   HRO!3354.96N\08415.62WhAtlanta
#   HRO!3837.73N\07716.46WhWoodbridgeVA
#   HRO!4247.43N\07114.10WhSalemNH
#   AES!4308.92N\08758.92WhMilwaukee
#   AES!4136.15N\08128.77WhWickliffeOH
#   AES!2833.17N\08120.19WhOrlando
#   AES!2758.11N\08245.52WhClearwater
#   AES!3610.97N\11510.82WhLas Vagas
#
# Some lines from "wx-aprs.pos" (10 chars before the '!' and no
# leading comment (from APRSdos):
# -------------------------------------------------------------
#   KA0RDE    !4040.38N/09448.21W_U-II(auto)
#   KG0DW     !4136.45N/09345.16W_U-II(auto)
#   KU0G-2    !3913.31N/09430.40W_KC APRS WX (U-ll Remote)(Parkville)
#   NOAN      !4158.20N/09333.59W_U-II(auto)
#   N0BKB     !4120.57N/09419.03W_U-II(auto)
#   N0GGU     !4009.00N/09502.49W_U-II(auto)
#
# All of "freqs.pos" from APRSdos:
# --------------------------------
#   * ALL Known APRS Frequencies
#   .39!4630.36N/11727.96W.PHG5<50
#   .39!4458.20N/10409.24W.PHG9:90
#   5.15!3920.28N/11119.32W.PHG3:60
#   .39!3748.12N/12032.28W.PHG6:60
#   5.79!3342.36N/11626.52W.PHG3:30
#   .39!3443.80N/11119.32W.PHG3:30
#   5.01!3240.92N/10510.68W.PHG6:60
#   .39!3803.48N/10510.68W.PHG3:90
#   5.79!3600.60N/09800.60W.PHG6:60
#   .39!2951.96N/09800.60W.PHG6:60
#   5.79!4645.72N/09354.84W.PHG6:60
#   .39!4107.80N/09557.72W.PHG4:40
#   .39!4239.96N/08208.28W.PHG9;90
#   .39!3803.48N/09151.96W.PHG3:30
#   5.79!3327.00N/08137.56W.PHG5:50
#   5.79!3357.72N/08816.92W.PHG5:50
#   .39!2718.36N/08441.88W.PHG8:80
#   .39!2951.96N/09136.60W.PHG4:40
#   .39!4458.20N/06951.00W.PHG6:60
#
# -------------------------------------------------------------------


# We now have the shapefile and dbf file created, start populating from the
# overlay file:

$i=0;
$first_line = 1;
while (<INOBJ>)
{
    chomp($_);

    # Skip the first line if it starts with a '*' (comment line).
    next if ($first_line && (substr($_,0,1) eq '*') );
    $first_line = 0;

# -------------------------------------------------------------------
    $temp = $_;
    @bits = split('!', $temp);

    # Sanity check --- don't try to convert if the line doesn't
    # conform to what we expect.  We should have two items in the
    # array at this point.
    next if (length(@bits) < 1);

    $name = $bits[0];
    $rest = $bits[1];

# TODO: Chop $name at 9 chars then remove trailing spaces

    $lat=substr($rest,0,8);
    $symtab=substr($rest,8,1);
    $long=substr($rest,9,9);
    $sym=substr($rest,18,1);

# TODO: Do something with the comment field
    $comment=substr($rest,19,100);
# -------------------------------------------------------------------

    $i++;  # bump the ID number so every point has a unique one

    $lat_deg=substr($lat,0,2);
    $lat_min=substr($lat,2,5);
    $lat_hem=substr($lat,7,1);

    $long_deg=substr($long,0,3);
    $long_min=substr($long,3,5);
    $long_hem=substr($long,8,1);

    $lat=$lat_deg+$lat_min/60;
    $lat *= -1 if ($lat_hem eq "S");

    $long=$long_deg+$long_min/60;
    $long *= -1 if ($long_hem eq "W");

    # Construct symbol
    
    if ($symtab ne "/" && $symtab ne "\\")
    {
        print "overlay symbol, symtab is $symtab\n";
        $overlay=$symtab;
        $symtab="\\";
        print " reset values symtab is $symtab, overlay is $overlay\n";
    } 
    else
    {
        $overlay=" ";
    }

    $cmd[0]="shpadd $outfile $long $lat";
    $cmd[1]="dbfadd $outfile $i \'X$symtab$sym$overlay\' $name";
    print $cmd[1]."\n";
    foreach $command (@cmd)
    {
        system($command);
        if ($? == -1) 
        {
            print "failed to execute: $!\n";
        }
        elsif ($? & 127) 
        {
            printf "child died with signal %d, %s coredump\n",
            ($? & 127),  ($? & 128) ? 'with' : 'without';
        }
        elsif ($?&0xF0)
        {
            printf "child exited with value %d\n", $? >> 8;
        }
    }
    
}


