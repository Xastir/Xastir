my $file = $ARGV[0] || "-";
 
my %from = ();
 
open FILE, "< $file" or die "Can't open $file : $!";
 
while( <FILE> ) {
    if (/^EN\|(\d+)\|\|\|(\w+)\|.*/) {
	$x = $1;
	$z = $2;
	$y = $_;
	if (defined $from{$2}) {
	    if ($from{$z} =~ /^EN\|(\d+)\|\|\|(\w+)\|.*/) {
		if ($1 < $x) {
		    $replaced++;
		    $from{$2} = $y }
	    }
	} else {
	    $from{$2} = $_
	}
    }
}
 
close FILE;
 
for my $callsign ( sort keys %from ) {
    print "$from{$callsign}";
}

print STDERR "Total callsigns:  " . keys( %from ) . ".\n";
print STDERR " Replaced callsigns:  " . $replaced . ".\n";
