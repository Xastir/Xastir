#!/usr/bin/perl -W
###########################################################################
#
# $Id$
#
# XASTIR, Amateur Station Tracking and Information Reporting
# Copyright (C) 2016  The Xastir Group
#
# "ais.pl", a Perl script to connect aisdecoder and Xastir. This script
# will create a UDP server at localhost:10110 for aisdecoder to send
# packets into. It currently decodes AIS sentence types 1/2/3, creates
# APRS packets out of them and sends them to Xastir's server port (2023)
# via UDP.
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
# Run "rtl_ais" like this (create a simple script!):
#   ./rtl_ais -h 127.0.0.1 -P 10110 -d 0 -l 161.975M -r 162.025M -n -p -1 -g 48
#
# Note that the "-p -1" bit is the frequency error of the RTL dongle.
# Set that to the proper number determined from "Kal".
#
#
# Run "ais.pl" like this (again, create a simple script!):
#   ./ais.pl boats <callpass>
#
#
# Antenna length should be:
# 1/2 wave dipole: 2.89' -or- 2' 10.7" -or- 2' 10 5/8"
# 1/4 wave: 1.44' -or- 1' 5.3" -or- 1' 5 5/16"
#
#
# Good AIS info:
#   http://catb.org/gpsd/AIVDM.html
#
# Example packets w/decoding of fields, plus large sample NMEA data file:
#   http://fossies.org/linux/gpsd/test/sample.aivdm
#
# Possible live data feed (Too many connections when I try):
#   ais.exploratorium.edu:80
#
#
# TODO: Look for N/A values, like 63 for Knots, 511 for Course. Don't add
#     those values to the APRS packets.
#
# TODO: Decode country from 1st 3 digits of MMSI ($userID). Note that
# U.S. ships sometimes incorrectly send "669" for those first 3 digits.
#     http://www.itu.int/online/mms/glad/cga_mids.sh?lng=E
#
# TODO: Decode MID out of MMSI ($userID) too:
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
# TODO: Decode Navigation Status
#
###########################################################################


# Enable debug mode by setting this first variable to a 1, then
# can pipe packets into the program's STDIN for testing.
$debug_mode   = 0;

# Turn on/off printing of various types of messages. Enables debugging of a few
# sentence types at a time w/o other messages cluttering up the output.
$print_123    = 1;
$print_5      = 1;
$print_9      = 1;
$print_18     = 1;
$print_19     = 1;
$print_24_A   = 1;  # A Variant
$print_24_B   = 0;  # B Variant, not decoded yet
$print_27     = 1;  # Not decoded correctly yet
$print_others = 1;

$udp_client = "xastir_udp_client";
$xastir_host = "localhost"; # Server where Xastir is running
$xastir_port = 2023;        # 2023 is Xastir default UDP port

%countries = (
    "201" => "Albania (Republic of)",
    "202" => "Andorra (Principality of)",
    "203" => "Austria",
    "204" => "Azores - Portugal",
    "205" => "Belgium",
    "206" => "Belarus (Republic of)",
    "207" => "Bulgaria (Republic of)",
    "208" => "Vatican City State",
    "209" => "Cyprus (Republic of)",
    "210" => "Cyprus (Republic of)",
    "211" => "Germany (Federal Republic of)",
    "212" => "Cyprus (Republic of)",
    "213" => "Georgia",
    "214" => "Moldova (Republic of)",
    "215" => "Malta",
    "216" => "Armenia (Republic of)",
    "218" => "Germany (Federal Republic of)",
    "219" => "Denmark",
    "220" => "Denmark",
    "224" => "Spain",
    "225" => "Spain",
    "226" => "France",
    "227" => "France",
    "228" => "France",
    "229" => "Malta",
    "230" => "Finland",
    "231" => "Faroe Islands - Denmark",
    "232" => "United Kingdom of Great Britain and Northern Ireland",
    "233" => "United Kingdom of Great Britain and Northern Ireland",
    "234" => "United Kingdom of Great Britain and Northern Ireland",
    "235" => "United Kingdom of Great Britain and Northern Ireland",
    "236" => "Gibraltar - United Kingdom of Great Britain and Northern Ireland",
    "237" => "Greece",
    "238" => "Croatia (Republic of)",
    "239" => "Greece",
    "240" => "Greece",
    "241" => "Greece",
    "242" => "Morocco (Kingdom of)",
    "243" => "Hungary",
    "244" => "Netherlands (Kingdom of the)",
    "245" => "Netherlands (Kingdom of the)",
    "246" => "Netherlands (Kingdom of the)",
    "247" => "Italy",
    "248" => "Malta",
    "249" => "Malta",
    "250" => "Ireland",
    "251" => "Iceland",
    "252" => "Liechtenstein (Principality of)",
    "253" => "Luxembourg",
    "254" => "Monaco (Principality of)",
    "255" => "Madeira - Portugal",
    "256" => "Malta",
    "257" => "Norway",
    "258" => "Norway",
    "259" => "Norway",
    "261" => "Poland (Republic of)",
    "262" => "Montenegro",
    "263" => "Portugal",
    "264" => "Romania",
    "265" => "Sweden",
    "266" => "Sweden",
    "267" => "Slovak Republic",
    "268" => "San Marino (Republic of)",
    "269" => "Switzerland (Confederation of)",
    "270" => "Czech Republic",
    "271" => "Turkey",
    "272" => "Ukraine",
    "273" => "Russian Federation",
    "274" => "The Former Yugoslav Republic of Macedonia",
    "275" => "Latvia (Republic of)",
    "276" => "Estonia (Republic of)",
    "277" => "Lithuania (Republic of)",
    "278" => "Slovenia (Republic of)",
    "279" => "Serbia (Republic of)",
    "301" => "Anguilla - United Kingdom of Great Britain and Northern Ireland",
    "303" => "Alaska (State of) - United States of America",
    "304" => "Antigua and Barbuda",
    "305" => "Antigua and Barbuda",
    "306" => "Sint Maarten (Dutch part) - Netherlands (Kingdom of the)",
    "306" => "Bonaire, Sint Eustatius and Saba - Netherlands (Kingdom of the)",
    "306" => "Curaçao - Netherlands (Kingdom of the)",
    "307" => "Aruba - Netherlands (Kingdom of the)",
    "308" => "Bahamas (Commonwealth of the)",
    "309" => "Bahamas (Commonwealth of the)",
    "310" => "Bermuda - United Kingdom of Great Britain and Northern Ireland",
    "311" => "Bahamas (Commonwealth of the)",
    "312" => "Belize",
    "314" => "Barbados",
    "316" => "Canada",
    "319" => "Cayman Islands - United Kingdom of Great Britain and Northern Ireland",
    "321" => "Costa Rica",
    "323" => "Cuba",
    "325" => "Dominica (Commonwealth of)",
    "327" => "Dominican Republic",
    "329" => "Guadeloupe (French Department of) - France",
    "330" => "Grenada",
    "331" => "Greenland - Denmark",
    "332" => "Guatemala (Republic of)",
    "334" => "Honduras (Republic of)",
    "336" => "Haiti (Republic of)",
    "338" => "United States of America",
    "339" => "Jamaica",
    "341" => "Saint Kitts and Nevis (Federation of)",
    "343" => "Saint Lucia",
    "345" => "Mexico",
    "347" => "Martinique (French Department of) - France",
    "348" => "Montserrat - United Kingdom of Great Britain and Northern Ireland",
    "350" => "Nicaragua",
    "351" => "Panama (Republic of)",
    "352" => "Panama (Republic of)",
    "353" => "Panama (Republic of)",
    "354" => "Panama (Republic of)",
    "355" => "Panama (Republic of)",
    "356" => "Panama (Republic of)",
    "357" => "Panama (Republic of)",
    "358" => "Puerto Rico - United States of America",
    "359" => "El Salvador (Republic of)",
    "361" => "Saint Pierre and Miquelon (Territorial Collectivity of) - France",
    "362" => "Trinidad and Tobago",
    "364" => "Turks and Caicos Islands - United Kingdom of Great Britain and Northern Ireland",
    "366" => "United States of America",
    "367" => "United States of America",
    "368" => "United States of America",
    "369" => "United States of America",
    "370" => "Panama (Republic of)",
    "371" => "Panama (Republic of)",
    "372" => "Panama (Republic of)",
    "373" => "Panama (Republic of)",
    "374" => "Panama (Republic of)",
    "375" => "Saint Vincent and the Grenadines",
    "376" => "Saint Vincent and the Grenadines",
    "377" => "Saint Vincent and the Grenadines",
    "378" => "British Virgin Islands - United Kingdom of Great Britain and Northern Ireland",
    "379" => "United States Virgin Islands - United States of America",
    "401" => "Afghanistan",
    "403" => "Saudi Arabia (Kingdom of)",
    "405" => "Bangladesh (People's Republic of)",
    "408" => "Bahrain (Kingdom of)",
    "410" => "Bhutan (Kingdom of)",
    "412" => "China (People's Republic of)",
    "413" => "China (People's Republic of)",
    "414" => "China (People's Republic of)",
    "416" => "Taiwan (Province of China) - China (People's Republic of)",
    "417" => "Sri Lanka (Democratic Socialist Republic of)",
    "419" => "India (Republic of)",
    "422" => "Iran (Islamic Republic of)",
    "423" => "Azerbaijan (Republic of)",
    "425" => "Iraq (Republic of)",
    "428" => "Israel (State of)",
    "431" => "Japan",
    "432" => "Japan",
    "434" => "Turkmenistan",
    "436" => "Kazakhstan (Republic of)",
    "437" => "Uzbekistan (Republic of)",
    "438" => "Jordan (Hashemite Kingdom of)",
    "440" => "Korea (Republic of)",
    "441" => "Korea (Republic of)",
    "443" => "State of Palestine (In accordance with Resolution 99 Rev. Guadalajara, 2010)",
    "445" => "Democratic People's Republic of Korea",
    "447" => "Kuwait (State of)",
    "450" => "Lebanon",
    "451" => "Kyrgyz Republic",
    "453" => "Macao (Special Administrative Region of China) - China (People's Republic of)",
    "455" => "Maldives (Republic of)",
    "457" => "Mongolia",
    "459" => "Nepal (Federal Democratic Republic of)",
    "461" => "Oman (Sultanate of)",
    "463" => "Pakistan (Islamic Republic of)",
    "466" => "Qatar (State of)",
    "468" => "Syrian Arab Republic",
    "470" => "United Arab Emirates",
    "472" => "Tajikistan (Republic of)",
    "473" => "Yemen (Republic of)",
    "475" => "Yemen (Republic of)",
    "477" => "Hong Kong (Special Administrative Region of China) - China (People's Republic of)",
    "478" => "Bosnia and Herzegovina",
    "501" => "Adelie Land - France",
    "503" => "Australia",
    "506" => "Myanmar (Union of)",
    "508" => "Brunei Darussalam",
    "510" => "Micronesia (Federated States of)",
    "511" => "Palau (Republic of)",
    "512" => "New Zealand",
    "514" => "Cambodia (Kingdom of)",
    "515" => "Cambodia (Kingdom of)",
    "516" => "Christmas Island (Indian Ocean) - Australia",
    "518" => "Cook Islands - New Zealand",
    "520" => "Fiji (Republic of)",
    "523" => "Cocos (Keeling) Islands - Australia",
    "525" => "Indonesia (Republic of)",
    "529" => "Kiribati (Republic of)",
    "531" => "Lao People's Democratic Republic",
    "533" => "Malaysia",
    "536" => "Northern Mariana Islands (Commonwealth of the) - United States of America",
    "538" => "Marshall Islands (Republic of the)",
    "540" => "New Caledonia - France",
    "542" => "Niue - New Zealand",
    "544" => "Nauru (Republic of)",
    "546" => "French Polynesia - France",
    "548" => "Philippines (Republic of the)",
    "553" => "Papua New Guinea",
    "555" => "Pitcairn Island - United Kingdom of Great Britain and Northern Ireland",
    "557" => "Solomon Islands",
    "559" => "American Samoa - United States of America",
    "561" => "Samoa (Independent State of)",
    "563" => "Singapore (Republic of)",
    "564" => "Singapore (Republic of)",
    "565" => "Singapore (Republic of)",
    "566" => "Singapore (Republic of)",
    "567" => "Thailand",
    "570" => "Tonga (Kingdom of)",
    "572" => "Tuvalu",
    "574" => "Viet Nam (Socialist Republic of)",
    "576" => "Vanuatu (Republic of)",
    "577" => "Vanuatu (Republic of)",
    "578" => "Wallis and Futuna Islands - France",
    "601" => "South Africa (Republic of)",
    "603" => "Angola (Republic of)",
    "605" => "Algeria (People's Democratic Republic of)",
    "607" => "Saint Paul and Amsterdam Islands - France",
    "608" => "Ascension Island - United Kingdom of Great Britain and Northern Ireland",
    "609" => "Burundi (Republic of)",
    "610" => "Benin (Republic of)",
    "611" => "Botswana (Republic of)",
    "612" => "Central African Republic",
    "613" => "Cameroon (Republic of)",
    "615" => "Congo (Republic of the)",
    "616" => "Comoros (Union of the)",
    "617" => "Cabo Verde (Republic of)",
    "618" => "Crozet Archipelago - France",
    "619" => "Côte d'Ivoire (Republic of)",
    "620" => "Comoros (Union of the)",
    "621" => "Djibouti (Republic of)",
    "622" => "Egypt (Arab Republic of)",
    "624" => "Ethiopia (Federal Democratic Republic of)",
    "625" => "Eritrea",
    "626" => "Gabonese Republic",
    "627" => "Ghana",
    "629" => "Gambia (Republic of the)",
    "630" => "Guinea-Bissau (Republic of)",
    "631" => "Equatorial Guinea (Republic of)",
    "632" => "Guinea (Republic of)",
    "633" => "Burkina Faso",
    "634" => "Kenya (Republic of)",
    "635" => "Kerguelen Islands - France",
    "636" => "Liberia (Republic of)",
    "637" => "Liberia (Republic of)",
    "638" => "South Sudan (Republic of)",
    "642" => "Libya",
    "644" => "Lesotho (Kingdom of)",
    "645" => "Mauritius (Republic of)",
    "647" => "Madagascar (Republic of)",
    "649" => "Mali (Republic of)",
    "650" => "Mozambique (Republic of)",
    "654" => "Mauritania (Islamic Republic of)",
    "655" => "Malawi",
    "656" => "Niger (Republic of the)",
    "657" => "Nigeria (Federal Republic of)",
    "659" => "Namibia (Republic of)",
    "660" => "Reunion (French Department of) - France",
    "661" => "Rwanda (Republic of)",
    "662" => "Sudan (Republic of the)",
    "663" => "Senegal (Republic of)",
    "664" => "Seychelles (Republic of)",
    "665" => "Saint Helena - United Kingdom of Great Britain and Northern Ireland",
    "666" => "Somalia (Federal Republic of)",
    "667" => "Sierra Leone",
    "668" => "Sao Tome and Principe (Democratic Republic of)",
    "669" => "Swaziland (Kingdom of)",
    "670" => "Chad (Republic of)",
    "671" => "Togolese Republic",
    "672" => "Tunisia",
    "674" => "Tanzania (United Republic of)",
    "675" => "Uganda (Republic of)",
    "676" => "Democratic Republic of the Congo",
    "677" => "Tanzania (United Republic of)",
    "678" => "Zambia (Republic of)",
    "679" => "Zimbabwe (Republic of)",
    "701" => "Argentine Republic",
    "710" => "Brazil (Federative Republic of)",
    "720" => "Bolivia (Plurinational State of)",
    "725" => "Chile",
    "730" => "Colombia (Republic of)",
    "735" => "Ecuador",
    "740" => "Falkland Islands (Malvinas) - United Kingdom of Great Britain and Northern Ireland",
    "745" => "Guiana (French Department of) - France",
    "750" => "Guyana",
    "755" => "Paraguay (Republic of)",
    "760" => "Peru",
    "765" => "Suriname (Republic of)",
    "770" => "Uruguay (Eastern Republic of)",
    "775" => "Venezuela (Bolivarian Republic of)"
);




$xastir_user = shift;
chomp $xastir_user;
if ($xastir_user eq "") {
    print "Please enter a callsign for Xastir injection, but not Xastir's callsign/SSID!\n";
    die;
}
$xastir_user =~ tr/a-z/A-Z/;


$xastir_pass = shift;
chomp $xastir_pass;
if ($xastir_pass eq "") {
    print "Please enter a passcode for Xastir injection\n";
    die;
}


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

# # Find out Callsign/SSID of Xastir instance
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


if ($debug_mode) {
    #
    # Debug mode:
    # Read from pipe in a loop
    #
    
    #$received_data = "!AIVDM,1,1,,B,ENkb9Ma89h:9V\@9Q@@@@@@@@@@@;W\@2v=hvOP00003vP000,0*17";
    #$received_data = "!AIVDM,1,1,,B,403Ot`Qv0q1Kio@7<0K?fL702L4q,0*4A";
    #$received_data = "!AIVDM,1,1,,B,D03Ot`Qe\@Nfp00N01FTf9IFdv9H,0*7B";
    #$received_data = "!AIVDM,1,1,,A,15MToB0ukAo?8H\@KKC0DtCq00pBU,0*0F";   # Message Type 1
    #$received_data = "!AIVDM,1,1,,B,15Mf@6P001G?\@vdKFC:ckaD@0<26,0*26";   # Message Type 1
    #$received_data = "!AIVDM,1,1,,A,15MToB0w3?o?MG8KEswnQU:@0h4S,0*1A";    # Message Type 1
    #$received_data = "!AIVDM,1,1,,B,15Mf@6P001G?\@v>KFC5;kaD@084M,0*52";   # Message Type 1
    #$received_data = "!AIVDM,1,1,,B,25Cjtd0Oj;Jp7ilG7=UkKBoB0<06,0*60";    # Message Type 2
    #$received_data = "!AIVDM,1,1,,A,38Id705000rRVJhE7cl9n;160000,0*40";    # Message Type 3
    #$received_data = "!AIVDM,2,1,1,A,55?MbV02;H;s<HtKR20EHE:0\@T4\@Dn2222222216L961O5Gf0NSQEp6ClRp8,0*1C";   # Message Type 5
    #$received_data = "!AIVDM,1,1,,A,91b77=h3h00nHt0Q3r@@07000<0b,0*69";    # Message Type 9
    #$received_data = "!AIVDM,1,1,,B,91b55wi;hbOS@OdQAC062Ch2089h,0*30";    # Message Type 9
    #$received_data = "!AIVDM,1,1,,A,B52K>;h00Fc>jpUlNV@ikwpUoP06,0*4C";    # Message Type 18 
    #$received_data = "!AIVDM,1,1,,A,B52KB8h006fu`Q6:g1McCwb5oP06,0*00";    # Message Type 18 
    #$received_data = "!AIVDM,1,1,,B,B5O6hr00<veEKmUaMFdEow`UWP06,0*4F";    # Message Type 18 
    #$received_data = "!AIVDM,1,1,,B,C5N3SRgPEnJGEBT>NhWAwwo862PaLELTBJ:V00000000S0D:R220,0*0B";    # Message Type 19
    #$received_data = "!AIVDM,1,1,,A,H42O55i18tMET00000000000000,2*6D";     # Message Type 24
    #$received_data = "!AIVDM,1,1,,A,H42O55lti4hhhilD3nink000?050,0*40";    # Message Type 24
    #$received_data = "!AIVDM,1,1,,A,KCQ9r=hrFUnH7P00,0*41";                # Message Type 27
    #$received_data = "!AIVDM,1,1,,B,KC5E2b@U19PFdLbMuc5=ROv62<7m,0*16";    # Message Type 27

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
    use IO::Socket;
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

    if ($debug_mode) {
        $received_data =~ s/\n//;   # Remove LF if present
        $received_data =~ s/\r//;   # Remove CR if present
    }
    else {  # Live mode, receiving from rtl-ais. CRLF line-ends.
        chop($received_data);
        chop($received_data);
    }

    # Verify the checksum: Refuse to process a message if it's bad.
    # Computed on entire sentence including the AIVDM tag but excluding
    # the leading "!" character.
    my $str = $received_data;
    $str =~ s/^!//;
    $str =~ s/(,\d)\*../$1/;

    # Received checksum. Catches the correct bytes except
    # for those cases where the NMEA sentences continues
    # on past the checksum.
    my $str2 = $received_data;
    $str2 =~ s/.*(..)/$1/;

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
    	#printf("*** Bad checksum: Received:%s  Computed:%s ***\n\n", $str2, $sum2);
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
#    print "Message Type: $message_type\n";
    # If not messages types 1, 2, 3, skip for now
    if (    $message_type !=  1
         && $message_type !=  2
         && $message_type !=  3
         && $message_type !=  5
         && $message_type !=  9
         && $message_type != 18
         && $message_type != 19
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

    elsif ( $message_type == 24 ) {
        #if ($print_24) { print " Msg Type: $message_type\t"; }   # This is printed from the subroutine instead
        &process_type_24($message_type);
    }

    elsif ( $message_type == 27 ) {
        if ($print_27) { print " Msg Type: $message_type\n"; }
        &process_type_27($message_type);
    }
   
} # End of "process_line"



# Message types 1, 2 and 3: Position Report Class A
#
sub process_types_1_2_3() {
    # substr($bin_string,   0,   6); # Message Type
    #my $brepeat_indicator = substr($bin_string, 6, 2);
    my $bUserID = substr($bin_string, 8, 30); # MMSI
    #my $bnav_status = substr($bin_string, 38, 4);
    #my $brateOfTurn = substr($bin_string, 42, 8);
    my $bSpeedOverGnd = substr($bin_string, 50, 10);    #####
    #my $bpositionAccuracy = substr($bin_string, 60, 1);
    my $bLongitude = substr($bin_string, 61, 28);   #####
    my $bLatitude = substr($bin_string, 89, 27);    #####
    my $bCourseOverGnd = substr($bin_string, 116, 12); #####
    #my $btrueHeading = substr($bin_string, 128, 9);
    #my $btimestamp = substr($bin_string, 137, 6);
    #my $bregional = substr($bin_string, 143, 2);
    #my $bspare = substr($bin_string, 145, 3);
    #my $bRAIM = substr($bin_string, 148, 1);
    #my $bSOTDMAsyncState = substr($bin_string, 149, 3);
    #my $bSOTDMAslotTimeout = substr($bin_string, 151, 3);
    #my $bSOTDMAslotOffset = substr($bin_string, 154, 14);

    my $userID = &bin2dec($bUserID);
    if ($print_123) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_123) { print "  Country: $country\n"; }

# NOTE: 3600 = N/A
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    if ($courseOverGnd == 0) {
        $courseOverGnd = 360;
    }
    if ($print_123) { print "   Course: $courseOverGnd\n"; }
    my $course = sprintf("%03d", $courseOverGnd);

# NOTE: 1023 = N/A
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    if ($print_123) { print "    Speed: $speedOverGnd\n"; }
    my $speed = sprintf("%03d", $speedOverGnd);

# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
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

# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
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
    my $aprs="$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed";
    if ($print_123) { print "     APRS: $aprs\n"; }
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }

    if ($print_123) { print "\n"; }

    return();
}



# Message type 5: Static and Voyage Related Data (Vessel name, callsign, ship type)
#
sub process_type_5() {
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   2); # AIS Version
    # substr($bin_string,  40,  30); # IMO Number
    # substr($bin_string,  70,  42); # Call Sign
    my $bVesselName = substr($bin_string, 112, 120); # Vessel Name #####
    # substr($bin_string, 232,   8); # Ship Type
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

    my $country = &decode_MID($userID);
    if ($print_5) { print "  Country: $country\n"; }

    my $vesselName = &bin2text($bVesselName);
    if ($print_5) { print "   Vessel: $vesselName\n"; }

    # Assign tactical call = $vesselName
    # Max tactical call in Xastir is 57 chars (56 + terminator?)
    #
    $aprs = $xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $vesselName;
    if ($print_5) { print "     APRS: $aprs\n"; }
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }

    if ($print_5) { print "\n"; }

    return();
}



# Message type 9: Standard SAR Aircraft Position Report
#
sub process_type_9() {
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
    # substr($bin_string, 134,   8); # Regional reserved
    # substr($bin_string, 142,   1); # DTE
    # substr($bin_string, 143,   3); # Spare
    # substr($bin_string, 146,   1); # Assigned
    # substr($bin_string, 147,   1); # RAIM flag
    # substr($bin_string, 148,  20); # Radio status

    my $userID = &bin2dec($bUserID);
    if ($print_9) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_9) { print "  Country: $country\n"; }

# NOTE: 4095 = N/A
# NOTE: 4094 = 4094 meters or higher.
    my $altitude_meters = &bin2dec($bAltitude);
    my $altitude = "";
    if ($altitude_meters != 4095) {
      $altitude = sprintf( "%s%06d", " /A=", $altitude_meters * 3.28084 );
      if ($print_9) { print " Altitude: $altitude\n"; }
    }

# NOTE: 3600 = N/A
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    if ($courseOverGnd == 0) {
        $courseOverGnd = 360;
    }
    if ($print_9) { print "   Course: $courseOverGnd\n"; }
    my $course = sprintf("%03d", $courseOverGnd);

# NOTE: 1023 = N/A
# NOTE: 1022 = 1022 knots or higher
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    if ($print_9) { print "    Speed: $speedOverGnd\n"; }
    my $speed = sprintf("%03d", $speedOverGnd);

# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
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

# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
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
    my $aprs="$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed$altitude SAR Aircraft";
    if ($print_9) { print "     APRS: $aprs\n"; }
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }
 
    if ($print_9) { print "\n"; }

    return();
}



# Message type 18: Standard Class B CS Position Report
#
sub process_type_18() {
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   8); # Regional Reserved
    my $bSpeedOverGnd = substr($bin_string,  46,  10); # Speed Over Ground
    # substr($bin_string,  56,   1); # Position Accuracy
    my $bLongitude = substr($bin_string,  57,  28); # Longitude
    my $bLatitude = substr($bin_string,  85,  27); # Latitude
    my $bCourseOverGnd = substr($bin_string, 112,  12); # Course Over Ground
    # substr($bin_string, 124,   9); # True Heading
    # substr($bin_string, 133,   6); # Time Stamp
    # substr($bin_string, 139,   2); # Regional reserved
    # substr($bin_string, 141,   1); # CS Unit
    # substr($bin_string, 142,   1); # Display flag
    # substr($bin_string, 143,   1); # DSC Flag
    # substr($bin_string, 144,   1); # Band flag
    # substr($bin_string, 145,   1); # Message 22 flag
    # substr($bin_string, 146,   1); # Assigned
    # substr($bin_string, 147,   1); # RAIM flag
    # substr($bin_string, 148,  20); # Radio status
 
    my $userID = &bin2dec($bUserID);
    if ($print_18) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_18) { print "  Country: $country\n"; }

# NOTE: 3600 = N/A
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    if ($courseOverGnd == 0) {
        $courseOverGnd = 360;
    }
    if ($print_18) { print "   Course: $courseOverGnd\n"; }
    my $course = sprintf("%03d", $courseOverGnd);

# NOTE: 1023 = N/A
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    if ($print_18) { print "    Speed: $speedOverGnd\n"; }
    my $speed = sprintf("%03d", $speedOverGnd);

# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
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

# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
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
    my $aprs="$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed";
    if ($print_18) { print "     APRS: $aprs\n"; }
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }
 
    if ($print_18) { print "\n"; }

    return();
}



# Message type 19: Extended Class B CS Position Report
#
sub process_type_19() {
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   8); # Regional Reserved
    my $bSpeedOverGnd = substr($bin_string,  46,  10); # Speed Over Ground
    # substr($bin_string,  56,   1); # Position Accuracy
    my $bLongitude = substr($bin_string,  57,  28); # Longitude
    my $bLatitude = substr($bin_string,  85,  27); # Latitude
    my $bCourseOverGnd = substr($bin_string, 112,  12); # Course Over Ground
    # substr($bin_string, 124,   9); # True Heading
    # substr($bin_string, 133,   6); # Time Stamp
    # substr($bin_string, 139,   4); # Regional reserved
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

# NOTE: 3600 = N/A
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    if ($courseOverGnd == 0) {
        $courseOverGnd = 360;
    }
    if ($print_19) { print "   Course: $courseOverGnd\n"; }
    my $course = sprintf("%03d", $courseOverGnd);

# NOTE: 1023 = N/A
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    if ($print_19) { print "    Speed: $speedOverGnd\n"; }
    my $speed = sprintf("%03d", $speedOverGnd);

# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600000.0;
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

# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600000.0;
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

    my $symbol = "s";
    my $aprs="$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed";
    if ($print_19) { print "     APRS: $aprs\n"; }
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }

    my $vesselName = "";
    if ($bVesselName ne "") { 
        $vesselName = &bin2text($bVesselName);
    }
    if ($vesselName ne "") {
        if ($print_19) { print "   Vessel: $vesselName\n"; }

        # Assign tactical call = $vesselName
        # Max tactical call in Xastir is 57 chars (56 + terminator?)
        #
        $aprs = $xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $vesselName;
        if ($print_19) { print "     APRS: $aprs\n"; }
        # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
        $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
        if ($result =~ m/NACK/) {
            die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
        }
    }

    if ($print_19) { print "\n"; }

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
sub process_type_24() {
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
        }
        if ($vesselName ne "") {
            if ($print_24_A) { print "   Vessel: $vesselName\n"; }
    
            # Assign tactical call = $vesselName
            # Max tactical call in Xastir is 57 chars (56 + terminator?)
            #
            $aprs = $xastir_user . '>' . "APRS::TACTICAL :" . $userID . "=" . $vesselName;
            if ($print_24_A) { print "     APRS: $aprs\n"; }
            # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
            $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
            if ($result =~ m/NACK/) {
                die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
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

        # Alternate format starting at bit 40:
        # substr($bin_string,  40,   8); # Ship Type
        # substr($bin_string,  48,  18); # Vendor ID
        # substr($bin_string,  66,   4); # Unit Model Code
        # substr($bin_string,  70,  20); # Serial Number)
        # substr($bin_string,  90,  42); # Call Sign
        # substr($bin_string, 132,   9); # Dimension to bow
        # substr($bin_string, 141,   9); # Dimension to Stern
        # substr($bin_string, 150,   6); # Dimension to Port
        # substr($bin_string, 156,   6); # Dimension to Starboard

        # Alternate format starting at bit 132:
        # substr($bin_string, 132,  30); # Mothership MMSI
        # substr($bin_string, 162,   6); # Spare

        if ($print_24_B) { print "\n"; }
    }

    return();
}



# Message type 27: Position Report For Long-Range Applications (rare)
#
sub process_type_27() {
    # substr($bin_string,   0,   6); # Message Type
    # my $brepeat_indicator = substr($bin_string,   6,   2); # Repeat Indicator
    my $bUserID = substr($bin_string,   8,  30); # MMSI
    # substr($bin_string,  38,   1); # Position Accuracy
    # substr($bin_string,  39,   1); # RAIM flag
    # substr($bin_string,  40,   4); # Navigation Status
    my $bLongitude = substr($bin_string,  44,  18); # Longitude
    my $bLatitude = substr($bin_string,  62,  17); # Latitude
    my $bSpeedOverGnd = substr($bin_string,  79,   6); # Speed Over Ground
    my $bCourseOverGnd = substr($bin_string,  85,   9); # Course Over Ground
    # substr($bin_string,  94,   1); # GNSS Position Status
    # substr($bin_string,  95,   1); # SPare

    my $userID = &bin2dec($bUserID);
    if ($print_27) { print "  User ID: $userID\n"; }

    my $country = &decode_MID($userID);
    if ($print_27) { print "  Country: $country\n"; }

# NOTE: 511 = N/A 
    my $courseOverGnd = &bin2dec($bCourseOverGnd) / 10 ;
    if ($courseOverGnd == 0) {
        $courseOverGnd = 360;
    }
    if ($print_27) { print "   Course: $courseOverGnd\n"; }
    my $course = sprintf("%03d", $courseOverGnd);

# NOTE: 63 = N/A
    my $speedOverGnd = &bin2dec($bSpeedOverGnd) / 10;
    if ($print_27) { print "    Speed: $speedOverGnd\n"; }
    my $speed = sprintf("%03d", $speedOverGnd);

# NOTE: 91 = N/A
    my $latitude = &signedBin2dec($bLatitude) / 600.0;
    my $NS;
    if ($print_27) { printf(" Latitude: %07.5f\n", $latitude); }
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

# NOTE: 181 = N/A
    my $longitude = &signedBin2dec($bLongitude) / 600.0;
    my $EW;
    if ($print_27) { printf("Longitude: %08.5f\n", $longitude); }
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
    my $aprs="$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed";
    if ($print_27) { print "     APRS: $aprs\n"; }
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
#    my $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
#    if ($result =~ m/NACK/) {
#        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
#    }
 
    if ($print_27) { print "\n"; }
 
    return();
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
        my $d = unpack("N", pack("B32", substr("0" x 32 . $binary_char, -32)));
        if ($d > 0) {   # Skip NULL characters
            if ($d < 32) { $d += 64; }
            $final_string = $final_string . chr($d);
        }
    }
    $final_string =~ s/\s\s/ /g;
    return($final_string);
}
 


# Convert a signed binary string of ASCII 0's and 1's to a decimal
sub signedBin2dec {
    my $input = shift;
    if ( substr($input, 0, 1) eq "1") { # Negative number
        # Invert all the bits, add 1, convert to decimal, add '-' sign.
        my $binary = pack("B32", substr("1" x 32 . $input, -32));
        $binary = ~$binary;
        my $output = unpack("N", $binary);
        $output++;
        $output = -$output;
        return $output;
    }
    else { # Positive number
        $output = unpack("N", pack("B32", substr("0" x 32 . $input, -32))); #####
        return $output;
    }
}



# Convert an unsigned binary string of ASCII 0's and 1's to a decimal
sub bin2dec {
    return unpack("N", pack("B32", substr("0" x 32 . shift, -32))); #####
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
sub decode_MID {
    $userID = shift;
    $MID = $userID;
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

    $country = "";
    if (defined($countries{$MID})) {
        $country = $countries{$MID};
    }
    else {
        $country = "*** Unknown ***";
    }

    return $country; 
}



