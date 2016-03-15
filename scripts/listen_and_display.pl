#!/usr/bin/perl -w

# listen_and_display.pl:  Create a listener port which dumps anything received to STDOUT

$server_port = 2024;

$| = 1;

use Socket;
use IO::Handle;
use POSIX qw(:errno_h);

# Make the socket
socket(SERVER, PF_INET, SOCK_STREAM, getprotobyname('tcp'));

# So we can restart our server quickly
setsockopt(SERVER, SOL_SOCKET, SO_REUSEADDR, 1);

# Build up my socket address
$my_addr = sockaddr_in($server_port, INADDR_ANY);
bind(SERVER, $my_addr)
  or die "Couldn't bind to port $server_port : $!\n";

# Establish a queue for incoming connections
listen(SERVER, SOMAXCONN)
  or die "Couldn't listen on port $server_port : $!\n";

# Accept and process connections
while (accept(CLIENT, SERVER)) {

  printf("\n*** A client connected on port $server_port ***\n");

  CLIENT->autoflush(1);

  while (<CLIENT>) {
    print;
  }

  printf("\n*** The client disconnected ***\n");

}

close(SERVER);


