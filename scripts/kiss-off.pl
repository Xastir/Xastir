#!/usr/bin/perl

# $Id$

# Copyright (C) 2004  Curt Mills, WE7U
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

# This script will send the proper characters to STDOUT to command a
# KISS TNC out of KISS mode.  Redirect it to the port that the TNC
# is connected to.  Turn off the port in Xastir first.

# It is assumed that the baud rate on the port and the baud rate of
# the TNC match, if not, this won't work.  If you've just been using
# the TNC in Xastir, they probably match.

# Use the script like this:
#
# ./kiss-off.pl >/dev/ttyS1
#


sleep 1;
printf("%c%c%c", 192, 255, 192);
sleep 1;

