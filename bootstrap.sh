#!/bin/sh
#
# Copyright (C) 2000-2004  The Xastir Group
#
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#

echo "Removing autom4te.cache directory"
rm -rf autom4te.cache

echo "Running aclocal"
aclocal

echo "Running autoheader"
autoheader

echo "Running autoconf"
autoconf

# Cygwin needs these params to be separate
echo "Running automake"
automake -a -c

