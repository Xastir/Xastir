#!/bin/sh
#
# $Id$
#
# Copyright (C) 2000-2006  The Xastir Group
#
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#

echo "    6) Removing autom4te.cache directory..."
rm -rf autom4te.cache

echo "    5) Running aclocal..."
aclocal

echo "    4) Running autoheader..."
autoheader

echo "    3) Running autoconf..."
autoconf

# Cygwin needs these params to be separate
echo "    2) Running automake..."
automake -a -c

# Automake-1.9 on SuSE 10 doesn't copy mkinstalldirs.  Check whether
# it is missing and copy it over ourselves if so.
if [ -e "mkinstalldirs" ]
then
  echo "    1) Checking for 'mkinstalldirs'... Found!"
else
  echo "    1) Checking for 'mkinstalldirs'... Not Found"
  echo "       Copying it from '/usr/share/automake*'"
  cp /usr/share/automake*/mkinstalldirs .
  if [ -e "mkinstalldirs" ]
  then
    echo "       Checking for 'mkinstalldirs'... Found!"
  else
    echo "***ERROR: Couldn't copy the file***"
    cp /usr/share/automake*/mkinstalldirs .
  fi
fi

echo "Bootstrap complete."

