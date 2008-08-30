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
#   ../scripts/langOldeEnglish.pl <language-English.sys >language-OldeEnglish.sys
#
# If you would like, replace the language-English.sys file in the
# destination directory (usually "/usr/local/share/xastir/config")
# with this new file (renaming it to "language-English.sys" of
# course), then restart Xastir and you'll have it displaying in
# OldeEnglish!


# Regex strings derived from:
# http://www.faqs.org/docs/diveintopython/dialect_divein.html
# http://www.siafoo.net/snippet/133


my @regexs = (
  "i([bcdfghjklmnpqrstvwxyz])e\b:y$1",
  "i([bcdfghjklmnpqrstvwxyz])e:y$1$1e",
  "ick\b:yk",
  "ia([bcdfghjklmnpqrstvwxyz]):e$1e",
  "e[ea]([bcdfghjklmnpqrstvwxyz]):e$1e",
  "([bcdfghjklmnpqrstvwxyz])y:$1ee",
  "([bcdfghjklmnpqrstvwxyz])er:$1re",
  "([aeiou])re\b:$1r",
  "ia([bcdfghjklmnpqrstvwxyz]):i$1e",
  "tion\b:cioun",
  "ion\b:ioun",
  "aid:ayde",
  "ai:ey",
  "ay\b:y",
  "ay:ey",
  "ant:aunt",
  "ea:ee",
  "oa:oo",
  "ue:e",
  "oe:o",
  "ou:ow",
  "ow:ou",
  "\bhe:hi",
  "ve\b:veth",
  "se\b:e",
  "\'s\b:es",
  "ic\b:ick",
  "ics\b:icc",
  "ical\b:ick",
  "tle\b:til",
  "ll\b:l",
  "ould\b:olde",
  "own\b:oune",
  "un\b:onne",
  "rry\b:rye",
  "est\b:este",
  "pt\b:pte",
  "th\b:the",
  "ch\b:che",
  "ss\b:sse",
  "([wybdp])\b:$1e",
  "([rnt])\b:$1$1e",
  "from:fro",
  "when:whan"
);

# Change the "Id:" RCS tag to show that we translated the file.
if (m/^#.*\$Id:/) {
    print "# language-OldeEnglish.sys, translated from language-English.sys\n";
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


