#!/usr/bin/perl -w


#
# Coordinate.pm:  Perl module for:
#
# 1) Creating and manipulating Coordinate objects,
# 2) Translating coordinates between UTM and Latitude/Longitude,
# 3) Translating coordinates between ~231 different datums,
# 4) Formatting coordinates into decimal degrees, degrees/minutes,
#    and degrees/minutes/seconds.
#
# Copyright (C) 2000-2003  Curt Mills, WE7U
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#


#
# $Id$
#
#
# Reference ellipsoids derived from Peter H. Dana's website- 
# New:  http://www.Colorado.EDU/geography/gcraft/notes/datum/elist.html
#
# Old:  http://www.utexas.edu/depts/grg/gcraft/notes/datum/elist.html
# Department of Geography, University of Texas at Austin
# Internet: pdana@mail.utexas.edu
# 3/22/95
#
#
# Reference datums derived from Peter H. Dana's website-
# http://www.Colorado.EDU/geography/gcraft/notes/datum/datum.html#GeoDat
#
#
# UTM<->LAT/LON translations were originally written in C++ by
# Chuck Gantz- chuck.gantz@globalstar.com.  Permission received
# by Chuck Gantz via e-mail to release it under the GPL license.
# Re-coded into Perl by Curt Mills.
#
# Datum translations were originally written in C by
# John F. Waers - jfwaers@csn.net.  His code was released to
# the public domain.  Re-coded into Perl by Curt Mills.
#
#
# TODO:
# -----
# Please note that I didn't pay a lot of attention to
# keeping the "double" notation in the form of higher
# precision floating point routines.  This means that
# the Perl5 code won't be as accurate as the original
# C-code.  It doesn't matter for my purposes.  If
# anyone converts to Math::BigFloat for higher precision,
# please send me the changes.  As it is I did a
# quick check and found a difference of only 1.4 meters
# between my Perl results and the results from a web-based
# datum-shift calculator on the 'net.  -- Curt.
#
# Should probably add return values for each method.  Check
# for error conditions and return 0 if problem.
#
# Add method to tell distance in meters of datum shift (verbose flag?)
#
# Get rid of print statements unless verbose or debug mode.
#
# POD documentation.
#


#------------------------------------------------------------------------------------------------


package CoordinateFormat;


$VERSION = do { my @r=(q$Revision$=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };


#
# Conversions between decimal degrees, degrees/decimal minutes, and
# degrees/minutes/seconds.  These are methods which all work on
# "CoordinateFormat" objects.
#
# These routines probably need a bit of work yet.  It'd be nice
# to be able to specify field widths.
#


sub new
{
  my $class = shift;			# What class are we constructing?
  my $self      = { };			# Allocate new memory
  bless $self, $class;			# Mark it of the right type
  $self->_init(@_);			# Call _init with remaining args
  return $self;
}


sub _init
{
  my $self = shift;
  $self->{RAW} = shift if @_;		# Input string
  $self->{FORMATTED} = shift if @_;	# Output string, formatted
}


sub raw					# Set'er/Get'er for input string
{
  my $self = shift;
  $self->{RAW} = shift if @_;
  return $self->{RAW};
}


sub formatted				# Set'er/Get'er for formatted string
{
  my $self = shift;
  $self->{FORMATTED} = shift if @_;
  return $self->{FORMATTED};
}


#
# Returns the version number (RCS version tag) of this module.
#
sub version
{
  return $VERSION;
}





#
# Degrees and Decimal Minutes
#
# Fills in the FORMATTED variable in the object and also returns
# the string.  If unsuccessful, returns an empty string.
#
sub degrees_minutes
{
  my $self = shift;				# Get the object to work on
  my $input = $self->raw();

  my ($degrees, $minutes, $seconds);

  if ( $input =~ /\d+\s+\d+\s+\d+\.*\d*/ )	# Input is in "DD MM SS.SS" format
  {
    $degrees = $minutes = $seconds = $input;
    $degrees =~ s/(\d+)\s+\d+\s+\d+\.*\d*/$1/;
    $minutes =~ s/\d+\s+(\d+)\s+\d+\.*\d*/$1/;
    $seconds =~ s/\d+\s+\d+\s+(\d+\.*\d*)/$1/;
    $minutes = $minutes + ($seconds / 60.0);
    $self->formatted( sprintf( "%02d %02.8f", $degrees, abs($minutes) ) );
    return( $self->formatted() );
  }
  elsif ( $input =~ /\d+\s+\d+\.*\d*/ )		# Input is in "DD MM.MM" format
  {
    $self->formatted( $input );			# No translation necessary
    return( $self->formatted() );
  }
  elsif ( $input =~ /\d+\.*\d*/ )		# Input is in "DD.DDDD" format
  {
    $degrees = int ($input);
    $minutes = ($input - $degrees) * 60.0;
    $self->formatted( sprintf( "%02d %02.8f", $degrees, abs($minutes) ) );
    return( $self->formatted() );
  }
  else
  {
    print "degrees_minutes() method: Input format not recognized: $input\n";
    $self->formatted( "" );
    return( "" );				# Should I die here instead?
  }
}





#
# Degrees/Minutes/Seconds
#
# Fills in the FORMATTED variable in the object and also returns
# the string.  If unsuccessful, returns an empty string.
#
sub degrees_minutes_seconds
{
  my $self = shift;				# Get the object to work on
  my $input = $self->raw();

  my ($degrees, $minutes, $seconds);

  if ( $input =~ /\d+\s+\d+\s+\d+\.*\d*/ )	# Input is in "DD MM SS.SS" format
  {
    $self->formatted( $input );			# No translation necessary
    return( $self->formatted() );
  }
  elsif ( $input =~ /\d+\s+\d+\.*\d*/ )		# Input is in "DD MM.MM" format
  {
    $degrees = $minutes = $input;
    $degrees =~ s/(\d+)\s+\d+\.*\d*/$1/;
    $minutes =~ s/\d+\s+(\d+\.*\d*)/$1/;
    $seconds = $minutes;
    $minutes = int ($minutes);
    $seconds = ($seconds - $minutes) * 60.0;
    $self->formatted( sprintf( "%02d %02d %02.8f", $degrees, abs($minutes), abs($seconds) ) );
    return( $self->formatted() );
  }
  elsif ( $input =~ /\d+\.*\d*/ )		# Input is in "DD.DDDD" format
  {
    $degrees = int ($input);
    $minutes = int ( ($input - $degrees) * 60.0 );
    $seconds = ( ( ($input - $degrees) * 60.0) - $minutes) * 60.0;
    $self->formatted( sprintf( "%02d %02d %02.8f", $degrees, abs($minutes), abs($seconds) ) );
    return( $self->formatted() );
  }
  else
  {
    print "degrees_minutes_seconds() method: Input format not recognized: $input\n";
    $self->formatted( "" );
    return( "" );				# Should I die here instead?
  }
}





#
# Decimal Degrees
#
# Fills in the FORMATTED variable in the object and also returns
# the string.  If unsuccessful, returns an empty string.
#
sub decimal_degrees
{
  my $self = shift;				# Get the object to work on
  my $input = $self->raw();

  my ($degrees, $minutes, $seconds);

  if ( $input =~ /\d+\s+\d+\s+\d+\.*\d*/ )	# Input is in "DD MM SS.SS" format
  {
    $degrees = $minutes = $seconds = $input;
    $degrees =~ s/(\d+)\s+\d+\s+\d+\.*\d*/$1/;
    $minutes =~ s/\d+\s+(\d+)\s+\d+\.*\d*/$1/;
    $seconds =~ s/\d+\s+\d+\s+(\d+\.*\d*)/$1/;
    $self->formatted( sprintf( "%02.8f", $degrees + ($minutes/60.0) + ($seconds/(60.0 * 60.0) ) ) );
    return( $self->formatted() );
  }
  elsif ( $input =~ /\d+\s+\d+\.*\d*/ )		# Input is in "DD MM.MM" format
  {
    $degrees = $minutes = $input;
    $degrees =~ s/(\d+)\s+\d+\.*\d*/$1/;
    $minutes =~ s/\d+\s+(\d+\.*\d*)/$1/;
    $self->formatted( sprintf( "%02.8f", $degrees + ($minutes / 60.0) ) );
    return( $self->formatted() );
  }
  elsif ( $input =~ /\d+\.*\d*/ )		# Input is in "DD.DDDD" format
  {
    $self->formatted( $input );			# No translation necessary
    return( $self->formatted() );
  }
  else
  {
    print "decimal_degrees() method: Input format not recognized: $input\n";
    $self->formatted( "" );
    return( "" );				# Should I die here instead?
  }
}


#------------------------------------------------------------------------------------------------


package Ellipsoid;


$VERSION = do { my @r=(q$Revision$=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };


#
# Ellipsoid: Equatorial_Radius is the ellipsoid semimajor axis (a)
#	     Polar_Radius is the ellipsoid semiminor axis (b).
#	     First_Ecc_Squared is the first eccentricity squared, defined as:  2f - (f^2)
#            Inverse_flattening (1/f) is the inverse of ellipsoid flattening 'f'
#	     Flattening (f) is defined as:  (a-b)/a.
#
# This object is used to create the Ellipsoid_Table object below,
# containing all of the parameters to describe the various
# ellipsoids that we need for datum and UTM<->Lat/Lon translations.
#
# Equatorial_Radius and First_Ecc_Squared are used by the UTM<->Lat/Lon code.
# Equatorial_Radius and Inverse_flattening are used by the datum_shift code.
#


sub new
{
  my $class = shift;		# What class are we constructing?
  my $self      = { };		# Allocate new memory
  bless $self, $class;		# Mark it of the right type
  $self->_init(@_);		# Call _init with remaining args
  return $self;
}


#
# Notice that we compute first_ecc_squared, flattening, and polar_radius
# from the other parameters.
#
sub _init
{
  my $self = shift;
  my $f;
  my $b;
  my $inverse_f;

  $self->{NAME} = shift if @_;					# Name of ellipsoid
  $a = $self->{EQUATORIAL_RADIUS} = shift if @_;		# Semi-major axis, meters
  $inverse_f = $self->{INVERSE_FLATTENING} = shift if @_;	# Inverse of the ellipsoid flattening 'f'

  if ($inverse_f)
  {
    $f = 1.0 / $inverse_f;
    $self->{FLATTENING} = $f;					# Store the flattening value as well
    $self->{FIRST_ECC_SQUARED}  = (2 * $f) - ($f**2);		# Compute First Eccentricity Squared from 'f'

    if ($a)
    {
      # f = (a-b)/a
      # f = a/a - b/a
      # f = 1 - b/a
      # b/a + f = 1
      # b/a = 1 - f
      # b = (1-f)a
      #
      $b = (1.0 - $f) * $a;
      $self->{POLAR_RADIUS} = $b;
    }
  }
}


#
# Returns the version number (RCS version tag) of this module.
#
sub version
{
  return $VERSION;
}


sub name
{
  my $self = shift;
  $self->{NAME} = shift if @_;
  return $self->{NAME};
}


sub equatorial_radius
{
  my $self = shift;
  $self->{EQUATORIAL_RADIUS} = shift if @_;
  return $self->{EQUATORIAL_RADIUS};
}


#
# Note that you can override the computed POLAR_RADIUS value
# by using this method to store a new value in the hash.
#
sub polar_radius
{
  my $self = shift;
  $self->{POLAR_RADIUS} = shift if @_;
  return $self->{POLAR_RADIUS};
}


#
# Note that you can override the computed FIRST_ECC_SQUARED value
# by using this method to store a new value in the hash.
#
sub first_ecc_squared
{
  my $self = shift;
  $self->{FIRST_ECC_SQUARED} = shift if @_;
  return $self->{FIRST_ECC_SQUARED};
}


#
# Note that you can override the computed FLATTENING value
# by using this method to store a new value in the hash.
#
sub flattening
{
  my $self = shift;
  $self->{FLATTENING} = shift if @_;
  return $self->{FLATTENING};
}


sub inverse_flattening
{
  my $self = shift;
  $self->{INVERSE_FLATTENING} = shift if @_;
  return $self->{INVERSE_FLATTENING};
}


#-----------------------------------------------------------------------------------------------


package EllipsoidTable;


$VERSION = do { my @r=(q$Revision$=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };


#
# I could have created this table as a hash of anonymous arrays, which might have been
# faster/more efficient.  Oh well.
#
# All of the methods here are class methods.  There isn't even a way to make an
# EllipsoidTable object given here.  This class is merely a large collection
# of Ellipsoid objects, collected in the %_ellipsoid hash.
#


my %_ellipsoid;		# Class data


#
# Returns the version number (RCS version tag) of this module.
#
sub version
{
  return $VERSION;
}


# Note that this method is really useless.  The name is the same as the
# hash key used in the %_ellipsoid hash.  Redundant.
#
sub name
{
  shift;	# Object reference (don't need it)
  my $ellipsoid_name = shift;

  return $_ellipsoid{$ellipsoid_name}->name();
}


sub equatorial_radius	# (a)
{
  shift;	# Object reference (don't need it)
  my $ellipsoid_name = shift;

  return $_ellipsoid{$ellipsoid_name}->equatorial_radius();
}


sub polar_radius	# (b)
{
  shift;	# Object reference (don't need it)
  my $ellipsoid_name = shift;

  return $_ellipsoid{$ellipsoid_name}->polar_radius();
}


sub first_ecc_squared	# (ecc)
{
  shift;	# Object reference (don't need it)
  my $ellipsoid_name = shift;

  return $_ellipsoid{$ellipsoid_name}->first_ecc_squared();
}


sub flattening		# (f)
{
  shift;	# Object reference (don't need it)
  my $ellipsoid_name = shift;

  return $_ellipsoid{$ellipsoid_name}->flattening();
}


sub inverse_flattening	# (1/f)
{
  shift;	# Object reference (don't need it)
  my $ellipsoid_name = shift;

  return $_ellipsoid{$ellipsoid_name}->inverse_flattening();
}


#
# This method allows printing out each defined Ellipsoid.
#
sub enumerate
{
  my $self = shift;

  print "\nEllipsoid\t\tEquatorial Radius (a)\tPolar Radius (b)\tFirst Eccentricity^2\tFlattening (f)\t\t1/f\n";
  print   "---------\t\t---------------------\t----------------\t--------------------\t--------------\t\t---\n";

  foreach my $key (sort keys %_ellipsoid)
  {
    printf("%23s,\t%s,\t%s,\t%s,\t%s,\t%s\n",
	$key,
	$self->equatorial_radius($key),
	$self->polar_radius($key),
	$self->first_ecc_squared($key),
	$self->flattening($key),
	$self->inverse_flattening($key) );
  }
  return(1);
}





#
# Auto-run code
#
# Fill in the hash containing Ellipsoid objects (fill in the Class data)
#
# Name, Equatorial_Radius (a), Inverse_Flattening (1/f).
#
# Name:				Reference Ellipsoid Name
# Equatorial_Radius:		(a) = Semi-Major Axis		(WGS-84 value = 6378137.0 meters)
# Polar_radius:			(b) = Semi-Minor Axis		(WGS-84 value = 6356752.3142 meters)
# Flattening:			f = (a-b)/a			(WGS-84 value = 0.00335281066475)
# First_Eccentricity_Squared:	ecc = 2f - (f^2)		(WGS-84 value = 0.00669437999013)
# Inverse_flattening:		Reciprocal Flattening (1/f)	(WGS-84 value = 298.257223563)
#
# We can calculate Polar_Radius (b), Flattening (f), and First_Eccentricity_Squared (ecc)
# from the values for Equatorial_Radius (a) and Inverse_flattening (1/f), so the table
# includes only the latter two parameters.
#


#print "Creating ellipsoid data\n";
#                                                                                   Equatorial
#            Name                                          Name                     Radius       Inverse_flattening
$_ellipsoid{"Airy"}			= Ellipsoid->new( "Airy",		    6377563.396, 299.324964600 );
$_ellipsoid{"Modified Airy"}          	= Ellipsoid->new( "Modified Airy",	    6377340.189, 299.324964600 );
$_ellipsoid{"Australian National"}	= Ellipsoid->new( "Australian National",    6378160.000, 298.250000000 );
$_ellipsoid{"Bessel 1841"}		= Ellipsoid->new( "Bessel 1841",	    6377397.155, 299.152812800 );
$_ellipsoid{"Bessel 1841 (Namibia)"}	= Ellipsoid->new( "Bessel 1841 (Namibia)",  6377483.865, 299.152812800 );
$_ellipsoid{"Clarke 1866"}		= Ellipsoid->new( "Clarke 1866",	    6378206.400, 294.978698200 );
$_ellipsoid{"Clarke 1880"}		= Ellipsoid->new( "Clarke 1880",	    6378249.145, 293.465000000 );
$_ellipsoid{"Everest (India 1830)"}	= Ellipsoid->new( "Everest (India 1830)",   6377276.345, 300.801700000 );
$_ellipsoid{"Everest (Sabah Sarawak)"}	= Ellipsoid->new( "Everest (Sabah Sarawak)",6377298.556, 300.801700000 ); 
$_ellipsoid{"Everest (India 1956)"}	= Ellipsoid->new( "Everest (India 1956)",   6377301.243, 300.801700000 );
$_ellipsoid{"Everest (Malaysia 1969)"}	= Ellipsoid->new( "Everest (Malaysia 1969)",6377295.664, 300.801700000 );
$_ellipsoid{"Everest (Malay. & Sing)"}	= Ellipsoid->new( "Everest (Malay. & Sing)",6377304.063, 300.801700000 );
$_ellipsoid{"Everest 1948"}		= Ellipsoid->new( "Everest 1948",           6377304.063, 300.801700000 );
$_ellipsoid{"Everest (Pakistan)"}	= Ellipsoid->new( "Everest (Pakistan)",	    6377309.613, 300.801700000 );
$_ellipsoid{"Fischer 1960 (Mercury)"}	= Ellipsoid->new( "Fischer 1960 (Mercury)", 6378166.000, 298.300000000 );
$_ellipsoid{"Fischer 1968"}		= Ellipsoid->new( "Fischer 1968",	    6378150.000, 298.300000000 );
$_ellipsoid{"Modified Fischer 1960"}	= Ellipsoid->new( "Modified Fischer 1960",  6378155.000, 298.300000000 );
$_ellipsoid{"GRS 1967"}			= Ellipsoid->new( "GRS 1967",		    6378160.000, 298.247167427 );
$_ellipsoid{"GRS 1980"}			= Ellipsoid->new( "GRS 1980",		    6378137.000, 298.257222101 );
$_ellipsoid{"Helmert 1906"}		= Ellipsoid->new( "Helmert 1906",	    6378200.000, 298.300000000 );
$_ellipsoid{"Hough 1960"}		= Ellipsoid->new( "Hough 1960",		    6378270.000, 297.000000000 );
$_ellipsoid{"Indonesian 1974"}		= Ellipsoid->new( "Indonesian 1974",	    6378160.000, 298.247000000 );
$_ellipsoid{"International 1924"}	= Ellipsoid->new( "International 1924",	    6378388.000, 297.000000000 );
$_ellipsoid{"Krassovsky 1940"}		= Ellipsoid->new( "Krassovsky 1940",	    6378245.000, 298.300000000 );
$_ellipsoid{"South American 1969"}	= Ellipsoid->new( "South American 1969",    6378160.000, 298.250000000 );
$_ellipsoid{"WGS 60"}			= Ellipsoid->new( "WGS 60",		    6378165.000, 298.300000000 );
$_ellipsoid{"WGS 66"}			= Ellipsoid->new( "WGS 66",		    6378145.000, 298.250000000 );
$_ellipsoid{"WGS 72"}			= Ellipsoid->new( "WGS 72",		    6378135.000, 298.260000000 );
$_ellipsoid{"WGS 84"}			= Ellipsoid->new( "WGS 84",		    6378137.000, 298.257223563 );


#------------------------------------------------------------------------------------------------


package Datum;


$VERSION = do { my @r=(q$Revision$=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };


#
# These are the objects and methods used to create the DatumTable in the next package.
#


sub new
{
  my $class = shift;		# What class are we constructing?
  my $self      = { };		# Allocate new memory
  bless $self, $class;		# Mark it of the right type
  $self->_init(@_);		# Call _init with remaining args
  return $self;
}


sub _init
{
  my $self = shift;
  $self->{NAME} = shift if @_;
  $self->{ELLIPSOID} = shift if @_;
  $self->{DX} = shift if @_;
  $self->{DY} = shift if @_;
  $self->{DZ} = shift if @_;
}


#
# Returns the version number (RCS version tag) of this module.
#
sub version
{
  return $VERSION;
}


#
# This is mostly a useless method.  We store the name here, but it
# is also present as the hash key in the %_datum hash.  Redundant.
#
sub name			# Name of datum
{
  my $self = shift;
  $self->{NAME} = shift if @_;
  return $self->{NAME};
}


sub ellipsoid			# Name of ellipsoid used in datum
{
  my $self = shift;
  $self->{ELLIPSOID} = shift if @_;
  return $self->{ELLIPSOID};
}


sub dx				# X-offset from WGS84 ellipsoid center
{
  my $self = shift;
  $self->{DX} = shift if @_;
  return $self->{DX};
}


sub dy				# Y-offset from WGS84 ellipsoid center
{
  my $self = shift;
  $self->{DY} = shift if @_;
  return $self->{DY};
}


sub dz				# Z-offset from WGS84 ellipsoid center
{
  my $self = shift;
  $self->{DZ} = shift if @_;
  return $self->{DZ};
}


#------------------------------------------------------------------------------------------------


package DatumTable;


$VERSION = do { my @r=(q$Revision$=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };


#
# I could have created this table as a hash of anonymous arrays, which might have been
# faster/more efficient.  Oh well.
#
# All of the methods here are class methods.  There isn't even a way to make a
# DatumTable object given here.  This class is merely a large collection
# of Datum objects, collected in the %_datum hash.
#

my %_datum;			# Class data


#
# Returns the version number (RCS version tag) of this module.
#
sub version
{
  return $VERSION;
}


sub name				# Name of datum
{
  shift;				# Object reference (don't need it)
  my $datum_name = shift;

  return $_datum{$datum_name}->name();
}


sub ellipsoid				# Name of ellipsoid used in datum
{
  shift;				# Object reference (don't need it)
  my $datum_name = shift;

  return $_datum{$datum_name}->ellipsoid();
}


sub dx					# X-offset from WGS84 ellipsoid center
{
  shift;				# Object reference (don't need it)
  my $datum_name = shift;

  return $_datum{$datum_name}->dx();
}


sub dy					# Y-offset from WGS84 ellipsoid center
{
  shift;				# Object reference (don't need it)
  my $datum_name = shift;

  return $_datum{$datum_name}->dy();
}


sub dz					# Z-offset from WGS84 ellipsoid center
{
  shift;				# Object reference (don't need it)
  my $datum_name = shift;

  return $_datum{$datum_name}->dz();
}


#
# This method allows printing out each defined Datum.
#
sub enumerate
{
  my $self = shift;

  print "\nDatum\t\t\t\t\t\t\t\tEllipsoid\tDx\tDy\tDz\n";
  print   "-----\t\t\t\t\t\t\t\t---------\t--\t--\t--\n";

  foreach my $key (sort keys %_datum)
  {
    printf("%s\n\t\t\t\t\t\t%23s,\t%s,\t%s,\t%s\n",
	$key,
	$self->ellipsoid($key),
	$self->dx($key),
	$self->dy($key),
	$self->dz($key) );
  }
  return(1);
}





#
# Auto-run code
#
# Fill in the Class data.
#
# This code fills in the %_datum hash which has entries consisting of Datum objects.
#
# From the original C code:
#
# "Dx, Dy, Dz: ellipsoid center with respect to WGS 84 ellipsoid center
#  x axis is the prime meridian
#  y axis is 90 degrees east longitude
#  z axis is the axis of rotation of the ellipsoid"
#
# Most of the current data is from Peter Dana's website.
# This increased the number of datums to around 231.  -- Curt.
#
#
# NOTE:  Consider adding a field for "region of use".
#
#
#	 Name	Name	Ellipsoid	Dx	Dy	Dz
$_datum{"Adindan (Burkina Faso)"}
	= Datum->new( "Adindan (Burkina Faso)",
			"Clarke 1880", -118, -14, 218 );

$_datum{"Adindan (Cameroon)"}
	= Datum->new( "Adindan (Cameroon)",
			"Clarke 1880", -134, -2, 210 );

$_datum{"Adindan (Ethiopia)"}
	= Datum->new( "Adindan (Ethiopia)",
			"Clarke 1880", -165, -11, 206 );

$_datum{"Adindan (Mali)"}
	= Datum->new( "Adindan (Mali)",
			"Clarke 1880", -123, -20, 220 );

$_datum{"Adindan (MEAN for Ethiopia/Sudan)"}
	= Datum->new( "Adindan (MEAN for Ethiopia/Sudan)",
			"Clarke 1880", -166, -15, 204 );

$_datum{"Adindan (Senegal)"}
	= Datum->new( "Adindan (Senegal)",
			"Clarke 1880", -128, -18, 224 );

$_datum{"Adindan (Sudan)"}
	= Datum->new( "Adindan (Sudan)",
			"Clarke 1880", -161, -14, 205 );

$_datum{"Afgooye"}
	= Datum->new( "Afgooye",
			"Krassovsky 1940", -43, -163, 45 );

$_datum{"Ain el Abd 1970 (Bahrain)"}
	= Datum->new( "Ain el Abd 1970 (Bahrain)",
			"International 1924", -150, -250, -1 );

$_datum{"Ain el Abd 1970 (Saudi Arabia)"}
	= Datum->new( "Ain el Abd 1970 (Saudi Arabia)",
			"International 1924", -143, -236, 7 );

$_datum{"American Samoa 1962"}
	= Datum->new( "American Samoa 1962",
			"Clarke 1866", -115, 118, 426 );

$_datum{"Anna 1 Astro 1965"}
	= Datum->new( "Anna 1 Astro 1965",
			"Australian National", -491, -22, 435 );

$_datum{"Antigua Island Astro 1943"}
	= Datum->new( "Antigua Island Astro 1943",
			"Clarke 1880", -270, 13, 62 );

$_datum{"Arc 1950 (Botswana)"}
	= Datum->new( "Arc 1950 (Botswana)",
			"Clarke 1880", -138, -105, -289 );

$_datum{"Arc 1950 (Burundi)"}
	= Datum->new( "Arc 1950 (Burundi)",
			"Clarke 1880", -153, -5, -292 );

$_datum{"Arc 1950 (Lesotho)"}
	= Datum->new( "Arc 1950 (Lesotho)",
			"Clarke 1880", -125, -108, -295 );

$_datum{"Arc 1950 (Malawi)"}
	= Datum->new( "Arc 1950 (Malawi)",
			"Clarke 1880", -161, -73, -317 );

$_datum{"Arc 1950 (MEAN)"}
	= Datum->new( "Arc 1950 (MEAN)",
			"Clarke 1880", -143, -90, -294 );

$_datum{"Arc 1950 (Swaziland)"}
	= Datum->new( "Arc 1950",
			"Clarke 1880", -134, -105, -295 );

$_datum{"Arc 1950 (Zaire)"}
	= Datum->new( "Arc 1950",
			"Clarke 1880", -169, -19, -278 );

$_datum{"Arc 1950 (Zambia)"}
	= Datum->new( "Arc 1950",
			"Clarke 1880", -147, -74, -283 );

$_datum{"Arc 1950 (Zimbabwe)"}
	= Datum->new( "Arc 1950",
			"Clarke 1880", -142, -96, -293 );

$_datum{"Arc 1960 (MEAN)"}
	= Datum->new( "Arc 1960 (MEAN)",
			"Clarke 1880", -160, -6, -302 );

$_datum{"Arc 1960 (Kenya)"}
	= Datum->new( "Arc 1960 (Kenya)",
			"Clarke 1880", -157, -2, -299 );

$_datum{"Arc 1960 (Tanzania)"}
	= Datum->new( "Arc 1960 (Tanzania)",
			"Clarke 1880", -175, -23, -303 );

$_datum{"Ascension Island 1958"}
	= Datum->new( "Ascension Island 1958",
			"International 1924", -205, 107, 53 );

$_datum{"Astro B4 Sorol Atoll"}
	= Datum->new( "Astro B4 Sorol Atoll",
			"International 1924", 114, -116, -333 );

$_datum{"Astro Beacon E 1945"}
	= Datum->new( "Astro Beacon E 1945",
			"International 1924", 145, 75, -272 );

$_datum{"Astro DOS 71/4"}
	= Datum->new( "Astro DOS 71/4",
			"International 1924", -320, 550, -494 );

$_datum{"Astro Tern Island (FRIG) 1961"}
	= Datum->new( "Astro Tern Island (FRIG) 1961",
			"International 1924", 114, -116, -333 );

$_datum{"Astronomical Station 1952"}
	= Datum->new( "Astronomical Station 1952",
			"International 1924", 124, -234, -25 );

$_datum{"Australian Geodetic 1966"}
	= Datum->new( "Australian Geodetic 1966",
			"Australian National", -133, -48, 148 );

$_datum{"Australian Geodetic 1984"}
	= Datum->new( "Australian Geodetic 1984",
			"Australian National", -134, -48, 149 );

$_datum{"Ayabelle Lighthouse"}
	= Datum->new( "Ayabelle Lighthouse",
			"Clarke 1880", -79, -129, 145 );

$_datum{"Bellevue (IGN)"}
	= Datum->new( "Bellevue (IGN)",
			"International 1924", -127, -769, 472 );

$_datum{"Bermuda 1957"}
	= Datum->new( "Bermuda 1957",
			"Clarke 1866", -73, 213, 296 );

$_datum{"Bogota Observatory"}
	= Datum->new( "Bogota Observatory",
			"International 1924", 307, 304, -318 );

$_datum{"Bukit Rimpah"}
	= Datum->new( "Bukit Rimpah",
			"Bessel 1841", -384, 664, -48 );

$_datum{"Camp Area Astro"}
	= Datum->new( "Camp Area Astro",
			"International 1924", -104, -129, 239 );

$_datum{"Campo Inchauspe"}
	= Datum->new( "Campo Inchauspe",
			"International 1924", -148, 136, 90 );

$_datum{"Canton Astro 1966"}
	= Datum->new( "Canton Astro 1966",
			"International 1924", 298, -304, -375 );

$_datum{"Cape"}
	= Datum->new( "Cape",
			"Clarke 1880", -136, -108, -292 );

$_datum{"Cape Canaveral"}
	= Datum->new( "Cape Canaveral",
			"Clarke 1866", -2, 151, 181 );

$_datum{"Carthage"}
	= Datum->new( "Carthage",
			"Clarke 1880", -263, 6, 431 );

$_datum{"CH-1903"}
	= Datum->new( "CH-1903",
			"Bessel 1841", 674, 15, 405);

$_datum{"Chatham Island Astro 1971"}
	= Datum->new( "Chatham Island Astro 1971",
			"International 1924", 175, -38, 113);

$_datum{"Chua Astro"}
	= Datum->new( "Chua Astro",
			"International 1924", -134, 229, -29 );

$_datum{"Corrego Alegre"}
	= Datum->new( "Corrego Alegre",
			"International 1924", -206, 172, -6 );

$_datum{"Dabola"}
	= Datum->new( "Dabola",
			"Clarke 1880", -83, 37, 124 );

$_datum{"Deception Island"}
	= Datum->new( "Deception Island",
			"Clarke 1880", 260, 12, -147 );

$_datum{"Djakarta (Batavia)"}
	= Datum->new( "Djakarta (Batavia)",
			"Bessel 1841", -377, 681, -50 );

$_datum{"DOS 1968"}
	= Datum->new( "DOS 1968",
			"International 1924", 230, -199, -752 );

$_datum{"Easter Island 1967"}
	= Datum->new( "Easter Island 1967",
			"International 1924", 211, 147, 111 );

$_datum{"Estonia 1937"}
	= Datum->new( "Estonia 1937",
			"Bessel 1841", 374, 150, 588 );

$_datum{"European 1950 (Cyprus)"}
	= Datum->new( "European 1950 (Cyprus)",
			"International 1924", -104, -101, -140 );

$_datum{"European 1950 (Egypt)"}
	= Datum->new( "European 1950 (Egypt)",
			"International 1924", -130, -117, -151 );

$_datum{"European 1950 (England/Channel Is/Scotland/Shetland Is)"}
	= Datum->new( "European 1950 (England/Channel Is/Scotland/Shetland Is)",
			"International 1924", -86, -96, -120 );

$_datum{"European 1950 (England/Ireland/Scotland/Shetland Is)"}
	= Datum->new( "European 1950 (England/Ireland/Scotland/Shetland Is)",
			"International 1924", -86, -96, -120 );

$_datum{"European 1950 (Finland/Norway)"}
	= Datum->new( "European 1950 (Finland/Norway)",
			"International 1924", -87, -95, -120 );

$_datum{"European 1950 (Greece)"}
	= Datum->new( "European 1950 (Greece)",
			"International 1924", -84, -95, -130 );

$_datum{"European 1950 (Iran)"}
	= Datum->new( "European 1950 (Iran)",
			"International 1924", -117, -132, -164 );

$_datum{"European 1950 (Italy:Sardinia)"}
	= Datum->new( "European 1950 (Italy:Sardinia)",
			"International 1924", -97, -103, -120 );

$_datum{"European 1950 (Italy:Sicily)"}
	= Datum->new( "European 1950 (Italy:Sicily)",
			"International 1924", -97, -88, -135 );

$_datum{"European 1950 (Malta)"}
	= Datum->new( "European 1950 (Malta)",
			"International 1924", -107, -88, -149 );

$_datum{"European 1950 (MEAN for Austria/Belgium/Denmark/Finland/France/W Germany/Gibraltar/Greece/Italy/Luxembourg/Netherlands/Norway/Portugal/Spain/Sweden/Switzerland)"}
	= Datum->new( "European 1950 (MEAN for Austria/Belgium/Denmark/Finland/France/W Germany/Gibraltar/Greece/Italy/Luxembourg/Netherlands/Norway/Portugal/Spain/Sweden/Switzerland)",
			"International 1924", -87, -98, -121 );

$_datum{"European 1950 (MEAN for Austria/Denmark/France/W Germany/Netherlands/Switzerland)"}
	= Datum->new( "European 1950 (MEAN for Austria/Denmark/France/W Germany/Netherlands/Switzerland)",
			"International 1924", -87, -96, -120 );

$_datum{"European 1950 (MEAN for Iraq/Israel/Jordan/Lebanon/Kuwait/Saudi Arabia/Syria)"}
	= Datum->new( "European 1950 (MEAN for Iraq/Israel/Jordan/Lebanon/Kuwait/Saudi Arabia/Syria)",
			"International 1924", -103, -106, -141 );

$_datum{"European 1950 (Portugal/Spain)"}
	= Datum->new( "European 1950 (Portugal/Spain)",
			"International 1924", -84, -107, -120 );

$_datum{"European 1950 (Tunisia)"}
	= Datum->new( "European 1950 (Tunisia)",
			"International 1924", -112, -77, -145 );

$_datum{"European 1979 (MEAN for Austria/Finland/Netherlands/Norway/Spain/Sweden/Switzerland)"}
	= Datum->new( "European 1979 (MEAN for Austria/Finland/Netherlands/Norway/Spain/Sweden/Switzerland)",
			"International 1924",  -86, -98, -119 );

$_datum{"Finland Hayford"}
	= Datum->new( "Finland Hayford",
			"International 1924", -78, -231, -97 );

$_datum{"Fort Thomas"}
	= Datum->new( "Fort Thomas",
			"Clarke 1880", -7, 215, 225 );

$_datum{"Gandajika Base 1970"}
	= Datum->new( "Gandajika Base 1970",
			"International 1924", -133, -321, 50 );

$_datum{"Geodetic Datum 1949"}
	= Datum->new( "Geodetic Datum 1949",
			"International 1924", 84, -22, 209 );

$_datum{"Graciosa Base SW 1948"}
	= Datum->new( "Graciosa Base SW 1948",
			"International 1924", -104, 167, -38 );

$_datum{"Guam 1963"}
	= Datum->new( "Guam 1963",
			"Clarke 1866", -100, -248, 259 );

$_datum{"Gunung Segara"}
	= Datum->new( "Gunung Segara",
			"Bessel 1841", -403, 684, 41 );

$_datum{"GUX 1 Astro"}
	= Datum->new( "GUX 1 Astro",
			"International 1924", 252, -209, -751 );

$_datum{"Herat North"}
	= Datum->new( "Herat North",
			"International 1924", -333, -222, 114 );

$_datum{"Hermannskogel Datum (Namibia)"}
	= Datum->new( "Hermannskogel Datum (Namibia)",
			"Bessel 1841", 653, -212, 449 );

$_datum{"Hjorsey 1955"}
	= Datum->new( "Hjorsey 1955",
			"International 1924", -73, 46, -86 );

$_datum{"Hong Kong 1963"}
	= Datum->new( "Hong Kong 1963",
			"International 1924", -156, -271, -189 );

$_datum{"Hu-Tzu-Shan"}
	= Datum->new( "Hu-Tzu-Shan",
			"International 1924", -637, -549, -203 );

$_datum{"Indian (Bangladesh)"}
	= Datum->new( "Indian (Bangladesh)",
			"Everest (India 1830)", 282, 726, 254 );

$_datum{"Indian (India/Nepal)"}
	= Datum->new( "Indian (India/Nepal)",
			"Everest (India 1956)", 295, 736, 257 );

$_datum{"Indian (Pakistan)"}
	= Datum->new( "Indian (Pakistan)",
			"Everest (Pakistan)", 283, 682, 231 );

$_datum{"Indian 1954 (Thailand)"}
	= Datum->new( "Indian 1954 (Thailand)",
			"Everest (India 1830)", 217, 823, 299 );

$_datum{"Indian 1960 (Vietnam:Con Son Is)"}
	= Datum->new( "Indian 1960 (Vietnam:Con Son Is)",
			"Everest (India 1830)", 182, 915, 344 );

$_datum{"Indian 1960 (Vietnam:Near 160N)"}
	= Datum->new( "Indian 1960 (Vietnam:Near 160N)",
			"Everest (India 1830)", 198, 881, 317 );

$_datum{"Indian 1975 (Thailand)"}
	= Datum->new( "Indian 1975 (Thailand)",
			"Everest (India 1830)", 210, 814, 289 );

$_datum{"Indonesian 1974"}
	= Datum->new( "Indonesian 1974)",
			"Indonesian 1974)", -24, -15, 5 );

$_datum{"Ireland 1965"}
	= Datum->new( "Ireland 1965",
			"Modified Airy", 506, -122, 611 );

$_datum{"ISTS 061 Astro 1968"}
	= Datum->new( "ISTS 061 Astro 1968",
			"International 1924", -794, 119, -298 );

$_datum{"ISTS 073 Astro 1969"}
	= Datum->new( "ISTS 073 Astro 1969",
			"International 1924", 208, -435, -229 );

$_datum{"Johnston Island"}
	= Datum->new( "Johnston Island",
			"International 1924", 189, -79, -202 );

$_datum{"Kandawala"}
	= Datum->new( "Kandawala",
			"Everest (India 1830)", -97, 787, 86 );

$_datum{"Kerguelen Island 1949"}
	= Datum->new( "Kerguelen Island 1949",
			"International 1924", 145, -187, 103 );

$_datum{"Kertau 1948"}
	= Datum->new( "Kertau 1948",
			"Everest (Malay. & Sing)", -11, 851, 5 );

$_datum{"Kusaie Astro 1951"}
	= Datum->new( "Kusaie Astro 1951",
			"International 1924", 647, 1777, -1124 );

$_datum{"Korean Geodetic System"}
	= Datum->new( "Korean Geodetic System",
			"GRS 1980", 0, 0, 0 );

$_datum{"L. C. 5 Astro 1961"}
	= Datum->new( "L. C. 5 Astro 1961",
			"Clarke 1866", 42, 124, 147 );

$_datum{"Leigon"}
	= Datum->new( "Leigon",
			"Clarke 1880", -130, 29, 364 );

$_datum{"Liberia 1964"}
	= Datum->new( "Liberia 1964",
			"Clarke 1880", -90, 40, 88 );

$_datum{"Luzon (Philippines:Except Mindanao)"}
	= Datum->new( "Luzon (Philippines:Except Mindanao)",
			"Clarke 1866", -133, -77, -51 );

$_datum{"Luzon (Philippines:Mindanao)"}
	= Datum->new( "Luzon (Philippines:Mindanao)",
			"Clarke 1866", -133, -79, -72 );

$_datum{"M\'Poraloko"}
	= Datum->new( "M\'Poraloko",
			"Clarke 1880", -74, -130, 42 );

$_datum{"Mahe 1971"}
	= Datum->new( "Mahe 1971",
			"Clarke 1880", 41, -220, -134 );

$_datum{"Marco Astro"}
	= Datum->new( "Marco Astro",
			"International 1924", -289, -124, 60 );

$_datum{"Massawa"}
	= Datum->new( "Massawa",
			"Bessel 1841",  639, 405, 60 );

$_datum{"Merchich"}
	= Datum->new( "Merchich",
			"Clarke 1880", 31, 146, 47 );

$_datum{"Midway Astro 1961"}
	= Datum->new( "Midway Astro 1961",
			"International 1924", 912, -58, 1227 );

$_datum{"Minna (Cameroon)"}
	= Datum->new( "Minna (Cameroon)",
			"Clarke 1880", -81, -84, 115 );

$_datum{"Minna (Nigeria)"}
	= Datum->new( "Minna (Nigeria)",
			"Clarke 1880", -92, -93, 122 );

$_datum{"Montserrat Island Astro 1958"}
	= Datum->new( "Montserrat Island Astro 1958",
			"Clarke 1880", 174, 359, 365 );

$_datum{"Nahrwan (Oman:Masirah Is)"}
	= Datum->new( "Nahrwan (Oman:Masirah Is)",
			"Clarke 1880", -247, -148, 369 );

$_datum{"Nahrwan (Saudi Arabia)"}
	= Datum->new( "Nahrwan (Saudi Arabia)",
			"Clarke 1880", -243, -192, 477 );

$_datum{"Nahrwan (United Arab Emirates)"}
	= Datum->new( "Nahrwan (United Arab Emirates)",
			"Clarke 1880", -249, -156, 381 );

$_datum{"Naparima BWI"}
	= Datum->new( "Naparima BWI",
			"International 1924", -10, 375, 165 );

$_datum{"NAD27 Alaska:Except Aleutian Is"}
	= Datum->new( "NAD27 Alaska:Except Aleutian Is",
			"Clarke 1866", -5, 135, 172 );

$_datum{"NAD27 Alaska:Aleutian Is E of 180W"}
	= Datum->new( "NAD27 Alaska:Aleutian Is E of 180W",
			"Clarke 1866", -2, 152, 149 );

$_datum{"NAD27 Alaska:Aleutian Is W of 180W"}
	= Datum->new( "NAD27 Alaska:Aleutian Is W of 180W",
			"Clarke 1866", 2, 204, 105 );

$_datum{"NAD27 Bahamas:Except San Salvador Is"}
	= Datum->new( "NAD27 Bahamas:Except San Salvador Is",
			"Clarke 1866", -4, 154, 178 );

$_datum{"NAD27 Bahamas:San Salvador Is"}
	= Datum->new( "NAD27 Bahamas:San Salvador Is",
			"Clarke 1866", 1, 140, 165 );

$_datum{"NAD27 Canada (Alberta/B.C.)"}
	= Datum->new( "NAD27 Canada (Alberta/B.C.)",
			"Clarke 1866", -7, 162, 188 );

$_datum{"NAD27 Canada (Manitoba/Ontario)"}
	= Datum->new( "NAD27 Canada (Manitoba/Ontario)",
			"Clarke 1866", -9, 157, 184 );

$_datum{"NAD27 Canada MEAN"}
	= Datum->new( "NAD27 Canada MEAN",
			"Clarke 1866", -10, 158, 189 );

$_datum{"NAD27 Canada (New Brunswick/Newfoundland/Nova Scotia/Quebec)"}
	= Datum->new( "NAD27 Canada (New Brunswick/Newfoundland/Nova Scotia/Quebec)",
			"Clarke 1866", -22, 160, 190 );

$_datum{"NAD27 Canada (Northwest Territories/Saskatchewan)"}
	= Datum->new( "NAD27 Canada (Northwest Territories/Saskatchewan)",
			"Clarke 1866", 4, 159, 188 );

$_datum{"NAD27 Canada (Yukon)"}
	= Datum->new( "NAD27 Canada (Yukon)",
			"Clarke 1866", -7, 139, 181 );

$_datum{"NAD27 Canal Zone"}
	= Datum->new( "NAD27 Canal Zone",
			"Clarke 1866", 0, 125, 201 );

$_datum{"NAD27 Caribbean"}
	= Datum->new( "NAD27 Caribbean",
			"Clarke 1866", -7, 152, 178 );

$_datum{"NAD27 MEAN:Antigua/Barbados/Barbuda/Caicos Islands/Cuba/Dominican Republic/Grand Cayman/Jamaica/Turks Islands"}
	= Datum->new( "NAD27 MEAN:Antigua/Barbados/Barbuda/Caicos Islands/Cuba/Dominican Republic/Grand Cayman/Jamaica/Turks Islands",
			"Clarke 1866", -3, 142, 183 );

$_datum{"NAD27 MEAN:Belize/Costa Rica/El Salvador/Guatemala/Honduras/Nicaragua"}
	= Datum->new( "NAD27 MEAN:Belize/Costa Rica/El Salvador/Guatemala/Honduras/Nicaragua",
			"Clarke 1866", 0, 125, 194 );

$_datum{"NAD27 CONUS MEAN"}
	= Datum->new( "NAD27 CONUS MEAN",
			"Clarke 1866", -8, 160, 176 );

  # Added for compatibility reasons.  Same as above.
$_datum{"NAD27 CONUS"}
	= Datum->new( "NAD27 CONUS",
			"Clarke 1866", -8, 160, 176 );

$_datum{"NAD27 CONUS MEAN:E of Mississippi/Louisiana/Missouri/Minnesota"}
	= Datum->new( "NAD27 CONUS MEAN:E of Mississippi/Louisiana/Missouri/Minnesota",
			"Clarke 1866", -9, 161, 179 );

$_datum{"NAD27 CONUS MEAN:W of Mississippi/Except Louisiana/Minnesota/Missouri"}
	= Datum->new( "NAD27 CONUS W of Mississippi/Except Louisiana/Minnesota/Missouri",
			"Clarke 1866", -8, 159, 175 );

$_datum{"NAD27 Cuba"}
	= Datum->new( "NAD27 Cuba",
			"Clarke 1866", -9, 152, 178 );

$_datum{"NAD27 Greenland (Hayes Peninsula)"}
	= Datum->new( "NAD27 Greenland (Hayes Peninsula)",
			"Clarke 1866", 11, 114, 195 );

$_datum{"NAD27 Mexico"}
	= Datum->new( "NAD27 Mexico",
			"Clarke 1866", -12, 130, 190 );

$_datum{"NAD27 San Salvador"}
	= Datum->new( "NAD27 San Salvador",
			"Clarke 1866", 1, 140, 165 );

$_datum{"NAD83 Alaska (Except Aleutian Is)"}
	= Datum->new( "NAD83 Alaska (Except Aleutian Is)",
			"GRS 1980", 0, 0, 0 );

$_datum{"NAD83 Aleutian Is"}
	= Datum->new( "NAD83 Aleutian Is",
			"GRS 1980", -2, 0, 4 );

$_datum{"NAD83 Canada"}
	= Datum->new( "NAD83 Canada",
			"GRS 1980", 0, 0, 0 );

$_datum{"NAD83 CONUS"}
	= Datum->new( "NAD83 CONUS",
			"GRS 1980", 0, 0, 0 );

$_datum{"NAD83 Hawaii"}
	= Datum->new( "NAD83 Hawaii",
			"GRS 1980", 1, 1, -1 );

$_datum{"NAD83 Mexico/Central America"}
	= Datum->new( "NAD83 Mexico/Central America",
			"GRS 1980", 0, 0, 0 );

$_datum{"North Sahara"}
	= Datum->new( "North Sahara",
			"Clarke 1880", -186, -93, 310 );

$_datum{"Nahrwn Masirah Ilnd"}
	= Datum->new( "Nahrwn Masirah Ilnd",
			"Clarke 1880", -247, -148, 369 );

$_datum{"Nahrwn Saudi Arbia"}
	= Datum->new( "Nahrwn Saudi Arbia",
			"Clarke 1880", -231, -196, 482 );

$_datum{"Nahrwn United Arab"}
	= Datum->new( "Nahrwn United Arab",
			"Clarke 1880", -249, -156, 381 );

$_datum{"Naparima BWI"}
	= Datum->new( "Naparima BWI",
			"International 1924", -2, 374, 172 );

$_datum{"Observatorio Meteorologico 1939"}
	= Datum->new( "Observatorio Meteorologico 1939",
			"International 1924", -425, -169, 81 );

$_datum{"Old Egyptian 1907"}
	= Datum->new( "Old Egyptian 1907",
			"Helmert 1906", -130, 110, -13 );

$_datum{"Old Hawaiian Hawaii"}
	= Datum->new( "Old Hawaiian Hawaii",
			"Clarke 1866", 89, -279, -183 );

$_datum{"Old Hawaiian Kauai"}
	= Datum->new( "Old Hawaiian Kauai",
			"Clarke 1866", 45, -290, -172 );

$_datum{"Old Hawaiian Maui"}
	= Datum->new( "Old Hawaiian Maui",
			"Clarke 1866", 65, -290, -190 );

$_datum{"Old Hawaiian MEAN"}
	= Datum->new( "Old Hawaiian MEAN",
			"Clarke 1866", 61, -285, -181 );

$_datum{"Old Hawaiian Oahu"}
	= Datum->new( "Old Hawaiian Oahu",
			"Clarke 1866", 58, -283, -182 );

$_datum{"Oman"}
	= Datum->new( "Oman",
			"Clarke 1880", -346, -1, 224 );

$_datum{"Ordnance Survey Great Britain 1936 England"}
	= Datum->new( "Ordnance Survey Great Britain 1936 England",
			"Airy 1830", 371, -112, 434 );

$_datum{"Ordnance Survey Great Britain 1936 England/Isle of Man/Wales"}
	= Datum->new( "Ordnance Survey Great Britain 1936 England/Isle of Man/Wales",
			"Airy 1830", 371, -111, 434 );

$_datum{"Ordnance Survey Great Britain 1936 MEAN:England/Isle of Man/Scotland/Shetland Is/Wales"}
	= Datum->new( "Ordnance Survey Great Britain 1936 MEAN:England/Isle of Man/Scotland/Shetland Is/Wales",
			"Airy 1830", 375, -111, 431 );

$_datum{"Ordnance Survey Great Britain 1936 Scotland/Shetland Is"}
	= Datum->new( "Ordnance Survey Great Britain 1936 Scotland/Shetland Is",
			"Airy 1830", 384, -111, 425 );

$_datum{"Ordnance Survey Great Britain 1936 Wales"}
	= Datum->new( "Ordnance Survey Great Britain 1936 Wales",
			"Airy 1830", 370, -108, 434 );

$_datum{"Pico De Las Nieves"}
	= Datum->new( "Pico De Las Nieves",
			"International 1924", -307, -92, 127 );

$_datum{"Pitcairn Astro 1967"}
	= Datum->new( "Pitcairn Astro 1967",
			"International 1924", 185, 165, 42 );

$_datum{"Point 58"}
	= Datum->new( "Point 58",
			"Clarke 1880", -106, -129, 165 );

$_datum{"Pointe Noire 1948"}
	= Datum->new( "Pointe Noire 1948",
			"Clarke 1880", -148, 51, -291 );

$_datum{"Porto Santo 1936"}
	= Datum->new( "Porto Santo 1936",
			"International 1924", -499, -249, 314 );

$_datum{"Provisional South American 1956 Bolivia"}
	= Datum->new( "Provisional South American 1956 Bolivia",
			"International 1924", -270, 188, -388 );

$_datum{"Provisional South American 1956 Chile Northern/Near 19S"}
	= Datum->new( "Provisional South American 1956 Chile Northern/Near 19S",
			"International 1924", -270, 183, -390 );

$_datum{"Provisional South American 1956 Chile Southern/Near 43S"}
	= Datum->new( "Provisional South American 1956 Chile Southern/Near 43S",
			"International 1924", -305, 243, -442 );

$_datum{"Provisional South American 1956 Columbia"}
	= Datum->new( "Provisional South American 1956 Columbia",
			"International 1924", -282, 169, -371 );

$_datum{"Provisional South American 1956 Ecuador"}
	= Datum->new( "Provisional South American 1956 Ecuador",
			"International 1924", -278, 171, -367 );

$_datum{"Provisional South American 1956 Guyana"}
	= Datum->new( "Provisional South American 1956 Guyana",
			"International 1924", -298, 159, -369 );

$_datum{"Provisional South American 1956 MEAN:Bolivia/Chile/Columbia/Ecuador/Guyana/Peru/Venezuela"}
	= Datum->new( "Provisional South American 1956 MEAN:Bolivia/Chile/Columbia/Ecuador/Guyana/Peru/Venezuela",
			"International 1924", -288, 175, -376 );

$_datum{"Provisional South American 1956 Peru"}
	= Datum->new( "Provisional South American 1956 Peru",
			"International 1924", -279, 175, -379 );

$_datum{"Provisional South American 1956 Venezuela"}
	= Datum->new( "Provisional South American 1956 Venezuela",
			"International 1924", -295, 173, -371 );

$_datum{"Provisional South Chilean 1963 Near 53S"}
	= Datum->new( "Provisional South Chilean 1963 Near 53S",
			"International 1924", 16, 196, 93 );

$_datum{"Puerto Rico/Virgin Is"}
	= Datum->new( "Puerto Rico/Virgin Is",
			"Clarke 1866", 11, 72, -101 );

$_datum{"Pulkovo 1942"}
	= Datum->new( "Pulkovo 1942",
			"Krassovsky 1940", 28, -130, -95 );

$_datum{"Qatar National"}
	= Datum->new( "Qatar National",
			"International 1924", -128, -283, 22 );

$_datum{"Qornoq"}
	= Datum->new( "Qornoq",
			"International 1924", 164, 138, -189 );

$_datum{"Reunion"}
	= Datum->new( "Reunion",
			"International 1924", 94, -948, -1262 );

$_datum{"Rome 1940"}
	= Datum->new( "Rome 1940",
			"International 1924", -225, -65, 9 );

$_datum{"RT 90"}
	= Datum->new( "RT 90",
			"Bessel 1841", 498, -36, 568 );

$_datum{"S-42 (Pulkovo 1942) Hungary"}
	= Datum->new( "S-42 (Pulkovo 1942) Hungary",
			"Krassovsky 1940", 28, -121, -77 );

$_datum{"S-42 (Pulkovo 1942) Poland"}
	= Datum->new( "S-42 (Pulkovo 1942) Poland",
			"Krassovsky 1940", 23, -124, -82 );

$_datum{"S-42 (Pulkovo 1942) Czechoslavakia"}
	= Datum->new( "S-42 (Pulkovo 1942) Czechoslavakia",
			"Krassovsky 1940", 26, -121, -78 );

$_datum{"S-42 (Pulkovo 1942) Latvia"}
	= Datum->new( "S-42 (Pulkovo 1942) Latvia",
			"Krassovsky 1940", 24, -124, -82 );

$_datum{"S-42 (Pulkovo 1942) Kazakhstan"}
	= Datum->new( "S-42 (Pulkovo 1942) Kazakhstan",
			"Krassovsky 1940", 15, -130, -84 );

$_datum{"S-42 (Pulkovo 1942) Albania"}
	= Datum->new( "S-42 (Pulkovo 1942) Albania",
			"Krassovsky 1940", 24, -130, -92 );

$_datum{"S-42 (Pulkovo 1942) Romania"}
	= Datum->new( "S-42 (Pulkovo 1942) Romania",
			"Krassovsky 1940", 28, -121, -77 );

$_datum{"S-JTSK Czechoslavakia"}
	= Datum->new( "S-JTSK Czechoslavakia",
			"Bessel 1841", 589, 76, 480 );

$_datum{"Santo (DOS) 1965"}
	= Datum->new( "Santo (DOS) 1965",
			"International 1924", 170, 42, 84 );

$_datum{"Sao Braz"}
	= Datum->new( "Sao Braz",
			"International 1924", -203, 141, 53 );

$_datum{"Sapper Hill 1943"}
	= Datum->new( "Sapper Hill 1943",
			"International 1924", -355, 21, 72 );

$_datum{"Schwarzeck"}
	= Datum->new( "Schwarzeck",
			"Bessel 1841 (Namibia)", 616, 97, -251 );

$_datum{"Selvagem Grande 1938"}
	= Datum->new( "Selvagem Grande 1938",
			"International 1924", -289, -124, 60 );

$_datum{"Sierra Leone 1960"}
	= Datum->new( "Sierra Leone 1960",
			"Clarke 1880", -88, 4, 101 );

$_datum{"South American 1969 Argentina"}
	= Datum->new( "South American 1969 Argentina",
			"South American 1969", -62, -1, -37 );

$_datum{"South American 1969 Bolivia"}
	= Datum->new( "South American 1969 Bolivia",
			"South American 1969", -61, 2, -48 );

$_datum{"South American 1969 Brazil"}
	= Datum->new( "South American 1969 Brazil",
			"South American 1969", -60, -2, -41 );

$_datum{"South American 1969 Chile"}
	= Datum->new( "South American 1969 Chile",
			"South American 1969", -75, -1, -44 );

$_datum{"South American 1969 Colombia"}
	= Datum->new( "South American 1969 Colombia",
			"South American 1969", -44, 6, -36 );

$_datum{"South American 1969 Ecuador"}
	= Datum->new( "South American 1969 Ecuador",
			"South American 1969", -48, 3, -44 );

$_datum{"South American 1969 Ecuador:Baltra/Galapagos"}
	= Datum->new( "South American 1969 Ecuador:Baltra/Galapagos",
			"South American 1969", -47, 26, -42 );

$_datum{"South American 1969 Guyana"}
	= Datum->new( "South American 1969 Guyana",
			"South American 1969", -53, 3, -47 );

$_datum{"South American 1969 MEAN"}
	= Datum->new( "South American 1969 MEAN",
			"South American 1969", -57, 1, -41 );

$_datum{"South American 1969 Paraguay"}
	= Datum->new( "South American 1969 Paraguay",
			"South American 1969", -61, 2, -33 );

$_datum{"South American 1969 Peru"}
	= Datum->new( "South American 1969 Peru",
			"South American 1969", -58, 0, -44 );

$_datum{"South American 1969 Trinidad/Tobago"}
	= Datum->new( "South American 1969 Trinidad/Tobago",
			"South American 1969", -45, 12, -33 );

$_datum{"South American 1969 Venezuela"}
	= Datum->new( "South American 1969 Venezuela",
			"South American 1969", -45, 8, -33 );

$_datum{"South Asia"}
	= Datum->new( "South Asia",
			"Modified Fischer 1960", 7, -10, -26 );

$_datum{"Southeast Base"}
	= Datum->new( "Southeast Base",
			"International 1924", -499, -249, 314 );

$_datum{"Southwest Base"}
	= Datum->new( "Southwest Base",
			"International 1924", -104, 167, -38 );

$_datum{"Tananarive Observatory 1925"}
	= Datum->new( "Tananarive Observatory 1925",
			"International 1924", -189, -242, -91 );

$_datum{"Timbalai 1948"}
	= Datum->new( "Timbalai 1948",
			"Everest (Sabah Sarawak)", -679, 669, -48 );

$_datum{"Tokyo Japan"}
	= Datum->new( "Tokyo Japan",
			"Bessel 1841", -148, 507, 685 );

$_datum{"Tokyo MEAN"}
	= Datum->new( "Tokyo MEAN",
			"Bessel 1841", -148, 507, 685 );

$_datum{"Tokyo Okinawa"}
	= Datum->new( "Tokyo Okinawa",
			"Bessel 1841", -158, 507, 676 );

$_datum{"Tokyo South Korea"}
	= Datum->new( "Tokyo South Korea",
			"Bessel 1841", -147, 506, 687 );

$_datum{"Tristan Astro 1968"}
	= Datum->new( "Tristan Astro 1968",
			"International 1924", -632, 438, -609 );

$_datum{"Viti Levu 1916"}
	= Datum->new( "Viti Levu 1916",
			"Clarke 1880", 51, 391, -36 );

$_datum{"Voirol 1960"}
	= Datum->new( "Voirol 1960",
			"Clarke 1880", -123, -206, 219 );
  
$_datum{"Wake Island Astro 1952"}
	= Datum->new( "Wake Island Astro 1952",
			"International 1924", 276, -57, 149 );

$_datum{"Wake-Eniwetok 1960"}
	= Datum->new( "Wake-Eniwetok 1960",
			"Hough 1960", 102, 52, -38 );

$_datum{"WGS 72"}
	= Datum->new( "WGS 72",
			"WGS 72", 0, 0, 0 );

$_datum{"WGS 84"}
	= Datum->new( "WGS 84",
			"WGS 84", 0, 0, 0 );

$_datum{"Yacare"}
	= Datum->new( "Yacare",
			"International 1924", -155, 171, 37 );

$_datum{"Zanderij"}
	= Datum->new( "Zanderij",
			"International 1924", -265, 120, -358 );


#------------------------------------------------------------------------------------------------


package Coordinate;


#
# Note that we export NOTHING!  This module strives to be object-oriented,
# therefore exporting is a bad thing.  You can get to any of the public
# methods by using normal method calls on an object.
#


$VERSION = do { my @r=(q$Revision$=~/\d+/g); sprintf "%d."."%02d"x$#r,@r };


use Math::Trig;
#use Math::BigFloat;    # Allows arbitrary length floating point operations
#use strict;





#
# Here is a Coordinate object.  It contains all of the data
# for describing a single point on the earth and the methods
# for setting/getting these values.
#

#
# This constructor can be used to create and initialize an object
# with Latitude/Longitude/Datum fields.
#
sub new
{
  my $class = shift;		# What class are we constructing?
  my $self      = { };		# Allocate new memory
  bless $self, $class;		# Mark it of the right type
  $self->_init(@_);		# Call _init with remaining args
  return $self;
}


sub _init
{
  my $self = shift;
  $self->{LATITUDE} = shift if @_;
  $self->{LONGITUDE} = shift if @_;
  $self->{DATUM} = shift if @_;
}


#
# This constructor can be used to create and initialize an object
# with easting/northing/zone/datum fields (UTM grid).
#
sub new_utm
{
  my $class = shift;            # What class are we constructing?
  my $self      = { };          # Allocate new memory
  bless $self, $class;          # Mark it of the right type
  $self->_init_utm(@_);         # Call _init with remaining args
  return $self;
}


sub _init_utm
{
  my $self = shift;
  $self->{EASTING} = shift if @_;
  $self->{NORTHING} = shift if @_;
  $self->{ZONE} = shift if @_;
  $self->{DATUM} = shift if @_;
}


#
# Returns the version number (RCS version tag) of this module.
#
sub version
{
  return $VERSION;
}



sub latitude
{
  my $self = shift;
  $self->{LATITUDE} = shift if @_;
  return $self->{LATITUDE};
}





sub longitude
{
  my $self = shift;
  $self->{LONGITUDE} = shift if @_;
  return $self->{LONGITUDE};
}





sub formatted_latitude
{
  my $self = shift;
  $self->{FORMATTED_LATITUDE} = shift if @_;
  return $self->{FORMATTED_LATITUDE};
}





sub formatted_longitude
{
  my $self = shift;
  $self->{FORMATTED_LONGITUDE} = shift if @_;
  return $self->{FORMATTED_LONGITUDE};
}





sub easting
{
  my $self = shift;
  $self->{EASTING} = shift if @_;
  return $self->{EASTING};
}





sub northing
{
  my $self = shift;
  $self->{NORTHING} = shift if @_;
  return $self->{NORTHING};
}





sub zone
{
  my $self = shift;
  $self->{ZONE} = shift if @_;
  return $self->{ZONE};
}





sub datum
{
  my $self = shift;
  $self->{DATUM} = shift if @_;
  return $self->{DATUM};
}





#
# Converts lat/lon to UTM coordinates.  Equations from USGS Bulletin 1532
# (According to comments in the original C code).
#
# East Longitudes are positive, West are negative. 
# North latitudes are positive, South are negative.
# Latitude and longitude are in decimal degrees.
#
# This method fills in the Easting/Northing/Zone fields in the object.
# The Zone field will look something like: "10U" when filled in.
#
sub lat_lon_to_utm
{
  my $position = shift;			# Snag the pointer to the object.

  my $PI = 3.14159265358979323846;
  my $deg_2_rad = $PI / 180;
  my $k0 = 0.9996;

  # First get the datum used in the position from the object:
  my $datum = $position->datum();

  # Now find out the ellipsoid used by consulting the DatumTable:
  my $ellipsoid = DatumTable->ellipsoid( $datum );
  
  # Do a lookup in the $ellipsoid{} hash to find the Radius value.
  my $a = EllipsoidTable->equatorial_radius( $ellipsoid );

  # Do a lookup in the $ellipsoid{} hash to find the Ecc value.
  my $ecc_Squared = EllipsoidTable->first_ecc_squared( $ellipsoid );

  my $Lat = $position->latitude();
  my $Long = $position->longitude();
 	
  # Make sure the longitude is between -180.00 .. 179.9
  my $Long_Temp = ($Long+180)-int(($Long+180)/360)*360-180; 	# -180.00 .. 179.9;

  my $Lat_Rad = $Lat * $deg_2_rad;
  my $Long_Rad = $Long_Temp * $deg_2_rad;
  my $Zone_Number = int(($Long_Temp + 180)/6) + 1;
 
  if( $Lat >= 56.0 && $Lat < 64.0 && $Long_Temp >= 3.0 && $Long_Temp < 12.0 )
  {
    $Zone_Number = 32;
  }

  # Special zones for Svalbard
  if( $Lat >= 72.0 && $Lat < 84.0 ) 
  {
    if(    $Long_Temp >= 0.0  && $Long_Temp <  9.0 ) { $Zone_Number = 31 }
    elsif( $Long_Temp >= 9.0  && $Long_Temp < 21.0 ) { $Zone_Number = 33 }
    elsif( $Long_Temp >= 21.0 && $Long_Temp < 33.0 ) { $Zone_Number = 35 }
    elsif( $Long_Temp >= 33.0 && $Long_Temp < 42.0 ) { $Zone_Number = 37 };
  }
  my $Long_Origin = ($Zone_Number - 1)*6 - 180 + 3;	# +3 puts origin in middle of zone
  my $Long_Origin_Rad = $Long_Origin * $deg_2_rad;

  # Compute the UTM Zone from the latitude and longitude
  # NOTE:  Need to check for out-of-range here.
  #
  my $Zone_Letter = &_utm_letter_designator($Lat);
  #printf( "Zone_Number: %d\t\tZone_Letter: %s\n", $Zone_Number, $Zone_Letter );
  my $UTM_Zone = sprintf( "%d%s", $Zone_Number, $Zone_Letter );

  my $ecc_Prime_Squared = ($ecc_Squared)/(1-$ecc_Squared);

  my $N = $a/sqrt(1-$ecc_Squared*sin($Lat_Rad)*sin($Lat_Rad));
  my $T = tan($Lat_Rad) * tan($Lat_Rad);
  my $C = $ecc_Prime_Squared * cos($Lat_Rad) * cos($Lat_Rad);
  my $A = cos($Lat_Rad) * ($Long_Rad-$Long_Origin_Rad);

  my $M = $a * ((1 - $ecc_Squared/4 - 3 * $ecc_Squared * $ecc_Squared/64 - 5 * $ecc_Squared * $ecc_Squared * $ecc_Squared/256) * $Lat_Rad
	- (3 * $ecc_Squared/8 + 3 * $ecc_Squared * $ecc_Squared/32 + 45 * $ecc_Squared * $ecc_Squared * $ecc_Squared/1024) * sin(2 * $Lat_Rad)
	+ (15 * $ecc_Squared * $ecc_Squared/256 + 45 * $ecc_Squared * $ecc_Squared * $ecc_Squared/1024) * sin(4 * $Lat_Rad) 
	- (35 * $ecc_Squared * $ecc_Squared * $ecc_Squared/3072) * sin(6 * $Lat_Rad));
	
  my $UTM_Easting = ($k0 * $N * ($A + (1 - $T + $C) * $A * $A * $A/6
	+ (5 - 18 * $T + $T * $T + 72 * $C - 58 * $ecc_Prime_Squared) * $A * $A * $A * $A * $A/120)
	+ 500000.0);

  my $UTM_Northing = ($k0 * ($M + $N * tan($Lat_Rad) * ($A * $A/2 + (5 - $T + 9 * $C + 4 * $C * $C) * $A * $A * $A * $A/24
	+ (61 - 58 * $T + $T * $T + 600 * $C - 330 * $ecc_Prime_Squared) * $A * $A * $A * $A * $A * $A/720)));

  if($Lat < 0)
  {
    $UTM_Northing += 10000000.0;	# 10000000 meter offset for southern hemisphere
  }

  # Write the results back to our position object
  $position->easting( $UTM_Easting );
  $position->northing( $UTM_Northing );
  $position->zone( $UTM_Zone );
  return(1);
}





#
# This routine determines the correct UTM letter designator for the given latitude.
# Returns 'Z' if latitude is outside the UTM limits of 84N to 80S.
#
# Note that this is a normal subroutine, not an object method.  It is to
# be used inside this module only and is not exported.
#
sub _utm_letter_designator
{
  my $Lat = shift;
  my $Letter_Designator;

  if    (( 84 >= $Lat) && ($Lat >=  72)) { $Letter_Designator = 'X' }
  elsif (( 72  > $Lat) && ($Lat >=  64)) { $Letter_Designator = 'W' }
  elsif (( 64  > $Lat) && ($Lat >=  56)) { $Letter_Designator = 'V' }
  elsif (( 56  > $Lat) && ($Lat >=  48)) { $Letter_Designator = 'U' }
  elsif (( 48  > $Lat) && ($Lat >=  40)) { $Letter_Designator = 'T' }
  elsif (( 40  > $Lat) && ($Lat >=  32)) { $Letter_Designator = 'S' }
  elsif (( 32  > $Lat) && ($Lat >=  24)) { $Letter_Designator = 'R' }
  elsif (( 24  > $Lat) && ($Lat >=  16)) { $Letter_Designator = 'Q' }
  elsif (( 16  > $Lat) && ($Lat >=   8)) { $Letter_Designator = 'P' }
  elsif ((  8  > $Lat) && ($Lat >=   0)) { $Letter_Designator = 'N' }
  elsif ((  0  > $Lat) && ($Lat >=  -8)) { $Letter_Designator = 'M' }
  elsif (( -8  > $Lat) && ($Lat >= -16)) { $Letter_Designator = 'L' }
  elsif ((-16  > $Lat) && ($Lat >= -24)) { $Letter_Designator = 'K' }
  elsif ((-24  > $Lat) && ($Lat >= -32)) { $Letter_Designator = 'J' }
  elsif ((-32  > $Lat) && ($Lat >= -40)) { $Letter_Designator = 'H' }
  elsif ((-40  > $Lat) && ($Lat >= -48)) { $Letter_Designator = 'G' }
  elsif ((-48  > $Lat) && ($Lat >= -56)) { $Letter_Designator = 'F' }
  elsif ((-56  > $Lat) && ($Lat >= -64)) { $Letter_Designator = 'E' }
  elsif ((-64  > $Lat) && ($Lat >= -72)) { $Letter_Designator = 'D' }
  elsif ((-72  > $Lat) && ($Lat >= -80)) { $Letter_Designator = 'C' }
  else { $Letter_Designator = 'Z' }; # An error flag: Latitude is outside UTM limits

  return $Letter_Designator;
}





#
# Converts UTM coordinates to lat/lon.  Equations from USGS Bulletin 1532.
# East longitudes are positive, West are negative. 
# North latitudes are positive, South are negative.
# Latitude and longitude are in decimal degrees. 
# Zone should look something like: "10U" before starting the conversion.
#
sub utm_to_lat_lon
{
  my $position = shift;		# Input is in the form of a "Coordinate" object

  my $PI = 3.14159265358979323846;
  my $rad_2_deg = 180.0 / $PI;
  my $k0 = 0.9996;

  # First get the datum used in the position from the object:
  my $datum = $position->datum();

  # Now find out the ellipsoid used by consulting the DatumTable:
  my $ellipsoid = DatumTable->ellipsoid( $datum );

  # Do a lookup in the $ellipsoid{} hash to find the Radius value.
  my $a = EllipsoidTable->equatorial_radius( $ellipsoid );

  # Do a lookup in the $ellipsoid{} hash to find the Ecc value.
  my $ecc_Squared = EllipsoidTable->first_ecc_squared( $ellipsoid );

  my $x = $position->easting();
  my $y = $position->northing();
  my $Zone_Letter = $position->zone();
  my $Zone_Number = $Zone_Letter;
  $Zone_Number =~ s/(\d+).*/$1/;	# Convert to an integer

  my $e1 = (1-sqrt(1-$ecc_Squared))/(1+sqrt(1-$ecc_Squared));

  $Zone_Letter =~ s/\d+(\w)/$1/;	# Convert UTM zone to just a letter
  #print "$Zone_Letter\n\n";

  my $Northern_Hemisphere;		# 1 for northern hemisphere, 0 for southern

  $x = $x - 500000.0;			# Remove 500,000 meter offset for longitude
  $y = $y;

  if(($Zone_Letter ge 'N'))
  {
    $Northern_Hemisphere = 1;		# 'N' or higher is in the northern hemisphere
    #print "Northern Hemisphere\n";
  }
  else
  {
    $Northern_Hemisphere = 0;		# Point is in southern hemisphere
    $y -= 10000000.0;			# Remove 10,000,000 meter offset used for southern hemisphere
    #print "Southern Hemisphere\n";
  }

  #print "Zone_Number: $Zone_Number\n";
  my $Long_Origin = ($Zone_Number - 1) * 6 - 180 + 3;	# +3 puts origin in middle of zone

  my $ecc_Prime_Squared = ($ecc_Squared)/(1 - $ecc_Squared);

  my $M = $y / $k0;
  my $mu = $M/($a * (1 - $ecc_Squared/4 - 3 * $ecc_Squared * $ecc_Squared/64 - 5 * $ecc_Squared * $ecc_Squared * $ecc_Squared/256));

  my $phi1_Rad = $mu + (3 * $e1/2 - 27 * $e1 * $e1 * $e1/32) * sin(2 * $mu) 
	+ (21 * $e1 * $e1/16 - 55 * $e1 * $e1 * $e1 * $e1/32) * sin(4 * $mu)
	+(151 * $e1 * $e1 * $e1/96) * sin(6 * $mu);

  my $phi1 = $phi1_Rad * $rad_2_deg;

  my $N1 = $a/sqrt(1 - $ecc_Squared * sin($phi1_Rad) * sin($phi1_Rad));
  my $T1 = tan($phi1_Rad) * tan($phi1_Rad);
  my $C1 = $ecc_Prime_Squared * cos($phi1_Rad) * cos($phi1_Rad);

  my $R1 = $a * (1 - $ecc_Squared)/(1 - $ecc_Squared * sin($phi1_Rad) * sin($phi1_Rad)**1.5);

  my $D = $x/($N1 * $k0);

  $Lat = $phi1_Rad - ($N1 * tan($phi1_Rad)/$R1) * ($D * $D/2 - (5 + 3 * $T1 + 10 * $C1 - 4 * $C1 * $C1 - 9 * $ecc_Prime_Squared) * $D * $D * $D * $D/24
	+(61 + 90 * $T1 + 298 * $C1 + 45 * $T1 * $T1 - 252 * $ecc_Prime_Squared - 3 * $C1 * $C1) * $D * $D * $D * $D * $D * $D/720);

  $Lat = $Lat * $rad_2_deg;

  $Long = ($D - (1 + 2 * $T1 + $C1) * $D * $D * $D/6 + (5 - 2 * $C1 + 28 * $T1 - 3 * $C1 * $C1 + 8 * $ecc_Prime_Squared + 24 * $T1 * $T1)
	*$D * $D * $D * $D * $D/120)/cos($phi1_Rad);

  $Long = $Long_Origin + $Long * $rad_2_deg;

  # Write the results back to our position object
  $position->latitude( $Lat );
  $position->longitude( $Long );

  return(1);
}





#
# Function to convert latitude and longitude in decimal degrees between WGS 84 and
# another datum. The arguments to this function are a Coordinate object ($position)
# and a direction flag ($from_WGS84).  The Datum you're translating to/from is
# stored in the Datum() field of the object (the other Datum is by default WGS 84).
#
sub _datum_shift
{
  my $position = shift;				# This is our position object

  my $from_WGS84 = shift;			# If 1, then we're converting from WGS 84
						# to the datum listed in the position object
						# If 0, we're going the other way.

  my $PI = 3.14159265358979323846;
  my $rad_2_deg = 180.0 / $PI;
  my $deg_2_rad = $PI / 180;

  #printf("%s\n", $position->datum() );

  my $datum = $position->datum();		# Snag the parameters from the position object
						# (Get the "other" datum).

  my $latitude = $position->latitude();
  my $longitude = $position->longitude();

  # Now find out the ellipsoid used by consulting the DatumTable:
  my $ellipsoid = DatumTable->ellipsoid( $datum );

  my $dx = DatumTable->dx($datum);		# Grab the parameters for the "other" datum.
  my $dy = DatumTable->dy($datum);		# Offsets between "other" datum and WGS84
  my $dz = DatumTable->dz($datum);		# ellipsoid centers.

  #print "$dx\t$dy\t$dz\n";

	
  my $WGS_a    = EllipsoidTable->equatorial_radius("WGS 84");	# WGS 84 semimajor axis
  my $WGS_inv_f = EllipsoidTable->inverse_flattening("WGS 84");	# WGS 84 1/f

  my $phi = $latitude * $deg_2_rad;
  my $lambda = $longitude * $deg_2_rad;
  my ($a0, $b0, $es0, $f0);			# Reference ellipsoid of input data
  my ($a1, $b1, $es1, $f1);			# Reference ellipsoid of output data
  my $psi;					# geocentric latitude 
  my ($x, $y, $z);				# 3D coordinates with respect to original datum 
  my $psi1;					# transformed geocentric latitude 

  if ($datum eq "WGS 84")			# do nothing if current datum is WGS 84
  {
    return(1);
  }
		
  if ($from_WGS84)				# convert from WGS 84 to new datum 
  {
    $a0 = $WGS_a;				# WGS 84 semimajor axis 
    $f0 = 1.0 / $WGS_inv_f;			# WGS 84 flattening 
    $a1 = EllipsoidTable->equatorial_radius($ellipsoid);
    $f1 = 1.0 / EllipsoidTable->inverse_flattening($ellipsoid);
  }
  else						# convert from old datum to WGS 84 
  {
    $a0 = EllipsoidTable->equatorial_radius($ellipsoid);	# semimajor axis 
    $f0 = 1.0 / EllipsoidTable->inverse_flattening($ellipsoid);	# flattening 
    $a1 = $WGS_a;				# WGS 84 semimajor axis 
    $f1 = 1 / $WGS_inv_f;				# WGS 84 flattening 
    $dx = -$dx;
    $dy = -$dy;
    $dz = -$dz;
  }

  $b0 = $a0 * (1 - $f0); 			# semiminor axis for input datum 
  $es0 = 2 * $f0 - $f0*$f0;			# eccentricity^2 

  $b1 = $a1 * (1 - $f1);			# semiminor axis for output datum 
  $es1 = 2 * $f1 - $f1*$f1;			# eccentricity^2 

  # Convert geodedic latitude to geocentric latitude, psi 
	
  if ($latitude == 0.0 || $latitude == 90.0 || $latitude == -90.0)
  {
    $psi = $phi;
  }
  else
  {
    $psi = atan((1 - $es0) * tan($phi));
  }

  # Calculate x and y axis coordinates with respect to the original ellipsoid 
	
  if ($longitude == 90.0 || $longitude == -90.0)
  {
    $x = 0.0;
    $y = abs($a0 * $b0 / sqrt($b0*$b0 + $a0*$a0* ( tan($psi)**2.0) ) );
  }
  else
  {
    $x = abs(($a0 * $b0) /
        sqrt( (1 + (tan($lambda)**2.0) ) * ($b0*$b0 + $a0*$a0 * (tan($psi)**2.0) ) ) );

    $y = abs($x * tan($lambda));
  }
	
  if ($longitude < -90.0 || $longitude > 90.0)
  {
    $x = -$x;
  }
  if ($longitude < 0.0)
  {
    $y = -$y;
  }
	
  # Calculate z axis coordinate with respect to the original ellipsoid 
	
  if ($latitude == 90.0)
  {
    $z = $b0;
  }
  elsif ($latitude == -90.0)
  {
    $z = -$b0;
  }
  else
  {
    $z = tan($psi) * sqrt( ($a0*$a0 * $b0*$b0) / ($b0*$b0 + $a0*$a0 * (tan($psi)**2.0) ) );
  }

  # Calculate the geocentric latitude with respect to the new ellipsoid 

  $psi1 = atan(($z - $dz) / sqrt(($x - $dx)*($x - $dx) + ($y - $dy)*($y - $dy)));
	
  # Convert to geocentric latitude and save return value 

  $latitude = atan(tan($psi1) / (1 - $es1)) * $rad_2_deg;

  # Calculate the longitude with respect to the new ellipsoid 
	
  $longitude = atan(($y - $dy) / ($x - $dx)) * $rad_2_deg;

  # Correct the resultant for negative x values 
	
  if ($x-$dx < 0.0)
  {
    if ($y-$dy > 0.0)
    {
      $longitude = 180.0 + $longitude;
    }
    else
    {
      $longitude = -180.0 + $longitude;
    }
  }

  # Write the results back to our position object
  $position->latitude($latitude);
  $position->longitude($longitude);

  if (! $from_WGS84)			# If we're converting to WGS 84 datum
  {
    $position->datum( "WGS 84" );	# Change the datum to correspond to
					# the new data in the object
  }

  #print "New one:  $latitude, $longitude\n";

  return(1);
}





#
# Method to convert latitude and longitude in decimal degrees from another
# datum to WGS 84.  The only argument to this method is a Coordinate object
# with the lat/lon/datum fields filled in.
#
# This method returns a NEW Coordinate object with lat/lon/datum fields
# filled in and the other fields empty.  The original object is not changed.
#
sub datum_shift_to_wgs84
{
  my $old_position = shift;	# This is our original position object

  my $to_WGS84 = 0;

  # Create a new object and fill in a few fields
  my $new_position = Coordinate->new(   $old_position->latitude(),
					$old_position->longitude(),
					$old_position->datum() );

  &_datum_shift( $new_position, $to_WGS84 );

  return( $new_position );	# Return the datum-shifted object
}





#
# Method to convert latitude and longitude in decimal degrees from WGS 84 to
# another datum.  The arguments to this method are a Coordinate object
# with the lat/lon/datum fields filled in (the datum field must be "WGS 84"),
# AND a datum (string) to shift to.
#
# This method returns a NEW Coordinate object with lat/lon/datum fields
# filled in.  The original object is not changed.
#
sub datum_shift_from_wgs84_to
{
  my $old_position = shift;			# This is our original position object
  my $new_datum = shift;			# The datum we wish to shift to

  my $from_WGS84 = 1;

  # Create a new object and fill in a few fields
  my $new_position = Coordinate->new(	$old_position->latitude(),
					$old_position->longitude(),
					$new_datum );

  &_datum_shift( $new_position, $from_WGS84 );

  return( $new_position );	# Return the datum-shifted object
}





#
# Degrees and Decimal Minutes
#
# Fills in the Formatted_latitude and Formatted_longitude variables
# in the object.
#
sub degrees_minutes
{
  my $self = shift;						# Get the object to work on

  $temp = CoordinateFormat->new();				# Create temporary object

  $temp->raw( $self->latitude() );				# Fill in with raw data
  $self->formatted_latitude ( $temp->degrees_minutes() );	# Process it & fill in our finished string

  $temp->raw( $self->longitude() );				# Fill in with raw data
  $self->formatted_longitude( $temp->degrees_minutes() );	# process it & fill in our finished string

  return(1);
}





#
# Degrees/Minutes/Seconds
#
# Fills in the Formatted_latitude and Formatted_longitude variables
# in the object.
#
sub degrees_minutes_seconds
{
  my $self = shift;							# Get the object to work on

  $temp = CoordinateFormat->new();					# Create the temporary object

  $temp->raw( $self->latitude() );					# Fill in with raw data
  $self->formatted_latitude ( $temp->degrees_minutes_seconds() );	# Process it & fill in our finished string

  $temp->raw( $self->longitude() );					# Fill in with raw data
  $self->formatted_longitude( $temp->degrees_minutes_seconds() );	# Process it & fill in our finished string

  return(1);
}





#
# Decimal Degrees
#
# Fills in the Formatted_latitude and Formatted_longitude variables
# in the object.
#
sub decimal_degrees
{
  my $self = shift;						# Get the object to work on

  $temp = CoordinateFormat->new();				# Create the temporary object

  $temp->raw( $self->latitude() );				# Fill in with raw data
  $self->formatted_latitude ( $temp->decimal_degrees() );	# Process it & fill in our finished string

  $temp->raw( $self->longitude() );				# Fill in with raw data
  $self->formatted_longitude( $temp->decimal_degrees() );	# Process it & fill in our finished string

  return(1);
}


#------------------------------------------------------------------------------------------------


#
# Auto-run code
#
# This code will get run automatically whenever this file gets "required"
# or "used" in another Perl program.
#

  
1;		# Needed to prevent exception when loading this module


