#!/usr/bin/perl -n

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
#   ../scripts/langMuppetsChef.pl <language-English.sys >language-MuppetsChef.sys
#
# If you would like, replace the language-English.sys file in the
# destination directory (usually "/usr/local/share/xastir/config")
# with this new file (renaming it to "language-English.sys" of
# course), then restart Xastir and you'll have it displaying in
# Muppets Chef-speak!


# Regex strings derived from:
# http://www.siafoo.net/snippet/133
# http://www.faqs.org/docs/diveintopython/dialect_divein.html
# http://dougal.gunters.org/blog/2004/08/30/text-filter-suite


my @regexs = (
  "a([nu]):u$1",
  "A([nu]):U$1",
  "a\B:e",
  "A\B:E",
  "en\b:ee",
  "\Bew:oo",
  "\Be\b:e-a",
  "\be:i",
  "\bE:I",
  "\Bf:ff",
  "\Bir:ur",
  "(\w*?)i(\w*?)$:$1ee$2",
  "\bow:oo",
  "\bo:oo",
  "\bO:Oo",
  "the:zee",
  "The:Zee",
  "th\b:t",
  "\Btion:shun",
  "\Bu:oo",
  "\BU:Oo",
  "v:f",
  "V:F",
  "w:w",
  "W:W",
  "([a-z])[.]:$&.  Bork Bork Bork!"

# From the text-filter-suite:
# '%an%' => 'un',
# '%An%' => 'Un',
#
# '%au%' => 'oo',
# '%Au%' => 'Oo',
# '%(\w)ew%' => '$1oo',
# '%(\w)ow%' => '$1oo',
# '%(\W)o%' => '$1oo',
# '%(\W)O%' => '$1Oo',
# '%(\w)u%' => '$1oo',
# '%(\w)U%' => '$1Oo',
#
# '%a(\w)%' => 'e$1',
# '%A(\w)%' => 'E$1',
# '%en(\W)%' => 'ee$1',
#
# '%(\w)e(\W)%' => '$1e-a$2',
# '%(\W)e%' => '$1i',
# '%(\W)E%' => '$1I',
#
# '%(\w)f%' => '$1ff',
#
# '%(\w)ir%' => '$1ur',
#
# '%([a-m])i%' => '$1ee',
# '%([A-M])i%' => '$1EE',
#
# '%(\w)o%' => '$1u',
# '%the%' => 'zee',
# '%The%' => 'Zee',
# '%th(\W)%' => 't$1',
# '%(\w)tion%' => '$1shun',
# '%v%' => 'f',
# '%V%' => 'F',
# '%w%' => 'v',
# '%W%' => 'V',
#
# '%f{2,}%' => 'ff',
# '%o{2,}%' => 'oo',
# '%e{2,}%' => 'ee',
#
# '%([\.!\?])\s*(</[^>]+>)?\s*$%' => '$1 Bork Bork Bork!$2',

);

# Change the "Id:" RCS tag to show that we translated the file.
if (m/^#.*\$Id:/) {
    print "# language-MuppetsChef.sys, translated from language-English.sys\n";
    print "# Please do not edit this derived file.\n";
    next;
}
# Skip other comment lines
if (m/^#/) { next; }

# Split each incoming line by the '|' character
@pieces = split /\|/;

foreach my $test (@regexs) {

    @reg_parts = split /\:/, $test;

    # Modify the second portion of each line only
    $pieces[1] =~ s/$reg_parts[0]/$reg_parts[1]/;
}

# Combine the line again for output to STDOUT
print join '|', @pieces;


