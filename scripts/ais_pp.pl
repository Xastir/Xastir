#!/usr/bin/env perl
use warnings;
###########################################################################
#
# XASTIR, Amateur Station Tracking and Information Reporting
# Copyright (C) 2000-2023 The Xastir Group
#
# "ais_pp.pl", a Perl script that prints a formatted list of the 
# ships found in the vessel hash checkpoint file created by ais.pl
#
###########################################################################


#use IO::Socket;
use Storable;
#use File::HomeDir;
use Data::Dumper;  # Only used for debugging vessel_hash

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
#	store \%vessel_hash , $persistentFileSpec;
}

# Retrieve the cache
%vessel_hash = %{retrieve($persistentFileSpec)};

# Debug - print the hash values 
#print Dumper(\%vessel_hash); 
#open(my $fh, '>', 'report.txt');
#print $fh Dumper(\%vessel_hash);

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
    "267" => "Slovakia",    # "Slovak Republic"
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
    "303" => "Alaska - U.S.",    # "Alaska (State of) - United States of America"
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
    "358" => "Puerto Rico - U.S.",   # "Puerto Rico - United States of America"
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
    "375" => "St. Vincent & the Grenadines",    # "Saint Vincent and the Grenadines"
    "376" => "St. Vincent & the Grenadines",    # "Saint Vincent and the Grenadines"
    "377" => "St. Vincent & the Grenadines",    # "Saint Vincent and the Grenadines"
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
    "451" => "Kyrgyzstan",    # "Kyrgyz Republic"
    "453" => "Macao",    # "Macao (Special Administrative Region of China) - China (People's Republic of)"
    "455" => "Maldives",    # "Maldives (Republic of)"
    "457" => "Mongolia",
    "459" => "Nepal",    # "Nepal (Federal Democratic Republic of)"
    "461" => "Oman",    # "Oman (Sultanate of)"
    "463" => "Pakistan",    # "Pakistan (Islamic Republic of)"
    "466" => "Qatar",    # "Qatar (State of)"
    "468" => "Syria",    # "Syrian Arab Republic"
    "470" => "United Arab Emirates",
    "471" => "United Arab Emirates",
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
    "536" => "N. Mariana Is. - U.S.",    # "Northern Mariana Islands (Commonwealth of the) - United States of America"
    "538" => "Marshall Is.",    # "Marshall Islands (Republic of the)"
    "540" => "New Caledonia",    # "New Caledonia - France"
    "542" => "Niue",    # "Niue - New Zealand"
    "544" => "Nauru",    # "Nauru (Republic of)"
    "546" => "French Polynesia",    # "French Polynesia - France"
    "548" => "Philippines",    # "Philippines (Republic of the)"
    "553" => "Papua New Guinea",
    "555" => "Pitcairn Is.",    # "Pitcairn Island - United Kingdom of Great Britain and Northern Ireland"
    "557" => "Solomon Islands",
    "559" => "American Samoa - U.S.",   # "American Samoa - United States of America"
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
    "617" => "Cape Verde",    # "Cabo Verde (Republic of)"
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
    "676" => "Dem. Rep. of the Congo",    # "Democratic Republic of the Congo"
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
    "755" => "Paraguay",    # "Paraguay (Republic of)"
    "760" => "Peru",
    "765" => "Suriname",    # "Suriname (Republic of)"
    "770" => "Uruguay",    # "Uruguay (Eastern Republic of)"
    "775" => "Venezuela",    # "Venezuela (Bolivarian Republic of)"
);




    # Main processing loop. Fetch lines from vessel_hash 
    # and print them out on stdio
    # Format: MILLENIUM FALCON        366978720    U.S.
while(my($userID, $vesselName) = each %vessel_hash) { 
    my $country = &decode_MID($userID);
    printf "%-20s    %-9d    %s\n",$vesselName,$userID,$country; 
}
exit;

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



