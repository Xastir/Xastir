
# $Id$
#
# Copyright (C) 2000-2004  The Xastir Group
#

AUTOMAKE_OPTIONS = gnu dist-bzip2
ACLOCAL_AMFLAGS = -I m4

D=      `date +%G%m%d.%H%M%S`

SUBDIRS = src m4 config help scripts symbols callpass

# Remove this file here (it is created via configure), not from within intl.
# DISTCLEANFILES = intl/libintl.h

EXTRA_DIST = xastir.spec.in xastir.spec config.guess config.sub install-sh \
        missing mkinstalldirs placeholder FAQ LICENSE README README.CVS \
        README.Getting-Started README.MAPS README.win32 DEBUG_LEVELS \
        bootstrap.sh INSTALL UPGRADE xastir.1 terraserver.geo \
        toposerver.geo tigermap.geo USRadar.geo CanadaTopo250k.geo \
        CanadaTopo50k.geo TXRadar.geo Davis

MAINTAINERCLEANFILES = configure aclocal.m4 Makefile.in Makefile \
        config.guess config.sub install-sh missing mkinstalldirs \
        xastir.spec

countydir=${pkgdatadir}/Counties
county_DATA=placeholder

fccdir=${pkgdatadir}/fcc
fcc_DATA=placeholder

gnisdir=${pkgdatadir}/GNIS
gnis_DATA=placeholder

mapdir=${pkgdatadir}/maps

onlinedir=${pkgdatadir}/maps/Online
online_DATA=terraserver.geo toposerver.geo tigermap.geo \
        USRadar.geo CanadaTopo250k.geo CanadaTopo50k.geo \
        TXRadar.geo

gpsdir=${pkgdatadir}/maps/GPS
gps_DATA=placeholder

sounddir=${pkgdatadir}/sounds
sound_DATA=placeholder

docdir=${pkgdatadir}/doc
doc_DATA=AUTHORS FAQ ChangeLog LICENSE README README.CVS \
        README.MAPS README.win32 COPYING INSTALL UPGRADE \
        README.Getting-Started

mandir=${prefix}/man
man_MANS=xastir.1

ChangeLog::


