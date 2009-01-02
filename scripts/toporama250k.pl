#!/usr/bin/perl -W


# $Id$
#
# Copyright (C) 2004-2009 The Xastir Group.
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# Look at the README for more information on the program.


# This will retrieve the 1:250k map images from the "Department of
# Natural Resources Geomatics Canada" site.  These are topographic
# map files for the whole of Canada!
#
#
# - "cd /usr/local/share/xastir/maps"
#
# - Assure you have write privileges in the directory above either
#   by becoming root using the "su" command, using "sudo", or
#   temporarily changing ownership and/or privileges on the
#   "maps" directory.
#
# - "/usr/local/lib/xastir/toporama250k.pl"
#
# - The script will create/populate this directory heirarchy:
#   .../maps/toporama/images/
#   .../maps/toporama/images/b250k/
#
# - Move/rename directories/contents as you wish.
#
# - In Xastir:  "Map->Configure->Index: Reindex ALL Maps!"
#
#
# Note that the same map files are also available from Steve Dimse's
# site:  "http://mm.aprs.net/maps/geo/toporama"
#
#
# Code for this script contributed by Adi Linden, VA3ADI.
# Modifications for latitudes above 67 degrees north contributed by Tom Tessier, VE4TRT



# Create the "toporama" directory
mkdir "toporama";
chdir "toporama";


# Retrieve the 1:250,000 map images.  Skipping those that we've
# already downloaded.
`wget -nv -np -nH -N -r -m -l 0 -T 10 http://toporama.cits.rncan.gc.ca/images/b250k/`;


# Remove index.html files
`rm -rf \`find ./images -type f -name index.html\*\``;
# Remove *.asc files
`rm -rf \`find ./images -type f -name \*.asc\``;



# Define some stuff
$base_dir = "images/b250k";

# This defines the top left corner of the 0 grid up to 67 degrees north latitude. 
$basex = '-56';
$basey = '44';
%offsetx = ( 'a'  => 3,
             'b'  => 2,
             'c'  => 1,
             'd'  => 0,
             'e'  => 0,
             'f'  => 1,
             'g'  => 2,
             'h'  => 3,
             'i'  => 3,
             'j'  => 2,
             'k'  => 1,
             'l'  => 0,
             'm'  => 0,
             'n'  => 1,
             'o'  => 2,
             'p'  => 3,
             '1'  => 3,
             '2'  => 2,
             '3'  => 1,
             '4'  => 0,
             '5'  => 0,
             '6'  => 1,
             '7'  => 2,
             '8'  => 3,
             '9'  => 3,
             '10' => 2,
             '11' => 1,
             '12' => 0,
             '13' => 0,
             '14' => 1,
             '15' => 2,
             '16' => 3 );
%offsety = ( 'a'  => 3,
             'b'  => 3,
             'c'  => 3,
             'd'  => 3,
             'e'  => 2,
             'f'  => 2,
             'g'  => 2,
             'h'  => 2,
             'i'  => 1,
             'j'  => 1,
             'k'  => 1,
             'l'  => 1,
             'm'  => 0,
             'n'  => 0,
             'o'  => 0,
             'p'  => 0,
             '1'  => 3,
             '2'  => 3,
             '3'  => 3,
             '4'  => 3,
             '5'  => 2,
             '6'  => 2,
             '7'  => 2,
             '8'  => 2,
             '9'  => 1,
             '10' => 1,
             '11' => 1,
             '12' => 1,
             '13' => 0,
             '14' => 0,
             '15' => 0,
             '16' => 0 );

# This defines the top left corner of the 0 grid between 68 and 84 degrees north latitude.
%offsetx68 = ( 'a'  => 4,
             'b'  => 0,
             'c'  => 0,
             'd'  => 4,
             'e'  => 4,
             'f'  => 0,
             'g'  => 0,
             'h'  => 4,
             '1'  => 4,
             '2'  => 0,
             '3'  => 0,
             '4'  => 4,
             '5'  => 4,
             '6'  => 0,
             '7'  => 0,
             '8'  => 4 );
%offsety68 = ( 'a'  => 3,
             'b'  => 3,
             'c'  => 2,
             'd'  => 2,
             'e'  => 1,
             'f'  => 1,
             'g'  => 0,
             'h'  => 0,
             '1'  => 3,
             '2'  => 3,
             '3'  => 2,
             '4'  => 2,
             '5'  => 1,
             '6'  => 1,
             '7'  => 0,
             '8'  => 0 );

#def

use File::Find;

print "Writing .geo files\n";
 
find(\&process, $base_dir);

print "\nDone.\n";



# Run for each file found
sub process
{
    # Only look for .gif files
    if ( /.gif$/ ) {
        
        # Get the map sheet designation from the file name
        $file = "$_";
        $grida = substr($file, 0, 3);
        $gridb = substr($file, 3, 1);
	$hilat = substr($file, 2, 1);

	# .geo calculations for lattitudes greater than 78 degrees north latitude.
        if ( $grida > 119 ) {
 
           # Calculate top left coordinates of the map sheet
            use integer;
            $topx = $basex - ((($grida/10) +10 ) / 22 * 16) + $offsetx68{$gridb} * 2; 
            $topy = 84 - $offsety68{$gridb} * 1;
            $botx = $topx + 8;
            $boty = $topy - 2;

            # Create the output file
            $out = substr($file, 0, 4) . ".geo";
            open (OUT, ">$out") or die "Can't open $OUT: $!\n";
            print OUT "# Automatically created by toporama250k.pl\n";
            print OUT "FILENAME\t$file\n";
            print OUT "#\t\tx\ty\tlon\tlat\n";
            print OUT "TIEPOINT\t0\t0\t$topx\t$topy\n";
            print OUT "TIEPOINT\t6399\t1599\t$botx\t$boty\n";
            print OUT "IMAGESIZE\t6400\t1600\n";
            close OUT;

            print ".";
            next;
        }

	# .geo calculations for lattitudes between 68 and 78 degrees north latitude.
	if ( $hilat > 6 ) {

            # Calculate top left coordinates of the map sheet
            use integer;
            $topx = $basex - $grida / 10 * 8 + $offsetx68{$gridb} * 1; 
            $topy = $basey + ($grida % 10) * 4 - $offsety68{$gridb} * 1;
            $botx = $topx + 4;
            $boty = $topy - 1;

            # Create the output file
            $out = substr($file, 0, 4) . ".geo";
            open (OUT, ">$out") or die "Can't open $OUT: $!\n";
            print OUT "# Automatically created by toporama250k.pl\n";
            print OUT "FILENAME\t$file\n";
            print OUT "#\t\tx\ty\tlon\tlat\n";
            print OUT "TIEPOINT\t0\t0\t$topx\t$topy\n";
            print OUT "TIEPOINT\t6399\t1599\t$botx\t$boty\n";
            print OUT "IMAGESIZE\t6400\t1600\n";
            close OUT;

            print ".";
	    next;

	}

        # Calculate top left coordinates of the map sheet
        use integer;
        $topx = $basex - $grida / 10 * 8 + $offsetx{$gridb} * 2; 
        $topy = $basey + ($grida % 10) * 4 - $offsety{$gridb} * 1;
        $botx = $topx + 2;
        $boty = $topy - 1;

        # Create the output file
        $out = substr($file, 0, 4) . ".geo";
        open (OUT, ">$out") or die "Can't open $OUT: $!\n";
        print OUT "# Automatically created by toporama250k.pl\n";
        print OUT "FILENAME\t$file\n";
        print OUT "#\t\tx\ty\tlon\tlat\n";
        print OUT "TIEPOINT\t0\t0\t$topx\t$topy\n";
        print OUT "TIEPOINT\t3199\t1599\t$botx\t$boty\n";
        print OUT "IMAGESIZE\t3200\t1600\n";
        close OUT;

        print ".";
    }
        
}
