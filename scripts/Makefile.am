#
# Copyright (C) 2000-2018 The Xastir Group
#

scriptsdir=${pkgdatadir}/scripts
dist_scripts_DATA = \
    ads-b.pl \
    ais.pl \
    ais_pp.pl \
    Coordinate.pm \
    coord-convert.pl \
    geopdf2gtiff.pl \
    get-BOMdata \
    get-fcc-rac.pl \
    get-gnis \
    get-pop \
    get-NWSdata \
    gpx2shape \
    icontable.pl \
    inf2geo.pl \
    kiss-off.pl \
    langElmerFudd.pl \
    langMuppetsChef.pl \
    langOldeEnglish.pl \
    langPigLatin.pl \
    langPirateEnglish.pl \
    mapblast2geo.pl \
    mapfgd.pl \
    object2shp.pl \
    overlay.pl \
    ozi2geo.pl \
    permutations.pl \
    pos2shp.pl \
    ridge_radar.pl \
    slideshow.pl \
    split_gnis.bash \
    split_gnis.pl \
    test_coord.pl \
    toporama50k.pl \
    toporama250k.pl\
    track-get.pl \
    update_langfile.pl \
    waypoint-get.pl \
    wms.pl \
    wxnowsrv.pl \
    Xastir_tigerpoly.py

install-data-hook:
	cd $(DESTDIR)$(scriptsdir) && \
	chmod a+x *.pl *.py get-* gpx2* *.bash
