# $Id$
AUTOMAKE_OPTIONS = gnu dist-bzip2
ACLOCAL_AMFLAGS = -I m4

D=      `date +%G%m%d.%H%M%S`

SUBDIRS = src m4 config help scripts symbols callpass 

# Remove this file here (it is created via configure), not from within intl.
# DISTCLEANFILES = intl/libintl.h

EXTRA_DIST = xastir.spec.in xastir.spec config.guess config.sub install-sh \
        missing mkinstalldirs placeholder FAQ LICENSE README \
        README.CVS README.MAPS README.win32 DEBUG_LEVELS \
        bootstrap.sh INSTALL UPGRADE xastir.1

MAINTAINERCLEANFILES = configure aclocal.m4 Makefile.in Makefile \
        config.guess config.sub install-sh missing mkinstalldirs \
        xastir.spec

countydir=${prefix}/xastir/Counties
county_DATA=placeholder

fccdir=${prefix}/xastir/fcc
fcc_DATA=placeholder

gnisdir=${prefix}/xastir/GNIS
gnis_DATA=placeholder

mapdir=${prefix}/xastir/maps
map_DATA=placeholder

gpsdir=${prefix}/xastir/maps/GPS
gps_DATA=placeholder

sounddir=${prefix}/xastir/sounds
sound_DATA=placeholder

docdir=${prefix}/xastir/doc
doc_DATA=AUTHORS FAQ ChangeLog LICENSE README README.CVS \
        README.MAPS README.win32 COPYING INSTALL UPGRADE

mandir=${prefix}/man
man_MANS=xastir.1

ChangeLog::


