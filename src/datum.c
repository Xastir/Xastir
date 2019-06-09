/*
   See the top of datum.h for information on this code.
   N7TAP

   Portions Copyright (C) 2000-2019 The Xastir Group

*/


#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif  // HAVE_CONFIG_H

#include "snprintf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "xastir.h"
#include "datum.h"
#include "main.h"
#include "util.h"

// Must be last include file
#include "leak_detection.h"



//  ellipsoid: index into the gEllipsoid[] array, in which
//             a is the ellipsoid semimajor axis
//             invf is the inverse of the ellipsoid flattening f
//  dx, dy, dz: ellipsoid center with respect to WGS84 ellipsoid center
//    x axis is the prime meridian
//    y axis is 90 degrees east longitude
//    z axis is the axis of rotation of the ellipsoid

// The following values for dx, dy and dz were extracted from the output of
// the GARMIN PCX5 program. The output also includes values for da and df, the
// difference between the reference ellipsoid and the WGS84 ellipsoid semi-
// major axis and flattening, respectively. These are replaced by the
// data contained in the structure array gEllipsoid[], which was obtained from
// the Defence Mapping Agency document number TR8350.2, "Department of Defense
// World Geodetic System 1984."

/*
   The above are the original comments by John Waers.

   Curt Mills, WE7U wrote a perl version of this code and added more datums
   from Peter H. Dana's website.

   Reference Ellipsoids:
   http://www.Colorado.EDU/geography/gcraft/notes/datum/elist.html

   Reference Ellipsoids and Datums:
   http://www.Colorado.EDU/geography/gcraft/notes/datum/edlist.html

   I've loaded the numbers from that second, newer web page.
   N7TAP
*/

/* Keep the enum in datum.h up to date with the order of this array */
const Ellipsoid gEllipsoid[] =
{
//      name                         a            1/f
  {  "Airy 1830",                  6377563.396, 299.3249646   },
  {  "Modified Airy",              6377340.189, 299.3249646   },
  {  "Australian National",        6378160.0,   298.25        },
  {  "Bessel 1841",                6377397.155, 299.1528128   },
  {  "Bessel 1841 (Namibia)",      6377483.865, 299.1528128   },
  {  "Clarke 1866",                6378206.4,   294.9786982   },
  {  "Clarke 1880",                6378249.145, 293.465       },
  {  "Everest (India 1830)",       6377276.345, 300.8017      },
  {  "Everest (India 1956)",       6377301.243, 300.8017      },
  {  "Everest (Sabah Sarawak)",    6377298.556, 300.8017      },
  {  "Everest (Malaysia 1969)",    6377295.664, 300.8017      },
  {  "Everest (Malay. & Sing)",    6377304.063, 300.8017      },
  {  "Everest (Pakistan)",         6377309.613, 300.8017      },
  {  "Fischer 1960 (Mercury)",     6378166.0,   298.3         },
  {  "Modified Fischer 1960",      6378155.0,   298.3         },
  {  "Fischer 1968",               6378150.0,   298.3         },
  {  "Helmert 1906",               6378200.0,   298.3         },
  {  "Hough 1960",                 6378270.0,   297.0         },
  {  "Indonesian 1974",            6378160.0,   298.247       },
  {  "International 1924",         6378388.0,   297.0         },
  {  "Krassovsky 1940",            6378245.0,   298.3         },
  {  "GRS 67",                     6378160.0,   298.247167427 },
  {  "GRS 80",                     6378137.0,   298.257222101 },
  {  "South American 1969",        6378160.0,   298.25        },
  {  "WGS 60",                     6378165.0,   298.3         },
  {  "WGS 66",                     6378145.0,   298.25        },
  {  "WGS 72",                     6378135.0,   298.26        },
  {  "WGS 84",                     6378137.0,   298.257223563 }
};





/* Keep correct indices to commonly used datums in the enum in datum.h */
/* Feel free to add mnemonic indices for datums that you use */
const Datum gDatum[] =
{
//     name                                                             ellipsoid      dx       dy      dz
  { "Adindan (Burkina Faso)",                                         E_CLARKE_80,   -118,    -14,    218 }, // 0
  { "Adindan (Cameroon)",                                             E_CLARKE_80,   -134,     -2,    210 }, // 1
  { "Adindan (Ethiopia)",                                             E_CLARKE_80,   -165,    -11,    206 }, // 2
  { "Adindan (Mali)",                                                 E_CLARKE_80,   -123,    -20,    220 }, // 3
  { "Adindan (MEAN FOR Ethiopia; Sudan)",                             E_CLARKE_80,   -166,    -15,    204 }, // 4
  { "Adindan (Senegal)",                                              E_CLARKE_80,   -128,    -18,    224 }, // 5
  { "Adindan (Sudan)",                                                E_CLARKE_80,   -161,    -14,    205 }, // 6
  { "Afgooye (Somalia)",                                               E_KRASS_40,    -43,   -163,     45 }, // 7
  { "Ain el Abd 1970 (Bahrain)",                                         E_INT_24,   -150,   -250,     -1 }, // 8
  { "Ain el Abd 1970 (Saudi Arabia)",                                    E_INT_24,   -143,   -236,      7 }, // 9
  { "American Samoa 1962 (American Samoa Islands)",                   E_CLARKE_66,   -115,    118,    426 }, // 10
  { "Anna 1 Astro 1965 (Cocos Islands)",                                E_AUS_NAT,   -491,    -22,    435 }, // 11
  { "Antigua Island Astro 1943 (Antigua (Leeward Islands))",          E_CLARKE_80,   -270,     13,     62 }, // 12
  { "Arc 1950 (Botswana)",                                            E_CLARKE_80,   -138,   -105,   -289 }, // 13
  { "Arc 1950 (Burundi)",                                             E_CLARKE_80,   -153,     -5,   -292 }, // 14
  { "Arc 1950 (Lesotho)",                                             E_CLARKE_80,   -125,   -108,   -295 }, // 15
  { "Arc 1950 (Malawi)",                                              E_CLARKE_80,   -161,    -73,   -317 }, // 16
  { "Arc 1950 (MEAN FOR Botswana; Lesotho; Malawi; Swaziland; Zaire; Zambia; Zimbabwe)",      E_CLARKE_80,   -143,    -90,   -294 }, // 17
  { "Arc 1950 (Swaziland)",                                           E_CLARKE_80,   -134,   -105,   -295 }, // 18
  { "Arc 1950 (Zaire)",                                               E_CLARKE_80,   -169,    -19,   -278 }, // 19
  { "Arc 1950 (Zambia)",                                              E_CLARKE_80,   -147,    -74,   -283 }, // 20
  { "Arc 1950 (Zimbabwe)",                                            E_CLARKE_80,   -142,    -96,   -293 }, // 21
  { "Arc 1960 (MEAN FOR Kenya; Tanzania)",                            E_CLARKE_80,   -160,     -6,   -302 }, // 22
  { "Arc 1960 (Kenya)",                                               E_CLARKE_80,   -157,     -2,   -299 }, // 23
  { "Arc 1960 (Taanzania)",                                           E_CLARKE_80,   -175,    -23,   -303 }, // 24
  { "Ascension Island 1958 (Ascension Island)",                          E_INT_24,   -205,    107,     53 }, // 25
  { "Astro Beacon E 1945 (Iwo Jima)",                                    E_INT_24,    145,     75,   -272 }, // 26
  { "Astro DOS 71/4 (St Helena Island)",                                 E_INT_24,   -320,    550,   -494 }, // 27
  { "Astro Tern Island (FRIG) 1961 (Tern Island)",                       E_INT_24,    114,   -116,   -333 }, // 28
  { "Astronomical Station 1952 (Marcus Island)",                         E_INT_24,    124,   -234,    -25 }, // 29
  { "Australian Geodetic 1966 (Australia; Tasmania)",                   E_AUS_NAT,   -133,    -48,    148 }, // 30
  { "Australian Geodetic 1984 (Australia; Tasmania)",                   E_AUS_NAT,   -134,    -48,    149 }, // 31
  { "Ayabelle Lighthouse (Djibouti)",                                 E_CLARKE_80,    -79,   -129,    145 }, // 32
  { "Bellevue (IGN) (Efate & Erromango Islands)",                        E_INT_24,   -127,   -769,    472 }, // 33
  { "Bermuda 1957 (Bermuda)",                                         E_CLARKE_66,    -73,    213,    296 }, // 34
  { "Bissau (Guinea-Bissau)",                                            E_INT_24,   -173,    253,     27 }, // 35
  { "Bogota Observatory (Colombia)",                                     E_INT_24,    307,    304,   -318 }, // 36
  { "Bukit Rimpah (Indonesia (Bangka & Belitung Ids))",                 E_BESS_41,   -384,    664,    -48 }, // 37
  { "Camp Area Astro (Antarctica (McMurdo Camp Area))",                  E_INT_24,   -104,   -129,    239 }, // 38
  { "Campo Inchauspe (Argentina)",                                       E_INT_24,   -148,    136,     90 }, // 39
  { "Canton Astro 1966 (Phoenix Islands)",                               E_INT_24,    298,   -304,   -375 }, // 40
  { "Cape (South Africa)",                                            E_CLARKE_80,   -136,   -108,   -292 }, // 41
  { "Cape Canaveral (Bahamas; Florida)",                              E_CLARKE_66,     -2,    151,    181 }, // 42
  { "Carthage (Tunisia)",                                             E_CLARKE_80,   -263,      6,    431 }, // 43
  { "Chatham Island Astro 1971 (New Zealand (Chatham Island))",          E_INT_24,    175,    -38,    113 }, // 44
  { "Chua Astro (Paraguay)",                                             E_INT_24,   -134,    229,    -29 }, // 45
  { "Corrego Alegre (Brazil)",                                           E_INT_24,   -206,    172,     -6 }, // 46
  { "Dabola (Guinea)",                                                E_CLARKE_80,    -83,     37,    124 }, // 47
  { "Deception Island (Deception Island; Antarctia)",                 E_CLARKE_80,    260,     12,   -147 }, // 48
  { "Djakarta (Batavia) (Indonesia (Sumatra))",                         E_BESS_41,   -377,    681,    -50 }, // 49
  { "DOS 1968 (New Georgia Islands (Gizo Island))",                      E_INT_24,    230,   -199,   -752 }, // 50
  { "Easter Island 1967 (Easter Island)",                                E_INT_24,    211,    147,    111 }, // 51
  { "Estonia; Coordinate System 1937 (Estonia)",                        E_BESS_41,    374,    150,    588 }, // 52
  { "European 1950 (Cyprus)",                                            E_INT_24,   -104,   -101,   -140 }, // 53
  { "European 1950 (Egypt)",                                             E_INT_24,   -130,   -117,   -151 }, // 54
  { "European 1950 (England; Channel Islands; Scotland; Shetland Islands)",         E_INT_24,    -86,    -96,   -120 }, // 55
  { "European 1950 (England; Ireland; Scotland; Shetland Islands)",         E_INT_24,    -86,    -96,   -120 }, // 56
  { "European 1950 (Finland; Norway)",                                   E_INT_24,    -87,    -95,   -120 }, // 57
  { "European 1950 (Greece)",                                            E_INT_24,    -84,    -95,   -130 }, // 58
  { "European 1950 (Iran)",                                              E_INT_24,   -117,   -132,   -164 }, // 59
  { "European 1950 (Italy (Sardinia))",                                  E_INT_24,    -97,   -103,   -120 }, // 60
  { "European 1950 (Italy (Sicily))",                                    E_INT_24,    -97,    -88,   -135 }, // 61
  { "European 1950 (Malta)",                                             E_INT_24,   -107,    -88,   -149 }, // 62
  { "European 1950 (MEAN FOR Austria; Belgium; Denmark; Finland; France; W Germany; Gibraltar; Greece; Italy; Luxembourg; Netherlands; Norway; Portugal; Spain; Sweden; Switzerland)",         E_INT_24,    -87,    -98,   -121 }, // 63
  { "European 1950 (MEAN FOR Austria; Denmark; France; W Germany; Netherlands; Switzerland)",         E_INT_24,    -87,    -96,   -120 }, // 64
  { "European 1950 (MEAN FOR Iraq; Israel; Jordan; Lebanon; Kuwait; Saudi Arabia; Syria)",         E_INT_24,   -103,   -106,   -141 }, // 65
  { "European 1950 (Portugal; Spain)",                                   E_INT_24,    -84,   -107,   -120 }, // 66
  { "European 1950 (Tunisia)",                                           E_INT_24,   -112,    -77,   -145 }, // 67
  { "European 1979 (MEAN FOR Austria; Finland; Netherlands; Norway; Spain; Sweden; Switzerland)",         E_INT_24,    -86,    -98,   -119 }, // 68
  { "Fort Thomas 1955 (Nevis; St. Kitts (Leeward Islands))",          E_CLARKE_80,     -7,    215,    225 }, // 69
  { "Gan 1970 (Republic of Maldives)",                                   E_INT_24,   -133,   -321,     50 }, // 70
  { "Geodetic Datum 1949 (New Zealand)",                                 E_INT_24,     84,    -22,    209 }, // 71
  { "Graciosa Base SW 1948 (Azores (Faial; Graciosa; Pico; Sao Jorge; Terceira))",         E_INT_24,   -104,    167,    -38 }, // 72
  { "Guam 1963 (Guam)",                                               E_CLARKE_66,   -100,   -248,    259 }, // 73
  { "Gunung Segara (Indonesia (Kalimantan))",                           E_BESS_41,   -403,    684,     41 }, // 74
  { "GUX 1 Astro (Guadalcanal Island)",                                  E_INT_24,    252,   -209,   -751 }, // 75
  { "Herat North (Afghanistan)",                                         E_INT_24,   -333,   -222,    114 }, // 76
  { "Hermannskogel Datum (Croatia -Serbia, Bosnia-Herzegovina)",    E_BESS_41_NAM,    653,   -212,    449 }, // 77
  { "Hjorsey 1955 (Iceland)",                                            E_INT_24,    -73,     46,    -86 }, // 78
  { "Hong Kong 1963 (Hong Kong)",                                        E_INT_24,   -156,   -271,   -189 }, // 79
  { "Hu-Tzu-Shan (Taiwan)",                                              E_INT_24,   -637,   -549,   -203 }, // 80
  { "Indian (Bangladesh)",                                           E_EVR_IND_30,    282,    726,    254 }, // 81
  { "Indian (India; Nepal)",                                         E_EVR_IND_56,    295,    736,    257 }, // 82
  { "Indian (Pakistan)",                                                E_EVR_PAK,    283,    682,    231 }, // 83
  { "Indian 1954 (Thailand)",                                        E_EVR_IND_30,    217,    823,    299 }, // 84
  { "Indian 1960 (Vietnam (Con Son Island))",                        E_EVR_IND_30,    182,    915,    344 }, // 85
  { "Indian 1960 (Vietnam (Near 16N))",                              E_EVR_IND_30,    198,    881,    317 }, // 86
  { "Indian 1975 (Thailand)",                                        E_EVR_IND_30,    210,    814,    289 }, // 87
  { "Indonesian 1974 (Indonesia)",                                       E_IND_74,    -24,    -15,      5 }, // 88
  { "Ireland 1965 (Ireland)",                                          E_MOD_AIRY,    506,   -122,    611 }, // 89
  { "ISTS 061 Astro 1968 (South Georgia Islands)",                       E_INT_24,   -794,    119,   -298 }, // 90
  { "ISTS 073 Astro 1969 (Diego Garcia)",                                E_INT_24,    208,   -435,   -229 }, // 91
  { "Johnston Island 1961 (Johnston Island)",                            E_INT_24,    189,    -79,   -202 }, // 92
  { "Kandawala (Sri Lanka)",                                         E_EVR_IND_30,    -97,    787,     86 }, // 93
  { "Kerguelen Island 1949 (Kerguelen Island)",                          E_INT_24,    145,   -187,    103 }, // 94
  { "Kertau 1948 (West Malaysia & Singapore)",                     E_EVR_MAL_SING,    -11,    851,      5 }, // 95
  { "Kusaie Astro 1951 (Caroline Islands)",                              E_INT_24,    647,   1777,  -1124 }, // 96
  { "Korean Geodetic System (South Korea)",                              E_GRS_80,      0,      0,      0 }, // 97
  { "L. C. 5 Astro 1961 (Cayman Brac Island)",                        E_CLARKE_66,     42,    124,    147 }, // 98
  { "Leigon (Ghana)",                                                 E_CLARKE_80,   -130,     29,    364 }, // 99
  { "Liberia 1964 (Liberia)",                                         E_CLARKE_80,    -90,     40,     88 }, // 100
  { "Luzon (Philippines (Excluding Mindanao))",                       E_CLARKE_66,   -133,    -77,    -51 }, // 101
  { "Luzon (Philippines (Mindanao))",                                 E_CLARKE_66,   -133,    -79,    -72 }, // 102
  { "M'Poraloko (Gabon)",                                             E_CLARKE_80,    -74,   -130,     42 }, // 103
  { "Mahe 1971 (Mahe Island)",                                        E_CLARKE_80,     41,   -220,   -134 }, // 104
  { "Massawa (Ethiopia (Eritrea))",                                     E_BESS_41,    639,    405,     60 }, // 105
  { "Merchich (Morocco)",                                             E_CLARKE_80,     31,    146,     47 }, // 106
  { "Midway Astro 1961 (Midway Islands)",                                E_INT_24,    912,    -58,   1227 }, // 107
  { "Minna (Cameroon)",                                               E_CLARKE_80,    -81,    -84,    115 }, // 108
  { "Minna (Nigeria)",                                                E_CLARKE_80,    -92,    -93,    122 }, // 109
  { "Montserrat Island Astro 1958 (Montserrat (Leeward Islands))",      E_CLARKE_80,    174,    359,    365 }, // 110
  { "Nahrwan (Oman (Masirah Island))",                                E_CLARKE_80,   -247,   -148,    369 }, // 111
  { "Nahrwan (Saudi Arabia)",                                         E_CLARKE_80,   -243,   -192,    477 }, // 112
  { "Nahrwan (United Arab Emirates)",                                 E_CLARKE_80,   -249,   -156,    381 }, // 113
  { "Naparima BWI (Trinidad & Tobago)",                                  E_INT_24,    -10,    375,    165 }, // 114
  { "North American 1927 (Alaska (Excluding Aleutian Ids))",          E_CLARKE_66,     -5,    135,    172 }, // 115
  { "North American 1927 (Alaska (Aleutian Ids East of 180W))",       E_CLARKE_66,     -2,    152,    149 }, // 116
  { "North American 1927 (Alaska (Aleutian Ids West of 180W))",       E_CLARKE_66,      2,    204,    105 }, // 117
  { "North American 1927 (Bahamas (Except San Salvador Id))",         E_CLARKE_66,     -4,    154,    178 }, // 118
  { "North American 1927 (Bahamas (San Salvador Island))",            E_CLARKE_66,      1,    140,    165 }, // 119
  { "North American 1927 (Canada (Alberta; British Columbia))",       E_CLARKE_66,     -7,    162,    188 }, // 120
  { "North American 1927 (Canada (Manitoba; Ontario))",               E_CLARKE_66,     -9,    157,    184 }, // 121
  { "North American 1927 (Canada (New Brunswick; Newfoundland; Nova Scotia; Quebec))",      E_CLARKE_66,    -22,    160,    190 }, // 122
  { "North American 1927 (Canada (Northwest Territories; Saskatchewan))",      E_CLARKE_66,      4,    159,    188 }, // 123
  { "North American 1927 (Canada (Yukon))",                           E_CLARKE_66,     -7,    139,    181 }, // 124
  { "North American 1927 (Canal Zone)",                               E_CLARKE_66,      0,    125,    201 }, // 125
  { "North American 1927 (Cuba)",                                     E_CLARKE_66,     -9,    152,    178 }, // 126
  { "North American 1927 (Greenland (Hayes Peninsula))",              E_CLARKE_66,     11,    114,    195 }, // 127
  { "North American 1927 (MEAN FOR Antigua; Barbados; Barbuda; Caicos Islands; Cuba; Dominican Republic; Grand Cayman; Jamaica; Turks Islands)",      E_CLARKE_66,     -3,    142,    183 }, // 128
  { "North American 1927 (MEAN FOR Belize; Costa Rica; El Salvador; Guatemala; Honduras; Nicaragua)",      E_CLARKE_66,      0,    125,    194 }, // 129
  { "North American 1927 (MEAN FOR Canada)",                          E_CLARKE_66,    -10,    158,    187 }, // 130
  { "North American 1927 (MEAN FOR CONUS)",                           E_CLARKE_66,     -8,    160,    176 }, // 131
  { "North American 1927 (MEAN FOR CONUS (East of Mississippi; River Including Louisiana; Missouri; Minnesota))",      E_CLARKE_66,     -9,    161,    179 }, // 132
  { "North American 1927 (MEAN FOR CONUS (West of Mississippi; River Excluding Louisiana; Minnesota; Missouri))",      E_CLARKE_66,     -8,    159,    175 }, // 133
  { "North American 1927 (Mexico)",                                   E_CLARKE_66,    -12,    130,    190 }, // 134
  { "North American 1983 (Alaska (Excluding Aleutian Ids))",             E_GRS_80,      0,      0,      0 }, // 135
  { "North American 1983 (Aleutian Ids)",                                E_GRS_80,     -2,      0,      4 }, // 136
  { "North American 1983 (Canada)",                                      E_GRS_80,      0,      0,      0 }, // 137
  { "North American 1983 (CONUS)",                                       E_GRS_80,      0,      0,      0 }, // 138
  { "North American 1983 (Hawaii)",                                      E_GRS_80,      1,      1,     -1 }, // 139
  { "North American 1983 (Mexico; Central America)",                     E_GRS_80,      0,      0,      0 }, // 140
  { "North Sahara 1959 (Algeria)",                                    E_CLARKE_80,   -186,    -93,    310 }, // 141
  { "Observatorio Meteorologico 1939 (Azores (Corvo & Flores Islands))",         E_INT_24,   -425,   -169,     81 }, // 142
  { "Old Egyptian 1907 (Egypt)",                                        E_HELM_06,   -130,    110,    -13 }, // 143
  { "Old Hawaiian (Hawaii)",                                          E_CLARKE_66,     89,   -279,   -183 }, // 144
  { "Old Hawaiian (Kauai)",                                           E_CLARKE_66,     45,   -290,   -172 }, // 145
  { "Old Hawaiian (Maui)",                                            E_CLARKE_66,     65,   -290,   -190 }, // 146
  { "Old Hawaiian (MEAN FOR Hawaii; Kauai; Maui; Oahu)",              E_CLARKE_66,     61,   -285,   -181 }, // 147
  { "Old Hawaiian (Oahu)",                                            E_CLARKE_66,     58,   -283,   -182 }, // 148
  { "Oman (Oman)",                                                    E_CLARKE_80,   -346,     -1,    224 }, // 149
  { "Ordnance Survey Great Britain 1936 (England)",                     E_AIRY_30,    371,   -112,    434 }, // 150
  { "Ordnance Survey Great Britain 1936 (England; Isle of Man; Wales)",        E_AIRY_30,    371,   -111,    434 }, // 151
  { "Ordnance Survey Great Britain 1936 (MEAN FOR England; Isle of Man; Scotland; Shetland Islands; Wales)",        E_AIRY_30,    375,   -111,    431 }, // 152
  { "Ordnance Survey Great Britain 1936 (Scotland; Shetland Islands)",        E_AIRY_30,    384,   -111,    425 }, // 153
  { "Ordnance Survey Great Britain 1936 (Wales)",                       E_AIRY_30,    370,   -108,    434 }, // 154
  { "Pico de las Nieves (Canary Islands)",                               E_INT_24,   -307,    -92,    127 }, // 155
  { "Pitcairn Astro 1967 (Pitcairn Island)",                             E_INT_24,    185,    165,     42 }, // 156
  { "Point 58 (MEAN FOR Burkina Faso & Niger)",                       E_CLARKE_80,   -106,   -129,    165 }, // 157
  { "Pointe Noire 1948 (Congo)",                                      E_CLARKE_80,   -148,     51,   -291 }, // 158
  { "Porto Santo 1936 (Porto Santo; Madeira Islands)",                   E_INT_24,   -499,   -249,    314 }, // 159
  { "Provisional South American 1956 (Bolivia)",                         E_INT_24,   -270,    188,   -388 }, // 160
  { "Provisional South American 1956 (Chile (Northern; Near 19S))",          E_INT_24,   -270,    183,   -390 }, // 161
  { "Provisional South American 1956 (Chile (Southern; Near 43S))",          E_INT_24,   -305,    243,   -442 }, // 162
  { "Provisional South American 1956 (Colombia)",                        E_INT_24,   -282,    169,   -371 }, // 163
  { "Provisional South American 1956 (Ecuador)",                         E_INT_24,   -278,    171,   -367 }, // 164
  { "Provisional South American 1956 (Guyana)",                          E_INT_24,   -298,    159,   -369 }, // 165
  { "Provisional South American 1956 (MEAN FOR Bolivia; Chile; Colombia; Ecuador; Guyana; Peru; Venezuela)",         E_INT_24,   -288,    175,   -376 }, // 166
  { "Provisional South American 1956 (Peru)",                            E_INT_24,   -279,    175,   -379 }, // 167
  { "Provisional South American 1956 (Venezuela)",                       E_INT_24,   -295,    173,   -371 }, // 168
  { "Provisional South Chilean 1963 (Chile (Near 53S) (Hito XVIII))",          E_INT_24,     16,    196,     93 }, // 169
  { "Puerto Rico (Puerto Rico; Virgin Islands)",                      E_CLARKE_66,     11,     72,   -101 }, // 170
  { "Pulkovo 1942 (Russia)",                                           E_KRASS_40,     28,   -130,    -95 }, // 171
  { "Qatar National (Qatar)",                                            E_INT_24,   -128,   -283,     22 }, // 172
  { "Qornoq (Greenland (South))",                                        E_INT_24,    164,    138,   -189 }, // 173
  { "Reunion (Mascarene Islands)",                                       E_INT_24,     94,   -948,  -1262 }, // 174
  { "Rome 1940 (Italy (Sardinia))",                                      E_INT_24,   -225,    -65,      9 }, // 175
  { "S-42 (Pulkovo 1942) (Hungary)",                                   E_KRASS_40,     28,   -121,    -77 }, // 176
  { "S-42 (Pulkovo 1942) (Poland)",                                    E_KRASS_40,     23,   -124,    -82 }, // 177
  { "S-42 (Pulkovo 1942) (Czechoslavakia)",                            E_KRASS_40,     26,   -121,    -78 }, // 178
  { "S-42 (Pulkovo 1942) (Latvia)",                                    E_KRASS_40,     24,   -124,    -82 }, // 179
  { "S-42 (Pulkovo 1942) (Kazakhstan)",                                E_KRASS_40,     15,   -130,    -84 }, // 180
  { "S-42 (Pulkovo 1942) (Albania)",                                   E_KRASS_40,     24,   -130,    -92 }, // 181
  { "S-42 (Pulkovo 1942) (Romania)",                                   E_KRASS_40,     28,   -121,    -77 }, // 182
  { "S-JTSK (Czechoslavakia (Prior 1 JAN 1993))",                       E_BESS_41,    589,     76,    480 }, // 183
  { "Santo (DOS) 1965 (Espirito Santo Island)",                          E_INT_24,    170,     42,     84 }, // 184
  { "Sao Braz (Azores (Sao Miguel; Santa Maria Ids))",                   E_INT_24,   -203,    141,     53 }, // 185
  { "Sapper Hill 1943 (East Falkland Island)",                           E_INT_24,   -355,     21,     72 }, // 186
  { "Schwarzeck (Namibia)",                                         E_BESS_41_NAM,    616,     97,   -251 }, // 187
  { "Selvagem Grande 1938 (Salvage Islands)",                            E_INT_24,   -289,   -124,     60 }, // 188
  { "Sierra Leone 1960 (Sierra Leone)",                               E_CLARKE_80,    -88,      4,    101 }, // 189
  { "South American 1969 (Argentina)",                                E_S_AMER_69,    -62,     -1,    -37 }, // 190
  { "South American 1969 (Bolivia)",                                  E_S_AMER_69,    -61,      2,    -48 }, // 191
  { "South American 1969 (Brazil)",                                   E_S_AMER_69,    -60,     -2,    -41 }, // 192
  { "South American 1969 (Chile)",                                    E_S_AMER_69,    -75,     -1,    -44 }, // 193
  { "South American 1969 (Colombia)",                                 E_S_AMER_69,    -44,      6,    -36 }, // 194
  { "South American 1969 (Ecuador)",                                  E_S_AMER_69,    -48,      3,    -44 }, // 195
  { "South American 1969 (Ecuador (Baltra; Galapagos))",              E_S_AMER_69,    -47,     26,    -42 }, // 196
  { "South American 1969 (Guyana)",                                   E_S_AMER_69,    -53,      3,    -47 }, // 197
  { "South American 1969 (MEAN FOR Argentina; Bolivia; Brazil; Chile; Colombia; Ecuador; Guyana; Paraguay; Peru; Trinidad & Tobago; Venezuela)",      E_S_AMER_69,    -57,      1,    -41 }, // 198
  { "South American 1969 (Paraguay)",                                 E_S_AMER_69,    -61,      2,    -33 }, // 199
  { "South American 1969 (Peru)",                                     E_S_AMER_69,    -58,      0,    -44 }, // 200
  { "South American 1969 (Trinidad & Tobago)",                        E_S_AMER_69,    -45,     12,    -33 }, // 201
  { "South American 1969 (Venezuela)",                                E_S_AMER_69,    -45,      8,    -33 }, // 202
  { "South Asia (Singapore)",                                      E_MOD_FISCH_60,      7,    -10,    -26 }, // 203
  { "Tananarive Observatory 1925 (Madagascar)",                          E_INT_24,   -189,   -242,    -91 }, // 204
  { "Timbalai 1948 (Brunei; E. Malaysia (Sabah Sarawak))",          E_EVR_SAB_SAR,   -679,    669,    -48 }, // 205
  { "Tokyo (Japan)",                                                    E_BESS_41,   -148,    507,    685 }, // 206
  { "Tokyo (MEAN FOR Japan; South Korea; Okinawa)",                     E_BESS_41,   -148,    507,    685 }, // 207
  { "Tokyo (Okinawa)",                                                  E_BESS_41,   -158,    507,    676 }, // 208
  { "Tokyo (South Korea)",                                              E_BESS_41,   -147,    506,    687 }, // 209
  { "Tristan Astro 1968 (Tristan da Cunha)",                             E_INT_24,   -632,    438,   -609 }, // 210
  { "Viti Levu 1916 (Fiji (Viti Levu Island))",                       E_CLARKE_80,     51,    391,    -36 }, // 211
  { "Voirol 1960 (Algeria)",                                          E_CLARKE_80,   -123,   -206,    219 }, // 212
  { "Wake Island Astro 1952 (Wake Atoll)",                               E_INT_24,    276,    -57,    149 }, // 213
  { "Wake-Eniwetok 1960 (Marshall Islands)",                           E_HOUGH_60,    102,     52,    -38 }, // 214
  { "WGS 1972 (Global Definition)",                                      E_WGS_72,      0,      0,      0 }, // 215
  { "WGS 1984 (Global Definition)",                                      E_WGS_84,      0,      0,      0 }, // 216
  { "Yacare (Uruguay)",                                                  E_INT_24,   -155,    171,     37 }, // 217
  { "Zanderij (Suriname)",                                               E_INT_24,   -265,    120,   -358 }  // 218
};



static const double PI = 3.14159265358979323846;





/* As you can see this little function is just a 2 step datum shift, going through WGS84. */
void datum_shift(double *latitude, double *longitude, short fromDatumID, short toDatumID)
{
  wgs84_datum_shift(TO_WGS_84,   latitude, longitude, fromDatumID);
  wgs84_datum_shift(FROM_WGS_84, latitude, longitude, toDatumID);
}





/*
  Function to convert latitude and longitude in decimal degrees from WGS84 to
  another datum or from another datum to WGS84. The arguments to this function
  include a direction flag 'fromWGS84', pointers to double precision latitude
  and longitude, and an index to the gDatum[] array.
*/
void wgs84_datum_shift(short fromWGS84, double *latitude, double *longitude, short datumID)
{
  double dx = gDatum[datumID].dx;
  double dy = gDatum[datumID].dy;
  double dz = gDatum[datumID].dz;

  double phi = *latitude * PI / 180.0;
  double lambda = *longitude * PI / 180.0;
  double a0, b0, es0, f0;                     /* Reference ellipsoid of input data */
  // a1 and b1 are never actually used, so don't declare them and set
  // them (gcc warns about set-but-unused vars)
  //double a1, b1, es1, f1;                     /* Reference ellipsoid of output data */
  double es1, f1;                     /* Reference ellipsoid of output data */
  double psi;                                 /* geocentric latitude */
  double x, y, z;                             /* 3D coordinates with respect to original datum */
  double psi1;                                /* transformed geocentric latitude */

  if (datumID == D_WGS_84)                    // do nothing if current datum is WGS84
  {
    return;
  }

  if (fromWGS84)                              /* convert from WGS84 to new datum */
  {
    a0 = gEllipsoid[E_WGS_84].a;                            /* WGS84 semimajor axis */
    f0 = 1.0 / gEllipsoid[E_WGS_84].invf;                   /* WGS84 flattening */
    // a1 is never used except to set b1, which itself is never used
    // a1 = gEllipsoid[gDatum[datumID].ellipsoid].a;
    f1 = 1.0 / gEllipsoid[gDatum[datumID].ellipsoid].invf;
  }
  else                                        /* convert from datum to WGS84 */
  {
    a0 = gEllipsoid[gDatum[datumID].ellipsoid].a;           /* semimajor axis */
    f0 = 1.0 / gEllipsoid[gDatum[datumID].ellipsoid].invf;  /* flattening */
    // a1 is never used except to set b1, which is never used.
    // a1 = gEllipsoid[E_WGS_84].a;                            /* WGS84 semimajor axis */
    f1 = 1.0 / gEllipsoid[E_WGS_84].invf;                   /* WGS84 flattening */
    dx = -dx;
    dy = -dy;
    dz = -dz;
  }

  b0 = a0 * (1 - f0);                         /* semiminor axis for input datum */
  es0 = 2 * f0 - f0*f0;                       /* eccentricity^2 */

  // b1 is never used
  // b1 = a1 * (1 - f1);                         /* semiminor axis for output datum */
  es1 = 2 * f1 - f1*f1;                       /* eccentricity^2 */

  /* Convert geodedic latitude to geocentric latitude, psi */
  if (*latitude == 0.0 || *latitude == 90.0 || *latitude == -90.0)
  {
    psi = phi;
  }
  else
  {
    psi = atan((1 - es0) * tan(phi));
  }

  /* Calculate x and y axis coordinates with respect to the original ellipsoid */
  if (*longitude == 90.0 || *longitude == -90.0)
  {
    x = 0.0;
    y = fabs(a0 * b0 / sqrt(b0*b0 + a0*a0*pow(tan(psi), 2.0)));
  }
  else
  {
    x = fabs((a0 * b0) /
             sqrt((1 + pow(tan(lambda), 2.0)) * (b0*b0 + a0*a0 * pow(tan(psi), 2.0))));
    y = fabs(x * tan(lambda));
  }

  if (*longitude < -90.0 || *longitude > 90.0)
  {
    x = -x;
  }
  if (*longitude < 0.0)
  {
    y = -y;
  }

  /* Calculate z axis coordinate with respect to the original ellipsoid */
  if (*latitude == 90.0)
  {
    z = b0;
  }
  else if (*latitude == -90.0)
  {
    z = -b0;
  }
  else
  {
    z = tan(psi) * sqrt((a0*a0 * b0*b0) / (b0*b0 + a0*a0 * pow(tan(psi), 2.0)));
  }

  /* Calculate the geocentric latitude with respect to the new ellipsoid */
  psi1 = atan((z - dz) / sqrt((x - dx)*(x - dx) + (y - dy)*(y - dy)));

  /* Convert to geocentric latitude and save return value */
  *latitude = atan(tan(psi1) / (1 - es1)) * 180.0 / PI;

  /* Calculate the longitude with respect to the new ellipsoid */
  *longitude = atan((y - dy) / (x - dx)) * 180.0 / PI;

  /* Correct the resultant for negative x values */
  if (x-dx < 0.0)
  {
    if (y-dy > 0.0)
    {
      *longitude = 180.0 + *longitude;
    }
    else
    {
      *longitude = -180.0 + *longitude;
    }
  }
}





#define deg2rad (PI / 180)
#define rad2deg (180.0 / PI)





/*
  Source
  Defense Mapping Agency. 1987b. DMA Technical Report: Supplement to Department of Defense World Geodetic System
  1984 Technical Report. Part I and II. Washington, DC: Defense Mapping Agency
*/
//
// Convert lat/long to UTM/UPS coordinates
void ll_to_utm_ups(short ellipsoidID, const double lat, const double lon,
                   double *utmNorthing, double *utmEasting, char* utmZone, int utmZoneLength)
{
  //converts lat/long to UTM coords.  Equations from USGS Bulletin 1532
  //East Longitudes are positive, West longitudes are negative.
  //North latitudes are positive, South latitudes are negative
  //Lat and Long are in decimal degrees
  //Written by Chuck Gantz- chuck.gantz@globalstar.com

  double a = gEllipsoid[ellipsoidID].a;
  double f = 1.0 / gEllipsoid[ellipsoidID].invf;
  double eccSquared = (2 * f) - (f * f);
  double k0 = 0.9996;

  double LongOrigin;
  double eccPrimeSquared;
  double N, T, C, A, M;

  //Make sure the longitude is between -180.00 .. 179.9
  double LongTemp = (lon+180)-(int)((lon+180)/360)*360-180; // -180.00 .. 179.9;

  double LatRad = lat*deg2rad;
  double LongRad = LongTemp*deg2rad;
  double LongOriginRad;
  int    ZoneNumber;

  ZoneNumber = (int)((LongTemp + 180)/6) + 1;

  if (coordinate_system == USE_UTM_SPECIAL
      || coordinate_system == USE_MGRS)
  {

    // Special zone for southern Norway.  Used for military
    // version of UTM (MGRS) only.
    if ( lat >= 56.0 && lat < 64.0 && LongTemp >= 3.0 && LongTemp < 12.0 )
    {
      ZoneNumber = 32;
    }

    // Handle the special zones for Svalbard.  Used for military
    // version of UTM (MGRS) only.
    if (lat >= 72.0 && lat < 84.0)
    {
      if (LongTemp >= 0.0  && LongTemp <  9.0)
      {
        ZoneNumber = 31;
      }
      else if (LongTemp >= 9.0  && LongTemp < 21.0)
      {
        ZoneNumber = 33;
      }
      else if (LongTemp >= 21.0 && LongTemp < 33.0)
      {
        ZoneNumber = 35;
      }
      else if (LongTemp >= 33.0 && LongTemp < 42.0)
      {
        ZoneNumber = 37;
      }
    }
  }

  LongOrigin = (ZoneNumber - 1)*6 - 180 + 3;  //+3 puts origin in middle of zone
  LongOriginRad = LongOrigin * deg2rad;

  if (lat > 84.0 || lat < -80.0)
  {
    // We're in the UPS areas (near the poles).  ZoneNumber
    // should not be printed in this case.
    xastir_snprintf(utmZone,
                    utmZoneLength,
                    "%c",
                    utm_letter_designator(lat, lon));
  }
  else    // We're in the UTM areas (not near the poles).
  {
    //compute the UTM Zone from the latitude and longitude
    xastir_snprintf(utmZone,
                    utmZoneLength,
                    "%d%c",
                    ZoneNumber,
                    utm_letter_designator(lat, lon));
  }

  eccPrimeSquared = (eccSquared)/(1-eccSquared);

  if (lat > 84.0 || lat < -80.0)
  {
    //
    // We're dealing with UPS coordinates (near the poles)
    //
    // The following piece of code which implements UPS
    // conversion is derived from code that John Waers
    // <jfwaers@csn.net> placed in the public domain.  It's from
    // his program "MacGPS45".

    double t, e, rho;
    const double k0 = 0.994;
    double lambda = lon * (PI/180.0);
    double phi = fabs(lat * (PI/180.0) );

    e = sqrt(eccSquared);
    t = tan(PI/4.0 - phi/2.0) / pow( (1.0 - e * sin(phi)) / (1.0 + e * sin(phi)), (e/2.0) );
    rho = 2.0 * a * k0 * t / sqrt(pow(1.0+e, 1.0+e) * pow(1.0-e, 1.0-e));
    *utmEasting = rho * sin(lambda);
    *utmNorthing = rho * cos(lambda);

    if (lat > 0.0)  // Northern hemisphere
    {
      *utmNorthing = -(*utmNorthing);
    }

    *utmEasting  += 2.0e6;  // Add in false easting and northing
    *utmNorthing += 2.0e6;
  }
  else
  {
    //
    // We're dealing with UTM coordinates
    //
    N = a/sqrt(1-eccSquared*sin(LatRad)*sin(LatRad));
    T = tan(LatRad)*tan(LatRad);
    C = eccPrimeSquared*cos(LatRad)*cos(LatRad);
    A = cos(LatRad)*(LongRad-LongOriginRad);

    M = a*((1 -
            eccSquared/4 -
            3*eccSquared*eccSquared/64 -
            5*eccSquared*eccSquared*eccSquared/256) * LatRad -
           (3*eccSquared/8 +
            3*eccSquared*eccSquared/32 +
            45*eccSquared*eccSquared*eccSquared/1024) * sin(2*LatRad) +
           (15*eccSquared*eccSquared/256 + 45*eccSquared*eccSquared*eccSquared/1024) * sin(4*LatRad) -
           (35*eccSquared*eccSquared*eccSquared/3072) * sin(6*LatRad));

    *utmEasting = (double)(k0*N*(A+(1-T+C)*A*A*A/6
                                 + (5-18*T+T*T+72*C-58*eccPrimeSquared)*A*A*A*A*A/120)
                           + 500000.0);

    *utmNorthing = (double)(k0*(M+N*tan(LatRad)*
                                (A*A/2+(5-T+9*C+4*C*C)*A*A*A*A/24
                                 + (61-58*T+T*T+600*C-330*eccPrimeSquared)*A*A*A*A*A*A/720)));

    if (lat < 0)
    {
      *utmNorthing += 10000000.0;  //10000000 meter offset for southern hemisphere
    }
  }
}





// Handles UPS/UTM coordinates equally well!
//
char utm_letter_designator(double lat, double lon)
{
  // This routine determines the correct UTM/UPS letter designator
  // for the given latitude.  Originally written by Chuck Gantz-
  // chuck.gantz@globalstar.com
  // Modified to handle UPS zones.  --we7u
  char LetterDesignator;

  if ((84 >= lat) && (lat >=  72))
  {
    LetterDesignator = 'X';
  }
  else if ((72  > lat) && (lat >=  64))
  {
    LetterDesignator = 'W';
  }
  else if ((64  > lat) && (lat >=  56))
  {
    LetterDesignator = 'V';
  }
  else if ((56  > lat) && (lat >=  48))
  {
    LetterDesignator = 'U';
  }
  else if ((48  > lat) && (lat >=  40))
  {
    LetterDesignator = 'T';
  }
  else if ((40  > lat) && (lat >=  32))
  {
    LetterDesignator = 'S';
  }
  else if ((32  > lat) && (lat >=  24))
  {
    LetterDesignator = 'R';
  }
  else if ((24  > lat) && (lat >=  16))
  {
    LetterDesignator = 'Q';
  }
  else if ((16  > lat) && (lat >=   8))
  {
    LetterDesignator = 'P';
  }
  else if (( 8  > lat) && (lat >=   0))
  {
    LetterDesignator = 'N';
  }
  else if (( 0  > lat) && (lat >=  -8))
  {
    LetterDesignator = 'M';
  }
  else if ((-8  > lat) && (lat >= -16))
  {
    LetterDesignator = 'L';
  }
  else if ((-16 > lat) && (lat >= -24))
  {
    LetterDesignator = 'K';
  }
  else if ((-24 > lat) && (lat >= -32))
  {
    LetterDesignator = 'J';
  }
  else if ((-32 > lat) && (lat >= -40))
  {
    LetterDesignator = 'H';
  }
  else if ((-40 > lat) && (lat >= -48))
  {
    LetterDesignator = 'G';
  }
  else if ((-48 > lat) && (lat >= -56))
  {
    LetterDesignator = 'F';
  }
  else if ((-56 > lat) && (lat >= -64))
  {
    LetterDesignator = 'E';
  }
  else if ((-64 > lat) && (lat >= -72))
  {
    LetterDesignator = 'D';
  }
  else if ((-72 > lat) && (lat >= -80))
  {
    LetterDesignator = 'C';
  }
  else
  {
    //
    // We're dealing with UPS (N/S Pole) coordinates, not UTM
    //
    if (lat > 84)   // North Pole, Y/Z zones
    {
      if ((0 <= lon) && (lon <= 180))
      {
        LetterDesignator = 'Z';  // E or + longitude
      }
      else
      {
        LetterDesignator = 'Y';  // W or - longitude
      }
    }
    else    // Lat < 80S, South Pole, A/B zones
    {
      if ((0 <= lon) && (lon <= 180))
      {
        LetterDesignator = 'B';  // E or + longitude
      }
      else
      {
        LetterDesignator = 'A';  // W or - longitude
      }
    }
  }
  return LetterDesignator;
}





// The following piece of code which implements UPS conversion is
// derived from code that John Waers <jfwaers@csn.net> placed in
// the public domain.  It's from his program "MacGPS45".
//
static void calcPhi(double *phi, double e, double t)
{
  double old = PI/2.0 - 2.0 * atan(t);
  short maxIterations = 20;

  while ( (fabs((*phi - old) / *phi) > 1.0e-8) && maxIterations-- )
  {
    old = *phi;
    *phi = PI/ 2.0 - 2.0 * atan( t * pow((1.0 - e * sin(*phi)) / ((1.0 + e * sin(*phi))), (e / 2.0)) );
  }
}





// Converts from UTM/UPS coordinates to Lat/Long coordinates.
//
void utm_ups_to_ll(short ellipsoidID, const double utmNorthing, const double utmEasting,
                   const char* utmZone, double *lat,  double *lon)
{
  // Converts UTM coords to lat/long.  Equations from USGS
  // Bulletin 1532.  East Longitudes are positive, West longitudes
  // are negative.  North latitudes are positive, South latitudes
  // are negative Lat and Long are in decimal degrees.
  // Written by Chuck Gantz- chuck.gantz@globalstar.com
  // Modified by WE7U to add UPS support.

  double k0 = 0.9996;
  double a = gEllipsoid[ellipsoidID].a;
  double f = 1.0 / gEllipsoid[ellipsoidID].invf;
  double eccSquared = (2 * f) - (f * f);
  double eccPrimeSquared;
  double e1 = (1-sqrt(1-eccSquared))/(1+sqrt(1-eccSquared));
  double N1, T1, C1, R1, D, M;
  double LongOrigin;
  // phi1 is never used, but is set.  Don't make gcc warn us
  // double mu, phi1, phi1Rad;
  double mu, phi1Rad;
  double x, y;
  int ZoneNumber;
  char* ZoneLetter;
  // Unused variable
  // int NorthernHemisphere; // 1=northern hemisphere, 0=southern


  //fprintf(stderr,"%s  %f  %f\n",
  //    utmZone,
  //    utmEasting,
  //    utmNorthing);

  x = utmEasting;
  y = utmNorthing;

  ZoneNumber = strtoul(utmZone, &ZoneLetter, 10);

  // Remove any possible leading spaces
  remove_leading_spaces(ZoneLetter);

  // Make sure the zone letter is upper-case
  *ZoneLetter = toupper(*ZoneLetter);

  //fprintf(stderr,"ZoneLetter: %s\n", ZoneLetter);

  if (       *ZoneLetter == 'Y'       // North Pole
             || *ZoneLetter == 'Z'       // North Pole
             || *ZoneLetter == 'A'       // South Pole
             || *ZoneLetter == 'B')      // South Pole
  {

    // The following piece of code which implements UPS
    // conversion is derived from code that John Waers
    // <jfwaers@csn.net> placed in the public domain.  It's from
    // his program "MacGPS45".

    //
    // We're dealing with a UPS coordinate (near the poles)
    // instead of a UTM coordinate.  We need to do entirely
    // different calculations for UPS.
    //
    double e, t, rho;
    const double k0 = 0.994;


    //fprintf(stderr,"UPS Coordinates\n");


    e = sqrt(eccSquared);

    x -= 2.0e6; // Remove false easting and northing
    y -= 2.0e6;

    rho = sqrt(x*x + y*y);
    t = rho * sqrt(pow(1.0+e, 1.0+e) * pow(1.0-e, 1.0-e)) / (2.0 * a * k0);

    calcPhi(lat, e, t);

    *lat /= (PI/180.0);

    // This appears to be necessary in order to get proper
    // positions in the south polar region
    if (*ZoneLetter == 'A' || *ZoneLetter == 'B')
    {
      *lat = -*lat;
    }

    if (y != 0.0)
    {
      t = atan(fabs(x/y));
    }
    else
    {
      t = PI / 2.0;
      if (x < 0.0)
      {
        t = -t;
      }
    }

    if (*ZoneLetter == 'Z' || *ZoneLetter == 'Y')
    {
      y = -y;  // Northern hemisphere
    }

    if (y < 0.0)
    {
      t = PI - t;
    }

    if (x < 0.0)
    {
      t = -t;
    }

    *lon = t / (PI/180.0);

    /*
            fprintf(stderr,"datum.c:utm_ups_to_ll(): Found UPS Coordinate: %s %f %f\n",
                utmZone,
                utmEasting,
                utmNorthing);
    */
    return; // Done computing UPS coordinates
  }


  // If we make it here, we're working on UTM coordinates (not
  // UPS coordinates).


  x = utmEasting - 500000.0; //remove 500,000 meter offset for longitude
  y = utmNorthing;


  if ((*ZoneLetter - 'N') >= 0)
  {
    // We never use this variable
    // NorthernHemisphere = 1;//point is in northern hemisphere
  }
  else
  {
    // we never use NorthernHemisphere
    // NorthernHemisphere = 0;//point is in southern hemisphere
    y -= 10000000.0;//remove 10,000,000 meter offset used for southern hemisphere
  }

  LongOrigin = (ZoneNumber - 1)*6 - 180 + 3;  //+3 puts origin in middle of zone

  eccPrimeSquared = (eccSquared)/(1-eccSquared);

  M = y / k0;
  mu = M/(a*(1-eccSquared/4-3*eccSquared*eccSquared/64-5*eccSquared*eccSquared*eccSquared/256));

  phi1Rad = mu + (3*e1/2-27*e1*e1*e1/32)*sin(2*mu)
            + (21*e1*e1/16-55*e1*e1*e1*e1/32)*sin(4*mu)
            + (151*e1*e1*e1/96)*sin(6*mu);
  // This variable is never used, it is just phi1Rad converted to degrees
  //    phi1 = phi1Rad*rad2deg;

  N1 = a/sqrt(1-eccSquared*sin(phi1Rad)*sin(phi1Rad));
  T1 = tan(phi1Rad)*tan(phi1Rad);
  C1 = eccPrimeSquared*cos(phi1Rad)*cos(phi1Rad);
  R1 = a*(1-eccSquared)/pow(1-eccSquared*sin(phi1Rad)*sin(phi1Rad), 1.5);
  D = x/(N1*k0);

  *lat = phi1Rad - (N1*tan(phi1Rad)/R1)*(D*D/2-(5+3*T1+10*C1-4*C1*C1-9*eccPrimeSquared)*D*D*D*D/24
                                         +(61+90*T1+298*C1+45*T1*T1-252*eccPrimeSquared-3*C1*C1)*D*D*D*D*D*D/720);
  *lat *= rad2deg;

  *lon = (D-(1+2*T1+C1)*D*D*D/6+(5-2*C1+28*T1-3*C1*C1+8*eccPrimeSquared+24*T1*T1)
          *D*D*D*D*D/120)/cos(phi1Rad);
  *lon = LongOrigin + (*lon) * rad2deg;
}


