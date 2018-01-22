#!/usr/bin/perl -W
###########################################################################
#
#
# XASTIR, Amateur Station Tracking and Information Reporting
# Copyright (C) 2000-2018 The Xastir Group
#
# "ais.pl", a Perl script to connect aisdecoder and Xastir for receiving
# packets from ships. This script will create a UDP server at localhost:10110
# for aisdecoder to send packets into. It decodes AIS sentences, creates APRS
# packets out of them and sends them to Xastir's server port (2023) via UDP.
#
# AIS is the international tracking system for ships. They transmit on Marine
# VHF frequencies between 156.025 and 162.025, using a low power output of 1W,
# high power output of either 5W or 12.5W, at a bit rate of 9600 bits/s using
# NRZI data encoding and GMSK/FM at 0.5 modulation index.
#
# The two default AIS channels are:
# Ch 87B, AIS-1: 161.975 MHz
# Ch 88B, AIS-2: 162.025 MHz
#
# The two long-range channels are:
# Ch 75: 156.775 MHz
# Ch 76: 156.825 MHz
#
#
# You'll need "libusb", "libpthread", "librtlsdr" installed to be able to
# compile "rtl_ais".
#
# Get aisdecoder (rtl-ais).
#   git clone https://github.com/dgiardini/rtl-ais/
#   cd rtl_ais
#   make
#
#
# Run "rtl_ais" like this (create a simple script!) for the two default channels:
#
#   ./rtl_ais -h 127.0.0.1 -P 10110 -d 0 -l 161.975M -r 162.025M -n -p -2
#
# Change to marine channels 75 (156.775 MHz) and 76 (156.825 MHz) to pick
# up long-range AIS (Type 27 packets), perhaps with another dongle:
#
#   ./rtl_ais -h 127.0.0.1 -P 10110 -d 1 -l 156.775M -r 156.825M -n -p -2
#
# Note that "-p -2" is the frequency error of the RTL dongle.
# Set that to the proper number determined from "Kal".
#
# "-d 0" or "-d 1" selects which dongle you're attaching to.
#
# Don't use "-g <number>" and instead use auto-gain, which appears to work
# better. This also works better than enabling AGC in the chipset itself.
#
#
# Run "ais.pl" like this (again, create a simple script!):
#   ./ais.pl boats <passcode> [--pipe] [--logging]
#
# You may add the "--pipe" flag in order to have "ais.pl" accept data
# from a pipe instead of setting up a UDP server for "rtl_ais" to send
# packets to. This is very useful for debugging or for processing
# stored log files.
#
# If you add " --logging" to the end, this script will save the APRS portion of
# the output to a file called "~/.xastir/logs/ships.log". You can later suck
# this file back in to see the ships move around the map in hyperspeed. Useful
# for a quick demo.
#
#
# Antenna length should be:
# 1/2 wave dipole: 2.89' -or- 2' 10.7" -or- 2' 10 5/8"
# 1/4 wave: 1.44' -or- 1' 5.3" -or- 1' 5 5/16"
#
#
# Excellent AIS info:
#   http://catb.org/gpsd/AIVDM.html     # Eric Raymond/GPSD
#   https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.1371-5-201402-I!!PDF-E.pdf   # ITU Spec
#   http://www.navcen.uscg.gov/pdf/AIS/AIS_Special_Notice_and_AIS_Encoding_Guide_2012.pdf
#
# Example packets w/decoding of fields, plus large sample NMEA data file:
#   http://fossies.org/linux/gpsd/test/sample.aivdm
#
# Possible live data feed (Too many connections when I try):
#   ais.exploratorium.edu:80
#
###########################################################################


use IO::Socket;
use Storable;
#use File::HomeDir;
#use Data::Dumper;  # Only used for debugging vessel_hash


# Enable sending APRS packets to Xastir
$enable_tx    = 1;

# Enable the printing of a statistics line every 100 messages
# The format is: total type1 type2 ... type27
# Example: 3600 62 0 15 857 0 0 0 0 0 0 0 0 0 0 113 0 0 0 0 685 1868 0 0 0 0 0 0 
$statistics_mode = 1;

# Turn on/off printing of various types of messages. Enables debugging of a few
# sentence types at a time w/o other messages cluttering up the output.
$print_123    = 1;
$print_4      = 1;
$print_5      = 1;  # (includes vesselName and shipType)
$print_9      = 1;
$print_18     = 1;
$print_19     = 1;  # (includes vesselName)
$print_21     = 1;
$print_24_A   = 1;  # A Variant (includes vesselName)
$print_24_B   = 1;  # B Variant (includes shipType
$print_27     = 1;
$print_others = 1;  # Not decoded yet
 
# Keep statistics on each type (28 slots)
@message_type_count = (0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);
$statistics_count = 0; # Number of messages before printing stats 

# Hash to store tac-calls in, to minimize tac-call assignment packets
# This stores the complete tactical call, including whatever name
# the ship goes by currently + the country of registry.
my %tactical_hash;

# Create external copy of vessel_hash for persistent store.
# This contains only the vessel name, not the country of registry
# or the 9-digit MMSI number.
$home = `echo ~`;
chomp $home;
$persistentFileSpec = "$home/.xastir/config/vessel_hash";
#$persistentFileSpec = File::HomeDir->my_home . "/.xastir/config/vessel_hash";
#
#
# Hash to store vessel names in, for assigning tactical calls
# If it doesn't exist, create it with some initial values
# For final release this should simply create a dummy file
my %vessel_hash;

if ( !(-e $persistentFileSpec )) {
    %vessel_hash = (
    );
	
	# Create the new hash file
	store \%vessel_hash , $persistentFileSpec;
}

# Retrieve the cache
%vessel_hash = %{retrieve($persistentFileSpec)};

# Debug - print the hash values 
#print Dumper(\%vessel_hash); 


$udp_client = "xastir_udp_client";

$xastir_host = "localhost"; # Server where Xastir is running
$xastir_port = 2023;        # 2023 is Xastir default UDP port

$log_file = "~/.xastir/logs/ships.log";


%countries = (
    "201" => "Albania", #"Albania (Republic of)"
    "202" => "Andorra",    # "Andorra (Principality of)"
    "203" => "Austria",
    "204" => "Azores",    # "Azores - Portugal"
    "205" => "Belgium",
    "206" => "Belarus",    # "Belarus (Republic of)"
    "207" => "Bulgaria",    # "Bulgaria (Republic of)"
    "208" => "Vatican",    # "Vatican City State"
    "209" => "Cyprus",    # "Cyprus (Republic of)"
    "210" => "Cyprus",    # "Cyprus (Republic of)"
    "211" => "Germany",    # "Germany (Federal Republic of)"
    "212" => "Cyprus",    # "Cyprus (Republic of)"
    "213" => "Georgia",
    "214" => "Moldova",    # "Moldova (Republic of)"
    "215" => "Malta",
    "216" => "Armenia",    # "Armenia (Republic of)"
    "218" => "Germany",    # "Germany (Federal Republic of)"
    "219" => "Denmark",
    "220" => "Denmark",
    "224" => "Spain",
    "225" => "Spain",
    "226" => "France",
    "227" => "France",
    "228" => "France",
    "229" => "Malta",
    "230" => "Finland",
    "231" => "Faroe Is.",    # "Faroe Islands - Denmark"
    "232" => "U.K.",    # "United Kingdom of Great Britain and Northern Ireland"
    "233" => "U.K.",    # "United Kingdom of Great Britain and Northern Ireland"
    "234" => "U.K.",    # "United Kingdom of Great Britain and Northern Ireland"
    "235" => "U.K.",    # "United Kingdom of Great Britain and Northern Ireland"
    "236" => "Gibraltar",    # "Gibraltar - United Kingdom of Great Britain and Northern Ireland"
    "237" => "Greece",
    "238" => "Croatia",    # "Croatia (Republic of)"
    "239" => "Greece",
    "240" => "Greece",
    "241" => "Greece",
    "242" => "Morocco",    # "Morocco (Kingdom of)"
    "243" => "Hungary",
    "244" => "Netherlands",    # "Netherlands (Kingdom of the)"
    "245" => "Netherlands",    # "Netherlands (Kingdom of the)"
    "246" => "Netherlands",    # "Netherlands (Kingdom of the)"
    "247" => "Italy",
    "248" => "Malta",
    "249" => "Malta",
    "250" => "Ireland",
    "251" => "Iceland",
    "252" => "Liechtenstein",    # "Liechtenstein (Principality of)"
    "253" => "Luxembourg",
    "254" => "Monaco",    # "Monaco (Principality of)"
    "255" => "Madeira",    # "Madeira - Portugal"
    "256" => "Malta",
    "257" => "Norway",
    "258" => "Norway",
    "259" => "Norway",
    "261" => "Poland",    # "Poland (Republic of)"
    "262" => "Montenegro",
    "263" => "Portugal",
    "264" => "Romania",
    "265" => "Sweden",
    "266" => "Sweden",
    "267" => "Slovak",    # "Slovak Republic"
    "268" => "San Marino",    # "San Marino (Republic of)"
    "269" => "Switzerland",    # "Switzerland (Confederation of)"
    "270" => "Czech Rep.",    # "Czech Republic"
    "271" => "Turkey",
    "272" => "Ukraine",
    "273" => "Russian Fed.",    # "Russian Federation"
    "274" => "Macedonia",    # "The Former Yugoslav Republic of Macedonia"
    "275" => "Latvia",    # "Latvia (Republic of)"
    "276" => "Estonia",    # "Estonia (Republic of)"
    "277" => "Lithuania",    # "Lithuania (Republic of)"
    "278" => "Slovenia",    # "Slovenia (Republic of)"
    "279" => "Serbia",    # "Serbia (Republic of)"
    "301" => "Anguilla",    # "Anguilla - United Kingdom of Great Britain and Northern Ireland"
    "303" => "Alaska,U.S.",    # "Alaska (State of) - United States of America"
    "304" => "Antigua & Barbuda",    # "Antigua and Barbuda"
    "305" => "Antigua & Barbuda",    # "Antigua and Barbuda"
    "306" => "Sint Maarten",    # "Sint Maarten (Dutch part) - Netherlands (Kingdom of the)"
    "306" => "Bonaire, Sint Eustatius & Saba",    # "Bonaire, Sint Eustatius and Saba - Netherlands (Kingdom of the)"
    "306" => "Curaçao",    # "Curaçao - Netherlands (Kingdom of the)"
    "307" => "Aruba",    # "Aruba - Netherlands (Kingdom of the)"
    "308" => "Bahamas",    # "Bahamas (Commonwealth of the)"
    "309" => "Bahamas",    # "Bahamas (Commonwealth of the)"
    "310" => "Bermuda",    # "Bermuda - United Kingdom of Great Britain and Northern Ireland"
    "311" => "Bahamas",    # "Bahamas (Commonwealth of the)"
    "312" => "Belize",
    "314" => "Barbados",
    "316" => "Canada",
    "319" => "Cayman Is.",    # "Cayman Islands - United Kingdom of Great Britain and Northern Ireland"
    "321" => "Costa Rica",
    "323" => "Cuba",
    "325" => "Dominica",    # "Dominica (Commonwealth of)"
    "327" => "Dominican Rep.",    # "Dominican Republic"
    "329" => "Guadeloupe",    # "Guadeloupe (French Department of) - France"
    "330" => "Grenada",
    "331" => "Greenland",    # "Greenland - Denmark"
    "332" => "Guatemala",    # "Guatemala (Republic of)"
    "334" => "Honduras",    # "Honduras (Republic of)"
    "336" => "Haiti",    # "Haiti (Republic of)"
    "338" => "U.S.", # "United States of America"
    "339" => "Jamaica",
    "341" => "St. Kitts & Nevis",    # "Saint Kitts and Nevis (Federation of)"
    "343" => "St. Lucia",
    "345" => "Mexico",
    "347" => "Martinique",    # "Martinique (French Department of) - France"
    "348" => "Montserrat",    # "Montserrat - United Kingdom of Great Britain and Northern Ireland"
    "350" => "Nicaragua",
    "351" => "Panama",    # "Panama (Republic of)"
    "352" => "Panama",    # "Panama (Republic of)"
    "353" => "Panama",    # "Panama (Republic of)"
    "354" => "Panama",    # "Panama (Republic of)"
    "355" => "Panama",    # "Panama (Republic of)"
    "356" => "Panama",    # "Panama (Republic of)"
    "357" => "Panama",    # "Panama (Republic of)"
    "358" => "Puerto Rico,U.S.",   # "Puerto Rico - United States of America"
    "359" => "El Salvador",    # "El Salvador (Republic of)"
    "361" => "St. Pierre & Miquelon",    # "Saint Pierre and Miquelon (Territorial Collectivity of) - France"
    "362" => "Trinidad & Tobago",    # "Trinidad and Tobago"
    "364" => "Turks & Caicos Is.",    # "Turks and Caicos Islands - United Kingdom of Great Britain and Northern Ireland"
    "366" => "U.S.", # "United States of America"
    "367" => "U.S.", # "United States of America"
    "368" => "U.S.", # "United States of America"
    "369" => "U.S.", # "United States of America"
    "370" => "Panama",    # "Panama (Republic of)"
    "371" => "Panama",    # "Panama (Republic of)"
    "372" => "Panama",    # "Panama (Republic of)"
    "373" => "Panama",    # "Panama (Republic of)"
    "374" => "Panama",    # "Panama (Republic of)"
    "375" => "St. Vincent & Grenadines",    # "Saint Vincent and the Grenadines"
    "376" => "St. Vincent & Grenadines",    # "Saint Vincent and the Grenadines"
    "377" => "St. Vincent & Grenadines",    # "Saint Vincent and the Grenadines"
    "378" => "British Virgin Is.",    # "British Virgin Islands - United Kingdom of Great Britain and Northern Ireland"
    "379" => "U.S. Virgin Is.",    # "United States Virgin Islands - United States of America"
    "401" => "Afghanistan",
    "403" => "Saudi Arabia",    # "Saudi Arabia (Kingdom of)"
    "405" => "Bangladesh",    # "Bangladesh (People's Republic of)"
    "408" => "Bahrain",    # "Bahrain (Kingdom of)"
    "410" => "Bhutan",    # "Bhutan (Kingdom of)"
    "412" => "China",    # "China (People's Republic of)"
    "413" => "China",    # "China (People's Republic of)"
    "414" => "China",    # "China (People's Republic of)"
    "416" => "Taiwan",    # "Taiwan (Province of China) - China (People's Republic of)"
    "417" => "Sri Lanka",    # "Sri Lanka (Democratic Socialist Republic of)"
    "419" => "India",    # "India (Republic of)"
    "422" => "Iran",    # "Iran (Islamic Republic of)"
    "423" => "Azerbaijan",    # "Azerbaijan (Republic of)"
    "425" => "Iraq",    # "Iraq (Republic of)"
    "428" => "Israel",    # "Israel (State of)"
    "431" => "Japan",
    "432" => "Japan",
    "434" => "Turkmenistan",
    "436" => "Kazakhstan",    # "Kazakhstan (Republic of)"
    "437" => "Uzbekistan",    # "Uzbekistan (Republic of)"
    "438" => "Jordan",    # "Jordan (Hashemite Kingdom of)"
    "440" => "Korea",    # "Korea (Republic of)"
    "441" => "Korea",    # "Korea (Republic of)"
    "443" => "Palestine",    # "State of Palestine (In accordance with Resolution 99 Rev. Guadalajara, 2010)"
    "445" => "N. Korea",    # "Democratic People's Republic of Korea"
    "447" => "Kuwait",    # "Kuwait (State of)"
    "450" => "Lebanon",
    "451" => "Kyrgyz Rep.",    # "Kyrgyz Republic"
    "453" => "Macao",    # "Macao (Special Administrative Region of China) - China (People's Republic of)"
    "455" => "Maldives",    # "Maldives (Republic of)"
    "457" => "Mongolia",
    "459" => "Nepal",    # "Nepal (Federal Democratic Republic of)"
    "461" => "Oman",    # "Oman (Sultanate of)"
    "463" => "Pakistan",    # "Pakistan (Islamic Republic of)"
    "466" => "Qatar",    # "Qatar (State of)"
    "468" => "Syria",    # "Syrian Arab Republic"
    "470" => "United Arab Emirates",
    "472" => "Tajikistan",    # "Tajikistan (Republic of)"
    "473" => "Yemen",    # "Yemen (Republic of)"
    "475" => "Yemen",    # "Yemen (Republic of)"
    "477" => "Hong Kong",    # "Hong Kong (Special Administrative Region of China) - China (People's Republic of)"
    "478" => "Bosnia & Herzegovina",    # "Bosnia and Herzegovina"
    "501" => "Adelie Land",    # "Adelie Land - France"
    "503" => "Australia",
    "506" => "Myanmar",    # "Myanmar (Union of)"
    "508" => "Brunei Darussalam",
    "510" => "Micronesia",    # "Micronesia (Federated States of)"
    "511" => "Palau",    # "Palau (Republic of)"
    "512" => "New Zealand",
    "514" => "Cambodia",    # "Cambodia (Kingdom of)"
    "515" => "Cambodia",    # "Cambodia (Kingdom of)"
    "516" => "Christmas Is.",    # "Christmas Island (Indian Ocean) - Australia"
    "518" => "Cook Is.",    # "Cook Islands - New Zealand"
    "520" => "Fiji",    # "Fiji (Republic of)",
    "523" => "Cocos (Keeling) Is.",    # "Cocos (Keeling) Islands - Australia"
    "525" => "Indonesia",    # "Indonesia (Republic of)"
    "529" => "Kiribati",    # "Kiribati (Republic of)"
    "531" => "Laos",    # "Lao People's Democratic Republic"
    "533" => "Malaysia",
    "536" => "N. Mariana Is,U.S.",    # "Northern Mariana Islands (Commonwealth of the) - United States of America"
    "538" => "Marshall Is.",    # "Marshall Islands (Republic of the)"
    "540" => "New Caledonia",    # "New Caledonia - France"
    "542" => "Niue",    # "Niue - New Zealand"
    "544" => "Nauru",    # "Nauru (Republic of)"
    "546" => "French Polynesia",    # "French Polynesia - France"
    "548" => "Philippines",    # "Philippines (Republic of the)"
    "553" => "Papua New Guinea",
    "555" => "Pitcairn Is.",    # "Pitcairn Island - United Kingdom of Great Britain and Northern Ireland"
    "557" => "Solomon Islands",
    "559" => "American Samoa,U.S.",   # "American Samoa - United States of America"
    "561" => "Samoa",    # "Samoa (Independent State of)"
    "563" => "Singapore",    # "Singapore (Republic of)"
    "564" => "Singapore",    # "Singapore (Republic of)"
    "565" => "Singapore",    # "Singapore (Republic of)"
    "566" => "Singapore",    # "Singapore (Republic of)"
    "567" => "Thailand",
    "570" => "Tonga",    # "Tonga (Kingdom of)"
    "572" => "Tuvalu",
    "574" => "Viet Nam",    # "Viet Nam (Socialist Republic of)"
    "576" => "Vanuatu",    # "Vanuatu (Republic of)"
    "577" => "Vanuatu",    # "Vanuatu (Republic of)"
    "578" => "Wallis & Futuna Is.",    # "Wallis and Futuna Islands - France"
    "601" => "S. Africa",    # "South Africa (Republic of)"
    "603" => "Angola",    # "Angola (Republic of)"
    "605" => "Algeria",    # "Algeria (People's Democratic Republic of)"
    "607" => "St. Paul & Amsterdam Is.",    # "Saint Paul and Amsterdam Islands - France"
    "608" => "Ascension Is.",    # "Ascension Island - United Kingdom of Great Britain and Northern Ireland"
    "609" => "Burundi",    # "Burundi (Republic of)"
    "610" => "Benin",    # "Benin (Republic of)"
    "611" => "Botswana",    # "Botswana (Republic of)"
    "612" => "Central African Rep.",    # "Central African Republic"
    "613" => "Cameroon",    # "Cameroon (Republic of)"
    "615" => "Congo",    # "Congo (Republic of the)"
    "616" => "Comoros",    # "Comoros (Union of the)"
    "617" => "Cabo Verde",    # "Cabo Verde (Republic of)"
    "618" => "Crozet Archipelago",    # "Crozet Archipelago - France"
    "619" => "Côte d'Ivoire",    # "Côte d'Ivoire (Republic of)"
    "620" => "Comoros",    # "Comoros (Union of the)"
    "621" => "Djibouti",    # "Djibouti (Republic of)"
    "622" => "Egypt",    # "Egypt (Arab Republic of)"
    "624" => "Ethiopia",    # "Ethiopia (Federal Democratic Republic of)"
    "625" => "Eritrea",
    "626" => "Gabonese Rep.",    # "Gabonese Republic"
    "627" => "Ghana",
    "629" => "Gambia",    # "Gambia (Republic of the)"
    "630" => "Guinea-Bissau",    # "Guinea-Bissau (Republic of)"
    "631" => "Equatorial Guinea",    # "Equatorial Guinea (Republic of)"
    "632" => "Guinea",    # "Guinea (Republic of)"
    "633" => "Burkina Faso",
    "634" => "Kenya",    # "Kenya (Republic of)"
    "635" => "Kerguelen Is.",    # "Kerguelen Islands - France"
    "636" => "Liberia",    # "Liberia (Republic of)"
    "637" => "Liberia",    # "Liberia (Republic of)"
    "638" => "S. Sudan",    # "South Sudan (Republic of)"
    "642" => "Libya",
    "644" => "Lesotho",    # "Lesotho (Kingdom of)"
    "645" => "Mauritius",    # "Mauritius (Republic of)"
    "647" => "Madagascar",    # "Madagascar (Republic of)"
    "649" => "Mali",    # "Mali (Republic of)"
    "650" => "Mozambique",    # "Mozambique (Republic of)"
    "654" => "Mauritania",    # "Mauritania (Islamic Republic of)"
    "655" => "Malawi",
    "656" => "Niger",    # "Niger (Republic of the)"
    "657" => "Nigeria",    # "Nigeria (Federal Republic of)"
    "659" => "Namibia",    # "Namibia (Republic of)"
    "660" => "Reunion",    # "Reunion (French Department of) - France"
    "661" => "Rwanda",    # "Rwanda (Republic of)"
    "662" => "Sudan",    # "Sudan (Republic of the)"
    "663" => "Senegal",    # "Senegal (Republic of)"
    "664" => "Seychelles",    # "Seychelles (Republic of)"
    "665" => "St. Helena",    # "Saint Helena - United Kingdom of Great Britain and Northern Ireland"
    "666" => "Somalia",    # "Somalia (Federal Republic of)"
    "667" => "Sierra Leone",
    "668" => "Sao Tome & Principe",    # "Sao Tome and Principe (Democratic Republic of)"
    "669" => "Swaziland",    # "Swaziland (Kingdom of)"
    "670" => "Chad",    # "Chad (Republic of)"
    "671" => "Togolese Rep.",    # "Togolese Republic"
    "672" => "Tunisia",
    "674" => "Tanzania",    # "Tanzania (United Republic of)"
    "675" => "Uganda",    # "Uganda (Republic of)",
    "676" => "Dem. Rep. of Congo",    # "Democratic Republic of the Congo"
    "677" => "Tanzania",    # "Tanzania (United Republic of)"
    "678" => "Zambia",    # "Zambia (Republic of)"
    "679" => "Zimbabwe",    # "Zimbabwe (Republic of)"
    "701" => "Argentine Rep.",    # "Argentine Republic"
    "710" => "Brazil",    # "Brazil (Federative Republic of)"
    "720" => "Bolivia",    # "Bolivia (Plurinational State of)"
    "725" => "Chile",
    "730" => "Columbia",    # "Colombia (Republic of)"
    "735" => "Ecuador",
    "740" => "Falkland Is.",    # "Falkland Islands (Malvinas) - United Kingdom of Great Britain and Northern Ireland"
    "745" => "Guiana",    # "Guiana (French Department of) - France"
    "750" => "Guyana",
    "755" => "Parguay",    # "Paraguay (Republic of)"
    "760" => "Peru",
    "765" => "Suriname",    # "Suriname (Republic of)"
    "770" => "Uruguay",    # "Uruguay (Eastern Republic of)"
    "775" => "Venezuela",    # "Venezuela (Bolivarian Republic of)"
);




$xastir_user = shift;
if (defined($xastir_user)) {
  chomp $xastir_user;
}
if ( (!defined($xastir_user)) || ($xastir_user eq "") ) {
    print "Please enter a callsign for Xastir injection, but not Xastir's callsign/SSID!\n";
    die;
}
$xastir_user =~ tr/a-z/A-Z/;


$xastir_pass = shift;
if (defined($xastir_pass)) {
  chomp $xastir_pass;
}
if ( (!defined($xastir_pass)) || ($xastir_pass eq "") ) {
    print "Please enter a passcode for Xastir injection\n";
    die;
}


# Enable pipe mode by adding the "--pipe" flag, then can pipe
# packets into the program's STDIN for testing. Without this flag
# we'll get our packets from "rtl_ais" via a UDP server we set up.
# Enable logging mode by adding the "--logging" flag.
$pipe_mode = 0;
$logging_mode = 0;
$pipe_flag = shift;
if (defined($pipe_flag) && $pipe_flag ne "") {
    chomp $pipe_flag;
    if ($pipe_flag eq "--pipe") { $pipe_mode = 1; }
    if ($pipe_flag eq "--logging") { $logging_mode = 1; }
}

# Try again in case there are two flags
$pipe_flag = shift;
if (defined($pipe_flag) && $pipe_flag ne "") {
    chomp $pipe_flag;
    if ($pipe_flag eq "--pipe") { $pipe_mode = 1; }
    if ($pipe_flag eq "--logging") { $logging_mode = 1; }
}


if ($enable_tx) {
    # Check Xastir's callsign/SSID to make sure we don't have a collision.  This
    # will prevent Xastir adopting the Items as its own and retransmitting them.
    #   "xastir_udp_client localhost 2023 <callsign> <passcode> -identify"
    #   "Received: WE7U-13"
    #
    $injection_call = $xastir_user;
    $injection_call =~ s/-\d+//;    # Get rid of dash and numbers

    $injection_ssid = $xastir_user;
    $injection_ssid =~ s/\w+//;     # Get rid of letters
    $injection_ssid =~ s/-//;       # Get rid of dash
    if ($injection_ssid eq "") { $injection_ssid = 0; }

    # Find out Callsign/SSID of Xastir instance
    $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass -identify`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }
    ($remote_call, $remote_ssid) = split('-', $result);

    chomp($remote_call);
    $remote_call =~ s/Received:\s+//;

    if ( !defined($remote_ssid) ) { $remote_ssid = ""; }
    if ($remote_ssid ne "") { chomp($remote_ssid); }
    if ($remote_ssid eq "") { $remote_ssid = 0; }

    #print "$remote_call $remote_ssid $injection_call $injection_ssid\n";
    #if ($remote_call eq $injection_call) { print "Call matches\n"; }
    #if ($remote_ssid == $injection_ssid) { print "SSID matches\n"; }

    if (     ($remote_call eq $injection_call)
         &&  ($remote_ssid == $injection_ssid) ) {
        $remote_ssid++;
        $remote_ssid%= 16;  # Increment by 1 mod 16
        $xastir_user = "$remote_call-$remote_ssid";
        print "Injection conflict. Corrected. New user = $xastir_user\n";
    }
}


if ($pipe_mode) {
    #
    # Debug mode:
    # Read from pipe in a loop
    #
    while (<STDIN>) {
        &process_line($_);
    }
}
else {
    #
    # Not debug mode:
    # Create UDP server which connects to rtl_ais to get AIS data
    # Listen on localhost port 10110 for UDP packets
    #
    my $host = "localhost";
    my $port = 10110;
    my $protocol = 'udp';

    my $server = IO::Socket::INET->new(
        #PeerAddr => $host,
        LocalPort => $port,
        Proto    => $protocol,
        Type     => SOCK_DGRAM
    ) or die "Socket could not be created, failed with error: $!\n";


    # Main processing loop. Fetch packets as they are received via
    # UDP from port 10110. Create APRS packets out of the interesting
    # ones and inject them into Xastir at port 2023 using UDP.
    #
    while ($server->recv($received_data, 1024)) {

        #my $peer_address = $server->peerhost();
        #my $peer_port    = $server->peerport();
        #print "Message was received from: $peer_address, $peer_port\n";
        #print "$received_data";

        &process_line($received_data);

    } # End of main processing loop
}



sub process_line () {

    $received_data = shift;

    if ($pipe_mode) {
        $received_data =~ s/\n//;   # Remove LF if present
        $received_data =~ s/\r//;   # Remove CR if present
    }
    else {  # Live mode, receiving from rtl-ais. CRLF line-ends.
        chop($received_data);
        chop($received_data);
    }

    # Check for empty line
    if ($received_data eq "") { return; }

    # Verify the checksum: Refuse to process a message if it's bad.
    # Computed on entire sentence including the AIVDM tag but excluding
    # the leading "!" character.
    my $str = $received_data;
    $str =~ s/^!//;             # Chop off leading '!'
    #$str =~ s/(,\d)\*../$1/;    # Chop to '*' char with leading digit and two trailing chars
    $str =~ s/(,\d)\*...*/$1/;    # Chop to '*' char with leading digit and at least two trailing chars
    #print "$str\n";

    # Received checksum. Catches the correct bytes except
    # for those cases where the NMEA sentences continues
    # on past the checksum.
    my $str2 = $received_data;
    #$str2 =~ s/.*(..)/$1/;         # Catches only a checksum at the end
    $str2 =~ s/.*,\d\*(..).*/$1/;   # Catches checksum at end or middle

    # Compute the checksum on the string between the '!' and the '*' chars.
    $j = length($str);
    my $sum = 0;
    my $i;
    for ($i = 0; $i <= $j; $i++) {
        my $c = ord( substr($str, $i, 1) );
        $sum = $sum ^ $c;
    }
    my $sum2 = sprintf( "%02X", $sum);
    if ($str2 ne $sum2) {
	    #print "$received_data\n";
    	#print "$str\t$str2\t$sum2\n";
    	printf("***** Bad checksum: Received:%s  Computed:%s $received_data\n\n", $str2, $sum2);
        return(); # Skip this sentence
    }
    else {
        #print "$str2\t$sum2\n";
    }

    @pieces = split( ',', $received_data);

    # $pieces[0] = NMEA message type
    # $pieces[1] = Number of sentences
    # $pieces[2] = Sequential Message ID
    # $pieces[3] = Sentence number
    # $pieces[4] = AIS Channel
    # $pieces[5] = Encoded AIS data
    # $pieces[6] = End of data & NMEA Checksum

    $data = $pieces[5];

    # Encoded AIS data: Each ASCII character represents 6 bits.
    # Subtract 48. If still a decimal number > 40, subtract 8.
    # Convert to binary.
    $bin_string = "";
    $len = length($data);
    for ($i = 0; $i < $len; $i++) {
        $c = substr($data, $i, 1);
        #print("$c");
        $d = ord($c) - 48;
        if ($d > 40) { $d = $d - 8; }
        #print "\t$d\n";

        # Convert each character to 6 bits of binary.
        # Add the binary strings together to make one
        # long binary string. Pull out the bits we need
        # to decode each piece of the message.
        #
        $b = &dec2bin($d);
        #print "\t$b\n";
        $bin_string = $bin_string . $b;
    }
    #print "$bin_string\n";
    #printf("%d\n", length($data) );
    #printf("%d\n", length($bin_string) );


    # If first six bits are 000001 (message type 1):
    # MMSI Number - bit 8 for 30 bits
    # HDG - bit 128 for 9 bits
    # COG - bit 116 for 12 bits (and divide by 10)
    # SOG - bit 50 for 10 bits (and divide by 10)
    # Lat - bit 89 for 27 bits (a signed binary number, divide by 600,000)
    # Lon - bit 61 for 28 bits (a signed binary number, divide by 600,000)
    #
    #if (substr($bin_string,0,6) eq "000001") {
    #    print "Message Type: 1\n";
    #}
    #else {
    #    printf"Message Type: %s\n", substr($bin_string,0,6);
    #    die;
    #}

    $bmessage_type = substr($bin_string, 0, 6);
    $message_type = &bin2dec($bmessage_type);

    if ($statistics_mode) {
        # Keep a count of the received message types
        if ($message_type < 28) {
            $message_type_count[$message_type] += 1;
        }

        # Since there's no message type 0 use it's array location for total
        $message_type_count[0] += 1;

        # Print the totals every 100 messages
        if ( ++$statistics_count >= 100 ) {
            printf("Stats:  ");
            for (my $jj = 0; $jj < 28; $jj++) {
                if ($jj == 0) { printf("%d", $message_type_count[$jj]); }
                else {          printf(",%d", $message_type_count[$jj]); }
            }
            print "\n\n";
            $statistics_count = 0;
        }
    }

    # The escape clause for bad message types is here
    # so that they get included in the total count above.
    # Don't move this escape above that code.
    if ( ($message_type > 27) || ($message_type == 0) ) {
#        print "***** Undefined Msg Type $message_type: $received_data\n\n";
        return();
    }

    #print "Message Type: $message_type\n";
    # If not messages types 1, 2, 3, skip for now
    if (    $message_type !=  1
         && $message_type !=  2
         && $message_type !=  3
	 && $message_type !=  4
         && $message_type !=  5
         && $message_type !=  9
         && $message_type != 18
         && $message_type != 19
	 && $message_type != 21
         && $message_type != 24
         && $message_type != 27
                                 ) {
        # Not something we decode so don't process the sentence.
        if ($print_others == 1) { print " Msg Type: $message_type\n\n"; }
        return;
    }

    if (    $message_type ==  1
         || $message_type ==  2
         || $message_type ==  3 ) {
        if ($print_123) { print " Msg Type: $message_type\n"; }
        &process_types_1_2_3($message_type);
    }

    elsif ( $message_type == 4 ) {
        if ($print_4) { print " Msg Type: $message_type\n"; }
        &process_type_4($message_type);
    }

    elsif ( $message_type == 5 ) {
        if ($print_5) { print " Msg Type: $message_type\n"; }
        &process_type_5($message_type);
    }

    elsif ( $message_type == 9 ) {
        if ($print_9) { print " Msg Type: $message_type\n"; }
        &process_type_9($message_type);
    }

    elsif ( $message_type == 18 ) {
        if ($print_18) { print " Msg Type: $message_type\n"; }
        &process_type_18($message_type);
 
    }

    elsif ( $message_type == 19 ) {
        if ($print_19) { print " Msg Type: $message_type\n"; }
        &process_type_19($message_type);
 
    }

    elsif ( $message_type == 21 ) {
        if ($print_21) { print " Msg Type: $message_type\n"; }
        &process_type_21($message_type);

    }

    elsif ( $message_type == 24 ) {
        #if ($print_24) { print " Msg Type: $message_type\t"; }   # This is printed from the subroutine instead
        &process_type_24($message_type);
    }

    elsif ( $message_type == 27 ) {
        if ($print_27) { print " Msg Type: $message_type\n"; }
if ($print_27) { print "$received_data\n"; }
        &process_type_27($message_type);
    }
   
} # End of "process_line"



# Message types 1, 2 and 3: Position Report Class A
# These messages should have 168 bits total
#
sub process_types_1_2_3() {

    #if ( length($bin_string) < 168 ) {
    if ( length($bin_string) < 128 ) {
        #printf("123\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("123\t%d\n", length($bin_string));
        if ($print_123) { print "Msg too short\n\n"; }
        return;
    }

    # substr($bin_string,   0,   6); # Message Type
    #my $brepeat_indicator = substr($bin_string, 6, 2);
    my $bUserID = substr($bin_string, 8, 30); # MMSI
    my $bnavStatus = substr($bin_string, 38, 4);
    #my $brateOfTurn = substr($bin_string, 42, 8);
    my $bSpeedOverGnd = substr($bin_string, 50, 10);    #####
    #my $bpositionAccuracy = substr($bin_string, 60, 1);
    my $bLongitude = substr($bin_string, 61, 28);   #####
    my $bLatitude = substr($bin_string, 89, 27);    #####
    my $bCourseOverGnd = substr($bin_string, 116, 12); #####
    #my $btrueHeading = substr($bin_string, 128, 9);
    #my $btimestamp = substr($bin_string, 137, 6);
    #my $bspecialManeuver = substr($bin_string, 143, 2);
    #my $bspare = substr($bin_string, 145, 3);
    #my $bRAIM = substr($bin_string, 148, 1);
    #my $bCommState = substr($bin_string, 149, 19);

    my $userID = &bin2dec($bUserID);
    if ($print_123) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_123) { print "  Country: $country\n"; }

    my $navStatus = &bin2dec($bnavStatus);
    my $navStatusTxt = "";
    if    ($navStatus ==  0) { $navStatusTxt = "Under_way:engine"; }
    elsif ($navStatus ==  1) { $navStatusTxt = "At_anchor"; }
    elsif ($navStatus ==  2) { $navStatusTxt = "Not_under_command"; }
    elsif ($navStatus ==  3) { $navStatusTxt = "Restricted_maneuverability"; }
    elsif ($navStatus ==  4) { $navStatusTxt = "Constrained_by_draught"; }
    elsif ($navStatus ==  5) { $navStatusTxt = "Moored"; }
    elsif ($navStatus ==  6) { $navStatusTxt = "Aground"; }
    elsif ($navStatus ==  7) { $navStatusTxt = "Fishing"; }
    elsif ($navStatus ==  8) { $navStatusTxt = "Under_way:sailing"; }
    #elsif ($navStatus ==  9) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 10) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 11) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 12) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 13) { $navStatusTxt = "Reserved"; }
    elsif ($navStatus == 14) { $navStatusTxt = "AIS-SART_is_active"; }
    #elsif ($navStatus == 15) { $navStatusTxt = "Not defined"; }

# NOTE: 0-359 degrees
# NOTE: 360 = N/A
# APRS spec says can set to "000", "...", or "   " if unknown
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    my $course = "";
    if ($courseOverGnd == 360) {
        $course = "...";
    }
    elsif ($courseOverGnd == 0) {
        $course = "360";
    }
    else {
        $course = sprintf("%03d", $courseOverGnd);
    }
    if ($print_123) { print "   Course: $courseOverGnd\n"; }

# NOTE: 0 to 102 knots
# NOTE: 102.3 = N/A
# NOTE: 102.2 = 102.2 knots or higher
# APRS spec says can set to "000", "...", or "   " if unknown
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    my $speed = "";
    if ($speedOverGnd == 102.3) {
        $speed = "...";
    }
    else {
        $speed = sprintf("%03d", $speedOverGnd);
    }
    if ($print_123) { print "    Speed: $speedOverGnd\n"; }
 
# NOTE: -90 to +90
# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
    if ($latitude == 91) {
        if ($print_123) { print "\n"; }
        return();
    }
    my $NS;
    if ($print_123) { printf(" Latitude: %07.5f\n", $latitude); }
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

# NOTE: -180 to +180
# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
    if ($longitude == 181) {
        if ($print_123) { print "\n"; }
        return();
    }
    my $EW;
    if ($print_123) { printf("Longitude: %08.5f\n", $longitude); }
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $symbol = "s";

    # Set up to add country or vessel_name + country to comment field
    my $vesselTag = " ($country)";
    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }

    my $aprs= &escape_from_shell("$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed $navStatusTxt$vesselTag");
    if ($print_123) { print "     APRS: $aprs\n"; }

    &log_aprs($aprs);
 
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }

    # Assign tactical call = "$userID + $country" or "$vesselName + $country"
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    my $temp;
    if ( defined( $vessel_hash{$userID}) ) {
        $temp = substr($vessel_hash{$userID} . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    else {
        # No vessel name
        # N7IPB: Display country of registry
        $temp = substr("($country)", 0, 56);  # Chop at 56 chars

        # WE7U: Display the 9-digit MMSI + country of registry
        #$temp = substr($userID . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    if ( !defined($tactical_hash{$userID}) ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_123) { print "     APRS: $aprs\n"; }

        &log_aprs($aprs);
 
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }

    if ($print_123) { print "\n"; }

    return();
}

# Message type 4: Base station report 
# These messages should have 168 bits total
#
sub process_type_4() {

    #if ( length($bin_string) < 168 ) {
    if ( length($bin_string) < 128 ) {
        #printf("123\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("123\t%d\n", length($bin_string));
        if ($print_4) { print "Msg too short\n\n"; }
        return;
    }

    my $bUserID = substr($bin_string, 8, 30); # MMSI
    my $bLongitude = substr($bin_string, 79, 28);   #####
    my $bLatitude = substr($bin_string, 107, 27);    #####
    my $userID = &bin2dec($bUserID);
    if ($print_4) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_4) { print "  Country: $country\n"; }


# NOTE: -90 to +90
# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
    if ($latitude == 91) { return(); }
    my $NS;
    if ($print_4) { printf(" Latitude: %07.5f\n", $latitude); }
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

# NOTE: -180 to +180
# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
    if ($longitude == 181) { return(); }
    my $EW;
    if ($print_4) { printf("Longitude: %08.5f\n", $longitude); }
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $symbol = "/";

    # Set up to add country or vessel_name + country to comment field
    my $vesselTag = " Fixed Base Station";

    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }

    my $aprs= &escape_from_shell("$xastir_user>APRS:)$userID!$lat/$lon$symbol $vesselTag");
    if ($print_4) { print "     APRS: $aprs\n"; }
    &log_aprs($aprs);
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }

    # Assign tactical call = "$userID + $country" or "$vesselName + $country"
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    my $temp;
#    if ( defined( $vessel_hash{$userID}) ) {
#        $temp = substr($vessel_hash{$userID} . " (" . $country . ")", 0, 56);  # Chop at 56 chars
#    }
#    else {
#        # No vessel name
#        # N7IPB: Display country of registry
#        $temp = substr("($country)", 0, 56);  # Chop at 56 chars
#
#        # WE7U: Display the 9-digit MMSI + country of registry
#        #$temp = substr($userID . " (" . $country . ")", 0, 56);  # Chop at 56 chars
#    }
    # Force the tactical call to be FS since there are no names for Base Stations
    #
    $temp = "FS";
    if ( !defined($tactical_hash{$userID}) ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_4) { print "     APRS: $aprs\n"; }
        &log_aprs($aprs);
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }

    if ($print_4) { print "\n"; }

    return();


}


# Message type 5: Static and Voyage Related Data (Vessel name, callsign, ship type)
# This message should have 424 bits total
#
sub process_type_5() {

    #if ( length($bin_string) < 424  ) {
    if ( length($bin_string) < 240  ) {
        #printf("5\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("5\t%d\n", length($bin_string));
        if ($print_5) { print "Msg too short\n\n"; }
        return;
    }

    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   2); # AIS Version
    # substr($bin_string,  40,  30); # IMO Number
    #my $bCallsign = substr($bin_string,  70,  42); # Call Sign
    my $bVesselName = substr($bin_string, 112, 120); # Vessel Name #####
    my $bShipType = substr($bin_string, 232,   8); # Ship Type: Page 114 of ITU spec, Table 53
    # substr($bin_string, 240,   9); # Dimension to Bow
    # substr($bin_string, 249,   9); # Dimension to Stern
    # substr($bin_string, 258,   6); # Dimension to Port
    # substr($bin_string, 264,   6); # Dimension to Starboard
    # substr($bin_string, 270,   4); # Position Fix Type
    # substr($bin_string, 274,   4); # ETA month (UTC)
    # substr($bin_string, 278,   5); # ETA day (UTC)
    # substr($bin_string, 283,   5); # ETA hour (UC)
    # substr($bin_string, 288,   6); # ETA minute (UTC)
    # substr($bin_string, 294,   8); # Draught
    # substr($bin_string, 302, 120); # Destination
    # substr($bin_string, 422,   1); # DTE
    # substr($bin_string, 423,   1); # Spare

    my $userID = &bin2dec($bUserID);
    if ($print_5) { print "  User ID: $userID\n"; }

    #my $callSign = &bin2text($bCallsign);
    #print "Callsign: $callSign\n";

    my $country = &decode_MID($userID);
    if ($print_5) { print "  Country: $country\n"; }

    my $vesselName = &bin2text($bVesselName);
    $vesselName =~ s/\s+$//;    # Remove extra spaces at end
    
    $vessel_hash{$userID} = $vesselName;
    # Update the persistent store
    store \%vessel_hash , $persistentFileSpec;

    if ($print_5) { print "   Vessel: $vesselName\n"; }

    my $shipTypeTxt = &decodeShipType($bShipType);
    if ($print_5) { print "Ship Type: $shipTypeTxt\n"; }

    # Assign tactical call = $vesselName + $country
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
#    my $temp = substr($vesselName . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    my $temp = substr($vesselName . " ($shipTypeTxt:$country)", 0, 56);  # Chop at 56 chars
 
    if ( !defined($tactical_hash{$userID}) || $tactical_hash{$userID} ne $temp ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_5) { print "     APRS: $aprs\n"; }

        &log_aprs($aprs);
 
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }
    if ($print_5) { print "\n"; }

    return();
}



# Message type 9: Standard SAR Aircraft Position Report
# This message should have 168 bits total
#
sub process_type_9() {

    #if ( length($bin_string) < 168 ) {
    if ( length($bin_string) < 128 ) {
        #printf("9\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("9\t%d\n", length($bin_string));
        if ($print_9) { print "Msg too short\n\n"; }
        return;
    }

    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    my $bAltitude = substr($bin_string,  38,  12); # Altitude
    my $bSpeedOverGnd = substr($bin_string,  50,  10); # SOG
    # substr($bin_string,  60,   1); # Position Accuracy
    my $bLongitude = substr($bin_string,  61,  28); # Longitude
    my $bLatitude = substr($bin_string,  89,  27); # Latitude
    my $bCourseOverGnd = substr($bin_string, 116,  12); # Course Over Ground
    # substr($bin_string, 128,   6); # Time Stamp
    # substr($bin_string, 134,   1); # Altitude sensor
    # substr($bin_string, 135,   7); # Spare
    # substr($bin_string, 142,   1); # DTE
    # substr($bin_string, 143,   3); # Spare
    # substr($bin_string, 146,   1); # Assigned mode
    # substr($bin_string, 147,   1); # RAIM flag
    # substr($bin_string, 148,   1); # Comm state selector flag
    # substr($bin_string, 149,  19); # Radio status

    my $userID = &bin2dec($bUserID);
    if ($print_9) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_9) { print "  Country: $country\n"; }

# NOTE: 4095 = N/A
# NOTE: 4094 = 4094 meters or higher.
    my $altitude_meters = &bin2dec($bAltitude);
    my $altitude = "";
    if ($altitude_meters != 4095) {
      if ($altitude_meters == 4094) {
          $altitude = sprintf( "%s%06d%s", " /A=", $altitude_meters * 3.28084, "(Higher than)" );
      }
      else {
          $altitude = sprintf( "%s%06d", " /A=", $altitude_meters * 3.28084 );
      }
      if ($print_9) { print " Altitude: $altitude\n"; }
    }

# NOTE: 0-359 degrees
# NOTE: 360 = N/A
# APRS spec says can set to "000", "...", or "   " if unknown
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    my $course = "";
    if ($courseOverGnd == 360) {
        $course = "...";
    }
    elsif ($courseOverGnd == 0) {
        $course = "360";
    }
    else {
        $course = sprintf("%03d", $courseOverGnd);
    }
    if ($print_9) { print "   Course: $courseOverGnd\n"; }

# NOTE: 0 to 102 knots
# NOTE: 102.3 = N/A
# NOTE: 102.2 = 102.2 knots or higher
# APRS spec says can set to "000", "...", or "   " if unknown
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    my $speed = "";
    if ($speedOverGnd == 102.3) {
        $speed = "...";
    }
    else {
        $speed = sprintf("%03d", $speedOverGnd);
    }
    if ($print_9) { print "    Speed: $speedOverGnd\n"; }
 
# NOTE: -90 to +90
# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
    if ($latitude == 91) {
        if ($print_9) { print "\n"; }
        return();
    }
    my $NS;
    if ($print_9) { printf(" Latitude: %07.5f\n", $latitude); }
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

# NOTE: -180 to +180
# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
    if ($longitude == 181) {
        if ($print_9) { print "\n"; }
        return();
    }
    my $EW;
    if ($print_9) { printf("Longitude: %08.5f\n", $longitude); }
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $symbol = "^";   # "^" = Large Aircraft. Could be "'" for small aircraft, "X" for helicopter.
    my $vesselTag = " ($country)";
    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }
    my $aprs = &escape_from_shell("$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed$altitude SAR Aircraft$vesselTag");
    if ($print_9) { print "     APRS: $aprs\n"; }

    &log_aprs($aprs);
 
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }
 
    # Assign tactical call = "$userID + $country" or "$vesselName + $country"
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    my $temp;
    if ( defined( $vessel_hash{$userID}) ) {
        $temp = substr($vessel_hash{$userID} . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    else {
        # No vessel name
        # N7IPB: Display country of registry
        $temp = substr("($country)", 0, 56);  # Chop at 56 chars

        # WE7U: Display the 9-digit MMSI + country of registry
        #$temp = substr($userID . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    if ( !defined($tactical_hash{$userID}) ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_9) { print "     APRS: $aprs\n"; }

        &log_aprs($aprs);
 
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }

    if ($print_9) { print "\n"; }

    return();
}



# Message type 18: Standard Class B CS Position Report
# This message should have 168 bits total
#
sub process_type_18() {

    #if ( length($bin_string) < 168 ) {
    if ( length($bin_string) < 124 ) {
        #printf("18\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("18\t%d\n", length($bin_string));
        if ($print_18) { print "Msg too short\n\n"; }
        return;
    }

    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   8); # Spare
    my $bSpeedOverGnd = substr($bin_string,  46,  10); # Speed Over Ground
    # substr($bin_string,  56,   1); # Position Accuracy
    my $bLongitude = substr($bin_string,  57,  28); # Longitude
    my $bLatitude = substr($bin_string,  85,  27); # Latitude
    my $bCourseOverGnd = substr($bin_string, 112,  12); # Course Over Ground
    # substr($bin_string, 124,   9); # True Heading
    # substr($bin_string, 133,   6); # Time Stamp
    # substr($bin_string, 139,   2); # Spare
    # substr($bin_string, 141,   1); # CS Unit
    # substr($bin_string, 142,   1); # Display flag
    # substr($bin_string, 143,   1); # DSC Flag
    # substr($bin_string, 144,   1); # Band flag
    # substr($bin_string, 145,   1); # Message 22 flag
    # substr($bin_string, 146,   1); # Assigned mode
    # substr($bin_string, 147,   1); # RAIM flag
    # substr($bin_string, 148,   1); # Comm state selector flag
    # substr($bin_string, 149,  19); # Radio status
 
    my $userID = &bin2dec($bUserID);
    if ($print_18) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_18) { print "  Country: $country\n"; }

# NOTE: 0-359 degrees
# NOTE: 360 = N/A
# APRS spec says can set to "000", "...", or "   " if unknown
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    my $course = "";
    if ($courseOverGnd == 360) {
        $course = "...";
    }
    elsif ($courseOverGnd == 0) {
        $course = "360";
    }
    else {
        $course = sprintf("%03d", $courseOverGnd);
    }
    if ($print_18) { print "   Course: $courseOverGnd\n"; }

# NOTE: 0 to 102 knots
# NOTE: 102.3 = N/A
# NOTE: 102.2 = 102.2 knots or higher
# APRS spec says can set to "000", "...", or "   " if unknown
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    my $speed = "";
    if ($speedOverGnd == 102.3) {
        $speed = "...";
    }
    else {
        $speed = sprintf("%03d", $speedOverGnd);
    }
    if ($print_18) { print "    Speed: $speedOverGnd\n"; }
 
# NOTE: -90 to +90
# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
    if ($latitude == 91) {
        if ($print_18) { print "\n"; }
        return();
    }
    my $NS;
    if ($print_18) { printf(" Latitude: %07.5f\n", $latitude); }
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

# NOTE: -180 to +180
# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
    if ($longitude == 181) {
        if ($print_18) { print "\n"; }
        return();
    }
    my $EW;
    if ($print_18) { printf("Longitude: %08.5f\n", $longitude); }
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $symbol = "s";
    my $vesselTag = " ($country)";
    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }
    my $aprs = &escape_from_shell("$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed$vesselTag");
    if ($print_18) { print "     APRS: $aprs\n"; }

    &log_aprs($aprs);
 
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }
 
    # Assign tactical call = "$userID + $country" or "$vesselName + $country"
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    my $temp;
    if ( defined( $vessel_hash{$userID}) ) {
        $temp = substr($vessel_hash{$userID} . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    else {
        # No vessel name
        # N7IPB: Display country of registry
        $temp = substr("($country)", 0, 56);  # Chop at 56 chars

        # WE7U: Display the 9-digit MMSI + country of registry
        #$temp = substr($userID . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    if ( !defined($tactical_hash{$userID}) ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_18) { print "     APRS: $aprs\n"; }

        &log_aprs($aprs);
 
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }

    if ($print_18) { print "\n"; }

    return();
}



# Message type 19: Extended Class B CS Position Report
# This message should have 312 bits total
#
sub process_type_19() {

    #if ( length($bin_string) < 312 ) {
    if ( length($bin_string) < 263 ) {
        #printf("19\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("19\t%d\n", length($bin_string));
        if ($print_19) { print "Msg too short\n\n"; }
        return;
    }
 
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   8); # Spare
    my $bSpeedOverGnd = substr($bin_string,  46,  10); # Speed Over Ground
    # substr($bin_string,  56,   1); # Position Accuracy
    my $bLongitude = substr($bin_string,  57,  28); # Longitude
    my $bLatitude = substr($bin_string,  85,  27); # Latitude
    my $bCourseOverGnd = substr($bin_string, 112,  12); # Course Over Ground
    # substr($bin_string, 124,   9); # True Heading
    # substr($bin_string, 133,   6); # Time Stamp
    # substr($bin_string, 139,   4); # Spare
    my $bVesselName = substr($bin_string, 143, 120); # Name
    # substr($bin_string, 263,   8); # Type of ship and cargo
    # substr($bin_string, 271,   9); # Dimension to Bow
    # substr($bin_string, 280,   9); # Dimension to Stern
    # substr($bin_string, 289,   6); # Dimension to Port
    # substr($bin_string, 295,   6); # Dimension to Starboard
    # substr($bin_string, 301,   4); # Position Fix Type
    # substr($bin_string, 305,   1); # RAIM flag
    # substr($bin_string, 306,   1); # DTE
    # substr($bin_string, 307,   1); # Assigned mode flag
    # substr($bin_string, 308,   4); # Spare
 
    my $userID = &bin2dec($bUserID);
    if ($print_19) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_19) { print "  Country: $country\n"; }

# NOTE: 0-359 degrees
# NOTE: 360 = N/A
# APRS spec says can set to "000", "...", or "   " if unknown
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    my $course = "";
    if ($courseOverGnd == 360) {
        $course = "...";
    }
    elsif ($courseOverGnd == 0) {
        $course = "360";
    }
    else {
        $course = sprintf("%03d", $courseOverGnd);
    }
    if ($print_19) { print "   Course: $courseOverGnd\n"; }

# NOTE: 0 to 102 knots
# NOTE: 102.3 = N/A
# NOTE: 102.2 = 102.2 knots or higher
# APRS spec says can set to "000", "...", or "   " if unknown
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    my $speed = "";
    if ($speedOverGnd == 102.3) {
        $speed = "...";
    }
    else {
        $speed = sprintf("%03d", $speedOverGnd);
    }
    if ($print_19) { print "    Speed: $speedOverGnd\n"; }
 
# NOTE: -90 to +90
# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
    if ($latitude == 91) {
        if ($print_19) { print "\n"; }
        return();
    }
    my $NS;
    if ($print_19) { printf(" Latitude: %07.5f\n", $latitude); }
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);
# NOTE: -180 to +180
# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
    if ($longitude == 181) {
        if ($print_19) { print "\n"; }
        return();
    }
    my $EW;
    if ($print_19) { printf("Longitude: %08.5f\n", $longitude); }
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $vesselName = "";
    if ($bVesselName ne "") { 
        $vesselName = &bin2text($bVesselName);
        $vesselName =~ s/\s+$//;    # Remove extra spaces at end
    }

    my $symbol = "s";
    my $vesselTag = " ($country)";
    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }
    my $aprs = &escape_from_shell("$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed$vesselTag");
    if ($print_19) { print "     APRS: $aprs\n"; }

    &log_aprs($aprs);
 
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }

    if ($vesselName ne "") {
        $vessel_hash{$userID} = $vesselName;
        # Update the persistent store
        store \%vessel_hash , $persistentFileSpec;

        if ($print_19) { print "   Vessel: $vesselName\n"; }

        # Assign tactical call = $vesselName
        # Max tactical call in Xastir is 57 chars (56 + terminator?)
        #
        my $temp = substr($vesselName . " (" . $country . ")", 0, 56);  # Chop at 56 chars
        if ( !defined($tactical_hash{$userID}) || $tactical_hash{$userID} ne $temp ) {
            $tactical_hash{$userID} = $temp;
            $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
            if ($print_19) { print "     APRS: $aprs\n"; }

            &log_aprs($aprs);
 
            if ($enable_tx) {
                $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
                if ($result =~ m/NACK/) {
                    die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
                }
            }
        }
    }

    if ($print_19) { print "\n"; }

    return();
}

# Message type 21: Aid to Navigation report 
# These messages should have between 272 and 360 bits total
# We don't use the full message and don't parse the name extension
#
sub process_type_21() {

    #if ( length($bin_string) < 168 ) {
    if ( length($bin_string) < 272 ) {
        #printf("21\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("21\t%d\n", length($bin_string));
        if ($print_21) { print "Msg too short\n\n"; }
        return;
    }

    my $bUserID = substr($bin_string, 8, 30); # MMSI
    my $bAidType = substr($bin_string, 38, 5);
    my $bVesselName = substr($bin_string, 43, 120); # Vessel Name #####
    my $bLongitude = substr($bin_string, 164, 28);   #####
    my $bLatitude = substr($bin_string, 192, 27);    #####

    my $userID = &bin2dec($bUserID);
    if ($print_21) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_21) { print "  Country: $country\n"; }

    my $vesselName = &bin2text($bVesselName);
    $vesselName =~ s/\s+$//;    # Remove extra spaces at end

    $vessel_hash{$userID} = $vesselName;
    # Update the persistent store
    store \%vessel_hash , $persistentFileSpec;

    if ($print_21) { print "   Vessel: $vesselName\n"; }

    my $AidTypeTxt = &decodeAidType($bAidType);
    if ($print_21) { print "Navaid Type: $AidTypeTxt\n"; }


# NOTE: -90 to +90
# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
    if ($latitude == 91) { return(); }
    my $NS;
    if ($print_21) { printf(" Latitude: %07.5f\n", $latitude); }
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

# NOTE: -180 to +180
# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
    if ($longitude == 181) { return(); }
    my $EW;
    if ($print_21) { printf("Longitude: %08.5f\n", $longitude); }
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $symbol = "N";

    # Set up to add country or vessel_name + country to comment field
    my $vesselTag = " (nav-aid)";
    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }

    my $aprs= &escape_from_shell("$xastir_user>APRS:)$userID!$lat\\$lon$symbol $AidTypeTxt");
    if ($print_21) { print "     APRS: $aprs\n"; }
    &log_aprs($aprs);
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }

    # Assign tactical call = $vesselName + $country
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    my $temp = substr($vesselName, 0, 56);  # Chop at 56 chars
    if ( !defined($tactical_hash{$userID}) || $tactical_hash{$userID} ne $temp ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_21) { print "     APRS: $aprs\n"; }
    &log_aprs($aprs);
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }

    if ($print_21) { print "\n"; }

    return();

}


# Message type 24: Static Data Report (Vessel name, callsign, ship type)
# This one has three variations:
#
#   If Part Number field is 0, it's a Type A message.
#   If Part Number is 1, it's a Type B.
#
#   Part B has two variations as well:
#       If the MMSI is that of an auxiliary craft, then the Mothership MMSI
#           is that of the mothership.
#       If not, then those 30 bits represent vessel dimmensions.
#
# This message should have:
#   Type A: 160 bits total
#   Type B: 168 bits total
#
sub process_type_24() {

    #if ( length($bin_string) < 160 ) {
    if ( length($bin_string) < 160 ) {
        #printf("24\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("24\t%d\n", length($bin_string));
        if ($print_24_A || $print_24_B) { print "Msg too short\n\n"; }
        return;
    }
 
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    my $bPartNumber = substr($bin_string,  38,   2); # Part Number

    my $userID = &bin2dec($bUserID);
 
    # Check for Type A/B Variant (Type B also has two variants!)
    $PartNumber = &bin2dec($bPartNumber);
    if ($PartNumber == 0) {

        # It is a Type A Variant
        if ($print_24_A) { print " Msg Type: $message_type\tType A Variant\n"; }
 
        if ($print_24_A) { print "  User ID: $userID\n"; }

        my $country = &decode_MID($userID);
        if ($print_24_A) { print "  Country: $country\n"; }
 
        # Type A Variant format starts at bit 40:
        my $bVesselName = substr($bin_string,  40, 120); # Vessel Name
        # substr($bin_string, 160,   8); # Spare.

        my $vesselName = "";
        if ($bVesselName ne "") { 
            $vesselName = &bin2text($bVesselName);
            $vesselName =~ s/\s+$//;    # Remove extra spaces at end
        }
        if ($vesselName ne "") {
            $vessel_hash{$userID} = $vesselName;
            # Update the persistent store
            store \%vessel_hash , $persistentFileSpec;

            if ($print_24_A) { print "   Vessel: $vesselName\n"; }
    
            # Assign tactical call = $vesselName
            # Max tactical call in Xastir is 57 chars (56 + terminator?)
            #
            my $temp = substr($vesselName . " (" . $country . ")", 0, 56);  # Chop at 56 chars
            if ( !defined($tactical_hash{$userID}) || $tactical_hash{$userID} ne $temp ) {
                $tactical_hash{$userID} = $temp;
                $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
                if ($print_24_A) { print "     APRS: $aprs\n"; }

                &log_aprs($aprs);
 
                if ($enable_tx) {
                    $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
                    if ($result =~ m/NACK/) {
                        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
                    }
                }
            }
        }
        if ($print_24_A) { print "\n"; }
    }
    else {

        # It is a Type B Variant. There are two variants of this type!
        if ($print_24_B) { print " Msg Type: $message_type\tType B Variant\n"; }
 
        if ($print_24_B) { print "  User ID: $userID\n"; }

        my $country = &decode_MID($userID);
        if ($print_24_B) { print "  Country: $country\n"; }
 
        # Check the UserID to see if it is an Auxiliary Craft,
        # to determine which Type B Variant to decode.
        # Auxiliary craft: MMSI of form 98XXXYYYY, the XXX digits are the country code.

        if ( !($userID =~ m/^98/)) {
            # It is NOT an auxiliary craft:

            # Alternate format starting at bit 40:
            $bShipType = substr($bin_string,  40,   8); # Ship Type: Page 114 of ITU spec, Table 53
            # substr($bin_string,  48,  18); # Vendor ID
            # substr($bin_string,  66,   4); # Unit Model Code
            # substr($bin_string,  70,  20); # Serial Number)
            # substr($bin_string,  90,  42); # Call Sign
            # substr($bin_string, 132,   9); # Dimension to bow
            # substr($bin_string, 141,   9); # Dimension to Stern
            # substr($bin_string, 150,   6); # Dimension to Port
            # substr($bin_string, 156,   6); # Dimension to Starboard
            # substr($bin_string, 162,   4); # Type of fix
            # substr($bin_string, 166,   2); # Spare

            my $shipTypeTxt = &decodeShipType($bShipType);
            if ($print_24_B) { print "Ship Type: $shipTypeTxt\n"; }

            # Assign tactical call = $vesselName + $shipTypeTxt
            # Max tactical call in Xastir is 57 chars (56 + terminator?)
            #
            #my $temp = substr($vesselName . " (" . $country . ")", 0, 56);  # Chop at 56 chars
            my $temp;
            if (defined($vessel_hash{$userID}) ) {
                $temp = substr($vessel_hash{$userID} . " ($shipTypeTxt:$country)", 0, 56);  # Chop at 56 chars
            }
            else {
                $temp = substr("($shipTypeTxt:$country)", 0, 56);  # Chop at 56 chars
            }
 
            if ( !defined($tactical_hash{$userID}) || $tactical_hash{$userID} ne $temp ) {
                $tactical_hash{$userID} = $temp;
                $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
                if ($print_24_A) { print "     APRS: $aprs\n"; }

                &log_aprs($aprs);
 
                if ($enable_tx) {
                    $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
                    if ($result =~ m/NACK/) {
                        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
                    }
                }
            }
        }
        else { 
            # It IS an auxiliary craft:

            # Alternate format starting at bit 132:
            # substr($bin_string, 132,  30); # Mothership MMSI
            # substr($bin_string, 162,   6); # Spare
        }

        if ($print_24_B) { print "\n"; }
    }

    return();
}



# Message type 27: Position Report For Long-Range Applications (rare)
# This message should have 96 bits total
#
sub process_type_27() {

    #if ( length($bin_string) < 96 ) {
    if ( length($bin_string) < 94 ) {
        #printf("27\t%s\t%d\n", $bin_string, length($bin_string));
        #printf("27\t%d\n", length($bin_string));
        if ($print_27) { print "Msg too short\n\n"; }
        return;
    }
 
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   1); # Position Accuracy
    # substr($bin_string,  39,   1); # RAIM flag
    my $bNavStatus = substr($bin_string,  40,   4); # Navigation Status
    my $bLongitude = substr($bin_string,  44,  18); # Longitude
    my $bLatitude = substr($bin_string,  62,  17); # Latitude
    my $bSpeedOverGnd = substr($bin_string,  79,   6); # Speed Over Ground
    my $bCourseOverGnd = substr($bin_string,  85,   9); # Course Over Ground
    # substr($bin_string,  94,   1); # GNSS Position latency
    # substr($bin_string,  95,   1); # Spare

    my $userID = &bin2dec($bUserID);
    if ($print_27) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_27) { print "  Country: $country\n"; }

    my $navStatus = &bin2dec($bNavStatus);
    my $navStatusTxt = "";
    if    ($navStatus ==  0) { $navStatusTxt = "Under_way:engine"; }
    elsif ($navStatus ==  1) { $navStatusTxt = "At_anchor"; }
    elsif ($navStatus ==  2) { $navStatusTxt = "Not_under_command"; }
    elsif ($navStatus ==  3) { $navStatusTxt = "Restricted_maneuverability"; }
    elsif ($navStatus ==  4) { $navStatusTxt = "Constrained_by_draught"; }
    elsif ($navStatus ==  5) { $navStatusTxt = "Moored"; }
    elsif ($navStatus ==  6) { $navStatusTxt = "Aground"; }
    elsif ($navStatus ==  7) { $navStatusTxt = "Fishing"; }
    elsif ($navStatus ==  8) { $navStatusTxt = "Under_way:sailing"; }
    #elsif ($navStatus ==  9) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 10) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 11) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 12) { $navStatusTxt = "Reserved"; }
    #elsif ($navStatus == 13) { $navStatusTxt = "Reserved"; }
    elsif ($navStatus == 14) { $navStatusTxt = "AIS-SART_is_active"; }
    #elsif ($navStatus == 15) { $navStatusTxt = "Not defined"; }
 
# NOTE: 0-359 degrees
# NOTE: 511 = N/A
# APRS spec says can set to "000", "...", or "   " if unknown
    my $courseOverGnd = &bin2dec($bCourseOverGnd);
    my $course = "";
    if ($courseOverGnd == 511) {
        $course = "...";
    }
    elsif ($courseOverGnd == 0) {
        $course = "360";
    }
    else {
        $course = sprintf("%03d", $courseOverGnd);
    }
    if ($print_27) { print "   Course: $courseOverGnd\n"; }

# NOTE: 0-62 knots
# NOTE: 63 = N/A
# APRS spec says can set to "000", "...", or "   " if unknown
    my $speedOverGnd = &bin2dec($bSpeedOverGnd);
    my $speed = "";
    if ($speedOverGnd == 63) {
        $speed = "...";
    }
    else {
        $speed = sprintf("%03d", $speedOverGnd);
    }
    if ($print_27) { print "    Speed: $speedOverGnd\n"; }
 
# NOTE: -90 to +90
# NOTE: 91 = N/A = D548h or 54600 decimal. Divide by 91 to get 600 (our factor).
    my $latitude = &signedBin2dec($bLatitude) / 600.0;
    if ($latitude == 91) {
        if ($print_27) { print "\n"; }
        return();
    }
    if ($print_27) {
        #printf(" bLatitude: %s\n", $bLatitude);
        printf(" Latitude: %07.5f\n", $latitude);
    }
    my $NS;
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    my $latdeg = int($latitude);
    my $latmins = $latitude - $latdeg;
    my $latmins2 = $latmins * 60.0;
    my $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

# NOTE: -180 to +180
# NOTE: 181 = N/A = 1A838h or 108600 decimal. Divide by 181 to get 600 (our factor).
    my $longitude = &signedBin2dec($bLongitude) / 600.0;
    if ($longitude == 181) {
        if ($print_27) { print "\n"; }
        return();
    }
    if ($print_27) {
        #printf("bLongitude: %s\n", $bLongitude);
        printf("Longitude: %08.5f\n", $longitude);
    }
    my $EW;
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    my $londeg = int($longitude);
    my $lonmins = $longitude - $londeg;
    my $lonmins2 = $lonmins * 60.0;
    my $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    my $symbol = "s";
    my $vesselTag = " ($country)";
    if ( defined($vessel_hash{$userID}) ) { $vesselTag = " " . $vessel_hash{$userID} . " (" . $country . ")"; }
    my $aprs = &escape_from_shell("$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed $navStatusTxt$vesselTag");
    if ($print_27) { print "     APRS: $aprs\n"; }

    &log_aprs($aprs);
 
    if ($enable_tx) {
        my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }
 
    # Assign tactical call = "$userID + $country" or "$vesselName + $country"
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    my $temp;
    if ( defined( $vessel_hash{$userID}) ) {
        $temp = substr($vessel_hash{$userID} . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    else {
        # No vessel name
        # N7IPB: Display country of registry
        $temp = substr("($country)", 0, 56);  # Chop at 56 chars

        # WE7U: Display the 9-digit MMSI + country of registry
        #$temp = substr($userID . " (" . $country . ")", 0, 56);  # Chop at 56 chars
    }
    if ( !defined($tactical_hash{$userID}) ) {
        $tactical_hash{$userID} = $temp;
        $aprs = &escape_from_shell($xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $temp);
        if ($print_27) { print "     APRS: $aprs\n"; }

        &log_aprs($aprs);
 
        if ($enable_tx) {
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
            }
        }
    }

    if ($print_27) { print "\n"; }
 
    return();
}

# Convert binary string $bShipType into text string $shipType
#
sub decodeAidType() {
    my $bAidType = shift;

    my $aidType = sprintf("%02d", &bin2dec($bAidType));
    my $aidTypeTxt = "$aidType:";

    # Special Craft
    if    ($aidType == 1) { $aidTypeTxt = $aidTypeTxt . "Reference point"; }
    elsif ($aidType == 2) { $aidTypeTxt = $aidTypeTxt . "RACON"; }
    elsif ($aidType == 3) { $aidTypeTxt = $aidTypeTxt . "Fixed structure off shore"; }
    elsif ($aidType == 4) { $aidTypeTxt = $aidTypeTxt . "Reserved"; }
    elsif ($aidType == 5) { $aidTypeTxt = $aidTypeTxt . "Light, without sectors"; }
    elsif ($aidType == 6) { $aidTypeTxt = $aidTypeTxt . "Light, with sectors"; }
    elsif ($aidType == 7) { $aidTypeTxt = $aidTypeTxt . "Leading Light Front"; }
    elsif ($aidType == 8) { $aidTypeTxt = $aidTypeTxt . "Leading Light Rear"; }
    elsif ($aidType == 9) { $aidTypeTxt = $aidTypeTxt . "Beacon, Cardinal N"; }
    elsif ($aidType == 10) { $aidTypeTxt = $aidTypeTxt . "Beacon, Cardinal E"; }
    elsif ($aidType == 11) { $aidTypeTxt = $aidTypeTxt . "Beacon, Cardinal S"; }
    elsif ($aidType == 12) { $aidTypeTxt = $aidTypeTxt . "Beacon, Cardinal W"; }
    elsif ($aidType == 13) { $aidTypeTxt = $aidTypeTxt . "Beacon, Port hand"; }
    elsif ($aidType == 14) { $aidTypeTxt = $aidTypeTxt . "Beacon, Starboard hand"; }
    elsif ($aidType == 15) { $aidTypeTxt = $aidTypeTxt . "Beacon, Preferred Channel port hand"; }
    elsif ($aidType == 16) { $aidTypeTxt = $aidTypeTxt . "Beacon, Preferred Channel starboard hand"; }
    elsif ($aidType == 17) { $aidTypeTxt = $aidTypeTxt . "Beacon, Isolated danger"; }
    elsif ($aidType == 18) { $aidTypeTxt = $aidTypeTxt . "Beacon, Safe water"; }
    elsif ($aidType == 19) { $aidTypeTxt = $aidTypeTxt . "Beacon, Special mark"; }
    elsif ($aidType == 20) { $aidTypeTxt = $aidTypeTxt . "Cardinal Mark N"; }
    elsif ($aidType == 21) { $aidTypeTxt = $aidTypeTxt . "Cardinal Mark E"; }
    elsif ($aidType == 22) { $aidTypeTxt = $aidTypeTxt . "Cardinal Mark S"; }
    elsif ($aidType == 23) { $aidTypeTxt = $aidTypeTxt . "Cardinal Mark W"; }
    elsif ($aidType == 24) { $aidTypeTxt = $aidTypeTxt . "Port hand Mark"; }
    elsif ($aidType == 25) { $aidTypeTxt = $aidTypeTxt . "Starboard hand Mark"; }
    elsif ($aidType == 26) { $aidTypeTxt = $aidTypeTxt . "Preferred Channel Port hand"; }
    elsif ($aidType == 27) { $aidTypeTxt = $aidTypeTxt . "Preferred Channel Starboard hand"; }
    elsif ($aidType == 28) { $aidTypeTxt = $aidTypeTxt . "Isolated danger"; }
    elsif ($aidType == 29) { $aidTypeTxt = $aidTypeTxt . "Safe Water"; }
    elsif ($aidType == 30) { $aidTypeTxt = $aidTypeTxt . "Special Mark"; }
    elsif ($aidType == 31) { $aidTypeTxt = $aidTypeTxt . "Light Vessel / LANBY / Rigs"; }

    return($aidTypeTxt);
}




# Convert binary string $bShipType into text string $shipType
#
sub decodeShipType() {
    my $bShipType = shift;

    my $shipType = sprintf("%02d", &bin2dec($bShipType));
    my $shipTypeTxt = "$shipType:";

    # Special Craft
    if    ($shipType == 50) { $shipTypeTxt = $shipTypeTxt . "Pilot_Vessel"; }
    elsif ($shipType == 51) { $shipTypeTxt = $shipTypeTxt . "Search_and_Rescue"; }
    elsif ($shipType == 52) { $shipTypeTxt = $shipTypeTxt . "Harbor_Tug"; }
    elsif ($shipType == 53) { $shipTypeTxt = $shipTypeTxt . "Fish,Offshore_or_Port_Tender"; }
    elsif ($shipType == 54) { $shipTypeTxt = $shipTypeTxt . "Anti-pollution"; }
    elsif ($shipType == 55) { $shipTypeTxt = $shipTypeTxt . "Law_Enforcement"; }
    elsif ($shipType == 56) { $shipTypeTxt = $shipTypeTxt . "Local"; }
    elsif ($shipType == 57) { $shipTypeTxt = $shipTypeTxt . "Local"; }
    elsif ($shipType == 58) { $shipTypeTxt = $shipTypeTxt . "Medical_Transport_or_Public_Safety"; }
    elsif ($shipType == 59) { $shipTypeTxt = $shipTypeTxt . "Neutral_State_Vessel"; }

    # U.S. Specific Vessels
    elsif ($shipType =~ m/^2/) {
        if ($shipType =~ m/0$/) { $shipTypeTxt = $shipTypeTxt . "Wing_in_Ground"; }
        if ($shipType =~ m/1$/) { $shipTypeTxt = $shipTypeTxt . "Tow-Push"; }
        if ($shipType =~ m/2$/) { $shipTypeTxt = $shipTypeTxt . "Tow-Push"; }
        if ($shipType =~ m/3$/) { $shipTypeTxt = $shipTypeTxt . "Light_Boat"; }
        if ($shipType =~ m/4$/) { $shipTypeTxt = $shipTypeTxt . "Mobile_Offshore_Drilling"; }
        if ($shipType =~ m/5$/) { $shipTypeTxt = $shipTypeTxt . "Offshore_Supply_Vessel"; }
        if ($shipType =~ m/6$/) { $shipTypeTxt = $shipTypeTxt . "Processing_Vessel"; }
        if ($shipType =~ m/7$/) { $shipTypeTxt = $shipTypeTxt . "School/Scientific/Research/Training_Vessel"; }
        if ($shipType =~ m/8$/) { $shipTypeTxt = $shipTypeTxt . "U.S._Public_or_Govt"; }
        if ($shipType =~ m/9$/) { $shipTypeTxt = $shipTypeTxt . "Autonomous_or_Remotely-Operated"; }
    }

    # Other Vessels
    elsif ($shipType =~ m/^3/) {
        if ($shipType =~ m/0$/) { $shipTypeTxt = $shipTypeTxt . "Fishing"; }
        if ($shipType =~ m/1$/) { $shipTypeTxt = $shipTypeTxt . "Towing-Pull"; }
        if ($shipType =~ m/2$/) { $shipTypeTxt = $shipTypeTxt . "Towing-Big"; }
        if ($shipType =~ m/3$/) { $shipTypeTxt = $shipTypeTxt . "Dredging_or_Underwater_Ops"; }
        if ($shipType =~ m/4$/) { $shipTypeTxt = $shipTypeTxt . "Diving_Ops"; }
        if ($shipType =~ m/5$/) { $shipTypeTxt = $shipTypeTxt . "Military_Ops"; }
        if ($shipType =~ m/6$/) { $shipTypeTxt = $shipTypeTxt . "Sailing"; }
        if ($shipType =~ m/7$/) { $shipTypeTxt = $shipTypeTxt . "Pleasure_Craft"; }
        if ($shipType =~ m/8$/) { $shipTypeTxt = $shipTypeTxt . "Other:Reserved"; }
        if ($shipType =~ m/9$/) { $shipTypeTxt = $shipTypeTxt . "Other:Reserved"; }
    }

    elsif ($shipType =~ m/^0/) { $shipTypeTxt = $shipTypeTxt . "Not-Available"; }
    elsif ($shipType =~ m/^1/) { $shipTypeTxt = $shipTypeTxt . "Reserved"; }
    elsif ($shipType =~ m/^4/) { $shipTypeTxt = $shipTypeTxt . "Passenger_or_High_Speed"; }
    elsif ($shipType =~ m/^6/) { $shipTypeTxt = $shipTypeTxt . "Passenger-Big"; }
    elsif ($shipType =~ m/^7/) { $shipTypeTxt = $shipTypeTxt . "Cargo"; }
    elsif ($shipType =~ m/^8/) { $shipTypeTxt = $shipTypeTxt . "Tanker"; }
    elsif ($shipType =~ m/^9/) { $shipTypeTxt = $shipTypeTxt . "Other"; }

    # Decode 2nd digit for 1/4/6/7/8/9 types:
    if (   ($shipType =~ m/^1/)
        || ($shipType =~ m/^4/)
        || ($shipType =~ m/^6/)
        || ($shipType =~ m/^7/)
        || ($shipType =~ m/^8/)
        || ($shipType =~ m/^9/) ) {

        if ($shipType =~ m/0$/) { $shipTypeTxt = $shipTypeTxt . ""; }
        if ($shipType =~ m/1$/) { $shipTypeTxt = $shipTypeTxt . "-Haz_Cat:A/X"; }
        if ($shipType =~ m/2$/) { $shipTypeTxt = $shipTypeTxt . "-Haz_Cat:B/Y"; }
        if ($shipType =~ m/3$/) { $shipTypeTxt = $shipTypeTxt . "-Haz_Cat:C/Z"; }
        if ($shipType =~ m/4$/) { $shipTypeTxt = $shipTypeTxt . "-Haz_Cat:D/O"; }
        if ($shipType =~ m/5$/) { $shipTypeTxt = $shipTypeTxt . "-Reserved"; }
        if ($shipType =~ m/6$/) { $shipTypeTxt = $shipTypeTxt . "-Reserved"; }
        if ($shipType =~ m/7$/) { $shipTypeTxt = $shipTypeTxt . "-Reserved"; }
        if ($shipType =~ m/8$/) { $shipTypeTxt = $shipTypeTxt . "-Reserved"; }
        if ($shipType =~ m/9$/) { $shipTypeTxt = $shipTypeTxt . ""; }
    }


    return($shipTypeTxt);
}



# Convert a 6-digit binary string to ASCII text
# Encoded AIS data: Each 6 bits represents one ASCII character.
#
# Each six-bit nibble maps to an ASCII character.
# Decimal 0-31 map to "@" ( ASCII 64) through "\_" (ASCII 95)
# Decimal 32-63 map to " " (ASCII 32) though "?" (ASCII 63)
# Lowercase, backtick, right/left braces, pipe, tilde and DEL can't be encoded
sub bin2text() {
    my $input = shift;
    my $final_string = "";
    my $len = length($input);
    my $i;
    for ($i = 0; $i < $len; $i+=6) {
        my $binary_char = substr($input, $i, 6);
        # Convert from binary to decimal (ord)
        my $d = unpack("N", pack("B32", substr("00000000000000000000000000000000" . $binary_char, -32)));
        if ($d > 0) {   # Skip NULL characters
            if ($d < 32) { $d += 64; }
            $final_string = $final_string . chr($d);
        }
    }
    $final_string =~ s/\s\s/ /g;
    return($final_string);
}
 


## Convert a signed binary string of ASCII 0's and 1's to a decimal
#sub signedBin2dec {
#    my $input = shift;
#
#    if ( substr($input, 0, 1) eq "1") { # Negative number
#        # Pad to 32 bits with 1's
##        return unpack("i", pack("B32", substr("11111111111111111111111111111111" . $input, -32)));
#        # Invert all the bits. Add 1. Convert to decimal. Negate.
#        my $ii;
#        my $input2 = "";
#        my $len = length($input);
#        for ($ii = 0; $ii < $len; $ii++) {
#            my $j = substr($input, $ii, 1);
#            if ($j eq "0") { $j = "1"; }
#            else { $j = "0"; }
#            $input2 = $input2 . $j;
#        }
##print "$input\n";
##print "$input2\n";
#        my $binary = pack("B32", substr("00000000000000000000000000000000" . $input2, -32, 32));
#        my $decimal = unpack("N", $binary);
#        $decimal++;
#        $decimal = -$decimal;
#        return $decimal;
#    }
#    else { # Positive number
#        # Pad to 32 bits with 0's
#        return unpack("N", pack("B32", substr("00000000000000000000000000000000" . $input, -32))); #####
#
#    }
#}



# Convert a signed binary string of ASCII 0's and 1's to a decimal the hard way.
# This subroutine should be platform-agnostic as it does all the work internally.
sub signedBin2dec {
    my $input = shift;
    my $input2 = "";
    my $decimal;
    my $multiplier;
 
    #print "\n$input\n";
    if ( substr($input, 0, 1) eq "1") { # Negative number
        # Invert all the bits. Add 1. Convert to decimal. Negate.
        my $ii;
        my $len = length($input);
        for ($ii = 0; $ii < $len; $ii++) {
            my $j = substr($input, $ii, 1);
            if ($j eq "0") { $j = "1"; }
            else { $j = "0"; }
            $input2 = $input2 . $j;
        }
        $decimal = &toDecimal($input2);
        $decimal++;
        $decimal = -$decimal;
        return $decimal;
    }
    else { # Positive number
        return &toDecimal($input);
    }
}



# Convert to decimal from positive binary string.
# Used by above signedBin2dec subroutine.
sub toDecimal {        
    my $input = shift;
    my $multiplier = 1;
    my $decimal = 0;
    my $len = length($input);
    # Start at LSB bit, work toward MSB bit
    for (my $ii = $len-1; $ii >= 0; $ii--) {
        if (substr($input, $ii, 1) == 1) {
            $decimal = $decimal + $multiplier;
            #print "1\tMult: $multiplier\n";
        }   
        else {
            #print "0\n";
        }
        $multiplier = $multiplier * 2 ;
    }   
    return $decimal;
}



# Convert an unsigned binary string of ASCII 0's and 1's to a decimal
sub bin2dec {
    my $temp = shift;
    if (!defined($temp) || $temp eq "") { return; }
    return unpack("N", pack("B32", substr("00000000000000000000000000000000" . $temp, -32))); #####
}



# Convert a decimal string (ASCII) to a binary string (ASCII)
sub dec2bin {
    my $input = "00000000000000000000000000" . shift;
    my $str = unpack("B32", pack("N", $input));
    my $str2 = substr($str, -6); 
    return $str2;
}



# Decode MID out of MMSI ($userID) too:
#    8MIDXXXXX Diver's radio (not used in the U.S. in 2013)
#    MIDXXXXXX Ship
#    0MIDXXXXX Group of ships; the U.S. Coast Guard, for example, is 03699999
#    00MIDXXXX Coastal stations
#    111MIDXXX SAR (Search and Rescue) aircraft
#    99MIDXXXX Aids to Navigation
#    98MIDXXXX Auxiliary craft associated with a parent ship
#    970MIDXXX AIS SART (Search and Rescue Transmitter)
#    972XXXXXX MOB (Man Overboard) device
#    974XXXXXX EPIRB (Emergency Position Indicating Radio Beacon) AIS
#
# NOTE: U.S. ships sometimes incorrectly send "669" for those first 3 digits.
#     http://www.itu.int/online/mms/glad/cga_mids.sh?lng=E
#
sub decode_MID {
    my $userID = shift;
    my $MID = $userID;
    if ($userID =~ m/^8.*/) {         # Diver's radio
        $MID =~ s/^8(...).*/$1/;
    }
    elsif ($userID =~ m/^00.*/) {    # Coastal station
        $MID =~ s/^00(...).*/$1/;
    }
    elsif ($userID =~ m/^0.*/) {     # Group of ships
        $MID =~ s/^0(...).*/$1/;
    }
    elsif ($userID =~ m/^111.*/) {   # SAR aircraft
        $MID =~ s/^111(...).*/$1/;
    }
    elsif ($userID =~ m/^99.*/) {    # Navigation aid
        $MID =~ s/^99(...).*/$1/;
    }
    elsif ($userID =~ m/^98.*/) {    # Auxiliary craft
        $MID =~ s/^98(...).*/$1/;
    }
    elsif ($userID =~ m/^970.*/) {   # AIS SART
        $MID =~ s/^970(...).*/$1/;
    }
    elsif ($userID =~ m/^972.*/) {    # Man overboard device
        $MID = "MOB";
    }
    elsif ($userID =~ m/^974.*/) {    # EPIRB
        $MID = "EPIRB";
    }
    else {                          # Ship
        $MID =~ s/^(...).*/$1/;
    }

    my $country = "";
    if (defined($countries{$MID})) {
        $country = $countries{$MID};
    }
    else {
        $country = "Unknown";
    }

    return $country; 
}



# Escape characters from the shell
#
# Between double quotes the literal value of all characters is preserved except
# for dollar sign, backticks (backward single quotes, ``) and backslash.
#
# The backslash retains its meaning only when followed by dollar, backtick, double quote,
# backslash or newline. Within double quotes, the backslashes are removed from the input
# stream when followed by one of these characters. Backslashes preceding characters that
# don't have a special meaning are left unmodified for processing by the shell interpreter.
#
sub escape_from_shell {
    $temp = shift;
    $temp =~ s/\\/\\/g;     # Look for \ and escape it. Do this one FIRST!
    $temp =~ s/"/\\"/g;     # Look for " and escape it
    $temp =~ s/\$/\\\$/g;   # Look for $ and escape it
    $temp =~ s/\`/\\`/g;    # Look for ` and escape it
    return ($temp);
}



# Log messages to log file if logging enabled
#
sub log_aprs {
  $sentence = shift;
  if ($logging_mode) {
    `echo \"$sentence\" >> $log_file`;
  }
}


