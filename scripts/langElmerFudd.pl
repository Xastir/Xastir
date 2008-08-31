#!/usr/bin/perl -W

# $Id$

# Copyright (C) 2008  The Xastir Group
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


# Run it like this:
#
#   cd xastir/config
#   ../scripts/langElmerFudd.pl -split  <language-English.sys >language-ElmerFudd.sys
# or
#   ../scripts/langElmerFudd.pl <some-input-file >some-output-file
#
# "-split": Translate 2nd part of line only (Xastir language file).
# Without it:  Translate entire text.


# Regex strings derived from:
# http://www.faqs.org/docs/diveintopython/dialect_divein.html
# http://www.siafoo.net/snippet/133
# http://dougal.gunters.org/blog/2004/08/30/text-filter-suite


my @regexs = (
  "[rl]:w",
  "[RL]:W",
  "Qu:Qw",
  "qu:qw",
  "th(\b):f",
  "TH(\b):F",
  "th:d",
  "Th:D",
  "N[.]:N, uh-hah-hah-hah.",
  "n[.]:n, uh-hah-hah-hah."
);


# Check whether we're translating an Xastir language file or plain
# text:
#   "-split" present:  Translate the 2nd piece of each line.
#   "-split" absent:   Translate the entire text.
my $a;
if ($#ARGV < 0) { $a = ""; }
else            { $a = shift; }
$do_split = 0;
if (length($a) > 0 && $a =~ m/-split/) {
  $do_split = 1;
}

while ( <> ) {

  # Change the "Id:" RCS tag to show that we translated the file.
  if (m/^#.*\$Id:/) {
      print "# language-ElmerFudd.sys, translated from language-English.sys\n";
      print "# Please do not edit this derived file.\n";
      next;
  }
  # Skip other comment lines
  if (m/^#/) {
    next;
  }

  if ($do_split) {
    # Split each incoming line by the '|' character
    @pieces = split /\|/;
  }

  foreach my $test (@regexs) {

    @reg_parts = split /\:/, $test;

    if ($do_split) {
      # Translate the second portion of each line only
      $pieces[1] =~ s/$reg_parts[0]/$reg_parts[1]/g;
    }
    else {
      # Translate the entire line of text
      s/$reg_parts[0]/$reg_parts[1]/g;
    }
  }

  if ($do_split) {
    # Combine the line again for output to STDOUT
    print join '|', @pieces;
  }
  else {
    print;
  }
}


