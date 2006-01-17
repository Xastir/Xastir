# $Id$
#
# Copyright (C) 2000-2006  The Xastir Group
#

configdir = ${pkgdatadir}/config

EXTRA_DIST = \
    language-Dutch.sys \
    language-English.sys \
    language-French.sys \
    language-German.sys \
    language-Italian.sys \
    language-Portuguese.sys \
    language-Spanish.sys \
    xastir.rgb \
    gps_wpt.dbfawk \
    24kgrid.dbfawk \
    tgr2shp.dbfawk \
    tgr2shppoly.dbfawk \
    tgrcty.dbfawk \
    tgrkgl.dbfawk \
    tgrlk.dbfawk \
    tgrlpt.dbfawk \
    tgrlpy.dbfawk \
    tgrplc00.dbfawk \
    tgrwat.dbfawk \
    nwsc_ddmmyy.dbfawk \
    nwsmzddmmyy.dbfawk \
    nwsmzoddmmyy.dbfawk \
    nwsozddmmyy.dbfawk \
    nwsw_ddmmyy.dbfawk \
    nwsz_ddmmyy.dbfawk \
    nwszoddmmyy.dbfawk \
    predefined_EVENT.sys \
    predefined_SAR.sys \
    tnc-startup.aea \
    tnc-startup.d700 \
    tnc-startup.kam \
    tnc-startup.kpc2 \
    tnc-startup.kpc3 \
    tnc-startup.null \
    tnc-startup.paccomm \
    tnc-startup.pico \
    tnc-startup.sys \
    tnc-startup.thd7 \
    tnc-startup.tnc2 \
    tnc-startup.tnc2-ui \
    tnc-stop.d700 \
    tnc-stop.sys \
    tnc-stop.thd7 \
    tnc-stop.tnc2-ui

config_DATA = ${EXTRA_DIST}

install-exec-hook:
	rm -f \$(configdir)/nwsfz_ddmmyy.dbfawk

