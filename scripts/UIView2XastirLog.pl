#!/usr/bin/perl -W
#
# $Id$
#
# Script to convert a UI-View log file to an Xastir log file.
#
# Copyright (C) 2009  The Xastir Group
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
# NOTE:  Run it against an input file and tell it where to put the
# output:
#
#  ./UIView2XastirLog.pl  <input_file.log  >output_file.log
#
# No timestamp info is converted or saved by this script.


while (<>) {

    chomp;

    if (m/START UTC/) {
        # Skip it
        next;
    }

    # If line starts with date field, chop off the date, time1,
    # time2 fields.
    #
    if (m/^\d\d\d\d\-\d\d\-\d\d\s.*/) {
        s/^\d\d\d\d\-\d\d\-\d\d\s+\d\d\:\d\d:\d\d\s+\d\d\:\d\d:\d\dR//;
        # Get rid of " <UI Len=19>"
        s/\s\<UI\sLen\=\d+\>:\r/:/;

        # Save current line
        $temp = $_;
   
        # Read next line 
        $_ = <>;
        chomp;

        # Concatenate the two
        $temp = $temp . $_ . "\n";

        print $temp;
    }
}


