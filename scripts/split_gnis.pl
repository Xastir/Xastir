#!/usr/bin/perl -w
#
# $Id$
#
# split_gnis.pl -- 2004 Mar 15 -- jmt@twilley.org

# This script is designed to break large GNIS datapoint files
# into smaller chunks and will dispose of extra whitespace properly.

# It is based on a bash script written by kc9asi@arrl.net.

# The filenames used as input should be put on the command line.


while (<>) {
  next unless /[A-Z][a-z]/;
  s/\s+$//;
  my $line = $_;
  my @fields = split /\|/, $line;
#  print "Fields is @fields\n";
  # key is "state county"
  my $key = $fields[1] . " " . $fields[4];
  $key =~ s/ /_/g;
  push @{$county{$key}}, $line;
}

foreach $elem (keys %county) {
  my $name = $elem . ".gnis";
  open(OUT,">$name");
  foreach $line (@{$county{$elem}}) {
    print OUT $line, "\n";
  }
  close(OUT);
}

