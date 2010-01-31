#!/usr/bin/perl

# $Id$
#
# Script to create a slideshow from Xastir snapshot images.  Note
# that this script is Unix/Linux-centric due to the use of the $HOME
# variable and the use of the "cp" command.
#
# Written 20090415 by Curt Mills, WE7U
#
# Copyright (C) 2010  The Xastir Group
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
#
# Copies/renames ~/.xastir/tmp/snapshot.png files so that a
# slideshow can be created from the images at a later date.  The
# script monitors the input directory.  Any new snapshot.png file
# that appears is allowed to age for a few seconds to make sure that
# Xastir is done writing the file, then the file is copied to the
# ~/.xastir/tmp/slideshow/ directory as a numbered file.  See below
# for one method for turning these files into an animated GIF, or
# else bring them up in some slideshow program and display them in
# numerical order.  The name of the file contains the Unix Epoch
# timestamp (the exact second that the file was made).
#
#
# A note from Carl Makin, vk1kcm, modified slightly by Curt, we7u,
# regarding creation of an animated GIF from the slideshow images:
#
# -------------------------------------------------------------------
# This will create an animated gif that loops in your browser.
#   gm convert -delay 20 -loop snapshot*.png animation.gif
# See:
#   http://www.graphicsmagick.org/FAQ.html#how-do-i-create-a-gif-animation-sequence-to-display-within-netscape
# I think you can also great mpegs by changing the filename to "animation.mpg".
# -------------------------------------------------------------------
#


$home = $ENV{"HOME"};
$input_dir = "$home/.xastir/tmp";
$output_dir = "$input_dir/slideshow";


# Create the slideshow directory if it doesn't exist
mkdir $output_dir;

chdir $input_dir;

while (1) {
  $current_time = time;

  # Snag the age of the existing snapshot.png file
  $number = (stat("snapshot.png"))[9];

  $difference = $current_time - $number;

  # If the age of the file is more than 15 seconds but less than 31
  # minutes, copy the file to the slideshow directory and rename it
  # along the way.  Note that Xastir can produce snapshots at a rate
  # of between one per minute, and one per 30 minutes.
  #
  if ( ($difference > 15) && ($difference < (31 * 60)) ) {

    # Copy the snapshot image to the slideshow directory, renaming
    # it with the Unix Epoch time, something like: "1239897417.png".
    # Only copies the file if the source file is newer than the
    # destination, which prevents copying the file again and again
    # if it's the last file in your sequence.
    #
    `cp -u snapshot.png $output_dir/$number.png`;
  }
  sleep 30;
}


