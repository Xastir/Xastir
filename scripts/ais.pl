#!/usr/bin/perl -W
###########################################################################
# "ais.pl", a Perl script to connect aisdecoder and Xastir. This script
# will create a UDP server at localhost:10110 for aisdecoder to send
# packets into. It currently decodes AIS sentence types 1/2/3, creates
# APRS packets out of them and sends them to Xastir's server port (2023)
# via UDP.
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
# Good AIS info:
# http://catb.org/gpsd/AIVDM.html
#
###########################################################################


$udp_client = "xastir_udp_client";
$xastir_host = "localhost"; # Server where Xastir is running
$xastir_port = 2023;        # 2023 is Xastir default UDP port


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

chomp($remote_ssid);
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

    #$received_data = "!AIVDM,1,1,,B,ENkb9Ma89h:9V\@9Q@@@@@@@@@@@;W\@2v=hvOP00003vP000,0*17";
    #$received_data = "!AIVDM,1,1,,B,403Ot`Qv0q1Kio@7<0K?fL702L4q,0*4A";
    #$received_data = "!AIVDM,1,1,,B,D03Ot`Qe\@Nfp00N01FTf9IFdv9H,0*7B";
    #$received_data = "!AIVDM,1,1,,A,15MToB0ukAo?8H\@KKC0DtCq00pBU,0*0F";   # Message Type 1
    #$received_data = "!AIVDM,1,1,,B,15Mf@6P001G?\@vdKFC:ckaD@0<26,0*26";   # Message Type 1
    #$received_data = "!AIVDM,1,1,,A,15MToB0w3?o?MG8KEswnQU:@0h4S,0*1A";    # Message Type 1
    #$received_data = "!AIVDM,1,1,,B,15Mf@6P001G?\@v>KFC5;kaD@084M,0*52";   # Message Type 1

    chomp($received_data);
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
    #$brepeat_indicator = substr($bin_string, 6, 2);
    $buserID = substr($bin_string, 8, 30);
    #$bnav_status = substr($bin_string, 38, 4);
    #$brateOfTurn = substr($bin_string, 42, 8);
    $bspeedOverGnd = substr($bin_string, 50, 10);
    #$bpositionAccuracy = substr($bin_string, 60, 1);
    $blongitude = substr($bin_string, 61, 28);
    $blatitude = substr($bin_string, 89, 27);
    $bcourseOverGnd = substr($bin_string, 116, 12);
    #$btrueHeading = substr($bin_string, 128, 9);
    #$btimestamp = substr($bin_string, 137, 6);
    #$bregional = substr($bin_string, 143, 2);
    #$bspare = substr($bin_string, 145, 3);
    #$bRAIM = substr($bin_string, 148, 1);
    #$bSOTDMAsyncState = substr($bin_string, 149, 3);
    #$bSOTDMAslotTimeout = substr($bin_string, 151, 3);
    #$bSOTDMAslotOffset = substr($bin_string, 154, 14);

    $message_type = &bin2dec($bmessage_type);
    #print "Message Type: $message_type\n";
    # If not messages types 1, 2, 3, skip for now
    if ($message_type > 3) {
        next;
    }

    $userID = &bin2dec($buserID);
    print "User ID: $userID\n";

    $courseOverGnd = &bin2dec($bcourseOverGnd) / 10 ;
    if ($courseOverGnd == 0) {
        $courseOverGnd = 360;
    }
    print "Course: $courseOverGnd\n";
    $course = sprintf("%03d", $courseOverGnd);

    $speedOverGnd = &bin2dec($bspeedOverGnd) / 10;
    print "Speed: $speedOverGnd\n";
    $speed = sprintf("%03d", $speedOverGnd);

    $latitude = &signedBin2dec($blatitude) / 600000.0;
    print "Latitude: $latitude\n";
    if ($latitude >= 0.0) {
        $NS = 'N';
    } else {
        $NS = 'S';
        $latitude = abs($latitude);
    }
    $latdeg = int($latitude);
    $latmins = $latitude - $latdeg;
    $latmins2 = $latmins * 60.0;
    $lat = sprintf("%02d%05.2f%s", $latdeg, $latmins2, $NS);

    $longitude = &signedBin2dec($blongitude) / 600000.0;
    print "Longitude: $longitude\n";
    if ($longitude >= 0.0) {
        $EW = 'E';
    } else {
        $EW = 'W';
        $longitude = abs($longitude);
    }
    $londeg = int($longitude);
    $lonmins = $longitude - $londeg;
    $lonmins2 = $lonmins * 60.0;
    $lon = sprintf("%03d%05.2f%s", $londeg, $lonmins2, $EW);

    print "\n";

    $symbol = "s";
    $aprs="$xastir_user>APRS:)$userID!$lat/$lon$symbol$course/$speed";
    print "$aprs\n";
    # xastir_udp_client  <hostname> <port> <callsign> <passcode> {-identify | [-to_rf] <message>}
    $result = `$udp_client $xastir_host $xastir_port $xastir_user $xastir_pass \"$aprs\"`;
    if ($result =~ m/NACK/) {
        die "Received NACK from Xastir: Callsign/Passcode don't match?\n";
    }
} # End of main processing loop



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
        $output = unpack("N", pack("B32", substr("0" x 32 . $input, -32)));
        return $output;
    }
}



# Convert an unsigned binary string of ASCII 0's and 1's to a decimal
sub bin2dec {
    return unpack("N", pack("B32", substr("0" x 32 . shift, -32)));
}



# Convert a decimal string (ASCII) to a binary string (ASCII)
sub dec2bin {
    my $input = "00000000000000000000000000" . shift;
    my $str = unpack("B32", pack("N", $input));
    my $str2 = substr($str, -6); 
    return $str2;
}


