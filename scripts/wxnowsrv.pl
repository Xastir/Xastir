#!/usr/bin/env perl
# Copyright (C) 2000-2023 The Xastir Group
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

# This script is used to feed weather data from a "Wxnow.txt" file
# into Xastir as if it were a networked weather station.  Wxnow.txt is
# a standard file format that is created by a number different types
# of weather station control software, and Xastir is unable to read
# this file directly.
#
# The command line arguments are the path to the Wxnow.txt file, and
# a polling period in seconds (this should be chosen to be the same period
# that the Wxnow.txt file is updated by whatever tool is creating it.
# An optional third argument allows the user to specify an alternate
# server port other than 5500.

# The script works by polling the Wxnow.txt file each $POLLTIME, and if
# anything changes, flagging the change.  This is done in one thread.

# For each client that connects to the port, a new thread is created for
# communicating with that client over a socket.  Each time the polling thread
# flags that data has changed, a string of weather data is sent to all connected
# clients.

# This script leverages the "Davis METEO" code in Xastir that implements
# the networked weather station interface.  As such, all weather stations
# connected in this way to Xastir are assumed to be Davis weather stations,
# no matter what type of weather station is connected.

# Once this script is running, add a "Networked WX" interface to your Xastir
# config, and provide the IP address on which the script is running and the
# port number (default 5500) in the interface properties.

# Once Xastir connects to the script, "View->Own Weather Data" should show
# the current weather data in Wxnow.txt, and be updated every time Wxnow.txt
# changes.

use threads;
use threads::shared;
use IO::Socket;
use Net::hostent;              # for OO version of gethostbyaddr

sub read_wxnow {
    my ($pathname,$lastwxref,$wxchangedref,$POLLTIME) = @_;
    my $slash="/";

    while ( -e $pathname)
    {
        open (WXNOWFILE, "<$pathname") || die "Cannot open wxnow file $pathname\n";
        my $firstline=1;
        while (<WXNOWFILE>)
        {
            if ($firstline==1)
            {
                $firstline=0;
                next;
            }
            else
            {
                $newwx=$_;
                chomp($newwx);
                $newwx =~ s/^([0-9]{3})$slash([0-9]{3})g/c$1s$2g/;
                $newwx = $newwx . "xDvs";
                if ($newwx ne $$lastwxref)
                {
                    $$lastwxref=$newwx;
                    $$wxchangedref =1;
                }
                else
                {
                    $$wxchangedref=0;
                }
            }
        }
        close (WXNOWFILE);
        sleep $POLLTIME;
    }
}

sub handle_connection {
    my ($socket,$lastwxref,$wxchangedref,$POLLTIME) = @_;
    my $disconn = 0;
    my $firsttime=1;
    $socket->autoflush(1);
    my $hostinfo = gethostbyaddr($socket->peeraddr);

    while (1) {
      $foobar=getpeername($socket) or $disconn=1;
      if ($socket->connected())
      {
          $disconn=0;
      }
      else
      {
          $disconn=1;
      }
      if ($disconn==0)
      {
          if ($$wxchangedref==1 || $firsttime==1)
          {
              send($socket,"$$lastwxref\n",0);
              $firsttime=0;
          }
          sleep $POLLTIME/2;
      }
      last if $disconn==1;
    }
}

if ( $#ARGV < 1)
{
    print STDERR "Usage:  $0 wxnowpath pollsecs [port]\n";
    print STDERR " where wxnowpath is the full path to your wxnow.txt file\n";
    print STDERR " and pollsecs is how often we should check this file for changes (in seconds)\n";
    print STDERR " If given, the third argument is the port number on which to listen.  Default=5500";
    exit(1);
}
$SERVER_PORT=5500;

if ( $#ARGV >1)
{
    $SERVER_PORT=$ARGV[2];
}
my $listen = IO::Socket::INET->new(
    Proto => 'tcp',
    LocalPort => $SERVER_PORT,
    ReuseAddr => 1,
    Listen => SOMAXCONN
    );

my $lastwx : shared ="";
my $wxchanged : shared =0;
my $POLLTIME  =$ARGV[1];

$pathname=$ARGV[0];


async(\&read_wxnow, $pathname,\$lastwx,\$wxchanged,$POLLTIME)->detach;

while (my $socket = $listen->accept) {
    async(\&handle_connection, $socket,\$lastwx,\$wxchanged,$POLLTIME)->detach;
}

