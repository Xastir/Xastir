#!/bin/sh
#
# $Id$
#
# Copyright (C) 2000-2007  The Xastir Group
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

# Cygwin needs these parameters to be separate.
echo "    2) Running automake..."
automake -a -c

# Automake-1.9 on SuSE 10 doesn't copy over "mkinstalldirs" if
# missing.  Check whether it is missing and copy it over ourselves
# if so.
if test -x "mkinstalldirs"
then
  echo "    1) Checking for 'mkinstalldirs'... Found!"
else
  echo "    1) Checking for 'mkinstalldirs'... Not Found"
  echo "       Attempting to copy it from system directories'"
  (cp /usr/local/share/automake*/mkinstalldirs . 2>/dev/null)
  # Did we succeed?
  if [ $? ]
  then
    # Failed the copy above, try in another directory.
    (cp /usr/share/automake*/mkinstalldirs . 2>/dev/null)
  fi
  # Check whether we have the file now in our current directory and
  # that it is executable.
  if test -x "mkinstalldirs"
  then
    echo "       Checking for 'mkinstalldirs'... Found!"
  else
    echo "***ERROR: Couldn't copy the file***"
  fi
fi

echo "Bootstrap complete."

