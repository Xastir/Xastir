#!/usr/bin/perl -W
#
# $Id$
#
# Copyright (C) 2000-2009  The Xastir Group
#
# Updated on 7/5/03 to reflect the new directory structure
# N0VH
#
# Note: Run this script as root in order to write the files into the
# destination directory listed below, or change directory write access.
use File::Basename;
$dirname=dirname($0);
require ($dirname."/values.pl");

my $XASTIR_BASE="${prefix}/share/xastir";


# This script uses temporary storage space in /var/tmp to do its work.
chdir "/var/tmp";


#####################################################################
# Get the FCC database, process it.
#   Download size:  ~84MB
# Final file size: ~101MB
#####################################################################
#
my $file  = "l_amat.zip";
my $file2 = "EN.dat";

 
print STDERR "*********************************\n";
print STDERR "*** Fetching the FCC database ***\n";
print STDERR "*********************************\n";
`wget -c http://wireless.fcc.gov/uls/data/complete/$file`;

if (-e $file && -r $file && -f $file) {

  my $file_out = "$XASTIR_BASE/fcc/$file2";

  # Get rid of characters "^M^M^J" which are sometimes present, sort
  # the file by callsign & remove old entries for vanity call access.
  print STDERR "*****************************************************\n";
  print STDERR "*** Filtering/sorting/installing the FCC database ***\n";
  print STDERR "*****************************************************\n";

  my %from = ();
 
  open FILE, "unzip -p $file $file2|" or die "Can't open $file2 in $file : $!";
  while( <FILE> ) {
    if (/^EN\|(\d+)\|\|\|(\w+)\|.*/) {
      $x = $1;
      $z = $2;
      chop;
      chop;
      $y = $_;
      if (defined $from{$2}) { # check for vanity reassignment
        if ($from{$z} =~ /^EN\|(\d+)\|\|\|(\w+)\|.*/) {
          if ($1 < $x) {
            $replaced++;
            $from{$2} = $y;
          }
        }
      }
      else {
        $from{$2} = $_;
      }
    }
  }
  close FILE;
 
  open FILE_OUT, "|sort -k 5,5 -t \\| > $file_out" or die "Can't sort $file_out : $!";
  for my $callsign ( keys %from ) {
    $total++;
    print FILE_OUT "$from{$callsign}\n";
  }
  close FILE_OUT;

  print STDERR "Total callsigns:  " . $total . ".\n";
  print STDERR " Replaced callsigns:  " . $replaced . ".\n";
}

# Remove the FCC download files
unlink $file;




#####################################################################
# Get the RAC database, process it.
#   Download size:  ~2MB
# Final file size: ~13MB
#####################################################################
#

$file  = "amateur.zip";
$file2 = "amateur.rpt";


print STDERR "*********************************\n";
print STDERR "*** Fetching the RAC database ***\n";
print STDERR "*********************************\n";
`wget -c http://205.236.99.41/%7Eindicatif/download/$file`;


if (-e $file && -r $file && -f $file) {

  print STDERR "***********************************\n";
  print STDERR "*** Installing the RAC database ***\n";
  print STDERR "***********************************\n";
  `unzip $file $file2`;
  `mv $file2 $XASTIR_BASE/fcc/AMACALL.LST`;
}

# Remove the RAC download files
unlink $file, $file2;


print STDERR "*************\n";
print STDERR "*** Done! ***\n";
print STDERR "*************\n";


