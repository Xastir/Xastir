#!/bin/sh
#
# This simple routine will run autostuff in the appropriate
# order to generate the needed configure/makefiles
#
aclocal
autoheader
autoconf
automake -a