#!/bin/sh
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#
echo "Running aclocal"
aclocal

echo "Running autoheader"
autoheader

echo "Running autoconf"
autoconf

# Cygwin needs these params to be separate
echo "Running automake"
automake -a -c

