# $Id$
#
# Copyright (C) 2000-2014  The Xastir Group
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
    language-ElmerFudd.sys \
    language-MuppetsChef.sys \
    language-OldeEnglish.sys \
    language-PigLatin.sys \
    language-PirateEnglish.sys \
    xastir.rgb \
    gps_wpt.dbfawk \
    24kgrid.dbfawk \
    stored_track.dbfawk \
    tgr2shp.dbfawk \
    tgr2shppoly.dbfawk \
    tgr2shppoly_2006.dbfawk \
    tgrcty.dbfawk \
    tgrkgl.dbfawk \
    tgrlk.dbfawk \
    tgrlpt.dbfawk \
    tgrlpy.dbfawk \
    tgrplc00.dbfawk \
    tgrwat.dbfawk \
    nwsc_ddmmyy.dbfawk \
    nwsc_ddmmyy_09.dbfawk \
    nwsc_ddmmyy_09b.dbfawk \
    nwsc_ddmmyy_10.dbfawk \
    nwsc_ddmmyy_10a.dbfawk \
    nwsc_ddmmyy_13.dbfawk \
    nwsc_ddmmyy_15.dbfawk \
    nwsmzddmmyy.dbfawk \
    nwsmzddmmyy_09.dbfawk \
    nwsmzddmmyy_11.dbfawk \
    nwsmzoddmmyy.dbfawk \
    nwsozddmmyy.dbfawk \
    nwsozddmmyy_09.dbfawk \
    nwsozddmmyy_14.dbfawk \
    nwsozddap12.dbfawk \
    nwsw_ddmmyy.dbfawk \
    nwsw_ddmmyy_09.dbfawk \
    nwsw_ddmmyy_10.dbfawk \
    nwsw_ddmmyy_13.dbfawk \
    nwsw_ddmmyy_14.dbfawk \
    nwsw_ddmmyy_14a.dbfawk \
    nwsw_ddjn12.dbfawk \
    nwsz_ddmmyy.dbfawk \
    nwsz_ddmmyy_09.dbfawk \
    nwsz_ddmmyy_10.dbfawk \
    nwsz_ddmmyy_10b.dbfawk \
    nwsz_ddmmyy_10c.dbfawk \
    nwsz_ddmmyy_11.dbfawk \
    nwsz_ddmmyy_13.dbfawk \
    nwsz1ddmmyy.dbfawk \
    nwszoddmmyy.dbfawk \
    nwshzddmmyy.dbfawk \
    OSM_Cloudmade_administrative.dbfawk \
    OSM_Cloudmade_highway.dbfawk \
    OSM_Cloudmade_natural.dbfawk \
    OSM_Cloudmade_poi.dbfawk \
    OSM_Cloudmade_water_and_coastline.dbfawk \
    predefined_EVENT.sys \
    predefined_SAR.sys \
    tl_2009_aiannh.dbfawk \
    tl_2009_aits.dbfawk \
    tl_2009_arealm.dbfawk \
    tl_2009_areawater.dbfawk \
    tl_2009_county.dbfawk \
    tl_2009_cousub.dbfawk \
    tl_2009_edges.dbfawk \
    tl_2009_mil.dbfawk \
    tl_2009_nn_county.dbfawk \
    tl_2009_pointlm.dbfawk \
    tl_2009_zcta5.dbfawk \
    tnc-startup.aea \
    tnc-startup.d700 \
    tnc-startup.d72_d710 \
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
    tnc-stop.d72_d710 \
    tnc-stop.sys \
    tnc-stop.thd7 \
    tnc-stop.tnc2-ui \
    areawater.dbfawk \
    arealm.dbfawk \
    tabblock.dbfawk \
    cousub.dbfawk \
    cousub00.dbfawk \
    edge.dbfawk \
    featnames.dbfawk \
    pointlm.dbfawk \
    gfe_public_weather.dbfawk \
    gfe_metro_areas.dbfawk \
    gfe_fire_weather.dbfawk \
    gfe_coastal_waters_warnings.dbfawk \
    gfe_coastal_waters.dbfawk


config_DATA = ${EXTRA_DIST}

install-exec-hook:
	-rm -f $(DESTDIR)$(configdir)/nwsfz_ddmmyy.dbfawk

language-ElmerFudd.sys: language-English.sys
	$(top_srcdir)/scripts/langElmerFudd.pl -split >$@ <$(srcdir)/language-English.sys

language-MuppetsChef.sys: language-English.sys
	$(top_srcdir)/scripts/langMuppetsChef.pl -split >$@ <$(srcdir)/language-English.sys

language-OldeEnglish.sys: language-English.sys
	$(top_srcdir)/scripts/langOldeEnglish.pl -split >$@ <$(srcdir)/language-English.sys

language-PigLatin.sys: language-English.sys
	$(top_srcdir)/scripts/langPigLatin.pl -split >$@ <$(srcdir)/language-English.sys

language-PirateEnglish.sys: language-English.sys
	$(top_srcdir)/scripts/langPirateEnglish.pl -split >$@ <$(srcdir)/language-English.sys

