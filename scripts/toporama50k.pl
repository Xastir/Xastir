#!/usr/bin/perl

# $Id$
#
# This will retrieve the 1:50k map images from the "Department of
# Natural Resources Geomatics Canada" site.  These are topographic
# map files for the whole of Canada!
#
#
# * cd /usr/local/share/xastir/maps
# * Run this script from there.  Make sure that you have write
#   privileges there.
# * Move the directories and contents to whereever makes sense.
# * Reindex the maps in Xastir in order to have them appear in the
#   Map Chooser.
#
#
# Note that the same map files are also available from Steve Dimse's
# site:  "http://mm.aprs.net/maps/geo/toporama"
#
#
# Code for this script contributed by Adi Linden, VA3ADI.



# Create the "toporama" directory
mkdir "toporama";
chdir "toporama";


# Retrieve the 1:50,000 map images.  Skipping those that we've
# already downloaded.
`wget -nv -np -nH -N -r -m -l 0 -T 10 http://toporama.cits.rncan.gc.ca/images/b50k/`;


# Remove index.html files
`rm -rf \`find ./images -type f -name index.html\*\``;
# Remove *.asc files
`rm -rf \`find ./images -type f -name \*.asc\``;


# Define some stuff
$base_dir = "images/b50k";

# This defines the top left corner of the 0 grid. 
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
             '01' => 3,
             '02' => 2,
             '03' => 1,
             '04' => 0,
             '05' => 0,
             '06' => 1,
             '07' => 2,
             '08' => 3,
             '09' => 3,
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
             '01' => 3,
             '02' => 3,
             '03' => 3,
             '04' => 3,
             '05' => 2,
             '06' => 2,
             '07' => 2,
             '08' => 2,
             '09' => 1,
             '10' => 1,
             '11' => 1,
             '12' => 1,
             '13' => 0,
             '14' => 0,
             '15' => 0,
             '16' => 0 );

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
        $gridc = substr($file, 4, 2);

        if ( $grida > 119 ) {
            next;
        }

        # Calculate top left coordinates of the map sheet
        use integer;
        $topx = $basex - $grida / 10 * 8 + $offsetx{$gridb} * 2; 
        $topy = $basey + ($grida % 10) * 4 - $offsety{$gridb} * 1;
        no integer;
        $topx = $topx + $offsetx{$gridc} * 0.5;
        $topy = $topy - $offsety{$gridc} * 0.25;
        $botx = $topx + 0.5;
        $boty = $topy - 0.25;

        # Create the output file
        $out = substr($file, 0, 6) . ".geo";
        open (OUT, ">$out") or die "Can't open $OUT: $!\n";
        print OUT "# Automatically created by toporama50k2geo.pl\n";
        print OUT "FILENAME\t$file\n";
        print OUT "#\t\tx\ty\tlon\tlat\n";
        print OUT "TIEPOINT\t0\t0\t$topx\t$topy\n";
        print OUT "TIEPOINT\t3199\t1599\t$botx\t$boty\n";
        print OUT "IMAGESIZE\t3200\t1600\n";
        close OUT;

        print ".";
    }
        
}


