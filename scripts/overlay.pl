#!/usr/bin/perl


# Copyright (C) 2004-2010 Curt Mills, WE7U
# Released to the public domain.
#
# $Id$


# Script to create Xastir "Overlay" files from "CSV" files of the
# proper format (comma-delimited files).
#
# 1) Creates files in Xastir "log" format if you enter a callsign
# below.  These files can then be put in your ~/.xastir/logs/
# directory and brought in via the File->Open Log File menu option.
# If you start with the CSV files in the ~/.xastir/logs/ directory
# and process them there with this script, the output files will be
# placed into the proper place for Xastir to find them.
#
# 2) If no callsign is entered, this script will create files in
# Xastir's "~/.xastir/config/object.log" format.  You can then
# replace or append the file to the object.log file, restart Xastir
# or "Reload Object/Item History".
#
# NOTE: These APRS Items will become part of your locally
# owned/transmitted objects, so if you don't want them transmitted,
# turn of Object/Item transmit before bringing them in.
#
# Input:  Directory name.  If no directory name passed in, it will
# operate on every CSV file in the current directory.
#
# Input format:
# Name  N/S  lat  E/W  long   //  comment fields.............
# SUPPLY,N,34.0000,W,78.0000,ICON,text1,text2,text3,text4,...
#
#
# The name will have spaces removed if it is longer than nine
# characters.  If it is still too long, vowels will be removed, then
# it will be truncated to nine characters if still needed.  The full
# name will be transmitted as a comment.  All other text fields will
# also be transmitted as comments, so that they will all appear in
# the Station Info dialog.
#
# Icons will be a default small red circle unless '/' or '\' is the
# leading character in that field and the next specifies the APRS
# symbol.  In that case the two-letter combination will get used as
# the symbol for the Item.



# Change this to match whatever callsign you're running Xastir as,
# so that the APRS Items appear to have been generated locally.  You
# can then suck this file in as a "log" file from within Xastir.  If
# this field is empty, then instead write the packets out without a
# header, as in Xastir's "object.log" format.
$callsign = "";

if ($callsign ne "") {
  $callsign = uc($callsign) . '>APRS:';
}



# Main program.  Process every ".csv" file found in the directory.
#
$dirname = shift;
if ($dirname == "") {
  $dirname = ".";
}

opendir(DIR, $dirname) or die "Can't opendir $dirname: %!";

while (defined($file = readdir(DIR))) {
  chomp $file;

  if ( $file =~ /\.csv$/ ) {

    # Do something with "$dirname/$file"
    &process_file($dirname, $file);
  }
  else {
#    printf("$dirname/$file\n");
  }
}



# Process one file.  Creates an output file that matches the
# basename but changes the "csv" to "overlay".
#
sub process_file() {

  $output_file = $_[1];
  $output_file =~ s/\.csv$/.overlay/;

  printf("$dirname/$file -> $output_file\n");
 
  open(SOURCE, "< $_[1]")
    or die "Couldn't open $path for reading: $!\n";

  open(OUTPUT, "> $output_file")
    or die "Couldn't open $output_file for writing: $!\n";
  
  while (<SOURCE>) {
    &process_line($_);
  }

  close(SOURCE);
  close(OUTPUT);
}



# Process one line.  Write the formatted data to the OUTPUT file.
#
sub process_line() {
  #printf("$_[0]");

  # Parse the CSV line into an array
  @list = parse_csv($_[0]);

  if (!($list[0] =~ m/Location/i)) {
    #print OUTPUT @list;

    # As a temporary measure, create items out of each of the lines,
    # with a status line for each extra data object so that they
    # appear in the Station Info dialog.
    &create_items(@list);
  }
}



# Create an APRS Item out of each array.  Create a status line for
# each extra column associated with a line so that the info shows up
# in the Station Info dialog.
#
# Examples.  Name is 3-9 characters:
# )AID #2!4903.50N/07201.75WA
# )G/WB4APR!53  .  N\002  .  Wd
#
sub create_items {

  $name = $_[0];
  if ($name eq "") {
    printf("Error, name column is empty\n");
  }

  # If too long, try removing spaces first
  if (length($name) > 9) {
    $name =~ s/\s//ig;
  }
 
  # If still too long, remove vowels
  if (length($name) > 9) {
    $name =~ s/a//ig;
    $name =~ s/e//ig;
    $name =~ s/i//ig;
    $name =~ s/o//ig;
    $name =~ s/u//ig;
  }
 
  # Extend to three characters if short
  if (length($name) < 3) {
    $name = $name . "   ";
  }

  $name = substr($name,0,9);
  $name[9] = "\0";  # Terminate name at 9 characters, minimum 3

  $n_s = uc( substr($_[1],0,1) );
  $latitude = $_[2];
  $e_w = uc( substr($_[3],0,1) );
  $longitude = $_[4];
  if ($_[5] =~ /ICON/i) {
    $icon1 = "/";
    $icon2 = "/";
  }
  else {
    $icon1 = substr($_[5],0,1);
    $icon2 = substr($_[5],1,1);
  }

  # Convert lat/long to APRS format (or Base-91 Compressed format)
  $lat_deg = $latitude;
  $lat_deg =~ s/\.\d+$//;
  $lat_deg = sprintf("%02d", $lat_deg);
  $lat_min = $latitude;
  $lat_min =~ s/^\d+\./0./;
  $lat_min = $lat_min * 60.0;
  $lon_deg = $longitude;
  $lon_deg =~ s/\.\d+$//;
  $lon_deg = sprintf("%03d", $lon_deg);
  $lon_min = $longitude;
  $lon_min =~ s/^\d+\./0./;
  $lon_min = $lon_min * 60;

  # Create an APRS "Item" packet
  $line = sprintf("%s)%s!%s%05.2f%s%s%s%05.2f%s%s",
    $callsign,
    $name,
    $lat_deg,
    $lat_min,
    $n_s,
    $icon1,
    $lon_deg,
    $lon_min,
    $e_w,
    $icon2);

  # Go process the rest of the columns, if any.  Create APRS Item
  # packets with comments from them.
  for ($i = 6; $i < 106; $i++) {
    chomp $_[$i];
    if ($_[$i] ne "") {
      #printf("$_[$i]");
      printf(OUTPUT "%s%s\n",
        $line,
        $_[$i]); 
    }
  }

  # Write it out to the file, with the full name as a comment
  printf(OUTPUT "%s%s\n",
    $line,
    $_[0]);
}



# Parse CVS line into an array, removing double-quotes and such if
# found.
#
sub parse_csv {
  my $text = shift; # Record containing comma-separated values
  my @new = ();
  push(@new, $+) while $text =~ m{
    # The first part groups the phrase inside the quotes.
    # See explanation of this pattern in MRE
    "([^\"\\]*(?:\\.[^\"\\]*)*)",?
      | ([^,]+),?
      |  ,
  }gx;
  push(@new, undef) if substr($text, -1, 1) eq ',';
  return @new;  # List of values that were comma-separated
}


