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
#   ../scripts/langMuppetsChef.pl -split <language-English.sys >language-MuppetsChef.sys
# or
#   ../scripts/langMuppetsChef.pl <some-input-file >some-output-file
#
# "-split": Translate 2nd part of line only (Xastir language file).
# Without it:  Translate entire text.


# Regex strings derived from:
# http://www.siafoo.net/snippet/133
# http://www.faqs.org/docs/diveintopython/dialect_divein.html
# http://dougal.gunters.org/blog/2004/08/30/text-filter-suite


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
    print "# language-MuppetsChef.sys, translated from language-English.sys\n";
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

    # Translate the second portion of each line only
    $_ = $pieces[1];
  }

  s/An/Un/g;
  s/an/un/g;
  s/Au/Uu/g;
  s/au/uu/g;
  s/a\b/e/g;
  s/A\b/E/g;
  s/en\b/ee/g;
  s/\bew/oo/g;
  s/\be\b/e-a/g;
  s/\be/i/g;
  s/\bE/I/g;
  s/\bf/ff/g;
  s/\bir/ur/g;
  s/(\w*?)i(\w*?)$/$1ee$2/g;
  s/\bow/oo/g;
  s/\bo/oo/g;
  s/\bO/Oo/g;
  s/the/zee/g;
  s/The/Zee/g;
  s/th\b/t/g;
  s/\btion/shun/g;
  s/\bu/oo/g;
  s/\bU/Oo/g;
  s/v/f/g;
  s/V/F/g;
  s/w/w/g;
  s/W/W/g;
  s/([a-z])[.]/$&.  Bork Bork Bork!/g;

# From the text-filter-suite:
# '%an%' => 'un',
# '%An%' => 'Un',
#
# '%au%' => 'oo',
# '%Au%' => 'Oo',
# '%(\\w)ew%' => '$1oo',
# '%(\\w)ow%' => '$1oo',
# '%(\\W)o%' => '$1oo',
# '%(\\W)O%' => '$1Oo',
# '%(\\w)u%' => '$1oo',
# '%(\\w)U%' => '$1Oo',
#
# '%a(\\w)%' => 'e$1',
# '%A(\\w)%' => 'E$1',
# '%en(\\W)%' => 'ee$1',
#
# '%(\\w)e(\\W)%' => '$1e-a$2',
# '%(\\W)e%' => '$1i',
# '%(\\W)E%' => '$1I',
#
# '%(\\w)f%' => '$1ff',
#
# '%(\\w)ir%' => '$1ur',
#
# '%([a-m])i%' => '$1ee',
# '%([A-M])i%' => '$1EE',
#
# '%(\\w)o%' => '$1u',
# '%the%' => 'zee',
# '%The%' => 'Zee',
# '%th(\\W)%' => 't$1',
# '%(\\w)tion%' => '$1shun',
# '%v%' => 'f',
# '%V%' => 'F',
# '%w%' => 'v',
# '%W%' => 'V',
#
# '%f{2,}%' => 'ff',
# '%o{2,}%' => 'oo',
# '%e{2,}%' => 'ee',
#
# '%([\.!\?])\\s*(</[^>]+>)?\\s*$%' => '$1 Bork Bork Bork!$2',


  if ($do_split) {
    # Combine the line again for output to STDOUT
    $pieces[1] = $_;
    print join '|', @pieces;
  }
  else {
    print;
  }
}


