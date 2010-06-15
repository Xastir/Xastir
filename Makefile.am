
# $Id$
#
# Copyright (C) 2000-2010  The Xastir Group
#

AUTOMAKE_OPTIONS = gnu dist-bzip2
ACLOCAL_AMFLAGS = -I m4

D=      `date +%G%m%d.%H%M%S`

SUBDIRS = src m4 config help scripts symbols callpass

# Remove this file here (it is created via configure), not from within intl.
# DISTCLEANFILES = intl/libintl.h

EXTRA_DIST = xastir.spec.in xastir.spec xastir-min.spec.in xastir-min.spec config.guess config.sub install-sh \
        missing mkinstalldirs placeholder FAQ LICENSE README README.CVS README.CYGWIN\
        README.Getting-Started README.Contributing README.MAPS README.win32 testdbfawk.1 \
        DEBUG_LEVELS bootstrap.sh INSTALL UPGRADE callpass.1 xastir.1 xastir_udp_client.1 terraserver.geo \
        terraserver-reflectivity.geo terraserver-urban.geo worldhi.map \
        terraserver-topo.geo tigermap.geo USRadar.geo CanadaTopo250k.geo \
        CanadaTopo50k.geo TXRadar.geo WMSRadar.geo Davis LaCrosse \
        OSM_cloudmade_1.geo OSM_cloudmade_1.osm OSM_cloudmade_2.geo \
        OSM_cloudmade_2.osm OSM_cloudmade_5.geo OSM_cloudmade_5.osm \
        OSM_cloudmade_998.geo OSM_cloudmade_998.osm OSM_cloudmade_999.geo \
        OSM_cloudmade_999.osm OSM_cycle.geo OSM_cycle.osm \
        OSM_mapnik.geo OSM_mapnik.osm OSM_osmarender.geo \
        OSM_osmarender.osm OSM_skiing.geo OSM_skiing.osm \
        COPYING COPYING.LIB.LESSTIF AUTHORS

MAINTAINERCLEANFILES = configure aclocal.m4 Makefile.in Makefile \
        config.guess config.sub install-sh missing mkinstalldirs \
        xastir.spec xastir-min.spec

countydir=${pkgdatadir}/Counties
county_DATA=placeholder

fccdir=${pkgdatadir}/fcc
fcc_DATA=placeholder

gnisdir=${pkgdatadir}/GNIS
gnis_DATA=placeholder

mapdir=${pkgdatadir}/maps
map_DATA=worldhi.map

onlinedir=${pkgdatadir}/maps/Online
online_DATA=terraserver.geo terraserver-topo.geo tigermap.geo \
        terraserver-reflectivity.geo terraserver-urban.geo \
        USRadar.geo CanadaTopo250k.geo CanadaTopo50k.geo \
        TXRadar.geo WMSRadar.geo \
        OSM_cloudmade_1.geo OSM_cloudmade_1.osm OSM_cloudmade_2.geo \
        OSM_cloudmade_2.osm OSM_cloudmade_5.geo OSM_cloudmade_5.osm \
        OSM_cloudmade_998.geo OSM_cloudmade_998.osm OSM_cloudmade_999.geo \
        OSM_cloudmade_999.osm OSM_cycle.geo OSM_cycle.osm \
        OSM_mapnik.geo OSM_mapnik.osm OSM_osmarender.geo \
        OSM_osmarender.osm OSM_skiing.geo OSM_skiing.osm

gpsdir=${pkgdatadir}/maps/GPS
gps_DATA=placeholder

sounddir=${pkgdatadir}/sounds
sound_DATA=placeholder

docdir=${prefix}/share/doc/xastir
doc_DATA=AUTHORS FAQ ChangeLog LICENSE README README.CVS README.CYGWIN \
        README.MAPS README.win32 COPYING INSTALL UPGRADE \
        README.Getting-Started README.Contributing \
        COPYING.LIB.LESSTIF

# It'd be nice to remove the directory and contents at
# "${pkgdatadir}/doc" then install a symlink called "doc" there pointing
# to "${prefix}/share/doc/xastir/".  The new location for the
# documentation is FHS compliant.

install-exec-hook:
	-rm -rf ${pkgdatadir}/doc
 
mandir=${prefix}/share/man
man_MANS=xastir.1 callpass.1 xastir_udp_client.1 testdbfawk.1

ChangeLog::


