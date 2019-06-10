/*
   Portions Copyright (C) 2000-2019 The Xastir Group

   The datum conversion code here and in datum.c is from MacGPS 45.

   According to the Read_Me file in the source archive of that program,
   the author says the following:

   "I've read the legalese statements that everyone attaches to works like this,
   but I can never remember what they say. Suffice it to say that I am releasing
   this source code to the public domain, and you are free to do with it what you
   like. If you find it of some use and include any of it in an application,
   credits (and perhaps a copy of your program, if your feel so inclined) would
   be appreciated.

   John F. Waers <jfwaers@csn.net>"

   If you ever read this, John, thanks for the code and feel free to try out
   Xastir, it's free.

   The UTM to/from Lat/Long translations were written by Chuck Gantz
   <chuck.gantz@globalstar.com>.  Curt Mills received permission via e-mail
   to release the code under the GPL for a conversion he did to perl.
   I deduce from this that including it in Xastir, a GPL program is no problem.
   Thanks Chuck!

   N7TAP
*/



// Equatorial radius of the Earth.  In our distance/angular/area
// calculations (not here in datum.h/datum.c, but elsewhere in the
// code) we currently ignore flattening as you go towards the poles.
//
// The datum translation code in datum.h/datum.c doesn't use these
// three defines at all:  That code uses ellipsoids and so
// flattening is accounted for there.
#define EARTH_RADIUS_METERS     6378138.0
#define EARTH_RADIUS_KILOMETERS 6378.138
#define EARTH_RADIUS_MILES      3963.1836


#define FROM_WGS_84 1
#define TO_WGS_84   0
void wgs84_datum_shift(short fromWGS84, double *latitude, double *longitude, short datumID);
void datum_shift(double *latitude, double *longitude, short fromDatumID, short toDatumID);

typedef struct
{
  char  *name;    // name of ellipsoid
  double a;       // semi-major axis, meters
  double invf;    // 1/f
} Ellipsoid;

extern const Ellipsoid gEllipsoid[];

enum Ellipsoid_Names   // Must match the order of the Ellipsoids defined in datum.c
{
  E_AIRY_30,
  E_MOD_AIRY,
  E_AUS_NAT,
  E_BESS_41,
  E_BESS_41_NAM,
  E_CLARKE_66,
  E_CLARKE_80,
  E_EVR_IND_30,
  E_EVR_IND_56,
  E_EVR_SAB_SAR,
  E_EVR_MAL_69,
  E_EVR_MAL_SING,
  E_EVR_PAK,
  E_FISCH_60_MERC,
  E_MOD_FISCH_60,
  E_FISCH_68,
  E_HELM_06,
  E_HOUGH_60,
  E_IND_74,
  E_INT_24,
  E_KRASS_40,
  E_GRS_67,
  E_GRS_80,
  E_S_AMER_69,
  E_WGS_60,
  E_WGS_66,
  E_WGS_72,
  E_WGS_84
};

typedef struct
{
  char *name;
  short ellipsoid;
  short dx;
  short dy;
  short dz;
} Datum;

extern const Datum gDatum[];

enum Common_Datum_Names   // Must match the indices of the Datums defined in datum.c
{
  D_NAD_27_CONUS = 131,
  D_NAD_83_CONUS = 138,
  D_WGS_72 = 215,
  D_WGS_84 = 216
};

void ll_to_utm_ups(short ellipsoidID, const double lat, const double lon,
                   double *utmNorthing, double *utmEasting, char* utmZone, int utmZoneLength);
void utm_ups_to_ll(short ellipsoidID, const double utmNorthing, const double utmEasting,
                   const char* utmZone, double *lat, double *lon);
char utm_letter_designator(double lat, double lon);


