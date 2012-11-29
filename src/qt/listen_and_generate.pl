#!/usr/bin/perl -w

# listen_and_generate.pl
# Create a listener port which sends some text back to clients.
# Multiple clients can connect at once.

$server_port = 2023;
$packet_prefix = "WE7U>CQ,WIDE2-2:>Test Packet";

use Socket;
use IO::Handle;
use POSIX qw(:errno_h);

$SIG{CHLD} = 'IGNORE';
$sockaddr = 'S n a4 x8';
socket(SERVER, PF_INET, SOCK_STREAM, getprotobyname('tcp'));
setsockopt(SERVER, SOL_SOCKET, SO_REUSEADDR, 1);
$my_addr = sockaddr_in($server_port, INADDR_ANY);
bind(SERVER, $my_addr) or die "Couldn't bind to port $server_port : $!\n";
listen(SERVER, SOMAXCONN) or die "Couldn't listen on port $server_port : $!\n";

while (1) {
  $client_addr = accept(CLIENT, SERVER);
  $af = "";
  ($af, $port, $inetaddr) = unpack($sockaddr, $client_addr);
  @inetaddr = unpack('C4', $inetaddr);
  $client_ip = join('.', @inetaddr);
  printf("Connection from $client_ip port $port\n");
 
  if ((fork()) == 0) {  # Child process
    $packet_count = 1;
    $done = 0;
    while (!$done) {
      $packet = "$packet_prefix $packet_count\n";
      defined (send(CLIENT, $packet, 0)) or $done++;
      printf("  Sent:  $packet");
      $packet_count++;
      sleep(1);
    }
    # We never get to here
    printf("$client_ip port $port disconnected\n");
    close(CLIENT);
    exit;
  }

  # Parent process
  close(CLIENT);
}
close(SERVER);


