
# $Id$
#
# Copyright (C) 2000-2012  The Xastir Group
#

AUTOMAKE_OPTIONS = gnu dist-bzip2
ACLOCAL_AMFLAGS = -I m4

D=      `date +%G%m%d.%H%M%S`

SUBDIRS = src m4 config help scripts symbols callpass

# Remove this file here (it is created via configure), not from within intl.
# DISTCLEANFILES = intl/libintl.h

EXTRA_DIST = xastir.spec.in xastir.spec xastir-min.spec.in xastir-min.spec config.guess config.sub install-sh \
        missing mkinstalldirs placeholder FAQ LICENSE README README.CVS \
        README.CYGWIN README.Getting-Started README.Contributing \
        README.MAPS README.win32 README.OSM_maps testdbfawk.1 \
        DEBUG_LEVELS bootstrap.sh INSTALL UPGRADE callpass.1 xastir.1 xastir_udp_client.1 terraserver.geo \
        terraserver-reflectivity.geo terraserver-urban.geo worldhi.map \
        terraserver-topo.geo USRadar.geo CanadaTopo250k.geo \
        CanadaTopo50k.geo TXRadar.geo WMSRadar.geo Davis LaCrosse \
        OSM_template \
        CC_OpenStreetMap_logo.png CC_OpenStreetMap_txt.png \
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
map_DATA=worldhi.map CC_OpenStreetMap_logo.png CC_OpenStreetMap_txt.png

onlinedir=${pkgdatadir}/maps/Online
online_DATA=terraserver.geo terraserver-topo.geo \
        terraserver-reflectivity.geo terraserver-urban.geo \
        USRadar.geo CanadaTopo250k.geo CanadaTopo50k.geo \
        TXRadar.geo WMSRadar.geo \
        OSM_cloudmade_1.geo OSM_cloudmade_2.geo \
        OSM_cloudmade_5.geo OSM_cloudmade_998.geo \
        OSM_cloudmade_999.geo OSM_tiled_cycle.geo OSM_tiled_mapnik.geo \
        OSM_tiled_osmarender.geo OSM_tiled_skiing.geo \
        OSM_tiled_topOSM_0_relief.geo \
        OSM_tiled_topOSM_1_contour.geo  OSM_tiled_topOSM_2_features.geo

gpsdir=${pkgdatadir}/maps/GPS
gps_DATA=placeholder

sounddir=${pkgdatadir}/sounds
sound_DATA=placeholder

docdir=${prefix}/share/doc/xastir
doc_DATA=AUTHORS FAQ ChangeLog LICENSE README README.CVS README.CYGWIN \
        README.MAPS README.win32 COPYING INSTALL UPGRADE \
        README.Getting-Started README.Contributing README.OSM_maps\
        COPYING.LIB.LESSTIF

# It'd be nice to remove the directory and contents at
# "${pkgdatadir}/doc" then install a symlink called "doc" there pointing
# to "${prefix}/share/doc/xastir/".  The new location for the
# documentation is FHS compliant.

install-exec-hook:
	-rm -rf $(DESTDIR)${pkgdatadir}/doc

install-data-hook:
	cd $(DESTDIR)$(mapdir) && \
	rm -f CC_OpenStreetMap.png && \
	$(LN_S) CC_OpenStreetMap_logo.png CC_OpenStreetMap.png
 
mandir=${prefix}/share/man
man_MANS=xastir.1 callpass.1 xastir_udp_client.1 testdbfawk.1

# Tiles from cloudmade require an access code, so use the static map.
OSM_cloudmade_1.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/-STYLE/-layer=cloudmade_1/' -e's/^#OSMSTATICMAP/OSMSTATICMAP/' < $(top_srcdir)/OSM_template >$@

OSM_cloudmade_2.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/-STYLE/-layer=cloudmade_2/' -e's/^#OSMSTATICMAP/OSMSTATICMAP/' < $(top_srcdir)/OSM_template >$@

OSM_cloudmade_5.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/-STYLE/-layer=cloudmade_5/' -e's/^#OSMSTATICMAP/OSMSTATICMAP/' < $(top_srcdir)/OSM_template >$@

OSM_cloudmade_998.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/-STYLE/-layer=cloudmade_998/' -e's/^#OSMSTATICMAP/OSMSTATICMAP/' < $(top_srcdir)/OSM_template >$@

OSM_cloudmade_999.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/-STYLE/-layer=cloudmade_999/' < $(top_srcdir)/OSM_template >$@

# Use tiles for these maps
OSM_tiled_cycle.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tile.opencyclemap.org/cycle/!' \
	    -e 's/-STYLE/-cycle/' \
	    < $(top_srcdir)/OSM_template >$@

OSM_tiled_mapnik.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tile.openstreetmap.org/!' \
	    -e 's/-STYLE/-mapnik/' \
	    < $(top_srcdir)/OSM_template >$@

OSM_tiled_osmarender.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tah.openstreetmap.org/Tiles/tile/!' \
	    -e 's/-STYLE/-osmarender/' \
	    < $(top_srcdir)/OSM_template >$@

OSM_tiled_skiing.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tiles.openpistemap.org/contours/!' \
	    -e 's/-STYLE/-skiing/' \
	    < $(top_srcdir)/OSM_template >$@

OSM_tiled_topOSM_0_relief.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tile1.toposm.com/usw/color-relief/!' \
	    -e 's/-STYLE/-topOSMr/' \
	    -e 's/#TILE_EXT png/TILE_EXT jpg/' \
	    -e 's/#ZOOM_LEVEL_MIN 0/ZOOM_LEVEL_MIN 5/' \
	    -e 's/#ZOOM_LEVEL_MAX 18/ZOOM_LEVEL_MAX 15/' \
	    < $(top_srcdir)/OSM_template >$@

OSM_tiled_topOSM_1_contour.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tile1.toposm.com/usw/contours/!' \
	    -e 's/-STYLE/-topOSMc/' \
	    -e 's/#ZOOM_LEVEL_MIN 0/ZOOM_LEVEL_MIN 5/' \
	    -e 's/#ZOOM_LEVEL_MAX 18/ZOOM_LEVEL_MAX 15/' \
	    < $(top_srcdir)/OSM_template >$@

OSM_tiled_topOSM_2_features.geo: OSM_template
	sed -e '/THIS IS A TEMPLATE FILE/d' -e's/^#OSM_TILED_MAP/OSM_TILED_MAP/' \
	    -e's!^#URL tileURL!URL http://tile1.toposm.com/usw/features/!' \
	    -e 's/-STYLE/-topOSMf/' \
	    -e 's/#ZOOM_LEVEL_MIN 0/ZOOM_LEVEL_MIN 5/' \
	    -e 's/#ZOOM_LEVEL_MAX 18/ZOOM_LEVEL_MAX 15/' \
	    < $(top_srcdir)/OSM_template >$@

ChangeLog::


