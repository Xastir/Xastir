#!/usr/bin/perl -W

# $Id$

# Copyright (C) 2008-2010  The Xastir Group
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
#   ../scripts/langOldeEnglish.pl -split <language-English.sys >language-OldeEnglish.sys
# or
#   ../scripts/langOldeEnglish.pl <some-input-file >some-output-file
#
# "-split": Translate 2nd part of line only (Xastir language file).
# Without it:  Translate entire text.


# Regex strings derived from:
# http://www.faqs.org/docs/diveintopython/dialect_divein.html
# http://www.siafoo.net/snippet/133


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
    print "# language-OldeEnglish.sys, translated from language-English.sys\n";
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

  s/i([bcdfghjklmnpqrstvwxyz])e\b/y$1/g;
  s/i([bcdfghjklmnpqrstvwxyz])e/y$1$1e/g;
  s/ick\b/yk/g;
  s/ia([bcdfghjklmnpqrstvwxyz])/e$1e/g;
  s/e[ea]([bcdfghjklmnpqrstvwxyz])/e$1e/g;
  s/([bcdfghjklmnpqrstvwxyz])y/$1ee/g;
  s/([bcdfghjklmnpqrstvwxyz])er/$1re/g;
  s/([aeiou])re\b/$1r/g;
  s/ia([bcdfghjklmnpqrstvwxyz])/i$1e/g;
  s/tion\b/cioun/g;
  s/ion\b/ioun/g;
  s/aid/ayde/g;
  s/ai/ey/g;
  s/ay\b/y/g;
  s/ay/ey/g;
  s/ant/aunt/g;
  s/ea/ee/g;
  s/oa/oo/g;
  s/ue/e/g;
  s/oe/o/g;
  s/ou/ow/g;
  s/ow/ou/g;
  s/\bhe/hi/g;
  s/ve\b/veth/g;
  s/se\b/e/g;
  s/\'s\b/es/g;
  s/ic\b/ick/g;
  s/ics\b/icc/g;
  s/ical\b/ick/g;
  s/tle\b/til/g;
  s/ll\b/l/g;
  s/ould\b/olde/g;
  s/own\b/oune/g;
  s/un\b/onne/g;
  s/rry\b/rye/g;
  s/est\b/este/g;
  s/pt\b/pte/g;
  s/th\b/the/g;
  s/ch\b/che/g;
  s/ss\b/sse/g;
  s/([wybdp])\b/$1e/g;
  s/([rnt])\b/$1$1e/g;
  s/from/fro/g;
  s/when/whan/g;

  if ($do_split) {
    # Combine the line again for output to STDOUT
    $pieces[1] = $_;
    print join '|', @pieces;
  }
  else {
    print;
  }
}


