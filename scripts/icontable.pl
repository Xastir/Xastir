#!/usr/bin/perl
# $Id$

# XASTIR icon table overview generator                        20.02.01
#  Copyright (C) 2001 Rolf Bleher                  http://www.dk7in.de

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

# This script produces an overview graphics with all symbols used by Xastir
# Output is as a XPM file to STDOUT
#     typical call:  icontable.pl > symbols.xpm

#--------------------------------------------------------------------------
					    
# symbols file from XASTIR V1.1, change the path for your environment
$SYMBFILE = "/usr/local/xastir/symbols/symbols.dat";

#--------------------------------------------------------------------------
%sympix = ();
$lasttable  = $table;
$lastsymbol = $symbol;
$tablist = "/\\";
@coltab = qw(#FFFF00 #CD6500 #A020F0 #CCCCCC #CD0000 #FF4040 #CD3333 #00008B #00BFFF #006400 #EE0000 #00CD00 #0000CD #FFFFFF #5A5A5A #878787 #454545 #000000 None);
setuppics();
#storepics();    # store all icons as separate XPM files
exit;
#--------------------------------------------------------------------------
sub setuppics {
  my $table  = '';
  my $symbol = '';
  my ($i,$j,$c);
  my $line;
  my $head;
  my $str;
  my $pix;
  my $pixstr;
  if (-f $SYMBFILE) {
    if (open(FH, "<$SYMBFILE")) {
      my %col = ();
SYM:  while(<FH>) {
        last if (/DONE/);
	if (/TABLE (.)/) {
	  $table = $1;
          if(length($_)>20) { $descr = 1 }
	  next;
	}
	if (/APRS (.)/) {
	  $symbol = $1;
	  next if ($table ne '/' && $table ne '\\');   # ignore other
	  $pixstr = '';
	  for ($i=0;$i<20;$i++) {
	    $line = <FH>;
	    $line =~ s/\n//;
	    $line =~ s/\r//;
	    next SYM if (length($line) != 20);
	    $pixstr .= $line;
	    for ($j=0;$j<20;$j++) {
	      $c = substr($line,$j,1);
	      $col{$c} = $c;
	    }
	  }
	  $sympix{$table.$symbol} = $pixstr;
	}
      }
      $str = "";
      for ($i=0;$i<20;$i++) {
        $str .= "....................";
      }
      $sympix{"  "} = $str;
  
      $head = ''; $j = 0;
      foreach $c (keys %col) {
        $head .= colstr($c);
        $j++;
      }
     $head = "/* XPM */{\"337 258 $j 1 \",\n".$head;

#      12 lines with 16 symbols each     x 337  y 

      $pix = "";
      foreach $table ("/","\\") {
        for ($i=2;$i<8;$i++) {                  # symbol row
	  $pix .=  "\"".("q" x 337)."\",\n";    # black hor line
	  for ($j=0;$j<20;$j++) {               # scan line
	    $pix .= "\"";                       # start of scan line
	    for ($k=0;$k<16;$k++) {             # symbol column
	      $pix .= "q";                      # vert line
	      $symbol = chr($i*16+$k);
	      $pix .= substr(getpic($table.$symbol),$j*20,20);
	    }
            $pix .= "q\",\n";                   # vert line
	  }
	}
	if ($table eq "\\") {           # {
	  $pix .=  "\"".("q" x 337)."\"};\n";      # black hor line
	} else {
	  $pix .=  "\"".("q" x 337)."\",\n";       # black hor line
	  for ($i=0;$i<4;$i++) {
	    $pix .=  "\"".("." x 337)."\",\n";     # hor space
	  }
	}
      }
      printf($head.$pix);
      close(FH);
    }
  }
}
#--------------------------------------------------------------------------
sub colstr {                                 # setup string for color
  my ($c) = @_;
  if ($c eq '#') {                           # Yellow
    $cidx = 0;
  } elsif ($c ge 'a' && $c le 'q') {
    $cidx = ord($c)-ord('a')+1;
  } else {
    $cidx = @coltab-1;                       # Transparent
  }
  return("\"$c c $coltab[$cidx]\",\n");
}
#--------------------------------------------------------------------------
sub getpic {
  my ($id) = @_;
  $str = $sympix{$id};
  if (! $str) {
    $str = $sympix{"  "};      # default
  }
  $str;
}
#--------------------------------------------------------------------------
sub storepics {                              # extract all icons to files
  foreach $cc (keys %sympix) {
    $fname = sprintf("Aprs%2.2X%2.2X.xpm",ord(substr($cc,0,1)),ord(substr($cc,1,1)));
    if (open(FH,">$fname")) {
      printf(FH "%s",getpic($cc));
      close(FH);
    }
  }
}
#--------------------------------------------------------------------------
