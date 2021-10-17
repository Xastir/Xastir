#!/bin/sh
#This script prints the short-format git SHA-1 string if the source directory
# has a ".git" directory.
#
# This will typically only be called from a Makefile, with $(top_srcdir) as the
# first argument.  Its purpose is so we can inject the git commit ID
# into the "Help->About" dialog box.

SRCDIR=$1
SHASTRING=""
cd $SRCDIR
if [ -e .git ]
then
    GITSHA=`git describe --dirty`
    SHASTRING=" (${GITSHA})"
fi
echo $SHASTRING
