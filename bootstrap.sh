#!/bin/sh
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#

aclocal

autoheader

autoconf

# Cygwin needs these params to be separate
automake -a -c

