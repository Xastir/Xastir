#!/usr/bin/perl -w
# $Id$

# Update utility for XASTIR language files                  17.04.2001
#  Copyright (C) 2001 Rolf Bleher <Rolf@dk7in.de>  http://www.dk7in.de

#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#  see file COPYING for details
                                                                                
#--------------------------------------------------------------------------

# This program adds missing entries to the translated language files and
# deletes obsolete entries.
# If a translated comment before an entry block is found it will be preserved.
# There might be problems with the comment with a changed block sequence.
# I assume that you start it in the directory with the language files,
# call it with the translated language file as parameter.

# DK7IN: for now it assumes that the language files reside
#        in the current directory

#--------------------------------------------------------------------------
$LF1   = "language-English.sys";        # original language file
$LF2   = "language-German.sys";         # translated language file
$LFOUT = "language-new.sys";            # modified translated language file
if (@ARGV) {
    $LF2 = shift @ARGV;
}
#--------------------------------------------------------------------------
open(IN1,$LF1)      or die "ERROR: could not open $LF1!\n";
open(IN2,$LF2)      or die "ERROR: could not open $LF2!\n";
open(OUT,">$LFOUT") or die "ERROR: could not write to $LFOUT!\n";
#--------------------------------------------------------------------------
%token = ();                                    # translated tokens
@cmt1  = ();                                    # original comment
@cmt2  = ();                                    # translated comment

# collect all available translated tokens
while (<IN2>) {
    next if (/^#/);                             # skip comment lines
    if (/^([A-Z]+[0-9]+)(|.+|.?|.*)$/) {
        $token{$1} = $2;                        # store translated text entry
    }
}

# process language file
while (<IN1>) {                                 # original language file
    if (/^#/) {                                 # comment line
        push(@cmt1,$_);                         # store comment
    } else {
        if (/^([A-Z]+)([0-9]+)(|.+|.?|.*)$/) {   # data line
            $name = $1;
            $numb = $2;
            $arg  = $3;

            if (@cmt1) {                        # pending comment
                                                # try to find the translation
                seek IN2, 0, 0;                 # reposition to begin of file
                @cmt2  = ();                    # translated comment
                $match = 0;
                while (<IN2>) {
                    $line = $_;
                    if (/^([A-Z]+)([0-9]+)(|.+|.?|.*)$/) {
                        $curr = $1;
                        if ($name eq $curr) {   # same block
                            $match = 1;
                            last;
                        } else {
                            @cmt2 = ();         # clear wrong comment
                        }
                    } else {
                        if (/^#/) {             # store comment
                            push(@cmt2,$_);
                        }
                    }
                }
                if ($match && @cmt2) {          # found translated comment
                    foreach $line (@cmt2) {
                        print(OUT $line);       # translated comment
                    }
                } else {
                    foreach $line (@cmt1) {
                        print(OUT $line);       # original comment
                    }
                }
                @cmt1 = ();
            }

            if ($token{$name.$numb}) {          # found translation
                $arg = $token{$name.$numb};
            }
            printf(OUT "%s%s%s\n",$name,$numb,$arg);
        } else {
            print("ERROR: $_");                 # bad line format
        }
    }
}
close(IN1);
close(IN2);
close(OUT);

exit;
#--------------------------------------------------------------------------

